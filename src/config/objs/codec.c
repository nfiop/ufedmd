/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2026 Liav A
 */

#include "config/objs/codec.h"
#include "config/common.h"
#include "config/return_codes.h"
#include "config/scheme.h"

#include <string.h>

#include <jansson.h>

static void __cfg_codecs_destroy(struct cfg_codec_obj *codecs, size_t max_idx)
{
	size_t idx;
	struct cfg_codec_obj *cur;
	for (idx = 0; idx < max_idx; idx++) {
		cur = &codecs[idx];
		destroy_string_param(&cur->name);
		destroy_string_param(&cur->type);
		destroy_dict(&cur->special_params);
	}
}

void cfg_codecs_destroy(struct cfg_scheme *scheme)
{
	__cfg_codecs_destroy(scheme->codecs.objs, scheme->codecs.objs_count);
	free(scheme->codecs.objs);
}

static int key_value_pair_compare(const void *a, const void *b, void *udata)
{
	UNUSED(udata);
	const struct key_value_pair *ua = a;
	const struct key_value_pair *ub = b;
	return strcmp(ua->key.str, ub->key.str);
}

static uint64_t key_value_pair_hash(
    const void *item, uint64_t seed0, uint64_t seed1)
{
	const struct key_value_pair *pair = item;
	return hashmap_sip(pair->key.str, strlen(pair->key.str), seed0, seed1);
}

static void key_value_pair_destroy(void *item)
{
	struct key_value_pair *pair = item;

	destroy_string_param(&pair->key);
	destroy_scalar_param(&pair->value);
}

static cfg_return_code_t create_key_value_pair(
    struct key_value_pair *pair, const char *key, json_t *value)
{
	cfg_return_code_t ret;

	ret = adopt_string_param(&pair->key, key);
	if (!CFG_RC_CHECK_SUCCESS(ret))
		return ret;

	ret = create_scalar_param(&pair->value, value);
	if (CFG_RC_CHECK_SUCCESS(ret))
		goto exit;

	destroy_string_param(&pair->key);
exit:
	return ret;
}

static cfg_return_code_t append_pair_to_dict(
    struct cfg_dict *dict, const char *key, json_t *value)
{
	cfg_return_code_t ret;
	struct key_value_pair pair;
	void *hashmap_ret;

	if (!json_is_string(value) && !json_is_integer(value) &&
	    !json_is_real(value) && !json_is_true(value) &&
	    !json_is_false(value)) {
		CFG_RC_SET_WITH_OFFENDING_NODE(
		    ret, CFG_RC_NODE_IS_NOT_SCALAR, value);
		goto exit;
	}

	ret = create_key_value_pair(&pair, key, value);
	if (!CFG_RC_CHECK_SUCCESS(ret)) {
		CFG_RC_SET_OFFENDING_NODE(ret, value);
		goto exit;
	}

	hashmap_ret = (void *)hashmap_set(dict->values, &pair);
	if (!hashmap_ret && hashmap_oom(dict->values)) {
		CFG_RC_SET(ret, CFG_RC_MEMORY_ALLOCATION_FAILED);
		CFG_RC_SET_OFFENDING_NODE(ret, value);
		goto free_pair;
	} else if (hashmap_ret) {
		CFG_RC_SET(ret, CFG_RC_ENTRY_ALREADY_PARSED);
		CFG_RC_SET_OFFENDING_NODE(ret, value);
		goto free_pair;
	}

	CFG_RC_SET_SUCCESS(ret);
	goto exit;

free_pair:
	destroy_string_param(&pair.key);
	destroy_scalar_param(&pair.value);
exit:
	return ret;
}

static cfg_return_code_t parse_special_params(
    struct cfg_dict *special_params, json_t *params)
{
	cfg_return_code_t ret;
	size_t count;
	size_t idx;
	json_t *obj;
	json_t *key, *value;

	if (!json_is_array(params)) {
		CFG_RC_SET(ret, CFG_RC_INVALID_OBJECT_NODE);
		goto exit;
	}

	count = json_array_size(params);
	if (count == 0) {
		CFG_RC_SET(ret, CFG_RC_EMPTY_NODE);
		goto exit;
	}

	special_params->values = hashmap_new(sizeof(struct key_value_pair),
	    count, 0, 0, key_value_pair_hash, key_value_pair_compare,
	    key_value_pair_destroy, NULL);
	if (!special_params->values) {
		CFG_RC_SET(ret, CFG_RC_MEMORY_ALLOCATION_FAILED);
		goto exit;
	}

	/* We can fail here because of two cases -
	 * 1. Out of memory
	 * 2. Duplicate key
	 */
	json_array_foreach(params, idx, obj)
	{
		if (!json_is_object(obj)) {
			CFG_RC_SET(ret, CFG_RC_INVALID_OBJECT_NODE);
			goto free_pairs;
		}
		key = get_json_string_by_key(obj, "key");
		if (!key) {
			CFG_RC_SET(ret, CFG_RC_INVALID_OBJECT_NODE);
			goto free_pairs;
		}
		value = json_object_get(obj, "value");
		if (!value) {
			CFG_RC_SET(ret, CFG_RC_INVALID_OBJECT_NODE);
			goto free_pairs;
		}

		ret = append_pair_to_dict(
		    special_params, json_string_value(key), value);
	}

	CFG_RC_SET_SUCCESS(ret);
	goto exit;

free_pairs:
	destroy_dict(special_params);
exit:
	return ret;
}

static cfg_return_code_t add_codec(struct cfg_codec_obj *codec, json_t *obj)
{
	json_t *name;
	json_t *type;
	json_t *params;
	cfg_return_code_t ret;

	name = get_json_string_by_key(obj, "name");
	type = get_json_string_by_key(obj, "type");
	params = json_object_get(obj, "params");

	/* params is optional. Others are not. */
	if (!name || !type) {
		CFG_RC_SET(ret, CFG_RC_MISSING_CODEC_PARAMETERS);
		goto exit;
	}

	ret = create_string_param(&codec->name, name);
	if (!CFG_RC_CHECK_SUCCESS(ret)) {
		goto exit;
	}

	ret = create_string_param(&codec->type, type);
	if (!CFG_RC_CHECK_SUCCESS(ret)) {
		goto free_name;
	}

	/* Lastly, add all codec-specific params into a dictionary */
	if (params) {
		ret = parse_special_params(&codec->special_params, params);
		if (!CFG_RC_CHECK_SUCCESS(ret)) {
			goto free_type;
		}
	}

	CFG_RC_SET_SUCCESS(ret);
	goto exit;

free_type:
	destroy_string_param(&codec->type);
free_name:
	destroy_string_param(&codec->name);
exit:
	return ret;
}

cfg_return_code_t cfg_codecs_parse(json_t *codecs, struct cfg_scheme *scheme)
{
	size_t idx;
	size_t count;
	json_t *codec;
	cfg_return_code_t ret;
	struct cfg_codecs_section *section;

	if (!json_is_array(codecs)) {
		CFG_RC_SET(ret, CFG_RC_INVALID_SECTION_TYPE);
		goto exit;
	}

	count = json_array_size(codecs);
	if (count == 0) {
		CFG_RC_SET(ret, CFG_RC_EMPTY_NODE);
		goto exit;
	}

	section = &scheme->codecs;
	section->objs = calloc(count, sizeof(struct cfg_codec_obj));
	if (!section->objs) {
		CFG_RC_SET(ret, CFG_RC_MEMORY_ALLOCATION_FAILED);
		goto exit;
	}

	json_array_foreach(codecs, idx, codec)
	{
		ret = add_codec(&section->objs[idx], codec);
		if (!CFG_RC_CHECK_SUCCESS(ret))
			goto free_objs;
	}

	section->objs_count = count;

	CFG_RC_SET_SUCCESS(ret);
	goto exit;

free_objs:
	__cfg_codecs_destroy(section->objs, idx);
	free(section->objs);
exit:
	return ret;
}
