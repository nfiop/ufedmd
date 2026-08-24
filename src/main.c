/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2026 Liav A
 */

#include <stdio.h>
#include <stdlib.h>
#include <yaml.h>

#include "config/common.h"
#include "config/file_parser.h"

struct yaml_mtd_scheme scheme;

static void parse_config_file(FILE *fp)
{
	yaml_return_code_t ret;
	yaml_node_t *node;

	ret = parse_yaml_file(fp, &scheme);

	if (!YAML_RC_CHECK_SUCCESS(ret)) {
		fprintf(
		    stderr, "Error: %s\n", return_code_value_to_string(ret.rc));
		if (ret.offending != NULL) {
			node = ret.offending;
			fprintf(stderr,
			    "Offending node: lines %zu-%zu, columns %zu-%zu\n",
			    node->start_mark.line + 1, node->end_mark.line + 1,
			    node->start_mark.column + 1,
			    node->end_mark.column + 1);
		}
		exit(1);
	}
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

	parse_config_file(fp);

	fclose(fp);

exit:
	if (ret < 0) {
		fprintf(stderr, "Failed parsing: %s\n", strerror(-ret));
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
