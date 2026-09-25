/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2026 Liav A
 */

#include "common/logging.h"

#include <ctype.h>

void hexdump(FILE *out, const void *buf, size_t len)
{
	const unsigned char *p = buf;
	size_t i, j;

	for (i = 0; i < len; i += 16) {
		fprintf(out, "%08zx  ", i);

		/* Hex bytes */
		for (j = 0; j < 16; j++) {
			if (i + j < len)
				fprintf(out, "%02x ", p[i + j]);
			else
				fprintf(out, "   ");

			if (j == 7)
				fprintf(out, " ");
		}

		fprintf(out, " |");

		/* ASCII representation */
		for (j = 0; j < 16 && i + j < len; j++) {
			unsigned char c = p[i + j];

			fputc(isprint(c) ? c : '.', out);
		}

		fprintf(out, "|\n");
	}
}
