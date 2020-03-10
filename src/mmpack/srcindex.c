/*
 * @mindmaze_header@
 */
#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include <mmerrno.h>
#include <mmlib.h>
#include <mmsysio.h>

#include "mmstring.h"
#include "package-utils.h"
#include "srcindex.h"
#include "utils.h"


static
void srcpkg_init(struct srcpkg * pkg)
{
	*pkg = (struct srcpkg) {NULL};
}


static
void srcpkg_deinit(struct srcpkg * pkg)
{
	mmstr_free(pkg->name);
	mmstr_free(pkg->filename);
	mmstr_free(pkg->sha256);
	mmstr_free(pkg->version);
}


static
void srcpkg_reinit(struct srcpkg * pkg)
{
	struct repolist_elt* repo = pkg->repo;

	srcpkg_deinit(pkg);

	*pkg = (struct srcpkg) {.repo = repo};
}


static
int srcpkg_is_empty(const struct srcpkg * pkg)
{
	return (!pkg->name && !pkg->filename && !pkg->sha256 && !pkg->version);
}


static
int srcpkg_is_fully_set(const struct srcpkg * pkg)
{
	return (pkg->name && pkg->filename && pkg->sha256 && pkg->version);
}


LOCAL_SYMBOL
void srcindex_init(struct srcindex* srcindex)
{
	*srcindex = (struct srcindex) {0};
	indextable_init(&srcindex->pkgname_idx, -1, -1);
}


LOCAL_SYMBOL
void srcindex_deinit(struct srcindex* srcindex)
{
	indextable_deinit(&srcindex->pkgname_idx);
}


static
int srcindex_add_pkg(struct srcindex* srcindex, struct srcpkg* pkg)
{
	struct it_entry * entry;
	struct srcpkg* pkg_in_idx;
	mmstr* name_id;

	// Check package entry validity
	if (!srcpkg_is_fully_set(pkg)) {
		return mm_raise_error(EINVAL,
	                              "Missing fields in entries of source "
				      "index of repo %s", pkg->repo->name);
	}


	// Form the indexing key "<name>_<srcsha>"
	name_id = mmstr_malloc(mmstrlen(pkg->name) + mmstrlen(pkg->sha256) + 1);
	mmstrcpy(name_id, pkg->name);
	mmstrcat_cstr(name_id, "_");
	mmstrcat(name_id, pkg->sha256);

	// Try to create the entry in the indextable. If already existing (ie
	// entry->value != NULL), skip adding a source package
	entry = indextable_lookup_create(&srcindex->pkgname_idx, name_id);
	if (entry->value) {
		mmstr_free(name_id);
		return 0;
	}

	// Create a package in table that take ownership of all fields of pkg
	pkg_in_idx = xx_malloc(sizeof(*pkg_in_idx));
	srcpkg_init(pkg_in_idx);
	*pkg_in_idx = *pkg;
	srcpkg_init(pkg);

	// This is a new entry, set the key and val fields to created objects
	entry->key = name_id;
	entry->value = pkg_in_idx;

	return 0;
}


static
int srcpkg_setfield(struct srcpkg* pkg, struct strview line)
{
	struct strview key, val;
	mmstr** str_ptr = NULL;
	int pos;

	pos = strview_find(line, ':');
	if (pos == line.len)
		return -1;

	key = strview_strip(strview_lpart(line, pos));
	val = strview_strip(strview_rpart(line, pos));

	if (STR_EQUAL(key.buf, key.len, "size") == 0) {
		return strview_parse_size(&pkg->size, val);
	} else if (STR_EQUAL(key.buf, key.len, "name") == 0) {
		str_ptr = &pkg->name;
	} else if (STR_EQUAL(key.buf, key.len, "filename") == 0) {
		str_ptr = &pkg->filename;
	} else if (STR_EQUAL(key.buf, key.len, "sha256") == 0) {
		str_ptr = &pkg->sha256;
	} else if (STR_EQUAL(key.buf, key.len, "version") == 0) {
		str_ptr = &pkg->version;
	} else {
		// ignore unknown field
		return 0;
	}

	*str_ptr = mmstr_copy_realloc(*str_ptr, val.buf, val.len);
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
                      struct repolist_elt * repo)
{
	int fd, rv = 0;
	struct strview line, filedata;
	struct mm_stat buf;
	struct srcpkg pkg;
	void* map;

	srcpkg_init(&pkg);
	pkg.repo = repo;
	fd = mm_open(index_filename, O_RDONLY, 0);
	if (fd == -1)
		return -1;

	mm_fstat(fd, &buf);
	if (buf.size == 0)
		goto exit;

	map = mm_mapfile(fd, 0, buf.size, MM_MAP_READ|MM_MAP_SHARED);
	mm_check(map != NULL);

	filedata.len = buf.size;
	filedata.buf = map;
	while (filedata.len) {
		line = strview_getline(&filedata);
		line = strview_strip(line);

		// Set field of current package entry if line is not empty
		if (line.len > 0) {
			rv = srcpkg_setfield(&pkg, line);
			if (rv)
				break;
		}

		// Add package entry to index when the first line of a group of
		// consecutive empty line is encountered or when the end of
		// file is reached.
		if ((line.len == 0 || filedata.len == 0)) {
			if (!srcpkg_is_empty(&pkg)) {
				rv = srcindex_add_pkg(srcindex, &pkg);
				srcpkg_reinit(&pkg);
			}
		}
	}

	mm_unmap(map);

exit:
	mm_close(fd);
	srcpkg_deinit(&pkg);
	return rv;
}
