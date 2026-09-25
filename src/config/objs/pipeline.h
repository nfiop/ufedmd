/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2026 Liav A
 */

#ifndef __CFG__OBJS_PIPELINE_H_
#define __CFG__OBJS_PIPELINE_H_

#include "config/types.h"

struct cfg_page_layout_part_obj {
	cfg_str_param_t part_name;
	cfg_str_param_t span_name;

	struct range _range;
};

struct cfg_page_layout_obj {
	struct cfg_page_layout_part_obj *parts;
	size_t parts_count;
};

struct cfg_span_specifier {
	cfg_str_param_t span_name;
	struct range _range;
};

struct cfg_write_codec_specifier_obj {
	cfg_str_param_t codec_name;

	/* A codec specifier might not have a source
	 * span because the codec won't use it.
	 * However, a destination span is a must.
	 */
	struct cfg_span_specifier *src;
	struct cfg_span_specifier dest;
};

struct cfg_read_codec_specifier_obj {
	cfg_str_param_t codec_name;

	struct cfg_span_specifier data;

	/* Only the XOR codec supports write and read capabilities
	 * but it doesn't need an OOB span at all (the fill-bytes codec
	 * for example, doesn't support a source span, and probably will never
	 * be used by in a read context anyway).
	 * The flag of has_valid_oob_specifier determines whether the specifier
	 * is actually valid or not (so it's set in the configuration file
	 * properly).
	 */
	struct cfg_span_specifier oob;
	bool has_valid_oob_specifier;
};

struct cfg_write_codecs_sequence {
	struct cfg_write_codec_specifier_obj *objs;
	size_t objs_count;
};

struct cfg_read_codecs_sequence {
	struct cfg_read_codec_specifier_obj *objs;
	size_t objs_count;
};

struct cfg_request_span_mappings {
	cfg_str_param_t data_span_name;
	cfg_str_param_t oob_span_name;
};

struct cfg_pipeline_obj {
	cfg_str_param_t name;
	cfg_str_param_t layout;

	struct cfg_request_span_mappings read_mappings;
	struct cfg_request_span_mappings *write_mappings;

	struct cfg_read_codecs_sequence read_codecs;
	struct cfg_write_codecs_sequence *write_codecs;

	/* Page layout */
	struct cfg_page_layout_obj page_layout;
};

#endif
