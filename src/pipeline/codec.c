/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2026 Liav A
 */

#include <assert.h>
#include <string.h>

#include "pipeline/codec.h"

ufedmd_rc_t create_standard_codec(pipeline_codec_t *codec,
    void (*deinit_callback)(struct pipeline_codec *), struct cfg_dict *config,
    struct codec_entry_parser *parsers, size_t nparsers,
    struct write_ops *write_ops, struct read_ops *read_ops)

{
	ufedmd_rc_t ret;

	ret = parse_codec_entries(codec, config, parsers, nparsers);
	if (!UFEDMD_RC_CHECK_SUCCESS(ret))
		goto exit;

	codec->write_ops = write_ops;
	codec->read_ops = read_ops;
	codec->deinit = deinit_callback;

	UFEDMD_RC_SET_SUCCESS(ret);
exit:
	return ret;
}

void default_revert_entry(pipeline_codec_t *codec)
{
	/* Do nothing here... */
	UNUSED(codec);
}

struct codec_param_entry {
	struct codec_entry_parser *parser;
	struct key_value_pair *pair;
};

static ufedmd_rc_t create_handlers_array(
    struct codec_param_entry **ordered_entriesp, struct cfg_dict *config,
    struct codec_entry_parser *parsers, size_t nparsers)
{
	size_t idx;
	void *hashmap_ret;
	ufedmd_rc_t ret;
	struct key_value_pair pair;
	struct codec_entry_parser *parser;
	struct codec_param_entry *ordered;

	ordered = calloc(nparsers, sizeof(struct codec_param_entry));
	if (!(ordered)) {
		UFEDMD_RC_SET(ret, UFEDMD_RC_MEMORY_ALLOCATION_FAILED);
		goto exit;
	}

	for (idx = 0; idx < nparsers; idx++) {
		parser = &parsers[idx];
		pair.key.str = (char *)parser->key;
		pair.key.len = strlen(parser->key);
		hashmap_ret = (void *)hashmap_get(config->values, &pair);
		if (!hashmap_ret && parser->required) {
			UFEDMD_RC_SET(
			    ret, UFEDMD_RC_CODEC_HAS_MISSING_PARAMETER);
			goto free_parsers;
		}

		ordered[idx].parser = parser;
		/* This can be NULL as well... */
		ordered[idx].pair = hashmap_ret;
	}

	*ordered_entriesp = ordered;
	UFEDMD_RC_SET_SUCCESS(ret);
	goto exit;

free_parsers:
	free(ordered);
exit:
	return ret;
}

static void do_revert_entries(pipeline_codec_t *codec,
    struct codec_param_entry *ordered_entries, size_t max_idx)
{
	size_t idx;
	for (idx = 0; idx < max_idx; idx++) {
		if (!ordered_entries[idx].pair)
			continue;

		ordered_entries[idx].parser->revert(codec);
	}
}

ufedmd_rc_t parse_value(
    pipeline_codec_t *codec, struct codec_param_entry *entry)
{
	ufedmd_rc_t ret;
	char *str;
	union {
		bool flag;
		unsigned int uinteger;
		int integer;
		double real;
	} value;

	switch (entry->parser->type) {
	case CFG_VALUE_PARAM_TYPE_STRING:
		/* String is different from other types - other types are
		 * are passed through a pointer to a union.
		 * A string is already a pointer, so we pass it directly.
		 */
		if (entry->pair->value.type != CFG_VALUE_PARAM_TYPE_STRING) {
			UFEDMD_RC_SET(
			    ret, UFDEMD_RC_CODEC_ENTRY_TYPE_NOT_MATCHING);
			goto exit;
		}

		str = entry->pair->value._value.string.str;
		ret = entry->parser->handle(codec, (void *)str);
		goto exit;
	case CFG_VALUE_PARAM_TYPE_INTEGER:
		if (entry->pair->value.type != CFG_VALUE_PARAM_TYPE_INTEGER) {
			UFEDMD_RC_SET(
			    ret, UFDEMD_RC_CODEC_ENTRY_TYPE_NOT_MATCHING);
			goto exit;
		}

		value.integer = entry->pair->value._value.integer;
		break;
	case CFG_VALUE_PARAM_TYPE_UNSIGNED_INTEGER:
		/* An unsigned number is not a "native" JSON type.
		 * To support it and "offload" checks from parsing code,
		 * we accept a raw integer and check if it's not negative.
		 */
		if (entry->pair->value.type != CFG_VALUE_PARAM_TYPE_INTEGER) {
			UFEDMD_RC_SET(
			    ret, UFDEMD_RC_CODEC_ENTRY_TYPE_NOT_MATCHING);
			goto exit;
		}

		if (entry->pair->value._value.integer < 0) {
			UFEDMD_RC_SET(
			    ret, UFDEMD_RC_CODEC_ENTRY_TYPE_NOT_MATCHING);
			goto exit;
		}

		value.uinteger =
		    (unsigned int)entry->pair->value._value.integer;
		break;
	case CFG_VALUE_PARAM_TYPE_BOOLEAN:
		if (entry->pair->value.type != CFG_VALUE_PARAM_TYPE_BOOLEAN) {
			UFEDMD_RC_SET(
			    ret, UFDEMD_RC_CODEC_ENTRY_TYPE_NOT_MATCHING);
			goto exit;
		}

		value.flag = entry->pair->value._value.flag;
		break;
	case CFG_VALUE_PARAM_TYPE_DOUBLE:
		if (entry->pair->value.type != CFG_VALUE_PARAM_TYPE_DOUBLE) {
			UFEDMD_RC_SET(
			    ret, UFDEMD_RC_CODEC_ENTRY_TYPE_NOT_MATCHING);
			goto exit;
		}

		value.real = entry->pair->value._value.real;
		break;
	default:
		assert(false);
	}

	ret = entry->parser->handle(codec, (void *)&value);
exit:
	return ret;
}

ufedmd_rc_t parse_codec_entries(pipeline_codec_t *codec,
    struct cfg_dict *config, struct codec_entry_parser *parsers,
    size_t nparsers)
{
	ufedmd_rc_t ret;
	size_t entry_idx;
	struct codec_param_entry *ordered_entries;

	if (hashmap_count(config->values) > nparsers) {
		UFEDMD_RC_SET(ret, UFEDMD_RC_CODEC_HAS_UNKNOWN_PARAMETERS);
		goto exit;
	}

	ret =
	    create_handlers_array(&ordered_entries, config, parsers, nparsers);
	if (!UFEDMD_RC_CHECK_SUCCESS(ret))
		goto exit;

	for (entry_idx = 0; entry_idx < nparsers; entry_idx++) {
		/* A pair can be NULL, so don't run it */
		if (!ordered_entries[entry_idx].pair) {
			continue;
		}

		ret = parse_value(codec, &ordered_entries[entry_idx]);
		if (!UFEDMD_RC_CHECK_SUCCESS(ret)) {
			goto revert_entries;
		}
	}

	UFEDMD_RC_SET_SUCCESS(ret);
	goto free_handlers;

revert_entries:
	do_revert_entries(codec, ordered_entries, entry_idx);
free_handlers:
	free(ordered_entries);
exit:
	return ret;
}
