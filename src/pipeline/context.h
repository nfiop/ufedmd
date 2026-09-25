/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2026 Liav A
 */

#ifndef __PIPELINE__CODEC_EXEC_CONTEXT_H_
#define __PIPELINE__CODEC_EXEC_CONTEXT_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Kernel module headers */
#include "proxy_ioctl.h"

#include "pipeline/page_layout.h"

typedef struct {
	uint8_t *srcbuf;
	size_t srclen;

	uint8_t *destbuf;
	size_t destlen;

	/* A local storage is a place for a running codec to store
	 * temporary data for its calculations during runtime. It's acquired
	 * during a pipeline run, and a codec should not assume any valid data
	 * is remaining nor retained between codecs' switch.
	 *
	 * We certainly don't want each codec to implement its own scratch
	 * buffer-like mechanism - so by putting this here, and allocating this
	 * memory for each partition, we ensure that each concurrent context can
	 * be handled separately.
	 *
	 * As to who provides this buffer - it should be the I/O worker (thread)
	 * that handle an incoming I/O request from a ufedm_proxy device. Each
	 * worker should be initialized with such buffer, and should "donate" it
	 * for a context. This ensures that there's contention on a lock to
	 * acquire that kind of buffer, and no "hard"-object (like a codec,
	 * pipeline or a partition) owns it, but just the thread that handles
	 * requests.
	 *
	 * A local storage should have sufficient storage for any calculation
	 * that should be needed. Something like 2 whole NAND pages' size
	 * (data+OOB) should suffice, and we use codec_local_storage_size just
	 * for assertion purposes. The exact size could be calculated for each
	 * pipeline, but it is probably much easier to go for a universal value
	 * until boldly needed otherwise. To support codecs that might need to
	 * do heavy copies, we should just allocate two NAND pages in size for
	 * this purpose.
	 */
	uint8_t *local_storage;
	size_t local_storage_size;

	/* Some codecs might want to interview some parameters about the MTD
	 * during in their context during a pipeline execution.
	 */
	const struct proxy_mtd_info *mtd_info;
} write_codec_context_t;

typedef struct {
	uint8_t *databuf;
	size_t datalen;

	uint8_t *oobbuf;
	size_t ooblen;

	/* Some codecs might want to interview some parameters about the MTD
	 * during in their context during a pipeline execution.
	 */
	const struct proxy_mtd_info *mtd_info;
} read_codec_context_t;

#endif
