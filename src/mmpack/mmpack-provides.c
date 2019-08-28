/*
 * @mindmaze_header@
 */
#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include "mmpack-provides.h"

#include <mmargparse.h>
#include <mmerrno.h>
#include <mmlib.h>
#include <mmsysio.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "context.h"
#include "download.h"
#include "package-utils.h"

static int update_db = 0;
static char provides_doc[] =
	"\"mmpack provides\" uses the repository file/package database "
	"to help knowing which package provides given file or command.";

static const struct mmarg_opt cmdline_optv[] = {
	{"u|update", MMOPT_NOVAL|MMOPT_INT, "1", {.iptr = &update_db},
	 "update local database of file/package correspondence"},
};


STATIC_CONST_MMSTR(file_db, "/var/lib/mmpack/file-db.yaml");
STATIC_CONST_MMSTR(file_db_tmp, "/var/lib/mmpack/.file-db.yaml.tmp");

static
int download_file_db(struct mmpack_ctx * ctx, int repo_index)
{
	STATIC_CONST_MMSTR(file_db_name, "mmpack-file-db.yaml");
	mmstr const * url = settings_get_repo_url(&ctx->settings, repo_index);

	if (download_from_repo(ctx, url, file_db_name, ctx->prefix,
	                       file_db_tmp)) {
		error("Failed to download mmpack file/package DB from %s\n",
		      url);
		return -1;
	}

	info("Updated mmpack file/package DB from repository: %s\n", url);
	return 0;
}


static
int file_db_index_save(struct mmpack_ctx * ctx,
                       struct indextable const * indextable)
{
	struct it_iterator iter;
	struct it_entry* entry;
	FILE * file;
	mmstr * path;

	path = mmstr_malloca(mmstrlen(ctx->prefix) + mmstrlen(file_db) + 1);
	mmstr_join_path(path, ctx->prefix, file_db);
	file = fopen(path, "wb");
	if (file == NULL)
		return -1;

	entry = it_iter_first(&iter, indextable);
	if (entry == NULL)
		goto exit;

	fprintf(file, "%s:%s\n", entry->key, (char const*) entry->value);
	while ((entry = it_iter_next(&iter)) != NULL)
		fprintf(file, "%s:%s\n", entry->key,
		        (char const*) entry->value);

exit:
	mmstr_freea(path);
	fclose(file);
	return 0;
}


static
int file_db_update(struct mmpack_ctx * ctx, struct indextable * indextable,
                   mmstr const * filename)
{
	char const * delim;
	mmstr * key;
	mmstr * path;
	int rv = -1;
	char line[1 << 12]; /* 4KB */
	size_t len;
	FILE * file;

	path = mmstr_malloca(mmstrlen(ctx->prefix) + mmstrlen(filename) + 1);
	mmstr_join_path(path, ctx->prefix, filename);
	file = fopen(path, "rb");
	mmstr_freea(path);
	if (file == NULL)
		return -1;

	while (fgets(line, sizeof(line), file)) {
		len = strlen(line);
		if (line[len - 1] == '\n') /* remove trailing \n */
			len = strlen(line) - 1;

		delim = strchr(line, ':');
		if (delim == NULL)
			continue; /* should not happen */

		key = mmstr_malloc_copy(line, delim - line);

		if (indextable_lookup(indextable, key) == NULL) {
			mmstr_free(key);
		} else {
			struct it_entry * entry;
			entry = indextable_insert(indextable, key);
			if (entry == NULL) {
				error("Internal error: ...\n");
				mmstr_free(key);
				goto exit;
			}

			entry->key = key;
			entry->value =
				mmstr_malloc_copy(delim + 1,
				                  len - (delim - line) - 2);
		}
	}

	rv = 0;

exit:
	fclose(file);
	return rv;
}



static
void search_providing_pkg(struct indextable const * indextable,
                          char const * pattern)
{
	struct it_iterator iter;
	struct it_entry* entry;

	entry = it_iter_first(&iter, indextable);
	if (entry == NULL)
		return;

	if (strstr(entry->key, pattern) != NULL)
		printf("%s: %s\n", (char const*) entry->value, entry->key);

	while ((entry = it_iter_next(&iter)) != NULL) {
		if (strstr(entry->key, pattern) != NULL)
			printf("%s: %s\n",
			       (char const*) entry->value,
			       entry->key);
	}
}


static
void file_db_destroy(struct indextable * indextable)
{
	struct it_iterator iter;
	struct it_entry* entry;

	entry = it_iter_first(&iter, indextable);
	if (entry == NULL)
		return;

	mmstr_free(entry->key);
	mmstr_free(entry->value);
	while ((entry = it_iter_next(&iter)) != NULL) {
		mmstr_free(entry->key);
		mmstr_free(entry->value);
	}

	indextable_deinit(indextable);
}


/**
 * mmpack_provides() - main function for the provides command
 * @ctx: mmpack context
 * @argc: number of arguments
 * @argv: array of arguments
 *
 * Searches for possible packages providing a file matching given string
 *
 * Return: 0 on success, -1 otherwise
 */
LOCAL_SYMBOL
int mmpack_provides(struct mmpack_ctx * ctx, int argc, char const ** argv)
{
	int nreq, arg_index;
	int i, num_repo;
	int rv = -1;
	const char** req_args;
	struct indextable file_db_index;
	struct mmarg_parser parser = {
		.flags = mmarg_is_completing() ? MMARG_PARSER_COMPLETION : 0,
		.doc = provides_doc,
		.args_doc = PROVIDES_SYNOPSIS,
		.optv = cmdline_optv,
		.num_opt = MM_NELEM(cmdline_optv),
		.execname = "mmpack",
	};

	arg_index = mmarg_parse(&parser, argc, (char**)argv);
	if (mmarg_is_completing())
		return 0;

	nreq = argc - arg_index;
	req_args = argv + arg_index;

	if (nreq > 1 || (!update_db && nreq != 1)) {
		fprintf(stderr, "only one pattern ca be requested at a time\n"
		        "Run \"mmpack provides --help\" to see usage\n");
		return -1;
	}

	if (!update_db && (arg_index + 1) > argc) {
		fprintf(stderr,
		        "missing package list argument in command line\n"
		        "Run \"mmpack provides --help\" to see usage\n");
		return -1;
	}

	/* Load prefix configuration and caches */
	if (mmpack_ctx_use_prefix(ctx, 0))
		return -1;

	num_repo = settings_num_repo(&ctx->settings);
	if (num_repo == 0) {
		error("Repository URL unspecified\n");
		return -1;
	}

	if (indextable_init(&file_db_index, -1, -1) != 0) {
		error("Internal error: initialization failure.\n");
		return -1;
	}

	/* download/update mmpack-file-db.yaml if needed */
	if (update_db) {
		for (i = 0; i < num_repo; i++) {
			if (download_file_db(ctx, i))
				goto exit;

			// update file-db from intermediary file -> indextable
			file_db_update(ctx, &file_db_index, file_db_tmp);
			mm_unlink(file_db_tmp);
		}

		/* save resulting indextable */
		if (file_db_index_save(ctx, &file_db_index) != 0) {
			error("Internal error: failed to save index-db.\n");
			goto exit;
		}
	} else {
		/* load db directly from the local mmpack-file-db.yaml */
		file_db_update(ctx, &file_db_index, file_db);
	}

	/* look for requested pattern within indextable keys */
	search_providing_pkg(&file_db_index, *req_args);

	rv = 0;

exit:
	file_db_destroy(&file_db_index);
	return rv;
}
