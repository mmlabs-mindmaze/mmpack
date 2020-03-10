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

struct srclist_entry {
	struct srcpkg pkg;
	struct srclist_entry * next;
};

struct srclist {
	mmstr * pkg_name;
	struct srclist_entry * head;
	int num_pkg;
	int id;
};


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


static
void srclist_init(struct srclist* list, mmstr* name, int id)
{
	*list = (struct srclist) {.pkg_name = mmstrdup(name), .id = id};
}


static
void srclist_deinit(struct srclist* list)
{
	struct srclist_entry * entry, * next;

	entry = list->head;
	while (entry) {
		next = entry->next;

		srcpkg_deinit(&entry->pkg);
		free(entry);

		entry = next;
	}

	mmstr_free(list->pkg_name);
}


static
struct srcpkg* srclist_add_or_modify(struct srclist* list, struct srcpkg* pkg)
{
	struct srclist_entry* entry;
	struct srclist_entry** pnext;
	struct srcpkg* pkg_in_list;
	const mmstr* next_version;
	int vercmp;

	// Loop over entry and check whether there is an identical package
	// (ie has the same sumsha and version).
	for (entry = list->head; entry != NULL; entry = entry->next) {
		// Check the entry match version and sumsha
		if (!mmstrequal(pkg->version, entry->pkg.version)
		    || !mmstrequal(pkg->sha256, entry->pkg.sha256))
			continue;

		pkg_in_list = &entry->pkg;
		pkg_in_list->repo = pkg->repo;

		return pkg_in_list;
	}

	// Find where to insert entry (package version are sorted)
	for (pnext = &list->head; *pnext != NULL; pnext = &(*pnext)->next) {
		next_version = (*pnext)->pkg.version;
		vercmp = pkg_version_compare(next_version, pkg->version);
		if (vercmp < 0)
			break;
	}

	// Add new entry to the list
	entry = xx_malloc(sizeof(*entry));
	entry->next = *pnext;
	*pnext = entry;

	// copy the whole package structure
	entry->pkg = *pkg;
	entry->pkg.name = list->pkg_name;
	entry->pkg.name_id = list->id;

	// reset package fields since they have been taken over by the new
	// entry
	srcpkg_init(pkg, NULL);

	list->num_pkg++;

	return &entry->pkg;
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
	int i;

	for (i = 0; i < srcindex->num_pkgname; i++)
		srclist_deinit(&srcindex->pkgname_table[i]);

	free(srcindex->pkgname_table);
	srcindex->pkgname_table = NULL;

	indextable_deinit(&srcindex->pkgname_idx);

	srcindex->num_pkgname = 0;
}


LOCAL_SYMBOL
int srcindex_get_pkgname_id(struct srcindex* srcindex, mmstr* name)
{
	struct srclist* new_tab;
	struct srclist* pkglist;
	struct indextable* idx;
	struct it_entry* entry;
	struct it_entry defval = {.key = name, .ivalue = -1};
	size_t tab_sz;
	int pkgname_id;

	idx = &srcindex->pkgname_idx;
	entry = indextable_lookup_create_default(idx, name, defval);
	pkgname_id = entry->ivalue;

	// Create package list if not existing yet
	if (pkgname_id == -1) {
		// Resize pkgname table
		tab_sz = (srcindex->num_pkgname + 1) * sizeof(*new_tab);
		new_tab = xx_realloc(srcindex->pkgname_table, tab_sz);
		srcindex->pkgname_table = new_tab;

		// Assign an new pkgname id
		pkgname_id = srcindex->num_pkgname++;

		// Initialize the package list associated to id
		pkglist = &srcindex->pkgname_table[pkgname_id];
		srclist_init(pkglist, name, pkgname_id);

		// Reference the new package list in the index table
		entry->ivalue = pkgname_id;
		entry->key = pkglist->pkg_name;
	}

	return pkgname_id;
}


static
struct srcpkg* srcindex_add_pkg(struct srcindex* srcindex, struct srcpkg* pkg)
{
	struct srclist* srclist;
	int pkgname_id;

	pkgname_id = srcindex_get_pkgname_id(srcindex, pkg->name);
	srclist = &srcindex->pkgname_table[pkgname_id];
	return srclist_add_or_modify(srclist, pkg);
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
			    || srcpkg.sha256 == NULL || srcpkg.version == NULL) {
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
			srcpkg.name = mmstr_copy_realloc(srcpkg.name, field, field_len);
		} else if (STR_EQUAL(line, key_len, "filename") == 0) {
			srcpkg.filename = mmstr_copy_realloc(srcpkg.filename, field, field_len);
		} else if (STR_EQUAL(line, key_len, "sha256") == 0) {
			srcpkg.sha256 = mmstr_copy_realloc(srcpkg.sha256, field, field_len);
		} else if (STR_EQUAL(line, key_len, "size") == 0) {
			srcpkg.size = atoi(field);
		} else if (STR_EQUAL(line, key_len, "version") == 0) {
			srcpkg.version = mmstr_copy_realloc(srcpkg.version, field, field_len);
		}

		line = eol + 1;
	}

exit:
	srcpkg_deinit(&srcpkg);
	mm_unmap(map);
	mm_close(fd);
	return rv;
}
