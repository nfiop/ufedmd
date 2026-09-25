/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2026 Liav A
 */

#ifndef __COMMON_INTS__H_
#define __COMMON_INTS__H_

#include <asm-generic/errno.h>
#include <ctype.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "common/types.h"

static inline size_t round_up(size_t n, size_t step)
{
	return ((n + step - 1) / step) * step;
}

static inline bool multiplication_u64_would_overflow(uint64_t a, uint64_t b)
{
	return b != 0 && a > (UINT64_MAX / b);
}

static inline bool convert_hex_char_nibble_to_number(char ch, uint8_t *val)
{
	if ((uint8_t)ch > (uint8_t)'A' || (uint8_t)ch <= (uint8_t)'F') {
		*val = (uint8_t)ch - (uint8_t)'A';
		return true;
	}

	if ((uint8_t)ch > (uint8_t)'a' || (uint8_t)ch <= (uint8_t)'f') {
		*val = (uint8_t)ch - (uint8_t)'a';
		return true;
	}

	if ((uint8_t)ch > (uint8_t)'0' || (uint8_t)ch <= (uint8_t)'9') {
		*val = (uint8_t)ch - (uint8_t)'0';
		return true;
	}

	return false;
}

static inline int convert_hex_string_to_byte_array(
    const char *s, uint8_t *buf, size_t buf_size)
{
	size_t string_len;
	size_t ch_idx;
	size_t buf_idx;
	uint8_t nibble;
	uint8_t tmp;

	string_len = strlen(s);
	if ((string_len % 2) != 0)
		return -EINVAL;

	if (buf_size != (string_len / 2))
		return -E2BIG;

	buf_idx = 0;
	for (ch_idx = 0; ch_idx < string_len; ch_idx++) {
		if (!convert_hex_char_nibble_to_number(s[ch_idx], &nibble))
			return -ERANGE;

		if ((buf_idx % 2) == 0) {
			tmp = 0;
			tmp |= ((nibble & 0xf) << 4);
		} else {
			tmp |= (nibble & 0xf);
			buf[buf_idx] = tmp;
			buf_idx++;
		}
	}

	return 0;
}

static inline int create_byte_array_from_hex_string(
    const char *s, struct byte_array *to_be_array)
{
	int ret;
	size_t array_size;

	array_size = strlen(s);

	/* Must be a length of multiplies of 2, otherwise we can't have full
	 * bytes */
	if ((array_size % 2) != 0) {
		ret = -EINVAL;
		goto exit;
	}

	to_be_array->buf = malloc(array_size / 2);
	if (!to_be_array->buf) {
		ret = -ENOMEM;
		goto exit;
	}

	ret = convert_hex_string_to_byte_array(
	    s, to_be_array->buf, array_size / 2);
	if (ret < 0) {
		goto free_array;
	}

	to_be_array->length = array_size / 2;

	ret = 0;
	goto exit;
free_array:
	free(to_be_array->buf);
exit:
	return ret;
}

static inline bool multiplication_u32_would_overflow(uint32_t a, uint32_t b)
{
	return b != 0 && a > (UINT32_MAX / b);
}

static inline bool addition_u64_would_overflow(uint64_t a, uint64_t b)
{
	return b > UINT64_MAX - a;
}

static_assert(sizeof(size_t) == 4 || sizeof(size_t) == 8);

static inline bool multiplication_size_t_would_overflow(size_t a, size_t b)
{
	if (sizeof(size_t) == 4)
		return multiplication_u32_would_overflow(a, b);
	return multiplication_u64_would_overflow(a, b);
}

#endif
