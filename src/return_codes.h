/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2026 Liav A
 */

#ifndef __UFEDMD_RETURN_CODES__H__
#define __UFEDMD_RETURN_CODES__H__

#include <jansson.h>

// FIXME: Reduce the amount of return codes (remove unused ones, etc).
typedef enum ufedmd_return_code {
	UFEDMD_RC_SUCCESS,

	/* General errors, invalid names, unknown types, etc */
	UFEDMD_RC_UNKNOWN_CODEC_TYPE,
	UFEDMD_RC_INVALID_CODEC_NAME,
	UFEDMD_RC_INVALID_PIPELINE_NAME,
	UFEDMD_RC_INVALID_PIPELINE_CODEC_NAME,
	UFEDMD_RC_INVALID_PARTITION_NAME,
	UFEDMD_RC_INVALID_PARTITION_PIPELINE,
	UFEDMD_RC_INVALID_PARTITION_RANGE,
	UFEDMD_RC_INVALID_CODEC_PROPERTY_VALUE,

	/* Invalid codec parts */
	UFEDMD_RC_INVALID_CODEC_KEY_TYPE,
	UFEDMD_RC_INVALID_CODEC_KEY_VALUE,

	/* Logical errors */
	UFEDMD_RC_HAS_NO_CODECS,
	UFEDMD_RC_HAS_NO_PIPELINES,
	UFEDMD_RC_HAS_NO_PARTITIONS,
	UFEDMD_RC_JSON_PARSER_FAILED,
	UFEDMD_RC_PIPELINE_INDEX_OVERFLOW,
	UFEDMD_RC_LAYOUT_PART_HAS_NO_SPAN,
	UFEDMD_RC_OVERLAPPING_PARTITION_RANGES,
	UFEDMD_RC_OVERLAPPING_PAGE_LAYOUT_PARTS,
	UFEDMD_RC_PAGE_LAYOUT_PART_OUT_OF_RANGE,
	UFEDMD_RC_HASHTABLE_HAS_EXISTING_ENTRY,
	UFEDMD_RC_PIPELINE_CODEC_NOT_FOUND,
	UFEDMD_RC_PIPELINE_NOT_FOUND,
	UFEDMD_RC_MISSING_OOB_SPAN,
	UFEDMD_RC_MISSING_DATA_SPAN,
	UFEDMD_RC_MISSING_SOURCE_SPAN,
	UFEDMD_RC_MISSING_DESTINATION_SPAN,
	UFEDMD_RC_PIPELINE_SPAN_LENGTH_BOUNDARY_INVALID,
	UFEDMD_RC_PIPELINE_SPAN_NOT_FOUND,
	UFEDMD_RC_INVALID_RANGE_FORMAT,
	UFEDMD_RC_CODEC_NOT_SUPPORTING_METHOD,
	UFEDMD_RC_RANGE_HAS_NEGATIVE_PARAMETER,
	UFEDMD_RC_RANGE_END_IS_BEFORE_START,
	UFEDMD_RC_MULTIPLE_PIPELINE_FOR_DEFAULT_PARTITION,
	UFEDMD_RC_PARTITION_BOUNDARY_VIOLATION,
	UFEDMD_RC_MULTIPLICATION_WOULD_OVERFLOW,
	UFEDMD_RC_ADDITION_WOULD_OVERFLOW,
	UFEDMD_RC_CODEC_TYPE_NOT_FOUND,
	UFEDMD_RC_PARSE_UINT_FAILED,

	/* OS errors */
	UFEDMD_OPEN_CONFIGURATION_FILE_FAILED,
	UFEDMD_RC_MEMORY_ALLOCATION_FAILED,
	UFEDMD_RC_HASHTABLE_MEMORY_ALLOCATION_FAILED,
} ufedmd_rc_enum_t;

typedef struct {
	ufedmd_rc_enum_t rc;
	char *filename;
	unsigned line;
} ufedmd_rc_t;

#define UFEDMD_RC_SET_SUCCESS(__var_name)                                      \
	do {                                                                   \
		__var_name.rc = UFEDMD_RC_SUCCESS;                             \
		__var_name.filename = NULL;                                    \
		__var_name.line = 0;                                           \
	} while (0)

#define UFEDMD_RC_CHECK_SUCCESS(__var_name) __var_name.rc == UFEDMD_RC_SUCCESS

#define UFEDMD_RC_SET(__var_name, __rc)                                        \
	do {                                                                   \
		__var_name.rc = __rc;                                          \
		__var_name.filename = __FILE__;                                \
		__var_name.line = __LINE__;                                    \
	} while (0)

#endif
