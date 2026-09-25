/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2026 Liav A
 */

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

ufedmd_rc_t parse_codec_entries(pipeline_codec_t *codec,
    struct cfg_dict *config, struct codec_entry_parser *parsers,
    size_t nparsers)
{
	size_t entry_idx;
	ufedmd_rc_t ret;

	(void)codec;
	(void)parsers;
	(void)nparsers;

	// TODO: Add parsing code here
	for (entry_idx = 0; entry_idx < config->key_value_pairs_count;
	    entry_idx++) {
	}

	UFEDMD_RC_SET_SUCCESS(ret);
	goto exit;

exit:
	return ret;
}
