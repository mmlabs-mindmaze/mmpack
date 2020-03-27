/*
 * @mindmaze_header@
 */
#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include <mmlib.h>
#include <mmsysio.h>

#include "mmstring.h"
#include "package-utils.h"
#include "srcindex.h"


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

	if (!pkg->name || !pkg->version || !pkg->filename || !pkg->sha256)
		return -1;

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

	// Create a packge in table that take ownership of all fields of pkg
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
int srcpkg_setfield(struct srcpkg* pkg, struct strbuf line)
{
	struct strbuf key, val;
	mmstr** str_ptr;

	if (strbuf_extract_keyval(line, ':', &key, &val) == -1)
		return -1;

	if (STR_EQUAL(key.buf, key.len, "size") == 0) {
		char cstr[16] = "";
		// Copy value to a nul-terminated string
		memcpy(cstr, key.buf, MIN(key.len, MM_NELEM(cstr)-1));
		pkg->size = atoi(cstr);
		return 0;
	} else if (STR_EQUAL(key.buf, key.len, "name") == 0) {
		str_ptr = &pkg->name;
	} else if (STR_EQUAL(key.buf, key.len, "filename") == 0) {
		str_ptr = &pkg->filename;
	} else if (STR_EQUAL(key.buf, key.len, "sha256") == 0) {
		str_ptr = &pkg->sha256;
	} else if (STR_EQUAL(key.buf, key.len, "version") == 0) {
		str_ptr = &pkg->version;
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
	struct strbuf line, filedata;
	struct mm_stat buf;
	struct srcpkg pkg;
	void* map;

	srcpkg_init(&pkg);
	pkg.repo = repo;

	fd = mm_open(index_filename, O_RDONLY, 0);
	if (fd == -1)
		return -1;

	mm_fstat(fd, &buf);
	map = mm_mapfile(fd, 0, buf.size, MM_MAP_READ|MM_MAP_SHARED);
	mm_check(filedata.buf != NULL);

	filedata.len = buf.size;
	filedata.buf = map;

	while (filedata.len && rv == 0) {
		line = strbuf_getline(&filedata);

		if (line.len == 0) {
			srcindex_add_pkg(srcindex, &pkg);
			srcpkg_deinit(&pkg);
			srcpkg_init(&pkg);
			pkg.repo = repo;
		} else {
			rv = srcpkg_setfield(&pkg, line);
		}
	}

	srcpkg_deinit(&pkg);
	mm_unmap(map);
	mm_close(fd);
	return rv;
}
