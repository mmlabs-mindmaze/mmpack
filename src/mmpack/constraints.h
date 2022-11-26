/*
 * @mindmaze_header@
 */
#ifndef CONSTRAINTS_H
#define CONSTRAINTS_H

#include <string.h>

#include "crypto.h"
#include "mmstring.h"
#include "repo.h"

/**
 * strcut constraints - structure containing all the possible constraints
 *                      imposed by the user in the command line.
 * @version: package version
 * @repo_name: the repository in which the package should be searched
 * @sumsha: package sumsha
 **/
struct constraints {
	mmstr * version;
	const struct repo* repo;
	digest_t* sumsha;
};


/**
 * constraints_deinit  -  deinit a structure struct constraints
 * @c: the structure to deinitialize
 */
static inline
void constraints_deinit(struct constraints * c)
{
	mmstr_free(c->version);
	c->version = NULL;
	free(c->sumsha);
	c->sumsha = NULL;
}


/**
 * constraints_is_empty() - indicates whether a struct constraints is empty or
 *                          not
 * @c: the structure to test
 *
 * Return: 1 if @c is empty, 0 otherwise.
 */
static inline
int constraints_is_empty(struct constraints * c)
{
	struct constraints empty = {0};
	return memcmp(&empty, c, sizeof(empty)) == 0;
}

#endif /* CONSTRAINTS_H */
