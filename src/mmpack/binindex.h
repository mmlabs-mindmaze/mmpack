/*
 * @mindmaze_header@
 */
#ifndef BININDEX_H
#define BININDEX_H

#include "binpkg.h"
#include "buffer.h"
#include "common.h"
#include "constraints.h"
#include "indextable.h"
#include "mmstring.h"
#include "repo.h"


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


struct pkglist_iter {
	struct pkglist_entry* curr;
};


struct pkg_iter {
	struct pkglist* curr_list;
	struct pkglist* list_ptr_bound;
	struct pkglist_entry* pkglist_elt;
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
struct binpkg* binindex_add_pkg(struct binindex* binindex, struct binpkg* pkg);
struct binpkg* add_pkgfile_to_binindex(struct binindex* binindex,
                                       char const * filename);
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


const struct binpkg* pkglist_iter_first(struct pkglist_iter* iter,
                                        const mmstr* pkgname,
                                        const struct binindex* binindex);
const struct binpkg* pkglist_iter_next(struct pkglist_iter* iter);
struct binpkg* pkg_iter_first(struct pkg_iter* pkg_iter,
                              const struct binindex* binindex);
struct binpkg* pkg_iter_next(struct pkg_iter* pkg_iter);

const struct binpkg* inst_rdeps_iter_first(struct inst_rdeps_iter* iter,
                                           const struct binpkg* pkg,
                                           const struct binindex* binindex,
                                           struct binpkg** inst_lut);
const struct binpkg* inst_rdeps_iter_next(struct inst_rdeps_iter* iter);

struct binpkg* rdeps_iter_first(struct rdeps_iter* iter,
                                const struct binpkg* pkg,
                                const struct binindex* binindex);
struct binpkg* rdeps_iter_next(struct rdeps_iter* iter);


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
#endif /* BININDEX_H */
