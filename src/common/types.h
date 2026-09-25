/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2026 Liav A
 */

#ifndef __COMMON__TYPES_H_
#define __COMMON__TYPES_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MAX_NAME_LEN (256)

#define UNUSED(__x) (void)__x

struct range {
	uint64_t start;
	uint64_t length;
};

static inline void copy_range_object(struct range *from, struct range *to)
{
	to->start = from->start;
	to->length = from->length;
}

struct byte_array {
	uint8_t *buf;
	size_t length;
};

#endif
