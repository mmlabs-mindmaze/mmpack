/*
 * @mindmaze_header@
 */
#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include <mmsysio.h>
#include <mmerrno.h>
#include <stdio.h>
#include <mmlib.h>

#include "buffer.h"
#include "mmstring.h"
#include "strset.h"
#include "sysdeps.h"
#include "utils.h"


/**************************************************************************
 *                                                                        *
 *                       dpkg-based sysdeps checking                      *
 *                                                                        *
 **************************************************************************/
#define CHECK_DPKG_INSTALLED PKGDATADIR "/check-dpkg-installed"

/**
 * dpkg_concat_sysdeps() - create a combined string of all sysdeps
 * @sysdeps:    string set of dpkg dependencies
 *
 * Return: the concatenated string. Free it with mmstr_free(). If @sysdeps
 * is empty, NULL is returned.
 */
static
mmstr* dpkg_concat_sysdeps(const struct strset* sysdeps)
{
	struct strset_iterator iter;
	mmstr* fulldeps;
	mmstr* dep;
	int len;

	dep = strset_iter_first(&iter, sysdeps);
	if (!dep)
		return NULL;

	fulldeps = mmstrdup(dep);

	// Concatenate all subsequent sysdeps preceded by comma
	for (; dep != NULL; dep = strset_iter_next(&iter)) {
		// Resize fulldeps string to accommodate the new elt
		len = mmstrlen(dep) + 2;
		fulldeps = mmstr_realloc(fulldeps, mmstrlen(fulldeps) + len);

		// Append new element
		mmstrcat_cstr(fulldeps, ", ");
		mmstrcat(fulldeps, dep);
	}

	return fulldeps;
}


static
int dpkg_check_sysdeps_installed(const struct strset* sysdeps)
{
	mmstr* strdeps;

	/* variables to handle the tests */
	const mmstr * env = mmstr_alloca_from_cstr(
		mm_getenv("_MMPACK_TEST_PREFIX", ""));
	STATIC_CONST_MMSTR(dpk, CHECK_DPKG_INSTALLED);
	mmstr * path;
	int rv;

	strdeps = dpkg_concat_sysdeps(sysdeps);

	// If strdeps is NULL, sysdeps is empty
	if (!strdeps)
		return 0;

	// Execute check-dpkg-installed script with sysdeps in arg and
	// check return value indeed 0 (success)

	path = mmstr_malloca(mmstrlen(env) + mmstrlen(dpk));
	mmstrcpy(path, env);
	mmstrcat(path, dpk);
	char* argv[] = {path, strdeps, NULL};
	rv = execute_cmd(argv);

	if (rv < 0)
		printf("%s\n", mm_get_lasterror_desc());
	else if (rv > 0)
		rv = DEPS_MISSING;

	mmstr_freea(path);
	mmstr_free(strdeps);
	return rv;
}


/**************************************************************************
 *                                                                        *
 *                      pacman-based sysdeps checking                     *
 *                                                                        *
 **************************************************************************/

#define DEFAULT_MSYS2 "C:\\msys64"

static
const mmstr* get_msys2_root(void)
{
	static char msys2_root_data[128];
	mmstr* msys2_root = mmstr_map_on_array(msys2_root_data);
	char* argv[] = {"cygpath.exe", "-w", "/", NULL};
	struct buffer cmd_output;
	int len;

	if (msys2_root[0])
		return msys2_root;

	buffer_init(&cmd_output);
	if (execute_cmd_capture_output(argv, &cmd_output)) {
		mm_log_warn("Could not execute cygpath. Assuming MSYS2 root"
		            " is "DEFAULT_MSYS2);
		mmstrcpy_cstr(msys2_root, DEFAULT_MSYS2);
		goto exit;
	}

	mmstr_copy(msys2_root, cmd_output.base, cmd_output.size);

	// Remove possible end of line
	len = mmstrlen(msys2_root);
	if (msys2_root[len-1] == '\n')
		mmstr_setlen(msys2_root, len-1);

exit:
	buffer_deinit(&cmd_output);
	return msys2_root;
}


/**
 * read_pkgname() - read pacman package description and get package name
 * @descpath:   path to pacman package description file
 * @instpkgs:   set of installed package to update
 *
 * Return: 0 in case of success, -1 otherwise with error state set
 */
static
int read_pkgname(const mmstr* descpath, struct strset* instpkgs)
{
	FILE* fp;
	char linedata[128];
	mmstr* line = mmstr_map_on_array(linedata);
	int len, rv;

	fp = fopen(descpath, "rb");
	if (!fp)
		return mm_raise_from_errno("Can't open %s", descpath);

	while (fgets(line, mmstr_maxlen(line)+1, fp)) {
		// skip until we reach the package name field
		if (!STR_STARTS_WITH(line, strlen(line), "%NAME%"))
			continue;

		// Read name field line
		if (!fgets(line, mmstr_maxlen(line)+1, fp))
			break;

		// Get line size minus possible end of line;
		len = mmstr_update_len_from_buffer(line);
		if (line[len-1] == '\n')
			mmstr_setlen(line, --len);

		// Now we have in line the package name, add it to install
		// package set
		strset_add(instpkgs, line);
		break;
	}

	// Check we did not stop reading because error
	rv = 0;
	if (ferror(fp))
		rv = mm_raise_from_errno("Can't read %s", descpath);

	fclose(fp);
	return rv;
}


/**
 * pacman_populate_instpkgs() - Read pacman installed packages
 * @instpkgs:   set of installed package to fill
 * @path:       path to pacman installed package metadata folder
 *
 * Return: 0 in case of success, -1 otherwise with error state set
 */
static
int pacman_populate_instpkgs(struct strset* instpkgs, const mmstr* path)
{
	MM_DIR* dir;
	const struct mm_dirent* dirent;
	mmstr* descpath = NULL;
	mmstr* name = NULL;
	int len, rv;

	dir = mm_opendir(path);
	if (!dir)
		return -1;

	// Read all directory entries and try to read and parse enclosed
	// desc file
	rv = 0;
	while (rv == 0) {
		dirent = mm_readdir(dir, NULL);
		if (!dirent)
			break;

		if (dirent->type != MM_DT_DIR)
			continue;

		name = mmstrcpy_cstr_realloc(name, dirent->name);
		if (STR_EQUAL(name, mmstrlen(name), ".")
		    || STR_EQUAL(name, mmstrlen(name), "..") )
			continue;

		// Form desc file path: <path>/<pkg-ver>/desc
		len = mmstrlen(path) + 1 +  mmstrlen(name) + 5;
		descpath = mmstr_realloc(descpath, len);
		mmstr_join_path(descpath, path, name);
		mmstrcat_cstr(descpath, "/desc");

		// Try read package name and add to install package set
		rv = read_pkgname(descpath, instpkgs);
	}

	mmstr_free(descpath);
	mmstr_free(name);

	return rv;
}


static
int pacman_check_sysdeps_installed(const struct strset* sysdeps)
{
	STATIC_CONST_MMSTR(pacmandb_relpath, "var/lib/pacman/local");
	mmstr* pacmandb_path;
	struct strset instpkgs;
	struct strset_iterator iter;
	mmstr* dep;
	int rv = -1;

	strset_init(&instpkgs, STRSET_HANDLE_STRINGS_MEM);

	// Read pacman installed package data
	pacmandb_path = mmstr_alloca(256);
	mmstr_join_path(pacmandb_path, get_msys2_root(), pacmandb_relpath);
	if (pacman_populate_instpkgs(&instpkgs, pacmandb_path))
		goto exit;

	// Loop over each system dependency and check whether it belongs to
	// instpkgs
	rv = DEPS_OK;
	dep = strset_iter_first(&iter, sysdeps);
	for (; dep != NULL; dep = strset_iter_next(&iter)) {
		if (strset_contains(&instpkgs, dep))
			continue;

		// Write initial message when the first missing dep occurs
		if (rv == DEPS_OK) {
			rv = DEPS_MISSING;
			printf("missing system dependencies:");
		}

		printf(" %s", dep);
	}

exit:
	strset_deinit(&instpkgs);
	return rv;
}


/**************************************************************************
 *                                                                        *
 *                          implementation switch                         *
 *                                                                        *
 **************************************************************************/

/**
 * check_sysdeps_installed() - test a set of sysdeps is installed
 * @sysdeps:    set of system dependencies
 *
 * Return: DEPS_OK (0) if all dependencies in @sysdeps are installed on the
 * system, DEPS_MISSING (1) if one or more are missing. -1 in case of
 * failure with error state set accordingly
 */
LOCAL_SYMBOL
int check_sysdeps_installed(const struct strset* sysdeps)
{
	int rv;
	os_id id = get_os_id();

	switch (id) {
	case OS_ID_DEBIAN:
		rv = dpkg_check_sysdeps_installed(sysdeps);
		break;

	case OS_ID_WINDOWS_10:
		rv = pacman_check_sysdeps_installed(sysdeps);
		break;

	default:
		rv = mm_raise_error(ENOSYS, "Backend not supported (%i)", id);
		break;
	}

	return rv;
}
