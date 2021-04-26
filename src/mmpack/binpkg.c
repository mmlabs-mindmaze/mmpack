/*
 * @mindmaze_header@
 */
#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include <mmerrno.h>

#include "binpkg.h"


static
void write_yaml_mmstr_multiline(FILE* fp, int num_indent, const char* str)
{
	const char * line, * next;
	int linelen;

	fprintf(fp, "|\n");

	line = str;
	while (line != NULL) {
		next = strchr(line, '\n');
		if (next) {
			linelen = next - line;
			next++;
		} else {
			linelen = strlen(line);
		}

		fprintf(fp, "%*s%.*s\n", num_indent, " ", linelen, line);
		line = next;
	}
}


/**************************************************************************
 *                                                                        *
 *                       package dependencies methods                     *
 *                                                                        *
 **************************************************************************/
/**
 * pkgdep_create() - create pkgdep
 * @name: name of the mmpack package dependency
 *
 * To be destroyed by calling pkgdep_destroy()
 *
 * Return: an initialized pkgdep structure
 */
LOCAL_SYMBOL
struct pkgdep* pkgdep_create(char const * name)
{
	struct pkgdep * dep = xx_malloc(sizeof(*dep));
	memset(dep, 0, sizeof(*dep));
	dep->name = mmstr_malloc_from_cstr(name);
	return dep;
}


/**
 * pkgdep_destroy() - destroy pkgdep structure
 * @dep: the pkgdep structure to destroy
 */
LOCAL_SYMBOL
void pkgdep_destroy(struct pkgdep * dep)
{
	if (dep == NULL)
		return;

	mmstr_free(dep->name);
	mmstr_free(dep->min_version);
	mmstr_free(dep->max_version);

	if (dep->next)
		pkgdep_destroy(dep->next);

	free(dep);
}


static
void pkgdep_save_to_index(struct pkgdep const * dep, FILE* fp, int lvl)
{
	if (!dep) {
		fprintf(fp, " {}\n");
		return;
	}

	fprintf(fp, "\n");
	while (dep) {
		// Print name , minver and maxver at lvl indentation level
		// (ie 4*lvl spaces are inserted before)
		fprintf(fp, "%*s%s: [%s, %s]\n", lvl*4, " ",
		        dep->name, dep->min_version, dep->max_version);
		dep = dep->next;
	}
}


/**************************************************************************
 *                                                                        *
 *                          binary package methods                        *
 *                                                                        *
 **************************************************************************/
LOCAL_SYMBOL
void binpkg_init(struct binpkg* pkg, const mmstr* name)
{
	*pkg = (struct binpkg) {.name = name};
	strlist_init(&pkg->sysdeps);
}


LOCAL_SYMBOL
void binpkg_deinit(struct binpkg * pkg)
{
	mmstr_free(pkg->version);
	mmstr_free(pkg->source);
	mmstr_free(pkg->desc);
	mmstr_free(pkg->sumsha);
	mmstr_free(pkg->srcsha);

	remote_resource_destroy(pkg->remote_res);
	pkg->remote_res = NULL;
	pkgdep_destroy(pkg->mpkdeps);
	strlist_deinit(&pkg->sysdeps);
	free(pkg->compdep);

	binpkg_init(pkg, NULL);
}


/**
 * binpkg_is_provided_by_repo() - indicates whether a package is provided by a
 *                              repository or not.
 * @pkg: package searched in the repository
 * @repo: repository in which we search the package
 *
 * Return: 1 if @pkg is provided by @repo or if no repository is given in
 * parameter, 0 otherwise.
 */
LOCAL_SYMBOL
int binpkg_is_provided_by_repo(struct binpkg const * pkg,
                               const struct repo* repo)
{
	if (!repo)
		return 1;

	return remote_resource_from_repo(pkg->remote_res, repo) != NULL;
}


LOCAL_SYMBOL
void binpkg_save_to_index(struct binpkg const * pkg, FILE* fp)
{
	const struct strlist_elt* elt;

	fprintf(fp, "%s:\n"
	        "    version: %s\n"
	        "    source: %s\n"
	        "    srcsha256: %s\n"
	        "    sumsha256sums: %s\n"
	        "    ghost: %s\n",
	        pkg->name, pkg->version, pkg->source, pkg->srcsha, pkg->sumsha,
	        binpkg_is_ghost(pkg) ? "true" : "false");


	fprintf(fp, "    depends:");
	pkgdep_save_to_index(pkg->mpkdeps, fp, 2 /*indentation level*/);

	fprintf(fp, "    sysdepends: [");
	for (elt = pkg->sysdeps.head; elt != NULL; elt = elt->next) {
		fprintf(fp, "'%s'%s", elt->str.buf, elt->next ? ", " : "");
	}

	fprintf(fp, "]\n");

	fprintf(fp, "    description: ");
	write_yaml_mmstr_multiline(fp, 8, pkg->desc);
}


/**
 * binpkg_get_or_create_remote_res() - returns or create and returns a remote
 * @pkg:       a package already registered
 * @repo:      a repository from which the package pkg is provided
 *
 * This function looks if the repository is already informed in the list of the
 * repositories that provide the package. If this is the case, then the function
 * returns the repository, otherwise it creates a from_repo pointing to the
 * appropriate repository.
 *
 * A remote_resource will be added to a package only if there is a filename, a
 * sha256, or a size read in the yaml file describing the package, otherwise
 * the package is created with a remote_resource set to NULL.
 */
LOCAL_SYMBOL
struct remote_resource* binpkg_get_or_create_remote_res(struct binpkg* pkg,
                                                        const struct repo* repo)
{
	struct remote_resource* res;

	// check that the repository of the package is not already known
	res = remote_resource_from_repo(pkg->remote_res, repo);
	if (res)
		return res;

	// add the new remote resource to the list
	res = remote_resource_create(repo);
	res->next = pkg->remote_res;
	pkg->remote_res = res;

	return res;
}


/**
 * binpkg_add_remote_resource() - add remote resource providing the package
 * @pkg:        package already registered
 * @res_added:  list of remote resource through which the package @pkg_in is
 *              provided
 *
 * Be careful when using this function: the function takes ownership on the
 * fields filename and sha256 of the argument list.
 */
LOCAL_SYMBOL
void binpkg_add_remote_resource(struct binpkg* pkg,
                                struct remote_resource* res_added)
{
	struct remote_resource * src, * dst, * next;
	for (src = res_added; src != NULL; src = src->next) {
		dst = binpkg_get_or_create_remote_res(pkg, src->repo);
		mmstr_free(dst->filename);
		mmstr_free(dst->sha256);

		// copy from src while preserving the original chaining
		next = dst->next;
		*dst = *src;
		dst->next = next;

		// the field of src have been taken over by dst, hence we
		// need to reset them so that src is freed properly
		*src = (struct remote_resource) {.next = src->next};
	}
}


static
int from_repo_check_valid(struct binpkg const * pkg)
{
	struct remote_resource* elt;

	for (elt = pkg->remote_res; elt != NULL; elt = elt->next) {
		if (!elt->sha256 || !elt->size || !elt->filename)
			return mm_raise_error(EINVAL,
			                      "Invalid package data for %s."
			                      " Missing fields needed in"
			                      " repository package index.",
			                      pkg->name);
	}

	return 0;
}


LOCAL_SYMBOL
int binpkg_check_valid(struct binpkg const * pkg, int in_repo_cache)
{
	if (!pkg->version
	    || !pkg->sumsha
	    || !pkg->source)
		return mm_raise_error(EINVAL, "Invalid package data for %s."
		                      " Missing fields.", pkg->name);

	if (!in_repo_cache)
		return 0;

	return from_repo_check_valid(pkg);
}
