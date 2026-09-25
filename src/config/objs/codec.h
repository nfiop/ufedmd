/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2026 Liav A
 */

#ifndef __CFG__OBJS_CODEC_H_
#define __CFG__OBJS_CODEC_H_

#include "config/types.h"

typedef struct cfg_range_object {
	cfg_str_param_t span_name;

	struct range _range;
} cfg_range_object_t;

struct cfg_codec_obj {
	/* Although the ordering of the C struct is not a restriction on the
	 * parsing logic, we require that the "name" and "type" keys to appear
	 * first on each configuration codec config object also in the actual
	 * JSON file, because these keys are "special" compared to custom
	 * key-value pairs that might appear.
	 */
	cfg_str_param_t name;
	cfg_str_param_t type;

	/* A source range is not mandatory, see the byte-fill codec that is
	 * used for the bad-block marker, for example.
	 * A destination range is necessary, because the entire point of a
	 * codec is to modify data in some way.
	 */
	cfg_range_object_t *src;
	cfg_range_object_t dest;

	/* An array of key-value pairs for settings which are not
	 * The size of the array is saved in the _count variable
	 */
	struct cfg_dict special_params;
};

#endif
