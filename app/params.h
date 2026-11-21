/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2026 Liav A
 */

#ifndef __PARAMS__H_
#define __PARAMS__H_

#include <stdio.h>
#include <unistd.h>

void print_help(const char *prog);
void print_params(void);

int handle_params(int argc, char **argv);

#endif
