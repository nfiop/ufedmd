/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2026 Liav A
 */

#ifndef __PIPELINE_SCHEME__H_
#define __PIPELINE_SCHEME__H_

#include "common/hashmap.h"
#include "config/scheme.h"
#include "config/types.h"
#include "pipeline/codec.h"
#include "pipeline/page_layout.h"
#include "pipeline/types.h"
#include "return_codes.h"

/* Kernel module headers */
#include "proxy_ioctl.h"

#define AUTOMATIC_NACK_PIPELINE_INDEX (0)

typedef struct {
	/* Merely a reference to an existing codec, not owned by this
	 * struct.
	 */
	pipeline_codec_t *codec;

	/* This is merely an array to pointers of already existing spans,
	 * which are not owned by this struct, but the `struct pipeline_layout`
	 * owns them.
	 * It should help during a pipeline execution to locate the source &
	 * destination spans. The source span pointer can be NULL (depending
	 * on the codec), the destination one must be non-NULL.
	 */
	struct pipeline_page_layout_span *src_span;
	struct pipeline_page_layout_span *dest_span;

	struct range in_src_span_range;
	struct range in_dest_span_range;
} pipeline_write_codec_context_t;

typedef struct {
	/* Merely a reference to an existing codec, not owned by this
	 * struct.
	 */
	pipeline_codec_t *codec;

	/* The XOR codec doesn't need an OOB span when reading.
	 * This pointer can be NULL, similarly to the source span in
	 * write context.
	 */
	struct pipeline_page_layout_span *oob_span;
	struct range in_oob_span_range;

	struct pipeline_page_layout_span *data_span;
	struct range in_data_span_range;
} pipeline_read_codec_context_t;

struct write_codec_contexts_set {
	size_t codecs_count;
	pipeline_write_codec_context_t **codecs;
};

struct read_codec_contexts_set {
	size_t codecs_count;
	pipeline_read_codec_context_t **codecs;
};

struct span_mappings {
	struct pipeline_page_layout_span *oob_span;
	struct pipeline_page_layout_span *data_span;
};

typedef struct {
	/* Allocated normally by strdup */
	char *name;

	/* Index 1-based order - 0 means a special index for an
	 * automatic NACK pipeline.
	 */
	pipeline_index_t idx;

	/* Pipeline layout parts */
	struct pipeline_page_layout page_layout;

	/* The idea behind these mappings is quite simple -
	 *
	 * For read request, we get a raw page buffer via the proxy device slot,
	 * with data + OOB from the kernel. We apply the chain of read codecs
	 * with the corresponding spans from the page layout, and finally fill
	 * the data and OOB buffers with the required spans before sending the
	 * ACK to the MTD client.
	 *
	 * For write request, we get a data buffer and an optional buffer for
	 * OOB via the proxy device slot, so we put these buffers in the
	 * corresponding spans, and then we apply the chain of write codecs.
	 * Finally we fill the raw page buffer with all required spans from
	 * the page layout, and send it for actual write.
	 */
	struct span_mappings read_spans;
	struct span_mappings *write_spans;

	/* A context of all codecs that in use by this pipeline -
	 * it connects a codec to its subranges in this pipeline.
	 */
	struct read_codec_contexts_set read_codecs;
	struct write_codec_contexts_set write_codecs;
} pipeline_t;

typedef struct {
	/* Allocated by strdup */
	char *name;

	/* Set to a pipeline from the pipelines map */
	pipeline_t *pipeline;

	struct range eraseblocks;
	struct range pages;
} partition_t;

/* This struct is called io_demux and **NOT** io_mux, because,
 * in digital logic circuitry, a demux is a component that gets
 * one signal and delivers it to a designated electrical output
 * from many others in a group.
 * Similarly, we _demux_ a I/O request to the correct pipeline
 * based on eraseblock & page selection.
 */

struct io_demux {
	/* This is for an optimization path - we hold a single
	 * pipeline object if there are no multiple partitions
	 * that use many pipelines.
	 */
	pipeline_t *_single_pipeline;

	/* This array is constructed when we have multiple pipelines
	 * to select from. It is built during pipelines' creation method.
	 */
	pipeline_t *pipelines;
	size_t pipelines_count;

	/* The indices' array is created in the pipeline selector creation
	 * phase, and its size is directly determined by the strategy of
	 * selection:
	 *  - A pipeline for every distinct page in every eraseblock, or
	 *  - A pipeline for every eraseblock
	 * So it's highly depend on the configuration, and therefore should
	 * be evaluated at last.
	 */
	pipeline_index_t *arr;
	size_t flash_pages_per_sector_stride;

	pipeline_t *(*select)(
	    struct io_demux *demux, uint64_t eraseblock_idx, uint64_t page_idx);
};

struct nand_pipeline_scheme {
	struct hashmap *codecs;

	/* This map is special - it doesn't own any object it refers to
	 * and instead it holds pointers to objects that are allocated on
	 * the struct io_demux pipelines array.
	 */
	struct hashmap *pipelines;

	struct hashmap *partitions;

	struct io_demux demux;
};

ufedmd_rc_t initialize_nand_pipeline_scheme(struct nand_pipeline_scheme *scheme,
    struct proxy_mtd_info *mtd_info, struct cfg_scheme *cfg_scheme);
void destroy_nand_pipeline_scheme(struct nand_pipeline_scheme *scheme);

#endif
