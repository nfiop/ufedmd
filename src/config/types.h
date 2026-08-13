/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2026 Liav A
 */

#ifndef __YAML__TYPES_H_
#define __YAML__TYPES_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct yaml_string_param {
	char *str;
	size_t len;
} yaml_str_param_t;

struct key_value_pair {
	yaml_str_param_t key;
	yaml_str_param_t value;
};

struct yaml_string_sequence {
	yaml_str_param_t **strings;
	size_t strings_count;
};

#endif
