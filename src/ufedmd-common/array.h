/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2026 Liav A
 */

#ifndef __COMMON__ARRAY__H_
#define __COMMON__ARRAY__H_

#include <stddef.h>

#define ARRAY_SIZE(_arr) ((size_t)sizeof(_arr) / sizeof(_arr[0]))

#endif