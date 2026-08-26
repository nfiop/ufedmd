/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2026 Liav A
 */

#include "config/mapping.h"
#include "config/return_codes.h"
#include "return_codes.h"
#include <assert.h>
#include <yaml.h>

static void free_nodes(yaml_document_t *doc, yaml_node_t *node,
    struct mapping_handle *handle, size_t max_item_idx)
{
	size_t item_idx = 0;
	yaml_node_t *key;
	yaml_node_pair_t *pair;

	pair = node->data.mapping.pairs.start;
	for (item_idx = 0; item_idx < max_item_idx; item_idx++) {
		if (pair == node->data.mapping.pairs.top)
			break;
		key = yaml_document_get_node(doc, pair->key);
		if (!key) {
			pair++;
			continue;
		}

		assert(key->type == YAML_SCALAR_NODE);

		handle->cleanup_entry(
		    handle->obj, (const char *)key->data.scalar.value);
		pair++;
	}
}

yaml_return_code_t parse_mapping_object(
    yaml_document_t *doc, yaml_node_t *node, struct mapping_handle *handle)
{
	yaml_return_code_t ret;
	yaml_node_pair_t *pair;
	yaml_node_t *key;
	yaml_node_t *value;
	size_t item_idx = 0;

	for (pair = node->data.mapping.pairs.start;
	    pair < node->data.mapping.pairs.top; pair++) {
		key = yaml_document_get_node(doc, pair->key);
		value = yaml_document_get_node(doc, pair->value);
		if (!key) {
			YAML_RC_SET_WITH_OFFENDING_NODE(
			    ret, YAML_RC_INVALID_MAPPING_NODE_ENTRY_KEY, node);
			goto free_parsed_nodes;
		}

		if (!value) {
			YAML_RC_SET_WITH_OFFENDING_NODE(ret,
			    YAML_RC_INVALID_MAPPING_NODE_ENTRY_VALUE, node);
			goto free_parsed_nodes;
		}

		switch (value->type) {
		case YAML_MAPPING_NODE: {
			YAML_RC_SET_WITH_OFFENDING_NODE(
			    ret, YAML_RC_INVALID_OBJECT_NODE, value);
			goto free_parsed_nodes;
		}
		case YAML_SEQUENCE_NODE:
		case YAML_SCALAR_NODE: {
			ret = handle->parse_entry(handle->obj,
			    (const char *)key->data.scalar.value, doc, value);
			if (!YAML_RC_CHECK_SUCCESS(ret))
				goto free_parsed_nodes;
			break;
		}

		default:
			YAML_RC_SET_WITH_OFFENDING_NODE(
			    ret, YAML_RC_INVALID_OBJECT_NODE, value);
			goto free_parsed_nodes;
		}
	}

	YAML_RC_SET_SUCCESS(ret);
	goto exit;

free_parsed_nodes:
	free_nodes(doc, node, handle, item_idx);
exit:
	return ret;
}
