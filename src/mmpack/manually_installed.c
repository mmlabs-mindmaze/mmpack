/*
 * @mindmaze_header@
 */

#include "manually_installed.h"
	
static
int parse_manually_installed(yaml_parser_t * parser, struct strset * set)
{
	yaml_token_t token;
	const char * value;
	int rv = -1;

	while (1) {
		if (!yaml_parser_scan(parser, &token))
			goto exit;

		switch (token.type) {
			case YAML_NO_TOKEN:
				goto exit;
			
			case YAML_BLOCK_END_TOKEN:
			case YAML_STREAM_END_TOKEN:
				rv = 0;
				goto exit;

			case YAML_SCALAR_TOKEN:
				value = (const char *) token.data.scalar.value;
				strset_add(set, value);
			default:
				break;
		}
		yaml_token_delete(&token);
	}

exit:
	yaml_token_delete(&token);

	return rv;	
}


static
int manually_installed_populate(struct strset * manually_inst, mmstr * filename)
{
	int rv = -1;
	FILE * file;
	yaml_parser_t parser;

	if (!yaml_parser_initialize(&parser))
		return mm_raise_error(ENOMEM, "failed to init yaml parsing");

	file = fopen(filename, "r");
	if (file == NULL) {
		mm_raise_error(EINVAL,
		               "failed to open given manually installed file");
		goto exit;
	}

	yaml_parser_set_input_file(&parser, file);
	rv = parse_manually_installed(&parser, manually_inst);

	fclose(file);

exit:
	yaml_parser_delete(&parser);	
}


/**
 * load_manually_installed() - loads the name list of the manually installed
 *                             packages
 * @manually_installed: set of names of the manually installed packages
 *
 * Return: 0 in case of success, -1 otherwise
 */
LOCAL_SYMBOL
int load_manually_installed(struct strset * manually_installed)
{
	STATIC_CONST_MMSTR(manually_inst_relpath, MANUALLY_INST_RELPATH);
	mmstr * filename;
	int rv, len;

	len = mmstrlen(ctx->prefix) + mmstrlen(manually_inst_relpath) + 1;
	filename = mmstr_malloca(len);

	mmstr_join_path(filename, ctx->prefix, manually_inst_relpath);
	rv = manually_installed_populate(&ctx->manually_inst, filename);

	mmstr_freea(filename);
	return rv;
}

