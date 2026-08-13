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
	if (!strings_count)
		return YAML_RC_EMPTY_COMPARE_SET;

	scalar_len = strlen((const char *)string);
	if (scalar_len > MAX_NAME_LEN)
		return YAML_RC_SCALAR_LENGTH_TOO_BIG;

	for (i = 0; i < strings_count; i++) {
		if (!strncmp((const char *)string, strings[i], scalar_len)) {
			*index = (int)i;
			return YAML_RC_SUCCESS;
		}
	}

	return YAML_RC_SCALAR_COMPARE_NO_MATCH;
}

yaml_return_code_t compare_key_scalar_value(
    yaml_node_t *node, int *index, const char **strings, size_t strings_count)
{
	if (node->type != YAML_SCALAR_NODE)
		return YAML_RC_COMPARING_WITH_NON_SCALAR;

	if (!strings_count)
		return YAML_RC_EMPTY_COMPARE_SET;

	return compare_key_string_value((const char *)node->data.scalar.value,
	    index, strings, strings_count);
}

yaml_return_code_t compare_key_scalar_one_value(
    yaml_node_t *node, const char *string)
{
	size_t scalar_len;
	if (node->type != YAML_SCALAR_NODE)
		return YAML_RC_COMPARING_WITH_NON_SCALAR;

	scalar_len = strlen((const char *)node->data.scalar.value);
	if (scalar_len > MAX_NAME_LEN)
		return YAML_RC_SCALAR_LENGTH_TOO_BIG;

	if (!strncmp((const char *)node->data.scalar.value, string, scalar_len))
		return YAML_RC_SUCCESS;

	return YAML_RC_SCALAR_COMPARE_NO_MATCH;
}

yaml_return_code_t yaml_sequence_get_objects_count(
    yaml_node_t *node, size_t *count)
{
	size_t tmp;
	yaml_node_item_t *item;

	if (node->type != YAML_SEQUENCE_NODE)
		return YAML_RC_NODE_IS_NOT_SEQUENCE;

	tmp = 0;
	for (item = node->data.sequence.items.start;
	    item < node->data.sequence.items.top; item++) {
		tmp++;
	}

	*count = tmp;

	return YAML_RC_SUCCESS;
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

	if (value->type != YAML_SCALAR_NODE)
		return YAML_RC_NODE_IS_NOT_SCALAR;

	if (dict->__used == dict->key_value_pairs_count)
		return YAML_RC_INVALID_MAPPING_NODE;

	/* Create a unique key-value pair */
	ret = find_key_value_pair(dict, &pair, key);
	if (ret == YAML_RC_SUCCESS)
		return YAML_RC_ENTRY_ALREADY_PARSED;

	ret = create_key_value_pair(
	    &dict->key_val_pairs[dict->__used], key, value);
	if (ret != YAML_RC_SUCCESS)
		return ret;

	dict->__used++;
	return YAML_RC_SUCCESS;
}

yaml_return_code_t create_string_param(
    yaml_str_param_t *param, yaml_node_t *value)
{
	if (value->type != YAML_SCALAR_NODE)
		return YAML_RC_NODE_IS_NOT_SCALAR;

	param->str = strdup((const char *)value->data.scalar.value);
	if (!param)
		return YAML_RC_MEMORY_ALLOCATION_FAILED;
	param->len = strlen(param->str);
	return YAML_RC_SUCCESS;
}

static yaml_return_code_t adopt_string_param(
    yaml_str_param_t *param, const char *str)
{
	param->str = strdup((const char *)str);
	if (!param)
		return YAML_RC_MEMORY_ALLOCATION_FAILED;
	param->len = strlen(param->str);
	return YAML_RC_SUCCESS;
}

yaml_return_code_t create_key_value_pair(
    struct key_value_pair *pair, const char *key, yaml_node_t *value)
{
	yaml_return_code_t ret;

	ret = adopt_string_param(&pair->key, key);
	if (ret != YAML_RC_SUCCESS)
		return ret;

	ret = create_string_param(&pair->value, value);
	if (ret == YAML_RC_SUCCESS)
		goto exit;

	destroy_string_param(&pair->key);
exit:
	return ret;
}

yaml_return_code_t find_key_value_pair(
    struct yaml_dict *dict, struct key_value_pair **pairp, const char *key)
{
	size_t idx;
	struct key_value_pair *pair;

	for (idx = 0; idx < dict->__used; idx++) {
		pair = &dict->key_val_pairs[idx];
		if (!strncmp(pair->key.str, key, pair->key.len)) {
			*pairp = pair;
			return YAML_RC_SUCCESS;
		}
	}

	return YAML_RC_KEY_NOT_FOUND;
}

yaml_return_code_t allocate_typed_array_for_section(
    yaml_node_t *node, void **arrp, size_t *objs_countp, size_t struct_size)
{
	yaml_return_code_t ret;
	size_t objs_count;
	void *arr;

	ret = yaml_sequence_get_objects_count(node, &objs_count);
	if (ret != YAML_RC_SUCCESS)
		return ret;

	arr = calloc(objs_count, struct_size);
	if (!arr)
		return YAML_RC_MEMORY_ALLOCATION_FAILED;

	*arrp = arr;
	*objs_countp = objs_count;
	return YAML_RC_SUCCESS;
}

yaml_return_code_t allocate_pairs_array(
    struct key_value_pair **arrp, size_t count)
{
	struct key_value_pair *arr;

	arr = calloc(count, sizeof(struct key_value_pair));
	if (!arr)
		return YAML_RC_MEMORY_ALLOCATION_FAILED;

	*arrp = arr;
	return YAML_RC_SUCCESS;
}

yaml_return_code_t yaml_mapping_get_objects_count(
    yaml_document_t *doc, yaml_node_t *node, size_t *count)
{
	yaml_return_code_t ret = YAML_RC_SUCCESS;
	size_t tmp;
	yaml_node_pair_t *pair;
	yaml_node_t *key, *value;

	if (node->type != YAML_MAPPING_NODE)
		return YAML_RC_NODE_IS_NOT_MAPPING;

	tmp = 0;
	for (pair = node->data.mapping.pairs.start;
	    pair < node->data.mapping.pairs.top; pair++) {
		key = yaml_document_get_node(doc, pair->key);
		value = yaml_document_get_node(doc, pair->value);
		if (!key || !value) {
			ret = YAML_RC_INVALID_OBJECT_NODE;
			goto exit;
		}
		tmp++;
	}

	*count = tmp;

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
		ret = YAML_RC_INVALID_MAPPING_NODE;
		goto exit;
	}

	key = yaml_document_get_node(doc, pair->key);
	if (!key) {
		ret = YAML_RC_INVALID_OBJECT_NODE;
		goto exit;
	}

	ret = compare_key_scalar_one_value(key, key_name);
	if (ret != YAML_RC_SUCCESS) {
		goto exit;
	}

	value = yaml_document_get_node(doc, pair->value);
	if (!value) {
		ret = YAML_RC_INVALID_OBJECT_NODE;
		goto exit;
	}

	ret = create_string_param(str, value);
exit:
	return ret;
}
