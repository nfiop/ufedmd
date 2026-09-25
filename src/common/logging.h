/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2026 Liav A
 */

#ifndef __COMMON_LOG__H_
#define __COMMON_LOG__H_

#include "common/program_params.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

extern struct program_params prog_params;

void hexdump(FILE *out, const void *buf, size_t len);

#define VLOG(level, out, fmt, ...)                                             \
	do {                                                                   \
		if (prog_params.verbose >= (level))                            \
			fprintf((out), fmt, ##__VA_ARGS__);                    \
	} while (0)
#define VHEXDUMP(level, out, buf, len)                                         \
	do {                                                                   \
		if (prog_params.verbose >= (level))                            \
			hexdump((out), buf, len);                              \
	} while (0)

#endif
