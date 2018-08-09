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


/*
 * # list of name: url
 * server1: http://mmpack-server-1:8888/
 * server2: http://mmpack-server-2:8888/
 */
static
int parse_config(yaml_parser_t * parser, server_cb cb, void * arg)
{
	yaml_token_t token;
	char * name;
	int rv, type;

	rv = -1;
	name = NULL;
	type = -1;
	while (1) {
		if (!yaml_parser_scan(parser, &token))
			goto exit;

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
				if (type == YAML_KEY_TOKEN) {
					name = strdup((char const *) token.data.scalar.value);
				} else if (type == YAML_VALUE_TOKEN) {
					rv = cb(name, (char const *) token.data.scalar.value, arg);
					if (rv != 0)
						goto exit;

					free(name);
					name = NULL;
				}
				break;
			default:
				type = -1;
				free(name);
				name = NULL;
				break;
		}

		yaml_token_delete(&token);
	}

exit:
	yaml_token_delete(&token);
	free(name);

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
