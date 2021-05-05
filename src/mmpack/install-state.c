/*
 * @mindmaze_header@
 */
#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include <string.h>

#include "binindex.h"
#include "binpkg.h"
#include "buffer.h"
#include "indextable.h"
#include "install-state.h"
#include "mmstring.h"


LOCAL_SYMBOL
int install_state_init(struct install_state* state)
{
	state->pkg_num = 0;
	return indextable_init(&state->idx, -1, -1);
}


LOCAL_SYMBOL
int install_state_copy(struct install_state* restrict dst,
                       const struct install_state* restrict src)
{
	return indextable_copy(&dst->idx, &src->idx);
}


LOCAL_SYMBOL
void install_state_deinit(struct install_state* state)
{
	indextable_deinit(&state->idx);
}


/**
 * install_state_get_pkg() - query the package installed under a name
 * @state:      install state to query
 * @name:       package name to query
 *
 * Return: a pointer to the struct binpkg installed if found, NULL if no package
 * with @name is in the install state.
 */
LOCAL_SYMBOL
const struct binpkg* install_state_get_pkg(const struct install_state* state,
                                           const mmstr* name)
{
	struct it_entry* entry;

	entry = indextable_lookup(&state->idx, name);
	if (entry == NULL)
		return NULL;

	return entry->value;
}


/**
 * install_state_add_pkg() - add or replace package
 * @state:      install state to modify
 * @pkg:        package to add to install state
 */
LOCAL_SYMBOL
void install_state_add_pkg(struct install_state* state,
                           const struct binpkg* pkg)
{
	struct it_entry* entry;

	entry = indextable_lookup_create(&state->idx, pkg->name);
	entry->value = (void*)pkg;
	state->pkg_num++;
}


/**
 * install_state_rm_pkgname() - remove package from the install state
 * @state:      install state to modify
 * @pkgname:    name of package to remove from @state
 */
LOCAL_SYMBOL
void install_state_rm_pkgname(struct install_state* state,
                              const mmstr* pkgname)
{
	if (!indextable_remove(&state->idx, pkgname))
		state->pkg_num--;
}


/**
 * install_state_save_to_buffer() - write state's pkgs in keyval to buffer
 * @state:      install state to save
 * @buffer:     buffer to which the data of packages must be written
 */
LOCAL_SYMBOL
void install_state_save_to_buffer(const struct install_state* state,
                                  struct buffer* buff)
{
	struct it_iterator iter;
	struct it_entry* entry;
	const struct binpkg* pkg;

	entry = it_iter_first(&iter, &state->idx);
	while (entry) {
		pkg = entry->value;
		binpkg_save_to_buffer(pkg, buff);
		entry = it_iter_next(&iter);

		if (entry)
			buffer_push(buff, "\n", 1);
	}
}


/**
 * install_state_fill_lookup_table() - fill an table of installed package
 * @state:      install state to dump
 * @binindex:   binary index associated with install state (used for package
 *              name to id resolution)
 * @installed:  lookup table of length binindex->num_pkgname
 *
 * This function will fill a lookup table @installed whose each element
 * correspond to the package installed for a package name id if one is
 * installed, NULL if not.
 */
LOCAL_SYMBOL
void install_state_fill_lookup_table(const struct install_state* state,
                                     struct binindex* binindex,
                                     struct binpkg** installed)
{
	struct it_iterator iter;
	struct it_entry* entry;
	struct binpkg* pkg;
	int id;

	// Set initially to all uninstalled
	memset(installed, 0, binindex->num_pkgname * sizeof(*installed));

	// Loop over the indextable... For each package package installed
	// update the element in @installed table corresponding to its
	// package id
	entry = it_iter_first(&iter, &state->idx);
	while (entry) {
		pkg = entry->value;
		id = binindex_get_pkgname_id(binindex, pkg->name);
		installed[id] = pkg;
		entry = it_iter_next(&iter);
	}
}


