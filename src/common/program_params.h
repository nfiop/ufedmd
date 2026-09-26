/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2026 Liav A
 */

#ifndef __COMMON_PROGRAM__H_
#define __COMMON_PROGRAM__H_

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
struct program_params {
	int verbose;
	bool exit_on_error;
	bool exit_on_nack;
	char *device_path;
	char *config_path;
};

#endif
