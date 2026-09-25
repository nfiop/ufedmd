/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2026 Liav A
 */

#include "config/objs/pipeline.h"
#include "config/common.h"
#include "config/return_codes.h"
#include "config/scheme.h"

#include <string.h>

#include <jansson.h>

static void destroy_layout_part(struct cfg_page_layout_part_obj *obj)
{
	destroy_string_param(&obj->part_name);
	destroy_string_param(&obj->span_name);
}

static void destroy_page_layout(struct cfg_pipeline_obj *pipeline)
{
	size_t part_idx;
	for (part_idx = 0; part_idx < pipeline->page_layout.parts_count;
	    part_idx++) {
		destroy_layout_part(&pipeline->page_layout.parts[part_idx]);
	}
}

static void destroy_write_codec_specifier(
    struct cfg_write_codec_specifier_obj *specifier)
{
	if (specifier->src) {
		destroy_string_param(&specifier->src->span_name);
		free(specifier->src);
	}
	destroy_string_param(&specifier->dest.span_name);
	destroy_string_param(&specifier->codec_name);
}

static void destroy_read_codec_specifier(
    struct cfg_read_codec_specifier_obj *specifier)
{
	destroy_string_param(&specifier->data.span_name);

	if (specifier->has_valid_oob_specifier) {
		destroy_string_param(&specifier->oob.span_name);
	}

	destroy_string_param(&specifier->codec_name);
}

static void destroy_write_codec_specifiers(
    struct cfg_write_codecs_sequence *obj, size_t max_idx)
{
	size_t idx;
	for (idx = 0; idx < max_idx; idx++) {
		destroy_write_codec_specifier(&obj->objs[idx]);
	}
}

static void destroy_read_codec_specifiers(
    struct cfg_read_codecs_sequence *obj, size_t max_idx)
{
	size_t idx;
	for (idx = 0; idx < max_idx; idx++) {
		destroy_read_codec_specifier(&obj->objs[idx]);
	}
}

static void __cfg_pipelines_destroy(
    struct cfg_pipeline_obj *pipelines, size_t max_idx)
{
	size_t idx;
	struct cfg_pipeline_obj *cur;
	for (idx = 0; idx < max_idx; idx++) {
		cur = &pipelines[idx];
		destroy_string_param(&cur->name);

		if (cur->write_codecs)
			destroy_write_codec_specifiers(
			    cur->write_codecs, cur->write_codecs->objs_count);

		destroy_read_codec_specifiers(
		    &cur->read_codecs, cur->read_codecs.objs_count);

		if (cur->write_mappings) {
			destroy_string_param(
			    &cur->write_mappings->data_span_name);
			destroy_string_param(
			    &cur->write_mappings->oob_span_name);
		}

		destroy_string_param(&cur->read_mappings.data_span_name);
		destroy_string_param(&cur->read_mappings.oob_span_name);

		destroy_page_layout(cur);
	}
}

void cfg_pipelines_destroy(struct cfg_scheme *scheme)
{
	__cfg_pipelines_destroy(
	    scheme->pipelines.objs, scheme->pipelines.objs_count);
	free(scheme->pipelines.objs);
}

static cfg_return_code_t parse_range(
    struct cfg_span_specifier *obj, json_t *range)
{
	cfg_return_code_t ret;
	json_t *span_name;

	if (!json_is_object(range)) {
		CFG_RC_SET(ret, CFG_RC_INVALID_OBJECT_NODE);
		goto exit;
	}

	span_name = get_json_string_by_key(range, "span_name");
	if (!span_name) {
		CFG_RC_SET(ret, CFG_RC_INVALID_OBJECT_NODE);
		goto exit;
	}

	ret = parse_bare_range_object(&obj->_range, range);
	if (!CFG_RC_CHECK_SUCCESS(ret)) {
		goto exit;
	}

	ret = create_string_param(&obj->span_name, span_name);
	if (!CFG_RC_CHECK_SUCCESS(ret))
		goto exit;

	CFG_RC_SET_SUCCESS(ret);
exit:
	return ret;
}

static cfg_return_code_t parse_write_codec_specifier(
    struct cfg_write_codec_specifier_obj *obj, json_t *codec_specifier)
{
	cfg_return_code_t ret;
	json_t *name, *src, *dest;

	if (!json_is_object(codec_specifier)) {
		CFG_RC_SET(ret, CFG_RC_INVALID_OBJECT_NODE);
		goto exit;
	}

	name = get_json_string_by_key(codec_specifier, "name");
	src = json_object_get(codec_specifier, "src");
	dest = json_object_get(codec_specifier, "dest");

	/* src is optional. dest & name are not. */
	if (!dest || !name) {
		CFG_RC_SET(ret, CFG_RC_MISSING_CODEC_PARAMETERS);
		goto exit;
	}

	if (src) {
		obj->src = calloc(1, sizeof(struct cfg_span_specifier));
		if (!obj->src) {
			CFG_RC_SET(ret, CFG_RC_MEMORY_ALLOCATION_FAILED);
			goto exit;
		}

		ret = parse_range(obj->src, src);
		if (!CFG_RC_CHECK_SUCCESS(ret)) {
			goto free_src;
		}
	}

	ret = parse_range(&obj->dest, dest);
	if (!CFG_RC_CHECK_SUCCESS(ret)) {
		goto clean_src;
	}

	ret = create_string_param(&obj->codec_name, name);
	if (!CFG_RC_CHECK_SUCCESS(ret)) {
		goto free_dest;
	}

	CFG_RC_SET_SUCCESS(ret);
	goto exit;

free_dest:
	destroy_string_param(&obj->dest.span_name);
clean_src:
	if (obj->src)
		destroy_string_param(&obj->src->span_name);
free_src:
	if (obj->src)
		free(obj->src);
exit:
	return ret;
}

static cfg_return_code_t parse_read_codec_specifier(
    struct cfg_read_codec_specifier_obj *obj, json_t *codec_specifier)
{
	cfg_return_code_t ret;
	json_t *name, *oob, *data;

	if (!json_is_object(codec_specifier)) {
		CFG_RC_SET(ret, CFG_RC_INVALID_OBJECT_NODE);
		goto exit;
	}

	name = get_json_string_by_key(codec_specifier, "name");
	oob = json_object_get(codec_specifier, "oob");
	data = json_object_get(codec_specifier, "data");

	/* Data and name are not optional. The XOR codec doesn't need OOB
	 * when doing a decoding, so we can't require it.
	 */
	if (!data || !name) {
		CFG_RC_SET(ret, CFG_RC_MISSING_CODEC_PARAMETERS);
		goto exit;
	}

	if (!oob) {
		obj->has_valid_oob_specifier = false;
	} else {
		obj->has_valid_oob_specifier = true;
		ret = parse_range(&obj->oob, oob);
		if (!CFG_RC_CHECK_SUCCESS(ret)) {
			goto exit;
		}
	}

	ret = parse_range(&obj->data, data);
	if (!CFG_RC_CHECK_SUCCESS(ret)) {
		goto free_oob;
	}

	ret = create_string_param(&obj->codec_name, name);
	if (!CFG_RC_CHECK_SUCCESS(ret)) {
		goto free_data;
	}

	CFG_RC_SET_SUCCESS(ret);
	goto exit;

free_data:
	destroy_string_param(&obj->data.span_name);
free_oob:
	if (obj->has_valid_oob_specifier)
		destroy_string_param(&obj->oob.span_name);
exit:
	return ret;
}

static cfg_return_code_t add_read_codecs(
    struct cfg_read_codecs_sequence *obj, json_t *codecs)
{
	cfg_return_code_t ret;
	size_t count;
	size_t idx;
	json_t *tmp;

	if (!json_is_array(codecs)) {
		CFG_RC_SET(ret, CFG_RC_NODE_IS_NOT_ARRAY);
		goto exit;
	}

	/* We require a non empty set of codecs. We might change this later on
	 * if this is very unreasonable to ask for any pipeline.
	 */
	count = json_array_size(codecs);
	if (count == 0) {
		CFG_RC_SET(ret, CFG_RC_EMPTY_NODE);
		goto exit;
	}

	obj->objs = calloc(count, sizeof(struct cfg_read_codec_specifier_obj));
	if (!obj->objs) {
		CFG_RC_SET(ret, CFG_RC_MEMORY_ALLOCATION_FAILED);
		goto exit;
	}

	json_array_foreach(codecs, idx, tmp)
	{
		if (!json_is_object(tmp)) {
			CFG_RC_SET(ret, CFG_RC_INVALID_OBJECT_NODE);
			goto free_codec_specifiers;
		}

		ret = parse_read_codec_specifier(&obj->objs[idx], tmp);
		if (!CFG_RC_CHECK_SUCCESS(ret)) {
			goto free_codec_specifiers;
		}
	}

	obj->objs_count = count;
	CFG_RC_SET_SUCCESS(ret);
	goto exit;

free_codec_specifiers:
	destroy_read_codec_specifiers(obj, idx);
exit:
	return ret;
}

static cfg_return_code_t add_write_codecs(
    struct cfg_write_codecs_sequence *obj, json_t *codecs)
{
	cfg_return_code_t ret;
	size_t count;
	size_t idx;
	json_t *tmp;

	if (!json_is_array(codecs)) {
		CFG_RC_SET(ret, CFG_RC_NODE_IS_NOT_ARRAY);
		goto exit;
	}

	/* We require a non empty set of codecs. We might change this later on
	 * if this is very unreasonable to ask for any pipeline.
	 */
	count = json_array_size(codecs);
	if (count == 0) {
		CFG_RC_SET(ret, CFG_RC_EMPTY_NODE);
		goto exit;
	}

	obj->objs = calloc(count, sizeof(struct cfg_write_codec_specifier_obj));
	if (!obj->objs) {
		CFG_RC_SET(ret, CFG_RC_MEMORY_ALLOCATION_FAILED);
		goto exit;
	}

	json_array_foreach(codecs, idx, tmp)
	{
		if (!json_is_object(tmp)) {
			CFG_RC_SET(ret, CFG_RC_INVALID_OBJECT_NODE);
			goto free_codec_specifiers;
		}

		ret = parse_write_codec_specifier(&obj->objs[idx], tmp);
		if (!CFG_RC_CHECK_SUCCESS(ret)) {
			goto free_codec_specifiers;
		}
	}

	obj->objs_count = count;
	CFG_RC_SET_SUCCESS(ret);
	goto exit;

free_codec_specifiers:
	destroy_write_codec_specifiers(obj, idx);
exit:
	return ret;
}

static cfg_return_code_t add_layout_part(
    struct cfg_page_layout_part_obj *obj, json_t *part)
{
	cfg_return_code_t ret;
	json_t *part_name, *span_name;

	ret = parse_bare_range_object(&obj->_range, part);
	if (!CFG_RC_CHECK_SUCCESS(ret)) {
		goto exit;
	}

	part_name = get_json_string_by_key(part, "part_name");
	span_name = get_json_string_by_key(part, "span_name");

	if (!part_name || !span_name) {
		CFG_RC_SET(ret, CFG_RC_MISSING_LAYOUT_PARAMETERS);
		goto exit;
	}

	ret = create_string_param(&obj->part_name, part_name);
	if (!CFG_RC_CHECK_SUCCESS(ret)) {
		goto exit;
	}

	ret = create_string_param(&obj->span_name, span_name);
	if (!CFG_RC_CHECK_SUCCESS(ret)) {
		goto free_name;
	}

	CFG_RC_SET_SUCCESS(ret);
	goto exit;

free_name:
	destroy_string_param(&obj->part_name);
exit:
	return ret;
}

static cfg_return_code_t create_page_layout(
    struct cfg_page_layout_obj *obj, json_t *page_layout)
{
	cfg_return_code_t ret;
	size_t count;
	size_t idx;
	size_t obj_idx;
	json_t *tmp;

	if (!json_is_array(page_layout)) {
		CFG_RC_SET(ret, CFG_RC_INVALID_OBJECT_NODE);
		goto exit;
	}

	/* We require a non empty set of codecs. We might change this later on
	 * if this is very unreasonable to ask for any pipeline.
	 */
	count = json_array_size(page_layout);
	if (count == 0) {
		CFG_RC_SET(ret, CFG_RC_EMPTY_NODE);
		goto exit;
	}

	obj->parts = calloc(count, sizeof(struct cfg_page_layout_part_obj));
	if (!obj->parts) {
		CFG_RC_SET(ret, CFG_RC_MEMORY_ALLOCATION_FAILED);
		goto exit;
	}

	json_array_foreach(page_layout, idx, tmp)
	{
		if (!json_is_object(tmp)) {
			CFG_RC_SET(ret, CFG_RC_INVALID_OBJECT_NODE);
			goto free_parts;
		}

		ret = add_layout_part(&obj->parts[idx], tmp);
		if (!CFG_RC_CHECK_SUCCESS(ret)) {
			goto free_parts;
		}
	}

	obj->parts_count = count;
	CFG_RC_SET_SUCCESS(ret);
	goto exit;

free_parts:
	for (obj_idx = 0; obj_idx < idx; obj_idx++) {
		destroy_layout_part(&obj->parts[obj_idx]);
	}
	free(obj->parts);
exit:
	return ret;
}

static cfg_return_code_t add_span_mappings(
    struct cfg_request_span_mappings *obj, json_t *mappings)
{
	json_t *oob, *data;
	cfg_return_code_t ret;

	if (!json_is_object(mappings)) {
		CFG_RC_SET(ret, CFG_RC_INVALID_OBJECT_NODE);
		goto exit;
	}

	oob = get_json_string_by_key(mappings, "oob_span");
	data = get_json_string_by_key(mappings, "data_span");

	if (!oob || !data) {
		CFG_RC_SET(ret, CFG_RC_MISSING_PIPELINE_PARAMETERS);
		goto exit;
	}

	ret = create_string_param(&obj->data_span_name, data);
	if (!CFG_RC_CHECK_SUCCESS(ret)) {
		goto exit;
	}

	ret = create_string_param(&obj->oob_span_name, oob);
	if (!CFG_RC_CHECK_SUCCESS(ret)) {
		goto free_data_name;
	}

	CFG_RC_SET_SUCCESS(ret);
	goto exit;
free_data_name:
	destroy_string_param(&obj->data_span_name);
exit:
	return ret;
}

static cfg_return_code_t add_pipeline(
    struct cfg_pipeline_obj *pipeline, json_t *obj)
{
	json_t *name;
	json_t *page_layout;
	json_t *request_mappings, *__read_mappings, *__write_mappings;
	json_t *codecs, *__read_codecs, *__write_codecs;
	cfg_return_code_t ret;

	name = get_json_string_by_key(obj, "name");
	page_layout = json_object_get(obj, "page_layout");
	request_mappings = json_object_get(obj, "request_mappings");
	codecs = json_object_get(obj, "codecs");

	if (!name || !page_layout || !request_mappings || !codecs) {
		CFG_RC_SET(ret, CFG_RC_MISSING_PIPELINE_PARAMETERS);
		goto exit;
	}

	/* The read codecs are not optional. */
	__read_mappings = json_object_get(request_mappings, "read");
	if (!__read_mappings) {
		CFG_RC_SET(ret, CFG_RC_MISSING_PIPELINE_PARAMETERS);
		goto exit;
	}

	/* The read codecs are not optional. */
	__read_codecs = json_object_get(codecs, "read");
	if (!__read_codecs) {
		CFG_RC_SET(ret, CFG_RC_MISSING_PIPELINE_PARAMETERS);
		goto exit;
	}

	/* Try to check if write codecs are specified */
	__write_codecs = json_object_get(codecs, "write");
	__write_mappings = json_object_get(request_mappings, "write");
	if ((__write_codecs && !__write_mappings) ||
	    (!__write_codecs && __write_mappings)) {
		CFG_RC_SET(ret, CFG_RC_MISSING_PIPELINE_PARAMETERS);
		goto free_write_codecs;
	}

	if (!json_is_array(page_layout)) {
		CFG_RC_SET(ret, CFG_RC_INVALID_OBJECT_NODE);
		goto exit;
	}

	if (!json_is_object(codecs)) {
		CFG_RC_SET(ret, CFG_RC_INVALID_OBJECT_NODE);
		goto exit;
	}

	ret = create_string_param(&pipeline->name, name);
	if (!CFG_RC_CHECK_SUCCESS(ret)) {
		goto exit;
	}

	ret = create_page_layout(&pipeline->page_layout, page_layout);
	if (!CFG_RC_CHECK_SUCCESS(ret)) {
		goto free_name;
	}

	ret = add_read_codecs(&pipeline->read_codecs, __read_codecs);
	if (!CFG_RC_CHECK_SUCCESS(ret)) {
		goto free_layout;
	}

	ret = add_span_mappings(&pipeline->read_mappings, __read_mappings);
	if (!CFG_RC_CHECK_SUCCESS(ret)) {
		goto free_layout;
	}

	if (__write_codecs && __write_mappings) {
		pipeline->write_codecs =
		    calloc(1, sizeof(struct cfg_write_codecs_sequence));
		if (!pipeline->write_codecs) {
			CFG_RC_SET(ret, CFG_RC_MEMORY_ALLOCATION_FAILED);
			goto destroy_read_codecs;
		}

		ret = add_write_codecs(pipeline->write_codecs, __write_codecs);
		if (!CFG_RC_CHECK_SUCCESS(ret)) {
			goto free_write_codecs;
		}

		pipeline->write_mappings =
		    calloc(1, sizeof(struct cfg_request_span_mappings));
		if (!pipeline->write_mappings) {
			CFG_RC_SET(ret, CFG_RC_MEMORY_ALLOCATION_FAILED);
			goto destroy_write_codecs;
		}

		ret = add_span_mappings(
		    pipeline->write_mappings, __write_mappings);
		if (!CFG_RC_CHECK_SUCCESS(ret)) {
			goto free_write_mappings;
		}
	}

	CFG_RC_SET_SUCCESS(ret);
	goto exit;

free_write_mappings:
	free(pipeline->write_mappings);
destroy_write_codecs:
	destroy_write_codec_specifiers(
	    pipeline->write_codecs, pipeline->write_codecs->objs_count);
free_write_codecs:
	free(pipeline->write_codecs);
destroy_read_codecs:
	destroy_read_codec_specifiers(
	    &pipeline->read_codecs, pipeline->read_codecs.objs_count);
free_layout:
	destroy_page_layout(pipeline);
free_name:
	destroy_string_param(&pipeline->name);
exit:
	return ret;
}

cfg_return_code_t cfg_pipelines_parse(
    json_t *pipelines, struct cfg_scheme *scheme)
{
	size_t idx;
	size_t count;
	json_t *codec;
	cfg_return_code_t ret;
	struct cfg_pipelines_section *section;

	if (!json_is_array(pipelines)) {
		CFG_RC_SET(ret, CFG_RC_INVALID_SECTION_TYPE);
		goto exit;
	}

	count = json_array_size(pipelines);
	if (count == 0) {
		CFG_RC_SET(ret, CFG_RC_EMPTY_NODE);
		goto exit;
	}

	section = &scheme->pipelines;
	section->objs = calloc(count, sizeof(struct cfg_pipeline_obj));
	if (!section->objs) {
		CFG_RC_SET(ret, CFG_RC_MEMORY_ALLOCATION_FAILED);
		goto exit;
	}

	json_array_foreach(pipelines, idx, codec)
	{
		ret = add_pipeline(&section->objs[idx], codec);
		if (!CFG_RC_CHECK_SUCCESS(ret))
			goto free_objs;
	}

	section->objs_count = count;

	CFG_RC_SET_SUCCESS(ret);
	goto exit;

free_objs:
	__cfg_pipelines_destroy(section->objs, idx);
	free(section->objs);
exit:
	return ret;
}
