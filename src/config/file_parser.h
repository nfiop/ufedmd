/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2026 Liav A
 */

#ifndef __YAML_FILE_PARSER_H_
#define __YAML_FILE_PARSER_H_

#include <stdio.h>

#include "config/return_codes.h"
#include "config/yaml_objs.h"

yaml_return_code_t parse_yaml_file(FILE *fp, struct yaml_mtd_scheme *scheme);

#endif
