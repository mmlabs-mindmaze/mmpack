/*
 * @mindmaze_header@
 */
#ifndef ACTION_SOLVER_H
#define ACTION_SOLVER_H

#include "mmstring.h"
#include "package-utils.h"
#include "context.h"

#define INSTALL_PKG 1
#define UPGRADE_PKG 0
#define REMOVE_PKG -1

#define SOLVER_ERROR (1 << 0)

#define ACTFL_FROM_PREFIX   (1 << 0)

/**
 * struct action - action to take on prefix hierarchy
 * @action:     type of action to perform
 * @flags:      flags (ACTFL_*)
 * @pkg:        pointer to package to install or remove
 * @oldpkg:     pointer to replaced package (applicable for upgrade)
 * @pathname:   path to downloaded mpk file if not NULL
 */
struct action {
	int action;
	int flags;
	struct binpkg const * pkg;
	struct binpkg const * oldpkg;
	mmstr* pathname;
};

struct action_stack {
	int index;
	int size;
	struct action actions[];
};

struct pkg_request {
	const mmstr* name;
	const mmstr* version;
	struct binpkg const * pkg;
	struct pkg_request* next;
};

struct action_stack* mmpkg_get_install_list(struct mmpack_ctx * ctx,
                                            const struct pkg_request* req);
struct action_stack* mmpkg_get_upgrade_list(struct mmpack_ctx * ctx,
                                            const struct pkg_request* reqlist);
struct action_stack* mmpkg_get_remove_list(struct mmpack_ctx * ctx,
                                           const struct pkg_request* reqlist);

struct action_stack* mmpack_action_stack_create(void);
void mmpack_action_stack_destroy(struct action_stack * stack);
struct action_stack* mmpack_action_stack_push(struct action_stack * stack,
                                              int action,
                                              struct binpkg const * pkg,
                                              struct binpkg const * oldpkg);

int confirm_action_stack_if_needed(int nreq, struct action_stack const * stack);

#endif /* ifndef ACTION_SOLVER_H */
