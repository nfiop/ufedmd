/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2026 Liav A
 */

#ifndef __YAML__OBJS_H_
#define __YAML__OBJS_H_

#include "config/types.h"

struct yaml_dict {
	struct key_value_pair *key_val_pairs;
	size_t key_value_pairs_count;
	size_t __used;
};

struct yaml_codec_obj {
	/* Although the ordering of the C struct is not a restriction on the
	 * parsing logic, we require that the "name" and "type" keys to appear
	 * first on each YAML codec config ojbect also in the actual YAML file,
	 * because these keys are "special" compared to custom key-value pairs
	 * that might appear.
	 */
	yaml_str_param_t name;
	yaml_str_param_t type;

	/* An array of key-value pairs for settings which are not
	 * The size of the array is saved in the _count variable
	 */
	struct yaml_dict dict;
};

struct yaml_pipeline_obj {
	yaml_str_param_t name;

	/* An array of codecs string for evaluating later.
	 * The size of the array is saved in the _count variable
	 */
	struct yaml_string_sequence codecs;
};

struct yaml_partition_obj {
	/* Mandatory strings */
	yaml_str_param_t name;
	yaml_str_param_t pipeline;

	/* Non-mandatory parameters (i.e. can be empty) */
	yaml_str_param_t eraseblocks_param;
	yaml_str_param_t pages_param;
};

struct yaml_section {
	void **objs;
	size_t objs_count;
};

struct yaml_mtd_scheme {
	struct yaml_section _codecs;
	struct yaml_section _pipelines;
	struct yaml_section _partitions;
};

#endif
