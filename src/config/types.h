/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2026 Liav A
 */

#ifndef __CFG__TYPES_H_
#define __CFG__TYPES_H_

#include <common/types.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "common/hashmap.h"
#include "config/cfg_types.h"

typedef struct cfg_string_param {
	char *str;
	size_t len;
} cfg_str_param_t;

typedef struct cfg_value_param {
	enum cfg_value_param_type type;
	union {
		bool flag;
		cfg_str_param_t string;
		unsigned int u_integer;
		int integer;
		double real;
	} _value;
} cfg_value_param_t;

struct key_value_pair {
	cfg_str_param_t key;
	cfg_value_param_t value;
};

struct cfg_dict {
	struct hashmap *values;
};

#endif
