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
	REPOSITORY,
};


static
int get_field_type(const char* name, int len)
{
	if (STR_EQUAL(name, len, "repository"))
		return REPOSITORY;
	else
		return UNKNOWN_FIELD;
}


static
int set_settings_field(struct settings* s, int field_type,
                       const char* data, int len)
{
	switch(field_type) {
	case REPOSITORY:
		s->repo_url = mmstr_copy_realloc(s->repo_url, data, len);
		break;
	default:
		// Unknown field are silently ignored
		break;
	}

	return 0;
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
			default:
				type = -1;
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
 * Return: 0 in case of sucess, -1 otherwise.
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
	*settings = (struct settings){0};
}


LOCAL_SYMBOL
void settings_deinit(struct settings* settings)
{
	mmstr_free(settings->repo_url);

	*settings = (struct settings){0};
}
