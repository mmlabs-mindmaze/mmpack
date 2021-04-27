/*
 * @mindmaze_header@
 */
#ifndef PACKAGE_UTILS_H
#define PACKAGE_UTILS_H

#include <stdio.h>

#include "binpkg.h"
#include "constraints.h"
#include "indextable.h"
#include "mmstring.h"
#include "repo.h"
#include "strlist.h"
#include "utils.h"


int pkg_version_compare(char const * v1, char const * v2);


/**
 * struct compiled_dep - compiled dependency
 * @pkgname_id: ID of package name
 * @num_pkg:    number of package alternatives that may match the requirement
 * @next_entry_delta: relative compiled_dep pointer offset from the current one
 *                    to the next compiled_dep in the list.
 * @pkgs: array of package alternatives that amy match the requirement
 *
 * This structure represents a processed version of struct pkgdep in the
 * context of a particular binary index. It is meant to be serialized in a
 * bigger buffer representing all the dependencies of a particular package.
 */
struct compiled_dep {
	int pkgname_id;
	short num_pkg;
	short next_entry_delta;
	struct binpkg* pkgs[];
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

struct install_state {
	struct indextable idx;
	int pkg_num;
};


struct pkglist_iter {
	struct pkglist_entry* curr;
};


struct pkg_iter {
	struct pkglist* curr_list;
	struct pkglist* list_ptr_bound;
	struct pkglist_entry* pkglist_elt;
};


struct inststate_iter {
	struct it_iterator it_iter;
};


static inline
const struct binpkg* inststate_first(struct inststate_iter* iter,
                                     struct install_state* state)
{
	struct it_entry* entry;

	entry = it_iter_first(&iter->it_iter, &state->idx);
	if (!entry)
		return NULL;

	return entry->value;
}


static inline
const struct binpkg* inststate_next(struct inststate_iter* iter)
{
	struct it_entry* entry;

	entry = it_iter_next(&iter->it_iter);
	if (!entry)
		return NULL;

	return entry->value;
}


/**
 * struct inst_rdeps_iter - data to iterate over installed reverse dependencies
 * @binindex:   binaray index in context which the reverse deps are scanned
 * @install_lut: lookup table of installed packages
 * @rdeps_ids:  array of potential reverse dependencies of @pkgname_id
 * @pkgname_id; id of name of package whose reverse dependencies are listed
 */
struct inst_rdeps_iter {
	const struct binindex* binindex;
	struct binpkg** install_lut;
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
	const struct binpkg* pkg;
	const struct binindex* binindex;
	const int* rdeps_ids;
	int rdeps_index;
	struct pkglist_entry* curr;
};


struct binpkg const* binindex_lookup(struct binindex* binindex,
                                     mmstr const * name,
                                     struct constraints const * c);
int binindex_is_pkg_upgradeable(struct binindex const * binindex,
                                struct binpkg const * pkg);
void binindex_init(struct binindex* binindex);
void binindex_deinit(struct binindex* binindex);
struct binpkg* add_pkgfile_to_binindex(struct binindex* binindex,
                                       char const * filename);
int binindex_populate(struct binindex* binindex, char const * index_filename,
                      const struct repo* repo);
struct binpkg** binindex_sorted_pkgs(struct binindex * binindex);
int binindex_compute_rdepends(struct binindex* binindex);
int binindex_get_pkgname_id(struct binindex* binindex, const mmstr* name);
struct compiled_dep* binindex_compile_upgrade(const struct binindex* binindex,
                                              struct binpkg* pkg,
                                              struct buffer* buff);
struct compiled_dep* binindex_compile_dep(const struct binindex* binindex,
                                          const struct pkgdep* dep,
                                          struct buffer* buff);
struct compiled_dep* compile_package(const struct binindex* binindex,
                                     struct binpkg const * pkg,
                                     struct buffer* buff);
struct compiled_dep* binindex_compile_pkgdeps(const struct binindex* binindex,
                                              struct binpkg* pkg,
                                              int * flag);
const int* binindex_get_potential_rdeps(const struct binindex* binindex,
                                        int pkgname_id, int* num_rdeps);

int install_state_init(struct install_state* state);
int install_state_copy(struct install_state* restrict dst,
                       const struct install_state* restrict src);
void install_state_deinit(struct install_state* state);
void install_state_fill_lookup_table(const struct install_state* state,
                                     struct binindex* binindex,
                                     struct binpkg** installed);
const struct binpkg* install_state_get_pkg(const struct install_state* state,
                                           const mmstr* name);
void install_state_add_pkg(struct install_state* state,
                           const struct binpkg* pkg);
void install_state_rm_pkgname(struct install_state* state,
                              const mmstr* pkgname);
void install_state_save_to_index(struct install_state* state, FILE* fp);
const struct binpkg** install_state_sorted_pkgs(struct install_state * is);

const struct binpkg* inst_rdeps_iter_first(struct inst_rdeps_iter* iter,
                                           const struct binpkg* pkg,
                                           const struct binindex* binindex,
                                           struct binpkg** inst_lut);
const struct binpkg* inst_rdeps_iter_next(struct inst_rdeps_iter* iter);

struct binpkg* rdeps_iter_first(struct rdeps_iter* iter,
                                const struct binpkg* pkg,
                                const struct binindex* binindex);
struct binpkg* rdeps_iter_next(struct rdeps_iter* iter);

const struct binpkg* pkglist_iter_first(struct pkglist_iter* iter,
                                        const mmstr* pkgname,
                                        const struct binindex* binindex);
const struct binpkg* pkglist_iter_next(struct pkglist_iter* iter);
struct binpkg* pkg_iter_first(struct pkg_iter* pkg_iter,
                              const struct binindex* binindex);
struct binpkg* pkg_iter_next(struct pkg_iter* pkg_iter);


static inline
size_t compiled_dep_size(int num_pkg)
{
	size_t size;

	size = sizeof(struct compiled_dep);
	size += num_pkg * sizeof(struct binpkg);

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
                           const struct binpkg* pkg)
{
	int i;

	for (i = 0; i < compdep->num_pkg; i++) {
		if (compdep->pkgs[i] == pkg)
			return 1;
	}

	return 0;
}

#endif /* PACKAGE_UTILS_H */
