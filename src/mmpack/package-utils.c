/*
 * @mindmaze_header@
 */
#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include <curl/curl.h>
#include <mmsysio.h>
#include <stdbool.h>
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
#include "strchunk.h"
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
 *                          parsing of binary index                       *
 *                                                                        *
 **************************************************************************/
enum field_type {
	FIELD_UNKNOWN = -1,
	FIELD_NAME = 0,
	FIELD_VERSION,
	FIELD_FILENAME,
	FIELD_SHA,
	FIELD_SIZE,
	FIELD_SOURCE,
	FIELD_DESC,
	FIELD_SUMSHA,
	FIELD_GHOST,
	FIELD_SRCSHA,
	FIELD_DEPENDS,
	FIELD_SYSDEPENDS,
};

static
const char* const field_names[] = {
	[FIELD_NAME] = "name",
	[FIELD_VERSION] = "version",
	[FIELD_FILENAME] = "filename",
	[FIELD_SHA] = "sha256",
	[FIELD_SIZE] = "size",
	[FIELD_SOURCE] = "source",
	[FIELD_DESC] = "description",
	[FIELD_SUMSHA] = "sumsha256sums",
	[FIELD_GHOST] = "ghost",
	[FIELD_SRCSHA] = "srcsha256",
	[FIELD_DEPENDS] = "depends",
	[FIELD_SYSDEPENDS] = "sysdepends",
};


static
int get_bool_value(struct strchunk val)
{
	char tmp[8] = "";

	if ((size_t)val.len > sizeof(tmp) - 1)
		goto error;

	memcpy(tmp, val.buf, val.len);
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
	mm_raise_error(EINVAL, "invalid bool value: %.*s", val.len, val.buf);
	return -1;
}


static
enum field_type get_field_type(struct strchunk key)
{
	int i;

	for (i = 0; i < MM_NELEM(field_names); i++) {
		if (strchunk_equal(key, field_names[i]))
			return i;
	}

	return FIELD_UNKNOWN;
}


static
int update_mmstr_unwrap(const mmstr** str, struct strchunk value)
{
	mmstr* mmval;
	struct strchunk line;

	mmstr_free(*str);

	line = strchunk_getline(&value);
	mmval = mmstr_malloc_copy(line.buf, line.len);

	while (value.len) {
		line = strchunk_getline(&value);
		line = strchunk_rpart(line, 0);  // skip leading space

		// Detect seqence for end of line
		if (strchunk_equal(line, ".")) {
			mmval = mmstrcat_realloc(mmval, "\n");
			continue;
		}

		mmval = mmstr_append_realloc(mmval, line.buf, line.len);
	}

	*str = mmval;
	return 0;
}


static
int update_mmstr(const mmstr** str, struct strchunk value)
{
	mmstr_free(*str);
	*str = mmstr_malloc_copy(value.buf, value.len);
	return 0;
}


static
int set_binpkg_deps(struct binpkg * pkg, struct strchunk deps)
{
	struct strchunk dep;
	int pos;

	// Empty previous dependency
	binpkg_clear_deps(pkg);

	while (deps.len) {
		pos = strchunk_find(deps, ',');
		dep = strchunk_strip(strchunk_lpart(deps, pos));
		deps = strchunk_rpart(deps, pos);

		if (binpkg_add_dep(pkg, dep))
			return -1;
	}

	return 0;
}


static
int set_binpkg_sysdeps(struct binpkg * pkg, struct strchunk sysdeps)
{
	struct strchunk sysdep;
	int pos;

	binpkg_clear_sysdeps(pkg);

	// Split supplied list over ',' characters
	while (sysdeps.len) {
		pos = strchunk_find(sysdeps, ',');
		sysdep = strchunk_strip(strchunk_lpart(sysdeps, pos));
		sysdeps = strchunk_rpart(sysdeps, pos);

		binpkg_add_sysdep(pkg, sysdep);
	}

	return 0;
}


static
int set_binpkg_field(struct binpkg * pkg,
                     enum field_type type,
                     struct strchunk value,
                     const struct repo* repo,
                     bool unwrap_desc)
{
	struct remote_resource* res;
	int bval;

	switch (type) {
	case FIELD_NAME:    return update_mmstr(&pkg->name, value);
	case FIELD_VERSION: return update_mmstr(&pkg->version, value);
	case FIELD_SOURCE:  return update_mmstr(&pkg->source, value);
	case FIELD_SUMSHA:  return update_mmstr(&pkg->sumsha, value);
	case FIELD_SRCSHA:  return update_mmstr(&pkg->srcsha, value);
	case FIELD_DESC:
		if (unwrap_desc)
			return update_mmstr_unwrap(&pkg->desc, value);
		else
			return update_mmstr(&pkg->desc, value);

	case FIELD_GHOST:
		bval = get_bool_value(value);
		if (bval == -1)
			return -1;

		binpkg_update_flags(pkg, MMPKG_FLAGS_GHOST, bval);
		return 0;

	case FIELD_DEPENDS:    return set_binpkg_deps(pkg, value);
	case FIELD_SYSDEPENDS: return set_binpkg_sysdeps(pkg, value);

	// Ignore unknown field
	case FIELD_UNKNOWN: return 0;

	default: break;
	}

	// The remaining fields are all related to remote resource
	res = binpkg_get_remote_res(pkg, repo);

	switch (type) {
	case FIELD_FILENAME: return update_mmstr(&res->filename, value);
	case FIELD_SHA:      return update_mmstr(&res->sha256, value);
	case FIELD_SIZE:     return strchunk_parse_size(&res->size, value);
	default:
		mm_crash("invalid field type");
	}
}


static
void binpkg_add_dependency(struct binpkg * pkg, struct pkgdep * dep)
{
	dep->next = pkg->mpkdeps;
	pkg->mpkdeps = dep;
}


/**************************************************************************
 *                                                                        *
 *                      YAML parsing of binary index                      *
 *                                                                        *
 **************************************************************************/
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
	struct strchunk data;

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
			data = (struct strchunk) {
				.buf = (const char*)token.data.scalar.value,
				.len = token.data.scalar.length,
			};

			switch (type) {
			case YAML_KEY_TOKEN:
				if (dep != NULL)
					goto exit;

				dep = pkgdep_create(data);
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
	struct strchunk data;
	enum field_type scalar_field;

	exitvalue = 0;
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
			data = (struct strchunk) {
				.buf = (const char*)token.data.scalar.value,
				.len = token.data.scalar.length,
			};

			switch (type) {
			case YAML_KEY_TOKEN:
				if (strchunk_equal(data, "depends")) {
					if (mmpack_parse_deplist(ctx, pkg))
						goto error;
				} else if (strchunk_equal(data,
				                          "sysdepends")) {
					if (mmpack_parse_sysdeplist(ctx, pkg))
						goto error;
				} else {
					scalar_field = get_field_type(data);
					type = -1;
				}

				break;

			case YAML_VALUE_TOKEN:
				if (set_binpkg_field(pkg, scalar_field,
				                     data, ctx->repo, false))
					goto error;

			/* fallthrough */
			default:
				scalar_field = FIELD_UNKNOWN;
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
int pkg_parse_yaml_info(const char* filename, struct binpkg * pkg)
{
	int rv = -1;
	struct buffer buffer;
	struct strchunk name, buffstr;
	struct parsing_ctx ctx = {.repo = NULL};

	buffer_init(&buffer);
	if (pkg_load_file(filename, "./MMPACK/info", &buffer))
		goto exit;

	if (!yaml_parser_initialize(&ctx.parser)) {
		mm_raise_error(ENOMEM, "failed to init yaml parse");
		goto exit;
	}

	yaml_parser_set_input_string(&ctx.parser,
	                             (const unsigned char*) buffer.base,
	                             buffer.size);
	rv = mmpack_parse_index_package(&ctx, pkg);

	yaml_parser_delete(&ctx.parser);
	if (rv != 0)
		goto exit;

	/* additionally, load the package name from the buffer */
	buffstr = strchunk_from_buffer(&buffer);
	name = strchunk_lpart(buffstr, strchunk_find(buffstr, ':'));
	name = strchunk_strip(name);
	pkg->name = mmstr_malloc_copy(name.buf, name.len);

exit:
	buffer_deinit(&buffer);
	return rv;
}


static
int yaml_load_binindex(struct binindex* binindex, const char* index_filename,
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
 *                    key/value format parsing of binary index            *
 *                                                                        *
 **************************************************************************/
static
int keyval_parse_binpkg_metadata(struct strchunk* sc, struct binpkg* pkg,
                                 const struct repo* repo)
{
	struct strchunk line, key, value, remaining = *sc;
	enum field_type field;
	int pos;

	while (1) {
		line = strchunk_getline(&remaining);
		if (strchunk_is_whitespace(line))
			break;

		// Extract key, value
		pos = strchunk_rfind(line, ':');
		key = strchunk_rstrip(strchunk_lpart(line, pos));
		value = strchunk_lstrip(strchunk_rpart(line, pos));

		// Add subsequent lines in case of multiline value
		while (remaining.len && remaining.buf[0] == ' ') {
			line = strchunk_getline(&remaining);
			value = strchunk_extent(value, line);
		}

		field = get_field_type(key);
		if (set_binpkg_field(pkg, field, value, repo, true))
			return -1;
	}

	// Skip next empty lines
	while (remaining.len) {
		pos = strchunk_find(remaining, '\n');
		line = strchunk_lpart(remaining, pos);
		if (!strchunk_is_whitespace(line))
			break;

		remaining = strchunk_rpart(remaining, pos);
	}

	*sc = remaining;
	return 0;
}


static
int keyval_load_binindex(struct binindex* binindex, const char* filename,
                         const struct repo* repo)
{
	struct buffer file_content;
	struct strchunk remaining;
	struct binpkg pkg;
	int rv = 0;

	buffer_init(&file_content);

	rv = load_compressed_file(filename, &file_content);

	remaining = strchunk_from_buffer(&file_content);

	while (remaining.len && rv == 0) {
		binpkg_init(&pkg, NULL);

		rv = keyval_parse_binpkg_metadata(&remaining, &pkg, repo);
		if (rv == 0)
			binindex_add_pkg(binindex, &pkg);

		// Because binpkg do not own name field (only a reference), it
		// must be free here
		mmstr_free(pkg.name);

		binpkg_deinit(&pkg);
	}

	buffer_deinit(&file_content);

	return rv;
}


/**************************************************************************
 *                                                                        *
 *                                 general parsing                        *
 *                                                                        *
 **************************************************************************/

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
	int fd;
	unsigned char magic[2] = {0};

	fd = mm_open(index_filename, O_RDONLY, 0);
	if (fd < 0)
		return -1;

	mm_read(fd, magic, sizeof(magic));
	mm_close(fd);

	// Test file is gzip
	if (magic[0] == 0x1f && magic[1] == 0x8b)
		return keyval_load_binindex(binindex, index_filename, repo);

	return yaml_load_binindex(binindex, index_filename, repo);
}


static
int pkg_parse_pkginfo(char const * filename, struct binpkg* pkg)
{
	struct buffer buffer;
	struct strchunk pkginfo;
	int rv = -1;

	buffer_init(&buffer);

	if (pkg_load_pkginfo(filename, &buffer))
		goto exit;

	pkginfo = strchunk_from_buffer(&buffer);
	rv = keyval_parse_binpkg_metadata(&pkginfo, pkg, NULL);

exit:
	buffer_deinit(&buffer);
	return rv;
}


/**
 * binindex_add_pkgfile() - add local mmpack package file to binindex
 * @binindex: initialized mmpack binindex
 * @filename: path to the mmpack archive
 *
 * Return: a pointer to the binpkg structure that has been inserted.
 * It belongs the the binindex and will be cleansed during mmpack global
 * cleanup.
 */
LOCAL_SYMBOL
struct binpkg* binindex_add_pkgfile(struct binindex* binindex,
                                    char const * filename)
{
	struct binpkg* pkg = NULL;
	struct binpkg tmppkg;
	struct remote_resource* res;
	mmstr* hash;

	binpkg_init(&tmppkg, NULL);
	res = binpkg_get_remote_res(&tmppkg, NULL);
	res->filename = mmstr_malloc_from_cstr(filename);
	res->sha256 = hash = mmstr_malloc(SHA_HEXSTR_LEN);

	if (sha_compute(hash, filename, NULL, 1))
		goto exit;

	if (pkg_parse_pkginfo(filename, &tmppkg)
	    && pkg_parse_yaml_info(filename, &tmppkg))
		goto exit;

	pkg = binindex_add_pkg(binindex, &tmppkg);

exit:
	mmstr_free(tmppkg.name);
	binpkg_deinit(&tmppkg);
	return pkg;
}
