/*
 * @mindmaze_header@
 */
#ifndef PACKAGE_UTILS_H
#define PACKAGE_UTILS_H

#include <stdio.h>

#include "context.h"
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

typedef enum {
	INSTALL_PKG = 1,
	REMOVE_PKG = -1,
} action;


/**
 * struct action - action to take on prefix hierarchy
 * @action:     type of action to perform
 * @pkg:        pointer to package to install
 * @pathname:   path to downloaded mpk file if not NULL
 */
struct action {
	action action;
	struct mmpkg const * pkg;
	const mmstr* pathname;
};

struct action_stack {
	int index;
	int size;
	struct action actions[];
};

struct pkg_request {
	const mmstr* name;
	const mmstr* version;
	struct pkg_request* next;
};

struct mmpkg * mmpkg_create(char const * name);
void mmpkg_destroy(struct mmpkg * pkg);
void mmpkg_dump(struct mmpkg const * pkg);
int mmpkg_is_valid(struct mmpkg const * pkg);
void mmpkg_save_to_index(struct mmpkg const * pkg, FILE* fp);

struct mmpkg_dep * mmpkg_dep_create(char const * name);
void mmpkg_dep_destroy(struct mmpkg_dep * dep);
void mmpkg_dep_dump(struct mmpkg_dep const * deps, char const * type);
void mmpkg_dep_save_to_index(struct mmpkg_dep const * dep, FILE* fp, int lvl);

struct mmpkg const * mmpkg_get_latest(struct mmpack_ctx * ctx, mmstr const * name,
                                mmstr const * max_version);

struct action_stack * mmpkg_get_install_list(struct mmpack_ctx * ctx,
                                             const struct pkg_request* req);

struct action_stack * mmpack_action_stack_create(void);
void mmpack_action_stack_destroy(struct action_stack * stack);
struct action * mmpack_action_stack_pop(struct action_stack * stack);
void mmpack_action_stack_dump(struct action_stack * actions);

#endif /* PACKAGE_UTILS_H */
