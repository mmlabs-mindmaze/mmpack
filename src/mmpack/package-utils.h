/*
 * @mindmaze_header@
 */
#ifndef PACKAGE_UTILS_H
#define PACKAGE_UTILS_H

#include <stdio.h>

#include "indextable.h"
#include "mmstring.h"
#include "settings.h"
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


/**
 * strcut constraints - structure containing all the possible constraints
 *                      imposed by the user in the command line.
 * @version: package version
 * @repo_name: the repository in which the package should be searched
 * @sumsha: package sumsha
 **/
struct constraints {
	mmstr * version;
	const struct repolist_elt * repo;
	mmstr * sumsha;
};

void constraints_deinit(struct constraints * c);
int constraints_is_empty(struct constraints * c);


struct from_repo {
	mmstr const * filename;
	mmstr const * sha256;
	size_t size;
	struct repolist_elt * repo;
	struct from_repo * next;
};


#define MMPKG_FLAGS_GHOST       (1 << 0)

struct mmpkg {
	int name_id;
	mmstr const * name;
	mmstr const * version;
	mmstr const * source;
	mmstr const * desc;
	mmstr const * sumsha;

	struct from_repo * from_repo;

	pkg_state state;
	int flags;

	struct mmpkg_dep * mpkdeps;
	struct strlist sysdeps;
	struct compiled_dep* compdep;
};

struct mmpkg_dep {
	mmstr const * name;
	mmstr const * min_version; /* inclusive */
	mmstr const * max_version; /* exclusive */

	struct mmpkg_dep * next;
};


/**
 * struct compiled_dep - compiled dependency
 * @pkgname_id: ID of package name
 * @num_pkg:    number of package alternatives that may match the requirement
 * @next_entry_delta: relative compiled_dep pointer offset from the current one
 *                    to the next compiled_dep in the list.
 * @pkgs: array of package alternatives that amy match the requirement
 *
 * This structure represents a processed version of struct mmpkg_dep in the
 * context of a particular binary index. It is meant to be serialized in a
 * bigger buffer representing all the dependencies of a particular package.
 */
struct compiled_dep {
	int pkgname_id;
	short num_pkg;
	short next_entry_delta;
	struct mmpkg* pkgs[];
};


/**
 * struct binindex - structure holding all known binary package
 * @pkgname_idx:        index table mapping package name to package name ID.
 * @pkgname_table:      table of list of package sharing the same name. This
 *                      table is indexed by the package name ID.
 * @pkg_num:            number of packages in struct binindex, counting
 *                      different versions.
 * @num_pkgname:        number of package in struct binindex without counting
 *                      different versions. This corresponds to the length of
 *                      @pkgname_table.
 */
struct binindex {
	struct indextable pkgname_idx;
	struct pkglist* pkgname_table;
	int num_pkgname;
	int pkg_num;
};
int binindex_foreach(struct binindex * binindex,
                     int (* cb)(struct mmpkg*, void*),
                     void * data);

int binindex_sorted_foreach(struct binindex * binindex,
                            int (* cb)(struct mmpkg*, void*),
                            void * data);

struct srcpkg {
	mmstr const * name;
	mmstr const * filename;
	mmstr const * sha256;
	mmstr const * version;
	size_t size;
	int name_id;
};

struct srcindex {
	struct indextable pkgname_idx;
	struct srclist * pkgname_table;
	int num_pkgname;
};

struct install_state {
	struct indextable idx;
	int pkg_num;
};


struct pkglist_iter {
	struct pkglist_entry* curr;
};


/**
 * struct inst_rdeps_iter - data to iterate over installed reverse dependencies
 * @binindex:   binaray index in context which the reverse deps are scanned
 * @install_lut: lookup table of installed packages
 * @rdeps_ids:  array of potential reverse dependencies of @pkgname_id
 * @pkgname_id; id of name of package whose reverse dependencies are listed
 */
struct inst_rdeps_iter {
	const struct binindex* binindex;
	struct mmpkg** install_lut;
	const int* rdeps_ids;
	int rdeps_index;
	int pkgname_id;
};


/**
 * struct rdeps_iter - data to iterate over all the potential reverse
 *                               dependencies of a package
 * @pkg:         package whose reverse dependencies are listed
 * @binindex:    binary index in context from which the reverse deps are scanned
 * @rdeps_ids:   array of potential reverse dependencies of @pkgname_id
 * @rdeps_index: index of the reverse dependency currently processed
 * @curr:        index of the versions currently processed
 */
struct rdeps_iter {
	const struct mmpkg* pkg;
	const struct binindex* binindex;
	const int* rdeps_ids;
	int rdeps_index;
	struct pkglist_entry* curr;
};


static inline
void mmpkg_update_flags(struct mmpkg* pkg, int mask, int set)
{
	if (set)
		pkg->flags |= mask;
	else
		pkg->flags &= ~mask;
}


static inline
int mmpkg_is_ghost(struct mmpkg const * pkg)
{
	return pkg->flags & MMPKG_FLAGS_GHOST;
}


int mmpkg_is_provided_by_repo(struct mmpkg const * pkg,
                              struct repolist_elt const * repo);

void mmpkg_dump(struct mmpkg const * pkg);
void mmpkg_print(struct mmpkg const * pkg);
void mmpkg_save_to_index(struct mmpkg const * pkg, FILE* fp);
void mmpkg_sysdeps_dump(const struct strlist* sysdeps, char const * type);

struct mmpkg_dep* mmpkg_dep_create(char const * name);
void mmpkg_dep_destroy(struct mmpkg_dep * dep);
void mmpkg_dep_dump(struct mmpkg_dep const * deps, char const * type);
void mmpkg_dep_save_to_index(struct mmpkg_dep const * dep, FILE* fp, int lvl);

struct mmpkg const* binindex_lookup(struct binindex* binindex,
                                    mmstr const * name,
                                    struct constraints const * c);
int binindex_is_pkg_upgradeable(struct binindex const * binindex,
                                struct mmpkg const * pkg);
void srcindex_init(struct srcindex* srcindex);
void srcindex_deinit(struct srcindex* srcindex);
void binindex_init(struct binindex* binindex);
void binindex_deinit(struct binindex* binindex);
struct mmpkg* add_pkgfile_to_srcindex(struct srcindex* srcindex,
                                      char const * filename);
struct mmpkg* add_pkgfile_to_binindex(struct binindex* binindex,
                                      char const * filename);
int binindex_populate(struct binindex* binindex, char const * index_filename,
                      struct repolist_elt * repo);
int srcindex_populate(struct srcindex * srcindex, char const * index_filename);
void binindex_dump(struct binindex const * binindex);
int binindex_compute_rdepends(struct binindex* binindex);
int srcindex_get_pkgname_id(struct srcindex* srcindex, const mmstr* name);
int binindex_get_pkgname_id(struct binindex* binindex, const mmstr* name);
struct compiled_dep* binindex_compile_upgrade(const struct binindex* binindex,
                                              struct mmpkg* pkg,
                                              struct buffer* buff);
struct compiled_dep* binindex_compile_dep(const struct binindex* binindex,
                                          const struct mmpkg_dep* dep,
                                          struct buffer* buff);
struct compiled_dep* compile_package(const struct binindex* binindex,
                                     struct mmpkg const * pkg,
                                     struct buffer* buff);
struct compiled_dep* binindex_compile_pkgdeps(const struct binindex* binindex,
                                              struct mmpkg* pkg,
                                              int * flag);
const int* binindex_get_potential_rdeps(const struct binindex* binindex,
                                        int pkgname_id, int* num_rdeps);

int install_state_init(struct install_state* state);
int install_state_copy(struct install_state* restrict dst,
                       const struct install_state* restrict src);
void install_state_deinit(struct install_state* state);
void install_state_fill_lookup_table(const struct install_state* state,
                                     struct binindex* binindex,
                                     struct mmpkg** installed);
const struct mmpkg* install_state_get_pkg(const struct install_state* state,
                                          const mmstr* name);
void install_state_add_pkg(struct install_state* state,
                           const struct mmpkg* pkg);
void install_state_rm_pkgname(struct install_state* state,
                              const mmstr* pkgname);
void install_state_save_to_index(struct install_state* state, FILE* fp);
const struct mmpkg** install_state_sorted_pkgs(struct install_state * is);

const struct mmpkg* inst_rdeps_iter_first(struct inst_rdeps_iter* iter,
                                          const struct mmpkg* pkg,
                                          const struct binindex* binindex,
                                          struct mmpkg** inst_lut);
const struct mmpkg* inst_rdeps_iter_next(struct inst_rdeps_iter* iter);

struct mmpkg* rdeps_iter_first(struct rdeps_iter* iter,
                               const struct mmpkg* pkg,
                               const struct binindex* binindex);
struct mmpkg* rdeps_iter_next(struct rdeps_iter* iter);

const struct mmpkg* pkglist_iter_first(struct pkglist_iter* iter,
                                       const mmstr* pkgname,
                                       const struct binindex* binindex);
const struct mmpkg* pkglist_iter_next(struct pkglist_iter* iter);


static inline
size_t compiled_dep_size(int num_pkg)
{
	size_t size;

	size = sizeof(struct compiled_dep);
	size += num_pkg * sizeof(struct mmpkg);

	return ROUND_UP(size, sizeof(struct compiled_dep));
}


static inline
struct compiled_dep* compiled_dep_next(struct compiled_dep* compdep)
{
	// delta 0 means end of list
	if (compdep->next_entry_delta == 0)
		return NULL;

	return compdep + compdep->next_entry_delta;
}


static inline
int compiled_dep_pkg_match(const struct compiled_dep* compdep,
                           const struct mmpkg* pkg)
{
	int i;

	for (i = 0; i < compdep->num_pkg; i++) {
		if (compdep->pkgs[i] == pkg)
			return 1;
	}

	return 0;
}

#endif /* PACKAGE_UTILS_H */
