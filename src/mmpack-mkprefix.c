/*
 * @mindmaze_header@
 */

#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include <mmerrno.h>
#include <mmsysio.h>

#include "context.h"
#include "mmstring.h"
#include "utils.h"

#include "mmpack-mkprefix.h"



static
int create_file_in_prefix(const mmstr* prefix, const mmstr* relpath)
{
	int fd;
	mmstr *path, *dirpath;

	path = mmstr_alloca(mmstrlen(prefix) + mmstrlen(relpath));
	dirpath = mmstr_alloca(mmstrlen(prefix) + mmstrlen(relpath));

	// Form path and dir of target file in prefix
	mmstrcpy(path, prefix);
	mmstrcat(path, relpath);
	mmstr_dirname(dirpath, path);

	// Create parent dir if not existing yet
	if (mm_mkdir(dirpath, 0777, MM_RECURSIVE))
		return -1;

	// Create file if not existing yet
	fd = mm_open(path, O_WRONLY|O_CREAT|O_EXCL, 0666);
	if (fd < 0) {
		fprintf(stderr, "Failed to create %s: %s\n",
		                path, mmstrerror(mm_get_lasterror_number()));
		return -1;
	}

	return fd;
}


static
int create_initial_empty_files(const mmstr* prefix)
{
	int fd;
	const mmstr *instlist_relpath, *repocache_relpath;

	instlist_relpath = mmstr_alloca_from_cstr(INSTALLED_INDEX_RELPATH);
	repocache_relpath = mmstr_alloca_from_cstr(REPO_INDEX_RELPATH);

	// Create initial empty installed package list
	fd = create_file_in_prefix(prefix, instlist_relpath);
	mm_close(fd);
	if (fd < 0)
		return -1;

	// Create initial empty cache repo package list
	fd = create_file_in_prefix(prefix, repocache_relpath);
	mm_close(fd);
	if (fd < 0)
		return -1;

	return 0;
}


static
int create_initial_prefix_cfg(const mmstr* prefix, const char* url)
{
	const mmstr* cfg_relpath = mmstr_alloca_from_cstr(CFG_RELPATH);
	char line[256];
	int fd, len;

	fd = create_file_in_prefix(prefix, cfg_relpath);
	if (fd < 0)
		return -1;

	len = sprintf(line, "remote: %s", url);
	mm_write(fd, line, len);

	mm_close(fd);
	return 0;
}


LOCAL_SYMBOL
int mmpack_mkprefix(struct mmpack_ctx * ctx, int argc, const char* argv[])
{
	const mmstr* prefix = ctx->prefix;
	const char* url;

	if (argc < 2) {
		fprintf(stderr, "Missing argument for mkprefix command\n");
		return -1;
	}
	url = argv[1];

	if (  create_initial_empty_files(prefix)
	   || create_initial_prefix_cfg(prefix, url) )
		return -1;

	return 0;
}
