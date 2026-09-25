/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2026 Liav A
 */

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "defs.h"

#include "common/array.h"
#include "common/ints.h"
#include "config/objs/codec.h"
#include "pipeline/codec.h"
#include "pipeline/types.h"
#include "return_codes.h"

#include "common/logging.h"
#include "common/parse.h"

/* Kernel module headers */
#include "proxy_ioctl.h"

#include <assert.h>

struct xor_codec_priv {
	uint8_t *byte_mask_buf;
	size_t byte_mask_buf_size;
	bool allow_partial_chunk;
};

static codec_transform_rc_t xor_write(
    struct pipeline_codec *codec, write_codec_context_t *context)
{
	codec_transform_rc_t ret;

	struct xor_codec_priv *priv = codec->priv;
	uint8_t *byte_mask_buf;
	uint8_t *bytebuf;
	size_t byte_idx;

	byte_mask_buf = priv->byte_mask_buf;
	bytebuf = context->destbuf;

	for (byte_idx = 0; byte_idx < context->destlen; byte_idx++) {
		bytebuf[byte_idx] ^= byte_mask_buf[byte_idx];
	}

	CODEC_ANSWER_ACK(ret);
	return ret;
}

static codec_transform_rc_t xor_read(
    struct pipeline_codec *codec, read_codec_context_t *context)
{
	codec_transform_rc_t ret;

	struct xor_codec_priv *priv = codec->priv;
	uint8_t *byte_mask_buf;
	uint8_t *bytebuf;
	size_t byte_idx;

	byte_mask_buf = priv->byte_mask_buf;
	bytebuf = context->databuf;

	for (byte_idx = 0; byte_idx < context->datalen; byte_idx++) {
		bytebuf[byte_idx] ^= byte_mask_buf[byte_idx];
	}

	CODEC_ANSWER_ACK(ret);
	return ret;
}

static void xor_deinit(struct pipeline_codec *codec)
{
	struct xor_codec_priv *priv = codec->priv;
	free(priv->byte_mask_buf);

	free(codec->priv);
}

static bool xor_needs_source_span(void)
{
	return false;
}

static bool xor_needs_oob_span(void)
{
	return false;
}

static bool xor_validate_write_spans_size_sufficient(
    struct pipeline_codec *codec, size_t src_span_size, size_t dest_span_size)
{
	struct xor_codec_priv *priv = codec->priv;

	UNUSED(src_span_size);

	if ((dest_span_size % priv->byte_mask_buf_size) != 0 &&
	    !priv->allow_partial_chunk) {
		return false;
	}
	return true;
}

static bool xor_validate_read_spans_size_sufficient(
    struct pipeline_codec *codec, size_t data_span_size, size_t oob_span_size)
{
	struct xor_codec_priv *priv = codec->priv;

	UNUSED(oob_span_size);

	if ((data_span_size % priv->byte_mask_buf_size) != 0 &&
	    !priv->allow_partial_chunk) {
		return false;
	}
	return true;
}

static struct write_ops xor_write_ops = {
    .on_write = xor_write,
    .validate_spans_size_sufficient = xor_validate_write_spans_size_sufficient,

    /* Static methods */
    .needs_source_span = xor_needs_source_span,
};

static struct read_ops xor_read_ops = {
    .on_read = xor_read,
    .validate_spans_size_sufficient = xor_validate_read_spans_size_sufficient,

    /* Static methods */
    .needs_oob_span = xor_needs_oob_span,
};

static ufedmd_rc_t handle_mask_entry(pipeline_codec_t *codec, void *value)
{
	int _ret;
	ufedmd_rc_t ret;
	struct byte_array mask_array;
	struct xor_codec_priv *priv;

	_ret =
	    create_byte_array_from_hex_string((const char *)value, &mask_array);
	if (_ret < 0) {
		UFEDMD_RC_SET(ret, UFEDMD_RC_INVALID_CODEC_PROPERTY_VALUE);
		goto exit;
	}

	priv = codec->priv;
	priv->byte_mask_buf = mask_array.buf;
	priv->byte_mask_buf_size = mask_array.length;

	UFEDMD_RC_SET_SUCCESS(ret);
exit:
	return ret;
}

static struct codec_entry_parser parsers[] = {
    {.key = "byte_pattern",
	.type = CODEC_ENTRY_PARSER_VALUE_TYPE_STRING,
	.handle = handle_mask_entry},
};

ufedmd_rc_t init_xor_codec(pipeline_codec_t *base,
    struct proxy_mtd_info *mtd_info, struct cfg_dict *config)
{
	ufedmd_rc_t ret;

	UNUSED(mtd_info);

	ALLOCATE_PRIVATE_DATA_OR_FAIL(base->priv, struct xor_codec_priv);

	ret = create_standard_codec(base, xor_deinit, config, parsers,
	    ARRAY_SIZE(parsers), &xor_write_ops, &xor_read_ops);
	if (UFEDMD_RC_CHECK_SUCCESS(ret)) {
		goto exit;
	}

	UFEDMD_RC_SET_SUCCESS(ret);
exit:
	return ret;
}
