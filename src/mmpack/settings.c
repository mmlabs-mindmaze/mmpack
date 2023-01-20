/*
 * @mindmaze_header@
 */
#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include "settings.h"

#include <stdio.h>
#include <string.h>
#include <yaml.h>

#include <mmerrno.h>
#include <mmlib.h>
#include <mmsysio.h>

#include "mmstring.h"
#include "repo.h"
#include "utils.h"


enum {
	UNKNOWN_FIELD = -1,
	REPOSITORIES,
	DEFAULT_PREFIX,
	DISABLE_IMPORT_OTHER,
};


static
int get_field_type(const char* name, int len)
{
	if (STR_EQUAL(name, len, "repositories"))
		return REPOSITORIES;
	else if (STR_EQUAL(name, len, "default-prefix"))
		return DEFAULT_PREFIX;
	else if (STR_EQUAL(name, len, "disable-import-other-prefix"))
		return DISABLE_IMPORT_OTHER;
	else
		return UNKNOWN_FIELD;
}


static
int set_settings_field(struct settings* s, int field_type,
                       const char* data, int len)
{
	switch (field_type) {
	case DEFAULT_PREFIX:
		s->default_prefix = mmstr_copy_realloc(s->default_prefix,
		                                       data,
		                                       len);
		break;

	case DISABLE_IMPORT_OTHER:
		s->disable_import_other = atoi(data);
		break;

	default:
		// Unknown field are silently ignored
		break;
	}

	return 0;
}


enum field_type {
	FIELD_UNKNOWN = -1,
	FIELD_URL = 0,
	FIELD_ENABLED,
};


static
const char * scalar_field_names[] = {
	[FIELD_URL] = "url",
	[FIELD_ENABLED] = "enabled",
};


static
enum field_type get_scalar_field_type(const char* key)
{
	int i;

	for (i = 0; i < MM_NELEM(scalar_field_names); i++) {
		if (strcmp(key, scalar_field_names[i]) == 0)
			return i;
	}

	return FIELD_UNKNOWN;
}


static
int repo_set_scalar_field(struct repo* repo, enum field_type type,
                          const char * data)
{
	switch (type) {
	case FIELD_URL:
		repo->url = mmstr_malloc_from_cstr(data);
		break;
	case FIELD_ENABLED:
		repo->enabled = atoi(data);
		break;
	default:
		return -1;
	}

	return 0;
}


static
int parse_one_repo(yaml_parser_t * parser, struct repo* repo)
{
	int rv = -1;
	int type = -1;
	enum field_type scalar_field = FIELD_UNKNOWN;
	char const * data;
	size_t data_len;
	yaml_token_t token;

	while (1) {
		if (!yaml_parser_scan(parser, &token))
			goto exit;

		switch (token.type) {
		case YAML_BLOCK_END_TOKEN:
			goto exit;
		case YAML_KEY_TOKEN:
			type = YAML_KEY_TOKEN;
			break;
		case YAML_VALUE_TOKEN:
			type = YAML_VALUE_TOKEN;
			break;
		case YAML_SCALAR_TOKEN:
			data = (char const*) token.data.scalar.value;
			data_len = token.data.scalar.length;

			switch (type) {
			case YAML_KEY_TOKEN:
				scalar_field = get_scalar_field_type(data);
				type = -1;
				break;
			case YAML_VALUE_TOKEN:
				if (data_len) {
					if (repo_set_scalar_field(repo,
					                          scalar_field,
					                          data))
						goto error;
				} else {
					goto error;
				}

				break;
			default:
				mm_raise_error(MM_EBADFMT,
				               "yaml file not well-formed");
				goto error;
			}

			break;
		default:
			break;
		}

		yaml_token_delete(&token);
	}

exit:
	rv = 0;
error:
	yaml_token_delete(&token);
	return rv;
}


static
int fill_repositories(yaml_parser_t* parser, struct settings* settings)
{
	yaml_token_t token;
	struct repolist* repo_list = &settings->repo_list;
	int type = -1;
	int cpt = 0; // counter to know when the list of repositories ends
	int rv = -1;
	char * name = NULL;
	struct repo* repo;

	repolist_reset(repo_list);
	while (1) {
		if (!yaml_parser_scan(parser, &token))
			goto exit;

		switch (token.type) {
		case YAML_NO_TOKEN:
			goto error;
		case YAML_FLOW_SEQUENCE_END_TOKEN:
			goto exit;

		case YAML_STREAM_END_TOKEN:
			goto exit;

		case YAML_BLOCK_END_TOKEN:
			if (cpt == 0)
				goto exit;

			cpt--;

			break;

		case YAML_BLOCK_MAPPING_START_TOKEN:
			cpt++;
			break;

		case YAML_VALUE_TOKEN:
			type = YAML_VALUE_TOKEN;
			break;

		case YAML_KEY_TOKEN:
			type = YAML_KEY_TOKEN;
			break;

		case YAML_SCALAR_TOKEN:
			if (type == YAML_KEY_TOKEN) {
				name = xx_malloc(token.data.scalar.length + 1);
				memcpy(name, token.data.scalar.value,
				       token.data.scalar.length + 1);
				if (!(repo = repolist_add(repo_list, name))
				    || parse_one_repo(parser, repo))
					goto error;

				free(name);
				name = NULL;
			}

		default:
			// silently ignore error
			break;
		}

		yaml_token_delete(&token);
	}

exit:
	rv = 0;

error:
	yaml_token_delete(&token);
	free(name);
	return rv;
}


static
int parse_config(yaml_parser_t* parser, struct settings* settings)
{
	yaml_token_t token;
	const char* data;
	int rv, type, field_type, datalen;

	rv = -1;
	data = NULL;
	type = -1;
	field_type = UNKNOWN_FIELD;
	while (1) {
		if (!yaml_parser_scan(parser, &token)) {
			rv = 0;
			goto exit;
		}

		switch (token.type) {
		case YAML_NO_TOKEN:
			goto exit;

		case YAML_STREAM_END_TOKEN:
			rv = 0;
			goto exit;
		case YAML_KEY_TOKEN:
			type = YAML_KEY_TOKEN;
			break;
		case YAML_VALUE_TOKEN:
			type = YAML_VALUE_TOKEN;
			break;
		case YAML_FLOW_SEQUENCE_START_TOKEN:
		case YAML_BLOCK_SEQUENCE_START_TOKEN:
			if (type == YAML_VALUE_TOKEN
			    && field_type == REPOSITORIES) {
				if (fill_repositories(parser, settings)) {
					return -1;
				}
			}

			break;
		case YAML_SCALAR_TOKEN:
			data = (const char*)token.data.scalar.value;
			datalen = token.data.scalar.length;
			if (type == YAML_KEY_TOKEN) {
				field_type = get_field_type(data, datalen);
			} else if (type == YAML_VALUE_TOKEN) {
				if (set_settings_field(settings, field_type,
				                       data, datalen))
					goto exit;
			}

			break;
		default:         // ignore
			break;
		}

		yaml_token_delete(&token);
	}

exit:
	yaml_token_delete(&token);
	return rv;
}


/**
 * settings_load() - read config file and update settings
 * @settings:   initialized settings structure to update
 * @filename:   configuration file to open
 *
 * Read and parse specified configuration file and set or update fields of
 * @settings accordingly.
 *
 * If @filename does not exists, no update will be done and the function
 * will succeed. However, if @filename exists but is not readable, the
 * function will fail.
 *
 * Return: 0 in case of success, -1 otherwise.
 */
LOCAL_SYMBOL
int settings_load(struct settings* settings, const char* filename)
{
	int rv = -1;
	FILE* fh = NULL;
	yaml_parser_t parser;

	if (mm_check_access(filename, F_OK))
		return 0;

	fh = fopen(filename, "rb");
	if (fh == NULL) {
		mm_raise_from_errno("Cannot open %s", filename);
		return -1;
	}

	if (!yaml_parser_initialize(&parser)) {
		mm_raise_error(ENOMEM, "failed to init yaml parse");
		goto exit;
	}

	yaml_parser_set_input_file(&parser, fh);
	rv = parse_config(&parser, settings);
	yaml_parser_delete(&parser);

	if (rv != 0)
		fprintf(stderr, "Failed to parse configuration file\n");

exit:
	fclose(fh);
	return rv;
}


/**
 * settings_init() - init settings structure
 * @settings: the settings structure to initialize
 *
 * Should be cleansed by calling settings_deinit()
 */
LOCAL_SYMBOL
void settings_init(struct settings* settings)
{
	*settings = (struct settings) {
		.default_prefix = NULL,
		.disable_import_other = 0,
	};

	repolist_init(&settings->repo_list);
}


/**
 * settings_deinit() - cleanse settings structure
 * @settings: the settings structure to clean
 */
LOCAL_SYMBOL
void settings_deinit(struct settings* settings)
{
	repolist_deinit(&settings->repo_list);
	mmstr_free(settings->default_prefix);

	*settings = (struct settings) {0};
}


/**
 * settings_reset() - reset settings structure
 * @settings: the settings structure to clean
 */
LOCAL_SYMBOL
void settings_reset(struct settings* settings)
{
	settings_deinit(settings);
	settings_init(settings);
}


/**
 * dump_repo_elt_and_children() - write all subsequent repo elements
 * @fd:  file descriptor of the configuration file to write
 * @elt: current repo element (can be NULL)
 *
 * Write the current repo list element @elt and all subsequent element in
 * reverse order. This reverse order is necessary to preserve the order as
 * appearing in user global configuration since repo element are always
 * inserted to head when populating the list.
 */
static
void dump_repo_elt_and_children(int fd, struct repolist_elt* elt)
{
	struct repo* repo;
	int len;
	char* line;
	char linefmt[] = "  - %s:\n        url: %s\n        enabled: %d\n";

	if (elt == NULL)
		return;

	// Write children element before current
	dump_repo_elt_and_children(fd, elt->next);

	repo = &elt->repo;

	// Allocate buffer large enough
	len = sizeof(linefmt) + mmstrlen(repo->url)
	      + mmstrlen(repo->name) + 10;
	line = xx_malloca(len);

	len = sprintf(line, linefmt, repo->name, repo->url, repo->enabled);
	mm_write(fd, line, len);

	mm_freea(line);
}


LOCAL_SYMBOL
int settings_serialize(const mmstr* prefix,
                       const struct settings * settings,
                       int force_create)
{
	const struct repolist* repo_list = &settings->repo_list;
	const mmstr* cfg_relpath = mmstr_alloca_from_cstr(CFG_RELPATH);
	char line[128];
	int fd, oflag;

	oflag = O_WRONLY|O_CREAT|(force_create ? O_TRUNC : O_EXCL);
	fd = open_file_in_prefix(prefix, cfg_relpath, oflag);
	if (fd < 0)
		return -1;

	sprintf(line, "disable-import-other-prefix: %i\n",
	        settings->disable_import_other);
	mm_write(fd, line, strlen(line));

	// Write list of repositories to configuration file
	strcpy(line, "repositories:\n");
	mm_write(fd, line, strlen(line));
	dump_repo_elt_and_children(fd, repo_list->head);

	mm_close(fd);
	return 0;
}


static
int create_one_empty_index_file(const mmstr * prefix, const mmstr * index,
                                const char * name)
{
	int fd, oflag, len;
	mmstr * avllist_relpath;

	len = mmstrlen(index) + strlen(name) + 1;
	avllist_relpath = mmstr_malloc(len);
	sprintf(avllist_relpath, "%s.%s", index, name);
	mmstr_setlen(avllist_relpath, len);

	oflag = O_WRONLY|O_CREAT|O_TRUNC;

	// Create initial empty available package list
	fd = open_file_in_prefix(prefix, avllist_relpath, oflag);
	mm_close(fd);
	mmstr_free(avllist_relpath);
	if (fd < 0)
		return -1;

	return 0;
}


/**
 * create_empty_index_file() - create the index file corresponding to
 *                                repository name passed in argument
 * @prefix:   prefix
 * @name: name of the repository
 *
 * Return: 0 in case of success, -1 otherwise
 */
LOCAL_SYMBOL
int create_empty_index_files(const mmstr* prefix, char const * name)
{
	STATIC_CONST_MMSTR(repo_relpath, REPO_INDEX_RELPATH);
	STATIC_CONST_MMSTR(srcindex_relpath, SRC_INDEX_RELPATH);
	if (create_one_empty_index_file(prefix, repo_relpath, name)
	    || create_one_empty_index_file(prefix, srcindex_relpath, name))
		return -1;

	return 0;
}


/**
 * create_initial_index_files() - create the binindex files corresponding to
 *                                   repository names passed in the list
 * @prefix:   prefix
 * @name: list of repositories
 *
 * Return: 0 in case of success, -1 otherwise
 */
LOCAL_SYMBOL
int create_initial_index_files(const mmstr* prefix, struct repolist* repos)
{
	struct repo_iter iter;
	struct repo* r;

	for (r = repo_iter_first(&iter, repos); r; r = repo_iter_next(&iter)) {
		if (create_empty_index_files(prefix, r->name))
			return -1;
	}

	return 0;
}
