/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2026 Liav A
 */

#ifndef __YAML__COMMON__H_
#define __YAML__COMMON__H_

#include "config/return_codes.h"
#include "config/yaml_objs.h"

#include <yaml.h>

yaml_return_code_t compare_key_scalar_value(
    yaml_node_t *node, int *index, const char **strings, size_t strings_count);
yaml_return_code_t compare_key_string_value(
    const char *string, int *index, const char **strings, size_t strings_count);
yaml_return_code_t compare_key_scalar_one_value(
    yaml_node_t *node, const char *string);

yaml_return_code_t yaml_sequence_get_objects_count(
    yaml_node_t *node, size_t *count);

void destroy_string_param(yaml_str_param_t *param);
void destroy_optional_string_param(yaml_str_param_t *param);
void destroy_key_value_pair(struct key_value_pair *pair);
yaml_return_code_t create_string_param(
    yaml_str_param_t *param, yaml_node_t *value);

yaml_return_code_t append_pair_to_dict(
    struct yaml_dict *dict, const char *key, yaml_node_t *value);
yaml_return_code_t create_key_value_pair(
    struct key_value_pair *pair, const char *key, yaml_node_t *value);
yaml_return_code_t find_key_value_pair(
    struct yaml_dict *dict, struct key_value_pair **pairp, const char *key);

yaml_return_code_t allocate_typed_array_for_section(
    yaml_node_t *node, void **arrp, size_t *objs_countp, size_t struct_size);

yaml_return_code_t allocate_pairs_array(
    struct key_value_pair **arrp, size_t count);
yaml_return_code_t yaml_mapping_get_objects_count(
    yaml_document_t *doc, yaml_node_t *node, size_t *count);

yaml_return_code_t build_str_param_from_node_val(yaml_document_t *doc,
    yaml_str_param_t *str, yaml_node_t *parent_node, yaml_node_pair_t *pair,
    const char *key_name);

#endif
