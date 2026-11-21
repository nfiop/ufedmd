/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2026 Liav A
 */

#ifndef __COMMON__RANGE_H_
#define __COMMON__RANGE_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct range {
	uint64_t start;
	uint64_t length;
};

static inline void copy_range_object(struct range *from, struct range *to)
{
	to->start = from->start;
	to->length = from->length;
}

#endif
