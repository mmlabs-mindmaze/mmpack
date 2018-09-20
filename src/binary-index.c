/*
 * @mindmaze_header@
 */
#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <yaml.h>

#include <mmerrno.h>
#include <mmlib.h>

#include "context.h"
#include "indextable.h"
#include "mm-alloc.h"
#include "mmstring.h"
#include "package-utils.h"

enum field_type {
	FIELD_UNKNOWN = -1,
	FIELD_VERSION = 0,
	FIELD_FILENAME,
	FIELD_SHA,
	FIELD_SIZE,
};

static
const char* scalar_field_names[] = {
	[FIELD_VERSION] = "version",
	[FIELD_FILENAME] = "filename",
	[FIELD_SHA] = "sha256",
	[FIELD_SIZE] = "size",
};


static
enum field_type get_scalar_field_type(const char* key)
{
	int i;

	for (i = 0; i < MM_NELEM(scalar_field_names); i++)
		if (strcmp(key, scalar_field_names[i]) == 0)
			return i;

	return FIELD_UNKNOWN;
}


static
int mmpkg_set_scalar_field(struct mmpkg * pkg, enum field_type type,
                           const char* value, size_t valuelen)
{
	const mmstr** field = NULL;

	switch(type) {
	case FIELD_VERSION:
		field = &pkg->version;
		break;

	case FIELD_FILENAME:
		field = &pkg->filename;
		break;

	case FIELD_SHA:
		field = &pkg->sha256;
		break;

	case FIELD_SIZE:
		pkg->size = atoi(value);
		return 0;

	default:
		return -1;
	}

	mmstr_free(*field);
	*field = mmstr_malloc_copy(value, valuelen);

	return 0;
}


static inline
void mmpkg_append_dependency_rec(struct mmpkg_dep * dep, struct mmpkg_dep * d)
{
	if (dep->next != NULL)
		return mmpkg_append_dependency_rec(dep->next, d);

	dep->next = d;
}

static
void mmpkg_insert_dependency(struct mmpkg * pkg, struct mmpkg_dep * dep,
                             int is_system_package)
{
	struct mmpkg_dep** deps;

	deps = is_system_package ? &pkg->sysdeps : &pkg->mpkdeps;
	if (*deps == NULL)
		*deps = dep;
	else
		mmpkg_append_dependency_rec(*deps, dep);
}

/* parse a single mmpack or system dependency
 * eg:
 *   pkg-b: [0.0.2, any]
 */
static
int mmpack_parse_dependency(yaml_parser_t* parser,
                            struct mmpkg * pkg,
                            struct mmpkg_dep * dep,
                            int is_system_package)
{
	int exitvalue;
	yaml_token_t token;

	exitvalue = -1;
	while (1) {
		if (!yaml_parser_scan(parser, &token))
			goto exit;

		switch(token.type) {
		case YAML_FLOW_SEQUENCE_END_TOKEN:
			if (dep->min_version != NULL && dep->max_version != NULL)
				exitvalue = 0;
			goto exit;

		case YAML_SCALAR_TOKEN:
			if (dep->min_version == NULL) {
				dep->min_version = mmstr_malloc_from_cstr((char const *) token.data.scalar.value);
			} else {
				if (dep->max_version != NULL) 
					goto exit;
				dep->max_version = mmstr_malloc_from_cstr((char const *) token.data.scalar.value);
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
		mmpkg_insert_dependency(pkg, dep, is_system_package);
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
int mmpack_parse_deplist(yaml_parser_t* parser,
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
		if (!yaml_parser_scan(parser, &token))
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
				exitvalue = mmpack_parse_dependency(parser, pkg, dep, is_system_package);
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
int mmpack_parse_index_package(yaml_parser_t* parser, struct mmpkg * pkg)
{
	int exitvalue, type;
	yaml_token_t token;
	char const * data;
	size_t data_len;
	enum field_type scalar_field;

	exitvalue = 0;
	data = NULL;
	type = -1;
	scalar_field = FIELD_UNKNOWN;
	do {
		if (!yaml_parser_scan(parser, &token))
			goto error;

		switch(token.type) {
		case YAML_BLOCK_END_TOKEN:
			if (!mmpkg_is_valid(pkg))
				goto error;
			goto exit;

		case YAML_KEY_TOKEN:
			type = YAML_KEY_TOKEN;
			break;

		case YAML_VALUE_TOKEN:
			type = YAML_VALUE_TOKEN;
			break;

		case YAML_SCALAR_TOKEN:
			data = (char const *) token.data.scalar.value;
			data_len = token.data.scalar.length;

			switch (type) {
			case YAML_KEY_TOKEN:
				if (STR_EQUAL(data, data_len, "depends")) {
					if (mmpack_parse_deplist(parser, pkg, 0) < 0)
						goto error;
				} else if (STR_EQUAL(data, data_len, "sysdepends")) {
					if (mmpack_parse_deplist(parser, pkg, 1) < 0)
						goto error;
				} else {
					scalar_field = get_scalar_field_type(data);
					type = -1;
				}
				break;

			case YAML_VALUE_TOKEN:
				if ((scalar_field != FIELD_UNKNOWN) && token.data.scalar.length) {
					if (mmpkg_set_scalar_field(pkg, scalar_field, data, data_len))
						goto error;
				}
			/* fallthrough */
			default:
				scalar_field = FIELD_UNKNOWN;
				data = NULL;
				type = -1;
				break;
			}
			break;

		default: /* ignore */
			break;
		}

		yaml_token_delete(&token);
	} while(token.type != YAML_STREAM_END_TOKEN);

error:
	exitvalue = -1;

exit:
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
int mmpack_parse_index(yaml_parser_t* parser, struct indextable * index,
                       struct indextable* installed_list)
{
	int exitvalue, type;
	yaml_token_t token;
	struct mmpkg * pkg;
	struct it_entry* entry;

	exitvalue = -1;
	type = -1;
	while (1) {
		if (!yaml_parser_scan(parser, &token))
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
				exitvalue = mmpack_parse_index_package(parser, pkg);
				if (exitvalue != 0) {
					mmpkg_destroy(pkg);
					goto exit;
				}

				/* insert into indextable */
				entry = indextable_lookup_create(index, pkg->name);
				assert(entry != NULL);

				/* prepend new version */
				pkg->next_version = entry->value;
				entry->value = pkg;

				/* Add to installed list if it is provided in argument for update */
				if (installed_list) {
					entry = indextable_lookup_create(installed_list, pkg->name);
					assert(entry != NULL);
					entry->value = pkg;
				}
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
int index_populate(char const * index_filename, struct indextable * binindex,
                   struct indextable * installed)
{
	int rv = -1;
	FILE * index_fh;
	yaml_parser_t parser;

	if (!yaml_parser_initialize(&parser))
		return mm_raise_error(ENOMEM, "failed to init yaml parse");

	index_fh = fopen(index_filename, "r");
	if (index_fh == NULL) {
		mm_raise_error(EINVAL, "failed to open given binary index file");
		goto exit;
	}

	yaml_parser_set_input_file(&parser, index_fh);
	rv = mmpack_parse_index(&parser, binindex, installed);

	fclose(index_fh);

exit:
	yaml_parser_delete(&parser);
	return rv;
}


LOCAL_SYMBOL
int binary_index_populate(struct mmpack_ctx * ctx, char const * index_filename)
{
	return index_populate(index_filename, &ctx->binindex, NULL);
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


/**
 * installed_index_populate() - parse index of installed package
 * @ctx:        mmpack context
 * @filename:   path to installed package index file
 *
 * This function will parse the index of index package and reference the
 * packages found there in the binary-index. Additionally it will fill the
 * indextable holding the list of installed package
 *
 * Return: 0 in case of success, -1 otherwise
 */
LOCAL_SYMBOL
int installed_index_populate(struct mmpack_ctx * ctx, char const * filename)
{
	return index_populate(filename, &ctx->binindex, &ctx->installed);
}
