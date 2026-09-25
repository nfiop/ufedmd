/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2026 Liav A
 */

#ifndef __CFG_RETURN_CODES__H__
#define __CFG_RETURN_CODES__H__

#include <jansson.h>

typedef enum cfg_return_code {
	CFG_RC_SUCCESS,

	/* Invalid nodes, invalid types */
	CFG_RC_SCALAR_LENGTH_TOO_BIG,
	CFG_RC_NODE_IS_NOT_MAPPING,
	CFG_RC_INVALID_OBJECT_NODE,
	CFG_RC_NODE_IS_NOT_SEQUENCE,
	CFG_RC_NODE_IS_NOT_STRING,
	CFG_RC_NODE_IS_NOT_SCALAR,
	CFG_RC_INVALID_BOOLEAN_FLAG,
	CFG_RC_INTEGER_IS_NEGATIVE,

	/* Logical errors */
	CFG_RC_EMPTY_NODE,
	CFG_RC_INVALID_SECTION_TYPE,
	CFG_RC_MISSING_PARTITION_PARAMETERS,
	CFG_RC_SCALAR_COMPARE_NO_MATCH,
	CFG_RC_NODE_FAILED_GET_CHILD_NODE,
	CFG_RC_MISSING_PIPELINE_PARAMETERS,
	CFG_RC_MISSING_LAYOUT_PARAMETERS,
	CFG_RC_NODE_IS_NOT_ARRAY,
	CFG_RC_SECTION_ALREADY_PARSED,
	CFG_RC_ENTRY_ALREADY_PARSED,
	CFG_RC_SECTION_NAME_UNKNOWN,
	CFG_RC_MISSING_CODEC_PARAMETERS,
	CFG_RC_COMPARING_WITH_NON_SCALAR,
	CFG_RC_KEY_NOT_FOUND,
	CFG_RC_INVALID_PAIR_POSITION,
	CFG_RC_SCALAR_COMPAREE_TOO_BIG,

	/* Parser errors from jansson */
	CFG_RC_NO_ROOT_NODE,

	/* OS errors */
	CFG_RC_MEMORY_ALLOCATION_FAILED,
} cfg_return_code_enum_t;

typedef struct {
	cfg_return_code_enum_t rc;
	json_t *offending;
	char *filename;
	const char *_func;
	unsigned line;
} cfg_return_code_t;

#define CFG_RC_SET_SUCCESS(__var_name)                                         \
	do {                                                                   \
		__var_name.rc = CFG_RC_SUCCESS;                                \
		__var_name.offending = NULL;                                   \
		__var_name.filename = NULL;                                    \
		__var_name._func = NULL;                                       \
		__var_name.line = 0;                                           \
	} while (0)

#define CFG_RC_CHECK_SUCCESS(__var_name) __var_name.rc == CFG_RC_SUCCESS

#define CFG_RC_SET(__var_name, __rc)                                           \
	do {                                                                   \
		__var_name.rc = __rc;                                          \
		__var_name.offending = NULL;                                   \
		__var_name.filename = __FILE__;                                \
		__var_name._func = __func__;                                   \
		__var_name.line = __LINE__;                                    \
	} while (0)

#define CFG_RC_SET_WITH_OFFENDING_NODE(__var_name, __rc, __node)               \
	do {                                                                   \
		__var_name.rc = __rc;                                          \
		__var_name.offending = __node;                                 \
		__var_name.filename = __FILE__;                                \
		__var_name._func = __func__;                                   \
		__var_name.line = __LINE__;                                    \
	} while (0)

#define CFG_RC_SET_OFFENDING_NODE(__var_name, __node)                          \
	do {                                                                   \
		__var_name.offending = __node;                                 \
	} while (0)

#endif
