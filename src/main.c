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

static void print_codecs_section(struct yaml_section *codecs)
{
	struct yaml_codec_obj *objs, *cur;
	size_t idx, dict_idx;
	struct yaml_dict *dict;

	objs = (struct yaml_codec_obj *)codecs->objs;

	for (idx = 0; idx < codecs->objs_count; idx++) {
		cur = &objs[idx];
		printf("codec %zu: name - %s, type - %s\n", idx, cur->name.str,
		    cur->type.str);
		dict = &cur->dict;
		for (dict_idx = 0; dict_idx < dict->key_value_pairs_count;
		    dict_idx++) {
			printf("\t%s: %s\n",
			    dict->key_val_pairs[dict_idx].key.str,
			    dict->key_val_pairs[dict_idx].value.str);
		}
	}
}

static void print_pipelines_section(struct yaml_section *pipelines)
{
	struct yaml_pipeline_obj *objs, *cur;
	size_t idx, codec_idx;
	struct yaml_string_sequence *codecs;

	objs = (struct yaml_pipeline_obj *)pipelines->objs;

	for (idx = 0; idx < pipelines->objs_count; idx++) {
		cur = &objs[idx];
		printf("pipeline %zu: name - %s\n", idx, cur->name.str);
		codecs = &cur->codecs;
		for (codec_idx = 0; codec_idx < codecs->strings_count;
		    codec_idx++) {
			printf("\t- %s\n", codecs->strings[codec_idx].str);
		}
	}
}

static void print_partitions_section(struct yaml_section *partitions)
{
	struct yaml_partition_obj *objs, *cur;
	size_t idx;

	objs = (struct yaml_partition_obj *)partitions->objs;

	for (idx = 0; idx < partitions->objs_count; idx++) {
		cur = &objs[idx];
		printf("partition %zu: name - %s, pipeline - %s (eraseblocks "
		       "[%s], pages [%s])\n",
		    idx, cur->name.str, cur->pipeline.str,
		    cur->eraseblocks_param.str, cur->pages_param.str);
	}
}

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

	print_codecs_section(&scheme._codecs);
	printf("\n");
	print_pipelines_section(&scheme._pipelines);
	printf("\n");
	print_partitions_section(&scheme._partitions);
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
