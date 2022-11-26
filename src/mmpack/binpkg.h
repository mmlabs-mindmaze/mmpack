/*
 * @mindmaze_header@
 */
#ifndef BINPKG_H
#define BINPKG_H

#include "buffer.h"
#include "crypto.h"
#include "mmstring.h"
#include "repo.h"
#include "strchunk.h"
#include "strlist.h"


#define MMPKG_FLAGS_GHOST       (1 << 0)


struct pkgdep {
	mmstr const * name;
	mmstr const * min_version; /* inclusive */
	mmstr const * max_version; /* exclusive */

	struct pkgdep * next;
};

struct pkgdep* pkgdep_create(struct strchunk name);
void pkgdep_destroy(struct pkgdep * dep);


struct binpkg {
	int name_id;
	mmstr const * name;
	mmstr const * version;
	mmstr const * source;
	mmstr const * desc;
	digest_t sumsha;
	digest_t srcsha;

	struct remote_resource* remote_res;

	int flags;

	struct pkgdep* mpkdeps;
	struct strlist sysdeps;
	struct compiled_dep* compdep;
};



void binpkg_init(struct binpkg* pkg, const mmstr* name);
void binpkg_deinit(struct binpkg * pkg);
void binpkg_save_to_buffer(const struct binpkg* pkg, struct buffer* buff);
int binpkg_check_valid(struct binpkg const * pkg, int in_repo_cache);
void binpkg_clear_deps(struct binpkg* pkg);
void binpkg_clear_sysdeps(struct binpkg* pkg);
int binpkg_add_dep(struct binpkg* pkg, struct strchunk value);
void binpkg_add_sysdep(struct binpkg* pkg, struct strchunk value);
int binpkg_is_provided_by_repo(struct binpkg const * pkg,
                               const struct repo* repo);
struct remote_resource* binpkg_get_remote_res(struct binpkg* pkg,
                                              const struct repo* repo);
void binpkg_add_remote_resource(struct binpkg* pkg,
                                struct remote_resource* res_added);


static inline
void binpkg_update_flags(struct binpkg* pkg, int mask, int set)
{
	if (set)
		pkg->flags |= mask;
	else
		pkg->flags &= ~mask;
}


static inline
int binpkg_is_ghost(struct binpkg const * pkg)
{
	return pkg->flags & MMPKG_FLAGS_GHOST;
}


static inline
int binpkg_is_available(struct binpkg const * pkg)
{
	return pkg->remote_res != NULL;
}


#endif /* BINPKG_H */
