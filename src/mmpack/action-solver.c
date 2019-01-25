/*
 * @mindmaze_header@
 */
#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include <assert.h>
#include <mmerrno.h>
#include <stdio.h>

#include "action-solver.h"
#include "context.h"
#include "package-utils.h"
#include "utils.h"


#define DEFAULT_STACK_SZ 10
LOCAL_SYMBOL
struct action_stack * mmpack_action_stack_create(void)
{
	size_t stack_size;
	struct action_stack * stack;

	stack_size = sizeof(*stack) + DEFAULT_STACK_SZ * sizeof(*stack->actions);
	stack = malloc(stack_size);
	memset(stack, 0, stack_size);
	stack->size = DEFAULT_STACK_SZ;

	return stack;
}


LOCAL_SYMBOL
void mmpack_action_stack_destroy(struct action_stack * stack)
{
	int i;

	if (!stack)
		return;

	for (i = 0; i < stack->index; i++)
		mmstr_free(stack->actions[i].pathname);

	free(stack);
}


static
struct action_stack * mmpack_action_stack_push(struct action_stack * stack,
                                                      action action,
                                                      struct mmpkg const * pkg)
{
	/* increase by DEFAULT_STACK_SZ if full */
	if ((stack->index + 1) == stack->size) {
		size_t stack_size = sizeof(*stack) + (stack->size + DEFAULT_STACK_SZ) * sizeof(*stack->actions);
		stack = mm_realloc(stack, stack_size);
	}

	stack->actions[stack->index] = (struct action) {
		.action = action,
		.pkg = pkg,
	};
	stack->index++;

	return stack;
}


LOCAL_SYMBOL
struct action * mmpack_action_stack_pop(struct action_stack * stack)
{
	struct action * action;

	if (stack->index == 0)
		return NULL;

	stack->index --;
	action = &stack->actions[stack->index];

	return action;
}


static
struct mmpkg_dep * mmpkg_dep_append_copy(struct mmpkg_dep * deps,
                                         struct mmpkg_dep const * new_deps,
                                         mmstr const * min_version,
                                         mmstr const * max_version)
{
	struct mmpkg_dep * d;
	struct mmpkg_dep * new = malloc(sizeof(*new));
	new->name = mmstrdup(new_deps->name);
	new->min_version = mmstrdup(min_version);
	new->max_version = mmstrdup(max_version);
	new->next = NULL;  /* ensure there is no bad chaining leftover */

	if (deps == NULL)
		return new;

	d = deps;
	while (d->next)
		d = d->next;

	d->next = new;

	return deps;
}


/**
 * dependency_already_met() - check whether a dependency has already been met
 * @ctx:     mmpack context
 * @actions: currently planned actions
 * @dep:     the dependency being investigated
 *
 * Return:
 *    1 if dependency has already been met
 *    0 if not
 *   -1 on version conflict
 */
static
int dependency_already_met(struct mmpack_ctx * ctx,
                           struct action_stack const * actions,
                           struct mmpkg_dep const * dep)
{
	int i;
	struct mmpkg const * pkg;

	assert(dep != NULL);

	/* is a version of the package already installed ? */
	pkg = install_state_get_pkg(&ctx->installed, dep->name);
	if (pkg != NULL) {
		if (pkg_version_compare(pkg->version, dep->max_version) <= 0
		    && pkg_version_compare(dep->min_version, pkg->version) <= 0)
			return 1;

		/* package found installed, but with an incompatible version */
		return -1;
	}

	/* package not already installed in the system, but maybe its installation
	 * has already been planned */
	for (i = 0 ; i < actions->index ; i++) {
		pkg = actions->actions[i].pkg;
		if (!mmstrequal(pkg->name, dep->name))
			continue;

		if (pkg_version_compare(pkg->version, dep->max_version) <= 0
		    && pkg_version_compare(dep->min_version, pkg->version) <= 0)
			return 1;

		/* package was planned to be installed, but with an incompatible version */
		return -1;
	}

	/* dependency not met */
	return 0;
}


static
struct mmpkg_dep * mmpkg_dep_lookup(struct mmpkg_dep * deplist,
                                    struct mmpkg_dep const * dep)
{
	while (deplist != NULL) {
		if (mmstrequal(deplist->name, dep->name))
			return deplist;

		deplist = deplist->next;
	}

	return NULL;
}


static
struct mmpkg_dep * mmpkg_dep_remove(struct mmpkg_dep * deps, mmstr const * name)
{
	struct mmpkg_dep * tmp;
	struct mmpkg_dep * d = deps;

	tmp = NULL;
	while (d != NULL) {
		if (mmstrequal(d->name, name)) {
			if (tmp == NULL)
				deps = d->next;
			else
				tmp->next = d->next;

			d->next = NULL;
			mmpkg_dep_destroy(d);
			return deps;
		}
		tmp = d;
		d = d->next;
	}

	assert(0);  /* should always remove an existing element */
	return deps;
}


static
struct mmpkg_dep * mmpkg_dep_update(struct mmpkg_dep * deps_out,
                                    struct mmpkg_dep const * new_dep)
{
	mmstr const * min_version, * max_version;
	struct mmpkg_dep * tmp, * old_dep;
	old_dep = mmpkg_dep_lookup(deps_out, new_dep);
	assert(old_dep != NULL);

	/* min = MAX(old->min, new->min) */
	if (pkg_version_compare(old_dep->min_version, new_dep->min_version) > 0)
		min_version = old_dep->min_version;
	else
		min_version = new_dep->min_version;

	/* max = MIN(old->max, new->max) */
	if (pkg_version_compare(old_dep->max_version, new_dep->max_version) < 0)
		max_version = old_dep->max_version;
	else
		max_version = new_dep->max_version;

	/* FAIL if min > max */
	if (pkg_version_compare(min_version, max_version) > 0)
		return NULL;

	/* create a copy of the new dependency at the end of the list */
	tmp = mmpkg_dep_append_copy(deps_out, new_dep, min_version, max_version);
	if (tmp == NULL)
		return NULL;
	deps_out = tmp;


	return mmpkg_dep_remove(deps_out, new_dep->name);
}

/**
 * mmpkg_dep_filter() - only keep unmet dependencies from given read-only dependency list.
 * @ctx:             the mmpack context
 * @actions:         the already staged actions
 * deps_in:          the list of dependencies to be filtered
 * deps_out:         the list of dependencies which will receive the new dependencies
 * new_dependencies: flag telling if a new dependency has been added to deps_out
 *
 * The deps_in is expected to be taken directly from the indextable, and as such
 * MUST NOT be touched in any way. The deps_out list will be increased with a
 * copy of the dependency read from deps_in.
 *
 * Return: the filtered list, newly allocated on success, NULL on failure.
 */
static
struct mmpkg_dep * mmpkg_dep_filter(struct mmpack_ctx * ctx,
                                    struct action_stack const * actions,
                                    struct mmpkg_dep const * deps_in,
                                    struct mmpkg_dep * deps_out,
                                    int * new_dependencies)
{
	int rv;
	struct mmpkg_dep * tmp;
	struct mmpkg_dep const * new_dep;

	*new_dependencies = 0;

	for (new_dep = deps_in ; new_dep != NULL ; new_dep = new_dep->next) {
		/* check whether the dependency is already in the unmet dependency list
		 * if it is, move it to the end of the list, and stop investigating it */
		if (mmpkg_dep_lookup(deps_out, new_dep)) {
			deps_out = mmpkg_dep_update(deps_out, new_dep);
			continue;
		}

		/* check whether the dependency is already installed, or already staged in
		 * the action stack */
		rv = dependency_already_met(ctx, actions, new_dep);
		if (rv > 0)
			continue;
		else if (rv < 0)
			return NULL;

		*new_dependencies = 1;
		/* we got deps_in from the global index table, which is read-only.
		 * duplicate it so it can be changed later */
		tmp = mmpkg_dep_append_copy(deps_out, new_dep,
		                            new_dep->min_version, new_dep->max_version);
		if (tmp == NULL)
			return NULL;
		deps_out = tmp;
	}

	return deps_out;
}


static
int mmpkg_depends_on(struct mmpkg const * pkg, mmstr const * name)
{
	struct mmpkg_dep const * d;
	for (d = pkg->mpkdeps ; d != NULL ; d = d->next) {
		if (mmstrequal(d->name, name))
			return 1;
	}

	return 0;
}


static
void mmpack_print_dependencies_on_conflict(struct action_stack const * actions,
                                           struct mmpkg const * pkg)
{
	int i;
	struct mmpkg_dep const * d;
	struct action const * a;

	printf("Package details: \n");
	mmpkg_dump(pkg);

	for (d = pkg->mpkdeps ; d != NULL ; d = d->next) {
		for (i = 0 ; i < actions->index ; i++) {
			a = &actions->actions[i];

			if (mmpkg_depends_on(a->pkg, d->name)) {
				printf("Package already staged with conflicting dependency :\n");
				mmpkg_dump(a->pkg);
			}
		}
	}
}


static
struct mmpkg_dep* dep_create_from_request(const struct pkg_request* req)
{
	struct mmpkg_dep * dep = NULL;
	const char* version = req->version ? req->version : "any";

	dep = mmpkg_dep_create(req->name);
	dep->min_version = mmstr_malloc_from_cstr(version);
	dep->max_version = mmstr_malloc_from_cstr(version);

	return dep;
}


static
struct mmpkg_dep* create_dependencies_from_reqlist(struct mmpack_ctx* ctx,
                                                   const struct pkg_request* req,
                                                   struct action_stack const * actions)
{
	struct mmpkg_dep* deps = NULL;
	struct mmpkg_dep* curr_dep;

	while (req != NULL) {
		curr_dep = dep_create_from_request(req);
		if (dependency_already_met(ctx, actions, curr_dep) == 1) {
			mmpkg_dep_destroy(curr_dep);
		} else {
			curr_dep->next = deps;
			deps = curr_dep;
		}
		req = req->next;
	}

	return deps;
}

/**
 * mmpkg_get_install_list() -  parse package dependencies and return install order
 * @ctx:     the mmpack context
 * @req:     requested package list to be installed
 *
 * In brief, this function will initialize a dependency ordered list with a
 * single element: the package passed as argument.
 *
 * Then while this list is not empty, take the last package of the list.
 * - if it introduces no new dependency, then stage it as an installed action
 *   in the action stack.
 * - if it has unmet *new* dependencies, append those to the dependency list,
 *   let the current dependency unmet in the list.
 *
 * This way, a dependency is removed from the list of needed package only if all
 * its dependency are already met.
 *
 * Returns: an action stack of the packages to be installed in the right order
 *          and at the correct version on success.
 *          NULL on error.
 */
LOCAL_SYMBOL
struct action_stack* mmpkg_get_install_list(struct mmpack_ctx * ctx,
                                            const struct pkg_request* req)
{
	void * tmp;
	struct mmpkg const * pkg;
	struct action_stack * actions = NULL;
	struct mmpkg_dep * deps = NULL;
	struct mmpkg_dep *curr_dep;
	struct binindex* binindex = &ctx->binindex;
	int new_dependencies;

	actions = mmpack_action_stack_create();

	/* create initial dependency list from pkg_request list */
	deps = create_dependencies_from_reqlist(ctx, req, actions);

	while (deps != NULL) {
		/* handle the last dependency introduced */
		curr_dep = deps;
		while (curr_dep->next != NULL)
			curr_dep = curr_dep->next;

		/* get the corresponding package, at its latest possible version */
		pkg = binindex_get_latest_pkg(binindex, curr_dep->name, curr_dep->max_version);
		if (pkg == NULL) {
			error("Cannot find package: %s\n", curr_dep->name);
			goto error;
		}

		/* merge this package dependency into the global dependency list */
		tmp = mmpkg_dep_filter(ctx, actions, pkg->mpkdeps, deps, &new_dependencies);
		if (tmp == NULL) {
			error("Failure while resolving package: %s\n", pkg->name);
			printf("Try resolving the conflicting dependency manally first\n");
			mmpack_print_dependencies_on_conflict(actions, pkg);
			goto error;
		}
		deps = tmp;

		/* if the package required yet unmet dependency, consider those first.
		 * leave this one in the dependency list for now */
		if (!new_dependencies) {
			/* current package can be installed.
			 * - add it on top of the stack
			 * - remove it from the list */
			tmp = mmpack_action_stack_push(actions, INSTALL_PKG, pkg);
			if (tmp == NULL)
				goto error;
			actions = tmp;

			deps = mmpkg_dep_remove(deps, pkg->name);
		}
	}

	return actions;

error:
	mmpack_action_stack_destroy(actions);
	mmpkg_dep_destroy(deps);

	return NULL;
}


/**
 * remove_package() - remove a package and its reverse dependencies
 * @pkgname:    name of package to remove
 * @binindex:   index of binary packages
 * @state:      temporary install state used to track removed packages
 * @stack:
 */
static
void remove_package(const mmstr* pkgname, struct binindex* binindex,
                    struct install_state* state, struct action_stack** stack)
{
	struct rdeps_iter iter;
	const struct mmpkg* rdep_pkg;
	const struct mmpkg* pkg;

	// Check this has not already been done
	pkg = install_state_get_pkg(state, pkgname);
	if (!pkg)
		return;

	// Mark it now remove from state (this avoid infinite loop in the
	// case of circular dependency)
	install_state_rm_pkgname(state, pkgname);

	// First remove recursively the reverse dependencies
	rdep_pkg = rdeps_iter_first(&iter, pkg, binindex, state);
	while (rdep_pkg) {
		remove_package(rdep_pkg->name, binindex, state, stack);
		rdep_pkg = rdeps_iter_next(&iter);
	}

	*stack = mmpack_action_stack_push(*stack, REMOVE_PKG, pkg);
}


/**
 * mmpkg_get_remove_list() -  compute a remove order
 * @ctx:     the mmpack context
 * @reqlist: requested package list to be removed
 *
 * Returns: an action stack of the actions to be applied in order to remove
 * the list of package in @reqlist in the right order.
 */
LOCAL_SYMBOL
struct action_stack* mmpkg_get_remove_list(struct mmpack_ctx * ctx,
                                           const struct pkg_request* reqlist)
{
	struct install_state state;
	struct action_stack * actions = NULL;
	const struct pkg_request* req;

	actions = mmpack_action_stack_create();

	// Copy the current install state of prefix context in order to
	// simulate the operation done on installed package list
	install_state_copy(&state, &ctx->installed);

	for (req = reqlist; req; req = req->next)
		remove_package(req->name, &ctx->binindex, &state, &actions);

	return actions;
}


LOCAL_SYMBOL
void mmpack_action_stack_dump(struct action_stack * stack)
{
	int i;

	for (i =  0 ; i < stack->index ; i++) {
		if (stack->actions[i].action == INSTALL_PKG)
			printf("INSTALL: ");
		else if (stack->actions[i].action == REMOVE_PKG)
			printf("REMOVE: ");

		mmpkg_dump(stack->actions[i].pkg);
	}
}


LOCAL_SYMBOL
int confirm_action_stack_if_needed(int nreq, struct action_stack const * stack)
{
	int i, rv;

	if (stack->index == 0) {
		printf("Nothing to do.\n");
		return 0;
	}

	printf("Transaction summary:\n");

	for (i =  0 ; i < stack->index ; i++) {
		if (stack->actions[i].action == INSTALL_PKG)
			printf("INSTALL: ");
		else if (stack->actions[i].action == REMOVE_PKG)
			printf("REMOVE: ");

		printf("%s (%s)\n", stack->actions[i].pkg->name,
		                  stack->actions[i].pkg->version);
	}

	if (nreq == stack->index) {
		/* mmpack is installing as many packages as requested:
		 * - they are exactly the one requested
		 * - they introduce no additional dependencies
		 *
		 * proceed with install without confirmation */
		return 0;
	}

	rv = prompt_user_confirm();
	if (rv != 0)
		printf("Abort.\n");

	return rv;
}
