/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2026 Liav A
 */

#ifndef __PIPELINE__CODECS__RC_H_
#define __PIPELINE__CODECS__RC_H_

#include <stddef.h>
#include <stdint.h>

typedef enum {
	CODEC_OK,

	CODEC_RC_INVALID_PARAMS_ERR,
	CODEC_RC_DATA_RECOVERY_FAILED_ERR,
	CODEC_RC_WILL_CORRUPT_DATA_ERR,
	CODEC_RC_BOUNDARIES_INVALID_ERR,

	CODEC_RC_UNKNOWN_ALGORITHM_ERR,

	CODEC_RC_SPECIAL_ERRNO_SPECIFIED,

	CODEC_RC_UNKNOWN_ERR,
} codec_rc_enum_t;

#endif
