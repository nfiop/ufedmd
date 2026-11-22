/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2026 Liav A
 */

#include "pipeline/standard_pipeline.h"
#include "pipeline/page_layout.h"
#include "pipeline/scheme.h"

#include "ufedmd-common/logging.h"

#include <assert.h>
#include <errno.h>
#include <string.h>

/* NOTE: We don't check the length of the raw page buffer - it's already
 * verified in the the scheme initialization from the configuration file.
 */
static void copy_write_data_and_oob_spans_to_raw_page_buffer(
    struct proxy_device_state *device, pipeline_t *pipeline, u8 *raw_page_buf,
    const struct shm_slot_hdr *hdr)
{
	size_t oob_offset =
	    device->mtd_info.flash_page_size - device->mtd_info.flash_oob_size;

	/* Copy data span from the first section of the raw page buffer.
	 * OOB comes afterwards, as a completely separate section.
	 * We use the shm slot header, and not the raw geometry values
	 * because that is what the user **requested** to be written.
	 * We also trust _blindly_ (therefore the assertions) the values
	 * from the header, because they came from the kernel.
	 */
	assert(hdr->datalen <= oob_offset);
	assert(hdr->ooblen <= device->mtd_info.flash_oob_size);

	memcpy(
	    pipeline->write_spans->data_span->buf, raw_page_buf, hdr->datalen);
	memcpy(pipeline->write_spans->oob_span->buf, raw_page_buf + oob_offset,
	    hdr->ooblen);
}

/* NOTE: We don't check the length of the raw page buffer - it's already
 * verified in the the scheme initialization from the configuration file.
 */
static void copy_result_data_and_oob_spans_to_raw_page_buffer(
    struct proxy_device_state *device, pipeline_t *pipeline, u8 *raw_page_buf,
    const struct shm_slot_hdr *hdr)
{
	size_t oob_offset =
	    device->mtd_info.flash_page_size - device->mtd_info.flash_oob_size;

	/* Copy data span to the first section of the raw page buffer.
	 * OOB comes afterwards, as a completely separate section.
	 * We use the shm slot header, and not the raw geometry values
	 * because that is what the user **requested** to be returned.
	 * We also trust _blindly_ (therefore the assertions) the values
	 * from the header, because they came from the kernel.
	 */
	assert(hdr->datalen <= oob_offset);
	assert(hdr->ooblen <= device->mtd_info.flash_oob_size);

	memset(raw_page_buf, 0, device->mtd_info.flash_page_size);
	memcpy(raw_page_buf, pipeline->read_spans.data_span->buf, hdr->datalen);
	memcpy(raw_page_buf + oob_offset, pipeline->read_spans.oob_span->buf,
	    hdr->ooblen);
}

static pipeline_t *select_pipeline(struct proxy_device_state *device,
    const struct shm_slot_hdr *hdr, struct nand_pipeline_scheme *scheme)
{
	pipeline_t *pipeline;
	VLOG(2, stderr, "%s: positition - lun:plane:block:page - %u:%u:%u:%u\n",
	    __func__, (unsigned)hdr->pos_params.lun,
	    (unsigned)hdr->pos_params.plane,
	    (unsigned)hdr->pos_params.eraseblock,
	    (unsigned)hdr->pos_params.page);

	assert(hdr->pos_params.eraseblock < device->mtd_info.flash_sectors_cnt);
	assert(
	    hdr->pos_params.page < device->mtd_info.flash_pages_per_sector_cnt);

	pipeline = scheme->demux.select(
	    &scheme->demux, hdr->pos_params.eraseblock, hdr->pos_params.page);

	/* There must be a pipeline, because the default is the automatic NACK
	 * pipeline.
	 */
	assert(pipeline != NULL);
	VLOG(2, stderr, "%s: pipeline selected %s\n", __func__, pipeline->name);
	return pipeline;
}

transform_answer_t standard_pipeline_write_process(
    const struct shm_slot_hdr *hdr, u8 *raw_page_buf,
    struct transform_context_results *context_res, void *private_data)
{
	size_t idx;
	pipeline_t *pipeline = NULL;
	struct standard_pipeline_thread_priv *priv = private_data;
	codec_transform_rc_t codec_rc;
	pipeline_write_codec_context_t *context;
	write_codec_context_t write_context;
	pipeline_codec_t *codec;

	pipeline = select_pipeline(priv->device, hdr, &priv->scheme);

	// FIXME: For now, we consider write operation as possibly optional,
	// so this might be NULL. It's probably better to change this so
	// this behaves more like a read operation, with a possible default to
	// the NACK pipeline.
	if (!pipeline->write_spans) {
		VLOG(2, stderr,
		    "%s: pipeline %s, has no support for write operation\n",
		    __func__, pipeline->name);
		context_res->errno_rc = EIO;
		return ANSWER_NACK;
	}

	copy_write_data_and_oob_spans_to_raw_page_buffer(
	    priv->device, pipeline, raw_page_buf, hdr);

	write_context.mtd_info = &priv->device->mtd_info;

	/* We are ready now, let's execute each read context with its codec */
	for (idx = 0; idx < pipeline->write_codecs.count; idx++) {
		context = pipeline->write_codecs.contexts[idx];
		codec = context->codec;

		write_context.destbuf =
		    context->dest_span->buf + context->in_dest_span_range.start;
		write_context.destlen = context->in_dest_span_range.length;

		/* The source span is optional. some codecs don't need it. */
		if (context->src_span) {
			write_context.srcbuf = context->src_span->buf +
					       context->in_src_span_range.start;
			write_context.srclen =
			    context->in_src_span_range.length;
		}

		codec_rc = codec->write_ops->on_write(codec, &write_context);

		if (!CODEC_RC_CHECK_SUCCESS(codec_rc)) {
			VLOG(2, stderr,
			    "%s: pipeline %s, codec %s: failed, error: %d\n",
			    __func__, pipeline->name, codec->name,
			    codec_rc._errno);
			context_res->errno_rc = codec_rc._errno;
			return ANSWER_NACK;
		}
	}

	// FIXME: Don't use priv->device->mtd_info.flash_page_size directly.
	compose_raw_page_from_pipeline_page_layout(&pipeline->page_layout,
	    raw_page_buf, priv->device->mtd_info.flash_page_size);

	context_res->data_retlen = priv->device->mtd_info.flash_page_size -
				   priv->device->mtd_info.flash_oob_size;
	context_res->oob_retlen = priv->device->mtd_info.flash_oob_size;
	VLOG(2, stderr, "%s: returned data length - %zu, oob length - %zu\n",
	    __func__, context_res->data_retlen, context_res->oob_retlen);
	context_res->errno_rc = 0;
	return ANSWER_ACK;
}

transform_answer_t standard_pipeline_read_process(
    const struct shm_slot_hdr *hdr, u8 *raw_page_buf,
    struct transform_context_results *context_res, void *private_data)
{
	size_t idx;
	pipeline_t *pipeline = NULL;
	struct standard_pipeline_thread_priv *priv = private_data;
	codec_transform_rc_t codec_rc;
	pipeline_read_codec_context_t *context;
	read_codec_context_t read_context;
	pipeline_codec_t *codec;

	pipeline = select_pipeline(priv->device, hdr, &priv->scheme);

	/* Extract the raw page from the shared memory buffer into spans
	 * according to the page layout of the pipeline.
	 */
	// FIXME: Don't use priv->device->mtd_info.flash_page_size directly.
	extract_raw_page_to_pipeline_page_layout(&pipeline->page_layout,
	    raw_page_buf, priv->device->mtd_info.flash_page_size);

	read_context.mtd_info = &priv->device->mtd_info;

	/* We are ready now, let's execute each read context with its codec */
	for (idx = 0; idx < pipeline->read_codecs.count; idx++) {
		context = pipeline->read_codecs.contexts[idx];
		codec = context->codec;

		read_context.databuf =
		    context->data_span->buf + context->in_data_span_range.start;
		read_context.datalen = context->in_data_span_range.length;

		/* The OOB span is optional. some codecs don't need it. */
		if (context->oob_span) {
			read_context.oobbuf = context->oob_span->buf +
					      context->in_oob_span_range.start;
			read_context.ooblen = context->in_oob_span_range.length;
		}

		codec_rc = codec->read_ops->on_read(codec, &read_context);

		if (!CODEC_RC_CHECK_SUCCESS(codec_rc)) {
			VLOG(2, stderr,
			    "%s: pipeline %s, codec %s: failed, error: %d\n",
			    __func__, pipeline->name, codec->name,
			    codec_rc._errno);
			context_res->errno_rc = codec_rc._errno;
			return ANSWER_NACK;
		}
	}

	copy_result_data_and_oob_spans_to_raw_page_buffer(
	    priv->device, pipeline, raw_page_buf, hdr);

	VLOG(2, stderr, "%s: returned data length - %d, oob length - %d\n",
	    __func__, hdr->datalen, hdr->ooblen);
	context_res->data_retlen = hdr->datalen;
	context_res->oob_retlen = hdr->ooblen;
	context_res->errno_rc = 0;
	return ANSWER_ACK;
}
