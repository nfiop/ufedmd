/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2026 Liav A
 */

#include <stdint.h>
#include <string.h>

#include "common.h"
#include "common/types.h"
#include "config/common.h"
#include "config/return_codes.h"
#include "config/types.h"
#include "return_codes.h"
#include <jansson.h>

json_t *get_json_string_by_key(json_t *obj, const char *key)
{
	json_t *tmp = json_object_get(obj, key);
	if (!tmp)
		return NULL;
	if (!json_is_string(tmp)) {
		json_decref(tmp);
		return NULL;
	}
	return tmp;
}

void fill_range_with_max_length(struct range *range)
{
	range->length = UINT64_MAX;
}

void destroy_scalar_param(cfg_value_param_t *param)
{
	if (param->type != CFG_VALUE_PARAM_TYPE_STRING)
		return;

	destroy_string_param(&param->_value.string);
}

void print_cfg_scalar_value(const cfg_value_param_t *param)
{
	switch (param->type) {
	case CFG_VALUE_PARAM_TYPE_STRING:
		printf("%s", param->_value.string.str);
		break;
	case CFG_VALUE_PARAM_TYPE_INTEGER:
		printf("%d", param->_value.integer);
		break;
	case CFG_VALUE_PARAM_TYPE_UNSIGNED_INTEGER:
		printf("%u", param->_value.u_integer);
		break;
	case CFG_VALUE_PARAM_TYPE_DOUBLE:
		printf("%f", param->_value.real);
		break;
	case CFG_VALUE_PARAM_TYPE_BOOLEAN:
		printf("%s", param->_value.flag ? "true" : "false");
		break;
	default:
		printf("(?)");
	}
}

cfg_return_code_t create_scalar_param(cfg_value_param_t *param, json_t *value)
{
	cfg_return_code_t ret;

	json_type type = json_typeof(value);

	switch (type) {
	case JSON_STRING:
		ret = adopt_string_param(
		    &param->_value.string, json_string_value(value));
		if (!CFG_RC_CHECK_SUCCESS(ret))
			goto exit;
		param->type = CFG_VALUE_PARAM_TYPE_STRING;
		break;
	case JSON_INTEGER:
		param->_value.integer = json_integer_value(value);
		param->type = CFG_VALUE_PARAM_TYPE_INTEGER;
		break;
	case JSON_REAL:
		param->_value.real = json_real_value(value);
		param->type = CFG_VALUE_PARAM_TYPE_DOUBLE;
		break;
	case JSON_TRUE:
	case JSON_FALSE:
		param->_value.flag = json_boolean_value(value);
		param->type = CFG_VALUE_PARAM_TYPE_BOOLEAN;
		break;
	default:
		CFG_RC_SET(ret, CFG_RC_NODE_IS_NOT_SCALAR);
		goto exit;
	}

	CFG_RC_SET_SUCCESS(ret);
exit:
	return ret;
}

void destroy_string_param(cfg_str_param_t *param)
{
	free(param->str);
	param->len = 0;
}

void destroy_optional_string_param(cfg_str_param_t *param)
{
	if (param->len == 0 || !param->str)
		return;

	free(param->str);
	param->len = 0;
}

void destroy_dict(struct cfg_dict *dict)
{
	hashmap_free(dict->values);
}

cfg_return_code_t parse_bare_range_object(struct range *obj, json_t *range)
{
	cfg_return_code_t ret;
	json_t *offset = NULL;
	json_t *len = NULL;
	json_int_t tmp;

	if (!json_is_object(range)) {
		CFG_RC_SET(ret, CFG_RC_INVALID_OBJECT_NODE);
		goto exit;
	}

	offset = json_object_get(range, "offset");
	len = json_object_get(range, "length");
	if (!offset || !len || !json_is_integer(offset) ||
	    !json_is_integer(len)) {
		CFG_RC_SET(ret, CFG_RC_INVALID_OBJECT_NODE);
		goto exit;
	}

	tmp = json_integer_value(offset);
	if (tmp < 0) {
		CFG_RC_SET(ret, CFG_RC_INTEGER_IS_NEGATIVE);
		goto exit;
	}

	obj->start = (long long)tmp;

	tmp = json_integer_value(len);
	if (tmp < 0) {
		CFG_RC_SET(ret, CFG_RC_INTEGER_IS_NEGATIVE);
		goto exit;
	}

	obj->length = (long long)tmp;

	CFG_RC_SET_SUCCESS(ret);

exit:
	return ret;
}

cfg_return_code_t create_string_param(cfg_str_param_t *param, json_t *value)
{
	cfg_return_code_t ret;
	if (!json_is_string(value)) {
		CFG_RC_SET_WITH_OFFENDING_NODE(
		    ret, CFG_RC_NODE_IS_NOT_STRING, value);
		goto exit;
	}

	param->str = strdup(json_string_value(value));
	if (!param) {
		CFG_RC_SET_WITH_OFFENDING_NODE(
		    ret, CFG_RC_MEMORY_ALLOCATION_FAILED, value);
		goto exit;
	}

	param->len = strlen(param->str);

	CFG_RC_SET_SUCCESS(ret);
exit:
	return ret;
}

cfg_return_code_t adopt_string_param(cfg_str_param_t *param, const char *str)
{
	cfg_return_code_t ret;

	param->str = strdup((const char *)str);
	if (!param) {
		CFG_RC_SET(ret, CFG_RC_MEMORY_ALLOCATION_FAILED);
		goto exit;
	}

	param->len = strlen(param->str);

	CFG_RC_SET_SUCCESS(ret);
exit:
	return ret;
}

const char *return_code_value_to_string(cfg_return_code_enum_t rc)
{
	switch (rc) {
	case CFG_RC_SUCCESS:
		return "Success (?)";
	case CFG_RC_SCALAR_LENGTH_TOO_BIG:
		return "Scalar length too big";
	case CFG_RC_NODE_IS_NOT_MAPPING:
		return "Node is not a mapping";
	case CFG_RC_INVALID_OBJECT_NODE:
		return "Invalid node object";
	case CFG_RC_NODE_IS_NOT_SEQUENCE:
		return "Node is not a seqeunce";
	case CFG_RC_NODE_IS_NOT_STRING:
		return "Node is not a string";
	case CFG_RC_NODE_IS_NOT_SCALAR:
		return "Node is not a scalar";
	case CFG_RC_INVALID_BOOLEAN_FLAG:
		return "Boolean flag value is not valid";
	case CFG_RC_INTEGER_IS_NEGATIVE:
		return "Integer is negative";
	case CFG_RC_EMPTY_NODE:
		return "Node is empty";
	case CFG_RC_INVALID_SECTION_TYPE:
		return "Section type is invalid";
	case CFG_RC_MISSING_PARTITION_PARAMETERS:
		return "Partition has missing parameters";
	case CFG_RC_SCALAR_COMPARE_NO_MATCH:
		return "Scalar compare yields no match";
	case CFG_RC_NODE_FAILED_GET_CHILD_NODE:
		return "Failed to get child node";
	case CFG_RC_MISSING_PIPELINE_PARAMETERS:
		return "Pipeline has missing parameters";
	case CFG_RC_MISSING_LAYOUT_PARAMETERS:
		return "Layout has missing parameters";
	case CFG_RC_NODE_IS_NOT_ARRAY:
		return "Node is not an array";
	case CFG_RC_SECTION_ALREADY_PARSED:
		return "Section already parsed";
	case CFG_RC_ENTRY_ALREADY_PARSED:
		return "Entry already parsed";
	case CFG_RC_SECTION_NAME_UNKNOWN:
		return "Section name unknown";
	case CFG_RC_MISSING_CODEC_PARAMETERS:
		return "Codec has missing parameters";
	case CFG_RC_COMPARING_WITH_NON_SCALAR:
		return "Compare with non scalar";
	case CFG_RC_KEY_NOT_FOUND:
		return "Key not found";
	case CFG_RC_INVALID_PAIR_POSITION:
		return "Invalid pair position";
	case CFG_RC_SCALAR_COMPAREE_TOO_BIG:
		return "Scalar comparee size limit exceeded";
	case CFG_RC_NO_ROOT_NODE:
		return "No root node / JSON parser error";
	case CFG_RC_MEMORY_ALLOCATION_FAILED:
		return "Out-of-memory";
	default:
		return "Unknown";
	}
}
