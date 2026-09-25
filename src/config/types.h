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

typedef struct cfg_string_param {
	char *str;
	size_t len;
} cfg_str_param_t;

enum cfg_value_param_type {
	CFG_VALUE_PARAM_TYPE_STRING,
	CFG_VALUE_PARAM_TYPE_INTEGER,
	CFG_VALUE_PARAM_TYPE_BOOLEAN,
	CFG_VALUE_PARAM_TYPE_DOUBLE,
};

typedef struct cfg_value_param {
	enum cfg_value_param_type type;
	union {
		bool flag;
		cfg_str_param_t string;
		unsigned int integer;
		double real;
	} value;
} cfg_value_param_t;

struct key_value_pair {
	cfg_str_param_t key;
	cfg_value_param_t value;
};

struct cfg_dict {
	struct key_value_pair *key_val_pairs;
	size_t key_value_pairs_count;
	size_t __used;
};

#endif
