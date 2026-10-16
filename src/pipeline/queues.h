/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2026 Liav A
 */

#ifndef __PIPELINE_QUEUES__H_
#define __PIPELINE_QUEUES__H_

#include <stdio.h>
#include <unistd.h>

#include "pipeline/scheme.h"
#include "proxy_dev.h"

int create_queues(void);
void destroy_queues(void);

#endif
