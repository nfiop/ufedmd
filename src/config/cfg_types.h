/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2026 Liav A
 */

#ifndef __CFG__CFG_TYPES__H_
#define __CFG__CFG_TYPES__H_

#include <stdint.h>

enum cfg_value_param_type {
	CFG_VALUE_PARAM_TYPE_STRING,
	CFG_VALUE_PARAM_TYPE_INTEGER,
	CFG_VALUE_PARAM_TYPE_UNSIGNED_INTEGER,
	CFG_VALUE_PARAM_TYPE_BOOLEAN,
	CFG_VALUE_PARAM_TYPE_DOUBLE,
};

#endif