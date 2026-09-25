/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2026 Liav A
 */

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <assert.h>

#include "common/array.h"
#include "common/hashmap.h"

#include "common/ints.h"
#include "pipeline/context.h"
#include "pipeline/scheme.h"

struct codec_initializer {
	char *name;
	ufedmd_rc_t (*init)(pipeline_codec_t *, struct proxy_mtd_info *mtd_info,
	    struct cfg_dict *config);
};

extern ufedmd_rc_t init_hamming_codec(pipeline_codec_t *base,
    struct proxy_mtd_info *mtd_info, struct cfg_dict *config);

static const struct codec_initializer s_initializers[] = {
    {.name = "hamming", .init = init_hamming_codec},
};

static codec_transform_rc_t nack_codec_write(
    struct pipeline_codec *codec, write_codec_context_t *context)
{
	codec_transform_rc_t ret;
	UNUSED(codec);
	UNUSED(context);
	CODEC_ANSWER_NACK_WITH_RC(ret, CODEC_RC_UNKNOWN_ALGORITHM_ERR);
	return ret;
}

static codec_transform_rc_t nack_codec_read(
    struct pipeline_codec *codec, read_codec_context_t *context)
{
	codec_transform_rc_t ret;
	UNUSED(codec);
	UNUSED(context);
	CODEC_ANSWER_NACK_WITH_RC(ret, CODEC_RC_UNKNOWN_ALGORITHM_ERR);
	return ret;
}

static void nack_codec_deinit(struct pipeline_codec *codec)
{
	UNUSED(codec);
}

static bool nack_needs_source_span(void)
{
	/* Nobody should have call this. We declare this method
	 * for completeness.
	 */
	assert(false);
	return true;
}

static bool nack_needs_oob_span(void)
{
	/* Nobody should have call this. We declare this method
	 * for completeness.
	 */
	assert(false);
	return true;
}

static bool nack_validate_spans_size_sufficient(
    struct pipeline_codec *codec, size_t span1_size, size_t span2_size)
{
	UNUSED(codec);
	UNUSED(span1_size);
	UNUSED(span2_size);
	return true;
}

static struct write_ops nack_write_ops = {
    .on_write = nack_codec_write,
    .validate_spans_size_sufficient = nack_validate_spans_size_sufficient,

    /* Static methods */
    .needs_source_span = nack_needs_source_span,
};

static struct read_ops nack_read_ops = {
    .on_read = nack_codec_read,
    .validate_spans_size_sufficient = nack_validate_spans_size_sufficient,

    /* Static methods */
    .needs_oob_span = nack_needs_oob_span,
};

static pipeline_codec_t nack_codec = {
    .name = "Automatic NACK",
    .type = "",
    .write_ops = &nack_write_ops,
    .read_ops = &nack_read_ops,
    .deinit = nack_codec_deinit,
    .priv = NULL,
};

/* 16 KiB is the biggest NAND page we have so far in the world for the
 * _biggest_ NAND chips in the market. We might adjust the buffer size
 * in runtime (i.e. shrink it) if needed.
 */
static uint8_t nack_dummy_span_buf[16384];

static pipeline_page_layout_part_t dummy_part = {
    .name = "",
    .part_idx = 0,
    .in_page_range =
	{
	    .start = 0,
	    .length = sizeof(nack_dummy_span_buf),
	},
};

static pipeline_page_layout_part_t *pipeline0_layout_parts[] = {&dummy_part};

static struct pipeline_page_layout_span nack_codec_layout_span = {
    .name = "",
    .parts =
	{
	    .objs = pipeline0_layout_parts,
	    .parts_count = 1,
	},
    /* We could se this to NULL, but for sake of completeness and keeping
     * everything in one standard, let's just have a dummy buffer.
     */
    .buf = nack_dummy_span_buf,
    .total_len = sizeof(nack_dummy_span_buf),
};

static pipeline_read_codec_context_t nack_read_codec_context = {
    .codec = &nack_codec,

    /* The automatic NACK codec is a special codec - it doesn't have
     * any OOB or data. It is just a placeholder, that NACKs
     * every I/O request automatically. Still, we set a placeholder
     * data span, just so it seems like there's a destination,
     * which simplifies handling of all codecs in general.
     */
    .oob_span = &nack_codec_layout_span,
    .data_span = &nack_codec_layout_span,
};

static pipeline_write_codec_context_t nack_write_codec_context = {
    .codec = &nack_codec,

    /* The automatic NACK codec is a special codec - it doesn't have
     * any source or destination. It is just a placeholder, that NACKs
     * every I/O request automatically. Still, we set a placeholder
     * destination span, just so it seems like there's a destination,
     * which simplifies handling of all codecs in general.
     */
    .src_span = NULL,
    .dest_span = &nack_codec_layout_span,
};

static pipeline_read_codec_context_t *pipeline0_read_codecs[] = {
    &nack_read_codec_context};
static pipeline_write_codec_context_t *pipeline0_write_codecs[] = {
    &nack_write_codec_context};

static pipeline_t pipeline0 = {
    .name = "Automatic NACK pipeline",
    .idx = AUTOMATIC_NACK_PIPELINE_INDEX,
    .page_layout =
	{
	    .spans = NULL,
	    .parts =
		{
		    .objs = pipeline0_layout_parts,
		    .parts_count = 1,
		},
	},
    .read_codecs =
	{
	    .codecs = pipeline0_read_codecs,
	    .codecs_count = 1,
	},
    .write_codecs =
	{
	    .codecs = pipeline0_write_codecs,
	    .codecs_count = 1,
	},
};

static int pipeline_page_layout_span_compare(
    const void *a, const void *b, void *udata)
{
	UNUSED(udata);
	const struct pipeline_page_layout_span *ua = a;
	const struct pipeline_page_layout_span *ub = b;
	return strcmp(ua->name, ub->name);
}

static uint64_t pipeline_page_layout_span_hash(
    const void *item, uint64_t seed0, uint64_t seed1)
{
	const struct pipeline_page_layout_span *span = item;
	return hashmap_sip(span->name, strlen(span->name), seed0, seed1);
}

static void pipeline_page_layout_span_destroy(void *item)
{
	/* During initialization, some parts of the struct
	 * are not immediately set. We should be careful about
	 * free'ing stuff.
	 */
	struct pipeline_page_layout_span *span = item;
	if (span->buf)
		free(span->buf);

	/* The span doesn't own the parts that "belong" to it.
	 * The parts are owned by the page layout struct, which will
	 * take care of them when it is destroyed.
	 */
	if (span->parts.objs)
		free(span->parts.objs);

	free(span->name);
}

static ufedmd_rc_t fixup_pipeline0(void)
{
	ufedmd_rc_t ret;
	void *hashmap_ret;

	// We don't set a destroy function. These structs are not
	// heap-allocated.
	pipeline0.page_layout.spans =
	    hashmap_new(sizeof(struct pipeline_page_layout_span), 1, 0, 0,
		pipeline_page_layout_span_hash,
		pipeline_page_layout_span_compare, NULL, NULL);
	if (!pipeline0.page_layout.spans) {
		UFEDMD_RC_SET(ret, UFEDMD_RC_MEMORY_ALLOCATION_FAILED);
		goto exit;
	}

	/* This part is very unlikely to fail, ever. An assertion is probably
	 * an overkill, still, we don't need to worry about this part here.
	 * Thus, we __could__ clean the memory allocation, but this seems overly
	 * unnecessary as the program will exit soon anyway. So for this
	 * function, we have an unusual exception on memory free'ing rule on
	 * failure.
	 */
	hashmap_ret = (void *)hashmap_set(
	    pipeline0.page_layout.spans, &nack_codec_layout_span);
	if (!hashmap_ret && hashmap_oom(pipeline0.page_layout.spans)) {
		UFEDMD_RC_SET(
		    ret, UFEDMD_RC_HASHTABLE_MEMORY_ALLOCATION_FAILED);
		goto exit;
	} else if (hashmap_ret) {
		UFEDMD_RC_SET(ret, UFEDMD_RC_HASHTABLE_HAS_EXISTING_ENTRY);
		goto exit;
	}

	UFEDMD_RC_SET_SUCCESS(ret);
exit:
	return ret;
}

static int codec_compare(const void *a, const void *b, void *udata)
{
	UNUSED(udata);
	const pipeline_codec_t *ua = a;
	const pipeline_codec_t *ub = b;
	return strcmp(ua->name, ub->name);
}

static uint64_t codec_hash(const void *item, uint64_t seed0, uint64_t seed1)
{
	const pipeline_codec_t *codec = item;
	return hashmap_sip(codec->name, strlen(codec->name), seed0, seed1);
}

static void codec_destroy(void *item)
{
	pipeline_codec_t *codec = item;
	free(codec->name);
	free(codec->type);
}

static ufedmd_rc_t initialize_codec_internals(pipeline_codec_t *codec,
    struct proxy_mtd_info *mtd_info, struct cfg_dict *dict)
{
	ufedmd_rc_t ret;
	size_t idx;

	for (idx = 0; idx < ARRAY_SIZE(s_initializers); idx++) {
		if (!strcmp(s_initializers[idx].name, codec->type)) {
			ret = s_initializers[idx].init(codec, mtd_info, dict);
			goto exit;
		}
	}

	UFEDMD_RC_SET(ret, UFEDMD_RC_CODEC_TYPE_NOT_FOUND);
exit:
	return ret;
}

static ufedmd_rc_t initialize_codecs(struct nand_pipeline_scheme *scheme,
    struct proxy_mtd_info *mtd_info, struct cfg_codecs_section *codecs)
{
	size_t idx;
	ufedmd_rc_t ret;
	struct cfg_codec_obj *cfg_objs, *cfg_cur_obj;
	void *hashmap_ret;
	pipeline_codec_t codec;

	/* This probably can never happen due to the fact that we check
	 * if the section has a sequence inside of it and fail otherwise,
	 * but it doesn't hurt anything to check again.
	 */
	if (codecs->objs_count == 0) {
		UFEDMD_RC_SET(ret, UFEDMD_RC_HAS_NO_CODECS);
		goto exit;
	}

	scheme->codecs =
	    hashmap_new(sizeof(pipeline_codec_t), codecs->objs_count, 0, 0,
		codec_hash, codec_compare, codec_destroy, NULL);

	if (!scheme->codecs) {
		UFEDMD_RC_SET(ret, UFEDMD_RC_MEMORY_ALLOCATION_FAILED);
		goto exit;
	}

	cfg_objs = codecs->objs;

	for (idx = 0; idx < codecs->objs_count; idx++) {
		cfg_cur_obj = &cfg_objs[idx];

		codec.name = strdup((const char *)cfg_cur_obj->name.str);
		if (!codec.name) {
			UFEDMD_RC_SET(ret, UFEDMD_RC_MEMORY_ALLOCATION_FAILED);
			goto free_entries;
		}

		codec.type = strdup((const char *)cfg_cur_obj->type.str);
		if (!codec.type) {
			UFEDMD_RC_SET(ret, UFEDMD_RC_MEMORY_ALLOCATION_FAILED);
			free(codec.name);
			goto free_entries;
		}

		ret = initialize_codec_internals(
		    &codec, mtd_info, &cfg_cur_obj->special_params);
		if (!UFEDMD_RC_CHECK_SUCCESS(ret)) {
			free(codec.name);
			free(codec.type);
			goto free_entries;
		}

		hashmap_ret = (void *)hashmap_set(scheme->codecs, &codec);
		if (!hashmap_ret && hashmap_oom(scheme->codecs)) {
			UFEDMD_RC_SET(
			    ret, UFEDMD_RC_HASHTABLE_MEMORY_ALLOCATION_FAILED);
			codec_destroy(&codec);
			goto free_entries;
		} else if (hashmap_ret) {
			UFEDMD_RC_SET(
			    ret, UFEDMD_RC_HASHTABLE_HAS_EXISTING_ENTRY);
			codec_destroy(&codec);
			goto free_entries;
		}
	}

	UFEDMD_RC_SET_SUCCESS(ret);
	goto exit;

free_entries:
	hashmap_free(scheme->codecs);
exit:
	return ret;
}

static int pipeline_compare(const void *a, const void *b, void *udata)
{
	UNUSED(udata);
	const pipeline_t **ua = (const pipeline_t **)a;
	const pipeline_t *pipeline1 = *ua;
	const pipeline_t **ub = (const pipeline_t **)b;
	const pipeline_t *pipeline2 = *ub;
	return strcmp(pipeline1->name, pipeline2->name);
}

static uint64_t pipeline_hash(const void *item, uint64_t seed0, uint64_t seed1)
{
	const pipeline_t **pipelinep = (const pipeline_t **)item;
	const pipeline_t *pipeline = *pipelinep;
	return hashmap_sip(
	    pipeline->name, strlen(pipeline->name), seed0, seed1);
}

static void __pipeline_destroy(void *item)
{
	/* The hashmap has pointers to the pipeline objects -
	 * it doesn't actually _own_ any of the objects.
	 */
	UNUSED(item);
}

static void destroy_layout_parts(
    struct pipeline_page_layout *layout, size_t max_idx)
{
	size_t idx;
	for (idx = 0; idx < max_idx; idx++) {
		free(layout->parts.objs[idx]->name);
		free(layout->parts.objs[idx]);
	}
	free(layout->parts.objs);
}

static void pipeline_page_layout_destroy(struct pipeline_page_layout *layout)
{
	hashmap_free(layout->spans);
	destroy_layout_parts(layout, layout->parts.parts_count);
}

static void destroy_pipeline_read_codec_contexts(
    struct read_codec_contexts_set *set, size_t max_idx)
{
	/* The pipeline_read_codec_context_t struct doesn't own any objects
	 * so we only release its memory storage.
	 */
	size_t idx;
	for (idx = 0; idx < max_idx; idx++) {
		free(set->codecs[idx]);
	}
	free(set->codecs);
}

static void destroy_pipeline_write_codec_contexts(
    struct write_codec_contexts_set *set, size_t max_idx)
{
	/* The pipeline_write_codec_context_t struct doesn't own any objects
	 * so we only release its memory storage.
	 */
	size_t idx;
	for (idx = 0; idx < max_idx; idx++) {
		free(set->codecs[idx]);
	}
	free(set->codecs);
}

static void pipeline_destroy(pipeline_t *pipeline)
{
	free(pipeline->name);
	destroy_pipeline_read_codec_contexts(
	    &pipeline->read_codecs, pipeline->read_codecs.codecs_count);

	if (pipeline->write_codecs.codecs) {
		destroy_pipeline_write_codec_contexts(&pipeline->write_codecs,
		    pipeline->write_codecs.codecs_count);
	}

	pipeline_page_layout_destroy(&pipeline->page_layout);
}

static ufedmd_rc_t add_pipeline_read_codec(pipeline_t *pipeline,
    struct cfg_read_codec_specifier_obj *specifier, pipeline_codec_t *found,
    pipeline_read_codec_context_t **codec_contextp)
{
	ufedmd_rc_t ret;
	struct pipeline_page_layout_span span;
	struct pipeline_page_layout_span *oob_span = NULL;
	size_t used_oob_span_size = 0;
	struct pipeline_page_layout_span *data_span;
	pipeline_read_codec_context_t *context;
	void *hashmap_ret;

	if (!found->read_ops) {
		UFEDMD_RC_SET(ret, UFEDMD_RC_CODEC_NOT_SUPPORTING_METHOD);
		goto exit;
	}

	context = malloc(sizeof(pipeline_read_codec_context_t));
	if (context == NULL) {
		UFEDMD_RC_SET(ret, UFEDMD_RC_MEMORY_ALLOCATION_FAILED);
		goto exit;
	}

	if (found->read_ops->needs_oob_span()) {
		if (!specifier->has_valid_oob_specifier) {
			UFEDMD_RC_SET(ret, UFEDMD_RC_MISSING_OOB_SPAN);
			goto free_context;
		}

		span.name = (char *)specifier->oob.span_name.str;
		hashmap_ret =
		    (void *)hashmap_get(pipeline->page_layout.spans, &span);
		if (!hashmap_ret) {
			UFEDMD_RC_SET(ret, UFEDMD_RC_PIPELINE_SPAN_NOT_FOUND);
			goto free_context;
		}

		oob_span = hashmap_ret;
		if (validate_range_length_within_span(
			oob_span->total_len, &specifier->oob._range) < 0) {
			UFEDMD_RC_SET(ret,
			    UFEDMD_RC_PIPELINE_SPAN_LENGTH_BOUNDARY_INVALID);
		}

		copy_range_object(
		    &specifier->oob._range, &context->in_oob_span_range);

		used_oob_span_size = context->in_oob_span_range.length;
	}

	span.name = (char *)specifier->data.span_name.str;
	hashmap_ret = (void *)hashmap_get(pipeline->page_layout.spans, &span);
	if (!hashmap_ret) {
		UFEDMD_RC_SET(ret, UFEDMD_RC_PIPELINE_SPAN_NOT_FOUND);
		goto free_context;
	}

	data_span = hashmap_ret;
	if (validate_range_length_within_span(
		data_span->total_len, &specifier->data._range) < 0) {
		UFEDMD_RC_SET(
		    ret, UFEDMD_RC_PIPELINE_SPAN_LENGTH_BOUNDARY_INVALID);
	}

	copy_range_object(
	    &specifier->data._range, &context->in_data_span_range);

	/* Finally, check that the codec is fine with lengths */
	if (!found->read_ops->validate_spans_size_sufficient(found,
		context->in_data_span_range.length, used_oob_span_size)) {
		UFEDMD_RC_SET(
		    ret, UFEDMD_RC_PIPELINE_SPAN_LENGTH_BOUNDARY_INVALID);
		goto free_context;
	}

	context->codec = found;
	context->data_span = data_span;
	context->oob_span = oob_span;
	*codec_contextp = context;

	UFEDMD_RC_SET_SUCCESS(ret);
free_context:
	free(context);
exit:
	return ret;
}

static ufedmd_rc_t add_pipeline_write_codec(pipeline_t *pipeline,
    struct cfg_write_codec_specifier_obj *specifier, pipeline_codec_t *found,
    pipeline_write_codec_context_t **codec_contextp)
{
	ufedmd_rc_t ret;
	struct pipeline_page_layout_span span;
	struct pipeline_page_layout_span *src_span = NULL;
	size_t used_src_span_size = 0;
	struct pipeline_page_layout_span *dest_span;
	pipeline_write_codec_context_t *context;
	void *hashmap_ret;

	if (!found->write_ops) {
		UFEDMD_RC_SET(ret, UFEDMD_RC_CODEC_NOT_SUPPORTING_METHOD);
		goto exit;
	}

	context = malloc(sizeof(pipeline_write_codec_context_t));
	if (context == NULL) {
		UFEDMD_RC_SET(ret, UFEDMD_RC_MEMORY_ALLOCATION_FAILED);
		goto exit;
	}

	if (found->write_ops->needs_source_span()) {
		if (!specifier->src) {
			UFEDMD_RC_SET(ret, UFEDMD_RC_MISSING_SOURCE_SPAN);
			goto free_context;
		}

		span.name = (char *)specifier->src->span_name.str;
		hashmap_ret =
		    (void *)hashmap_get(pipeline->page_layout.spans, &span);
		if (!hashmap_ret) {
			UFEDMD_RC_SET(ret, UFEDMD_RC_PIPELINE_SPAN_NOT_FOUND);
			goto free_context;
		}

		src_span = hashmap_ret;
		if (validate_range_length_within_span(
			src_span->total_len, &specifier->src->_range) < 0) {
			UFEDMD_RC_SET(ret,
			    UFEDMD_RC_PIPELINE_SPAN_LENGTH_BOUNDARY_INVALID);
		}

		copy_range_object(
		    &specifier->src->_range, &context->in_src_span_range);

		used_src_span_size = context->in_src_span_range.length;
	}

	span.name = (char *)specifier->dest.span_name.str;
	hashmap_ret = (void *)hashmap_get(pipeline->page_layout.spans, &span);
	if (!hashmap_ret) {
		UFEDMD_RC_SET(ret, UFEDMD_RC_PIPELINE_SPAN_NOT_FOUND);
		goto free_context;
	}

	dest_span = hashmap_ret;
	if (validate_range_length_within_span(
		dest_span->total_len, &specifier->dest._range) < 0) {
		UFEDMD_RC_SET(
		    ret, UFEDMD_RC_PIPELINE_SPAN_LENGTH_BOUNDARY_INVALID);
	}

	copy_range_object(
	    &specifier->dest._range, &context->in_dest_span_range);

	/* Finally, check that the codec is fine with lengths */
	if (!found->write_ops->validate_spans_size_sufficient(found,
		context->in_dest_span_range.length, used_src_span_size)) {
		UFEDMD_RC_SET(
		    ret, UFEDMD_RC_PIPELINE_SPAN_LENGTH_BOUNDARY_INVALID);
		goto free_context;
	}

	context->codec = found;
	context->dest_span = dest_span;
	context->src_span = src_span;
	*codec_contextp = context;

	UFEDMD_RC_SET_SUCCESS(ret);
free_context:
	free(context);
exit:
	return ret;
}

static ufedmd_rc_t add_pipeline_read_codecs(pipeline_t *pipeline,
    struct cfg_read_codecs_sequence *codec_specifiers,
    struct hashmap *codecs_map)
{
	ufedmd_rc_t ret;
	size_t idx;
	pipeline_codec_t comparee;
	pipeline_codec_t *found;
	struct read_codec_contexts_set *set = &pipeline->read_codecs;

	set->codecs = calloc(codec_specifiers->objs_count,
	    sizeof(pipeline_read_codec_context_t *));
	if (!set->codecs) {
		UFEDMD_RC_SET(ret, UFEDMD_RC_MEMORY_ALLOCATION_FAILED);
		goto exit;
	}

	for (idx = 0; idx < codec_specifiers->objs_count; idx++) {
		comparee.name = codec_specifiers->objs[idx].codec_name.str;
		found = (pipeline_codec_t *)hashmap_get(codecs_map, &comparee);
		if (!found) {
			UFEDMD_RC_SET(ret, UFEDMD_RC_PIPELINE_CODEC_NOT_FOUND);
			goto free_array;
		}

		/* We found a codec, but now we need to verify that we find the
		 * spans it needs for proper execution.
		 */
		ret = add_pipeline_read_codec(pipeline,
		    &codec_specifiers->objs[idx], found, &set->codecs[idx]);
		if (!UFEDMD_RC_CHECK_SUCCESS(ret)) {
			goto free_array;
		}
	}

	set->codecs_count = codec_specifiers->objs_count;
	UFEDMD_RC_SET_SUCCESS(ret);
	goto exit;

free_array:
	destroy_pipeline_read_codec_contexts(set, idx);
exit:
	return ret;
}

static ufedmd_rc_t add_pipeline_write_codecs(pipeline_t *pipeline,
    struct cfg_write_codecs_sequence *codec_specifiers,
    struct hashmap *codecs_map)
{
	ufedmd_rc_t ret;
	size_t idx;
	pipeline_codec_t comparee;
	pipeline_codec_t *found;
	struct write_codec_contexts_set *set = &pipeline->write_codecs;

	set->codecs = calloc(codec_specifiers->objs_count,
	    sizeof(pipeline_write_codec_context_t *));
	if (!set->codecs) {
		UFEDMD_RC_SET(ret, UFEDMD_RC_MEMORY_ALLOCATION_FAILED);
		goto exit;
	}

	for (idx = 0; idx < codec_specifiers->objs_count; idx++) {
		comparee.name = codec_specifiers->objs[idx].codec_name.str;
		found = (pipeline_codec_t *)hashmap_get(codecs_map, &comparee);
		if (!found) {
			UFEDMD_RC_SET(ret, UFEDMD_RC_PIPELINE_CODEC_NOT_FOUND);
			goto free_array;
		}

		/* We found a codec, but now we need to verify that we find the
		 * spans it needs for proper execution.
		 */
		ret = add_pipeline_write_codec(pipeline,
		    &codec_specifiers->objs[idx], found, &set->codecs[idx]);
		if (!UFEDMD_RC_CHECK_SUCCESS(ret)) {
			goto free_array;
		}
	}

	set->codecs_count = codec_specifiers->objs_count;
	UFEDMD_RC_SET_SUCCESS(ret);
	goto exit;

free_array:
	destroy_pipeline_write_codec_contexts(set, idx);
exit:
	return ret;
}

static int compare_pipeline_page_layout_parts(const void *a, const void *b)
{
	const pipeline_page_layout_part_t **part1p =
	    (const pipeline_page_layout_part_t **)a;
	const pipeline_page_layout_part_t **part2p =
	    (const pipeline_page_layout_part_t **)b;

	const pipeline_page_layout_part_t *part1 =
	    *(const pipeline_page_layout_part_t **)part1p;
	const pipeline_page_layout_part_t *part2 =
	    *(const pipeline_page_layout_part_t **)part2p;

	if (part1->in_page_range.start < part2->in_page_range.start)
		return -1;
	if (part1->in_page_range.start > part2->in_page_range.start)
		return 1;
	return 0;
}

struct page_layout_span_initializer {
	pipeline_page_layout_part_t *prev_item;
	/* This is used during initialization */
	size_t part_idx;
};

static bool check_non_overlapping_sorted(
    pipeline_page_layout_part_t *part1, pipeline_page_layout_part_t *part2)
{
	/* Because part1 < part2, it's generally safe & enough to check that
	 * part2 start is at least part1 start + length.
	 */
	return part2->in_page_range.start >=
	       part1->in_page_range.start + part1->in_page_range.length;
	return true;
}

static bool check_span_name_valid(const char *span_name, size_t namelen)
{
	/* TODO: We assume ASCII names, we should probably make it clear
	 * somehow.
	 */
	assert(span_name != NULL);
	assert(namelen != 0);

	size_t ch_idx, tmp_idx;
	char ch;
	const char *allowed_special_chars = "!@#$\%^&*()+-_";
	bool contains_allowed_char;

	for (ch_idx = 0; ch_idx < namelen; ch_idx++) {
		ch = span_name[ch_idx];
		/* Allow only -
		 * 1. A-Z, a-z
		 * 2. 0-9
		 * 3. !@#$%^&*()?+_-
		 */
		if ((uint8_t)ch >= (uint8_t)'a' && (uint8_t)ch <= 'z')
			continue;
		if ((uint8_t)ch >= (uint8_t)'A' && (uint8_t)ch <= 'Z')
			continue;
		if ((uint8_t)ch >= (uint8_t)'0' && (uint8_t)ch <= (uint8_t)'9')
			continue;
		contains_allowed_char = false;
		for (tmp_idx = 0; tmp_idx < ARRAY_SIZE(allowed_special_chars);
		    tmp_idx++) {
			if (allowed_special_chars[tmp_idx] == ch) {
				contains_allowed_char = true;
				break;
			}
		}
		if (contains_allowed_char)
			continue;
		return false;
	}

	return true;
}

static ufedmd_rc_t find_or_add_new_span(struct hashmap *spans,
    struct pipeline_page_layout_span **spanp, const char *span_name,
    bool *new_span_added)
{
	ufedmd_rc_t ret;
	void *hashmap_ret;
	size_t namelen;
	struct pipeline_page_layout_span span;

	assert(spanp != NULL);

	if (span_name != NULL) {
		namelen = strlen(span_name);
		if (namelen == 0) {
			UFEDMD_RC_SET(ret, UFEDMD_RC_LAYOUT_PART_HAS_NO_SPAN);
			goto exit;
		}
	} else {
		UFEDMD_RC_SET(ret, UFEDMD_RC_LAYOUT_PART_HAS_NO_SPAN);
		goto exit;
	}

	if (!check_span_name_valid(span_name, namelen)) {
		UFEDMD_RC_SET(ret, UFEDMD_RC_LAYOUT_PART_HAS_NO_SPAN);
		goto exit;
	}

	/* First, try to find an existing entry... */
	span.name = (char *)span_name;
	hashmap_ret = (void *)hashmap_get(spans, &span);
	if (hashmap_ret) {
		*new_span_added = false;
		*spanp = hashmap_ret;
		UFEDMD_RC_SET_SUCCESS(ret);
		goto exit;
	}

	/* An entry hasn't been found. Create a new one.
	 * This time, actually allocate the span name on the
	 * new object because it's no longer for comparison.
	 */
	span.name = strdup(span_name);
	if (!span.name) {
		UFEDMD_RC_SET(ret, UFEDMD_RC_MEMORY_ALLOCATION_FAILED);
		goto exit;
	}

	hashmap_ret = (void *)hashmap_set(spans, &span);
	if (!hashmap_ret && hashmap_oom(spans)) {
		UFEDMD_RC_SET(
		    ret, UFEDMD_RC_HASHTABLE_MEMORY_ALLOCATION_FAILED);
		goto exit;
	} else if (hashmap_ret) {
		UFEDMD_RC_SET(ret, UFEDMD_RC_HASHTABLE_HAS_EXISTING_ENTRY);
		goto exit;
	}

	*new_span_added = true;
	UFEDMD_RC_SET_SUCCESS(ret);

exit:
	return ret;
}

bool span_buf_alloc_iter(const void *item, void *udata)
{
	struct pipeline_page_layout_span *span =
	    (struct pipeline_page_layout_span *)item;
	UNUSED(udata);

	/* total_len is created from all parts of the span combined.
	 * Also, we verify that each part is not out of the page boundaries.
	 */
	assert(span->total_len > 0);
	span->buf = malloc(span->total_len);
	return span->buf != NULL;
}

static bool verify_page_layout_part_not_out_of_range(
    struct proxy_mtd_info *mtd_info, size_t part_start, size_t part_length)
{
	if ((part_start + part_length) > mtd_info->flash_page_size)
		return false;
	return true;
}

static ufedmd_rc_t add_pipeline_layout_parts(pipeline_t *pipeline,
    struct proxy_mtd_info *mtd_info,
    struct cfg_page_layout_obj *cfg_page_layout)
{
	/* We are very restrictive on 2 rules in this function -
	 * The first rule is no parts of a page layout may overlap, under any
	 * condition - the user can make up any list of layout parts and spans
	 * as needed, but none of them may overlap on another.
	 * This also implies that no part could have a length of 0, as it
	 * defeats the point of no overlapping parts' rule.
	 *
	 * The second rule is that no layout part exists out-of-bounds in
	 * terms of the NAND page geometry. For example, if you have a NAND
	 * chip with 2048 bytes of data and 64 OOB bytes, you cannot have a
	 * layout part that exceeds the limit of 2112 bytes in total (so if
	 * you specify, for example, an offset of 2100 and length of 16 bytes
	 * that would be invalid).
	 *
	 * These rules are enforced to keep everything sane.
	 *
	 * Besides that, we are very permissive on what the user can actually
	 * do - it can give any name it wants for a span, or a part.
	 *
	 * The user might even neglect to specify all parts of a NAND page,
	 * so essentially these are considered "holes".
	 * Although this is allowed, it's highly discouagred for a production
	 * usage to leave holes in a NAND page layout of a pipeline.
	 */

	ufedmd_rc_t ret;
	bool new_span_added;
	size_t part_idx = 0, spans_count;
	struct pipeline_page_layout *new_layout;
	pipeline_page_layout_part_t *part;
	struct cfg_page_layout_part_obj *cfg_page_layout_part;
	struct page_layout_span_initializer *span_initializers,
	    *span_initializer;
	struct pipeline_page_layout_span *span;

	new_layout = &pipeline->page_layout;
	new_layout->spans = hashmap_new(
	    sizeof(struct pipeline_page_layout_span), 0, 0, 0,
	    pipeline_page_layout_span_hash, pipeline_page_layout_span_compare,
	    pipeline_page_layout_span_destroy, NULL);
	if (!new_layout->spans) {
		UFEDMD_RC_SET(ret, UFEDMD_RC_MEMORY_ALLOCATION_FAILED);
		goto exit;
	}

	new_layout->parts.objs = calloc(cfg_page_layout->parts_count,
	    sizeof(pipeline_page_layout_part_t *));
	if (!new_layout->parts.objs) {
		UFEDMD_RC_SET(ret, UFEDMD_RC_MEMORY_ALLOCATION_FAILED);
		goto free_parts;
	}

	new_layout->parts.parts_count = cfg_page_layout->parts_count;

	/* Add all page layout parts, if there's a new span being mentioned
	 * then create a new entry in the hashmap and raise the spans_count.
	 *
	 * When we create a new part, we will put the pointer of the span we got
	 * from new/existing hashmap entry as the parent_span.
	 */
	spans_count = 0;
	for (part_idx = 0; part_idx < cfg_page_layout->parts_count;
	    part_idx++) {
		if (!verify_page_layout_part_not_out_of_range(mtd_info,
			cfg_page_layout_part->_range.start,
			cfg_page_layout_part->_range.length)) {
			UFEDMD_RC_SET(
			    ret, UFEDMD_RC_PAGE_LAYOUT_PART_OUT_OF_RANGE);
			goto free_parts;
		}

		part = calloc(1, sizeof(pipeline_page_layout_part_t));
		if (!part) {
			UFEDMD_RC_SET(ret, UFEDMD_RC_MEMORY_ALLOCATION_FAILED);
			goto free_parts;
		}

		cfg_page_layout_part = &cfg_page_layout->parts[part_idx];
		part->name =
		    strdup((const char *)cfg_page_layout_part->part_name.str);
		if (!part->name) {
			free(part);
			UFEDMD_RC_SET(ret, UFEDMD_RC_MEMORY_ALLOCATION_FAILED);
			goto free_parts;
		}

		/* We could technically allow a part to have no span
		 * but that would make it more harder to reason about
		 * other parts later on. It does cost nothing to specify
		 * a _bogus_ name for a span, so let's keep it that way,
		 * so configuration stays simple.
		 */
		ret =
		    find_or_add_new_span(new_layout->spans, &part->parent_span,
			cfg_page_layout_part->span_name.str, &new_span_added);
		if (!UFEDMD_RC_CHECK_SUCCESS(ret)) {
			free(part->name);
			free(part);
			goto free_parts;
		}

		assert(part->parent_span != NULL);

		if (new_span_added)
			spans_count++;

		part->parent_span->parts.parts_count++;

		part->span_idx = part->parent_span->idx;

		part->in_page_range.start = cfg_page_layout_part->_range.start;
		part->in_page_range.length =
		    cfg_page_layout_part->_range.length;

		part->part_idx = part_idx;
		new_layout->parts.objs[part_idx] = part;
	}

	/* Sort the list. It will be much easier to handle it for the whole
	 * layout and each span later on. Also, it's array of pointers, so
	 * it's not like we copy entire objects in that sort, but I guess
	 * it's meh anyway.
	 */
	qsort(new_layout->parts.objs, cfg_page_layout->parts_count,
	    sizeof(pipeline_page_layout_part_t *),
	    compare_pipeline_page_layout_parts);

	/* Now that we have all parts in & sorted, let's quickly traverse them -
	 * - We should check that no part is overlapping with another.
	 * - We will fix the next and previous pointers for each span item.
	 * - We will also add the part into the span parts' pointers array.
	 */
	span_initializers =
	    calloc(spans_count, sizeof(struct page_layout_span_initializer));
	if (!span_initializers) {
		UFEDMD_RC_SET(ret, UFEDMD_RC_MEMORY_ALLOCATION_FAILED);
		goto free_parts;
	}

	for (part_idx = 0; part_idx < cfg_page_layout->parts_count;
	    part_idx++) {
		part = new_layout->parts.objs[part_idx];
		/* Check that the current part is not in collision with next
		 * part. Skip this check if we are in the last item.
		 */
		if (part_idx + 1 != cfg_page_layout->parts_count) {
			if (!check_non_overlapping_sorted(
				part, new_layout->parts.objs[part_idx + 1])) {
				UFEDMD_RC_SET(ret,
				    UFEDMD_RC_OVERLAPPING_PAGE_LAYOUT_PARTS);
				goto free_span_initializers;
			}
		}

		span_initializer = &span_initializers[part->span_idx];
		span = part->parent_span;
		/* If not already allocated, create a new parts' array now.
		 * We know the count of parts, because on each part we iterated
		 * upon, we either found an existing span or created a new one,
		 * and incremented that span parts' count.
		 */
		if (span->parts.objs == NULL) {
			span->parts.objs = calloc(span->parts.parts_count,
			    sizeof(pipeline_page_layout_part_t *));
			if (!span->parts.objs) {
				UFEDMD_RC_SET(
				    ret, UFEDMD_RC_MEMORY_ALLOCATION_FAILED);
				goto free_span_initializers;
			}
		}
		span->parts.objs[span_initializer->part_idx] = part;
		span->total_len += part->in_page_range.length;
		span_initializer->part_idx++;

		/* This should be true for the first part in a span*/
		if (span_initializer->prev_item == NULL) {
			span_initializer->prev_item = part;
		} else {
			span_initializer->prev_item->next_span_part = part;
			span_initializer->prev_item = part;
		}
	}

	free(span_initializers);

	/* Finally, construct a buffer for each span so it can be passed
	 * to a codec execution context if needed.
	 */
	if (!hashmap_scan(new_layout->spans, span_buf_alloc_iter, mtd_info)) {
		UFEDMD_RC_SET(ret, UFEDMD_RC_MEMORY_ALLOCATION_FAILED);
		goto free_parts;
	}

	UFEDMD_RC_SET_SUCCESS(ret);
	goto exit;

free_span_initializers:
	free(span_initializers);
free_parts:
	hashmap_free(new_layout->spans);
	destroy_layout_parts(new_layout, part_idx);
exit:
	return ret;
}

static void destroy_pipelines(
    struct nand_pipeline_scheme *scheme, size_t max_idx)
{
	size_t idx;
	for (idx = 0; idx < max_idx; idx++) {
		pipeline_destroy(&scheme->demux.pipelines[idx]);
	}
	free(scheme->demux.pipelines);
}

static ufedmd_rc_t initialize_pipelines(struct nand_pipeline_scheme *scheme,
    struct cfg_pipelines_section *pipelines, struct proxy_mtd_info *mtd_info)
{
	/* Set to 0 so we can exit gracefully if there's
	 * allocation failure, but we didn't set any pipeline yet.
	 */
	size_t idx = 0;

	ufedmd_rc_t ret;
	struct cfg_pipeline_obj *cfg_objs, *cfg_cur_obj;
	pipeline_t *cur;
	void *hashmap_ret;

	/* This probably can never happen due to the fact that we check
	 * if the section has a sequence inside of it and fail otherwise,
	 * but it doesn't hurt anything to check again.
	 */
	if (pipelines->objs_count == 0) {
		UFEDMD_RC_SET(ret, UFEDMD_RC_HAS_NO_PIPELINES);
		goto exit;
	}

	/* There's a place for UINT16_MAX - 1 pipelines.
	 * Check for this and otherwise fail now.
	 */
	if ((pipelines->objs_count) > UINT16_MAX - 1) {
		UFEDMD_RC_SET(ret, UFEDMD_RC_PIPELINE_INDEX_OVERFLOW);
		goto exit;
	}

	scheme->demux.pipelines =
	    calloc(pipelines->objs_count + 1, sizeof(pipeline_t));
	if (!scheme->demux.pipelines) {
		UFEDMD_RC_SET(ret, UFEDMD_RC_MEMORY_ALLOCATION_FAILED);
		goto exit;
	}

	scheme->demux.pipelines_count = pipelines->objs_count;

	scheme->pipelines =
	    hashmap_new(sizeof(pipeline_t *), pipelines->objs_count, 0, 0,
		pipeline_hash, pipeline_compare, __pipeline_destroy, NULL);

	if (!scheme->pipelines) {
		UFEDMD_RC_SET(ret, UFEDMD_RC_MEMORY_ALLOCATION_FAILED);
		goto free_pipelines_array;
	}

	cfg_objs = pipelines->objs;

	/* Pipline index 0 is reserved, and real pipeline index starts at 1
	 */
	for (idx = 1; idx < pipelines->objs_count + 1; idx++) {
		cfg_cur_obj = &cfg_objs[idx];
		cur = &scheme->demux.pipelines[idx];

		cur->name = strdup((const char *)cfg_cur_obj->name.str);
		if (!cur->name) {
			UFEDMD_RC_SET(ret, UFEDMD_RC_MEMORY_ALLOCATION_FAILED);
			goto free_entries;
		}

		/* Before adding the pipeline codecs, we must initialize the
		 * pipeline page layout. That ensures we can also check for
		 * incompatible codecs (for example, codecs that require
		 * non-existing span), or overlapping layout parts.
		 */

		ret = add_pipeline_layout_parts(
		    cur, mtd_info, &cfg_cur_obj->page_layout);
		if (!UFEDMD_RC_CHECK_SUCCESS(ret)) {
			free(cur->name);
			goto free_entries;
		}

		// TODO: Add parsing of span mappings

		ret = add_pipeline_read_codecs(
		    cur, &cfg_cur_obj->read_codecs, scheme->codecs);
		if (!UFEDMD_RC_CHECK_SUCCESS(ret)) {
			free(cur->name);
			goto free_entries;
		}

		if (cfg_cur_obj->write_codecs) {
			ret = add_pipeline_write_codecs(
			    cur, cfg_cur_obj->write_codecs, scheme->codecs);
			if (!UFEDMD_RC_CHECK_SUCCESS(ret)) {
				free(cur->name);
				destroy_pipeline_read_codec_contexts(
				    &cur->read_codecs,
				    cur->read_codecs.codecs_count);
				goto free_entries;
			}
		}

		cur->idx = idx;

		/* Set a new entry to the pointer of the new object in the
		 * hashmap.
		 */
		hashmap_ret = (void *)hashmap_set(scheme->pipelines, cur);
		if (!hashmap_ret && hashmap_oom(scheme->pipelines)) {
			UFEDMD_RC_SET(
			    ret, UFEDMD_RC_HASHTABLE_MEMORY_ALLOCATION_FAILED);
			pipeline_destroy(cur);
			goto free_entries;
		} else if (hashmap_ret) {
			UFEDMD_RC_SET(
			    ret, UFEDMD_RC_HASHTABLE_HAS_EXISTING_ENTRY);
			pipeline_destroy(cur);
			goto free_entries;
		}
	}

	UFEDMD_RC_SET_SUCCESS(ret);
	goto exit;

free_entries:
	hashmap_free(scheme->pipelines);
free_pipelines_array:
	destroy_pipelines(scheme, idx);
exit:
	return ret;
}

static int partition_compare(const void *a, const void *b, void *udata)
{
	UNUSED(udata);
	const partition_t *ua = a;
	const partition_t *ub = b;
	return strcmp(ua->name, ub->name);
}

static uint64_t partition_hash(const void *item, uint64_t seed0, uint64_t seed1)
{
	const partition_t *codec = item;
	return hashmap_sip(codec->name, strlen(codec->name), seed0, seed1);
}

static void partition_destroy(void *item)
{
	partition_t *partition = item;
	free(partition->name);
}

static ufedmd_rc_t add_partition_details(partition_t *partition,
    cfg_str_param_t *pipeline, struct range *eraseblocks, struct range *pages,
    struct hashmap *pipelines_map)
{
	ufedmd_rc_t ret;
	pipeline_t pipeline_comparee;
	pipeline_t **pipelinep;

	pipeline_comparee.name = pipeline->str;
	pipelinep =
	    (pipeline_t **)hashmap_get(pipelines_map, &pipeline_comparee);
	if (!pipelinep) {
		UFEDMD_RC_SET(ret, UFEDMD_RC_PIPELINE_NOT_FOUND);
		goto exit;
	}

	partition->pipeline = *pipelinep;
	assert(partition->pipeline != NULL);

	if (addition_u64_would_overflow(
		partition->eraseblocks.start, partition->eraseblocks.length)) {
		UFEDMD_RC_SET(ret, UFEDMD_RC_ADDITION_WOULD_OVERFLOW);
		goto exit;
	}
	partition->eraseblocks.start = eraseblocks->start;
	partition->eraseblocks.length = eraseblocks->length;

	if (addition_u64_would_overflow(
		partition->pages.start, partition->pages.length)) {
		UFEDMD_RC_SET(ret, UFEDMD_RC_ADDITION_WOULD_OVERFLOW);
		goto exit;
	}

	partition->pages.start = pages->start;
	partition->pages.length = pages->length;

	UFEDMD_RC_SET_SUCCESS(ret);
exit:
	return ret;
}

static ufedmd_rc_t add_partitions_to_scheme_from_cfg_config(
    struct nand_pipeline_scheme *scheme,
    struct cfg_partitions_section *partitions)
{
	size_t idx;
	ufedmd_rc_t ret;
	struct cfg_partition_obj *cfg_objs, *cfg_cur_obj;
	partition_t partition;
	void *hashmap_ret;

	cfg_objs = partitions->objs;

	for (idx = 0; idx < partitions->objs_count; idx++) {
		cfg_cur_obj = &cfg_objs[idx];

		partition.name = strdup((const char *)cfg_cur_obj->name.str);
		if (!partition.name) {
			UFEDMD_RC_SET(ret, UFEDMD_RC_MEMORY_ALLOCATION_FAILED);
			goto exit;
		}

		ret = add_partition_details(&partition, &cfg_cur_obj->pipeline,
		    &cfg_cur_obj->eraseblocks, &cfg_cur_obj->pages,
		    scheme->pipelines);
		if (!UFEDMD_RC_CHECK_SUCCESS(ret)) {
			free(partition.name);
			goto exit;
		}

		hashmap_ret =
		    (void *)hashmap_set(scheme->partitions, &partition);
		if (!hashmap_ret && hashmap_oom(scheme->partitions)) {
			UFEDMD_RC_SET(
			    ret, UFEDMD_RC_HASHTABLE_MEMORY_ALLOCATION_FAILED);
			partition_destroy(&partition);
			goto exit;
		} else if (hashmap_ret) {
			UFEDMD_RC_SET(
			    ret, UFEDMD_RC_HASHTABLE_HAS_EXISTING_ENTRY);
			partition_destroy(&partition);
			goto exit;
		}
	}

	UFEDMD_RC_SET_SUCCESS(ret);

exit:
	return ret;
}

bool partition_range_fix_iter(const void *item, void *udata)
{
	partition_t *partition = (partition_t *)item;
	struct proxy_mtd_info *mtd_info = udata;

	assert(mtd_info->flash_sectors_cnt > 0);
	assert(mtd_info->flash_pages_per_sector_cnt > 0);

	if (partition->eraseblocks.length == 0 || partition->pages.length == 0)
		return false;

	if (partition->eraseblocks.length == UINT64_MAX)
		partition->eraseblocks.length =
		    mtd_info->flash_sectors_cnt - partition->eraseblocks.start;
	if (partition->pages.length == UINT64_MAX)
		partition->pages.length = mtd_info->flash_pages_per_sector_cnt -
					  partition->pages.start;

	if (partition->eraseblocks.start + partition->pages.length >
	    mtd_info->flash_sectors_cnt)
		return false;

	if (partition->eraseblocks.start + partition->eraseblocks.length >
	    mtd_info->flash_sectors_cnt)
		return false;

	return true;
}

/* Ranges that are not specified might have lenghts of UINT64_MAX,
 * which is not _correct_ per the actual MTD scheme and should be
 * corrected in this function.
 * However, a non UINT64_MAX end offset that is out of range should
 * be rejected.
 */
static ufedmd_rc_t validate_partitions_range_limits(
    struct nand_pipeline_scheme *scheme, struct proxy_mtd_info *mtd_info)
{
	ufedmd_rc_t ret;
	if (!hashmap_scan(
		scheme->partitions, partition_range_fix_iter, mtd_info)) {
		UFEDMD_RC_SET(ret, UFEDMD_RC_PARTITION_BOUNDARY_VIOLATION);
		goto exit;
	}

	UFEDMD_RC_SET_SUCCESS(ret);
exit:
	return ret;
}

static ufedmd_rc_t initialize_partitions(struct nand_pipeline_scheme *scheme,
    struct cfg_partitions_section *partitions, struct proxy_mtd_info *mtd_info)
{
	ufedmd_rc_t ret;
	size_t partitions_count = partitions->objs_count;

	/* FIXME: Can this ever happen? */
	if (partitions_count == 0) {
		UFEDMD_RC_SET(ret, UFEDMD_RC_HAS_NO_PARTITIONS);
		goto exit;
	}

	scheme->partitions = hashmap_new(sizeof(partition_t), partitions_count,
	    0, 0, partition_hash, partition_compare, partition_destroy, NULL);

	if (!scheme->partitions) {
		UFEDMD_RC_SET(ret, UFEDMD_RC_MEMORY_ALLOCATION_FAILED);
		goto exit;
	}

	/* I actually _considered_ adding an optimization path (from the era of
	 * using libyaml) so the user can omit the partitions section
	 * altogether, if they want that our program to work on "placeholder"
	 * partition, which spans the entire NAND flash chip. I realized this is
	 * a bad idea, and keeping the JSON configuration file in a consistent &
	 * known format is probably a **good** idea.
	 *
	 * Therefore, even for one partition, the user must specify the actual
	 * partition where they want this program to operate upon. No
	 * exceptions, no sneaky rules - this is the **only** to keep
	 * configuration and parsing somewhat sane.
	 */
	ret = add_partitions_to_scheme_from_cfg_config(scheme, partitions);
	if (!UFEDMD_RC_CHECK_SUCCESS(ret)) {
		goto free_partitions;
	}

	ret = validate_partitions_range_limits(scheme, mtd_info);
	if (!UFEDMD_RC_CHECK_SUCCESS(ret)) {
		goto free_partitions;
	}

	UFEDMD_RC_SET_SUCCESS(ret);
	goto exit;

free_partitions:
	hashmap_free(scheme->partitions);
exit:
	return ret;
}

static pipeline_t *single_pipeline_select(
    struct io_demux *demux, uint64_t eraseblock_idx, uint64_t page_idx)
{
	UNUSED(eraseblock_idx);
	UNUSED(page_idx);
	return demux->_single_pipeline;
}

static bool partition_get_single_instance(const void *item, void *udata)
{
	partition_t *partition = (partition_t *)item;
	partition_t **partitionp = udata;
	*partitionp = partition;
	return false;
}

static inline bool is_partition_spanning_entire_mtd(
    struct proxy_mtd_info *mtd_info, partition_t *partition)
{
	assert(mtd_info->flash_sectors_cnt > 0);
	assert(mtd_info->flash_pages_per_sector_cnt > 0);
	assert(!addition_u64_would_overflow(
	    partition->eraseblocks.start, partition->eraseblocks.length));
	assert(!addition_u64_would_overflow(
	    partition->pages.start, partition->pages.length));

	bool eraseblocks_start_at_zero = partition->eraseblocks.start == 0;
	bool eraseblocks_end_at_sector_count =
	    partition->eraseblocks.start + partition->eraseblocks.length ==
	    mtd_info->flash_sectors_cnt;

	return eraseblocks_start_at_zero && eraseblocks_end_at_sector_count &&
	       partition->pages.start == 0 &&
	       partition->pages.start + partition->pages.length ==
		   mtd_info->flash_pages_per_sector_cnt;
}

static bool is_partition_spanning_entire_eraseblock(
    const void *item, void *udata)
{
	const partition_t *partition = item;
	struct proxy_mtd_info *mtd_info = udata;
	assert(mtd_info->flash_pages_per_sector_cnt > 0);
	assert(!addition_u64_would_overflow(
	    partition->eraseblocks.start, partition->eraseblocks.length));

	return partition->pages.start == 0 &&
	       partition->pages.start + partition->pages.length ==
		   mtd_info->flash_pages_per_sector_cnt;
}

static pipeline_t *__select_pipeline_per_eraseblock(
    struct io_demux *demux, uint64_t eraseblock_idx, uint64_t page_idx)
{
	UNUSED(page_idx);
	return &demux->pipelines[demux->arr[eraseblock_idx]];
}

static bool set_partition_pipeline_on_array_per_eraseblock(
    const void *item, void *udata)
{
	const partition_t *partition = item;
	struct nand_pipeline_scheme *scheme = udata;
	struct io_demux *demux = &scheme->demux;
	uint64_t eraseblock_idx;

	for (eraseblock_idx = partition->eraseblocks.start;
	    eraseblock_idx <
	    partition->eraseblocks.start + partition->eraseblocks.length;
	    eraseblock_idx++) {
		if (demux->arr[eraseblock_idx] != AUTOMATIC_NACK_PIPELINE_INDEX)
			return false;
		demux->arr[eraseblock_idx] = partition->pipeline->idx;
	}

	return true;
}

static ufedmd_rc_t initialize_pipeline_selector_per_eraseblock(
    struct nand_pipeline_scheme *scheme, struct proxy_mtd_info *mtd_info)
{
	ufedmd_rc_t ret;
	struct io_demux *demux = &scheme->demux;

	assert(mtd_info->flash_sectors_cnt > 0);

	demux->arr =
	    calloc(mtd_info->flash_sectors_cnt, sizeof(pipeline_index_t));
	if (!demux->arr) {
		UFEDMD_RC_SET(ret, UFEDMD_RC_MEMORY_ALLOCATION_FAILED);
		goto exit;
	}

	/* Now try to set the pipeline index on every needed entry -
	 * If it is zero (the automatic NACK pipeline) then we are
	 * allowed to replace it, otherwise we fail.
	 */
	if (!hashmap_scan(scheme->partitions,
		set_partition_pipeline_on_array_per_eraseblock, scheme)) {
		/* Some partitions are overlapping, exit with error! */
		UFEDMD_RC_SET(ret, UFEDMD_RC_OVERLAPPING_PARTITION_RANGES);
		goto free_array;
	}

	/* Finally, set a callback */
	demux->select = __select_pipeline_per_eraseblock;

	UFEDMD_RC_SET_SUCCESS(ret);
	goto exit;

free_array:
	free(demux->arr);
exit:
	return ret;
}

static pipeline_t *__select_pipeline_per_page(
    struct io_demux *demux, uint64_t eraseblock_idx, uint64_t page_idx)
{
	return &demux->pipelines[demux
		->arr[(demux->flash_pages_per_sector_stride * eraseblock_idx) +
		      page_idx]];
}

static bool set_partition_pipeline_on_array_per_page(
    const void *item, void *udata)
{
	const partition_t *partition = item;
	struct nand_pipeline_scheme *scheme = udata;
	struct io_demux *demux = &scheme->demux;
	uint64_t eraseblock_idx;
	uint64_t page_idx;

	for (eraseblock_idx = partition->eraseblocks.start;
	    eraseblock_idx <
	    partition->eraseblocks.start + partition->eraseblocks.length;
	    eraseblock_idx++) {
		for (page_idx = partition->pages.start;
		    page_idx <=
		    partition->pages.start + partition->pages.length;
		    page_idx++) {
			if (demux
				->arr[eraseblock_idx *
					  demux->flash_pages_per_sector_stride +
				      page_idx] !=
			    AUTOMATIC_NACK_PIPELINE_INDEX) {
				return false;
			}
			demux->arr[eraseblock_idx] = partition->pipeline->idx;
		}
	}

	return true;
}

static ufedmd_rc_t initialize_pipeline_selector_per_page(
    struct nand_pipeline_scheme *scheme, struct proxy_mtd_info *mtd_info)
{
	ufedmd_rc_t ret;
	struct io_demux *demux = &scheme->demux;

	assert(mtd_info->flash_sectors_cnt > 0);
	assert(mtd_info->flash_pages_per_sector_cnt > 0);

	demux->flash_pages_per_sector_stride =
	    mtd_info->flash_pages_per_sector_cnt;

	if (multiplication_u64_would_overflow(
		mtd_info->flash_pages_per_sector_cnt,
		mtd_info->flash_sectors_cnt)) {
		UFEDMD_RC_SET(ret, UFEDMD_RC_MULTIPLICATION_WOULD_OVERFLOW);
		goto exit;
	}

	demux->arr = calloc(
	    mtd_info->flash_sectors_cnt * mtd_info->flash_pages_per_sector_cnt,
	    sizeof(pipeline_index_t));
	if (!demux->arr) {
		UFEDMD_RC_SET(ret, UFEDMD_RC_MEMORY_ALLOCATION_FAILED);
		goto exit;
	}

	/* Now try to set the pipeline index on every needed entry -
	 * If it is zero (the automatic NACK pipeline) then we are
	 * allowed to replace it, otherwise we fail.
	 */
	if (!hashmap_scan(scheme->partitions,
		set_partition_pipeline_on_array_per_page, scheme)) {
		/* Some partitions are overlapping, exit with error! */
		UFEDMD_RC_SET(ret, UFEDMD_RC_OVERLAPPING_PARTITION_RANGES);
		goto free_array;
	}

	/* Finally, set a callback */
	demux->select = __select_pipeline_per_page;

	UFEDMD_RC_SET_SUCCESS(ret);
	goto exit;

free_array:
	free(demux->arr);
exit:
	return ret;
}

static ufedmd_rc_t initialize_pipeline_selector(
    struct nand_pipeline_scheme *scheme, struct proxy_mtd_info *mtd_info)
{
	ufedmd_rc_t ret;
	partition_t *partition;

	/* This can't be unless there's a serious bug... */
	assert(hashmap_count(scheme->partitions) != 0);

	/* Optimization path - if there's only one pipeline that lies on the
	 * entire MTD, either because of a single YAML partition object that
	 * is not specifying any range, or lack of any partitions in the
	 * configuration file - we can't set it so there would be a direct
	 * selection of only one pipeline for the whole range.
	 */
	if (hashmap_count(scheme->partitions) == 1) {
		hashmap_scan(scheme->partitions, partition_get_single_instance,
		    &partition);
		assert(partition != NULL);
		if (is_partition_spanning_entire_mtd(mtd_info, partition)) {
			scheme->demux._single_pipeline = partition->pipeline;
			scheme->demux.select = single_pipeline_select;
			UFEDMD_RC_SET_SUCCESS(ret);
			goto exit;
		}
	}

	/* We are now in a path of looking for the pipeline strategy
	 * which is either worse or the worst - either a pipeline for
	 * every eraseblock, or a pipeline for every page in every eraseblock.
	 */
	if (!hashmap_scan(scheme->partitions,
		is_partition_spanning_entire_eraseblock, mtd_info)) {
		/* Some partition is not spanning an entire eraseblock (or
		 * eraseblocks) so we do need to use per-page index.
		 */
		ret = initialize_pipeline_selector_per_page(scheme, mtd_info);
	} else {
		/* Phew... All partitions are set on whole erase blocks. */
		ret = initialize_pipeline_selector_per_eraseblock(
		    scheme, mtd_info);
	}

	UFEDMD_RC_SET_SUCCESS(ret);
exit:
	return ret;
}

void destroy_nand_pipeline_scheme(struct nand_pipeline_scheme *scheme)
{
	size_t pipelines_count = hashmap_count(scheme->pipelines);
	hashmap_free(scheme->partitions);
	hashmap_free(scheme->pipelines);
	destroy_pipelines(scheme, pipelines_count);
	hashmap_free(scheme->codecs);
}

ufedmd_rc_t initialize_nand_pipeline_scheme(struct nand_pipeline_scheme *scheme,
    struct proxy_mtd_info *mtd_info, struct cfg_scheme *cfg_scheme)
{
	ufedmd_rc_t ret;
	size_t pipelines_count;

	ret = initialize_codecs(scheme, mtd_info, &cfg_scheme->codecs);
	if (!UFEDMD_RC_CHECK_SUCCESS(ret)) {
		goto exit;
	}

	/* Pipeline 0 is special - it's not a pipeline that is set by the
	 * configuration file. We need to fixup its spans' hashmap so it
	 * can be used safely.
	 */
	ret = fixup_pipeline0();
	if (!UFEDMD_RC_CHECK_SUCCESS(ret)) {
		goto exit;
	}

	ret = initialize_pipelines(scheme, &cfg_scheme->pipelines, mtd_info);
	if (!UFEDMD_RC_CHECK_SUCCESS(ret)) {
		goto free_codecs;
	}

	pipelines_count = hashmap_count(scheme->pipelines);

	ret = initialize_partitions(scheme, &cfg_scheme->partitions, mtd_info);
	if (!UFEDMD_RC_CHECK_SUCCESS(ret)) {
		goto free_pipelines;
	}

	/* Finally, try to initialize the pipeline selector
	 * which is the heart of the program - that's how we select in
	 * runtime the correct pipeline for a specific page/eraseblock
	 * while serving a request.
	 *
	 * We verify that there are no overlapping partitions and that we pick
	 * the best strategy (memory consumption vs I/O granularity).
	 */
	ret = initialize_pipeline_selector(scheme, mtd_info);
	if (!UFEDMD_RC_CHECK_SUCCESS(ret)) {
		goto free_partitions;
	}

	/* If something needs pipeline with index 0 which is the
	 * the automatic NACK pipeline, let's initialize it now, so
	 * the scheme is complete.
	 */
	memcpy(&scheme->demux.pipelines[0], &pipeline0, sizeof(pipeline_t));

	UFEDMD_RC_SET_SUCCESS(ret);
	goto exit;

free_partitions:
	hashmap_free(scheme->partitions);
free_pipelines:
	hashmap_free(scheme->pipelines);
	destroy_pipelines(scheme, pipelines_count);
free_codecs:
	hashmap_free(scheme->codecs);
exit:
	return ret;
}
