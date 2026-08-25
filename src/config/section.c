/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2026 Liav A
 */

#include "config/section.h"
#include "config/common.h"
#include "config/mapping.h"
#include "return_codes.h"
#include "types.h"
#include "utils.h"
#include "yaml_objs.h"
#include <yaml.h>

#include <assert.h>

static void destroy_codecs_objs(struct yaml_section *section, size_t max_idx)
{
	size_t idx;
	size_t pair_idx;
	struct yaml_codec_obj *obj;
	struct yaml_codec_obj *objs = (struct yaml_codec_obj *)section->objs;

	for (idx = 0; idx < max_idx; idx++) {
		obj = &objs[idx];

		/* These fields are mandatory, but the user might omit them for
		 * some unknown (and invalid) reason. So don't blindly free
		 * something which is not always set.
		 */
		destroy_optional_string_param(&obj->name);
		destroy_optional_string_param(&obj->type);

		/* We could use __used field but we assume this function is
		 * called after on completely-initialized objects.
		 */
		for (pair_idx = 0; pair_idx < obj->dict.key_value_pairs_count;
		    pair_idx++) {
			destroy_key_value_pair(
			    &obj->dict.key_val_pairs[pair_idx]);
		}

		free(obj->dict.key_val_pairs);
	}
}

static void free_codec_object_base_allocation(struct yaml_codec_obj *obj)
{
	free(obj->dict.key_val_pairs);
}

static yaml_return_code_t prepare_codec_object(
    yaml_node_t *node, struct yaml_codec_obj *obj)
{
	yaml_return_code_t ret;
	size_t pairs_count;

	if (node->type != YAML_MAPPING_NODE) {
		YAML_RC_SET_WITH_OFFENDING_NODE(
		    ret, YAML_RC_NODE_IS_NOT_MAPPING, node);
		goto exit;
	}

	ret = yaml_mapping_get_objects_count(node, &pairs_count);
	if (!YAML_RC_CHECK_SUCCESS(ret)) {
		goto exit;
	}

	/* If we get a mapping object with 2 entries or less, it can't ever be
	 * a valid object that we will want to use anyway. Reject it now.
	 */
	if (!(pairs_count > 2)) {
		YAML_RC_SET_WITH_OFFENDING_NODE(
		    ret, YAML_RC_INVALID_MAPPING_NODE_PAIRS_COUNT, node);
		goto exit;
	}

	ret = allocate_pairs_array(&obj->dict.key_val_pairs, pairs_count - 2);
	if (!YAML_RC_CHECK_SUCCESS(ret)) {
		YAML_RC_SET_OFFENDING_NODE(ret, node);
		goto exit;
	}

	obj->dict.key_value_pairs_count = pairs_count - 2;

	YAML_RC_SET_SUCCESS(ret);

exit:
	return ret;
}

static const char *codec_common_fields[] = {"name", "type"};

void codec_cleanup_entry(void *obj, const char *key)
{
	yaml_return_code_t ret;
	int idx;
	struct yaml_dict *dict;
	struct key_value_pair *pair;
	struct yaml_codec_obj *codec_obj = obj;

	ret = compare_key_string_value(key, &idx, codec_common_fields, 2);
	if (!YAML_RC_CHECK_SUCCESS(ret))
		goto remove_non_common_fields;

	switch (idx) {
	/* Name field */
	case 0: {
		destroy_string_param(&codec_obj->name);
		break;
	}
	/* Type field */
	case 1: {
		destroy_string_param(&codec_obj->type);
		break;
	}
	}

	return;

remove_non_common_fields:
	dict = &codec_obj->dict;

	ret = find_key_value_pair(dict, &pair, key);
	if (YAML_RC_CHECK_SUCCESS(ret))
		return;

	destroy_key_value_pair(pair);
}

yaml_return_code_t codec_parse_entry(
    void *obj, const char *key, yaml_node_t *child_node)
{
	yaml_return_code_t ret;
	int idx;
	struct yaml_dict *dict;
	struct yaml_codec_obj *codec_obj = obj;

	/* Currently all entries of all codec objects should be scalars */
	if (child_node->type != YAML_SCALAR_NODE) {
		YAML_RC_SET_WITH_OFFENDING_NODE(
		    ret, YAML_RC_NODE_IS_NOT_SCALAR, child_node);
		goto exit;
	}

	ret = compare_key_string_value(key, &idx, codec_common_fields, 2);
	if (!YAML_RC_CHECK_SUCCESS(ret))
		goto add_non_common_fields;

	switch (idx) {
	/* Name field */
	case 0: {
		if (codec_obj->name.str || codec_obj->name.len) {
			YAML_RC_SET_WITH_OFFENDING_NODE(
			    ret, YAML_RC_ENTRY_ALREADY_PARSED, child_node);
			goto exit;
		}
		return create_string_param(&codec_obj->name, child_node);
	}
	/* Type field */
	case 1: {
		if (codec_obj->type.str || codec_obj->type.len) {
			YAML_RC_SET_WITH_OFFENDING_NODE(
			    ret, YAML_RC_ENTRY_ALREADY_PARSED, child_node);
			goto exit;
		}
		return create_string_param(&codec_obj->type, child_node);
	}
	}

add_non_common_fields:

	dict = &codec_obj->dict;
	return append_pair_to_dict(dict, key, child_node);

exit:
	return ret;
}

static yaml_return_code_t parse_codecs_section(
    yaml_document_t *doc, yaml_node_t *node, struct yaml_section *section)
{
	size_t item_idx;
	yaml_return_code_t ret;
	yaml_node_t *child;
	yaml_node_item_t *item;
	struct mapping_handle handle;

	struct yaml_codec_obj *objs;

	if (node->type != YAML_SEQUENCE_NODE) {
		YAML_RC_SET_WITH_OFFENDING_NODE(
		    ret, YAML_RC_NODE_IS_NOT_SEQUENCE, node);
		goto exit;
	}

	ret = allocate_typed_array_for_section(node, &section->objs,
	    &section->objs_count, sizeof(struct yaml_codec_obj));
	if (!YAML_RC_CHECK_SUCCESS(ret)) {
		goto exit;
	}

	objs = (struct yaml_codec_obj *)section->objs;

	handle.parse_entry = codec_parse_entry;
	handle.cleanup_entry = codec_cleanup_entry;

	/* A series of YAML mapping nodes should appear now */
	item_idx = 0;
	for (item = node->data.sequence.items.start;
	    item < node->data.sequence.items.top; item++) {
		child = yaml_document_get_node(doc, *item);
		if (!child) {
			YAML_RC_SET(ret, YAML_RC_NODE_FAILED_GET_CHILD_NODE);
			goto exit;
		}

		handle.obj = &objs[item_idx];
		ret = prepare_codec_object(child, handle.obj);
		if (!YAML_RC_CHECK_SUCCESS(ret))
			goto cleanup_objs;

		ret = parse_mapping_object(doc, child, &handle);
		if (!YAML_RC_CHECK_SUCCESS(ret)) {
			free_codec_object_base_allocation(handle.obj);
			goto cleanup_objs;
		}

		item_idx++;
	}

	YAML_RC_SET_SUCCESS(ret);
	goto exit;

cleanup_objs:
	destroy_codecs_objs(section, item_idx);
	free(section->objs);
exit:
	return ret;
}

static void destroy_pipeline_objs(struct yaml_section *section, size_t max_idx)
{
	size_t idx;
	size_t str_idx;
	struct yaml_pipeline_obj *obj;
	struct yaml_pipeline_obj *objs =
	    (struct yaml_pipeline_obj *)section->objs;

	for (idx = 0; idx < max_idx; idx++) {
		obj = &objs[idx];

		/* This field is mandatory, but the user might omit them for
		 * some unknown (and invalid) reason. So don't blindly free
		 * something which is not always set.
		 */
		destroy_string_param(&obj->name);

		for (str_idx = 0; str_idx < obj->codecs.strings_count;
		    str_idx++) {
			destroy_string_param(&obj->codecs.strings[str_idx]);
		}
	}
}

static void free_pipeline_codec_entries(
    struct yaml_pipeline_obj *obj, size_t max_idx)
{
	size_t idx;

	for (idx = 0; idx < max_idx; idx++) {
		destroy_string_param(&obj->codecs.strings[idx]);
	}
}

static const char *pipeline_fields[] = {"name", "codecs"};

void pipeline_cleanup_entry(void *obj, const char *key)
{
	int idx;
	yaml_return_code_t ret;
	struct yaml_pipeline_obj *pipeline_obj = obj;

	ret = compare_key_string_value(key, &idx, pipeline_fields, 2);
	if (!YAML_RC_CHECK_SUCCESS(ret))
		return;

	switch (idx) {
	/* Name field */
	case 0: {
		destroy_string_param(&pipeline_obj->name);
		break;
	}
	/* Type field */
	case 1: {
		free_pipeline_codec_entries(
		    pipeline_obj, pipeline_obj->codecs.strings_count);
		break;
	}
	}

	return;
}

yaml_return_code_t pipeline_parse_entry(
    void *obj, const char *key, yaml_node_t *child_node)
{
	int idx;
	yaml_return_code_t ret;
	struct yaml_pipeline_obj *pipeline_obj = obj;

	ret = compare_key_string_value(key, &idx, pipeline_fields, 2);
	if (!YAML_RC_CHECK_SUCCESS(ret)) {
		YAML_RC_SET_OFFENDING_NODE(ret, child_node);
		return ret;
	}

	switch (idx) {
	/* Name field */
	case 0: {
		/* Already allocated, reject */
		if (pipeline_obj->name.str || pipeline_obj->name.len) {
			YAML_RC_SET_WITH_OFFENDING_NODE(
			    ret, YAML_RC_ENTRY_ALREADY_PARSED, child_node);
			goto exit;
		}
		return create_string_param(&pipeline_obj->name, child_node);
	}
	/* Codecs field */
	case 1: {
		/* Already allocated, reject */
		if (pipeline_obj->codecs.strings ||
		    pipeline_obj->codecs.strings_count) {
			YAML_RC_SET_WITH_OFFENDING_NODE(
			    ret, YAML_RC_ENTRY_ALREADY_PARSED, child_node);
			goto exit;
		}
		return allocate_typed_array_for_section(child_node,
		    (void **)&pipeline_obj->codecs.strings,
		    &pipeline_obj->codecs.strings_count,
		    sizeof(yaml_str_param_t));
	}
	}

	YAML_RC_SET(ret, YAML_RC_INVALID_OBJECT_NODE);
exit:
	return ret;
}

static yaml_return_code_t prepare_pipeline_object(yaml_node_t *node)
{
	yaml_return_code_t ret;
	size_t pairs_count;

	if (node->type != YAML_MAPPING_NODE) {
		YAML_RC_SET_WITH_OFFENDING_NODE(
		    ret, YAML_RC_NODE_IS_NOT_MAPPING, node);
		goto exit;
	}

	ret = yaml_mapping_get_objects_count(node, &pairs_count);
	if (!YAML_RC_CHECK_SUCCESS(ret)) {
		return ret;
	}

	/* If we get a mapping object with more or less than 2 entries then it
	 * can't ever be a valid object that we will want to use anyway. Reject
	 * it now. This can change if we add more fields to the pipeline object
	 * but for now it is damn simple...
	 */
	if (pairs_count != 2) {
		YAML_RC_SET_WITH_OFFENDING_NODE(
		    ret, YAML_RC_INVALID_MAPPING_NODE_PAIRS_COUNT, node);
		goto exit;
	}

	YAML_RC_SET_SUCCESS(ret);
exit:
	return ret;
}

static yaml_return_code_t parse_pipelines_section(
    yaml_document_t *doc, yaml_node_t *node, struct yaml_section *section)
{
	size_t item_idx;
	yaml_return_code_t ret;
	yaml_node_t *child;
	yaml_node_item_t *item;
	struct mapping_handle handle;
	struct yaml_pipeline_obj *objs;

	if (node->type != YAML_SEQUENCE_NODE) {
		YAML_RC_SET_WITH_OFFENDING_NODE(
		    ret, YAML_RC_NODE_IS_NOT_SEQUENCE, node);
		goto exit;
	}

	ret = allocate_typed_array_for_section(node, &section->objs,
	    &section->objs_count, sizeof(struct yaml_pipeline_obj));
	if (!YAML_RC_CHECK_SUCCESS(ret)) {
		goto exit;
	}
	objs = (struct yaml_pipeline_obj *)section->objs;

	handle.parse_entry = pipeline_parse_entry;
	handle.cleanup_entry = pipeline_cleanup_entry;

	/* A series of YAML mapping nodes should appear now */
	item_idx = 0;
	for (item = node->data.sequence.items.start;
	    item < node->data.sequence.items.top; item++) {
		child = yaml_document_get_node(doc, *item);
		if (!child) {
			YAML_RC_SET(ret, YAML_RC_NODE_FAILED_GET_CHILD_NODE);
			goto exit;
		}

		ret = prepare_pipeline_object(child);
		if (!YAML_RC_CHECK_SUCCESS(ret))
			goto cleanup_mappings;

		handle.obj = &objs[item_idx];
		ret = parse_mapping_object(doc, child, &handle);
		if (!YAML_RC_CHECK_SUCCESS(ret))
			goto cleanup_mappings;

		item_idx++;
	}

	YAML_RC_SET_SUCCESS(ret);
	goto exit;

cleanup_mappings:
	destroy_pipeline_objs(section, item_idx);
	free(section->objs);
exit:
	return ret;
}

static void destroy_partition_objs(struct yaml_section *section, size_t max_idx)
{
	struct yaml_partition_obj *obj;
	struct yaml_partition_obj *objs =
	    (struct yaml_partition_obj *)section->objs;

	size_t idx;

	for (idx = 0; idx < max_idx; idx++) {
		obj = &objs[idx];

		/* These fields are mandatory, but the user might omit them for
		 * some unknown (and invalid) reason. So don't blindly free
		 * something which is not always set.
		 */
		destroy_optional_string_param(&obj->name);
		destroy_optional_string_param(&obj->pipeline);
		destroy_optional_string_param(&obj->eraseblocks_param);
		destroy_optional_string_param(&obj->pages_param);
	}
}

static const char *partition_mandatory_fields[] = {"name", "pipeline"};
static const char *partition_non_mandatory_fields[] = {"eraseblocks", "pages"};

void partition_cleanup_entry(void *obj, const char *key)
{
	int idx;
	yaml_return_code_t ret;
	struct yaml_partition_obj *partition_obj = obj;

	ret =
	    compare_key_string_value(key, &idx, partition_mandatory_fields, 2);
	if (!YAML_RC_CHECK_SUCCESS(ret))
		goto check_non_mandatory_fields;

	switch (idx) {
	/* Name field */
	case 0: {
		destroy_string_param(&partition_obj->name);
		break;
	}
	/* Pipeline field */
	case 1: {
		destroy_string_param(&partition_obj->pipeline);
		break;
	}
	}

	return;
check_non_mandatory_fields:

	ret = compare_key_string_value(
	    key, &idx, partition_non_mandatory_fields, 2);
	if (!YAML_RC_CHECK_SUCCESS(ret))
		return;

	switch (idx) {
	/* Eraseblocks field */
	case 0: {
		destroy_string_param(&partition_obj->eraseblocks_param);
		break;
	}
	/* Pages field */
	case 1: {
		destroy_string_param(&partition_obj->pages_param);
		break;
	}
	}
}

yaml_return_code_t partition_parse_entry(
    void *obj, const char *key, yaml_node_t *child_node)
{
	int idx;
	yaml_return_code_t ret;
	struct yaml_partition_obj *partition_obj = obj;

	ret =
	    compare_key_string_value(key, &idx, partition_mandatory_fields, 2);
	if (!YAML_RC_CHECK_SUCCESS(ret))
		goto check_non_mandatory_fields;

	switch (idx) {
	/* Name field */
	case 0: {
		return create_string_param(&partition_obj->name, child_node);
	}
	/* Pipeline field */
	case 1: {
		return create_string_param(
		    &partition_obj->pipeline, child_node);
	}
	default:
		assert(false);
	}

check_non_mandatory_fields:

	ret = compare_key_string_value(
	    key, &idx, partition_non_mandatory_fields, 2);
	if (!YAML_RC_CHECK_SUCCESS(ret)) {
		YAML_RC_SET_OFFENDING_NODE(ret, child_node);
		return ret;
	}

	switch (idx) {
	/* Eraseblocks field */
	case 0: {
		ret = create_string_param(
		    &partition_obj->eraseblocks_param, child_node);
		break;
	}
	/* Pages field */
	case 1: {
		ret = create_string_param(
		    &partition_obj->pages_param, child_node);
		break;
	}
	default:
		assert(false);
	}

	return ret;
}

static yaml_return_code_t prepare_partition_object(yaml_node_t *node)
{
	yaml_return_code_t ret;
	size_t pairs_count;

	if (node->type != YAML_MAPPING_NODE) {
		YAML_RC_SET_WITH_OFFENDING_NODE(
		    ret, YAML_RC_NODE_IS_NOT_MAPPING, node);
		goto exit;
	}

	ret = yaml_mapping_get_objects_count(node, &pairs_count);
	if (!YAML_RC_CHECK_SUCCESS(ret)) {
		return ret;
	}

	/* If we get a mapping object with less than 2 entries then it can't
	 * ever be a valid object that we will want to use anyway. Reject it
	 * now. This can change if we add more fields to the pipeline object but
	 * for now we require only 2 mandatory fields for this type of object.
	 */
	if (pairs_count < 2) {
		YAML_RC_SET_WITH_OFFENDING_NODE(
		    ret, YAML_RC_INVALID_MAPPING_NODE_PAIRS_COUNT, node);
		goto exit;
	}

	YAML_RC_SET_SUCCESS(ret);

exit:
	return ret;
}

static yaml_return_code_t parse_partitions_section(
    yaml_document_t *doc, yaml_node_t *node, struct yaml_section *section)
{
	size_t item_idx;
	yaml_return_code_t ret;
	yaml_node_t *child;
	yaml_node_item_t *item;
	struct mapping_handle handle;
	struct yaml_partition_obj *objs;

	if (node->type != YAML_SEQUENCE_NODE) {
		YAML_RC_SET_WITH_OFFENDING_NODE(
		    ret, YAML_RC_NODE_IS_NOT_SEQUENCE, node);
		return ret;
	}

	ret = allocate_typed_array_for_section(node, &section->objs,
	    &section->objs_count, sizeof(struct yaml_partition_obj));

	if (!YAML_RC_CHECK_SUCCESS(ret)) {
		goto exit;
	}
	objs = (struct yaml_partition_obj *)section->objs;

	handle.parse_entry = partition_parse_entry;
	handle.cleanup_entry = partition_cleanup_entry;

	/* A series of YAML mapping nodes should appear now */
	item_idx = 0;
	for (item = node->data.sequence.items.start;
	    item < node->data.sequence.items.top; item++) {
		child = yaml_document_get_node(doc, *item);
		if (!child) {
			YAML_RC_SET(ret, YAML_RC_NODE_FAILED_GET_CHILD_NODE);
			goto exit;
		}

		ret = prepare_partition_object(child);
		if (!YAML_RC_CHECK_SUCCESS(ret))
			goto cleanup_mappings;

		handle.obj = &objs[item_idx];

		ret = parse_mapping_object(doc, child, &handle);
		if (!YAML_RC_CHECK_SUCCESS(ret))
			goto cleanup_mappings;

		item_idx++;
	}

	YAML_RC_SET_SUCCESS(ret);
	goto exit;

cleanup_mappings:
	destroy_partition_objs(section, item_idx);
	free(section->objs);
exit:
	return ret;
}

#define YAML_PARSE_SECTION(__name)                                             \
	do {                                                                   \
		if (scheme->_##__name.objs || scheme->_##__name.objs_count)    \
			YAML_RC_SET(ret, YAML_RC_SECTION_ALREADY_PARSED);      \
		else                                                           \
			ret = parse_##__name##_section(                        \
			    doc, value, &scheme->_##__name);                   \
	} while (0)

static const char *section_names[] = {"codecs", "pipelines", "partitions"};

yaml_return_code_t parse_section(yaml_document_t *doc,
    struct yaml_mtd_scheme *scheme, yaml_node_t *key, yaml_node_t *value)
{
	yaml_return_code_t ret;
	int idx;

	ret = compare_key_scalar_value(key, &idx, section_names, 3);
	if (!YAML_RC_CHECK_SUCCESS(ret))
		goto exit;

	switch (idx) {
	case 0: {
		YAML_PARSE_SECTION(codecs);
		break;
	}
	case 1: {
		YAML_PARSE_SECTION(pipelines);
		break;
	}
	case 2: {
		YAML_PARSE_SECTION(partitions);
		break;
	}

	default: {
		YAML_RC_SET(ret, YAML_RC_SECTION_NAME_UNKNOWN);
		break;
	}
	}
exit:
	return ret;
}
