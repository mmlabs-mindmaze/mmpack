/*
 * @mindmaze_header@
 */
#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include <assert.h>
#include <curl/curl.h>
#include <mmsysio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <yaml.h>

#include <mmerrno.h>
#include <mmlib.h>

#include "binpkg.h"
#include "buffer.h"
#include "common.h"
#include "indextable.h"
#include "package-utils.h"
#include "pkg-fs-utils.h"
#include "repo.h"
#include "strlist.h"
#include "utils.h"
#include "xx-alloc.h"

struct rdepends {
	int num;
	int nmax;
	int* ids;
};

struct pkglist_entry {
	struct binpkg pkg;
	struct pkglist_entry* next;
};

struct pkglist {
	const mmstr* pkg_name;
	struct pkglist_entry* head;
	struct rdepends rdeps;
	int num_pkg;
	int id;
};

struct parsing_ctx {
	yaml_parser_t parser;
	const struct repo* repo;
};

/* standard isdigit() is locale dependent making it unnecessarily slow.
 * This macro is here to keep the semantic of isdigit() as usually known by
 * most programmer while issuing the fast implementation of it. */
#define isdigit(c)      ((c) >= '0' && (c) <= '9')


/**
 * constraints_deinit  -  deinit a structure struct constraints
 * @c: the structure to deinitialize
 */
LOCAL_SYMBOL
void constraints_deinit(struct constraints * c)
{
	mmstr_free(c->version);
	c->version = NULL;
	mmstr_free(c->sumsha);
	c->sumsha = NULL;
}


/**
 * constraints_is_empty() - indicates whether a struct constraints is empty or
 *                          not
 * @c: the structure to test
 *
 * Return: 1 if @c is empty, 0 otherwise.
 */
LOCAL_SYMBOL
int constraints_is_empty(struct constraints * c)
{
	struct constraints empty = {0};
	return memcmp(&empty, c, sizeof(empty)) == 0;
}


/**
 * pkg_version_compare() - compare package version string
 * @v1: version string
 * @v2: version string
 *
 * This function compare the package version string is a way that take into
 * account the version number. It follows the lexicographic order excepting
 * when an numeric value is encounter. In such a case, the whole numeric
 * value is compared. In effect this ensure the result of the following
 * comparisons :
 *
 * * abcd1.3.5 > abc1.3.5
 * * abc1.3.5 < abc1.29.5
 * * abc1.30.5 > abc1.29.50
 *
 * "any" is an allowed wildcard.
 * Anything is lower and higher than "any". Comparing "any" to itself results
 * in an undefined behavior.
 *
 * * 1.0.0 <= any
 * * any <= 1.0.0
 *
 * Return: an integer less than, equal to, or greater than zero if @v1 is
 * found, respectively, to be less than, to match, or be greater than @v2.
 */
LOCAL_SYMBOL
int pkg_version_compare(char const * v1, char const * v2)
{
	int c1, c2;
	int first_diff;

	// version wildcards
	if (STR_EQUAL(v1, strlen(v1), "any")
	    || STR_EQUAL(v2, strlen(v2), "any"))
		return 0;

	// normal version processing
	do {
		c1 = *v1++;
		c2 = *v2++;

		// Compare the numeric value as a whole
		if (isdigit(c1) && isdigit(c2)) {
			// Skip leading '0' of v1
			while (c1 == '0')
				c1 = *v1++;

			// Skip leading '0' of v2
			while (c2 == '0')
				c2 = *v2++;

			// Advance while scanning a numeric value
			first_diff = 0;
			while (c1 && isdigit(c1) && c2 && isdigit(c2)) {
				if (!first_diff)
					first_diff = c1 - c2;

				c1 = *v1++;
				c2 = *v2++;
			}

			// We are leaving the numeric value. So check the
			// longest numeric value. If equal inspect the first
			// digit difference
			if (isdigit(c1) == isdigit(c2)) {
				if (!first_diff)
					continue;

				return first_diff;
			}

			// Check numeric value of v1 is longest or not
			return isdigit(c1) ? 1 : -1;
		}
	} while (c1 && c2 && c1 == c2);

	return c1 - c2;
}


/**
 * pkgdep_match_version() - test if a package meet requirement of dependency
 * @pkg:        pointer to package (may be NULL)
 * @dep:        pointer to dependency
 *
 * NOTE: if @pkg is not NULL, it is assumed to have the same name as @dep.
 *
 * Return: 1 if @pkg is not NULL and meet requirement of @dep, 0 otherwise
 */
static
int pkgdep_match_version(const struct pkgdep* dep,
                         const struct binpkg* pkg)
{
	return (pkg != NULL
	        && pkg_version_compare(pkg->version, dep->max_version) <= 0
	        && pkg_version_compare(dep->min_version, pkg->version) <= 0);
}


/**************************************************************************
 *                                                                        *
 *                          Reverse dependency                            *
 *                                                                        *
 **************************************************************************/

static
void rdepends_init(struct rdepends* rdeps)
{
	*rdeps = (struct rdepends) {0};
}


static
void rdepends_deinit(struct rdepends* rdeps)
{
	free(rdeps->ids);
	rdepends_init(rdeps);
}


/**
 * rdepends_add() - add a package name to an set of reverse dependencies
 * @rdeps:      reverse dependencies to update
 * @pkgname_id: package name to add as reverse dependency
 *
 * This add @pkgname to the set of reverse dependencies. If @pkgname is
 * already in the set, nothing is done.
 */
static
void rdepends_add(struct rdepends* rdeps, int pkgname_id)
{
	int i, nmax;

	// Check the reverse dependency has not been added yet.
	for (i = 0; i < rdeps->num; i++) {
		if (rdeps->ids[i] == pkgname_id)
			return;
	}

	// Resize if too small
	if (rdeps->num+1 > rdeps->nmax) {
		nmax = rdeps->nmax ? rdeps->nmax * 2 : 8;
		rdeps->ids = xx_realloc(rdeps->ids,
		                        nmax * sizeof(*rdeps->ids));
		rdeps->nmax = nmax;
	}

	rdeps->ids[rdeps->num++] = pkgname_id;
}


/**************************************************************************
 *                                                                        *
 *                          Package list                                  *
 *                                                                        *
 **************************************************************************/

/**
 * pkglist_init() - initialize a new package list
 * @list:       package list struct to initialize
 * @name:       package name to which the package list will be associated
 * @id:         id attributed to package name
 *
 * Return: pointer to package list
 */
static
void pkglist_init(struct pkglist* list, const mmstr* name, int id)
{
	*list = (struct pkglist) {.pkg_name = mmstrdup(name), .id = id};
	rdepends_init(&list->rdeps);
}


/**
 * pkglist_deinit() - deinit package list and free underlying resources
 * @list:       package list to deinit
 *
 * This function free the package list as well as the package data
 */
static
void pkglist_deinit(struct pkglist* list)
{
	struct pkglist_entry * entry, * next;

	entry = list->head;
	while (entry) {
		next = entry->next;

		binpkg_deinit(&entry->pkg);
		free(entry);

		entry = next;
	}

	mmstr_free(list->pkg_name);
	rdepends_deinit(&list->rdeps);
}


/**
 * pkglist_add_or_modify() - allocate or modifyt a package to list
 * @list:       package list to modify
 * @pkg:        package source holding the field values to update
 *
 * This function search for a package whose version and sumsha field match
 * the one provided in @pkg. If one is found, the fields that are
 * repository specific (ie not present in installed package list cache) are
 * updated in the repo_index field of the package found is not set yet (ie
 * the package information comes from the installed package list).
 *
 * If no matching package can be found in @list, a new package is created
 * and added to @list.  All of its fields are initialized from @pkg.
 *
 * The value strings of fields that have been updated or set (for a new
 * package) are taken over from @pkg into the package in the list. Hence
 * those are set to NULL in @pkg.
 *
 * Return: a pointer to new package in list
 */
static
struct binpkg* pkglist_add_or_modify(struct pkglist* list, struct binpkg* pkg)
{
	struct pkglist_entry* entry;
	struct pkglist_entry** pnext;
	struct binpkg* pkg_in_list;
	const mmstr* next_version;
	int vercmp;

	// Loop over entry and check whether there is an identical package
	// (ie has the same sumsha and version).
	for (entry = list->head; entry != NULL; entry = entry->next) {
		// Check the entry match version and sumsha
		if (!mmstrequal(pkg->version, entry->pkg.version)
		    || !mmstrequal(pkg->sumsha, entry->pkg.sumsha))
			continue;

		// Update repo specific fields if repo index is not set
		pkg_in_list = &entry->pkg;
		binpkg_add_remote_resource(pkg_in_list, pkg->remote_res);

		// update srcsha if not provided (might be missing in
		// installed.yaml produced by old version of mmpack)
		if (!pkg_in_list->srcsha) {
			pkg_in_list->srcsha = pkg->srcsha;
			pkg->srcsha = NULL;
		}

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
	binpkg_init(pkg, NULL);

	list->num_pkg++;

	return &entry->pkg;
}

/**************************************************************************
 *                                                                        *
 *              Iterator of packages in a binary package index            *
 *                                                                        *
 **************************************************************************/

LOCAL_SYMBOL
struct binpkg* pkg_iter_next(struct pkg_iter* pkg_iter)
{
	struct pkglist* curr_list;
	struct pkglist_entry* elt;

	elt = pkg_iter->pkglist_elt;
	if (elt == NULL) {
		do {
			curr_list = ++pkg_iter->curr_list;
			if (curr_list >= pkg_iter->list_ptr_bound)
				return NULL;

			elt = curr_list->head;
		} while (elt == NULL);
	}

	pkg_iter->pkglist_elt = elt->next;
	return &elt->pkg;
}


LOCAL_SYMBOL
struct binpkg* pkg_iter_first(struct pkg_iter* pkg_iter,
                              const struct binindex* binindex)
{
	struct pkglist* first_list;
	int num_pkgname;

	num_pkgname = binindex->num_pkgname;
	first_list = binindex->pkgname_table;

	pkg_iter->curr_list = first_list - 1;
	pkg_iter->list_ptr_bound = first_list + num_pkgname;
	pkg_iter->pkglist_elt = NULL;

	return pkg_iter_next(pkg_iter);
}


/**************************************************************************
 *                                                                        *
 *                      Binary package index                              *
 *                                                                        *
 **************************************************************************/


static
int binpkg_cmp(const void * v1, const void * v2)
{
	const struct binpkg * pkg1, * pkg2;
	int res;

	pkg1 = *((const struct binpkg**) v1);
	pkg2 = *((const struct binpkg**) v2);

	res = strcmp(pkg1->name, pkg2->name);

	if (res == 0)
		res = pkg_version_compare(pkg1->version, pkg2->version);

	return res;
}


/**
 * install_state_sorted_pkgs - get a sorted array of packages
 * @is:   struct install_state
 *
 * The array is allocated in the heap memory and the caller has the
 * responsibility to free it.
 *
 * Returns: a NULL terminated sorted array of packages
 */
LOCAL_SYMBOL
const struct binpkg** install_state_sorted_pkgs(struct install_state * is)
{
	struct it_iterator iter;
	struct it_entry* entry;
	const struct binpkg ** pkgs;
	int cnt, i = 0;

	cnt = is->pkg_num;
	pkgs = xx_malloc(sizeof(*pkgs) * (cnt + 1));

	i = 0;
	entry = it_iter_first(&iter, &is->idx);
	while (entry != NULL && i < cnt) {
		pkgs[i++] = entry->value;
		entry = it_iter_next(&iter);
	}

	qsort(pkgs, cnt, sizeof(struct binpkg*), binpkg_cmp);
	pkgs[cnt] = NULL;

	return pkgs;
}


/**
 * binindex_sorted_pkgs - get a sorted array of packages
 * @binindex:    The binindex to get the packages from.
 *
 * It allocates a NULL terminated array in the heap memory. It is
 * responsibility of the caller to free it.
 *
 * Returns:      A NULL terminated array of packages.
 */
LOCAL_SYMBOL
struct binpkg** binindex_sorted_pkgs(struct binindex * binindex)
{
	struct pkg_iter iter;
	struct binpkg ** pkgs, * pkg;
	int cnt, i = 0;

	cnt = binindex->pkg_num;
	pkgs = xx_malloc(sizeof(*pkgs) * (cnt + 1));

	i = 0;
	pkg = pkg_iter_first(&iter, binindex);
	while (pkg != NULL && i < cnt) {
		pkgs[i++] = pkg;
		pkg = pkg_iter_next(&iter);
	}

	qsort(pkgs, cnt, sizeof(struct binpkg*), binpkg_cmp);
	pkgs[cnt] = NULL;

	return pkgs;
}


LOCAL_SYMBOL
void binindex_init(struct binindex* binindex)
{
	*binindex = (struct binindex) {0};
	indextable_init(&binindex->pkgname_idx, -1, -1);
}


LOCAL_SYMBOL
void binindex_deinit(struct binindex* binindex)
{
	int i;

	for (i = 0; i < binindex->num_pkgname; i++)
		pkglist_deinit(&binindex->pkgname_table[i]);

	free(binindex->pkgname_table);
	binindex->pkgname_table = NULL;

	indextable_deinit(&binindex->pkgname_idx);

	binindex->num_pkgname = 0;
	binindex->pkg_num = 0;
}


/**
 * binindex_get_pkglist() - obtain a package list of given name
 * @binindex:    binary index to query
 * @pkg_name:    package name whose list is query
 *
 * Return: a pointer to the package list associated to @pkg_name if found.
 * NULL if no package with name @pkg_name can be found in the binary index.
 */
static
struct pkglist* binindex_get_pkglist(const struct binindex* binindex,
                                     const mmstr* pkg_name)
{
	struct it_entry* entry;

	entry = indextable_lookup(&binindex->pkgname_idx, pkg_name);
	if (entry == NULL)
		return NULL;

	return binindex->pkgname_table + entry->ivalue;
}


/**
 * binindex_lookup() - get a package according to some constraints.
 * @binindex:  binary package index
 * @name:      package name
 * @c:         constraints permitting to select an appropriate package
 *
 * Return: NULL on error, a pointer to the found package otherwise
 */
LOCAL_SYMBOL
struct binpkg const* binindex_lookup(struct binindex* binindex,
                                     mmstr const * name,
                                     struct constraints const * c)
{
	struct binpkg * pkg;
	struct pkglist_entry * pkgentry;
	struct pkglist * list;
	char const * version = (c && c->version) ? c->version : "any";

	list = binindex_get_pkglist(binindex, name);
	if (list == NULL)
		return NULL;

	for (pkgentry = list->head; pkgentry; pkgentry = pkgentry->next) {
		pkg = &pkgentry->pkg;

		if (c && c->sumsha && mmstrcmp(c->sumsha, pkg->sumsha))
			continue;

		if (c && c->repo && !binpkg_is_provided_by_repo(pkg, c->repo))
			continue;

		if (pkg_version_compare(version, pkg->version))
			continue;

		return pkg;
	}

	return NULL;
}


/**
 * binindex_is_pkg_upgradeable() - indicates if an installed package could be
 *                                 upgraded or not.
 * @binindex:    binary package index
 * @pkg:         package
 *
 * Return: 1 if the package is upgradeable, 0 otherwise.
 */
LOCAL_SYMBOL
int binindex_is_pkg_upgradeable(struct binindex const * binindex,
                                struct binpkg const * pkg)
{
	struct pkglist * list;

	list = binindex_get_pkglist(binindex, pkg->name);
	// when using is_upgradeable, an installed package is given in argument,
	// hence the list of packages named pkg->name cannot be NULL
	mm_check(list != NULL);

	return (pkg_version_compare(list->head->pkg.version, pkg->version) > 0);
}


/**
 * binindex_get_pkgname_id() - obtain a id of a package name
 * @binindex:    binary index to query
 * @name:        package name whose is queried
 *
 * This function will obtain the id of a package name. It cannot fail
 * because even if the package name is not yet known in @binindex, an empty
 * package list will be created and an id reserved for @name.
 *
 * Return: the id of package name.
 */
LOCAL_SYMBOL
int binindex_get_pkgname_id(struct binindex* binindex, const mmstr* name)
{
	struct pkglist* new_tab;
	struct pkglist* pkglist;
	struct indextable* idx;
	struct it_entry* entry;
	struct it_entry defval = {.key = name, .ivalue = -1};
	size_t tab_sz;
	int pkgname_id;

	idx = &binindex->pkgname_idx;
	entry = indextable_lookup_create_default(idx, name, defval);
	pkgname_id = entry->ivalue;

	// Create package list if not existing yet
	if (pkgname_id == -1) {
		// Rezize pkgname table
		tab_sz = (binindex->num_pkgname + 1) * sizeof(*new_tab);
		new_tab = xx_realloc(binindex->pkgname_table, tab_sz);
		binindex->pkgname_table = new_tab;

		// Assign an new pkgname id
		pkgname_id = binindex->num_pkgname++;

		// Initialize the package list associated to id
		pkglist = &binindex->pkgname_table[pkgname_id];
		pkglist_init(pkglist, name, pkgname_id);

		// Reference the new package list in the index table
		entry->ivalue = pkgname_id;
		entry->key = pkglist->pkg_name;
	}

	return pkgname_id;
}


/**
 * binindex_compile_upgrade() - compile a dep representing a pkg upgrade
 * @binindex:   binary index with which the dependencies must be compiled
 * @pkg:        package to upgrade in the context of @binindex
 * @buff:       struct buffer on which the dependency must be appended
 *
 * This function synthetizes the possible package upgrade for @pkg.
 *
 * Return: the pointer to compiled dependency located on data buffer managed by
 * @buff. If the package cannot be upgraded (for example is already the
 * latest), NULL is returned and size of @buff won't be updated
 */
LOCAL_SYMBOL
struct compiled_dep* binindex_compile_upgrade(const struct binindex* binindex,
                                              struct binpkg* pkg,
                                              struct buffer* buff)
{
	struct pkglist_entry* entry;
	struct pkglist* list;
	struct compiled_dep* compdep;
	size_t need_size, used_size;
	short num_pkg;

	list = &binindex->pkgname_table[pkg->name_id];

	// Ensure we can add a new element that could have as many
	// possible package as there is in the pkglist
	need_size = compiled_dep_size(list->num_pkg-1);
	compdep = buffer_reserve_data(buff, need_size);

	// Fill compiled dependency by inspected version of all package
	// sharing the same package name.
	num_pkg = 0;
	for (entry = list->head; entry != NULL; entry = entry->next) {
		if (&entry->pkg == pkg)
			break;

		compdep->pkgs[num_pkg++] = &entry->pkg;
	}

	if (!num_pkg)
		return NULL;

	used_size = compiled_dep_size(num_pkg);

	compdep->pkgname_id = list - binindex->pkgname_table;
	compdep->num_pkg = num_pkg;
	compdep->next_entry_delta = used_size / sizeof(*compdep);

	// Advance in the buffer after the compiled_dep we have just set
	buffer_inc_size(buff, used_size);

	return compdep;
}


/**
 * binindex_compile_dep() - compile a dependency on a buffer
 * @binindex:   binary index with which the dependencies must be compiled
 * @dep:        dependency to compile into context of @binindex
 * @buff:       struct buffer on which the dependency must be appended
 *
 * This function synthetizes the information pointed to by @dep and
 * confront it with the content of binary index specified by @binindex.
 * This essentially generates an array of package pointer of @binindex
 * which meet the version requirements of @dep.
 *
 * Return: the pointer to compiled dependency located on data buffer managed
 * by @buff. NULL is returned if the package named in @dep could not found,
 * or if no matching package could be found.
 */
LOCAL_SYMBOL
struct compiled_dep* binindex_compile_dep(const struct binindex* binindex,
                                          const struct pkgdep* dep,
                                          struct buffer* buff)
{
	struct pkglist_entry* entry;
	struct pkglist* list;
	size_t need_size, used_size;
	struct compiled_dep* compdep;
	short num_pkg;

	list = binindex_get_pkglist(binindex, dep->name);
	if (!list)
		return NULL;

	// Ensure we can add a new element that could have as many
	// possible package as there is in the pkglist
	need_size = compiled_dep_size(list->num_pkg);
	compdep = buffer_reserve_data(buff, need_size);

	// Fill compiled dependency by inspected version of all package
	// sharing the same package name.
	num_pkg = 0;
	for (entry = list->head; entry != NULL; entry = entry->next) {
		if (!pkgdep_match_version(dep, &entry->pkg))
			continue;

		compdep->pkgs[num_pkg++] = &entry->pkg;
	}

	// Gone through all packages without finding a matching candidate
	if (num_pkg == 0)
		return NULL;

	used_size = compiled_dep_size(num_pkg);

	compdep->pkgname_id = list - binindex->pkgname_table;
	compdep->num_pkg = num_pkg;
	compdep->next_entry_delta = used_size / sizeof(*compdep);

	// Advance in the buffer after the compiled_dep we have just set
	buffer_inc_size(buff, used_size);

	return compdep;
}


/**
 * compile_package() - compile a package on an buffer
 * @binindex: binary index with which the dependencies must be compiled
 * @pkg: package to compile into context of @binindex
 * @buff: struct buffer on which the dependency must be appended
 *
 * Return: the pointer to compiled dependency located on data buffer managed
 * by @buff.
 */
LOCAL_SYMBOL
struct compiled_dep* compile_package(const struct binindex* binindex,
                                     struct binpkg const * pkg,
                                     struct buffer* buff)
{
	size_t size;
	struct compiled_dep* compdep;
	struct pkglist* list;

	list = binindex_get_pkglist(binindex, pkg->name);
	assert(list != NULL);

	size = compiled_dep_size(1);
	compdep = buffer_reserve_data(buff, size);
	compdep->pkgs[0] = (struct binpkg*) pkg;
	compdep->pkgname_id = list - binindex->pkgname_table;
	compdep->num_pkg = 1;
	compdep->next_entry_delta = size / sizeof(*compdep);

	/* Advance in the buffer after the compiled_dep we have just set */
	buffer_inc_size(buff, size);

	return compdep;
}


/**
 * binindex_compile_pkgdeps() - get buffer of dependencies of package
 * @binindex:   binary index with which the dependencies must be compiled
 * @pkg:        package whose dependencies must be compiled
 *
 * This function function will inspect the direct dependencies listed in
 * @pkg and will generate a buffer containing the serialized compiled
 * version of dependencies. In other word, for each dependency of @pkg, the
 * resulting buffer will contain a serialized struct compiled_dep
 * representing the synthetized information in the dependency.
 *
 * To iterate over all dependencies in the returned buffer, use
 * compiled_dep_next().
 *
 * Note that the returned buffer is cached in a field of @pkg. Hence
 * subsequent calls to the function will return the cached buffer and no
 * computation are involved. This also implies that this function must be
 * only once all package from all repositories have been loaded in
 * @binindex, otherwise the buffer will contain only partial results.
 *
 * return: buffer of serialized compiled dependencies of @pkg among the
 * packages known in @binindex.
 */
LOCAL_SYMBOL
struct compiled_dep* binindex_compile_pkgdeps(const struct binindex* binindex,
                                              struct binpkg* pkg,
                                              int * flag)
{
	struct pkgdep* dep;
	struct buffer buff;
	// Init to NULL only to fix an illegitimate warning in gcc with
	// Wmaybe-uninitialized
	struct compiled_dep* compdep = NULL;

	// If no dependencencies return NULL
	if (!pkg->mpkdeps)
		return NULL;

	// If already compiled, return the result
	if (pkg->compdep != NULL)
		return pkg->compdep;

	buffer_init(&buff);

	// Compile all the dependencies, stacked in a single buffer
	for (dep = pkg->mpkdeps; dep != NULL; dep = dep->next) {
		compdep = binindex_compile_dep(binindex, dep, &buff);
		if (compdep == NULL) {
			printf("Unmet dependency: %s [%s -> %s]\n",
			       dep->name, dep->min_version, dep->max_version);
			*flag |= SOLVER_ERROR;
			return NULL;
		}
	}

	// Set last element as termination of the list
	if (compdep != NULL)
		compdep->next_entry_delta = 0;

	pkg->compdep = buffer_take_data_ownership(&buff);

	return pkg->compdep;
}


LOCAL_SYMBOL
const int* binindex_get_potential_rdeps(const struct binindex* binindex,
                                        int pkgname_id, int* num_rdeps)
{
	struct pkglist* pkglist;

	pkglist = &binindex->pkgname_table[pkgname_id];
	*num_rdeps = pkglist->rdeps.num;
	return pkglist->rdeps.ids;
}


static
struct binpkg* binindex_add_pkg(struct binindex* binindex, struct binpkg* pkg)
{
	struct pkglist* pkglist;
	int pkgname_id, elem_num;

	pkgname_id = binindex_get_pkgname_id(binindex, pkg->name);
	pkglist = &binindex->pkgname_table[pkgname_id];

	elem_num = pkglist->num_pkg;
	pkg = pkglist_add_or_modify(pkglist, pkg);
	if (pkglist->num_pkg > elem_num)
		binindex->pkg_num++;

	return pkg;
}


/**
 * binindex_compute_rdepends() - compute reverse dependencies of binindex
 * @binindex:   binary index to update
 *
 * This function computes the reverse dependencies in a loose way: it
 * computes the reverse dependencies between the package list. In other
 * words it establishes the following link: if a name A is in the reverse
 * dependency of package list named B, there is at least one version of a
 * package of A in the binary index @binindex that depends on one or more
 * version of B.
 */
LOCAL_SYMBOL
int binindex_compute_rdepends(struct binindex* binindex)
{
	int rv;
	struct pkg_iter iter;
	struct binpkg* pkg;
	struct pkglist* pkglist;
	struct pkgdep * dep;

	rv = 0;
	pkg = pkg_iter_first(&iter, binindex);
	while (pkg != NULL) {
		// For each dependency of package, add the package name in
		// the reverse dependency of dependency's package list
		dep = pkg->mpkdeps;
		while (dep) {
			pkglist = binindex_get_pkglist(binindex, dep->name);

			/* pkglist can be null if a package file is supplied
			 * through the command line and has unmet external
			 * dependencies. Those should also be passed in the
			 * same install command line */
			if (pkglist == NULL) {
				printf("Unmet dependency: %s\n", dep->name);
				rv = -1;
			} else {
				rdepends_add(&pkglist->rdeps, pkg->name_id);
			}

			dep = dep->next;
		}

		pkg = pkg_iter_next(&iter);
	}

	return rv;
}


/**************************************************************************
 *                                                                        *
 *                      YAML parsing of binary index                      *
 *                                                                        *
 **************************************************************************/
enum field_type {
	FIELD_UNKNOWN = -1,
	FIELD_VERSION = 0,
	FIELD_FILENAME,
	FIELD_SHA,
	FIELD_SIZE,
	FIELD_SOURCE,
	FIELD_DESC,
	FIELD_SUMSHA,
	FIELD_GHOST,
	FIELD_SRCSHA,
};

static
const char* scalar_field_names[] = {
	[FIELD_VERSION] = "version",
	[FIELD_FILENAME] = "filename",
	// sha256: hash of the mpk file (useful only to check the download)
	[FIELD_SHA] = "sha256",
	[FIELD_SIZE] = "size",
	[FIELD_SOURCE] = "source",
	[FIELD_DESC] = "description",
	// sumsha256sums: hash of the MMPACK/sha256sums which is an
	// invariant of the unpacked files collectively. This is used to
	// assess that packages from different source are actually the
	// same, even in their installed (unpacked) form
	[FIELD_SUMSHA] = "sumsha256sums",
	[FIELD_GHOST] = "ghost",
	[FIELD_SRCSHA] = "srcsha256",
};


static
int get_yaml_bool_value(const char* value, size_t valuelen)
{
	char tmp[8] = "";

	if (valuelen > sizeof(tmp) - 1)
		goto error;

	memcpy(tmp, value, valuelen);
	if (mm_strcasecmp(tmp, "true") == 0)
		return 1;
	else if (mm_strcasecmp(tmp, "false") == 0)
		return 0;
	else if (mm_strcasecmp(tmp, "on") == 0)
		return 1;
	else if (mm_strcasecmp(tmp, "off") == 0)
		return 0;
	else if (mm_strcasecmp(tmp, "yes") == 0)
		return 1;
	else if (mm_strcasecmp(tmp, "no") == 0)
		return 0;
	else if (mm_strcasecmp(tmp, "y") == 0)
		return 1;
	else if (mm_strcasecmp(tmp, "n") == 0)
		return 0;

error:
	mm_raise_error(EINVAL, "invalid bool value: %.*s", valuelen, value);
	return -1;
}


static
enum field_type get_scalar_field_type(const char* key)
{
	int i;

	for (i = 0; i < MM_NELEM(scalar_field_names); i++) {
		if (strcmp(key, scalar_field_names[i]) == 0)
			return i;
	}

	return FIELD_UNKNOWN;
}


static
int binpkg_set_scalar_field(struct binpkg * pkg,
                            enum field_type type,
                            const char* value,
                            size_t valuelen,
                            const struct repo* repo)
{
	const mmstr** field = NULL;
	struct remote_resource* res;
	int bval;

	switch (type) {
	case FIELD_VERSION:
		field = &pkg->version;
		break;

	case FIELD_FILENAME:
		res = binpkg_get_or_create_remote_res(pkg, repo);
		field = &res->filename;
		break;

	case FIELD_SHA:
		res = binpkg_get_or_create_remote_res(pkg, repo);
		field = &res->sha256;
		break;

	case FIELD_SOURCE:
		field = &pkg->source;
		break;

	case FIELD_DESC:
		field = &pkg->desc;
		break;

	case FIELD_SUMSHA:
		field = &pkg->sumsha;
		break;

	case FIELD_SRCSHA:
		field = &pkg->srcsha;
		break;

	case FIELD_GHOST:
		bval = get_yaml_bool_value(value, valuelen);
		if (bval == -1)
			return -1;

		binpkg_update_flags(pkg, MMPKG_FLAGS_GHOST, bval);
		return 0;

	case FIELD_SIZE:
		res = binpkg_get_or_create_remote_res(pkg, repo);
		res->size = atoi(value);
		return 0;

	default:
		return -1;
	}

	mmstr_free(*field);
	*field = mmstr_malloc_copy(value, valuelen);

	return 0;
}


static
void binpkg_add_dependency(struct binpkg * pkg, struct pkgdep * dep)
{
	dep->next = pkg->mpkdeps;
	pkg->mpkdeps = dep;
}


/* parse a single mmpack or system dependency
 * eg:
 *   pkg-b: [0.0.2, any]
 */
static
int mmpack_parse_dependency(struct parsing_ctx* ctx,
                            struct binpkg * pkg,
                            struct pkgdep * dep)
{
	int exitvalue;
	yaml_token_t token;
	char const * val;

	exitvalue = -1;
	while (1) {
		if (!yaml_parser_scan(&ctx->parser, &token))
			goto exit;

		switch (token.type) {
		case YAML_NO_TOKEN:
			goto exit;

		case YAML_FLOW_SEQUENCE_END_TOKEN:
			if (dep->min_version != NULL &&
			    dep->max_version != NULL)
				exitvalue = 0;

			goto exit;

		case YAML_SCALAR_TOKEN:
			val = (char const*) token.data.scalar.value;
			if (dep->min_version == NULL) {
				dep->min_version = mmstr_malloc_from_cstr(val);
			} else {
				if (dep->max_version != NULL)
					goto exit;

				dep->max_version = mmstr_malloc_from_cstr(val);
			}

			break;

		default: /* ignore */
			break;
		}

		yaml_token_delete(&token);
	}

exit:
	yaml_token_delete(&token);

	if (exitvalue == 0) {
		binpkg_add_dependency(pkg, dep);
	}

	return exitvalue;
}


/*
 * parse a list of mmpack or system dependency
 * eg:
 *   depends:
 *     pkg-b: [0.0.2, any]
 *     pkg-d: [0.0.4, 0.0.4]
 *     ...
 */
static
int mmpack_parse_deplist(struct parsing_ctx* ctx,
                         struct binpkg * pkg)
{
	int exitvalue, type;
	yaml_token_t token;
	struct pkgdep * dep;

	exitvalue = 0;
	dep = NULL;
	type = -1;
	while (1) {
		if (!yaml_parser_scan(&ctx->parser, &token))
			goto exit;

		switch (token.type) {
		case YAML_NO_TOKEN:
			goto error;

		case YAML_STREAM_END_TOKEN:
			goto exit;

		case YAML_FLOW_MAPPING_END_TOKEN:
		case YAML_BLOCK_END_TOKEN:
			goto exit;

		case YAML_KEY_TOKEN:
			type = YAML_KEY_TOKEN;
			break;

		case YAML_VALUE_TOKEN:
			type = YAML_VALUE_TOKEN;
			break;

		case YAML_FLOW_SEQUENCE_START_TOKEN:
			if (dep == NULL)
				goto exit;

			exitvalue = mmpack_parse_dependency(ctx, pkg, dep);
			if (exitvalue != 0)
				goto exit;

			dep = NULL;
			type = -1;

			break;

		case YAML_SCALAR_TOKEN:
			switch (type) {
			case YAML_KEY_TOKEN:
				if (dep != NULL)
					goto exit;

				dep = pkgdep_create(
					(char const*)token.data.scalar.value);
				break;

			default:
				pkgdep_destroy(dep);
				dep = NULL;
				type = -1;
				break;
			}

			break;

		default: /* ignore */
			break;
		}

		yaml_token_delete(&token);
	}

error:  /* reach end of file prematurely */
	exitvalue = -1;

exit:
	yaml_token_delete(&token);
	if (dep != NULL)  /* dep is set to NULL after being used */
		pkgdep_destroy(dep);

	return exitvalue;
}

/*
 * parse a list of mmpack or system dependency
 * eg:
 *   sysdepends:
 *     - pkg-b
 *     - pkg-d (>= 1.0.0)
 *     ...
 */
static
int mmpack_parse_sysdeplist(struct parsing_ctx* ctx,
                            struct binpkg * pkg)
{
	int exitvalue;
	yaml_token_t token;
	char const * expr;

	exitvalue = 0;
	while (1) {
		if (!yaml_parser_scan(&ctx->parser, &token))
			goto exit;

		switch (token.type) {
		case YAML_NO_TOKEN:
			goto error;
		case YAML_FLOW_SEQUENCE_END_TOKEN:
		case YAML_BLOCK_END_TOKEN:
		case YAML_KEY_TOKEN:
			goto exit;

		case YAML_SCALAR_TOKEN:
			expr = (char const*) token.data.scalar.value;
			strlist_add(&pkg->sysdeps, expr);
			break;

		default: /* ignore */
			break;
		}

		yaml_token_delete(&token);
	}

error:  /* reach end of file prematurely */
	exitvalue = -1;

exit:
	yaml_token_delete(&token);
	return exitvalue;
}


/* parse a single package entry */
static
int mmpack_parse_index_package(struct parsing_ctx* ctx, struct binpkg * pkg)
{
	int exitvalue, type;
	yaml_token_t token;
	char const * data;
	size_t data_len;
	enum field_type scalar_field;

	exitvalue = 0;
	data = NULL;
	type = -1;
	scalar_field = FIELD_UNKNOWN;
	do {
		if (!yaml_parser_scan(&ctx->parser, &token))
			goto error;

		switch (token.type) {
		case YAML_NO_TOKEN:
			goto error;

		case YAML_BLOCK_END_TOKEN:
			if (binpkg_check_valid(pkg, !ctx->repo ? 0 : 1))
				goto error;

			goto exit;

		case YAML_KEY_TOKEN:
			type = YAML_KEY_TOKEN;
			break;

		case YAML_VALUE_TOKEN:
			type = YAML_VALUE_TOKEN;
			break;

		case YAML_SCALAR_TOKEN:
			data = (char const*) token.data.scalar.value;
			data_len = token.data.scalar.length;

			switch (type) {
			case YAML_KEY_TOKEN:
				if (STR_EQUAL(data, data_len, "depends")) {
					if (mmpack_parse_deplist(ctx, pkg))
						goto error;
				} else if (STR_EQUAL(data, data_len,
				                     "sysdepends")) {
					if (mmpack_parse_sysdeplist(ctx, pkg))
						goto error;
				} else {
					scalar_field = get_scalar_field_type(
						data);
					type = -1;
				}

				break;

			case YAML_VALUE_TOKEN:
				if ((scalar_field != FIELD_UNKNOWN) &&
				    token.data.scalar.length) {
					if (binpkg_set_scalar_field(pkg,
					                            scalar_field,
					                            data,
					                            data_len,
					                            ctx->repo))
						goto error;
				}

			/* fallthrough */
			default:
				scalar_field = FIELD_UNKNOWN;
				data = NULL;
				type = -1;
				break;
			}

			break;

		default: /* ignore */
			break;
		}

		yaml_token_delete(&token);
	} while (token.type != YAML_STREAM_END_TOKEN);

error:
	exitvalue = -1;

exit:
	yaml_token_delete(&token);
	return exitvalue;
}


/*
 * Entry point to parse a whole binary index file
 *
 * pkg-x:
 *   depends:
 *     pkg-y
 *   sysdepends:
 *     sys-x
 *   version: 0.1.2
 * pkg-y:
 *   ...
 */
static
int mmpack_parse_index(struct parsing_ctx* ctx, struct binindex * binindex)
{
	int exitvalue, type;
	yaml_token_t token;
	struct binpkg pkg;
	mmstr* name = NULL;
	const char* valuestr;
	int valuelen;

	binpkg_init(&pkg, NULL);
	exitvalue = -1;
	type = -1;
	while (1) {
		if (!yaml_parser_scan(&ctx->parser, &token))
			goto exit;

		switch (token.type) {
		case YAML_NO_TOKEN:
			goto exit;

		case YAML_STREAM_END_TOKEN:
			exitvalue = 0;
			goto exit;

		case YAML_KEY_TOKEN:
			type = YAML_KEY_TOKEN;
			break;

		case YAML_SCALAR_TOKEN:
			valuestr = (const char*)token.data.scalar.value;
			valuelen = token.data.scalar.length;
			if (type == YAML_KEY_TOKEN) {
				name = mmstr_copy_realloc(name,
				                          valuestr,
				                          valuelen);
				binpkg_init(&pkg, name);
				exitvalue =
					mmpack_parse_index_package(ctx, &pkg);
				if (exitvalue != 0)
					goto exit;

				binindex_add_pkg(binindex, &pkg);
				binpkg_deinit(&pkg);
			}

			type = -1;
			break;

		default: /* ignore */
			break;
		}

		yaml_token_delete(&token);
	}

	exitvalue = 0;

exit:
	yaml_token_delete(&token);
	binpkg_deinit(&pkg);
	mmstr_free(name);

	return exitvalue;
}


static
mmstr const* parse_package_info(struct binpkg * pkg, struct buffer * buffer)
{
	int rv;
	char const * delim;
	char const * base = buffer->base;
	struct parsing_ctx ctx = {.repo = NULL};

	if (!yaml_parser_initialize(&ctx.parser)) {
		mm_raise_error(ENOMEM, "failed to init yaml parse");
		return NULL;
	}

	yaml_parser_set_input_string(&ctx.parser, (unsigned char const*) base,
	                             buffer->size);
	rv = mmpack_parse_index_package(&ctx, pkg);

	yaml_parser_delete(&ctx.parser);
	if (rv != 0)
		return NULL;

	/* additionally, load the package name from the buffer */
	delim = strchr(base, ':');
	pkg->name = mmstr_malloc_copy(base, delim - base);
	pkg->remote_res->repo = NULL;

	return pkg->name;
}


/**
 * add_pkgfile_to_binindex() - add local mmpack package file to binindex
 * @binindex: initialized mmpack binindex
 * @filename: path to the mmpack archive
 *
 * Return: a pointer to the binpkg structure that has been inserted.
 * It belongs the the binindex and will be cleansed during mmpack global
 * cleanup.
 */
LOCAL_SYMBOL
struct binpkg* add_pkgfile_to_binindex(struct binindex* binindex,
                                       char const * filename)
{
	struct buffer buffer;
	struct binpkg * pkg;
	struct binpkg tmppkg;
	struct remote_resource* res;
	mmstr const * name;
	mmstr* hash;

	pkg = NULL;
	name = NULL;
	buffer_init(&buffer);
	binpkg_init(&tmppkg, NULL);
	res = binpkg_get_or_create_remote_res(&tmppkg, NULL);
	res->filename = mmstr_malloc_from_cstr(filename);
	res->sha256 = hash = mmstr_malloc(SHA_HEXSTR_LEN);

	if (pkg_get_mmpack_info(filename, &buffer)
	    || sha_compute(hash, filename, NULL, 1))
		goto exit;

	name = parse_package_info(&tmppkg, &buffer);
	if (name != NULL)
		pkg = binindex_add_pkg(binindex, &tmppkg);

exit:
	mmstr_free(name);
	binpkg_deinit(&tmppkg);
	buffer_deinit(&buffer);
	return pkg;
}


/**
 * binindex_populate() - populate package database from package list
 * @binindex:   binary package index to populate
 * @index_filename: repository package list file
 * @repo:       pointer to repository from which the binary index is read (use
 *              NULL if package list is the list of installed package)
 *
 * Return: 0 in case of success, -1 otherwise
 */
LOCAL_SYMBOL
int binindex_populate(struct binindex* binindex, char const * index_filename,
                      const struct repo* repo)
{
	int rv = -1;
	FILE * index_fh;
	struct parsing_ctx ctx = {.repo = repo};

	if (!yaml_parser_initialize(&ctx.parser))
		return mm_raise_error(ENOMEM, "failed to init yaml parse");

	index_fh = fopen(index_filename, "r");
	if (index_fh == NULL) {
		mm_raise_error(EINVAL,
		               "failed to open given binary index file");
		goto exit;
	}

	yaml_parser_set_input_file(&ctx.parser, index_fh);
	rv = mmpack_parse_index(&ctx, binindex);

	fclose(index_fh);

exit:
	yaml_parser_delete(&ctx.parser);
	return rv;
}


/**************************************************************************
 *                                                                        *
 *                              Install state                             *
 *                                                                        *
 **************************************************************************/

LOCAL_SYMBOL
int install_state_init(struct install_state* state)
{
	state->pkg_num = 0;
	return indextable_init(&state->idx, -1, -1);
}


LOCAL_SYMBOL
int install_state_copy(struct install_state* restrict dst,
                       const struct install_state* restrict src)
{
	return indextable_copy(&dst->idx, &src->idx);
}


LOCAL_SYMBOL
void install_state_deinit(struct install_state* state)
{
	indextable_deinit(&state->idx);
}


/**
 * install_state_get_pkg() - query the package installed under a name
 * @state:      install state to query
 * @name:       package name to query
 *
 * Return: a pointer to the struct binpkg installed if found, NULL if no package
 * with @name is in the install state.
 */
LOCAL_SYMBOL
const struct binpkg* install_state_get_pkg(const struct install_state* state,
                                           const mmstr* name)
{
	struct it_entry* entry;

	entry = indextable_lookup(&state->idx, name);
	if (entry == NULL)
		return NULL;

	return entry->value;
}


/**
 * install_state_add_pkg() - add or replace package
 * @state:      install state to modify
 * @pkg:        package to add to install state
 */
LOCAL_SYMBOL
void install_state_add_pkg(struct install_state* state,
                           const struct binpkg* pkg)
{
	struct it_entry* entry;

	entry = indextable_lookup_create(&state->idx, pkg->name);
	entry->value = (void*)pkg;
	state->pkg_num++;
}


/**
 * install_state_rm_pkgname() - remove package from the install state
 * @state:      install state to modify
 * @pkgname:    name of package to remove from @state
 */
LOCAL_SYMBOL
void install_state_rm_pkgname(struct install_state* state,
                              const mmstr* pkgname)
{
	if (!indextable_remove(&state->idx, pkgname))
		state->pkg_num--;
}


LOCAL_SYMBOL
void install_state_save_to_index(struct install_state* state, FILE* fp)
{
	struct it_iterator iter;
	struct it_entry* entry;
	const struct binpkg* pkg;

	entry = it_iter_first(&iter, &state->idx);
	while (entry) {
		pkg = entry->value;
		binpkg_save_to_index(pkg, fp);
		entry = it_iter_next(&iter);
	}
}


/**
 * install_state_fill_lookup_table() - fill an table of installed package
 * @state:      install state to dump
 * @binindex:   binary index associated with install state (used for package
 *              name to id resolution)
 * @installed:  lookup table of length binindex->num_pkgname
 *
 * This function will fill a lookup table @installed whose each element
 * correspond to the package installed for a package name id if one is
 * installed, NULL if not.
 */
LOCAL_SYMBOL
void install_state_fill_lookup_table(const struct install_state* state,
                                     struct binindex* binindex,
                                     struct binpkg** installed)
{
	struct it_iterator iter;
	struct it_entry* entry;
	struct binpkg* pkg;
	int id;

	// Set initially to all uninstalled
	memset(installed, 0, binindex->num_pkgname * sizeof(*installed));

	// Loop over the indextable... For each package package installed
	// update the element in @installed table corresponding to its
	// package id
	entry = it_iter_first(&iter, &state->idx);
	while (entry) {
		pkg = entry->value;
		id = binindex_get_pkgname_id(binindex, pkg->name);
		installed[id] = pkg;
		entry = it_iter_next(&iter);
	}
}



/**************************************************************************
 *                                                                        *
 *                      Reverse dependency iterator                       *
 *                                                                        *
 **************************************************************************/

/**
 * inst_rdeps_iter_first() - initialize iterator of a set of reverse
 * dependencies
 * @iter:       pointer to an iterator structure
 * @pkg:        package whose reverse dependencies are requested
 * @binindex:   binary package index
 * @inst_lut:   lookup table of installed package, used combined with
 *              @binindex, to test which installed package is depending
 *              on @pkg.
 *
 * Return: the pointer to first package in the set of reverse dependencies
 * of @pkg if not empty, NULL otherwise.
 */
LOCAL_SYMBOL
const struct binpkg* inst_rdeps_iter_first(struct inst_rdeps_iter* iter,
                                           const struct binpkg* pkg,
                                           const struct binindex* binindex,
                                           struct binpkg** inst_lut)
{
	const struct pkglist* list;

	assert(pkg->name_id < binindex->num_pkgname);

	list = &binindex->pkgname_table[pkg->name_id];

	*iter = (struct inst_rdeps_iter) {
		.binindex = binindex,
		.install_lut = inst_lut,
		.rdeps_ids = list->rdeps.ids,
		.rdeps_index = list->rdeps.num,
		.pkgname_id = list->id,
	};

	return inst_rdeps_iter_next(iter);
}


/**
 * inst_rdeps_iter_next() - get next element in a set of reverse dependencies
 * @iter:       pointer to an initialized iterator structure
 *
 * This function return the next reverse dependency in the iterator @iter
 * initialized by inst_rdeps_iter_first().
 *
 * NOTE: It is guaranteed than no elements in the reverse dependency set can
 * be missed even if the install_state element passed in argument to
 * inst_rdeps_iter_first() is modified between two calls to
 * inst_rdeps_iter_next() or after inst_rdeps_iter_first() using same @iter
 * pointer, PROVIDED that the modification done in install state is only package
 * removals.
 *
 * Return: the pointer to next package in the set of reverse dependencies if
 * it is not the last item, NULL otherwise.
 */
LOCAL_SYMBOL
const struct binpkg* inst_rdeps_iter_next(struct inst_rdeps_iter* iter)
{
	struct binpkg* rdep_pkg;
	struct compiled_dep* dep;
	int flag, rdep_id;

	while (--iter->rdeps_index >= 0) {
		rdep_id = iter->rdeps_ids[iter->rdeps_index];
		rdep_pkg = iter->install_lut[rdep_id];
		if (!rdep_pkg)
			continue;

		// Loop over dependencies of candidate package to see if it
		// really depends on target package name
		flag = 0;
		dep = binindex_compile_pkgdeps(iter->binindex, rdep_pkg, &flag);
		while (dep) {
			if (dep->pkgname_id == iter->pkgname_id)
				return rdep_pkg;

			dep = compiled_dep_next(dep);
		}
	}

	return NULL;
}


/**
 * is_dependency() - determine if a package is a dependency of another package
 * @pkg:            package whose dependencies are analyzed to see whether
 *                  @dependency is a dependency of @pkg or not
 * @supposed_dep:   package to test whether it is a dependency of @pkg or not
 *
 * Return: 1 if @supposed_dep is a dependency of @pkg, otherwise returns 0.
 */
static
int is_dependency(struct binpkg const * pkg, struct binpkg const * supposed_dep)
{
	struct pkgdep * deps;

	for (deps = pkg->mpkdeps; deps != NULL; deps = deps->next) {
		if (mmstrequal(deps->name, supposed_dep->name) &&
		    pkgdep_match_version(deps, supposed_dep)) {
			return 1;
		}
	}

	return 0;
}


/**
 * rdeps_iter_first() - initialize iterator of a set of potential
 *                                reverse dependencies
 * @iter:       pointer to an iterator structure
 * @pkg:        package whose reverse dependencies are requested
 * @binindex:   binary package index
 *
 * Return: the pointer to first package in the set of reverse dependencies
 * of @pkg if not empty, NULL otherwise.
 */
LOCAL_SYMBOL
struct binpkg* rdeps_iter_first(struct rdeps_iter* iter,
                                const struct binpkg* pkg,
                                const struct binindex* binindex)
{
	const struct pkglist* list;

	assert(pkg->name_id < binindex->num_pkgname);

	list = &binindex->pkgname_table[pkg->name_id];

	*iter = (struct rdeps_iter) {
		.pkg = pkg,
		.binindex = binindex,
		.rdeps_ids = list->rdeps.ids,
		.rdeps_index = list->rdeps.num,
	};

	return rdeps_iter_next(iter);
}


/**
 * rdeps_iter_next() - get next element in a set of reverse dependencies
 * @iter:       pointer to an initialized iterator structure
 *
 * This function returns the next reverse dependency in the iterator @iter
 * initialized by rdeps_iter_first().
 *
 * Return: the pointer to next package in the set of reverse dependencies if
 * it is not the last item, NULL otherwise.
 */
LOCAL_SYMBOL
struct binpkg* rdeps_iter_next(struct rdeps_iter* iter)
{
	struct binpkg * ret;
	int id_dep;

	while (iter->curr || (iter->rdeps_ids && iter->rdeps_index > 0)) {
		if (!iter->curr) {
			id_dep = iter->rdeps_ids[--iter->rdeps_index];
			iter->curr = iter->binindex->pkgname_table[id_dep].head;
		}

		while (iter->curr) {
			ret = &iter->curr->pkg;
			iter->curr = iter->curr->next;
			if (is_dependency(ret, iter->pkg)) {
				return ret;
			}
		}
	}

	return NULL;
}

/**************************************************************************
 *                                                                        *
 *                        package listing iterator                        *
 *                                                                        *
 **************************************************************************/

/**
 * pkglist_iter_first() - init list iterator of packages with same name
 * @iter:       iterator structure to initialize
 * @pkgname:    name of all package in the list
 * @binindex:   binary package index
 *
 * Return: the pointer to the first package in the list. This can be NULL
 * if no package can be found of name @pkgname.
 */
LOCAL_SYMBOL
const struct binpkg* pkglist_iter_first(struct pkglist_iter* iter,
                                        const mmstr* pkgname,
                                        const struct binindex* binindex)
{
	struct pkglist* list;

	list = binindex_get_pkglist(binindex, pkgname);
	if (!list || !list->head)
		return NULL;

	iter->curr = list->head;
	return &list->head->pkg;
}


/**
 * pkglist_iter_next() - get next element in a list of same name packages
 * @iter:       pointer to an initialized iterator structure
 *
 * Return: the pointer to the next package in the list. This can be NULL
 * if no more package can be found of name @pkgname.
 */
LOCAL_SYMBOL
const struct binpkg* pkglist_iter_next(struct pkglist_iter* iter)
{
	struct pkglist_entry* entry = iter->curr->next;

	if (!entry)
		return NULL;

	iter->curr = entry;
	return &entry->pkg;
}
