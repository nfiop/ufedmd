/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2026 Liav A
 */

#include "config/objs/codec.h"
#include "config/common.h"
#include "config/return_codes.h"
#include "config/scheme.h"

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

	ret = allocate_pairs_array(&special_params->key_val_pairs, count);
	if (!CFG_RC_CHECK_SUCCESS(ret)) {
		CFG_RC_SET(ret, CFG_RC_MEMORY_ALLOCATION_FAILED);
		goto exit;
	}

	special_params->key_value_pairs_count = count;

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
		if (!CFG_RC_CHECK_SUCCESS(ret)) {
			goto free_pairs;
		}
	}

	CFG_RC_SET_SUCCESS(ret);
	goto exit;

free_pairs:
	/* If we set count to idx, the destroy function will do the rest for us
	 */
	special_params->key_value_pairs_count = idx;
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
