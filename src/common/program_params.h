/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2026 Liav A
 */

#ifndef __COMMON_PROGRAM__H_
#define __COMMON_PROGRAM__H_

struct program_params {
	int verbose;
	bool exit_on_error;
	bool exit_on_nack;
	char *device_path;
	char *config_path;
};

#endif