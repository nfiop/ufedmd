/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2026 Liav A
 */

#ifndef __YAML__SECTION_H_
#define __YAML__SECTION_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <yaml.h>

#include "config/return_codes.h"
#include "config/yaml_objs.h"

yaml_return_code_t parse_section(yaml_document_t *doc,
    struct yaml_mtd_scheme *scheme, yaml_node_t *key, yaml_node_t *value);

#endif
