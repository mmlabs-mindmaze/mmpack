/*
 * @mindmaze_header@
 */
#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include <mmerrno.h>
#include <mmlib.h>
#include <mmsysio.h>
#include <yaml.h>

#include "manually_installed.h"


/**
 * remove_from_manually_installed() - check if the name given is in the set
 *                                    and suppress it from the set
 * @manually_inst: name set of manually installed packages
 * @name: name of the package to remove from the manually installed set
 *
 * Return: 0 in case of success, -1 otherwise
 */
LOCAL_SYMBOL
void remove_from_manually_installed(struct strset * manually_inst,
                                    const mmstr * name)
{
	strset_remove(manually_inst, name);
}


static
int manually_installed_populate(struct strset * manually_inst,
                                const char * filename)
{
	FILE * file;
	char pkg_name[1000];
	mmstr * cpy;

	file = fopen(filename, "rb");
	if (file == NULL) {
		mm_raise_error(EINVAL,
		               "failed to open manually installed file");
		return -1;
	}

	while (fscanf(file, "%s\n", pkg_name) != -1) {
		cpy = mmstr_malloca_from_cstr((const char*)pkg_name);
		strset_add(manually_inst, cpy);
		mmstr_freea(cpy);
	}

	fclose(file);
	return 0;
}


/**
 * load_manually_installed() - loads the name list of the manually installed
 *                             packages
 * @prefix: prefix
 * @manually_inst: name set of manually installed packages
 *
 * The file from which the names are read is
 * prefix/var/lib/mmpack/manually_installed.yaml
 *
 * Return: 0 in case of success, -1 otherwise
 */
LOCAL_SYMBOL
int load_manually_installed(const mmstr * prefix, struct strset * manually_inst)
{
	STATIC_CONST_MMSTR(manually_inst_relpath, MANUALLY_INST_RELPATH);
	mmstr * filename;
	int rv, len;

	len = mmstrlen(prefix) + mmstrlen(manually_inst_relpath) + 1;
	filename = mmstr_malloca(len);

	mmstr_join_path(filename, prefix, manually_inst_relpath);

	rv = manually_installed_populate(manually_inst, filename);

	mmstr_freea(filename);
	return rv;
}


/**
 * save_manually_installed() - dump the name list of the manually installed
 *                             packages
 * @prefix: prefix
 * @manually_inst: name set of manually installed packages
 *
 * The file where the names are dumped is
 * prefix/var/lib/mmpack/manually_installed.yaml
 *
 * Return: 0 in case of success, -1 otherwise
 */
LOCAL_SYMBOL
int save_manually_installed(const mmstr * prefix, struct strset * manually_inst)
{
	struct strset_iterator iter;
	mmstr * curr;
	mmstr * filename;
	FILE * file;
	int len;

	STATIC_CONST_MMSTR(manually_inst_relpath, MANUALLY_INST_RELPATH);

	len = mmstrlen(prefix) + mmstrlen(manually_inst_relpath) + 1;
	filename = mmstr_malloca(len);

	mmstr_join_path(filename, prefix, manually_inst_relpath);

	if (!(file = fopen(filename, "wb")))
		return -1;

	for (curr = strset_iter_first(&iter, manually_inst); curr;
	     curr = strset_iter_next(&iter)) {
		fprintf(file, "%s\n", curr);
	}

	fclose(file);
	mmstr_freea(filename);
	return 0;
}
