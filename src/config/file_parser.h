/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2026 Liav A
 */

#ifndef __CONFIG_FILE_PARSER_H_
#define __CONFIG_FILE_PARSER_H_

#include <stdio.h>

#include "config/return_codes.h"
#include "config/scheme.h"

void print_codecs_section(struct cfg_codecs_section *codecs);
void print_pipelines_section(struct cfg_pipelines_section *pipelines);
void print_partitions_section(struct cfg_partitions_section *partitions);

cfg_return_code_t parse_config_file(FILE *fp, struct cfg_scheme *scheme);

#endif
