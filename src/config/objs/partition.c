/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2026 Liav A
 */

#include "config/objs/partition.h"
#include "config/common.h"
#include "config/return_codes.h"
#include "config/scheme.h"

#include <jansson.h>

static void __cfg_partitions_destroy(
    struct cfg_partition_obj *partitions, size_t max_idx)
{
	size_t idx;
	struct cfg_partition_obj *cur;
	for (idx = 0; idx < max_idx; idx++) {
		cur = &partitions[idx];
		destroy_string_param(&cur->name);
		destroy_string_param(&cur->pipeline);
	}
}

void cfg_partitions_destroy(struct cfg_scheme *scheme)
{
	__cfg_partitions_destroy(
	    scheme->partitions.objs, scheme->partitions.objs_count);
	free(scheme->partitions.objs);
}

static cfg_return_code_t add_partition(
    struct cfg_partition_obj *partition, json_t *obj)
{
	json_t *name;
	json_t *pipeline;
	json_t *eraseblocks;
	json_t *pages;
	cfg_return_code_t ret;

	name = get_json_string_by_key(obj, "name");
	pipeline = get_json_string_by_key(obj, "pipeline");
	eraseblocks = json_object_get(obj, "eraseblocks");
	pages = json_object_get(obj, "pages");

	/* eraseblocks and pages are optional. name and pipeline are not. */
	if (!name || !pipeline) {
		CFG_RC_SET(ret, CFG_RC_MISSING_PARTITION_PARAMETERS);
		goto exit;
	}

	ret = create_string_param(&partition->name, name);
	if (!CFG_RC_CHECK_SUCCESS(ret)) {
		goto exit;
	}

	ret = create_string_param(&partition->pipeline, pipeline);
	if (!CFG_RC_CHECK_SUCCESS(ret)) {
		goto free_name;
	}

	if (eraseblocks) {
		ret = parse_bare_range_object(
		    &partition->eraseblocks, eraseblocks);
		if (!CFG_RC_CHECK_SUCCESS(ret)) {
			goto free_pipeline;
		}
	} else {
		partition->eraseblocks.start = 0;
		fill_range_with_max_length(&partition->eraseblocks);
	}

	if (pages) {
		ret = parse_bare_range_object(&partition->pages, pages);
		if (!CFG_RC_CHECK_SUCCESS(ret)) {
			goto free_pipeline;
		}
	} else {
		partition->pages.start = 0;
		fill_range_with_max_length(&partition->pages);
	}

	CFG_RC_SET_SUCCESS(ret);
	goto exit;

free_pipeline:
	destroy_string_param(&partition->pipeline);
free_name:
	destroy_string_param(&partition->name);
exit:
	return ret;
}

cfg_return_code_t cfg_partitions_parse(
    json_t *partitions, struct cfg_scheme *scheme)
{
	size_t idx;
	size_t count;
	json_t *partition;
	cfg_return_code_t ret;
	struct cfg_partitions_section *section;

	if (!json_is_array(partitions)) {
		CFG_RC_SET(ret, CFG_RC_INVALID_SECTION_TYPE);
		goto exit;
	}

	count = json_array_size(partitions);
	if (count == 0) {
		CFG_RC_SET(ret, CFG_RC_EMPTY_NODE);
		goto exit;
	}

	section = &scheme->partitions;
	section->objs = calloc(count, sizeof(struct cfg_partition_obj));
	if (!section->objs) {
		CFG_RC_SET(ret, CFG_RC_MEMORY_ALLOCATION_FAILED);
		goto exit;
	}

	json_array_foreach(partitions, idx, partition)
	{
		ret = add_partition(&section->objs[idx], partition);
		if (!CFG_RC_CHECK_SUCCESS(ret))
			goto free_objs;
	}

	section->objs_count = count;

	CFG_RC_SET_SUCCESS(ret);
	goto exit;

free_objs:
	__cfg_partitions_destroy(section->objs, idx);
	free(section->objs);
exit:
	return ret;
}
