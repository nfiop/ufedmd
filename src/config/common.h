/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2026 Liav A
 */

#ifndef __CFG__COMMON__H_
#define __CFG__COMMON__H_

#include "config/return_codes.h"
#include "config/types.h"

#include <jansson.h>

#define RELEASE_JSON_OBJECT(__json_obj)                                        \
	do {                                                                   \
		if (__json_obj)                                                \
			json_decref(__json_obj);                               \
	} while (0)

json_t *get_json_string_by_key(json_t *obj, const char *key);

void print_cfg_scalar_value(const cfg_value_param_t *param);
void destroy_scalar_param(cfg_value_param_t *param);
cfg_return_code_t create_scalar_param(cfg_value_param_t *param, json_t *value);

void fill_range_with_max_length(struct range *range);

cfg_return_code_t parse_bare_range_object(struct range *obj, json_t *range);

void destroy_dict(struct cfg_dict *dict);

void destroy_string_param(cfg_str_param_t *param);
void destroy_optional_string_param(cfg_str_param_t *param);
void destroy_key_value_pair(struct key_value_pair *pair);
cfg_return_code_t adopt_string_param(cfg_str_param_t *param, const char *str);
cfg_return_code_t create_string_param(cfg_str_param_t *param, json_t *value);

const char *return_code_value_to_string(cfg_return_code_enum_t rc);

#endif
