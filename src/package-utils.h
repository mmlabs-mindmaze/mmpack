/*
 * @mindmaze_header@
 */
#ifndef PACKAGE_UTILS_H
#define PACKAGE_UTILS_H

#include <stdio.h>

#include "indextable.h"
#include "mmstring.h"
#include "utils.h"


/**
 * enum pkg_state - list of possible package state
 * @MMPACK_PKG_ERROR:     error while retrieving package state
 * @MMPACK_PKG_UNSET:     unset package state
 * @MMPACK_PKG_INSTALLED: package has already been installed by mmpack
 * @MMPACK_PKG_STAGED:    package will be installed by mmpack
 * @SYSTEM_PKG_UNSET:     system package in unknown state
 * @SYSTEM_PKG_INSTALLED: package has already been installed by the system
 * @SYSTEM_PKG_REQUIRED:  package is missing from the system
 */
typedef enum {
	MMPACK_PKG_ERROR = -1,
	MMPACK_PKG_UNSET = 0,
	MMPACK_PKG_INSTALLED,
	MMPACK_PKG_STAGED,
	SYSTEM_PKG_UNSET,
	SYSTEM_PKG_INSTALLED,
	SYSTEM_PKG_REQUIRED,
} pkg_state;

int pkg_version_compare(char const * v1, char const * v2);
pkg_state get_local_system_install_state(char const * name, char const * version);

struct mmpkg {
	mmstr const * name;
	mmstr const * version;
	mmstr const * filename;
	mmstr const * sha256;
	mmstr const * source;
	mmstr const * desc;
	mmstr const * sumsha;
	size_t size;

	pkg_state state;

	struct mmpkg_dep * mpkdeps;
	struct mmpkg_dep * sysdeps;
	struct mmpkg * next_version;
};

struct mmpkg_dep {
	mmstr const * name;
	mmstr const * min_version; /* inclusive */
	mmstr const * max_version; /* exclusive */

	struct mmpkg_dep * next;
};

struct binindex {
	struct indextable pkg_list_table;
};

void mmpkg_dump(struct mmpkg const * pkg);
void mmpkg_save_to_index(struct mmpkg const * pkg, FILE* fp);

struct mmpkg_dep * mmpkg_dep_create(char const * name);
void mmpkg_dep_destroy(struct mmpkg_dep * dep);
void mmpkg_dep_dump(struct mmpkg_dep const * deps, char const * type);
void mmpkg_dep_save_to_index(struct mmpkg_dep const * dep, FILE* fp, int lvl);

struct mmpkg const * binindex_get_latest_pkg(struct binindex* binindex, mmstr const * name,
                                mmstr const * max_version);

void binindex_init(struct binindex* binindex);
void binindex_deinit(struct binindex* binindex);
int binindex_populate(struct binindex* binindex, char const * index_filename,
                   struct indextable * installed);
void binindex_dump(struct binindex const * binindex);

#endif /* PACKAGE_UTILS_H */
