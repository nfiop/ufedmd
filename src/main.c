/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2026 Liav A
 */

#include <asm-generic/errno.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/eventfd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

/* Kernel module headers */
#include "proxy_dev.h"
#include "proxy_ioctl.h"
#include "shared_mem.h"

#include "common/parse.h"
#include "common/program_params.h"
#include "config/common.h"
#include "config/file_parser.h"
#include "pipeline/scheme.h"
#include "return_codes.h"

#include "params.h"

/* Externally recognized, so let's make it truly global. */
struct program_params prog_params;

static struct cfg_scheme s_cfg_scheme;
static struct nand_pipeline_scheme s_scheme;
static atomic_bool stop = false;
static struct proxy_device_queue_state queues[PROXY_MAX_QUEUES_COUNT];
static struct proxy_device_state state;

static cfg_return_code_t parse_given_config_file(FILE *fp)
{
	cfg_return_code_t ret;

	ret = parse_config_file(fp, &s_cfg_scheme);

	if (!CFG_RC_CHECK_SUCCESS(ret)) {
		fprintf(
		    stderr, "Error: %s\n", return_code_value_to_string(ret.rc));
		fprintf(stderr, "Failed at %s: %s, line %u\n", ret.filename,
		    ret._func, ret.line);
		goto exit;
	}

	CFG_RC_SET_SUCCESS(ret);
exit:
	return ret;
}

static void sigint_handler(int sig)
{
	(void)sig;
	atomic_store(&stop, true);
}

static int create_scheme(struct proxy_device_state *state, char *config_path)
{
	(void)state;

	ufedmd_rc_t ret;
	cfg_return_code_t cfg_ret;
	FILE *fp = fopen(config_path, "rb");
	if (!fp) {
		UFEDMD_RC_SET(ret, UFEDMD_OPEN_CONFIGURATION_FILE_FAILED);
		goto exit;
	}

	cfg_ret = parse_given_config_file(fp);
	if (!CFG_RC_CHECK_SUCCESS(cfg_ret)) {
		UFEDMD_RC_SET(ret, UFEDMD_RC_JSON_PARSER_FAILED);
		goto exit;
	}

	if (prog_params.verbose) {
		printf(
		    "JSON configuration file - %s, containing:\n", config_path);
		print_codecs_section(&s_cfg_scheme.codecs);
		printf("\n");
		print_pipelines_section(&s_cfg_scheme.pipelines);
		printf("\n");
		print_partitions_section(&s_cfg_scheme.partitions);
	}

	ret = initialize_nand_pipeline_scheme(
	    &s_scheme, &state->mtd_info, &s_cfg_scheme);

	fclose(fp);

exit:
	if (!UFEDMD_RC_CHECK_SUCCESS(ret)) {
		fprintf(stderr, "Error: file - %s, line - %d\n", ret.filename,
		    ret.line);
		return -EINVAL;
	}
	return 0;
}

int main(int argc, char **argv)
{
	int ret;
	struct timespec ts;

	struct sigaction sa = {
	    .sa_handler = sigint_handler,
	};

	memset(&prog_params, 0, sizeof(prog_params));

	if (handle_params(argc, argv) != 0)
		return EXIT_FAILURE;

	print_params();

	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	ret = sigaction(SIGINT, &sa, NULL);

	if (ret < 0) {
		perror("sigaction");
		goto exit;
	}

	ret = open_proxy_device_state(prog_params.device_path, &state);
	if (ret < 0) {
		fprintf(stderr, "Failed to open %s: %s\n",
		    prog_params.device_path, strerror(-ret));
		goto exit;
	}

	ret = create_scheme(&state, prog_params.config_path);
	if (ret < 0)
		goto destroy_proxy_device_state;

	ts.tv_sec = 0;
	ts.tv_nsec = 100 * 1000 * 1000; // 100 ms
	while (!atomic_load(&stop)) {
		nanosleep(&ts, NULL);
	}

	destroy_proxy_dev_queue_state(&queues[0]);
	destroy_proxy_dev_queue_state(&queues[1]);

	// FIXME: Destroy NAND pipeline scheme on quit

destroy_proxy_device_state:
	close_proxy_device(&state);
exit:
	return ret == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
