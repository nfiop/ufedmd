/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2026 Liav A
 */

#ifndef __PIPELINE__LAYOUT__H_
#define __PIPELINE__LAYOUT__H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "common/hashmap.h"
#include "common/types.h"

struct pipeline_page_layout_span;

typedef struct pipeline_page_layout_part {
	char *name;
	size_t part_idx;

	/* The parent span and its index. The index is used
	 * mainly during initialization, for quickly building
	 * a list of next and previous pointers in each span part.
	 */
	struct pipeline_page_layout_span *parent_span;
	size_t span_idx;

	struct range in_page_range;

	/* Both pointers can be NULL, if there's nothing after or before
	 * of the part.
	 */
	struct pipeline_page_layout_part *prev_span_part;
	struct pipeline_page_layout_part *next_span_part;
} pipeline_page_layout_part_t;

/* This struct is used in each span and for the whole layout
 * For a whole page layout, objs array holds (and own!) all parts
 * **necessarily** in a consecutive order.
 * For some sort of order we have a "linked list" behavior in each part -
 * the next & previous part pointers.
 */
struct pipeline_page_layout_parts {
	pipeline_page_layout_part_t **objs;
	size_t parts_count;
};

struct pipeline_page_layout_span {
	char *name;
	size_t idx;

	/* The objects in this list are **NOT** owned by this struct.
	 * The ownership of parts belongs to the global layout, and a span
	 * object only has a reference to those parts.
	 */
	struct pipeline_page_layout_parts parts;

	uint8_t *buf;
	size_t total_len;
};

struct pipeline_page_layout {
	struct hashmap *spans;

	/* A list of parts of a pipeline layout owned by the layout
	 * Initially, they're added and might be unsorted.
	 * We then run qsort, so the array is sorted.
	 */
	struct pipeline_page_layout_parts parts;
};

int validate_range_length_within_span(
    size_t total_span_length, struct range *range);

int calculate_real_range_length_within_span(
    size_t *lenp, size_t total_buf_length, struct range *range);

int compose_raw_page_from_pipeline_page_layout(void);

#endif
