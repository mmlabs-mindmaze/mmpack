/*
 * @mindmaze_header@
 */
#ifndef ACTION_SOLVER_H
#define ACTION_SOLVER_H

#include "mmstring.h"
#include "package-utils.h"
#include "context.h"

typedef enum {
	INSTALL_PKG = 1,
	UPGRADE_PKG,
	REMOVE_PKG = -1,
} action;


/**
 * struct action - action to take on prefix hierarchy
 * @action:     type of action to perform
 * @pkg:        pointer to package to install or remove
 * @oldpkg:     pointer to replaced package (applicable for upgrade)
 * @pathname:   path to downloaded mpk file if not NULL
 */
struct action {
	action action;
	struct mmpkg const * pkg;
	struct mmpkg const * oldpkg;
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

struct action_stack * mmpkg_get_install_list(struct mmpack_ctx * ctx,
                                             const struct pkg_request* req);
struct action_stack* mmpkg_get_upgrade_list(struct mmpack_ctx * ctx,
                                            const struct pkg_request* reqlist);
struct action_stack* mmpkg_get_remove_list(struct mmpack_ctx * ctx,
                                           const struct pkg_request* reqlist);

struct action_stack * mmpack_action_stack_create(void);
void mmpack_action_stack_destroy(struct action_stack * stack);
struct action * mmpack_action_stack_pop(struct action_stack * stack);
void mmpack_action_stack_dump(struct action_stack * actions);

int confirm_action_stack_if_needed(int nreq, struct action_stack const * stack);

#endif
