/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2026 Liav A
 */

#include "common/parse.h"

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

static int parse_uint_base10(const char *s, unsigned int *out)
{
	char *end;
	unsigned long val;

	errno = 0;
	val = strtoul(s, &end, 10);

	if (errno == ERANGE || val > UINT_MAX)
		return -1;

	if (end == s || *end != '\0')
		return -1;

	*out = (unsigned int)val;
	return 0;
}

static int parse_uint_base16(const char *s, unsigned int *out)
{
	char *end;
	unsigned long val;

	errno = 0;
	val = strtoul(s, &end, 10);

	if (errno == ERANGE || val > UINT_MAX)
		return -1;

	if (end == s || *end != '\0')
		return -1;

	*out = (unsigned int)val;
	return 0;
}

int parse_uint(const char *s, unsigned int *out)
{
	size_t len = strlen(s);
	if (len > 2 && s[0] == '0' && s[1] == 'x')
		return parse_uint_base16(&s[2], out);

	return parse_uint_base10(s, out);
}