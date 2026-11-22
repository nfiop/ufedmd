/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2026 Liav A
 */

#ifndef __PIPELINE__STANDARD_PIPELINE__H_
#define __PIPELINE__STANDARD_PIPELINE__H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Imported from libufedm-dev */
#include "proxy_dev.h"

#include "pipeline/scheme.h"

struct standard_pipeline_thread_priv {
	struct proxy_device_state *device;
	struct nand_pipeline_scheme scheme;
};

transform_answer_t standard_pipeline_write_process(
    const struct shm_slot_hdr *hdr, u8 *raw_page_buf,
    struct transform_context_results *context_res, void *private_data);

transform_answer_t standard_pipeline_read_process(
    const struct shm_slot_hdr *hdr, u8 *raw_page_buf,
    struct transform_context_results *context_res, void *private_data);

#endif
