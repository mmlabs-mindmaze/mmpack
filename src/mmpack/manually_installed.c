/*
 * @mindmaze_header@
 */
#if defined (HAVE_CONFIG_H)
# include <config.h>
#endif

#include <mmerrno.h>
#include <mmlib.h>
#include <mmsysio.h>
#include <yaml.h>

#include "manually_installed.h"


/**
 * suppress_from_manually_installed() - check if the name given is in the set
 *                                      and suppress it from the set
 * @manually_inst: name set of manually installed packages
 * @name: name of the package to remove from the manually installed set
 *
 * Return: 0 in case of success, -1 otherwise
 */
LOCAL_SYMBOL
int suppress_from_if_in_manually_installed(struct strset * manually_inst,
                                           const mmstr * name)
{
	if (strset_contains(manually_inst, name))
		return strset_remove(manually_inst, name);

	// if the key is not present in the set, we assume that the suppression
	// has been performed correctly
	return 0;
}

/**
 * complete_manually_installed() - add manually installed package names to
 *                                 the manually_inst set
 * @manually_inst: name set of manually installed packages
 * @reqlist: list of packages manually installed that MUST be added to the
 *           @manually_inst set
 *
 * Return: 0 in case of success, -1 otherwise
 */
LOCAL_SYMBOL
int complete_manually_installed(struct strset * manually_inst,
                                struct pkg_request * reqlist)
{
	struct pkg_request * curr;
	const mmstr * name;

	for (curr = reqlist; curr; curr = curr->next) {
		name = curr->pkg ? curr->pkg->name : curr->name;
		if (strset_add(manually_inst, name)) {
			info("Impossible to complete manually_inst set\n");
			return -1;
		}
	}

	return 0;
}


static
int parse_manually_installed(yaml_parser_t * parser,
                             struct strset * manually_inst)
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
			value = (const char*) token.data.scalar.value;
			strset_add(manually_inst, value);
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
int manually_installed_populate(struct strset * manually_inst,
                                const char * filename)
{
	int rv = -1;
	FILE * file;
	yaml_parser_t parser;

	if (!yaml_parser_initialize(&parser))
		return mm_raise_error(ENOMEM, "failed to init yaml parsing");

	file = fopen(filename, "rb");
	if (file == NULL) {
		mm_raise_error(EINVAL,
		               "failed to open manually installed file");
		goto exit;
	}

	yaml_parser_set_input_file(&parser, file);
	rv = parse_manually_installed(&parser, manually_inst);

	fclose(file);

exit:
	yaml_parser_delete(&parser);
	return rv;
}


/**
 * load_manually_installed() - loads the name list of the manually installed
 *                             packages
 * @prefix: prefix
 * @manually_inst: name set of manually installed packages
 *
 * The file from which the names are read is
 * prefix/var/lib/mmpack/manually_installed.yaml
 *
 * Return: 0 in case of success, -1 otherwise
 */
LOCAL_SYMBOL
int load_manually_installed(const mmstr * prefix, struct strset * manually_inst)
{
	STATIC_CONST_MMSTR(manually_inst_relpath, MANUALLY_INST_RELPATH);
	mmstr * filename;
	int rv, len;

	len = mmstrlen(prefix) + mmstrlen(manually_inst_relpath) + 1;
	filename = mmstr_malloca(len);

	mmstr_join_path(filename, prefix, manually_inst_relpath);

	rv = manually_installed_populate(manually_inst, filename);

	mmstr_freea(filename);
	return rv;
}


/**
 * dump_manually_installed() - dump the name list of the manually installed
 *                             packages
 * @prefix: prefix
 * @manually_inst: name set of manually installed packages
 *
 * The file where the names are dumped is
 * prefix/var/lib/mmpack/manually_installed.yaml
 *
 * Return: 0 in case of success, -1 otherwise
 */
LOCAL_SYMBOL
int dump_manually_installed(const mmstr * prefix, struct strset * manually_inst)
{
	struct strset_iterator iter;
	mmstr * curr;
	mmstr * filename;
	FILE * file;
	int len;

	STATIC_CONST_MMSTR(manually_inst_relpath, MANUALLY_INST_RELPATH);

	len = mmstrlen(prefix) + mmstrlen(manually_inst_relpath) + 1;
	filename = mmstr_malloca(len);

	mmstr_join_path(filename, prefix, manually_inst_relpath);

	if (!(file = fopen(filename, "wb")))
		return -1;

	for (curr = strset_iter_first(&iter, manually_inst); curr;
	     curr = strset_iter_next(&iter)) {
		fprintf(file, "- %s\n", curr);
	}

	fclose(file);
	mmstr_freea(filename);
	return 0;
}
