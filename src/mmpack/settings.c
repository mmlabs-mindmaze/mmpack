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
	switch(field_type) {
	case DEFAULT_PREFIX:
		s->default_prefix = mmstr_copy_realloc(s->default_prefix, data, len);
		break;

	default:
		// Unknown field are silently ignored
		break;
	}

	return 0;
}


static
void fill_repositories(yaml_parser_t* parser, struct settings* settings)
{
	yaml_token_t token;
	const char* data;
	struct strlist repo_list;

	strlist_init(&repo_list);
	while (1) {
		if (!yaml_parser_scan(parser, &token))
			goto exit;

		switch(token.type) {
		case YAML_FLOW_SEQUENCE_END_TOKEN:
		case YAML_BLOCK_END_TOKEN:
		case YAML_KEY_TOKEN:
			goto exit;

		case YAML_SCALAR_TOKEN:
			data = (const char*)token.data.scalar.value;
			strlist_add(&repo_list, data);
			break;

		default:
			// silently ignore error
			break;
		}
		yaml_token_delete(&token);
	}

exit:
	yaml_token_delete(&token);

	// Replace repo list in settings
	strlist_deinit(&settings->repo_list);
	settings->repo_list = repo_list;
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

		switch(token.type) {
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
				if (  type == YAML_VALUE_TOKEN
				   && field_type == REPOSITORIES) {
					fill_repositories(parser, settings);
				}
				break;
			case YAML_SCALAR_TOKEN:
				data = (const char*)token.data.scalar.value;
				datalen = token.data.scalar.length;
				if (type == YAML_KEY_TOKEN) {
					field_type = get_field_type(data, datalen);
				} else if (type == YAML_VALUE_TOKEN) {
					if (set_settings_field(settings, field_type, data, datalen))
						goto exit;
				}
				break;
			default: // ignore
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


LOCAL_SYMBOL
void settings_init(struct settings* settings)
{
	*settings = (struct settings) {
		.default_prefix = get_default_mmpack_prefix(),
	};

	strlist_init(&settings->repo_list);
}


LOCAL_SYMBOL
void settings_deinit(struct settings* settings)
{
	strlist_deinit(&settings->repo_list);
	mmstr_free(settings->default_prefix);

	*settings = (struct settings){0};
}


LOCAL_SYMBOL
int settings_num_repo(const struct settings* settings)
{
	struct strlist_elt* elt;
	int num;

	num = 0;
	for (elt = settings->repo_list.head; elt; elt = elt->next)
		num++;

	return num;
}


LOCAL_SYMBOL
const mmstr* settings_get_repo_url(const struct settings* settings, int index)
{
	struct strlist_elt* elt;
	const mmstr* url = NULL;
	int i;

	elt = settings->repo_list.head;
	for (i = 0; i <= index; i++) {
		if (!elt)
			return NULL;

		url = elt->str.buf;
		elt = elt->next;
	}

	return url;
}
