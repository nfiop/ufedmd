/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2026 Liav A
 */

#ifndef __COMMON__BYTE_ARRAY_H_
#define __COMMON__BYTE_ARRAY_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct byte_array {
	uint8_t *buf;
	size_t length;
};

#endif
