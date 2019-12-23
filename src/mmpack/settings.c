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
#include "utils.h"


enum {
	UNKNOWN_FIELD = -1,
	REPOSITORIES,
	DEFAULT_PREFIX,
};


/**
 * repolist_init() - init repolist structure
 * @list: repolist structure to initialize
 *
 * To be cleansed by calling repolist_deinit()
 */
LOCAL_SYMBOL
void repolist_init(struct repolist* list)
{
	*list = (struct repolist) {0};
}


/**
 * repolist_deinit() - cleanup repolist structure
 * @list: repolist structure to cleanse
 */
LOCAL_SYMBOL
void repolist_deinit(struct repolist* list)
{
	struct repolist_elt * elt, * next;

	elt = list->head;

	while (elt) {
		next = elt->next;
		mmstr_free(elt->url);
		mmstr_free(elt->name);
		free(elt);
		elt = next;
	}
}


/**
 * repolist_reset() - empty and reinit repolist structure
 * @list: repolist structure to reset
 */
LOCAL_SYMBOL
void repolist_reset(struct repolist* list)
{
	repolist_deinit(list);
	repolist_init(list);
}


/**
 * repolist_num_repo() - count the number of repositories in the repo list
 * @list: initialized repolist structure
 *
 * Returns: the number of repositories
 */
static
int repolist_num_repo(const struct repolist* list)
{
	struct repolist_elt* elt;
	int num;

	num = 0;
	for (elt = list->head; elt; elt = elt->next)
		num++;

	return num;
}


/**
 * repolist_add() - add a repository to the list
 * @list: initialized repolist structure
 * @name: the short name referencing the url
 *
 * Returns: the repolist_elt created in case of success, NULL otherwise.
 */
LOCAL_SYMBOL
struct repolist_elt* repolist_add(struct repolist* list, const char* name)
{
	struct repolist_elt* elt;
	char default_name[16];

	// set index-based default name if name unset
	if (name == NULL) {
		sprintf(default_name, "repo-%i", repolist_num_repo(list));
		name = default_name;
	}

	// check that no repository possesses already this name
	if (repolist_lookup(list, name)) {
		error("repository \"%s\" already exists\n", name);
		return NULL;
	}

	// Insert the element at the head of the list
	elt = xx_malloc(sizeof(*elt));
	*elt = (struct repolist_elt) {
		.name = mmstr_malloc_from_cstr(name),
		.next = list->head,
		.enabled = 1,
	};
	list->head = elt;
	return elt;
}


/**
 * repolist_lookup() - lookup a repository from the list by name
 * @list: pointer to an initialized repolist structure
 * @name: the short name referencing the url
 *
 * Return: a pointer to the repolist element on success, NULL otherwise.
 */
LOCAL_SYMBOL
struct repolist_elt* repolist_lookup(struct repolist * list,
                                     const char * name)
{
	int name_len = strlen(name);
	struct repolist_elt * elt;

	for (elt = list->head; elt != NULL; elt = elt->next) {
		if (name_len == mmstrlen(elt->name)
		    && strncmp(elt->name, name, name_len) == 0) {
			return elt;
		}
	}

	return NULL;
}


/**
 * repolist_remove() - remove a repository to the list
 * @list: pointer to an initialized repolist structure
 * @name: the short name referencing the url
 *
 * Return: always return 0 on success, a negative value otherwise
 */
LOCAL_SYMBOL
int repolist_remove(struct repolist * list, const char * name)
{
	int name_len = strlen(name);
	struct repolist_elt * elt;
	struct repolist_elt * prev = NULL;

	for (elt = list->head; elt != NULL; elt = elt->next) {
		if (name_len == mmstrlen(elt->name)
		    && strncmp(elt->name, name, name_len) == 0) {

			if (prev != NULL)
				prev->next = elt->next;
			else
				list->head = elt->next;

			mmstr_free(elt->url);
			mmstr_free(elt->name);
			free(elt);
			return 0;
		}

		prev = elt;
	}

	return -1;
}


static
int get_field_type(const char* name, int len)
{
	if (STR_EQUAL(name, len, "repositories"))
		return REPOSITORIES;
	else if (STR_EQUAL(name, len, "default-prefix"))
		return DEFAULT_PREFIX;
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
int repo_set_scalar_field(struct repolist_elt * repo, enum field_type type,
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
int parse_one_repo(yaml_parser_t * parser, struct repolist_elt * repo)
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
	struct repolist_elt * repo;

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
		.default_prefix = get_default_mmpack_prefix(),
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
 * settings_num_repo() - count the number of repositories in the configuration
 * @settings: an initialized settings structure
 *
 * Returns: the number of repositories
 */
LOCAL_SYMBOL
int settings_num_repo(const struct settings* settings)
{
	return repolist_num_repo(&settings->repo_list);
}


/**
 * settings_get_repo_url() - pick one url from the settings
 * @settings: an initialized settings structure
 * @index: index of the url to get
 *
 * Return: a pointer to a mmstr structure describing the url on success
 *         NULL otherwise
 */
LOCAL_SYMBOL
const mmstr* settings_get_repo_url(const struct settings* settings, int index)
{
	struct repolist_elt* repo = settings_get_repo(settings, index);

	return repo->url;
}


/**
 * settings_get_repo() - pick one repository from the settings
 * @settings: an initialized settings structure
 * @index: index of the repository to get
 *
 * Return: a pointer to a struct repolist_elt describing the repository on
 * success NULL otherwise
 */
LOCAL_SYMBOL
struct repolist_elt* settings_get_repo(const struct settings* settings,
                                       int index)
{
	struct repolist_elt* elt = settings->repo_list.head;
	int i;

	for (i = 0; i < index; i++) {
		if (!elt)
			return NULL;

		elt = elt->next;
	}

	return elt;
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
	int len;
	char* line;
	char linefmt[] = "  - %s:\n        url: %s\n        enabled: %d\n";

	if (elt == NULL)
		return;

	// Write children element before current
	dump_repo_elt_and_children(fd, elt->next);

	// Allocate buffer large enough
	len = sizeof(linefmt) + mmstrlen(elt->url) + mmstrlen(elt->name) + 10;
	line = xx_malloca(len);

	len = sprintf(line, linefmt, elt->name, elt->url, elt->enabled);
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
	char repo_hdr_line[] = "repositories:\n";
	int fd, oflag;

	oflag = O_WRONLY|O_CREAT|(force_create ? O_TRUNC : O_EXCL);
	fd = open_file_in_prefix(prefix, cfg_relpath, oflag);
	if (fd < 0)
		return -1;

	// Write list of repositories to configuration file
	mm_write(fd, repo_hdr_line, sizeof(repo_hdr_line)-1);
	dump_repo_elt_and_children(fd, repo_list->head);

	mm_close(fd);
	return 0;
}


/**
 * create_empty_binindex_file() - create the binindex file corresponding to
 *                                repository name passed in argument
 * @prefix:   prefix
 * @name: name of the repository
 *
 * Return: 0 in case of success, -1 otherwise
 */
LOCAL_SYMBOL
int create_empty_binindex_file(const mmstr* prefix, char const * name)
{
	int fd, oflag, len;
	mmstr * avllist_relpath;

	STATIC_CONST_MMSTR(repo_relpath, REPO_INDEX_RELPATH);
	len = mmstrlen(repo_relpath) + strlen(name) + 1;
	avllist_relpath = mmstr_malloc(len);
	sprintf(avllist_relpath, "%s.%s", repo_relpath, name);
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
 * create_initial_binindex_files() - create the binindex files corresponding to
 *                                   repository names passed in the list
 * @prefix:   prefix
 * @name: list of repositories
 *
 * Return: 0 in case of success, -1 otherwise
 */
LOCAL_SYMBOL
int create_initial_binindex_files(const mmstr* prefix, struct repolist* repos)
{
	struct repolist_elt * r;

	for (r = repos->head; r != NULL; r = r->next) {
		if (create_empty_binindex_file(prefix, r->name))
			return -1;
	}

	return 0;
}
