/*
 * @mindmaze_header@
 */
#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include <assert.h>
#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <yaml.h>

#include <mmerrno.h>
#include <mmlib.h>

#include "common.h"
#include "indextable.h"
#include "mm-alloc.h"
#include "package-utils.h"

struct rdepends {
	int num;
	int nmax;
	const mmstr** names;
};

struct pkglist_entry {
	struct mmpkg pkg;
	struct pkglist_entry* next;
};

struct pkglist {
	const mmstr* pkg_name;
	struct pkglist_entry* head;
	struct rdepends rdeps;
};

struct pkg_iter {
	struct pkglist* curr_list;
	struct pkglist* list_ptr_bound;
	struct pkglist_entry* pkglist_elt;
};

struct parsing_ctx {
	yaml_parser_t parser;
	int repo_index;
};

/* standard isdigit() is locale dependent making it unnecessarily slow.
 * This macro is here to keep the semantic of isdigit() as usually known by
 * most programmer while issuing the fast implementation of it. */
#define isdigit(c)      ((c) >= '0' && (c) <= '9')


/**
 * pkg_version_compare() - compare package version string
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


static
void mmpkg_init(struct mmpkg* pkg, const mmstr* name)
{
	*pkg = (struct mmpkg) {.name = name};
	strlist_init(&pkg->sysdeps);
}


static
void mmpkg_deinit(struct mmpkg * pkg)
{
	mmstr_free(pkg->version);
	mmstr_free(pkg->filename);
	mmstr_free(pkg->sha256);
	mmstr_free(pkg->source);
	mmstr_free(pkg->desc);
	mmstr_free(pkg->sumsha);

	mmpkg_dep_destroy(pkg->mpkdeps);
	strlist_deinit(&pkg->sysdeps);

	mmpkg_init(pkg, NULL);
}


LOCAL_SYMBOL
void mmpkg_sysdeps_dump(const struct strlist* sysdeps, char const * type)
{
	const struct strlist_elt* d = sysdeps->head;

	while (d != NULL) {
		printf("\t\t [%s] %s\n", type, d->str.buf);
		d = d->next;
	}
}


LOCAL_SYMBOL
void mmpkg_dump(struct mmpkg const * pkg)
{
	printf("# %s (%s)\n", pkg->name, pkg->version);
	printf("\tdependencies:\n");
	mmpkg_dep_dump(pkg->mpkdeps, "MMP");
	mmpkg_sysdeps_dump(&pkg->sysdeps, "SYS");
	printf("\n");
}


static
int mmpkg_check_valid(struct mmpkg const * pkg, int in_repo_cache)
{
	if (  !pkg->version
	   || !pkg->sumsha
	   || !pkg->source)
		return mm_raise_error(EINVAL, "Invalid package data for %s."
		                              " Missing fields.", pkg->name);

	if (!in_repo_cache)
		return 0;

	if (  !pkg->sha256
	   || !pkg->size
	   || !pkg->filename)
		return mm_raise_error(EINVAL, "Invalid package data for %s."
		                              " Missing fields needed in"
		                              " repository package index.",
		                              pkg->name);

	return 0;
}


LOCAL_SYMBOL
void mmpkg_save_to_index(struct mmpkg const * pkg, FILE* fp)
{
	const struct strlist_elt* elt;

	fprintf(fp, "%s:\n"
	            "    version: %s\n"
	            "    source: %s\n"
	            "    sumsha256sums: %s\n",
		    pkg->name, pkg->version, pkg->source, pkg->sumsha);

	fprintf(fp, "    depends:");
	mmpkg_dep_save_to_index(pkg->mpkdeps, fp, 2/*indentation level*/);

	fprintf(fp, "    sysdepends: [");
	for (elt = pkg->sysdeps.head; elt != NULL; elt = elt->next) {
		fprintf(fp, "'%s'%s", elt->str.buf, elt->next ? ", " : "");
	}
	fprintf(fp, "]\n");
}


LOCAL_SYMBOL
struct mmpkg_dep * mmpkg_dep_create(char const * name)
{
	struct mmpkg_dep * dep = mm_malloc(sizeof(*dep));
	memset(dep, 0, sizeof(*dep));
	dep->name = mmstr_malloc_from_cstr(name);
	return dep;
}


LOCAL_SYMBOL
void mmpkg_dep_destroy(struct mmpkg_dep * dep)
{
	if (dep == NULL)
		return;

	mmstr_free(dep->name);
	mmstr_free(dep->min_version);
	mmstr_free(dep->max_version);

	if (dep->next)
		mmpkg_dep_destroy(dep->next);

	free(dep);
}


LOCAL_SYMBOL
void mmpkg_dep_dump(struct mmpkg_dep const * deps, char const * type)
{
	struct mmpkg_dep const * d = deps;

	while (d != NULL) {
		if (STR_STARTS_WITH(type, strlen(type), "SYS"))
			printf("\t\t [%s] %s\n", type, d->name);
		else
			printf("\t\t [%s] %s [%s -> %s]\n", type, d->name,
			       d->min_version, d->max_version);
		d = d->next;
	}
}


LOCAL_SYMBOL
void mmpkg_dep_save_to_index(struct mmpkg_dep const * dep, FILE* fp, int lvl)
{
	if (!dep) {
		fprintf(fp, " {}\n");
		return;
	}

	fprintf(fp, "\n");
	while (dep) {
		// Print name , minver and maxver at lvl indentation level
		// (ie 4*lvl spaces are inserted before)
		fprintf(fp, "%*s%s: [%s, %s]\n", lvl*4, " ",
		        dep->name, dep->min_version, dep->max_version);
		dep = dep->next;
	}
}


/**************************************************************************
 *                                                                        *
 *                          Reverse dependency                            *
 *                                                                        *
 **************************************************************************/

static
void rdepends_init(struct rdepends* rdeps)
{
	*rdeps = (struct rdepends){0};
}


static
void rdepends_deinit(struct rdepends* rdeps)
{
	free(rdeps->names);
	rdepends_init(rdeps);
}


/**
 * rdepends_add() - add a package name to an set of reverse dependencies
 * @rdeps:      reverse dependencies to update
 * @pkgname:    package name to add as reverse dependency
 *
 * This add @pkgname to the set of reverse dependencies. If @pkgname is
 * already in the set, nothing is done.
 */
static
void rdepends_add(struct rdepends* rdeps, const mmstr* pkgname)
{
	int i, nmax;

	// Check the reverse dependency has not been added yet. The test is
	// done simply on the pointer value because package name always
	// come from member of pkglist unique in the binary index
	for (i = 0; i < rdeps->num; i++)
		if (rdeps->names[i] == pkgname)
			return;

	// Resize if too small
	if (rdeps->num+1 > rdeps->nmax) {
		nmax = rdeps->nmax ? rdeps->nmax * 2 : 8;
		rdeps->names = mm_realloc(rdeps->names,
		                          nmax * sizeof(*rdeps->names));
		rdeps->nmax = nmax;
	}

	rdeps->names[rdeps->num++] = pkgname;
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
 *
 * Return: pointer to package list
 */
static
void pkglist_init(struct pkglist* list, const mmstr* name)
{
	*list = (struct pkglist){.pkg_name = mmstrdup(name)};
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
	struct pkglist_entry *entry, *next;

	entry = list->head;
	while (entry) {
		next = entry->next;

		mmpkg_deinit(&entry->pkg);
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
void pkglist_add_or_modify(struct pkglist* list, struct mmpkg* pkg)
{
	struct pkglist_entry* entry;
	struct mmpkg* pkg_in_list;

	// Loop over entry and check whether there is an identical package
	// (ie has the same sumsha and version).
	for (entry = list->head; entry != NULL; entry = entry->next) {
		// Check the entry match version and sumsha
		if (  !mmstrequal(pkg->version, entry->pkg.version)
		   || !mmstrequal(pkg->sumsha, entry->pkg.sumsha))
			continue;

		// Update repo specific fields if repo index is not set
		pkg_in_list = &entry->pkg;
		if (pkg_in_list->repo_index == -1) {
			pkg_in_list->repo_index = pkg->repo_index;
			pkg_in_list->size = pkg->size;
			pkg_in_list->sha256 = pkg->sha256;
			pkg_in_list->filename = pkg->filename;

			// Unset string field in source package since the
			// string are taken over by the pkg in list
			pkg->sha256 = NULL;
			pkg->filename = NULL;
		}
		return;
	}

	// Add new entry to the list
	entry = mm_malloc(sizeof(*entry));
	entry->next = list->head;
	list->head = entry;

	// copy the whole package structure
	entry->pkg = *pkg;
	entry->pkg.name = list->pkg_name;

	// reset package fields since they have been taken over by the new
	// entry
	mmpkg_init(pkg, NULL);
}

/**************************************************************************
 *                                                                        *
 *              Iterator of packages in a binary package index            *
 *                                                                        *
 **************************************************************************/

static
struct mmpkg* pkg_iter_next(struct pkg_iter* pkg_iter)
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


static
struct mmpkg* pkg_iter_first(struct pkg_iter* pkg_iter,
                             const struct binindex* binindex)
{
	struct pkglist* first_list;
	int num_pkgname;

	num_pkgname = binindex->num_pkgname;
	first_list = binindex->pkgname_table;

	pkg_iter->curr_list = first_list - 1;
	pkg_iter->list_ptr_bound = first_list + num_pkgname;
	pkg_iter->pkglist_elt = NULL;;

	return pkg_iter_next(pkg_iter);
}


/**************************************************************************
 *                                                                        *
 *                      Binary package index                              *
 *                                                                        *
 **************************************************************************/

LOCAL_SYMBOL
int binindex_foreach(struct binindex * binindex,
                     int (*cb)(struct mmpkg*, void *),
                     void * data)
{
	int rv;
	struct pkg_iter iter;
	struct mmpkg* pkg;

	pkg = pkg_iter_first(&iter, binindex);
	while (pkg != NULL) {
		rv = cb(pkg, data);
		if (rv != 0)
			return rv;
		pkg = pkg_iter_next(&iter);
	}

	return 0;
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
}


LOCAL_SYMBOL
void binindex_dump(struct binindex const * binindex)
{
	struct pkg_iter iter;
	struct mmpkg* pkg;

	pkg = pkg_iter_first(&iter, binindex);
	while (pkg != NULL) {
		mmpkg_dump(pkg);
		pkg = pkg_iter_next(&iter);
	}
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
 * binindex_get_latest_pkg() - get the latest possible version of given package
 *                             inferior to given maximum
 * binindex:    binary package index
 * name:        package name
 * max_version: inclusive maximum boundary
 *
 * Return: NULL on error, a pointer to the found package otherwise
 */
LOCAL_SYMBOL
struct mmpkg const * binindex_get_latest_pkg(struct binindex* binindex, mmstr const * name,
                                             mmstr const * max_version)
{
	struct mmpkg * pkg, * latest_pkg;
	struct pkglist_entry* pkgentry;
	struct pkglist* list;
	const char* latest_version;

	list = binindex_get_pkglist(binindex, name);
	if (list == NULL)
		return NULL;

	pkgentry = list->head;
	latest_version = "any";
	latest_pkg = NULL;

	while (pkgentry != NULL) {
		pkg = &pkgentry->pkg;
		if (  pkg_version_compare(latest_version, pkg->version) <= 0
		   && pkg_version_compare(pkg->version, max_version) <= 0) {
			latest_pkg = pkg;
			latest_version = pkg->version;
		}

		pkgentry = pkgentry->next;
	}

	return latest_pkg;
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
		new_tab = mm_realloc(binindex->pkgname_table, tab_sz);
		binindex->pkgname_table = new_tab;

		// Assign an new pkgname id
		pkgname_id = binindex->num_pkgname++;

		// Initialize the package list associated to id
		pkglist = &binindex->pkgname_table[pkgname_id];
		pkglist_init(pkglist, name);

		// Reference the new package list in the index table
		entry->ivalue = pkgname_id;
		entry->key = pkglist->pkg_name;
	}

	return pkgname_id;
}


static
void binindex_add_pkg(struct binindex* binindex, struct mmpkg* pkg)
{
	struct pkglist* pkglist;
	int pkgname_id;

	pkgname_id = binindex_get_pkgname_id(binindex, pkg->name);
	pkglist = &binindex->pkgname_table[pkgname_id];
	pkglist_add_or_modify(pkglist, pkg);
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
void binindex_compute_rdepends(struct binindex* binindex)
{
	struct pkg_iter iter;
	struct mmpkg* pkg;
	struct pkglist* pkglist;
	struct mmpkg_dep * dep;

	pkg = pkg_iter_first(&iter, binindex);
	while (pkg != NULL) {
		// For each dependency of package, add the package name in
		// the reverse dependency of dependency's package list
		dep = pkg->mpkdeps;
		while (dep) {
			pkglist = binindex_get_pkglist(binindex, dep->name);
			assert(pkglist != NULL);
			rdepends_add(&pkglist->rdeps, pkg->name);

			dep = dep->next;
		}
		pkg = pkg_iter_next(&iter);
	}
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
};


static
enum field_type get_scalar_field_type(const char* key)
{
	int i;

	for (i = 0; i < MM_NELEM(scalar_field_names); i++)
		if (strcmp(key, scalar_field_names[i]) == 0)
			return i;

	return FIELD_UNKNOWN;
}


static
int mmpkg_set_scalar_field(struct mmpkg * pkg, enum field_type type,
                           const char* value, size_t valuelen)
{
	const mmstr** field = NULL;

	switch(type) {
	case FIELD_VERSION:
		field = &pkg->version;
		break;

	case FIELD_FILENAME:
		field = &pkg->filename;
		break;

	case FIELD_SHA:
		field = &pkg->sha256;
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

	case FIELD_SIZE:
		pkg->size = atoi(value);
		return 0;

	default:
		return -1;
	}

	mmstr_free(*field);
	*field = mmstr_malloc_copy(value, valuelen);

	return 0;
}


static inline
void mmpkg_append_dependency_rec(struct mmpkg_dep * dep, struct mmpkg_dep * d)
{
	if (dep->next != NULL)
		return mmpkg_append_dependency_rec(dep->next, d);

	dep->next = d;
}

static
void mmpkg_add_dependency(struct mmpkg * pkg, struct mmpkg_dep * dep)
{
	if (pkg->mpkdeps == NULL)
		pkg->mpkdeps = dep;
	else
		mmpkg_append_dependency_rec(pkg->mpkdeps, dep);
}


/* parse a single mmpack or system dependency
 * eg:
 *   pkg-b: [0.0.2, any]
 */
static
int mmpack_parse_dependency(struct parsing_ctx* ctx,
                            struct mmpkg * pkg,
                            struct mmpkg_dep * dep)
{
	int exitvalue;
	yaml_token_t token;

	exitvalue = -1;
	while (1) {
		if (!yaml_parser_scan(&ctx->parser, &token))
			goto exit;

		switch(token.type) {
		case YAML_FLOW_SEQUENCE_END_TOKEN:
			if (dep->min_version != NULL && dep->max_version != NULL)
				exitvalue = 0;
			goto exit;

		case YAML_SCALAR_TOKEN:
			if (dep->min_version == NULL) {
				dep->min_version = mmstr_malloc_from_cstr((char const *) token.data.scalar.value);
			} else {
				if (dep->max_version != NULL) 
					goto exit;
				dep->max_version = mmstr_malloc_from_cstr((char const *) token.data.scalar.value);
			}
			break;

		default: /* ignore */
			break;
		}

		yaml_token_delete(&token);
	};

exit:
	yaml_token_delete(&token);

	if (exitvalue == 0) {
		mmpkg_add_dependency(pkg, dep);
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
                         struct mmpkg * pkg)
{
	int exitvalue, type;
	yaml_token_t token;
	struct mmpkg_dep * dep;

	exitvalue = 0;
	dep = NULL;
	type = -1;
	while (1) {
		if (!yaml_parser_scan(&ctx->parser, &token))
			goto exit;

		switch(token.type) {
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
				dep = mmpkg_dep_create((char const *)token.data.scalar.value);
				break;

			default:
				mmpkg_dep_destroy(dep);
				dep = NULL;
				type = -1;
				break;
			}
		break;

		default: /* ignore */
			break;
		}

		yaml_token_delete(&token);
	};

	/* reach end of file prematurely */
	exitvalue = -1;

exit:
	yaml_token_delete(&token);
	if (dep != NULL)  /* dep is set to NULL after being used */
		mmpkg_dep_destroy(dep);

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
                            struct mmpkg * pkg)
{
	int exitvalue;
	yaml_token_t token;
	char const * expr;

	exitvalue = 0;
	while (1) {
		if (!yaml_parser_scan(&ctx->parser, &token))
			goto exit;

		switch(token.type) {
		case YAML_FLOW_SEQUENCE_END_TOKEN:
		case YAML_BLOCK_END_TOKEN:
		case YAML_KEY_TOKEN:
			goto exit;

		case YAML_SCALAR_TOKEN:
			expr = (char const *) token.data.scalar.value;
			strlist_add(&pkg->sysdeps, expr);
			break;

		default: /* ignore */
			break;
		}

		yaml_token_delete(&token);
	};

	/* reach end of file prematurely */
	exitvalue = -1;

exit:
	yaml_token_delete(&token);
	return exitvalue;
}


/* parse a single package entry */
static
int mmpack_parse_index_package(struct parsing_ctx* ctx, struct mmpkg * pkg)
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

		switch(token.type) {
		case YAML_BLOCK_END_TOKEN:
			if (mmpkg_check_valid(pkg, ctx->repo_index < 0 ? 0 : 1))
				goto error;
			goto exit;

		case YAML_KEY_TOKEN:
			type = YAML_KEY_TOKEN;
			break;

		case YAML_VALUE_TOKEN:
			type = YAML_VALUE_TOKEN;
			break;

		case YAML_SCALAR_TOKEN:
			data = (char const *) token.data.scalar.value;
			data_len = token.data.scalar.length;

			switch (type) {
			case YAML_KEY_TOKEN:
				if (STR_EQUAL(data, data_len, "depends")) {
					if (mmpack_parse_deplist(ctx, pkg) < 0)
						goto error;
				} else if (STR_EQUAL(data, data_len, "sysdepends")) {
					if (mmpack_parse_sysdeplist(ctx, pkg) < 0)
						goto error;
				} else {
					scalar_field = get_scalar_field_type(data);
					type = -1;
				}
				break;

			case YAML_VALUE_TOKEN:
				if ((scalar_field != FIELD_UNKNOWN) && token.data.scalar.length) {
					if (mmpkg_set_scalar_field(pkg, scalar_field, data, data_len))
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
	} while(token.type != YAML_STREAM_END_TOKEN);

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
	struct mmpkg pkg;
	mmstr* name = NULL;
	const char* valuestr;
	int valuelen;

	mmpkg_init(&pkg, NULL);
	exitvalue = -1;
	type = -1;
	while (1) {
		if (!yaml_parser_scan(&ctx->parser, &token))
			goto exit;

		switch(token.type) {
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
				name = mmstr_copy_realloc(name, valuestr, valuelen);
				mmpkg_init(&pkg, name);
				pkg.repo_index = ctx->repo_index;
				exitvalue = mmpack_parse_index_package(ctx, &pkg);
				if (exitvalue != 0)
					goto exit;

				binindex_add_pkg(binindex, &pkg);
				mmpkg_deinit(&pkg);
			}
			type = -1;
			break;

		default: /* ignore */
			break;
		}

		yaml_token_delete(&token);
	};
	exitvalue = 0;

exit:
	yaml_token_delete(&token);
	mmpkg_deinit(&pkg);
	mmstr_free(name);

	return exitvalue;
}


/**
 * binindex_populate() - populate package database from package list
 * @binindex:   binary package index to populate
 * @index_filename: repository package list file
 * @repo_index: index of the repository list being read (use -1 if package
 *              list is the list of installed package)
 *
 * Return: 0 in case of success, -1 otherwise
 */
LOCAL_SYMBOL
int binindex_populate(struct binindex* binindex, char const * index_filename,
                      int repo_index)
{
	int rv = -1;
	FILE * index_fh;
	struct parsing_ctx ctx = {.repo_index = repo_index};

	if (!yaml_parser_initialize(&ctx.parser))
		return mm_raise_error(ENOMEM, "failed to init yaml parse");

	index_fh = fopen(index_filename, "r");
	if (index_fh == NULL) {
		mm_raise_error(EINVAL, "failed to open given binary index file");
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
 * Return: a pointer to the struct mmpkg installed if found, NULL if no package
 * with @name is in the install state.
 */
LOCAL_SYMBOL
const struct mmpkg* install_state_get_pkg(const struct install_state* state,
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
                           const struct mmpkg* pkg)
{
	struct it_entry* entry;

	entry = indextable_lookup_create(&state->idx, pkg->name);
	entry->value = (void*)pkg;
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
	indextable_remove(&state->idx, pkgname);
}


LOCAL_SYMBOL
void install_state_save_to_index(struct install_state* state, FILE* fp)
{
	struct it_iterator iter;
	struct it_entry* entry;
	const struct mmpkg* pkg;

	entry = it_iter_first(&iter, &state->idx);
	while (entry) {
		pkg = entry->value;
		mmpkg_save_to_index(pkg, fp);
		entry = it_iter_next(&iter);
	}
}


/**************************************************************************
 *                                                                        *
 *                      Reverse dependency iterator                       *
 *                                                                        *
 **************************************************************************/

/**
 * rdeps_iter_first() - initialize iterator of a set of reverse dependencies
 * @iter:       pointer to an iterator structure
 * @pkg:        package whose reverse dependencies are requested
 * @binindex:   binary package index
 * @state:      install state to use combined with @binindex, to test which
 *              installed package is depending on @pkg
 *
 * Return: the pointer to first package in the set of reverse dependencies
 * of @pkg if not empty, NULL otherwise.
 */
LOCAL_SYMBOL
const struct mmpkg* rdeps_iter_first(struct rdeps_iter* iter,
                                     const struct mmpkg* pkg,
                                     const struct binindex* binindex,
                                     const struct install_state* state)
{
	const struct pkglist* list;

	list = binindex_get_pkglist(binindex, pkg->name);
	assert(list != NULL);

	*iter = (struct rdeps_iter) {
		.pkg_name = pkg->name,
		.state = state,
		.rdeps_names = list->rdeps.names,
		.rdeps_index = list->rdeps.num,
	};

	return rdeps_iter_next(iter);
}


/**
 * rdeps_iter_next() - get next element in a set of reverse dependencies
 * @iter:       pointer to an initialized iterator structure
 *
 * This function return the next reverse dependency in the iterator @iter
 * initialized by rdeps_iter_first().
 *
 * NOTE: It is guaranteed than no elements in the reverse dependency set can
 * be missed even if the install_state element passed in argument to
 * rdeps_iter_first() is modified between two calls to rdeps_iter_next() or
 * after rdeps_iter_first() using same @iter pointer, PROVIDED that the
 * modification done in install state is only package removals.
 *
 * Return: the pointer to next package in the set of reverse dependencies if
 * it is not the last item, NULL otherwise.
 */
LOCAL_SYMBOL
const struct mmpkg* rdeps_iter_next(struct rdeps_iter* iter)
{
	const struct mmpkg* rdep_pkg;
	const struct mmpkg_dep* dep;
	const mmstr* rdep_name;

	while (--iter->rdeps_index >= 0) {
		rdep_name = iter->rdeps_names[iter->rdeps_index];
		rdep_pkg = install_state_get_pkg(iter->state, rdep_name);
		if (!rdep_pkg)
			continue;

		// Loop over dependencies of candidate package to see if it
		// really depends on target package name
		dep = rdep_pkg->mpkdeps;
		while (dep) {
			if (mmstrequal(dep->name, iter->pkg_name))
				return rdep_pkg;

			dep = dep->next;
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
const struct mmpkg* pkglist_iter_first(struct pkglist_iter* iter,
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
const struct mmpkg* pkglist_iter_next(struct pkglist_iter* iter)
{
	struct pkglist_entry* entry = iter->curr->next;

	if (!entry)
		return NULL;

	iter->curr = entry;
	return &entry->pkg;
}
