/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2026 Liav A
 */

#ifndef __YAML_RETURN_CODES__H__
#define __YAML_RETURN_CODES__H__

#include <yaml.h>

typedef enum yaml_return_code {
	YAML_RC_SUCCESS,

	/* Invalid nodes, invalid types */
	YAML_RC_SCALAR_LENGTH_TOO_BIG,
	YAML_RC_NODE_IS_NOT_MAPPING,
	YAML_RC_INVALID_OBJECT_NODE,
	YAML_RC_NODE_IS_NOT_SEQUENCE,
	YAML_RC_NODE_IS_NOT_SCALAR,

	/* Logical errors */
	YAML_RC_SCALAR_COMPARE_NO_MATCH,
	YAML_RC_NODE_FAILED_GET_CHILD_NODE,
	YAML_RC_SECTION_ALREADY_PARSED,
	YAML_RC_ENTRY_ALREADY_PARSED,
	YAML_RC_SECTION_NAME_UNKNOWN,
	YAML_RC_EMPTY_COMPARE_SET,
	YAML_RC_INVALID_MAPPING_NODE,
	YAML_RC_INVALID_MAPPING_NODE_PAIRS_COUNT,
	YAML_RC_INVALID_MAPPING_NODE_ENTRY_KEY,
	YAML_RC_INVALID_MAPPING_NODE_ENTRY_VALUE,
	YAML_RC_COMPARING_WITH_NON_SCALAR,
	YAML_RC_KEY_NOT_FOUND,
	YAML_RC_INVALID_PAIR_POSITION,
	YAML_RC_SCALAR_COMPAREE_TOO_BIG,

	/* Parser errors from LibYAML */
	YAML_RC_NO_ROOT_NODE,
	YAML_RC_PARSER_INITIALIZE_FAILED,
	YAML_RC_PARSER_LOAD_FAILED,

	/* OS errors */
	YAML_RC_MEMORY_ALLOCATION_FAILED,
} yaml_return_code_enum_t;

typedef struct {
	yaml_return_code_enum_t rc;
	yaml_node_t *offending;
} yaml_return_code_t;

#define YAML_RC_SET_SUCCESS(__var_name)                                        \
	do {                                                                   \
		__var_name.rc = YAML_RC_SUCCESS;                               \
		__var_name.offending = NULL;                                   \
	} while (0)

#define YAML_RC_CHECK_SUCCESS(__var_name) __var_name.rc == YAML_RC_SUCCESS

#define YAML_RC_SET(__var_name, __rc)                                          \
	do {                                                                   \
		__var_name.rc = __rc;                                          \
		__var_name.offending = NULL;                                   \
	} while (0)

#define YAML_RC_SET_WITH_OFFENDING_NODE(__var_name, __rc, __node)              \
	do {                                                                   \
		__var_name.rc = __rc;                                          \
		__var_name.offending = __node;                                 \
	} while (0)

#define YAML_RC_SET_OFFENDING_NODE(__var_name, __node)                         \
	do {                                                                   \
		__var_name.offending = __node;                                 \
	} while (0)

#endif
