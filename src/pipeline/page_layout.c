/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2026 Liav A
 */

#include "pipeline/page_layout.h"
#include <asm-generic/errno.h>

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
