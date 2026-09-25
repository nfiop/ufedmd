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

struct fill_bytes_codec_priv {
	uint8_t byte_pattern;
};

static codec_transform_rc_t fill_bytes_write(
    struct pipeline_codec *codec, write_codec_context_t *context)
{
	codec_transform_rc_t ret;

	struct fill_bytes_codec_priv *priv = codec->priv;
	uint8_t byte_pattern;
	uint8_t *bytebuf;
	size_t byte_idx;

	byte_pattern = priv->byte_pattern;
	bytebuf = context->destbuf;

	for (byte_idx = 0; byte_idx < context->destlen; byte_idx++) {
		bytebuf[byte_idx] = byte_pattern;
	}

	CODEC_ANSWER_ACK(ret);
	return ret;
}

static void fill_bytes_deinit(struct pipeline_codec *codec)
{
	free(codec->priv);
}

static bool fill_bytes_needs_source_span(void)
{
	return false;
}

static bool fill_bytes_validate_spans_size_sufficient(
    struct pipeline_codec *codec, size_t span1_size, size_t span2_size)
{
	UNUSED(codec);
	UNUSED(span1_size);
	UNUSED(span2_size);
	return true;
}

static struct write_ops fill_bytes_write_ops = {
    .on_write = fill_bytes_write,
    .validate_spans_size_sufficient = fill_bytes_validate_spans_size_sufficient,

    /* Static methods */
    .needs_source_span = fill_bytes_needs_source_span,
};

static ufedmd_rc_t handle_byte_pattern_entry(
    pipeline_codec_t *codec, void *value)
{
	ufedmd_rc_t ret;
	unsigned int byte_pattern;
	struct fill_bytes_codec_priv *priv;

	byte_pattern = *(unsigned int *)value;

	if (byte_pattern > 255) {
		UFEDMD_RC_SET(ret, UFEDMD_RC_INVALID_CODEC_PROPERTY_VALUE);
		goto exit;
	}

	priv = codec->priv;
	priv->byte_pattern = byte_pattern;

	UFEDMD_RC_SET_SUCCESS(ret);
exit:
	return ret;
}

struct codec_entry_parser parsers[] = {
    {.key = "byte_pattern",
	.type = CODEC_ENTRY_PARSER_VALUE_TYPE_UINT,
	.handle = handle_byte_pattern_entry},
};

ufedmd_rc_t init_fill_bytes_codec(pipeline_codec_t *base,
    struct proxy_mtd_info *mtd_info, struct cfg_dict *config)
{
	ufedmd_rc_t ret;

	UNUSED(mtd_info);

	ALLOCATE_PRIVATE_DATA_OR_FAIL(base->priv, struct fill_bytes_codec_priv);

	ret = create_standard_codec(base, fill_bytes_deinit, config, parsers,
	    ARRAY_SIZE(parsers), &fill_bytes_write_ops, NULL);
	if (UFEDMD_RC_CHECK_SUCCESS(ret)) {
		goto exit;
	}

	UFEDMD_RC_SET_SUCCESS(ret);
exit:
	return ret;
}
