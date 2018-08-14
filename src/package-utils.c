/*
 * @mindmaze_header@
 */
#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include <assert.h>
#include <curl/curl.h>
#include <stdlib.h>
#include <string.h>
#include <yaml.h>

#include <mmerrno.h>
#include <mmlib.h>

#include "mmpack-common.h"
#include "package-utils.h"


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


LOCAL_SYMBOL
struct mmpkg * mmpkg_create(char const * name)
{
	struct mmpkg * pkg = malloc(sizeof(*pkg));
	if (pkg == NULL)
		return NULL;
	memset(pkg, 0, sizeof(*pkg));

	pkg->name = mmstr_malloc_from_cstr(name);
	if (pkg->name == NULL)
		goto enomem;

	return pkg;

enomem:
	mmpkg_destroy(pkg);
	return NULL;
}


LOCAL_SYMBOL
void mmpkg_destroy(struct mmpkg * pkg)
{
	if (pkg == NULL)
		return;

	mmstr_free(pkg->name);
	mmstr_free(pkg->version);

	mmpkg_dep_destroy(pkg->dependencies);
	/* Ignore next_version pointer. Is reachable for cleanup via indextable */

	free(pkg);
}


LOCAL_SYMBOL
void mmpkg_dump(struct mmpkg const * pkg)
{
	printf("# %s (%s)\n", pkg->name, pkg->version);
	printf("\tdependencies:\n");
	mmpkg_dep_dump(pkg->dependencies);
	printf("\n");
}


LOCAL_SYMBOL
struct mmpkg_dep * mmpkg_dep_create(char const * name)
{
	struct mmpkg_dep * dep = malloc(sizeof(*dep));
	if (dep == NULL)
		return NULL;
	memset(dep, 0, sizeof(*dep));

	dep->name = mmstr_malloc_from_cstr(name);
	if (dep->name == NULL)
		goto enomem;

	return dep;

enomem:
	mmpkg_dep_destroy(dep);
	return NULL;
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
void mmpkg_dep_dump(struct mmpkg_dep const * deps)
{
	struct mmpkg_dep const * d = deps;

	while (d != NULL) {
		printf("\t\t [%s] %s [%s -> %s]\n",
		       d->is_system_package ? "SYS":"MMP", d->name,
		       d->min_version, d->max_version);
		d = d->next;
	}
}


/**
 * mmpkg_get_latest() - get the latest possible version of given package
 *                      inferior to geven maximum
 * ctx:         mmapck context
 * name:        package name
 * max_version: exclusive maximum boundary
 *
 * Return: NULL on error, a pointer to the found package otherwise
 */
LOCAL_SYMBOL
struct mmpkg const * mmpkg_get_latest(struct mmpack_ctx * ctx, mmstr const * name,
                                      mmstr const * max_version)
{
	struct it_entry * entry;
	struct mmpkg * pkg, * latest_pkg;

	assert(mmpack_ctx_is_init(ctx));

	entry = indextable_lookup(&ctx->binindex, name);
	if (entry == NULL || entry->value == NULL)
		return NULL;

	latest_pkg = entry->value;
	pkg = latest_pkg->next_version;

	while (pkg != NULL) {
		if (pkg_version_compare(latest_pkg->version, pkg->version) < 0
		    && pkg_version_compare(pkg->version, max_version) < 0)
			latest_pkg = pkg;

		pkg = pkg->next_version;
	}

	return latest_pkg;
}
