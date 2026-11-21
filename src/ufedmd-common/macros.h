/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2026 Liav A
 */

#ifndef __COMMON__MACROS_H_
#define __COMMON__MACROS_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MAX_NAME_LEN (256)

#define UNUSED(__x) (void)__x

/* This macro is inspired from the Linux kernel - it should allocate
 * a memory area and zero it afterwards.
 */
#define zalloc_obj(__x) calloc(1, sizeof(*__x))
#define zalloc(__sizeof) calloc(1, __sizeof)

#endif
