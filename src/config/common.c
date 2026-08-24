/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2026 Liav A
 */

#include "config/common.h"
#include "config/return_codes.h"
#include "return_codes.h"

#define MAX_NAME_LEN (256)

yaml_return_code_t compare_key_string_value(
    const char *string, int *index, const char **strings, size_t strings_count)
{
	size_t i;
	size_t scalar_len;
	yaml_return_code_t ret;

	if (!strings_count) {
		YAML_RC_SET(ret, YAML_RC_EMPTY_COMPARE_SET);
		goto exit;
	}

	scalar_len = strlen((const char *)string);
	if (scalar_len > MAX_NAME_LEN) {
		YAML_RC_SET(ret, YAML_RC_SCALAR_LENGTH_TOO_BIG);
		goto exit;
	}

	for (i = 0; i < strings_count; i++) {
		if (!strncmp((const char *)string, strings[i], scalar_len)) {
			*index = (int)i;
			YAML_RC_SET(ret, YAML_RC_SUCCESS);
			goto exit;
		}
	}

	YAML_RC_SET(ret, YAML_RC_SCALAR_COMPARE_NO_MATCH);
exit:
	return ret;
}

yaml_return_code_t compare_key_scalar_value(
    yaml_node_t *node, int *index, const char **strings, size_t strings_count)
{
	yaml_return_code_t ret;

	if (node->type != YAML_SCALAR_NODE) {
		YAML_RC_SET(ret, YAML_RC_COMPARING_WITH_NON_SCALAR);
	}

	if (!strings_count) {
		YAML_RC_SET(ret, YAML_RC_EMPTY_COMPARE_SET);
		goto exit;
	}

	return compare_key_string_value((const char *)node->data.scalar.value,
	    index, strings, strings_count);

exit:
	return ret;
}

yaml_return_code_t compare_key_scalar_one_value(
    yaml_node_t *node, const char *string)
{
	yaml_return_code_t ret;
	size_t scalar_len;

	if (node->type != YAML_SCALAR_NODE) {
		YAML_RC_SET(ret, YAML_RC_COMPARING_WITH_NON_SCALAR);
		goto exit;
	}

	scalar_len = strlen((const char *)node->data.scalar.value);
	if (scalar_len > MAX_NAME_LEN) {
		YAML_RC_SET(ret, YAML_RC_SCALAR_LENGTH_TOO_BIG);
		goto exit;
	}

	if (!strncmp(
		(const char *)node->data.scalar.value, string, scalar_len)) {
		YAML_RC_SET_SUCCESS(ret);
		goto exit;
	}

	YAML_RC_SET(ret, YAML_RC_SCALAR_COMPARE_NO_MATCH);
exit:
	return ret;
}

yaml_return_code_t yaml_sequence_get_objects_count(
    yaml_node_t *node, size_t *count)
{
	size_t tmp;
	yaml_node_item_t *item;
	yaml_return_code_t ret;

	if (node->type != YAML_SEQUENCE_NODE) {
		YAML_RC_SET(ret, YAML_RC_NODE_IS_NOT_SEQUENCE);
		goto exit;
	}

	tmp = 0;
	for (item = node->data.sequence.items.start;
	    item < node->data.sequence.items.top; item++) {
		tmp++;
	}

	*count = tmp;

	YAML_RC_SET_SUCCESS(ret);
exit:
	return ret;
}

void destroy_string_param(yaml_str_param_t *param)
{
	free(param->str);
	param->len = 0;
}

void destroy_optional_string_param(yaml_str_param_t *param)
{
	if (param->len == 0 || !param->str)
		return;

	free(param->str);
	param->len = 0;
}

void destroy_key_value_pair(struct key_value_pair *pair)
{
	destroy_string_param(&pair->key);
	destroy_string_param(&pair->value);
}

yaml_return_code_t append_pair_to_dict(
    struct yaml_dict *dict, const char *key, yaml_node_t *value)
{
	yaml_return_code_t ret;
	struct key_value_pair *pair;

	if (value->type != YAML_SCALAR_NODE) {
		YAML_RC_SET(ret, YAML_RC_NODE_IS_NOT_SCALAR);
		goto exit;
	}

	if (dict->__used == dict->key_value_pairs_count) {
		YAML_RC_SET_WITH_OFFENDING_NODE(
		    ret, YAML_RC_INVALID_MAPPING_NODE, value);
		goto exit;
	}

	/* Create a unique key-value pair */
	ret = find_key_value_pair(dict, &pair, key);
	if (YAML_RC_CHECK_SUCCESS(ret)) {
		YAML_RC_SET(ret, YAML_RC_ENTRY_ALREADY_PARSED);
		goto exit;
	}

	ret = create_key_value_pair(
	    &dict->key_val_pairs[dict->__used], key, value);
	if (!YAML_RC_CHECK_SUCCESS(ret))
		return ret;

	dict->__used++;

	YAML_RC_SET_SUCCESS(ret);
exit:
	return ret;
}

yaml_return_code_t create_string_param(
    yaml_str_param_t *param, yaml_node_t *value)
{
	yaml_return_code_t ret;
	if (value->type != YAML_SCALAR_NODE) {
		YAML_RC_SET(ret, YAML_RC_NODE_IS_NOT_SCALAR);
		goto exit;
	}

	param->str = strdup((const char *)value->data.scalar.value);
	if (!param) {
		YAML_RC_SET(ret, YAML_RC_MEMORY_ALLOCATION_FAILED);
		goto exit;
	}

	param->len = strlen(param->str);

	YAML_RC_SET_SUCCESS(ret);
exit:
	return ret;
}

static yaml_return_code_t adopt_string_param(
    yaml_str_param_t *param, const char *str)
{
	yaml_return_code_t ret;

	param->str = strdup((const char *)str);
	if (!param) {
		YAML_RC_SET(ret, YAML_RC_MEMORY_ALLOCATION_FAILED);
		goto exit;
	}

	param->len = strlen(param->str);

	YAML_RC_SET_SUCCESS(ret);
exit:
	return ret;
}

yaml_return_code_t create_key_value_pair(
    struct key_value_pair *pair, const char *key, yaml_node_t *value)
{
	yaml_return_code_t ret;

	ret = adopt_string_param(&pair->key, key);
	if (!YAML_RC_CHECK_SUCCESS(ret))
		return ret;

	ret = create_string_param(&pair->value, value);
	if (YAML_RC_CHECK_SUCCESS(ret))
		goto exit;

	destroy_string_param(&pair->key);
exit:
	return ret;
}

yaml_return_code_t find_key_value_pair(
    struct yaml_dict *dict, struct key_value_pair **pairp, const char *key)
{
	yaml_return_code_t ret;
	size_t idx;
	struct key_value_pair *pair;

	for (idx = 0; idx < dict->__used; idx++) {
		pair = &dict->key_val_pairs[idx];
		if (!strncmp(pair->key.str, key, pair->key.len)) {
			*pairp = pair;
			YAML_RC_SET_SUCCESS(ret);
			goto exit;
		}
	}

	YAML_RC_SET(ret, YAML_RC_KEY_NOT_FOUND);
exit:
	return ret;
}

yaml_return_code_t allocate_typed_array_for_section(
    yaml_node_t *node, void **arrp, size_t *objs_countp, size_t struct_size)
{
	yaml_return_code_t ret;
	size_t objs_count;
	void *arr;

	ret = yaml_sequence_get_objects_count(node, &objs_count);
	if (!YAML_RC_CHECK_SUCCESS(ret))
		return ret;

	arr = calloc(objs_count, struct_size);
	if (!arr) {
		YAML_RC_SET(ret, YAML_RC_MEMORY_ALLOCATION_FAILED);
		goto exit;
	}

	*arrp = arr;
	*objs_countp = objs_count;

	YAML_RC_SET_SUCCESS(ret);
exit:
	return ret;
}

yaml_return_code_t allocate_pairs_array(
    struct key_value_pair **arrp, size_t count)
{
	yaml_return_code_t ret;
	struct key_value_pair *arr;

	arr = calloc(count, sizeof(struct key_value_pair));
	if (!arr) {
		YAML_RC_SET(ret, YAML_RC_MEMORY_ALLOCATION_FAILED);
		goto exit;
	}

	*arrp = arr;

	YAML_RC_SET_SUCCESS(ret);
exit:
	return ret;
}

yaml_return_code_t yaml_mapping_get_objects_count(
    yaml_node_t *node, size_t *count)
{
	yaml_return_code_t ret;

	YAML_RC_SET_SUCCESS(ret);

	if (node->type != YAML_MAPPING_NODE) {
		YAML_RC_SET(ret, YAML_RC_NODE_IS_NOT_MAPPING);
		goto exit;
	}

	*count =
	    (node->data.mapping.pairs.top - node->data.mapping.pairs.start);

exit:
	return ret;
}

yaml_return_code_t build_str_param_from_node_val(yaml_document_t *doc,
    yaml_str_param_t *str, yaml_node_t *parent_node, yaml_node_pair_t *pair,
    const char *key_name)
{
	yaml_return_code_t ret;
	yaml_node_t *value;
	yaml_node_t *key;

	if (pair == parent_node->data.mapping.pairs.top) {
		YAML_RC_SET_WITH_OFFENDING_NODE(
		    ret, YAML_RC_INVALID_MAPPING_NODE, parent_node);
		goto exit;
	}

	key = yaml_document_get_node(doc, pair->key);
	if (!key) {
		YAML_RC_SET(ret, YAML_RC_INVALID_OBJECT_NODE);
		goto exit;
	}

	ret = compare_key_scalar_one_value(key, key_name);
	if (!YAML_RC_CHECK_SUCCESS(ret)) {
		goto exit;
	}

	value = yaml_document_get_node(doc, pair->value);
	if (!value) {
		YAML_RC_SET(ret, YAML_RC_INVALID_OBJECT_NODE);
		goto exit;
	}

	ret = create_string_param(str, value);
exit:
	return ret;
}

const char *return_code_value_to_string(yaml_return_code_enum_t rc)
{
	switch (rc) {
	case YAML_RC_SUCCESS:
		return "Success (?)";
	case YAML_RC_SCALAR_LENGTH_TOO_BIG:
		return "Scalar length too big";
	case YAML_RC_NODE_IS_NOT_MAPPING:
		return "Node is not a mapping";
	case YAML_RC_INVALID_OBJECT_NODE:
		return "Invalid node object";
	case YAML_RC_NODE_IS_NOT_SEQUENCE:
		return "Node is not a seqeunce";
	case YAML_RC_NODE_IS_NOT_SCALAR:
		return "Node is not a scalar";
	case YAML_RC_SCALAR_COMPARE_NO_MATCH:
		return "Scalar compare yields no match";
	case YAML_RC_NODE_FAILED_GET_CHILD_NODE:
		return "Failed to get child node";
	case YAML_RC_SECTION_ALREADY_PARSED:
		return "Section already parsed";
	case YAML_RC_ENTRY_ALREADY_PARSED:
		return "Entry already parsed";
	case YAML_RC_SECTION_NAME_UNKNOWN:
		return "Section name unknown";
	case YAML_RC_EMPTY_COMPARE_SET:
		return "Empty compare set";
	case YAML_RC_INVALID_MAPPING_NODE:
		return "Invalid mapping node";
	case YAML_RC_INVALID_MAPPING_NODE_PAIRS_COUNT:
		return "Invalid pairs count in mapping node";
	case YAML_RC_INVALID_MAPPING_NODE_ENTRY_KEY:
		return "Invalid mapping node entry's key";
	case YAML_RC_INVALID_MAPPING_NODE_ENTRY_VALUE:
		return "Invalid mapping node entry's value";
	case YAML_RC_COMPARING_WITH_NON_SCALAR:
		return "Compare with non scalar";
	case YAML_RC_KEY_NOT_FOUND:
		return "Key not found";
	case YAML_RC_NO_ROOT_NODE:
		return "No root node";
	case YAML_RC_PARSER_INITIALIZE_FAILED:
		return "Parser initialization failed";
	case YAML_RC_PARSER_LOAD_FAILED:
		return "Parser load failed";
	case YAML_RC_MEMORY_ALLOCATION_FAILED:
		return "Out-of-memory";
	default:
		return "Unknown";
	}
}
