/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2026 Liav A
 */

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>

#include "common/program_params.h"
#include "params.h"

extern struct program_params prog_params;

static struct option long_options[] = {{"help", no_argument, 0, 'h'},
    {"verbose", no_argument, 0, 'v'}, {"timeout", no_argument, 0, 't'},
    {"exit-on-error", no_argument, 0, 'E'},
    {"exit-on-nack", no_argument, 0, 'e'}};

void print_help(const char *prog)
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
	return 0;
}
