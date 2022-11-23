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
#include "crypto.h"
#include "indextable.h"
#include "package-utils.h"
#include "pkg-fs-utils.h"
#include "repo.h"
#include "strchunk.h"
#include "strlist.h"
#include "tar.h"
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

		// Detect sequence for end of line
		if (strchunk_equal(line, ".")) {
			line.buf = "\n";
			line.len = 1;
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
	ssize_t rsz;
	unsigned char magic[2] = {0};

	fd = mm_open(index_filename, O_RDONLY, 0);
	if (fd < 0)
		return -1;

	rsz = mm_read(fd, magic, sizeof(magic));
	mm_close(fd);

	if (rsz == 0)
		return 0;

	// Test file is gzip
	if (magic[0] == 0x1f && magic[1] == 0x8b)
		return keyval_load_binindex(binindex, index_filename, repo);

	return mm_raise_error(MM_EBADFMT, "%s is invalid file format",
	                      index_filename);
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

	if (sha_compute(hash, filename, 1)
	    || pkg_parse_pkginfo(filename, &tmppkg))
		goto exit;

	pkg = binindex_add_pkg(binindex, &tmppkg);

exit:
	mmstr_free(tmppkg.name);
	binpkg_deinit(&tmppkg);
	return pkg;
}
