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

struct mmpkg {
	mmstr const * name;
	mmstr const * version;
	mmstr const * filename;
	mmstr const * sha256;
	mmstr const * source;
	mmstr const * desc;
	mmstr const * sumsha;
	size_t size;
	int repo_index;

	pkg_state state;

	struct mmpkg_dep * mpkdeps;
	struct strlist sysdeps;
};

struct mmpkg_dep {
	mmstr const * name;
	mmstr const * min_version; /* inclusive */
	mmstr const * max_version; /* exclusive */

	struct mmpkg_dep * next;
};


/**
 * struct binindex - structure holding all known binary package
 * @pkgname_idx:        index table mapping package name to package name ID.
 * @pkgname_table:      table of list of package sharing the same name. This
 *                      table is indexed by the package name ID.
 * @num_pkgname:        number of different package in struct binindex. This
 *                      corresponds to the length of @pkgname_table.
 */
struct binindex {
	struct indextable pkgname_idx;
	struct pkglist* pkgname_table;
	int num_pkgname;
};
int binindex_foreach(struct binindex * binindex,
                     int (*cb)(struct mmpkg*, void *),
                     void * data);

struct install_state {
	struct indextable idx;
};


struct pkglist_iter {
	struct pkglist_entry* curr;
};

struct rdeps_iter {
	const struct install_state* state;
	const mmstr* pkg_name;
	const mmstr** rdeps_names;
	int rdeps_index;
};


void mmpkg_dump(struct mmpkg const * pkg);
void mmpkg_save_to_index(struct mmpkg const * pkg, FILE* fp);
void mmpkg_sysdeps_dump(const struct strlist* sysdeps, char const * type);

struct mmpkg_dep * mmpkg_dep_create(char const * name);
void mmpkg_dep_destroy(struct mmpkg_dep * dep);
void mmpkg_dep_dump(struct mmpkg_dep const * deps, char const * type);
void mmpkg_dep_save_to_index(struct mmpkg_dep const * dep, FILE* fp, int lvl);

struct mmpkg const * binindex_get_latest_pkg(struct binindex* binindex, mmstr const * name,
                                mmstr const * max_version);

void binindex_init(struct binindex* binindex);
void binindex_deinit(struct binindex* binindex);
int binindex_populate(struct binindex* binindex, char const * index_filename,
                      int repo_index);
void binindex_dump(struct binindex const * binindex);
void binindex_compute_rdepends(struct binindex* binindex);
int binindex_get_pkgname_id(struct binindex* binindex, const mmstr* name);

int install_state_init(struct install_state* state);
int install_state_copy(struct install_state* restrict dst,
                       const struct install_state* restrict src);
void install_state_deinit(struct install_state* state);
const struct mmpkg* install_state_get_pkg(const struct install_state* state,
                                          const mmstr* name);
void install_state_add_pkg(struct install_state* state,
                           const struct mmpkg* pkg);
void install_state_rm_pkgname(struct install_state* state,
                              const mmstr* pkgname);
void install_state_save_to_index(struct install_state* state, FILE* fp);

const struct mmpkg* rdeps_iter_first(struct rdeps_iter* iter,
                                     const struct mmpkg* pkg,
                                     const struct binindex* binindex,
                                     const struct install_state* state);
const struct mmpkg* rdeps_iter_next(struct rdeps_iter* iter);

const struct mmpkg* pkglist_iter_first(struct pkglist_iter* iter,
                                       const mmstr* pkgname,
                                       const struct binindex* binindex);
const struct mmpkg* pkglist_iter_next(struct pkglist_iter* iter);

#endif /* PACKAGE_UTILS_H */
