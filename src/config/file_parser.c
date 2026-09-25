/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2026 Liav A
 */

#include <inttypes.h>
#include <jansson.h>
#include <stdio.h>
#include <stdlib.h>

#include "common/array.h"
#include "config/common.h"
#include "config/file_parser.h"
#include "config/return_codes.h"

typedef cfg_return_code_t (*parse_func_t)(
    json_t *node, struct cfg_scheme *scheme);
typedef void (*deinit_func_t)(struct cfg_scheme *scheme);

struct section_parser {
	const char *name;
	parse_func_t parse;
	deinit_func_t deinit;
};

extern cfg_return_code_t cfg_codecs_parse(
    json_t *node, struct cfg_scheme *scheme);
extern cfg_return_code_t cfg_pipelines_parse(
    json_t *node, struct cfg_scheme *scheme);
extern cfg_return_code_t cfg_partitions_parse(
    json_t *node, struct cfg_scheme *scheme);

extern void cfg_codecs_destroy(struct cfg_scheme *scheme);
extern void cfg_pipelines_destroy(struct cfg_scheme *scheme);
extern void cfg_partitions_destroy(struct cfg_scheme *scheme);

static const struct section_parser parsers[] = {
    {
	.name = "codecs",
	.parse = cfg_codecs_parse,
	.deinit = cfg_codecs_destroy,
    },
    {
	.name = "pipelines",
	.parse = cfg_pipelines_parse,
	.deinit = cfg_pipelines_destroy,
    },
    {
	.name = "partitions",
	.parse = cfg_partitions_parse,
	.deinit = cfg_partitions_destroy,
    },
};

static void deinit_scheme(struct cfg_scheme *scheme, size_t max_idx)
{
	size_t section_idx;
	for (section_idx = 0; section_idx < max_idx; section_idx++) {
		parsers[section_idx].deinit(scheme);
	}
}

static cfg_return_code_t parse_root_document(
    json_t *root, struct cfg_scheme *scheme)
{
	size_t section_idx;
	cfg_return_code_t ret;
	json_t *section_node;

	for (section_idx = 0; section_idx < ARRAY_SIZE(parsers);
	    section_idx++) {
		section_node = json_object_get(root, parsers[section_idx].name);
		if (!section_node) {
			CFG_RC_SET_WITH_OFFENDING_NODE(
			    ret, CFG_RC_SECTION_NAME_UNKNOWN, section_node);
			goto error;
		}

		if (!json_is_array(section_node)) {
			CFG_RC_SET_WITH_OFFENDING_NODE(
			    ret, CFG_RC_INVALID_OBJECT_NODE, section_node);
			goto error;
		}

		ret = parsers[section_idx].parse(section_node, scheme);
		if (!CFG_RC_CHECK_SUCCESS(ret)) {
			goto error;
		}
	}

	CFG_RC_SET_SUCCESS(ret);
	goto exit;

error:
	deinit_scheme(scheme, section_idx);
exit:
	return ret;
}

cfg_return_code_t parse_config_file(FILE *fp, struct cfg_scheme *scheme)
{
	cfg_return_code_t ret;
	json_error_t error;

	CFG_RC_SET_SUCCESS(ret);

	json_t *root = json_loadf(fp, JSON_REJECT_DUPLICATES, &error);

	if (!root) {
		fprintf(stderr,
		    "JSON error: %s\n"
		    "Line: %d\n"
		    "Column: %d\n"
		    "Position: %d\n",
		    error.text, error.line, error.column, error.position);
		CFG_RC_SET(ret, CFG_RC_NO_ROOT_NODE);
		goto exit;
	}

	if (!json_is_object(root)) {
		CFG_RC_SET(ret, CFG_RC_INVALID_OBJECT_NODE);
		goto exit;
	}

	/* root now contains the parsed JSON */
	ret = parse_root_document(root, scheme);

	json_decref(root);
exit:
	return ret;
}

#define FORMAT_UINT64_RANGE_PARAMETER(__buf, __value)                          \
	do {                                                                   \
		if (__value == UINT64_MAX) {                                   \
			snprintf(__buf, sizeof(__buf), "%s", "end");           \
		} else {                                                       \
			snprintf(__buf, sizeof(__buf), "%" PRIu64, __value);   \
		}                                                              \
	} while (0)

static void print_range(struct cfg_span_specifier *range)
{
	char start_buf[MAX_NAME_LEN];
	char length_buf[MAX_NAME_LEN];

	FORMAT_UINT64_RANGE_PARAMETER(start_buf, range->_range.start);
	FORMAT_UINT64_RANGE_PARAMETER(length_buf, range->_range.length);
	printf("span name - %s, range: start - %s, length - "
	       "%s\n",
	    range->span_name.str, start_buf, length_buf);
}

void print_codecs_section(struct cfg_codecs_section *codecs)
{
	struct cfg_codec_obj *objs, *cur;
	size_t idx, dict_idx;
	struct cfg_dict *dict;

	objs = codecs->objs;

	for (idx = 0; idx < codecs->objs_count; idx++) {
		cur = &objs[idx];
		printf("Codec %zu: name - %s, type - %s\n", idx, cur->name.str,
		    cur->type.str);

		dict = &cur->special_params;

		printf("\tParameters:\n");
		for (dict_idx = 0; dict_idx < dict->key_value_pairs_count;
		    dict_idx++) {
			printf(
			    "\t  %s: ", dict->key_val_pairs[dict_idx].key.str);
			print_cfg_scalar_value(
			    &dict->key_val_pairs[dict_idx].value);
			printf("\n");
		}
	}
}

void print_pipelines_section(struct cfg_pipelines_section *pipelines)
{
	struct cfg_pipeline_obj *objs, *cur;
	size_t idx, codec_idx, part_idx;
	struct cfg_write_codecs_sequence *write_codecs;
	struct cfg_read_codecs_sequence *read_codecs;
	struct cfg_page_layout_obj *page_layout;
	struct cfg_page_layout_part_obj *part;
	struct cfg_read_codec_specifier_obj *read_codec_specifier;
	struct cfg_write_codec_specifier_obj *write_codec_specifier;

	objs = pipelines->objs;

	char part_start_buf[128];
	char part_length_buf[128];

	for (idx = 0; idx < pipelines->objs_count; idx++) {
		cur = &objs[idx];
		printf("Pipeline %zu: name - %s, %s\n", idx, cur->name.str,
		    cur->write_codecs ? "rw" : "ro");

		page_layout = &cur->page_layout;
		printf("\tPage layout:\n");
		for (part_idx = 0; part_idx < page_layout->parts_count;
		    part_idx++) {
			part = &page_layout->parts[part_idx];
			FORMAT_UINT64_RANGE_PARAMETER(
			    part_start_buf, part->_range.start);
			FORMAT_UINT64_RANGE_PARAMETER(
			    part_length_buf, part->_range.length);
			printf(
			    "\t  - Part %zu: name - %s, span name - %s [start: "
			    "%s, length %s]\n",
			    part_idx + 1, part->part_name.str,
			    part->span_name.str, part_start_buf,
			    part_length_buf);
		}
		printf("\tRead request spans:\n");
		printf(
		    "\t - DATA: %s\n", cur->read_mappings.data_span_name.str);
		printf("\t - OOB: %s\n", cur->read_mappings.oob_span_name.str);
		if (cur->write_mappings) {
			printf("\tWrite request spans:\n");
			printf("\t - DATA: %s\n",
			    cur->write_mappings->data_span_name.str);
			printf("\t - OOB: %s\n",
			    cur->write_mappings->oob_span_name.str);
		}
		read_codecs = &cur->read_codecs;
		printf("\tRead codecs:\n");
		for (codec_idx = 0; codec_idx < read_codecs->objs_count;
		    codec_idx++) {
			read_codec_specifier = &read_codecs->objs[codec_idx];
			printf("\t  - %s:\n",
			    read_codec_specifier->codec_name.str);
			printf("\t    Data range => ");
			print_range(&read_codec_specifier->data);
			if (read_codec_specifier->has_valid_oob_specifier) {
				printf("\t    OOB range => ");
				print_range(&read_codec_specifier->oob);
			}
		}

		if (cur->write_codecs) {
			write_codecs = cur->write_codecs;
			printf("\tWrite codecs:\n");
			for (codec_idx = 0;
			    codec_idx < write_codecs->objs_count; codec_idx++) {
				write_codec_specifier =
				    &write_codecs->objs[codec_idx];
				printf("\t  - %s:\n",
				    write_codec_specifier->codec_name.str);
				if (write_codec_specifier->src) {
					printf("\t    Source range => ");
					print_range(write_codec_specifier->src);
				}

				printf("\t    Destination range => ");
				print_range(&write_codec_specifier->dest);
			}
		}
	}
}

void print_partitions_section(struct cfg_partitions_section *partitions)
{
	struct cfg_partition_obj *objs, *cur;
	size_t idx;

	objs = partitions->objs;
	char eraseblock_start_buf[128];
	char eraseblock_length_buf[128];
	char pages_start_buf[128];
	char pages_length_buf[128];

	for (idx = 0; idx < partitions->objs_count; idx++) {
		cur = &objs[idx];

		FORMAT_UINT64_RANGE_PARAMETER(
		    eraseblock_start_buf, cur->eraseblocks.start);
		FORMAT_UINT64_RANGE_PARAMETER(
		    eraseblock_length_buf, cur->eraseblocks.length);
		FORMAT_UINT64_RANGE_PARAMETER(
		    pages_start_buf, cur->pages.start);
		FORMAT_UINT64_RANGE_PARAMETER(
		    pages_length_buf, cur->pages.length);

		printf(
		    "Partition %zu: name - %s, pipeline - %s (eraseblocks "
		    "[start: %s, length: %s], pages [start: %s, length: %s])\n",
		    idx, cur->name.str, cur->pipeline.str, eraseblock_start_buf,
		    eraseblock_length_buf, pages_start_buf, pages_length_buf);
	}
}
