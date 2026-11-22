/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2026 Liav A
 */

#include "ufedmd-common/macros.h"

#include "pipeline/page_layout.h"

#include <assert.h>
#include <errno.h>
#include <string.h>

int validate_range_length_within_span(
    size_t total_span_length, struct range *range)
{
	if (range->start + range->length > total_span_length)
		return -ERANGE;

	return 0;
}

int calculate_real_range_length_within_span(
    size_t *lenp, size_t total_buf_length, struct range *range)
{
	size_t remaining_length;
	if (range->start > total_buf_length)
		return -ERANGE;

	remaining_length = total_buf_length - range->start;

	/* Equivalent to min(total_buf_length - range->start, range->length) */
	if (remaining_length > range->length) {
		*lenp = range->length;
	} else {
		*lenp = remaining_length;
	}

	return 0;
}

void extract_raw_page_to_pipeline_page_layout(
    struct pipeline_page_layout *page_layout, uint8_t *raw_page_buf,
    size_t buf_size)
{
	size_t idx;
	pipeline_page_layout_part_t *part;
	struct pipeline_page_layout_span *span;

	/* FIXME: We don't check the length of the raw page buffer - it's
	 * already verified in the the scheme initialization from the
	 * configuration file. Adding an assertion would be very good idea here.
	 */
	UNUSED(buf_size);

	/* The parts list is sorted, so we can just increment the index */
	for (idx = 0; idx < page_layout->parts.parts_count; idx++) {
		part = page_layout->parts.objs[idx];
		span = part->parent_span;
		assert(
		    part->in_span_range.length == part->in_page_range.length);
		memcpy(span->buf + part->in_span_range.start,
		    raw_page_buf + part->in_page_range.start,
		    part->in_page_range.length);
	}
}

/* FIXME: We don't check the length of the raw page buffer - it's already
 * verified in the the scheme initialization from the configuration file.
 * Adding an assertion would be very good idea here.
 */
void compose_raw_page_from_pipeline_page_layout(
    struct pipeline_page_layout *page_layout, uint8_t *raw_page_buf,
    size_t buf_size)
{
	// Default of 0xFF for any unused byte is probably a fine compromise.
	memset(raw_page_buf, 0xFF, buf_size);

	size_t idx;
	pipeline_page_layout_part_t *part;
	struct pipeline_page_layout_span *span;

	/* The parts list is sorted, so we can just increment the index */
	for (idx = 0; idx < page_layout->parts.parts_count; idx++) {
		part = page_layout->parts.objs[idx];
		span = part->parent_span;
		assert(
		    part->in_span_range.length == part->in_page_range.length);
		memcpy(raw_page_buf + part->in_page_range.start,
		    span->buf + part->in_span_range.start,
		    part->in_page_range.length);
	}
}