/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2026 Liav A
 */

#include <stdio.h>
#include <stdlib.h>
#include <yaml.h>

#include "config/common.h"
#include "config/file_parser.h"
#include "config/return_codes.h"
#include "config/section.h"

static yaml_return_code_t parse_root_document(
    yaml_document_t *doc, yaml_node_t *node, struct yaml_mtd_scheme *scheme)
{
	yaml_return_code_t ret;
	yaml_node_pair_t *pair;
	yaml_node_t *key;
	yaml_node_t *value;

	if (node->type != YAML_MAPPING_NODE) {
		YAML_RC_SET(ret, YAML_RC_NODE_IS_NOT_MAPPING);
		goto exit;
	}

	for (pair = node->data.mapping.pairs.start;
	    pair < node->data.mapping.pairs.top; pair++) {
		key = yaml_document_get_node(doc, pair->key);
		value = yaml_document_get_node(doc, pair->value);
		if (!key || !value) {
			YAML_RC_SET(ret, YAML_RC_INVALID_OBJECT_NODE);
			goto exit;
		}

		ret = parse_section(doc, scheme, key, value);
		if (!YAML_RC_CHECK_SUCCESS(ret))
			goto exit;
	}

	YAML_RC_SET_SUCCESS(ret);
exit:

	return ret;
}

yaml_return_code_t parse_yaml_file(FILE *fp, struct yaml_mtd_scheme *scheme)
{
	yaml_return_code_t ret;

	yaml_parser_t parser;
	yaml_document_t document;

	YAML_RC_SET_SUCCESS(ret);

	if (!yaml_parser_initialize(&parser)) {
		fprintf(stderr, "Failed to initialize parser\n");
		YAML_RC_SET(ret, YAML_RC_PARSER_INITIALIZE_FAILED);
		goto exit;
	}

	yaml_parser_set_input_file(&parser, fp);

	if (!yaml_parser_load(&parser, &document)) {
		YAML_RC_SET(ret, YAML_RC_PARSER_LOAD_FAILED);
		goto exit;
	}

	yaml_node_t *root = yaml_document_get_root_node(&document);

	if (root) {
		ret = parse_root_document(&document, root, scheme);
	} else {
		YAML_RC_SET(ret, YAML_RC_NO_ROOT_NODE);
	}

	yaml_document_delete(&document);
	yaml_parser_delete(&parser);

exit:
	return ret;
}
