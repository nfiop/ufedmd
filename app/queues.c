/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2026 Liav A
 */

#include <assert.h>
#include <errno.h>

#include "pipeline/standard_pipeline.h"

/* Imported from libufedm-dev */
#include "proxy_dev.h"

/* Application headers */
#include "queues.h"

extern struct cfg_scheme g_cfg_scheme;
extern struct proxy_device_state g_device_state;

static struct proxy_device_queue_state queues[PROXY_MAX_QUEUES_COUNT];
static int queues_initialized_count = 0;

static int __thread_priv_create(void **privp)
{
	int ret;
	ufedmd_rc_t _ret;
	struct standard_pipeline_thread_priv *priv;

	priv = zalloc_obj(priv);
	if (!priv) {
		ret = -ENOMEM;
		goto exit;
	}

	priv->device = &g_device_state;

	/* FIXME: This initializes a pipeline for read and write contexts.
	 * We should be creating a smaller scheme focused on either type.
	 */
	_ret = initialize_nand_pipeline_scheme(
	    &priv->scheme, &g_device_state.mtd_info, &g_cfg_scheme);
	if (!UFEDMD_RC_CHECK_SUCCESS(_ret)) {
		fprintf(stderr, "Error: file - %s, line - %d\n", _ret.filename,
		    _ret.line);
		ret = -EINVAL;
		goto exit;
	}

	ret = 0;
	*privp = priv;

exit:
	return ret;
}

void __thread_priv_teardown(void *priv)
{
	struct standard_pipeline_thread_priv *_priv = priv;
	destroy_nand_pipeline_scheme(&_priv->scheme);
	free(_priv);
}

int create_queues()
{
	/* This function should be called once.
	 * This is not the best guard though, but should be sufficient.
	 */
	assert(queues_initialized_count == 0);

	struct proxy_queue_func_ops ops = {
	    .fatal_stop = NULL,
	    .write = standard_pipeline_write_process,
	    .read = standard_pipeline_read_process,
	};

	queues_initialized_count = proxy_device_initialize_queues(
	    &g_device_state, queues, PROXY_MAX_QUEUES_COUNT, &ops,
	    __thread_priv_create, __thread_priv_teardown);

	if (queues_initialized_count < 0)
		return queues_initialized_count;
	return 0;
}

void destroy_queues()
{
	int i;
	for (i = 0; i < queues_initialized_count; i++) {
		proxy_device_destroy_queue_state(&queues[i]);
	}
}
