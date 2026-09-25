/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2026 Liav A
 */

#ifndef __PIPELINE_CODEC__H_
#define __PIPELINE_CODEC__H_

#include "config/types.h"
#include "context.h"
#include "pipeline/codecs/return_codes.h"
#include "pipeline/context.h"
#include "pipeline/types.h"
#include "return_codes.h"

#define CODEC_ANSWER_NACK_WITH_RC(__rc, __val)                                 \
	do {                                                                   \
		__rc.rc = __val;                                               \
		__rc._errno = -1;                                              \
	} while (0)

#define CODEC_ANSWER_NACK_WITH_GENERIC_RC(__rc, __val)                         \
	do {                                                                   \
		__rc.rc = CODEC_SPECIAL_ERRNO_SPECIFIED;                       \
		__rc._errno = __val;                                           \
	} while (0)

#define CODEC_ANSWER_ACK(__rc)                                                 \
	do {                                                                   \
		__rc.rc = CODEC_OK;                                            \
		__rc._errno = 0;                                               \
	} while (0)

#define ALLOCATE_PRIVATE_DATA_OR_FAIL(_priv, _struct)                          \
	do {                                                                   \
		ufedmd_rc_t __ret;                                             \
		_priv = calloc(1, sizeof(_struct));                            \
		if (!_priv) {                                                  \
			UFEDMD_RC_SET(                                         \
			    __ret, UFEDMD_RC_MEMORY_ALLOCATION_FAILED);        \
			return __ret;                                          \
		}                                                              \
	} while (0)

typedef struct codec_rc {
	codec_rc_enum_t rc;
	int _errno;
} codec_transform_rc_t;

struct pipeline_codec;

struct read_ops {
	codec_transform_rc_t (*on_read)(
	    struct pipeline_codec *codec, read_codec_context_t *context);

	/* Except for the XOR codec, we should probably require that OOB size
	 * isn't 0, because otherwise it's really weird and not reasonable on
	 * how the codec decodes data at all.
	 *
	 * In simple words, both data and OOB are **required** for decoding,
	 * as otherwise there would be no need for decoding.
	 */
	bool (*validate_spans_size_sufficient)(struct pipeline_codec *codec,
	    size_t data_span_size, size_t oob_span_size);

	/* Static methods */
	bool (*needs_oob_span)(void);
};

struct write_ops {
	codec_transform_rc_t (*on_write)(
	    struct pipeline_codec *codec, write_codec_context_t *context);

	/* Some codecs don't need a source span. In such case,
	 * We simply pass a src_span_size of 0, and ensure that
	 * the codec can still function correctly.
	 */
	bool (*validate_spans_size_sufficient)(struct pipeline_codec *codec,
	    size_t src_span_size, size_t dest_span_size);

	/* Static methods */
	bool (*needs_source_span)(void);
};

typedef struct pipeline_codec {
	/* name and type are allocated by strdup */
	char *name;
	char *type;

	struct write_ops *write_ops;
	struct read_ops *read_ops;
	void (*deinit)(struct pipeline_codec *);

	/* Private data */
	void *priv;
} pipeline_codec_t;

enum codec_entry_parser_value_type {
	CODEC_ENTRY_PARSER_VALUE_TYPE_INT,
	CODEC_ENTRY_PARSER_VALUE_TYPE_UINT,
	CODEC_ENTRY_PARSER_VALUE_TYPE_STRING,
	CODEC_ENTRY_PARSER_VALUE_TYPE_BOOL,
};

struct codec_entry_parser {
	const char *key;
	enum codec_entry_parser_value_type type;
	ufedmd_rc_t (*handle)(pipeline_codec_t *codec, void *value);
};

ufedmd_rc_t create_standard_codec(pipeline_codec_t *codec,
    void (*deinit_callback)(struct pipeline_codec *), struct cfg_dict *config,
    struct codec_entry_parser *parsers, size_t nparsers,
    struct write_ops *write_ops, struct read_ops *read_ops);

ufedmd_rc_t parse_codec_entries(pipeline_codec_t *codec, struct cfg_dict *dict,
    struct codec_entry_parser *parsers, size_t nparsers);

#endif
