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
void srcpkg_init(struct srcpkg * pkg, mmstr * name)
{
	*pkg = (struct srcpkg) {.name = name};
}


static
void srcpkg_deinit(struct srcpkg * pkg)
{
	mmstr_free(pkg->name);
	mmstr_free(pkg->filename);
	mmstr_free(pkg->sha256);
	mmstr_free(pkg->version);

	srcpkg_init(pkg, NULL);
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
	srcindex->num_pkgname = 0;
}


void srcindex_add_pkg(struct srcindex* srcindex, struct srcpkg* pkg)
{
	struct it_entry * entry;
	mmstr * name_id = NULL;
	int len = mmstrlen(pkg->name);

	name_id = mmstr_realloc(name_id, len);
	name_id = mmstrcpy(name_id, pkg->name);

	len += 1;
	name_id = mmstr_realloc(name_id, len);
	name_id = mmstrcat(name_id, "_");

	len += mmstrlen(pkg->sha256);
	name_id = mmstr_realloc(name_id, len);
	name_id = mmstrcat(name_id, pkg->sha256);

	entry = indextable_lookup_create(&srcindex->pkgname_idx, name_id);
	*((struct srcpkg*) entry->value) = *pkg;

	mmstr_free(name_id);
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
	char * line, * eol, * field;
	size_t key_len, field_len;
	void * map;
	struct mm_stat buf;
	struct srcpkg srcpkg;

	srcpkg_init(&srcpkg, NULL);

	fd = mm_open(index_filename, O_RDONLY, 0);
	if (fd == -1)
		return -1;

	mm_fstat(fd, &buf);
	map = mm_mapfile(fd, 0, buf.size, MM_MAP_READ|MM_MAP_SHARED);

	if (buf.size == 0)
		goto exit;

	mm_check(map != NULL);

	line = map;
	while ((eol = strchr(line, '\n')) != NULL) {
		field = strchr(line, ' ');

		if (field == NULL) {
			// the source-index file is empty
			if (srcpkg.name == NULL && srcpkg.filename == NULL
			    && srcpkg.sha256 == NULL && srcpkg.version == NULL)
				goto exit;

			// the source-index file is not well-formed
			if (srcpkg.name == NULL || srcpkg.filename == NULL
			    || srcpkg.sha256 == NULL ||
			    srcpkg.version == NULL) {
				rv = -1;
				goto exit;
			}

			srcpkg.repo = repo;
			srcindex_add_pkg(srcindex, &srcpkg);
			srcpkg_init(&srcpkg, NULL);
		}

		key_len = field - line;
		field_len = eol - field;
		if (STR_EQUAL(line, key_len, "name") == 0) {
			srcpkg.name = mmstr_copy_realloc(srcpkg.name,
			                                 field,
			                                 field_len);
		} else if (STR_EQUAL(line, key_len, "filename") == 0) {
			srcpkg.filename = mmstr_copy_realloc(srcpkg.filename,
			                                     field,
			                                     field_len);
		} else if (STR_EQUAL(line, key_len, "sha256") == 0) {
			srcpkg.sha256 = mmstr_copy_realloc(srcpkg.sha256,
			                                   field,
			                                   field_len);
		} else if (STR_EQUAL(line, key_len, "size") == 0) {
			srcpkg.size = atoi(field);
		} else if (STR_EQUAL(line, key_len, "version") == 0) {
			srcpkg.version = mmstr_copy_realloc(srcpkg.version,
			                                    field,
			                                    field_len);
		}

		line = eol + 1;
	}

exit:
	srcpkg_deinit(&srcpkg);
	mm_unmap(map);
	mm_close(fd);
	return rv;
}
