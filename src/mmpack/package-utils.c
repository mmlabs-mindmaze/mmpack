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

struct parsing_ctx {
	yaml_parser_t parser;
	const struct repo* repo;
};

/* standard isdigit() is locale dependent making it unnecessarily slow.
 * This macro is here to keep the semantic of isdigit() as usually known by
 * most programmer while issuing the fast implementation of it. */
#define isdigit(c)      ((c) >= '0' && (c) <= '9')


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


LOCAL_SYMBOL
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
