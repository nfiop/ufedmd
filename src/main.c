/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2026 Liav A
 */

#include <asm-generic/errno.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
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

/* Externally recognized, so let's make it truly global. */
struct program_params prog_params;

static struct cfg_scheme s_cfg_scheme;
static struct nand_pipeline_scheme s_scheme;
static atomic_bool stop = false;
static struct proxy_device_queue_state queues[PROXY_MAX_QUEUES_COUNT];
static struct proxy_device_state state;

static struct option long_options[] = {{"help", no_argument, 0, 'h'},
    {"verbose", no_argument, 0, 'v'}, {"timeout", no_argument, 0, 't'},
    {"exit-on-error", no_argument, 0, 'E'},
    {"exit-on-nack", no_argument, 0, 'e'}};

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

static void print_help(const char *prog)
{
	printf("Usage: %s [options] <ufedm_proxy> <json_file>\n\n", prog);

	printf("Options:\n");
	printf("  -h, --help                     Show help.\n");
	printf("  -v, --verbose                  Increase verbose output "
	       "level.\n");
	printf("  -t, --timeout                  Don't answer with ioctls at "
	       "all.\n");
	printf("  -e, --exit-on-nack             Exit on first real NACK.\n");
	printf("  -E, --exit-on-error            Exit on first error.\n");
	printf("  -W, --nack-writes <errno>      Answer with NACKs with a "
	       "specific errno for write requests.\n");
	printf("  -R, --nack-reads <errno>      Answer with NACKs with a "
	       "specific errno for read requests.\n");
}

void print_params()
{
	if (!prog_params.verbose)
		return;

	printf("Verbose enabled\n");

	printf("Device: %s\n", prog_params.device_path);
	printf("Configuration: %s\n", prog_params.config_path);

	if (prog_params.exit_on_error)
		printf("Exit on first error enabled\n");
}

int handle_params(int argc, char **argv)
{
	int opt;
	int failed_parsing_nack_reads_errno = 0;
	int failed_parsing_nack_writes_errno = 0;

	while ((opt = getopt_long(
		    argc, argv, "hvtDEeN:Iw", long_options, NULL)) != -1) {
		switch (opt) {
		case 'h':
			print_help(argv[0]);
			return EXIT_SUCCESS;

		case 'v':
			prog_params.verbose += 1;
			break;

		case 'E':
			prog_params.exit_on_error = 1;
			break;

		case 'e':
			prog_params.exit_on_nack = 1;
			break;

		default:
			print_help(argv[0]);
			return 1;
		}
	}

	/* Remaining argument is the file */
	if (argc - optind != 2) {
		fprintf(stderr, "Error: missing file argument\n\n");
		print_help(argv[0]);
		return 1;
	}

	prog_params.device_path = argv[optind];
	prog_params.config_path = argv[optind + 1];

	if (failed_parsing_nack_reads_errno < 0) {
		fprintf(stderr, "Failed parsing NACK reads errno");
		return 1;
	}

	if (failed_parsing_nack_writes_errno < 0) {
		fprintf(stderr, "Failed parsing NACK reads errno");
		return 1;
	}

	return 0;
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
	UFEDMD_RC_SET_SUCCESS(ret);

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
