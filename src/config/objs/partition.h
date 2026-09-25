/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2026 Liav A
 */

#ifndef __CFG__OBJS_PARTITION_H_
#define __CFG__OBJS_PARTITION_H_

#include "common/types.h"
#include "config/types.h"

struct cfg_partition_obj {
	cfg_str_param_t name;
	cfg_str_param_t pipeline;

	struct range eraseblocks;
	struct range pages;
};

#endif
