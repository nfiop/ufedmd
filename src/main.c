/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2026 Liav A
 */

#include <stdio.h>
#include <stdlib.h>
#include <yaml.h>

#include "config/file_parser.h"

struct yaml_mtd_scheme scheme;

static int parse_config_file(FILE *fp)
{
	yaml_return_code_t ret;

	ret = parse_yaml_file(fp, &scheme);

	if (ret != YAML_RC_SUCCESS)
		return -1;

	return 0;
}

int main(int argc, char **argv)
{
	int ret;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s <file.yaml>\n", argv[0]);
		goto exit;
	}

	FILE *fp = fopen(argv[1], "rb");
	if (!fp) {
		ret = -1;
		goto exit;
	}

	ret = parse_config_file(fp);

	fclose(fp);

exit:
	if (ret < 0) {
		fprintf(stderr, "Failed parsing: %s\n", strerror(-ret));
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
