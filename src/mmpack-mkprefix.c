/*
 * @mindmaze_header@
 */

#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include <mmsysio.h>

#include "context.h"
#include "mmstring.h"
#include "utils.h"

#include "mmpack-mkprefix.h"


static
int create_file_in_prefix(const mmstr* prefix, const char* relpath)
{
	int fd;
	mmstr *path, *dirpath;

	path = mmstr_alloca(mmstrlen(prefix) + 64);
	dirpath = mmstr_alloca(mmstrlen(prefix) + 64);

	// Form path and dir of installed package list file
	mmstrcpy(path, prefix);
	mmstrcat_cstr(path, relpath);
	mmstr_dirname(dirpath, path);

	// Create parent dir if not existing yet
	if (mm_mkdir(dirpath, 0777, MM_RECURSIVE))
		return -1;

	// Create file if not existing yet
	fd = mm_open(path, O_WRONLY|O_CREAT, 0666);
	if (fd < 0)
		return -1;

	mm_close(fd);
	return 0;
}


LOCAL_SYMBOL
int mmpack_mkprefix(struct mmpack_ctx * ctx)
{
	const mmstr* prefix = ctx->prefix;

	if (  create_file_in_prefix(prefix, INSTALLED_INDEX_RELPATH)
	   || create_file_in_prefix(prefix, REPO_INDEX_RELPATH))
		return -1;

	return 0;
}
