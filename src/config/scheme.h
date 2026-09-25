/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2026 Liav A
 */

#ifndef __CFG__SCHEME_H_
#define __CFG__SCHEME_H_

#include "config/objs/codec.h"
#include "config/objs/partition.h"
#include "config/objs/pipeline.h"

#include "config/types.h"

struct cfg_codecs_section {
	struct cfg_codec_obj *objs;
	size_t objs_count;
};

struct cfg_layouts_section {
	struct cfg_layout_obj *objs;
	size_t objs_count;
};

struct cfg_pipelines_section {
	struct cfg_pipeline_obj *objs;
	size_t objs_count;
};

struct cfg_partitions_section {
	struct cfg_partition_obj *objs;
	size_t objs_count;
};

struct cfg_scheme {
	struct cfg_codecs_section codecs;
	struct cfg_pipelines_section pipelines;
	struct cfg_partitions_section partitions;
	struct cfg_layouts_section layouts;
};

#endif
