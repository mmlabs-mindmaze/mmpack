/*
 * @mindmaze_header@
 */
#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include <assert.h>
#include <curl/curl.h>
#include <stdlib.h>
#include <string.h>
#include <yaml.h>

#include <mmerrno.h>
#include <mmlib.h>

#include "common.h"
#include "mm-alloc.h"
#include "package-utils.h"


/* standard isdigit() is locale dependent making it unnecessarily slow.
 * This macro is here to keep the semantic of isdigit() as usually known by
 * most programmer while issuing the fast implementation of it. */
#define isdigit(c)      ((c) >= '0' && (c) <= '9')


/**
 * pkg_version_compare() - compare package version string
 *
 * This function compare the package version string is a way that take into
 * account the version number. It follows the lexicographic order excepting
 * when an numeric value is encounter. In such a case, the whole numeric
 * value is compared. In effect this ensure the result of the following
 * comparisons :
 *
 * * abcd1.3.5 > abc1.3.5
 * * abc1.3.5 < abc1.29.5
 * * abc1.30.5 > abc1.29.50
 *
 * "any" is an allowed wildcard.
 * Anything is lower and higher than "any". Comparing "any" to itself results
 * in an undefined behavior.
 *
 * * 1.0.0 <= any
 * * any <= 1.0.0
 *
 * Return: an integer less than, equal to, or greater than zero if @v1 is
 * found, respectively, to be less than, to match, or be greater than @v2.
 */
LOCAL_SYMBOL
int pkg_version_compare(char const * v1, char const * v2)
{
	int c1, c2;
	int first_diff;

	// version wildcards
	if (STR_EQUAL(v1, strlen(v1), "any")
	    || STR_EQUAL(v2, strlen(v2), "any"))
		return 0;

	// normal version processing
	do {
		c1 = *v1++;
		c2 = *v2++;

		// Compare the numeric value as a whole
		if (isdigit(c1) && isdigit(c2)) {
			// Skip leading '0' of v1
			while (c1 == '0')
				c1 = *v1++;

			// Skip leading '0' of v2
			while (c2 == '0')
				c2 = *v2++;

			// Advance while scanning a numeric value
			first_diff = 0;
			while (c1 && isdigit(c1) && c2 && isdigit(c2)) {
				if (!first_diff)
					first_diff = c1 - c2;

				c1 = *v1++;
				c2 = *v2++;
			}

			// We are leaving the numeric value. So check the
			// longest numeric value. If equal inspect the first
			// digit difference
			if (isdigit(c1) == isdigit(c2)) {
				if (!first_diff)
					continue;

				return first_diff;
			}

			// Check numeric value of v1 is longest or not
			return isdigit(c1) ? 1 : -1;
		}
	} while (c1 && c2 && c1 == c2);

	return c1 - c2;
}


static inline
int get_install_state_debian(char const * name, char const * version)
{
	char cmd[512];
	char * sys_version = NULL;
	int installed = SYSTEM_PKG_REQUIRED;
	ssize_t nread;
	size_t len = 0;
	char * line = NULL;
	FILE * stream;

	sprintf(cmd, "dpkg --status %s 2>&1", name);
	stream = popen(cmd, "r");
	while ((nread = getline(&line, &len, stream)) != -1) {
		if (STR_STARTS_WITH(line, len, "Status:")) {
			if (strstr(line, "installed") != NULL)
				installed = SYSTEM_PKG_INSTALLED;
		} else if (STR_STARTS_WITH(line, len, "Version:")) {
			sys_version = strdup(line + sizeof("Version:"));
			sys_version[strlen(version) - 1] = '\0';
			if (pkg_version_compare(version, sys_version)) {
				free(sys_version);
				installed = SYSTEM_PKG_REQUIRED;
				goto exit;
			}
			free(sys_version);
		}
	}

exit:
	fclose(stream);
	free(line);
	return installed;
}


LOCAL_SYMBOL
int get_local_system_install_state(char const * name, char const * version)
{
	switch (get_os_id()) {
	case OS_ID_DEBIAN:
		return get_install_state_debian(name, version);

	case OS_ID_WINDOWS_10: /* TODO */
	default:
		return mm_raise_error(ENOSYS, "Unsupported OS");
	}
}


LOCAL_SYMBOL
struct mmpkg * mmpkg_create(char const * name)
{
	struct mmpkg * pkg = malloc(sizeof(*pkg));
	memset(pkg, 0, sizeof(*pkg));
	pkg->name = mmstr_malloc_from_cstr(name);
	return pkg;
}


LOCAL_SYMBOL
void mmpkg_destroy(struct mmpkg * pkg)
{
	if (pkg == NULL)
		return;

	mmstr_free(pkg->name);
	mmstr_free(pkg->version);
	mmstr_free(pkg->filename);
	mmstr_free(pkg->sha256);
	mmstr_free(pkg->source);
	mmstr_free(pkg->desc);
	mmstr_free(pkg->sumsha);

	mmpkg_dep_destroy(pkg->mpkdeps);
	mmpkg_dep_destroy(pkg->sysdeps);
	mmpkg_destroy(pkg->next_version);

	free(pkg);
}


LOCAL_SYMBOL
void mmpkg_dump(struct mmpkg const * pkg)
{
	printf("# %s (%s)\n", pkg->name, pkg->version);
	printf("\tdependencies:\n");
	mmpkg_dep_dump(pkg->mpkdeps, "MMP");
	mmpkg_dep_dump(pkg->sysdeps, "SYS");
	printf("\n");
}


LOCAL_SYMBOL
int mmpkg_is_valid(struct mmpkg const * pkg)
{
	return (  pkg->name != NULL
	       && pkg->version != NULL
	       && pkg->filename != NULL
	       && pkg->sha256 != NULL
	       && pkg->size != 0);
}


LOCAL_SYMBOL
struct mmpkg_dep * mmpkg_dep_create(char const * name)
{
	struct mmpkg_dep * dep = malloc(sizeof(*dep));
	memset(dep, 0, sizeof(*dep));
	dep->name = mmstr_malloc_from_cstr(name);
	return dep;
}


LOCAL_SYMBOL
void mmpkg_dep_destroy(struct mmpkg_dep * dep)
{
	if (dep == NULL)
		return;

	mmstr_free(dep->name);
	mmstr_free(dep->min_version);
	mmstr_free(dep->max_version);

	if (dep->next)
		mmpkg_dep_destroy(dep->next);

	free(dep);
}


LOCAL_SYMBOL
void mmpkg_dep_dump(struct mmpkg_dep const * deps, char const * type)
{
	struct mmpkg_dep const * d = deps;

	while (d != NULL) {
		printf("\t\t [%s] %s [%s -> %s]\n", type, d->name,
		       d->min_version, d->max_version);
		d = d->next;
	}
}


/**
 * mmpkg_get_latest() - get the latest possible version of given package
 *                      inferior to geven maximum
 * ctx:         mmapck context
 * name:        package name
 * max_version: exclusive maximum boundary
 *
 * Return: NULL on error, a pointer to the found package otherwise
 */
LOCAL_SYMBOL
struct mmpkg const * mmpkg_get_latest(struct mmpack_ctx * ctx, mmstr const * name,
                                      mmstr const * max_version)
{
	struct it_entry * entry;
	struct mmpkg * pkg, * latest_pkg;

	assert(mmpack_ctx_is_init(ctx));

	entry = indextable_lookup(&ctx->binindex, name);
	if (entry == NULL || entry->value == NULL)
		return NULL;

	latest_pkg = entry->value;
	pkg = latest_pkg->next_version;

	while (pkg != NULL) {
		if (pkg_version_compare(latest_pkg->version, pkg->version) < 0
		    && pkg_version_compare(pkg->version, max_version) < 0)
			latest_pkg = pkg;

		pkg = pkg->next_version;
	}

	return latest_pkg;
}


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
	struct it_entry * entry;
	struct mmpkg const * pkg;

	assert(dep != NULL);

	/* is a version of the package already installed ? */
	entry = indextable_lookup(&ctx->installed, dep->name);
	if (entry != NULL && entry->value != NULL) {
		pkg = entry->value;
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
	dep->min_version = mmstrdup(version);
	dep->max_version = mmstrdup(version);

	return dep;
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
	int new_dependencies;

	actions = mmpack_action_stack_create();

	/* create initial dependency list from pkg_request list */
	deps = dep_create_from_request(req);
	curr_dep = deps;
	while (req->next) {
		req = req->next;
		curr_dep->next = dep_create_from_request(req);
		curr_dep = curr_dep->next;
	}

	while (deps != NULL) {
		/* handle the last dependency introduced */
		curr_dep = deps;
		while (curr_dep->next != NULL)
			curr_dep = curr_dep->next;

		/* get the corresponding package, at its latest possible version */
		pkg = mmpkg_get_latest(ctx, curr_dep->name, curr_dep->max_version);
		if (pkg == NULL) {
			mm_raise_error(ENODATA, "Cannot resolve dependency: %s", curr_dep->name);
			goto error;
		}

		/* merge this package dependency into the global dependency list */
		tmp = mmpkg_dep_filter(ctx, actions, pkg->mpkdeps, deps, &new_dependencies);
		if (tmp == NULL) {
			mm_raise_error(EAGAIN, "Failure while resolving package: %s\n"
			                       "Try resolving the conflicting dependency manally first",
			                       pkg->name);
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
