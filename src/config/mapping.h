/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2026 Liav A
 */

#ifndef __CONFIG__MAPPING_H_
#define __CONFIG__MAPPING_H_

#include "config/return_codes.h"

#include <yaml.h>

typedef yaml_return_code_t (*parse_func_t)(
    void *obj, const char *key, yaml_node_t *child_node);
typedef void (*cleanup_func_t)(void *obj, const char *key);

struct mapping_handle {
	void *obj;
	parse_func_t parse_entry;
	cleanup_func_t cleanup_entry;
};

yaml_return_code_t parse_mapping_object(
    yaml_document_t *doc, yaml_node_t *node, struct mapping_handle *handle);

#endif
