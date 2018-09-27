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

struct pkglist_entry {
	struct mmpkg pkg;
	struct pkglist_entry* next;
};

struct pkglist {
	const mmstr* pkg_name;
	struct pkglist_entry head;
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


static inline
int get_install_state_debian(char const * name, char const * version)
{
	char cmd[512];
	char * sys_version = NULL;
	int installed = SYSTEM_PKG_REQUIRED;
	ssize_t nread;
	size_t len = 0;
	char * line = NULL;
	FILE * stream;

	sprintf(cmd, "dpkg --status %s 2>&1", name);
	stream = popen(cmd, "r");
	while ((nread = getline(&line, &len, stream)) != -1) {
		if (STR_STARTS_WITH(line, len, "Status:")) {
			if (strstr(line, "installed") != NULL)
				installed = SYSTEM_PKG_INSTALLED;
		} else if (STR_STARTS_WITH(line, len, "Version:")) {
			sys_version = strdup(line + sizeof("Version:"));
			sys_version[strlen(version) - 1] = '\0';
			if (pkg_version_compare(version, sys_version)) {
				free(sys_version);
				installed = SYSTEM_PKG_REQUIRED;
				goto exit;
			}
			free(sys_version);
		}
	}

exit:
	fclose(stream);
	free(line);
	return installed;
}


LOCAL_SYMBOL
int get_local_system_install_state(char const * name, char const * version)
{
	switch (get_os_id()) {
	case OS_ID_DEBIAN:
		return get_install_state_debian(name, version);

	case OS_ID_WINDOWS_10: /* TODO */
	default:
		return mm_raise_error(ENOSYS, "Unsupported OS");
	}
}


static
void mmpkg_init(struct mmpkg* pkg, const mmstr* name)
{
	*pkg = (struct mmpkg) {.name = name};
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
	mmpkg_dep_destroy(pkg->sysdeps);

	mmpkg_init(pkg, NULL);
}


LOCAL_SYMBOL
void mmpkg_dump(struct mmpkg const * pkg)
{
	printf("# %s (%s)\n", pkg->name, pkg->version);
	printf("\tdependencies:\n");
	mmpkg_dep_dump(pkg->mpkdeps, "MMP");
	mmpkg_dep_dump(pkg->sysdeps, "SYS");
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
	fprintf(fp, "%s:\n"
	            "    version: %s\n"
	            "    source: %s\n"
	            "    sumsha256sums: %s\n",
		    pkg->name, pkg->version, pkg->source, pkg->sumsha);

	fprintf(fp, "    depends:");
	mmpkg_dep_save_to_index(pkg->mpkdeps, fp, 2/*indentation level*/);

	fprintf(fp, "    sysdepends:");
	mmpkg_dep_save_to_index(pkg->sysdeps, fp, 2/*indentation level*/);
}


LOCAL_SYMBOL
struct mmpkg_dep * mmpkg_dep_create(char const * name)
{
	struct mmpkg_dep * dep = malloc(sizeof(*dep));
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
 *                          Package list                                  *
 *                                                                        *
 **************************************************************************/

/**
 * pkglist_create() - create a new package list
 * @name:       package name to which the package list will be associated
 *
 * Return: pointer to package list
 */
static
struct pkglist* pkglist_create(const mmstr* name)
{
	struct pkglist* list;

	list = mm_malloc(sizeof(*list));
	*list = (struct pkglist){.pkg_name = mmstrdup(name)};
	mmpkg_init(&list->head.pkg, list->pkg_name);

	return list;
}


/**
 * pkglist_destroy() - destroy package list and underlying resources
 * @list:       package list to destroy
 *
 * This function free the package list as well as the package data
 */
static
void pkglist_destroy(struct pkglist* list)
{
	struct pkglist_entry *entry, *next;

	if (!list)
		return;

	next = list->head.next;
	while (next) {
		entry = next;
		next = entry->next;

		mmpkg_deinit(&entry->pkg);
		free(entry);
	}

	mmpkg_deinit(&list->head.pkg);
	mmstr_free(list->pkg_name);
	free(list);
}


/**
 * pkglist_add_pkg() - allocate a new package to a package list
 * @list:       package list to which a new package is required
 *
 * This function create a new package and add it to @list. Its name field
 * will be initialized to the package name whose @list is associated.
 *
 * Return: a pointer to new package in list
 */
static
struct mmpkg* pkglist_add_pkg(struct pkglist* list)
{
	struct pkglist_entry* entry;

	entry = mm_malloc(sizeof(*entry));
	mmpkg_init(&entry->pkg, list->pkg_name);

	// Add new entry to the list
	entry->next = list->head.next;
	list->head.next = entry;

	return &entry->pkg;
}

/**************************************************************************
 *                                                                        *
 *                      Binary package index                              *
 *                                                                        *
 **************************************************************************/
LOCAL_SYMBOL
void binindex_init(struct binindex* binindex)
{
	indextable_init(&binindex->pkg_list_table, -1, -1);
}


LOCAL_SYMBOL
void binindex_deinit(struct binindex* binindex)
{
	struct it_iterator iter;
	struct it_entry * entry;
	struct pkglist* pkglist;

	entry = it_iter_first(&iter, &binindex->pkg_list_table);
	while (entry != NULL) {
		pkglist = entry->value;
		pkglist_destroy(pkglist);
		entry = it_iter_next(&iter);
	}

	indextable_deinit(&binindex->pkg_list_table);
}


LOCAL_SYMBOL
void binindex_dump(struct binindex const * binindex)
{

	struct it_iterator iter;
	struct it_entry * entry;

	entry = it_iter_first(&iter, &binindex->pkg_list_table);
	while (entry != NULL) {
		mmpkg_dump(entry->value);
		entry = it_iter_next(&iter);
	}
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
	struct it_entry * entry;
	struct mmpkg * pkg, * latest_pkg;
	struct pkglist_entry* pkgentry;
	struct pkglist* list;
	const char* latest_version = "any";

	entry = indextable_lookup(&binindex->pkg_list_table, name);
	if (entry == NULL || entry->value == NULL)
		return NULL;

	list = entry->value;
	pkgentry = &list->head;
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


static
struct mmpkg* binindex_add_pkg(struct binindex* binindex, const char* name)
{
	struct pkglist* pkglist;
	struct it_entry* entry;
	struct indextable* idx = &binindex->pkg_list_table;
	mmstr* pkg_name = mmstr_alloca_from_cstr(name);

	entry = indextable_lookup_create(idx, pkg_name);
	pkglist = entry->value;

	// Create package list if not existing yet
	if (!pkglist) {
		pkglist = pkglist_create(pkg_name);
		entry->key = pkglist->pkg_name;
		entry->value = pkglist;
		return &pkglist->head.pkg;
	}

	return pkglist_add_pkg(pkglist);
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
void mmpkg_insert_dependency(struct mmpkg * pkg, struct mmpkg_dep * dep,
                             int is_system_package)
{
	struct mmpkg_dep** deps;

	deps = is_system_package ? &pkg->sysdeps : &pkg->mpkdeps;
	if (*deps == NULL)
		*deps = dep;
	else
		mmpkg_append_dependency_rec(*deps, dep);
}

/* parse a single mmpack or system dependency
 * eg:
 *   pkg-b: [0.0.2, any]
 */
static
int mmpack_parse_dependency(yaml_parser_t* parser,
                            struct mmpkg * pkg,
                            struct mmpkg_dep * dep)
{
	int exitvalue;
	yaml_token_t token;

	exitvalue = -1;
	while (1) {
		if (!yaml_parser_scan(parser, &token))
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
		mmpkg_insert_dependency(pkg, dep, 0);
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
int mmpack_parse_deplist(yaml_parser_t* parser,
                         struct mmpkg * pkg)
{
	int exitvalue, type;
	yaml_token_t token;
	struct mmpkg_dep * dep;

	exitvalue = 0;
	dep = NULL;
	type = -1;
	while (1) {
		if (!yaml_parser_scan(parser, &token))
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
				exitvalue = mmpack_parse_dependency(parser, pkg, dep);
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
	if (exitvalue != 0)
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
int mmpack_parse_sysdeplist(yaml_parser_t* parser,
                            struct mmpkg * pkg)
{
	int exitvalue;
	yaml_token_t token;
	struct mmpkg_dep * dep;
	char const * name;

	exitvalue = 0;
	dep = NULL;
	while (1) {
		if (!yaml_parser_scan(parser, &token))
			goto exit;

		switch(token.type) {
		case YAML_BLOCK_END_TOKEN:
		case YAML_KEY_TOKEN:
			goto exit;

		case YAML_SCALAR_TOKEN:
			name = (char const *) token.data.scalar.value;
			dep = mmpkg_dep_create(name);
			mmpkg_insert_dependency(pkg, dep, 1);
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
	if (exitvalue != 0)
		mmpkg_dep_destroy(dep);

	return exitvalue;
}


/* parse a single package entry */
static
int mmpack_parse_index_package(yaml_parser_t* parser, struct mmpkg * pkg)
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
		if (!yaml_parser_scan(parser, &token))
			goto error;

		switch(token.type) {
		case YAML_BLOCK_END_TOKEN:
			if (mmpkg_check_valid(pkg, 0))
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
					if (mmpack_parse_deplist(parser, pkg) < 0)
						goto error;
				} else if (STR_EQUAL(data, data_len, "sysdepends")) {
					if (mmpack_parse_sysdeplist(parser, pkg) < 0)
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
int mmpack_parse_index(yaml_parser_t* parser, struct binindex * binindex,
                       struct install_state* state)
{
	int exitvalue, type;
	yaml_token_t token;
	struct mmpkg * pkg;

	exitvalue = -1;
	type = -1;
	while (1) {
		if (!yaml_parser_scan(parser, &token))
			goto exit;

		switch(token.type) {
		case YAML_STREAM_END_TOKEN:
			exitvalue = 0;
			goto exit;

		case YAML_KEY_TOKEN:
			type = YAML_KEY_TOKEN;
			break;

		case YAML_SCALAR_TOKEN:
			if (type == YAML_KEY_TOKEN) {
				pkg = binindex_add_pkg(binindex, (char const *) token.data.scalar.value);
				exitvalue = mmpack_parse_index_package(parser, pkg);
				if (exitvalue != 0)
					goto exit;

				/* Add to installed list if it is provided in argument for update */
				if (state) {
					install_state_add_pkg(state, pkg);
				} else if (mmpkg_check_valid(pkg, 1)) {
					exitvalue = -1;
					goto exit;
				}
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

	return exitvalue;
}


LOCAL_SYMBOL
int binindex_populate(struct binindex* binindex, char const * index_filename,
                      struct install_state* state)
{
	int rv = -1;
	FILE * index_fh;
	yaml_parser_t parser;

	if (!yaml_parser_initialize(&parser))
		return mm_raise_error(ENOMEM, "failed to init yaml parse");

	index_fh = fopen(index_filename, "r");
	if (index_fh == NULL) {
		mm_raise_error(EINVAL, "failed to open given binary index file");
		goto exit;
	}

	yaml_parser_set_input_file(&parser, index_fh);
	rv = mmpack_parse_index(&parser, binindex, state);

	fclose(index_fh);

exit:
	yaml_parser_delete(&parser);
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
