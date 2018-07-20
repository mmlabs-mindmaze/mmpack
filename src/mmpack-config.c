/*
 * @mindmaze_header@
 */
#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <yaml.h>

#include <mmerrno.h>
#include <mmlib.h>

#include "mmpack-config.h"

#define CONFIG_FILENAME "mmpack-config.yaml"


LOCAL_SYMBOL
char const * get_config_filename(void)
{
	char * filename;
	size_t filename_len;

	int xdg_env_set = 1;
	char * xdg_home = mm_getenv("XDG_CONFIG_HOME", NULL);
	if (xdg_home == NULL) {
		xdg_home = mm_getenv("HOME", NULL);
		xdg_env_set = 0;
	}

	if (xdg_home == NULL)
		return NULL;

	filename_len = strlen(xdg_home) + sizeof(CONFIG_FILENAME) + 1;
	if (!xdg_env_set)
		filename_len += sizeof("/.config/");
	filename = malloc(filename_len);
	if (filename == NULL)
		return NULL;

	filename[0] = '\0';
	strcat(filename, xdg_home);
	strcat(filename, "/");
	if (!xdg_env_set)
		strcat(filename, ".config/");
	strcat(filename, CONFIG_FILENAME);

	return filename;
}


/*
 * # list of name: url
 * server1: http://mmpack-server-1:8888/
 * server2: http://mmpack-server-2:8888/
 */
static
int parse_config(yaml_parser_t * parser, server_cb cb, void * arg)
{
	yaml_token_t token;
	char const * name;
	int rv, type;

	rv = -1;
	name = NULL;
	type = -1;
	do {
		if (!yaml_parser_scan(parser, &token))
			goto exit;

		switch(token.type) {
			case YAML_KEY_TOKEN:
				type = YAML_KEY_TOKEN;
				break;
			case YAML_VALUE_TOKEN:
				type = YAML_VALUE_TOKEN;
				break;
			case YAML_SCALAR_TOKEN:
				if (type == YAML_KEY_TOKEN) {
					name = (char const *) token.data.scalar.value;
				} else if (type == YAML_VALUE_TOKEN) {
					rv = cb(name, (char const *) token.data.scalar.value, arg);
					if (rv != 0)
						goto exit;
					name = NULL;
				}
				break;
			default:
				type = -1;
				name = NULL;
				break;
		}
	} while(token.type != YAML_STREAM_END_TOKEN);
	rv = 0;

exit:
	yaml_token_delete(&token);

	return rv;
}


LOCAL_SYMBOL
int foreach_config_server(char const * filename, server_cb cb, void * arg)
{
	int rv;
	FILE * fh;
	yaml_parser_t parser;

	if (filename == NULL) {
		return mm_raise_error(EINVAL, "invalid config filename");
	}

	fh = fopen(filename, "r");
	if(fh == NULL) {
		return mm_raise_from_errno("failed to open config file");
	}

	if (!yaml_parser_initialize(&parser)) {
		fclose(fh);
		return mm_raise_error(ENOMEM, "failed to parse config file");
	}
	yaml_parser_set_input_file(&parser, fh);

	rv = parse_config(&parser, cb, arg);

	yaml_parser_delete(&parser);
	fclose(fh);

	return rv;
}
