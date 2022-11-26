/*
 * @mindmaze_header@
 */
#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include <mmerrno.h>

#include "binpkg.h"
#include "buffer.h"
#include "strchunk.h"
#include "utils.h"


static
void push_dep_elt(struct buffer* buff, const mmstr* name,
                  const char* op, const mmstr* version)
{
	buffer_push(buff, name, mmstrlen(name));
	buffer_push(buff, " (", 2);
	buffer_push(buff, op, strlen(op));
	buffer_push(buff, version, mmstrlen(version));
	buffer_push(buff, ")", 1);
}


static
void write_keysha(struct buffer* buff, const char* key,
                  const digest_t* digest)
{
	buffer_push(buff, key, strlen(key));
	buffer_push(buff, ": ", 2);
	hexstr_from_digest(buffer_reserve_data(buff, SHA_HEXLEN), digest);
	buffer_inc_size(buff, SHA_HEXLEN);
	buffer_push(buff, "\n", 1);
}


static
void write_keyval(struct buffer* buff, const char* key, const mmstr* value)
{
	buffer_push(buff, key, strlen(key));
	buffer_push(buff, ": ", 2);
	buffer_push(buff, value, mmstrlen(value));
	buffer_push(buff, "\n", 1);
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
struct pkgdep* pkgdep_create(struct strchunk name)
{
	struct pkgdep * dep = xx_malloc(sizeof(*dep));
	memset(dep, 0, sizeof(*dep));
	dep->name = mmstr_malloc_copy(name.buf, name.len);
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
void pkgdep_write_element(const struct pkgdep* dep, struct buffer* buff)
{
	const mmstr* any = mmstr_alloca_from_cstr("any");
	bool is_minver_any = mmstrequal(dep->min_version, any);
	bool is_maxver_any = mmstrequal(dep->max_version, any);

	if (mmstrequal(dep->min_version, dep->max_version)) {
		if (is_minver_any)
			buffer_push(buff, dep->name, mmstrlen(dep->name));
		else
			push_dep_elt(buff, dep->name, "= ", dep->min_version);

		return;
	}

	if (!is_minver_any) {
		push_dep_elt(buff, dep->name, ">= ", dep->min_version);
		if (!is_maxver_any)
			buffer_push(buff, ", ", 2);
	}

	if (!is_maxver_any)
		push_dep_elt(buff, dep->name, "< ", dep->max_version);
}


static
void pkgdep_save_to_keyval(const struct pkgdep* dep, struct buffer* buff)
{
	const char key[] = "depends:\n ";

	if (!dep)
		return;

	buffer_push(buff, key, strlen(key));

	while (dep) {
		pkgdep_write_element(dep, buff);
		dep = dep->next;
		if (dep)
			buffer_push(buff, ",\n ", 3);
	}

	buffer_push(buff, "\n", 1);
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


static
void binpkg_sysdeps_to_keyval(const struct binpkg* pkg, struct buffer* buff)
{
	const struct strlist_elt* elt;
	const char sysdeps_field[] = "sysdepends:\n ";
	const mmstr* sysdep;

	if (!pkg->sysdeps.head)
		return;

	buffer_push(buff, sysdeps_field, strlen(sysdeps_field));

	for (elt = pkg->sysdeps.head; elt != NULL; elt = elt->next) {
		sysdep = elt->str.buf;
		buffer_push(buff, sysdep, mmstrlen(sysdep));

		if (elt->next)
			buffer_push(buff, ",\n ", 3);
	}

	buffer_push(buff, "\n", 1);
}


static
void binpkg_desc_to_keyval(const struct binpkg* pkg, struct buffer* buff)
{
	const char desc_field[] = "description:\n ";
	struct strchunk desc = {.buf = pkg->desc, .len = mmstrlen(pkg->desc)};
	mmstr* wrapped;

	buffer_push(buff, desc_field, strlen(desc_field));

	wrapped = mmstr_malloc(128);
	wrapped = textwrap_string(wrapped, desc, 80, " ", "\n .");
	buffer_push(buff, wrapped, mmstrlen(wrapped));
	mmstr_free(wrapped);

	buffer_push(buff, "\n", 1);
}


LOCAL_SYMBOL
void binpkg_save_to_buffer(const struct binpkg* pkg, struct buffer* buff)
{
	const char* ghost_val = binpkg_is_ghost(pkg) ? "true" : "false";

	write_keyval(buff, "name", pkg->name);
	write_keyval(buff, "version", pkg->version);
	write_keyval(buff, "source", pkg->source);
	write_keysha(buff, "srcsha256", &pkg->srcsha);
	write_keysha(buff, "sumsha256sums", &pkg->sumsha);
	write_keyval(buff, "ghost", mmstr_alloca_from_cstr(ghost_val));

	pkgdep_save_to_keyval(pkg->mpkdeps, buff);
	binpkg_sysdeps_to_keyval(pkg, buff);
	binpkg_desc_to_keyval(pkg, buff);
}


/**
 * binpkg_get_remote_res() - returns or create and returns a remote
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
struct remote_resource* binpkg_get_remote_res(struct binpkg* pkg,
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
		dst = binpkg_get_remote_res(pkg, src->repo);
		mmstr_free(dst->filename);

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
		if (!elt->size || !elt->filename)
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
	    || !pkg->source)
		return mm_raise_error(EINVAL, "Invalid package data for %s."
		                      " Missing fields.", pkg->name);

	if (!in_repo_cache)
		return 0;

	return from_repo_check_valid(pkg);
}


/**
 * binpkg_clear_deps() - empty dependencies previously set
 * @pkg:        binpkg whose dependencies must be removed
 */
LOCAL_SYMBOL
void binpkg_clear_deps(struct binpkg* pkg)
{
	pkgdep_destroy(pkg->mpkdeps);
	pkg->mpkdeps = NULL;
}


/**
 * binpkg_clear_sysdeps() - empty system dependencies previously set
 * @pkg:        binpkg whose system dependencies must be removed
 */
LOCAL_SYMBOL
void binpkg_clear_sysdeps(struct binpkg* pkg)
{
	strlist_deinit(&pkg->sysdeps);
	strlist_init(&pkg->sysdeps);
}


/**
 * binpkg_add_dep() - add a dependency specification string
 * @pkg:        binpkg to which the dependency must be added
 * @value:      dependency specification
 *
 * The dependency string must specified as:
 *   <name> [(<op> <version>)]
 * where <op> must be >=, = or <
 *
 * Returns: 0 in case of success, -1 otherwise
 */
LOCAL_SYMBOL
int binpkg_add_dep(struct binpkg* pkg, struct strchunk value)
{
	struct pkgdep* dep;
	struct strchunk name, op, version, cons, minver, maxver;
	int pos;

	minver = maxver = (struct strchunk) {.buf = "any", .len = 3};

	pos = strchunk_find(value, '(');

	name = strchunk_strip(strchunk_lpart(value, pos));

	cons = strchunk_rpart(value, pos);
	cons = strchunk_lpart(cons, strchunk_find(cons, ')'));
	if (cons.len) {
		cons = strchunk_strip(cons);
		op = strchunk_extract(cons, "=<>");
		version = strchunk_lstrip(strchunk_rpart(cons, op.len-1));
		if (strchunk_equal(op, ">=")) {
			minver = version;
		} else if (strchunk_equal(op, "=")) {
			minver = maxver = version;
		} else if (strchunk_equal(op, "<")) {
			maxver = version;
		} else {
			mm_raise_error(EINVAL, "invalid dep value: %.*s",
			               value.len, value.buf);
			return -1;
		}
	}

	dep = pkgdep_create(name);
	dep->min_version = mmstr_malloc_copy(minver.buf, minver.len);
	dep->max_version = mmstr_malloc_copy(maxver.buf, maxver.len);

	dep->next = pkg->mpkdeps;
	pkg->mpkdeps = dep;

	return 0;
}


LOCAL_SYMBOL
void binpkg_add_sysdep(struct binpkg* pkg, struct strchunk value)
{
	strlist_add_strchunk(&pkg->sysdeps, value);
}
