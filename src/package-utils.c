/*
 * @mindmaze_header@
 */
#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include <curl/curl.h>
#include <stdlib.h>
#include <string.h>
#include <yaml.h>

#include <mmerrno.h>
#include <mmlib.h>

#include "mmpack-common.h"
#include "package-utils.h"


LOCAL_SYMBOL
int pkg_version_compare(char const * v1, char const * v2)
{
	return strcmp(v1, v2);
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
