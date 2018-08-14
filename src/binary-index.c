/*
 * @mindmaze_header@
 */
#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <yaml.h>

#include <mmerrno.h>
#include <mmlib.h>

#include "indextable.h"
#include "mmstring.h"
#include "mmpack-context.h"
#include "package-utils.h"


static inline
void mmpkg_append_dependency_rec(struct mmpkg_dep * dep, struct mmpkg_dep * d)
{
	if (dep->next != NULL)
		return mmpkg_append_dependency_rec(dep->next, d);

	dep->next = d;
}

static
void mmpkg_insert_dependency(struct mmpkg * pkg, struct mmpkg_dep * dep)
{
	if (pkg->dependencies == NULL)
		pkg->dependencies = dep;
	else
		mmpkg_append_dependency_rec(pkg->dependencies, dep);
}

/* parse a single mmpack or system dependency
 * eg:
 *   pkg-b: [0.0.2, any]
 */
static
int mmpack_parse_dependency(struct mmpack_ctx * ctx,
                            struct mmpkg * pkg,
                            struct mmpkg_dep * dep,
                            int is_system_package)
{
	int exitvalue;
	yaml_token_t token;

	exitvalue = -1;
	while (1) {
		if (!yaml_parser_scan(&ctx->parser, &token))
			goto exit;

		switch(token.type) {
		case YAML_FLOW_SEQUENCE_END_TOKEN:
			if (dep->min_version != NULL && dep->max_version != NULL)
				exitvalue = 0;
			goto exit;

		case YAML_SCALAR_TOKEN:
			if (dep->min_version == NULL) {
				dep->min_version = mmstr_malloc_from_cstr((char const *) token.data.scalar.value);
				if (dep->min_version == NULL)
					goto exit;
			} else {
				if (dep->max_version != NULL) 
					goto exit;
				dep->max_version = mmstr_malloc_from_cstr((char const *) token.data.scalar.value);
				if (dep->max_version == NULL)
					goto exit;
			}
			break;

		default: /* ignore */
			break;
		}

		yaml_token_delete(&token);
	};

exit:
	yaml_token_delete(&token);

	if (exitvalue == 0) {
		dep->is_system_package = is_system_package;
		mmpkg_insert_dependency(pkg, dep);
	}

	return exitvalue;
}


/*
 * parse a list of mmpack or system dependency
 * eg:
 *   depends:
 *     pkg-b: [0.0.2, any]
 *     pkg-d: [0.0.4, 0.0.4]
 *     ...
 */
static
int mmpack_parse_deplist(struct mmpack_ctx * ctx,
                         struct mmpkg * pkg,
                         int is_system_package)
{
	int exitvalue, type;
	yaml_token_t token;
	struct mmpkg_dep * dep;

	exitvalue = 0;
	dep = NULL;
	type = -1;
	while (1) {
		if (!yaml_parser_scan(&ctx->parser, &token))
			goto exit;

		switch(token.type) {
		case YAML_STREAM_END_TOKEN:
			goto exit;

		case YAML_FLOW_MAPPING_END_TOKEN:
		case YAML_BLOCK_END_TOKEN:
			goto exit;

		case YAML_KEY_TOKEN:
			type = YAML_KEY_TOKEN;
			break;

		case YAML_VALUE_TOKEN:
			type = YAML_VALUE_TOKEN;
			break;

		case YAML_FLOW_SEQUENCE_START_TOKEN:
				if (dep == NULL)
					goto exit;
				exitvalue = mmpack_parse_dependency(ctx, pkg, dep, is_system_package);
				if (exitvalue != 0)
					goto exit;
				dep = NULL;
				type = -1;

				break;

		case YAML_SCALAR_TOKEN:
			switch (type) {
			case YAML_KEY_TOKEN:
				if (dep != NULL)
					goto exit;
				dep = mmpkg_dep_create((char const *)token.data.scalar.value);
				if (dep == NULL)
					goto exit;
				break;

			default:
				mmpkg_dep_destroy(dep);
				dep = NULL;
				type = -1;
				break;
			}
		break;

		default: /* ignore */
			break;
		}

		yaml_token_delete(&token);
	};

	/* reach end of file prematurely */
	exitvalue = -1;

exit:
	yaml_token_delete(&token);
	if (exitvalue != 0)
		mmpkg_dep_destroy(dep);

	return exitvalue;
}


/* parse a single package entry */
static
int mmpack_parse_index_package(struct mmpack_ctx * ctx,
                               struct mmpkg * pkg)
{
	int exitvalue, type;
	yaml_token_t token;
	char const * key;
	int flag_key;

	exitvalue = 0;
	key = NULL;
	flag_key = type = -1;
	do {
		if (!yaml_parser_scan(&ctx->parser, &token))
			goto error;

		switch(token.type) {
		case YAML_BLOCK_END_TOKEN:
			goto package_ok;

		case YAML_KEY_TOKEN:
			type = YAML_KEY_TOKEN;
			break;

		case YAML_VALUE_TOKEN:
			type = YAML_VALUE_TOKEN;
			break;

		case YAML_SCALAR_TOKEN:
			switch (type) {
			case YAML_KEY_TOKEN:
				key = (char const *) token.data.scalar.value;
				size_t key_len = token.data.scalar.length;
				if (STR_EQUAL(key, key_len, "depends")) {
					if (mmpack_parse_deplist(ctx, pkg, 0) < 0)
						goto error;
				} else if (STR_EQUAL(key, key_len, "sysdepends")) {
					if (mmpack_parse_deplist(ctx, pkg, 1) < 0)
						goto error;
				} else if (STR_EQUAL(key, key_len, "version")) {
					flag_key = 1;
					type = -1;
				}
				break;

			case YAML_VALUE_TOKEN:
				if (flag_key && token.data.scalar.length) {
					if (pkg->version != NULL)
						goto error;
					pkg->version = mmstr_malloc_from_cstr((char const *) token.data.scalar.value);
					if (pkg->version == NULL)
						goto error;
				}
			/* fallthrough */
			default:
				flag_key = 0;
				key = NULL;
				type = -1;
				break;
			}
			break;

		default: /* ignore */
			break;
		}

		yaml_token_delete(&token);
	} while(token.type != YAML_STREAM_END_TOKEN);

package_ok:

error:
	yaml_token_delete(&token);

	return exitvalue;
}


/*
 * Entry point to parse a whole binary index file
 *
 * pkg-x:
 *   depends:
 *     pkg-y
 *   sysdepends:
 *     sys-x
 *   version: 0.1.2
 * pkg-y:
 *   ...
 */
static
int mmpack_parse_index(struct mmpack_ctx * ctx, struct indextable * index)
{
	int exitvalue, type;
	yaml_token_t token;
	struct mmpkg * pkg;
	struct it_entry* entry;

	exitvalue = -1;
	type = -1;
	while (1) {
		if (!yaml_parser_scan(&ctx->parser, &token))
			goto exit;

		switch(token.type) {
		case YAML_STREAM_END_TOKEN:
			exitvalue = 0;
			goto exit;

		case YAML_KEY_TOKEN:
			type = YAML_KEY_TOKEN;
			break;

		case YAML_SCALAR_TOKEN:
			if (type == YAML_KEY_TOKEN) {
				pkg = mmpkg_create((char const *) token.data.scalar.value);
				if (pkg == NULL)
					goto exit;
				exitvalue = mmpack_parse_index_package(ctx, pkg);
				if (exitvalue != 0)
					goto exit;

				/* insert into indextable */
				entry = indextable_lookup_create(index, pkg->name);
				assert(entry != NULL);

				/* prepend new version */
				pkg->next_version = entry->value;
				entry->value = pkg;
			}
			type = -1;
			break;

		default: /* ignore */
			break;
		}

		yaml_token_delete(&token);
	};
	exitvalue = 0;

exit:
	yaml_token_delete(&token);

	return exitvalue;
}


static
int index_populate(struct mmpack_ctx * ctx, char const * index_filename,
                   struct indextable * index)
{
	int rv;
	FILE * index_fh;

	assert(mmpack_ctx_is_init(ctx));

	index_fh = fopen(index_filename, "r");
	if (index_fh == NULL)
		return mm_raise_error(EINVAL, "failed to open given binary index file");

	yaml_parser_set_input_file(&ctx->parser, index_fh);
	rv = mmpack_parse_index(ctx, index);

	fclose(index_fh);
	return rv;
}


LOCAL_SYMBOL
int binary_index_populate(struct mmpack_ctx * ctx, char const * index_filename)
{
	return index_populate(ctx, index_filename, &ctx->binindex);
}


LOCAL_SYMBOL
void binary_index_dump(struct indextable const * binindex)
{

	struct it_iterator iter;
	struct it_entry * entry;

	entry = it_iter_first(&iter, binindex);
	while (entry != NULL) {
		mmpkg_dump(entry->value);
		entry = it_iter_next(&iter);
	}
}


LOCAL_SYMBOL
void installed_index_dump(struct indextable const * installed)
{
	return binary_index_dump(installed);
}


LOCAL_SYMBOL
int installed_index_populate(struct mmpack_ctx * ctx, char const * index_filename)
{
	return index_populate(ctx, index_filename, &ctx->installed);
}
