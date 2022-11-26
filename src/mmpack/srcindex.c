/*
 * @mindmaze_header@
 */
#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include <mmerrno.h>
#include <mmlib.h>
#include <mmsysio.h>

#include "crypto.h"
#include "mmstring.h"
#include "package-utils.h"
#include "repo.h"
#include "srcindex.h"
#include "utils.h"


static
int keyid_len(const mmstr* srcname)
{
	return mmstrlen(srcname) + SHA_HEXLEN + 1;
}


static
mmstr* set_keyid(mmstr* keyid, const mmstr* srcname,
                 const digest_t* srcsha)
{
	int len;

	// Form the indexing key "<name>_<srcsha>"
	mmstrcpy(keyid, srcname);
	mmstrcat_cstr(keyid, "_");

	// Append the srcsha converted in hex
	len = mmstrlen(keyid);
	hexstr_from_digest(keyid + len, srcsha);
	mmstr_setlen(keyid, len + SHA_HEXLEN);

	return keyid;
}


static
mmstr* create_keyid(const mmstr* srcname, const digest_t* srcsha)
{
	mmstr* keyid = mmstr_malloc(keyid_len(srcname));
	return set_keyid(keyid, srcname, srcsha);
}

/**************************************************************************
 *                       source package implementation                    *
 **************************************************************************/
static
void srcpkg_init(struct srcpkg * pkg, const struct repo* repo)
{
	*pkg = (struct srcpkg) {
		.remote_res = remote_resource_create(repo),
	};
}


static
void srcpkg_deinit(struct srcpkg * pkg)
{
	mmstr_free(pkg->name);
	mmstr_free(pkg->version);
	remote_resource_destroy(pkg->remote_res);

	*pkg = (struct srcpkg) {NULL};
}


static
int srcpkg_is_empty(const struct srcpkg * pkg)
{
	return (!pkg->name
	        && !pkg->version
	        && !pkg->remote_res->filename);
}


static
int srcpkg_is_fully_set(const struct srcpkg * pkg)
{
	return (pkg->name
	        && pkg->version
	        && pkg->remote_res->filename
	        && pkg->remote_res->size);
}


static
int srcpkg_setfield(struct srcpkg* pkg, struct strchunk line)
{
	struct strchunk key, val;
	const mmstr** str_ptr = NULL;
	int pos;

	pos = strchunk_find(line, ':');
	if (pos == line.len)
		return -1;

	key = strchunk_strip(strchunk_lpart(line, pos));
	val = strchunk_strip(strchunk_rpart(line, pos));

	// Determine which mmstr* field of pkg must be updated
	if (STR_EQUAL(key.buf, key.len, "size")) {
		return strchunk_parse_size(&pkg->remote_res->size, val);
	} else if (STR_EQUAL(key.buf, key.len, "name")) {
		str_ptr = &pkg->name;
	} else if (STR_EQUAL(key.buf, key.len, "filename")) {
		str_ptr = &pkg->remote_res->filename;
	} else if (STR_EQUAL(key.buf, key.len, "version")) {
		str_ptr = &pkg->version;
	} else if (STR_EQUAL(key.buf, key.len, "sha256")) {
		digest_from_hexstr(&pkg->remote_res->sha256, val);
		pkg->sha256 = pkg->remote_res->sha256;
		return 0;
	} else {
		// ignore unknown field
		return 0;
	}

	// Update the mmstr* field with the value set in the line
	mmstr_free(*str_ptr);
	*str_ptr = mmstr_malloc_copy(val.buf, val.len);

	return 0;
}


static
void srcpkg_add_remote(struct srcpkg* pkg, struct remote_resource* to_add)
{
	struct remote_resource* res = pkg->remote_res;

	while (res->next)
		res = res->next;

	res->next = to_add;
}


/**************************************************************************
 *                        source index implementation                     *
 **************************************************************************/
LOCAL_SYMBOL
void srcindex_init(struct srcindex* srcindex)
{
	*srcindex = (struct srcindex) {0};
	indextable_init(&srcindex->pkgname_idx, -1, -1);
}


LOCAL_SYMBOL
void srcindex_deinit(struct srcindex* srcindex)
{
	struct it_entry* entry;
	struct it_iterator iter;

	// Free all data pointed by entries
	entry = it_iter_first(&iter, &srcindex->pkgname_idx);
	while (entry) {
		srcpkg_deinit(entry->value);
		free(entry->value);
		mmstr_free(entry->key);
		entry = it_iter_next(&iter);
	}

	indextable_deinit(&srcindex->pkgname_idx);
}


static
int srcindex_add_pkg(struct srcindex* srcindex, struct srcpkg* pkg)
{
	struct it_entry * entry;
	struct srcpkg* pkg_in_idx;
	const struct repo* repo = pkg->remote_res->repo;
	mmstr* keyid;

	// Check package entry validity
	if (!srcpkg_is_fully_set(pkg)) {
		return mm_raise_error(EINVAL,
		                      "Missing fields in entries of source "
		                      "index of repo %s", repo->name);
	}

	// Allocate and set the table key
	keyid = create_keyid(pkg->name, &pkg->remote_res->sha256);

	// Try to create the entry in the indextable. If already existing (ie
	// entry->value != NULL), only add remote resource
	entry = indextable_lookup_create(&srcindex->pkgname_idx, keyid);
	if (entry->value) {
		mmstr_free(keyid);
		pkg_in_idx = entry->value;

		// Take ownership of remote resource from package being added
		// and add it to the one in index
		srcpkg_add_remote(pkg_in_idx, pkg->remote_res);
		pkg->remote_res = NULL;
		return 0;
	}

	// Create a package in table that take ownership of all fields of pkg
	pkg_in_idx = xx_malloc(sizeof(*pkg_in_idx));
	*pkg_in_idx = *pkg;

	// pkg_in_idx has taken ownership of all fields of pkg, then reset it
	srcpkg_init(pkg, NULL);

	// This is a new entry, set the key and val fields to created objects
	entry->key = keyid;
	entry->value = pkg_in_idx;

	return 0;
}


/**
 * srcindex_populate() - populate source package database from package list
 * @binindex:   binary package index to populate
 * @index_filename: repository package list file
 *
 * Return: 0 in case of success, -1 otherwise
 */
LOCAL_SYMBOL
int srcindex_populate(struct srcindex * srcindex, char const * index_filename,
                      const struct repo* repo)
{
	int rv;
	struct strchunk line, data_to_parse;
	struct srcpkg pkg;
	void* map = NULL;
	size_t mapsize;

	srcpkg_init(&pkg, repo);
	rv = map_file_in_prefix(NULL, index_filename, &map, &mapsize);
	if (rv == -1)
		goto exit;

	data_to_parse = (struct strchunk) {.buf = map, .len = mapsize};
	while (data_to_parse.len) {
		line = strchunk_getline(&data_to_parse);
		line = strchunk_strip(line);

		// Set field of current package entry if line is not empty
		if (line.len > 0) {
			rv = srcpkg_setfield(&pkg, line);
			if (rv)
				break;
		}

		// Add package entry to index when the first line of a group of
		// consecutive empty line is encountered or when the end of
		// file is reached.
		if ((line.len == 0 || data_to_parse.len == 0)) {
			if (!srcpkg_is_empty(&pkg)) {
				rv = srcindex_add_pkg(srcindex, &pkg);
				srcpkg_deinit(&pkg);
				srcpkg_init(&pkg, repo);
			}
		}
	}

exit:
	mm_unmap(map);
	srcpkg_deinit(&pkg);
	return rv;
}


/**
 * srcindex_lookup() - find a source package in the table
 * @binindex:   binary package index to populate
 * @srcname:    name of source package
 * @version:    version of the source
 * @srchash:    hash identifier of source package
 *
 * Return: the pointer to source package if found, NULL otherwise
 */
LOCAL_SYMBOL
const struct srcpkg* srcindex_lookup(struct srcindex* srcindex,
                                     const mmstr* srcname,
                                     const mmstr* version,
                                     const digest_t* srchash)
{
	(void)version; // version is currently not used in lookup
	const mmstr* keyid;
	struct it_entry * entry;

	// Allocate and set on stack the key
	keyid = set_keyid(mmstr_alloca(keyid_len(srcname)), srcname, srchash);

	entry = indextable_lookup(&srcindex->pkgname_idx, keyid);
	if (!entry)
		return NULL;

	return entry->value;
}
