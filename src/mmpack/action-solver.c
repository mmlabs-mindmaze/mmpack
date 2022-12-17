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
#include "buffer.h"
#include "context.h"
#include "package-utils.h"
#include "strset.h"
#include "utils.h"

enum {
	DONE,
	CONTINUE,
};

enum solver_state {
	VALIDATION,
	SELECTION,
	UPGRADE_RDEPS,
	INSTALL_DEPS,
	NEXT,
	BACKTRACK,
};

#define DO_UPGRADE (1 << 0)

/**
 * struct proc_frame - processing data frame
 * @ipkg:       index of package currently selected for installation
 * @flags:      flags modify processing behavipr
 * @state:      next step to perform
 * @dep:        current compiled dependency being processed
 *
 * This structure hold the data to keep track the processing of one node
 * while walking in the directed graph that represents the binary index.
 * For example, when installing a package, its dependencies must be
 * inspected. The struct proc_frame keep track which dependencies is being
 * inspected. If one on the dependences must installed a new struct
 * proc_frame is created and must be stacked.
 */
struct proc_frame {
	short ipkg;
	short flags;
	enum solver_state state;
	struct compiled_dep* dep;
};


/**
 * struct decision_state - snapshot of processing state at decision time
 * @last_decstate_sz:   previous top index of decstate_store of solver
 * @ops_stack_size:     top index of ops_stack of solver
 * @upgrades_stack_sz:  previous top index of upgrades_stack
 * @curr_frame:         current processing data frame
 * @pkg_index:          index of chosen package in dependency being processed
 * @num_proc_frame:     current depth of processing stack
 * @proc_frames:        content of processing stack (length @num_proc_frame)
 *
 * Structure representing a snapshot of the internal data of solver needed
 * to go back at the time of previous decision.
 */
struct decision_state {
	size_t last_decstate_sz;
	size_t ops_stack_size;
	size_t upgrades_stack_sz;
	struct proc_frame curr_frame;
	int pkg_index;
	int num_proc_frame;
	struct proc_frame proc_frames[];
};


/**
 * struct planned_op - data representing a change in inst_lut and stage_lut
 * @action:     type of action: STAGE, INSTALL, REMOVE or UPGRADE
 * @id:         package name id involved by the change
 * @pkg:        pointer to package installed or removed
 * @pkg_diff:   pointer difference between new and old package. Valid only
 *              in the case of UPGRADE
 */
struct planned_op {
	enum {STAGE, INSTALL, REMOVE, UPGRADE} action;
	int id;
	union {
		struct binpkg* pkg;
		intptr_t pkg_diff;
	};
};


/**
 * struct solver - solver context
 * @binindex:   binary index used to inspect dependencies
 * @inst_lut:   lookup table of installed package
 * @stage_lut:  lookup table of package staged to be installed
 * @processing_stack: stack of processing frames
 * @decstate_store:   previous decision states store
 * @last_decstate_sz: decision state store size before last decision
 * @upgrades_stack:   stack of stored upgrade list
 * @ops_stack:  stack of planned operations
 * @num_proc_frame:   current depth of processing stack
 * @state:            flags about the solver state, used to raise errors
 */
struct solver {
	struct binindex* binindex;
	struct binpkg** inst_lut;
	struct binpkg** stage_lut;
	struct buffer processing_stack;
	struct buffer decstate_store;
	size_t last_decstate_sz;
	struct buffer ops_stack;
	struct buffer upgrades_stack;
	int num_proc_frame;
	int state;
};


/**************************************************************************
 *                                                                        *
 *                             Action stack                               *
 *                                                                        *
 **************************************************************************/
#define DEFAULT_STACK_SZ 10
/**
 * mmpack_action_stack_create() - create mmpack action stack
 *
 * Created object must be destroyed with mmpack_action_stack_destroy()
 *
 * Return: an initialized action_stack object.
 */
LOCAL_SYMBOL
struct action_stack* mmpack_action_stack_create(void)
{
	size_t stack_size;
	struct action_stack * stack;

	stack_size = sizeof(*stack) + DEFAULT_STACK_SZ *
	             sizeof(*stack->actions);
	stack = xx_malloc(stack_size);
	memset(stack, 0, stack_size);
	stack->size = DEFAULT_STACK_SZ;

	return stack;
}


/**
 * mmpack_action_stack_destroy() - destroy action stack
 * @stack: the action stack to destroy
 */
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


/**
 * mmpack_action_stack_push() - push described action unto the stack
 * @stack: initialized stack structure
 * @action: action to push unto the stack. Eg. INSTALL_PKG
 * @pkg: package to manipulate
 * @oldpkg: optional argument for removal cases.
 *
 * Return the updated stack pointer
 */
LOCAL_SYMBOL
struct action_stack* mmpack_action_stack_push(struct action_stack * stack,
                                              int action,
                                              struct binpkg const * pkg,
                                              struct binpkg const * oldpkg)
{
	/* increase by DEFAULT_STACK_SZ if full */
	if ((stack->index + 1) == stack->size) {
		size_t stack_size = sizeof(*stack) +
		                    (stack->size += DEFAULT_STACK_SZ) *
		                    sizeof(*stack->actions);
		stack = xx_realloc(stack, stack_size);
	}

	stack->actions[stack->index] = (struct action) {
		.action = action,
		.pkg = pkg,
		.oldpkg = oldpkg,
	};
	stack->index++;

	return stack;
}


/**************************************************************************
 *                                                                        *
 *                            solver context                              *
 *                                                                        *
 **************************************************************************/

/**
 * get_compdep_with_id() - get element involving a package from a list
 * @dep:        list of compiled dependencies
 * @id:         id of name whose package must be involved in the target dep
 *
 * Within a compiled dependency list supplied by @dep, this function search
 * the element whose depedendency involved @id as package name id.
 *
 * Return: the pointer to the compiled dependendency element involving
 * @id as package name, NULL if no element can be found within @dep.
 */
static
struct compiled_dep* get_compdep_with_id(struct compiled_dep* dep, int id)
{
	while (dep) {
		if (dep->pkgname_id == id)
			return dep;

		dep = compiled_dep_next(dep);
	}

	return NULL;
}


/**
 * solver_clean_upgrade_stack() - free most recent upgrade lists
 * @solver:     solver context to update
 * @prev_size:  size of the @solver->upgrades_stack up to which list have
 *              to be freed
 */
static
void solver_clean_upgrade_stack(struct solver* solver, size_t prev_size)
{
	struct compiled_dep* upgrades;

	while (solver->upgrades_stack.size > prev_size) {
		buffer_pop(&solver->upgrades_stack,
		           &upgrades, sizeof(uintptr_t));

		free(upgrades);
	}
}


static
void solver_init(struct solver* solver, struct mmpack_ctx* ctx)
{
	size_t size;

	*solver = (struct solver) {.binindex = &ctx->binindex};

	size = ctx->binindex.num_pkgname * sizeof(*solver->inst_lut);
	solver->inst_lut = xx_malloc(size);
	solver->stage_lut = xx_malloc(size);

	install_state_fill_lookup_table(&ctx->installed, &ctx->binindex,
	                                solver->inst_lut);
	memset(solver->stage_lut, 0, size);
	buffer_init(&solver->processing_stack);
	buffer_init(&solver->decstate_store);
	buffer_init(&solver->ops_stack);
	buffer_init(&solver->upgrades_stack);
}


static
void solver_deinit(struct solver* solver)
{
	solver_clean_upgrade_stack(solver, 0);
	buffer_deinit(&solver->upgrades_stack);
	buffer_deinit(&solver->ops_stack);
	buffer_deinit(&solver->decstate_store);
	buffer_deinit(&solver->processing_stack);
	free(solver->inst_lut);
	free(solver->stage_lut);
}


/**
 * solver_revert_planned_ops() - undo latest actions up to a previous state
 * @solver:     solver context to update
 * @prev_size:  size of the ops_stack up to which action must be undone
 *
 * This function undo the actions stored in the planned operation stack in
 * @solver from top to @prev_size. After this function, @solver->stage_lut
 * and solver->inst_lut will be the same as it was when @prev_size was the
 * actual size of @solver->ops_stack.
 */
static
void solver_revert_planned_ops(struct solver* solver, size_t prev_size)
{
	struct buffer* ops_stack = &solver->ops_stack;
	struct planned_op op;
	char * ptr;
	struct binpkg * pkg;

	while (ops_stack->size > prev_size) {
		buffer_pop(ops_stack, &op, sizeof(op));

		switch (op.action) {
		case STAGE:
			solver->stage_lut[op.id] = NULL;
			break;

		case INSTALL:
			solver->inst_lut[op.id] = NULL;
			break;

		case REMOVE:
			solver->inst_lut[op.id] = op.pkg;
			break;

		case UPGRADE:
			ptr = (char*) solver->inst_lut[op.id];
			pkg = (struct binpkg*) (ptr - op.pkg_diff);
			solver->inst_lut[op.id] = pkg;
			break;

		default:
			mm_crash("Unexpected action type: %i", op.action);
		}
	}
}


/**
 * solver_stage_pkg_install() - mark a package intended to be installed
 * @solver:     solver context to update
 * @id:         package name id
 * @pkg:        pointer to package intended to be installed
 *
 * This function will update the lookup table of staged package in @solver
 * and will register the change in @solver->ops_stack.
 */
static
void solver_stage_pkg_install(struct solver* solver, int id,
                              struct binpkg* pkg)
{
	struct planned_op op = {.action = STAGE, .pkg = pkg, .id = id};

	solver->stage_lut[id] = pkg;
	buffer_push(&solver->ops_stack, &op, sizeof(op));
}


/**
 * solver_commit_pkg_install() - register the package install operation
 * @solver:     solver context to update
 * @id:         package name id
 *
 * This function will update the lookup table of install package in @solver
 * and will register the change in @solver->ops_stack. This function can
 * only be called after solver_stage_pkg_install() has been called for the
 * same @id.
 */
static
void solver_commit_pkg_install(struct solver* solver, int id)
{
	struct binpkg* oldpkg;
	struct binpkg* pkg = solver->stage_lut[id];
	struct planned_op op = {.action = INSTALL, .pkg = pkg, .id = id};

	oldpkg = solver->inst_lut[id];
	solver->inst_lut[id] = pkg;

	if (oldpkg) {
		op.action = UPGRADE;
		op.pkg_diff = (char*)pkg - (char*)oldpkg;
	}

	buffer_push(&solver->ops_stack, &op, sizeof(op));
}


/**
 * solver_save_decision_state() - save solver state associated with a decision
 * @solver:     solver context to update
 * @frame:      pointer to the current processing frame
 *
 * This function is called when a choice is presented to the solver (to
 * choose installing a package or another). It stores the necessary
 * information of @solver to possibly backtrack on the decision later.
 */
static
void solver_save_decision_state(struct solver* solver,
                                struct proc_frame* frame)
{
	struct decision_state* state;
	struct proc_frame* proc_frames = solver->processing_stack.base;
	size_t state_sz;
	int i, nframe;

	// There is no point of saving decision since there is no
	// alternative anymore
	if (frame->ipkg >= frame->dep->num_pkg-1)
		return;

	// Reserve the data to store the whole state at decision point
	nframe = solver->num_proc_frame;
	state_sz = sizeof(*state) + nframe * sizeof(*proc_frames);
	state = buffer_reserve_data(&solver->decstate_store, state_sz);

	// Save all state of @solver needed to restore processing at the
	// moment of the decision
	state->last_decstate_sz = solver->last_decstate_sz;
	state->ops_stack_size = solver->ops_stack.size;
	state->upgrades_stack_sz = solver->upgrades_stack.size;
	state->num_proc_frame = nframe;
	state->curr_frame = *frame;
	for (i = 0; i < nframe; i++)
		state->proc_frames[i] = proc_frames[i];

	buffer_inc_size(&solver->decstate_store, state_sz);
	solver->last_decstate_sz = state_sz;
}


/**
 * solver_backtrack_on_decision() - revisit decision and restore state
 * @solver:     solver context to update
 * @frame:      pointer to processing frame
 *
 * This function should be called when a it has been realized that
 * constraints are not satisfiable. It restores the state saved at the
 * latest decision point and pick the next decision alternative.
 *
 * Return: an non-negative package index corresponding to the new package
 * alternative to try. If the return value is negative, there is no more
 * decision to revisit, hence it means that the global requirements are not
 * satisfiable.
 */
static
int solver_backtrack_on_decision(struct solver* solver,
                                 struct proc_frame* frame)
{
	struct decision_state* state;
	struct proc_frame* proc_frames = solver->processing_stack.base;
	int i, nframe;

	// If decision stack is empty, the overall problem is not satisfiable
	if (solver->last_decstate_sz == 0)
		return -1;

	state = buffer_dec_size(&solver->decstate_store,
	                        solver->last_decstate_sz);

	solver->last_decstate_sz = state->last_decstate_sz;
	solver_revert_planned_ops(solver, state->ops_stack_size);
	solver_clean_upgrade_stack(solver, state->upgrades_stack_sz);
	*frame = state->curr_frame;
	nframe = state->num_proc_frame;

	solver->num_proc_frame = nframe;
	solver->processing_stack.size = nframe * sizeof(*proc_frames);
	for (i = 0; i < nframe; i++)
		proc_frames[i] = state->proc_frames[i];

	frame->ipkg++;
	return 0;
}


/**
 * solver_add_deps_to_process() - Add new dependencies to process
 * @solver:     solver context to update
 * @frame:      pointer to the current processing frame
 * @deps:       compiled dependencies to process
 */
static
void solver_add_deps_to_process(struct solver* solver,
                                struct proc_frame* frame,
                                struct compiled_dep* deps)
{
	if (!deps)
		return;

	buffer_push(&solver->processing_stack, frame, sizeof(*frame));
	solver->num_proc_frame++;

	*frame = (struct proc_frame) {.dep = deps, .state = VALIDATION};
}


/**
 * solver_advance_processing() - update processing frame for next step
 * @solver:     solver context to update
 * @frame:      pointer to current processing frame. Updated on output
 *
 * Return: CONTINUE if there new iteration to run, DONE if the @solver
 * found a solution.
 */
static
int solver_advance_processing(struct solver* solver,
                              struct proc_frame* frame)
{
	struct buffer* proc_stack = &solver->processing_stack;

	if (solver->state & SOLVER_ERROR)
		return DONE;

	do {
		if (frame->state == UPGRADE_RDEPS) {
			frame->state = INSTALL_DEPS;
			break;
		}

		if (frame->state == INSTALL_DEPS) {
			// Mark as installed the package whose dependency list
			// has just been processed
			solver_commit_pkg_install(solver,
			                          frame->dep->pkgname_id);
			frame->state = NEXT;
		}

		if (frame->state == NEXT) {
			frame->dep = compiled_dep_next(frame->dep);
			if (frame->dep) {
				frame->state = VALIDATION;
				break;
			}

			// Since the end of dependency list is reached, if the
			// processing stack is empty, then work is finished
			if (solver->num_proc_frame <= 0)
				return DONE;

			// Resume the previous processing frame from stack
			buffer_pop(proc_stack, frame, sizeof(*frame));
			solver->num_proc_frame--;
		}

	} while (frame->state == INSTALL_DEPS || frame->state == NEXT);

	return CONTINUE;
}


/**
 * solver_step_validation() - validate dependency against system state
 * @solver:     solver context to update
 * @frame:      pointer to the current processing frame
 *
 * The function checks if dependency @frame->dep being processed is not
 * already fulfilled by the system state or conflicts with it. It updates
 * @frame->state to point to the next processing step to perform.
 *
 * Return: 0 in case of success, -1 if backtracking is necessary.
 */
static
int solver_step_validation(struct solver* solver, struct proc_frame* frame)
{
	int id, is_staged, is_match;
	struct binpkg* pkg;

	// Get package either installed or planned to be installed
	id = frame->dep->pkgname_id;
	pkg = solver->stage_lut[id];
	is_staged = 1;
	if (!pkg) {
		pkg = solver->inst_lut[id];
		is_staged = 0;
	}

	if (pkg) {
		// check package is suitable
		is_match = compiled_dep_pkg_match(frame->dep, pkg);
		if (is_staged) {
			frame->state = is_match ? NEXT : BACKTRACK;
			return is_match ? 0 : -1;
		}

		if (is_match && !(frame->flags & DO_UPGRADE)) {
			frame->state = NEXT;
			return 0;
		}
	}

	frame->ipkg = 0;
	frame->state = SELECTION;
	return 0;
}


/**
 * solver_step_select_pkg() - select the package to install
 * @solver:     solver context to update
 * @frame:      pointer to the current processing frame
 *
 * The function select the package attempted to be installed to fulfill
 * @frame->dep.  It updates @frame->state to point to the next processing
 * step to perform.
 *
 * Return: 0 in case of success, -1 if backtracking is necessary.
 */
static
int solver_step_select_pkg(struct solver* solver, struct proc_frame* frame)
{
	struct binpkg * pkg, * oldpkg;
	int id = frame->dep->pkgname_id;

	// Check that we are not about reinstall the same package. Since
	// packages are ordered with descending version, this prevents
	// downgrading as well
	pkg = frame->dep->pkgs[frame->ipkg];
	oldpkg = solver->inst_lut[id];
	if (oldpkg == pkg) {
		frame->state = NEXT;
		return -1;
	}

	// backup current state, ie before selected package is staged
	solver_save_decision_state(solver, frame);

	solver_stage_pkg_install(solver, id, pkg);
	frame->state = oldpkg ? UPGRADE_RDEPS : INSTALL_DEPS;
	return 0;
}


/**
 * solver_check_upgrade_rdep() - check/upgrade an individual reverse dependency
 * @solver:     solver context to update
 * @rdep_id:    package name id of reverse dependency of @pkg
 * @pkg:        package staged for installation
 * @buff:       buffer receiving element of compile dep for upgrading the
 *              reverse dependency if necessary
 * @last_upgrade_dep: pointer used to update the pointer to last element in
 *              the upgrade list
 *
 * This function reverse dependency specified by @rdep_id is not broken by
 * the installation of the new package. If a conflict is detected and the
 * reverse dependency is not staged yet, its upgrade will be append in the
 * upgrade list being hold by @buff.
 *
 * Return: 0 in case of success, -1 if backtracking is necessary.
 */
static
int solver_check_upgrade_rdep(struct solver* solver, int rdep_id,
                              const struct binpkg* pkg, struct buffer* buff,
                              struct compiled_dep** last_upgrade_dep)
{
	struct binpkg * rdep;
	struct compiled_dep * dep, * upgrade_dep;
	struct binindex* binindex = solver->binindex;
	int is_rdep_staged;

	// Get installed reverse dependency (staged or installed)
	rdep = solver->stage_lut[rdep_id];
	is_rdep_staged = 1;
	if (!rdep) {
		rdep = solver->inst_lut[rdep_id];
		is_rdep_staged = 0;
	}

	if (!rdep)
		return 0;

	// Get compiled_dep of rdep package involving oldpkg
	dep = binindex_compile_pkgdeps(binindex, rdep, &solver->state);
	dep = get_compdep_with_id(dep, pkg->name_id);
	if (!dep || compiled_dep_pkg_match(dep, pkg))
		return 0;

	// if requirement not fulfilled and reverse dependency is
	// staged, we must revisit previous decision
	if (is_rdep_staged)
		return -1;

	// Requirement not fulfilled but reverse dependency has not
	// been touched yet. Then let's try to upgrade rdep
	upgrade_dep = binindex_compile_upgrade(binindex, rdep, buff);
	if (!upgrade_dep)
		return -1;

	*last_upgrade_dep = upgrade_dep;
	return 0;
}


/**
 * solver_step_upgrade_rdeps() - check/upgrade the reverse deps of package
 * @solver:     solver context to update
 * @frame:      pointer to the current processing frame
 *
 * This step is executed after a package has been selected for
 * installation. If this installation is also an upgrade (a package
 * previously installed will be overwritten), this step check that the
 * reverse dependencies of the old package are not broken by the
 * installation of the new package.
 *
 * If a conflict is detected involving a package not staged yet, the system
 * will try to upgrade it to hopefully resolve the conflicts (this will be
 * backtracked if it appears later not to be a solution).
 *
 * Return: 0 in case of success, -1 if backtracking is necessary.
 */
static
int solver_step_upgrade_rdeps(struct solver* solver, struct proc_frame* frame)
{
	struct binindex* binindex = solver->binindex;
	int i, num;
	const int* rdep_ids;
	struct binpkg* newpkg;
	struct compiled_dep * upgrade_deps, * last_upgrade_dep;
	struct buffer buff;

	buffer_init(&buff);
	newpkg = frame->dep->pkgs[frame->ipkg];
	rdep_ids = binindex_get_potential_rdeps(binindex, newpkg->name_id,
	                                        &num);

	// Check all reverse dependencies of old package for compatibility with
	// the new package
	last_upgrade_dep = NULL;
	for (i = 0; i < num; i++) {
		if (solver_check_upgrade_rdep(solver, rdep_ids[i], newpkg,
		                              &buff, &last_upgrade_dep)) {
			frame->state = BACKTRACK;
			goto exit;
		}
	}

	// If an upgrade list has been created terminate it and queue it for
	// processing otherwise resume current processing (install deps)
	if (last_upgrade_dep) {
		// terminate last element
		last_upgrade_dep->next_entry_delta = 0;

		// Push upgrade list for processing
		upgrade_deps = buffer_take_data_ownership(&buff);
		buffer_push(&solver->upgrades_stack,
		            &upgrade_deps, sizeof(uintptr_t));

		solver_add_deps_to_process(solver, frame, upgrade_deps);
	} else {
		frame->state = INSTALL_DEPS;
	}

exit:
	buffer_deinit(&buff);
	return (frame->state == BACKTRACK) ? -1 : 0;
}


/**
 * solver_step_install_deps() - perform dependency installation queueing
 * @solver:     solver context to update
 * @frame:      pointer to the current processing frame
 *
 * The function queues for processing the dependencies of the package
 * staged for installation.
 * on.
 */
static
void solver_step_install_deps(struct solver* solver, struct proc_frame* frame)
{
	struct compiled_dep* deps;
	struct binpkg* pkg;

	pkg = frame->dep->pkgs[frame->ipkg];

	deps = binindex_compile_pkgdeps(solver->binindex, pkg, &solver->state);
	solver_add_deps_to_process(solver, frame, deps);
}


/**
 * solver_solve_deps() - determine a solution given dependency list
 * @solver:     solver context to update
 * @initial_deps: initial dependency list to try to solve
 * @proc_flags: processing flags apply on the first frame of processing
 *
 * This function is the heart of the package dependency resolution. It
 * takes @initial_deps which is a list of compiled dependencies that
 * represent the constraints of the problem being solved and try to find
 * the actions (package install, removal, upgrade) that lead to those
 * requirements being met.
 *
 * Return: 0 if a solution has been found, -1 if no solution could be found
 */
static
int solver_solve_deps(struct solver* solver, struct compiled_dep* initial_deps,
                      int proc_flags)
{
	struct proc_frame frame = {
		.dep = initial_deps,
		.state = VALIDATION,
		.flags = proc_flags,
	};

	mm_check(initial_deps != NULL);

	while (solver_advance_processing(solver, &frame) != DONE) {
		if (frame.state == BACKTRACK
		    && solver_backtrack_on_decision(solver, &frame))
			return -1;

		if (frame.state == VALIDATION
		    && solver_step_validation(solver, &frame))
			continue;

		if (frame.state == SELECTION
		    && solver_step_select_pkg(solver, &frame))
			continue;

		if (frame.state == UPGRADE_RDEPS
		    && solver_step_upgrade_rdeps(solver, &frame))
			continue;

		if (frame.state == INSTALL_DEPS)
			solver_step_install_deps(solver, &frame);
	}

	return (solver->state & SOLVER_ERROR) ? -1 : 0;
}


/**
 * solver_remove_pkgname() - remove a package and its reverse dependencies
 * @solver:     solver context to update
 * @pkgname_id: id of package name to remove
 */
static
void solver_remove_pkgname(struct solver* solver, int pkgname_id)
{
	struct inst_rdeps_iter iter;
	const struct binpkg* rdep_pkg;
	struct binpkg* pkg;
	struct planned_op op = {.action = REMOVE, .id = pkgname_id};

	// Check this has not already been done
	pkg = solver->inst_lut[pkgname_id];
	if (!pkg)
		return;

	// Mark it now remove from installed lookup table (this avoids infinite
	// loop in the case of circular dependency)
	solver->inst_lut[pkgname_id] = NULL;

	// First remove recursively the reverse dependencies
	rdep_pkg = inst_rdeps_iter_first(&iter,
	                                 pkg,
	                                 solver->binindex,
	                                 solver->inst_lut);
	while (rdep_pkg) {
		solver_remove_pkgname(solver, rdep_pkg->name_id);
		rdep_pkg = inst_rdeps_iter_next(&iter);
	}

	// Add package removal to planned operation stack
	op.pkg = pkg;
	buffer_push(&solver->ops_stack, &op, sizeof(op));
}




/**
 * solver_create_action_stack() - Create a action stack from solution
 * @solver:     solver context to query
 *
 * Return: action stack corresponding to the solution previously found by
 * @solver.
 */
static
struct action_stack* solver_create_action_stack(struct solver* solver)
{
	struct action_stack* stk;
	struct planned_op* ops;
	struct binpkg * pkg, * oldpkg;
	int i, num_ops;

	stk = mmpack_action_stack_create();

	ops = solver->ops_stack.base;
	num_ops = solver->ops_stack.size / sizeof(*ops);

	for (i = 0; i < num_ops; i++) {
		switch (ops[i].action) {
		case STAGE:
			// ignore
			break;

		case INSTALL:
			pkg = ops[i].pkg;
			stk = mmpack_action_stack_push(stk, INSTALL_PKG,
			                               pkg, NULL);
			break;

		case REMOVE:
			pkg = ops[i].pkg;
			stk = mmpack_action_stack_push(stk, REMOVE_PKG,
			                               pkg, NULL);
			break;

		case UPGRADE:
			pkg = solver->inst_lut[ops[i].id];
			oldpkg =
				(struct binpkg*) ((char*) pkg -
				                  ops[i].pkg_diff);
			stk = mmpack_action_stack_push(stk, UPGRADE_PKG,
			                               pkg, oldpkg);
			break;

		default:
			mm_crash("Unexpected action type: %i", ops[i].action);
		}
	}

	return stk;
}


/**************************************************************************
 *                                                                        *
 *                         package installation requests                  *
 *                                                                        *
 **************************************************************************/

/**
 * compdeps_from_reqlist() - compile a dependency list from install request
 * @reqlist:    installation request list to convert
 * @binindex:   binary index to use to compiled the dependencies
 * @buff:       buffer to use to hold the compiled dependency list
 *
 * Return: pointer to the compiled dependency just created in case of
 * success. NULL if the package name in a request cannot be found.
 */
static
struct compiled_dep* compdeps_from_reqlist(const struct pkg_request* reqlist,
                                           const struct binindex* binindex,
                                           struct buffer* buff)
{
	STATIC_CONST_MMSTR(any_version, "any");
	struct pkgdep dep = {0};
	struct compiled_dep* compdep;
	const struct pkg_request* req;

	mm_check(reqlist != NULL);

	for (req = reqlist; req != NULL; req = req->next) {
		if (req->pkg != NULL) {
			compdep = compile_package(binindex, req->pkg, buff);
			continue;
		}

		dep.name = req->name;
		dep.min_version = req->version ? req->version : any_version;
		dep.max_version = dep.min_version;

		// Append to buff a new compiled dependency
		compdep = binindex_compile_dep(binindex, &dep, buff);
		if (!compdep) {
			error("Cannot find package: %s\n", req->name);
			return NULL;
		}

		if (compdep->num_pkg == 0) {
			error("Cannot find version %s of package %s\n",
			      req->version, req->name);
			return NULL;
		}
	}

	// mark last element as termination
	compdep->next_entry_delta = 0;
	return buff->base;
}


/**
 * mmpkg_get_install_list() - parse package deps and return install order
 * @ctx: the mmpack context
 * @reqlist: requested package list to be installed
 *
 * This function will initialize a dependency ordered list from the requested
 * packages. Then pass those to the core function solver_solve_deps() which
 * will return an ordered stack of action that are required to make the
 * requested changes happen.
 *
 * The action stack is ordered so that when a package is installed, all its
 * required dependencies are already met and installed.
 *
 * Returns: an action stack of the packages to be installed in the right order
 *          and at the correct version on success.
 *          NULL on error.
 */
LOCAL_SYMBOL
struct action_stack* mmpkg_get_install_list(struct mmpack_ctx * ctx,
                                            const struct pkg_request* reqlist)
{
	int rv;
	struct compiled_dep * deplist;
	struct compiled_dep * curr;
	struct solver solver;
	struct action_stack* stack = NULL;
	struct buffer deps_buffer;

	buffer_init(&deps_buffer);
	solver_init(&solver, ctx);

	// create initial dependency list from pkg_request list
	deplist = compdeps_from_reqlist(reqlist, &ctx->binindex, &deps_buffer);
	if (!deplist)
		goto exit;

	// fill the manually installed packages set
	for (curr = deplist; curr; curr = compiled_dep_next(curr))
		strset_add(&ctx->manually_inst, curr->pkgs[0]->name);

	rv = solver_solve_deps(&solver, deplist, 0);
	if (rv == 0)
		stack = solver_create_action_stack(&solver);

exit:
	solver_deinit(&solver);
	buffer_deinit(&deps_buffer);
	return stack;
}


/**************************************************************************
 *                                                                        *
 *                         package upgrades requests                      *
 *                                                                        *
 **************************************************************************/
static
struct compiled_dep* upgrades_from_reqlist(const struct pkg_request* reqlist,
                                           const struct solver* solver,
                                           struct buffer* buff)
{
	STATIC_CONST_MMSTR(any_version, "any");
	struct binindex* binindex = solver->binindex;
	struct compiled_dep* compdep = NULL;
	const struct binpkg* pkg;
	const struct pkg_request* req;
	int name_id;
	struct pkgdep dep = {.max_version = any_version};

	mm_check(reqlist != NULL);

	for (req = reqlist; req != NULL; req = req->next) {
		name_id = binindex_get_pkgname_id(binindex, req->name);
		pkg = solver->inst_lut[name_id];

		dep.name = req->name;
		dep.min_version = pkg->version;
		compdep = binindex_compile_dep(binindex, &dep, buff);
		if (!compdep) {
			error("Cannot find package: %s\n", req->name);
			return NULL;
		}
	}

	// mark last element as termination
	compdep->next_entry_delta = 0;

	return buff->base;
}


/**
 * mmpkg_get_upgrade_list() - get actions to upgrade specified packages
 * @ctx:         the mmpack context
 * @reqs:        requested package list to be upgraded (version fields are
 *               ignored)
 *
 * Returns: an action stack of the packages to be installed in the right order
 *          and at the correct version on success. NULL on error.
 */
LOCAL_SYMBOL
struct action_stack* mmpkg_get_upgrade_list(struct mmpack_ctx * ctx,
                                            const struct pkg_request* reqs)
{
	struct compiled_dep* deplist;
	struct solver solver;
	struct action_stack* stack = NULL;
	struct buffer buff;
	int rv;

	buffer_init(&buff);
	solver_init(&solver, ctx);

	deplist = upgrades_from_reqlist(reqs, &solver, &buff);
	if (!deplist)
		goto exit;

	rv = solver_solve_deps(&solver, deplist, DO_UPGRADE);
	if (rv == 0)
		stack = solver_create_action_stack(&solver);

exit:
	solver_deinit(&solver);
	buffer_deinit(&buff);
	return stack;
}


/**************************************************************************
 *                                                                        *
 *                         package removal requests                       *
 *                                                                        *
 **************************************************************************/

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
	struct action_stack * actions = NULL;
	const struct pkg_request* req;
	struct solver solver;
	int id;

	solver_init(&solver, ctx);

	for (req = reqlist; req; req = req->next) {
		id = binindex_get_pkgname_id(solver.binindex, req->name);
		solver_remove_pkgname(&solver, id);
	}

	actions = solver_create_action_stack(&solver);

	solver_deinit(&solver);
	return actions;
}


LOCAL_SYMBOL
int confirm_action_stack_if_needed(int nreq, struct action_stack const * stack)
{
	int i, rv, is_ghost;
	const char* operation;
	const mmstr * old_version, * new_version;

	if (stack->index == 0) {
		printf("Nothing to do.\n");
		return 0;
	}

	printf("Transaction summary:\n");

	for (i = 0; i < stack->index; i++) {
		if (stack->actions[i].action == UPGRADE_PKG) {
			new_version = stack->actions[i].pkg->version;
			old_version = stack->actions[i].oldpkg->version;
			if (pkg_version_compare(new_version, old_version) < 0)
				operation = "DOWNGRADE";
			else
				operation = "UPGRADE";

			printf("%s: %s (%s -> %s)\n", operation,
			       stack->actions[i].pkg->name,
			       old_version, new_version);
			continue;
		}

		if (stack->actions[i].action == INSTALL_PKG)
			printf("INSTALL: ");
		else if (stack->actions[i].action == REMOVE_PKG)
			printf("REMOVE: ");

		if (stack->actions[i].oldpkg) {
		} else {
			is_ghost = binpkg_is_ghost(stack->actions[i].pkg);
			printf("%s (%s)%s\n", stack->actions[i].pkg->name,
			       stack->actions[i].pkg->version,
			       is_ghost ? "*" : "");
		}
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
