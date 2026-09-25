// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * NOTE!: This was taken from the Linux kernel NAND software Hamming ECC
 * support module, author is Frans Meulenbroeks <fransmeulenbroeks@gmail.com>.
 * This is NOT ORIGINAL CODE of the nfiop project NOR WAS INFLUENCED from the
 * nfiop project in any way possible. In other words - I am not the author,
 * and I cannot guarantee anything about the quality of this code.
 * It is taken almost as-is, only to be used as an Hamming ECC engine
 * for ufedmd.
 *
 *
 * This file contains an ECC algorithm that detects and corrects 1 bit
 * errors in a 256 byte block of data.
 *
 * Copyright © 2008 Koninklijke Philips Electronics NV.
 *                  Author: Frans Meulenbroeks
 *
 * Completely replaces the previous ECC implementation which was written by:
 *   Steven J. Hill (sjhill@realitydiluted.com)
 *   Thomas Gleixner (tglx@linutronix.de)
 *
 * Information on how this algorithm works and how it was developed
 * can be found in Documentation/driver-api/mtd/nand_ecc.rst
 *
 */

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "defs.h"

#include "common/array.h"
#include "common/ints.h"
#include "config/objs/codec.h"
#include "pipeline/codec.h"
#include "pipeline/page_layout.h"
#include "pipeline/types.h"
#include "return_codes.h"

#include "common/logging.h"
#include "common/parse.h"

/* Kernel module headers */
#include "proxy_ioctl.h"

#include <assert.h>

/*
 * invparity is a 256 byte table that contains the odd parity
 * for each byte. So if the number of bits in a byte is even,
 * the array element is 1, and when the number of bits is odd
 * the array eleemnt is 0.
 */
static const char invparity[256] = {1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0,
    1, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
    1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0,
    1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1,
    1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1,
    0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1,
    0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0,
    1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1,
    1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0,
    1, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
    1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1};

/*
 * bitsperbyte contains the number of bits per byte
 * this is only used for testing and repairing parity
 * (a precalculated value slightly improves performance)
 */
static const char bitsperbyte[256] = {
    0,
    1,
    1,
    2,
    1,
    2,
    2,
    3,
    1,
    2,
    2,
    3,
    2,
    3,
    3,
    4,
    1,
    2,
    2,
    3,
    2,
    3,
    3,
    4,
    2,
    3,
    3,
    4,
    3,
    4,
    4,
    5,
    1,
    2,
    2,
    3,
    2,
    3,
    3,
    4,
    2,
    3,
    3,
    4,
    3,
    4,
    4,
    5,
    2,
    3,
    3,
    4,
    3,
    4,
    4,
    5,
    3,
    4,
    4,
    5,
    4,
    5,
    5,
    6,
    1,
    2,
    2,
    3,
    2,
    3,
    3,
    4,
    2,
    3,
    3,
    4,
    3,
    4,
    4,
    5,
    2,
    3,
    3,
    4,
    3,
    4,
    4,
    5,
    3,
    4,
    4,
    5,
    4,
    5,
    5,
    6,
    2,
    3,
    3,
    4,
    3,
    4,
    4,
    5,
    3,
    4,
    4,
    5,
    4,
    5,
    5,
    6,
    3,
    4,
    4,
    5,
    4,
    5,
    5,
    6,
    4,
    5,
    5,
    6,
    5,
    6,
    6,
    7,
    1,
    2,
    2,
    3,
    2,
    3,
    3,
    4,
    2,
    3,
    3,
    4,
    3,
    4,
    4,
    5,
    2,
    3,
    3,
    4,
    3,
    4,
    4,
    5,
    3,
    4,
    4,
    5,
    4,
    5,
    5,
    6,
    2,
    3,
    3,
    4,
    3,
    4,
    4,
    5,
    3,
    4,
    4,
    5,
    4,
    5,
    5,
    6,
    3,
    4,
    4,
    5,
    4,
    5,
    5,
    6,
    4,
    5,
    5,
    6,
    5,
    6,
    6,
    7,
    2,
    3,
    3,
    4,
    3,
    4,
    4,
    5,
    3,
    4,
    4,
    5,
    4,
    5,
    5,
    6,
    3,
    4,
    4,
    5,
    4,
    5,
    5,
    6,
    4,
    5,
    5,
    6,
    5,
    6,
    6,
    7,
    3,
    4,
    4,
    5,
    4,
    5,
    5,
    6,
    4,
    5,
    5,
    6,
    5,
    6,
    6,
    7,
    4,
    5,
    5,
    6,
    5,
    6,
    6,
    7,
    5,
    6,
    6,
    7,
    6,
    7,
    7,
    8,
};

/*
 * addressbits is a lookup table to filter out the bits from the xor-ed
 * ECC data that identify the faulty location.
 * this is only used for repairing parity
 * see the comments in nand_ecc_sw_hamming_correct for more details
 */
static const char addressbits[256] = {0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x01,
    0x01, 0x02, 0x02, 0x03, 0x03, 0x02, 0x02, 0x03, 0x03, 0x00, 0x00, 0x01,
    0x01, 0x00, 0x00, 0x01, 0x01, 0x02, 0x02, 0x03, 0x03, 0x02, 0x02, 0x03,
    0x03, 0x04, 0x04, 0x05, 0x05, 0x04, 0x04, 0x05, 0x05, 0x06, 0x06, 0x07,
    0x07, 0x06, 0x06, 0x07, 0x07, 0x04, 0x04, 0x05, 0x05, 0x04, 0x04, 0x05,
    0x05, 0x06, 0x06, 0x07, 0x07, 0x06, 0x06, 0x07, 0x07, 0x00, 0x00, 0x01,
    0x01, 0x00, 0x00, 0x01, 0x01, 0x02, 0x02, 0x03, 0x03, 0x02, 0x02, 0x03,
    0x03, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x01, 0x01, 0x02, 0x02, 0x03,
    0x03, 0x02, 0x02, 0x03, 0x03, 0x04, 0x04, 0x05, 0x05, 0x04, 0x04, 0x05,
    0x05, 0x06, 0x06, 0x07, 0x07, 0x06, 0x06, 0x07, 0x07, 0x04, 0x04, 0x05,
    0x05, 0x04, 0x04, 0x05, 0x05, 0x06, 0x06, 0x07, 0x07, 0x06, 0x06, 0x07,
    0x07, 0x08, 0x08, 0x09, 0x09, 0x08, 0x08, 0x09, 0x09, 0x0a, 0x0a, 0x0b,
    0x0b, 0x0a, 0x0a, 0x0b, 0x0b, 0x08, 0x08, 0x09, 0x09, 0x08, 0x08, 0x09,
    0x09, 0x0a, 0x0a, 0x0b, 0x0b, 0x0a, 0x0a, 0x0b, 0x0b, 0x0c, 0x0c, 0x0d,
    0x0d, 0x0c, 0x0c, 0x0d, 0x0d, 0x0e, 0x0e, 0x0f, 0x0f, 0x0e, 0x0e, 0x0f,
    0x0f, 0x0c, 0x0c, 0x0d, 0x0d, 0x0c, 0x0c, 0x0d, 0x0d, 0x0e, 0x0e, 0x0f,
    0x0f, 0x0e, 0x0e, 0x0f, 0x0f, 0x08, 0x08, 0x09, 0x09, 0x08, 0x08, 0x09,
    0x09, 0x0a, 0x0a, 0x0b, 0x0b, 0x0a, 0x0a, 0x0b, 0x0b, 0x08, 0x08, 0x09,
    0x09, 0x08, 0x08, 0x09, 0x09, 0x0a, 0x0a, 0x0b, 0x0b, 0x0a, 0x0a, 0x0b,
    0x0b, 0x0c, 0x0c, 0x0d, 0x0d, 0x0c, 0x0c, 0x0d, 0x0d, 0x0e, 0x0e, 0x0f,
    0x0f, 0x0e, 0x0e, 0x0f, 0x0f, 0x0c, 0x0c, 0x0d, 0x0d, 0x0c, 0x0c, 0x0d,
    0x0d, 0x0e, 0x0e, 0x0f, 0x0f, 0x0e, 0x0e, 0x0f, 0x0f};

static int ecc_sw_hamming_calculate(const unsigned char *buf,
    unsigned int step_size, unsigned char *code, bool sm_order)
{
	const u32 *bp = (uint32_t *)buf;
	const u32 eccsize_mult = (step_size == 256) ? 1 : 2;
	/* current value in buffer */
	u32 cur;
	/* rp0..rp17 are the various accumulated parities (per byte) */
	u32 rp0, rp1, rp2, rp3, rp4, rp5, rp6, rp7, rp8, rp9, rp10, rp11, rp12,
	    rp13, rp14, rp15, rp16, rp17;
	/* Cumulative parity for all data */
	u32 par;
	/* Cumulative parity at the end of the loop (rp12, rp14, rp16) */
	u32 tmppar;
	u32 i;

	par = 0;
	rp4 = 0;
	rp6 = 0;
	rp8 = 0;
	rp10 = 0;
	rp12 = 0;
	rp14 = 0;
	rp16 = 0;
	rp17 = 0;

	/*
	 * The loop is unrolled a number of times;
	 * This avoids if statements to decide on which rp value to update
	 * Also we process the data by longwords.
	 * Note: passing unaligned data might give a performance penalty.
	 * It is assumed that the buffers are aligned.
	 * tmppar is the cumulative sum of this iteration.
	 * needed for calculating rp12, rp14, rp16 and par
	 * also used as a performance improvement for rp6, rp8 and rp10
	 */
	for (i = 0; i < eccsize_mult << 2; i++) {
		cur = *bp++;
		tmppar = cur;
		rp4 ^= cur;
		cur = *bp++;
		tmppar ^= cur;
		rp6 ^= tmppar;
		cur = *bp++;
		tmppar ^= cur;
		rp4 ^= cur;
		cur = *bp++;
		tmppar ^= cur;
		rp8 ^= tmppar;

		cur = *bp++;
		tmppar ^= cur;
		rp4 ^= cur;
		rp6 ^= cur;
		cur = *bp++;
		tmppar ^= cur;
		rp6 ^= cur;
		cur = *bp++;
		tmppar ^= cur;
		rp4 ^= cur;
		cur = *bp++;
		tmppar ^= cur;
		rp10 ^= tmppar;

		cur = *bp++;
		tmppar ^= cur;
		rp4 ^= cur;
		rp6 ^= cur;
		rp8 ^= cur;
		cur = *bp++;
		tmppar ^= cur;
		rp6 ^= cur;
		rp8 ^= cur;
		cur = *bp++;
		tmppar ^= cur;
		rp4 ^= cur;
		rp8 ^= cur;
		cur = *bp++;
		tmppar ^= cur;
		rp8 ^= cur;

		cur = *bp++;
		tmppar ^= cur;
		rp4 ^= cur;
		rp6 ^= cur;
		cur = *bp++;
		tmppar ^= cur;
		rp6 ^= cur;
		cur = *bp++;
		tmppar ^= cur;
		rp4 ^= cur;
		cur = *bp++;
		tmppar ^= cur;

		par ^= tmppar;
		if ((i & 0x1) == 0)
			rp12 ^= tmppar;
		if ((i & 0x2) == 0)
			rp14 ^= tmppar;
		if (eccsize_mult == 2 && (i & 0x4) == 0)
			rp16 ^= tmppar;
	}

	/*
	 * handle the fact that we use longword operations
	 * we'll bring rp4..rp14..rp16 back to single byte entities by
	 * shifting and xoring first fold the upper and lower 16 bits,
	 * then the upper and lower 8 bits.
	 */
	rp4 ^= (rp4 >> 16);
	rp4 ^= (rp4 >> 8);
	rp4 &= 0xff;
	rp6 ^= (rp6 >> 16);
	rp6 ^= (rp6 >> 8);
	rp6 &= 0xff;
	rp8 ^= (rp8 >> 16);
	rp8 ^= (rp8 >> 8);
	rp8 &= 0xff;
	rp10 ^= (rp10 >> 16);
	rp10 ^= (rp10 >> 8);
	rp10 &= 0xff;
	rp12 ^= (rp12 >> 16);
	rp12 ^= (rp12 >> 8);
	rp12 &= 0xff;
	rp14 ^= (rp14 >> 16);
	rp14 ^= (rp14 >> 8);
	rp14 &= 0xff;
	if (eccsize_mult == 2) {
		rp16 ^= (rp16 >> 16);
		rp16 ^= (rp16 >> 8);
		rp16 &= 0xff;
	}

	/*
	 * we also need to calculate the row parity for rp0..rp3
	 * This is present in par, because par is now
	 * rp3 rp3 rp2 rp2 in little endian and
	 * rp2 rp2 rp3 rp3 in big endian
	 * as well as
	 * rp1 rp0 rp1 rp0 in little endian and
	 * rp0 rp1 rp0 rp1 in big endian
	 * First calculate rp2 and rp3
	 */
#ifdef __BIG_ENDIAN
	rp2 = (par >> 16);
	rp2 ^= (rp2 >> 8);
	rp2 &= 0xff;
	rp3 = par & 0xffff;
	rp3 ^= (rp3 >> 8);
	rp3 &= 0xff;
#else
	rp3 = (par >> 16);
	rp3 ^= (rp3 >> 8);
	rp3 &= 0xff;
	rp2 = par & 0xffff;
	rp2 ^= (rp2 >> 8);
	rp2 &= 0xff;
#endif

	/* reduce par to 16 bits then calculate rp1 and rp0 */
	par ^= (par >> 16);
#ifdef __BIG_ENDIAN
	rp0 = (par >> 8) & 0xff;
	rp1 = (par & 0xff);
#else
	rp1 = (par >> 8) & 0xff;
	rp0 = (par & 0xff);
#endif

	/* finally reduce par to 8 bits */
	par ^= (par >> 8);
	par &= 0xff;

	/*
	 * and calculate rp5..rp15..rp17
	 * note that par = rp4 ^ rp5 and due to the commutative property
	 * of the ^ operator we can say:
	 * rp5 = (par ^ rp4);
	 * The & 0xff seems superfluous, but benchmarking learned that
	 * leaving it out gives slightly worse results. No idea why, probably
	 * it has to do with the way the pipeline in pentium is organized.
	 */
	rp5 = (par ^ rp4) & 0xff;
	rp7 = (par ^ rp6) & 0xff;
	rp9 = (par ^ rp8) & 0xff;
	rp11 = (par ^ rp10) & 0xff;
	rp13 = (par ^ rp12) & 0xff;
	rp15 = (par ^ rp14) & 0xff;
	if (eccsize_mult == 2)
		rp17 = (par ^ rp16) & 0xff;

	/*
	 * Finally calculate the ECC bits.
	 * Again here it might seem that there are performance optimisations
	 * possible, but benchmarks showed that on the system this is developed
	 * the code below is the fastest
	 */
	if (sm_order) {
		code[0] = (invparity[rp7] << 7) | (invparity[rp6] << 6) |
			  (invparity[rp5] << 5) | (invparity[rp4] << 4) |
			  (invparity[rp3] << 3) | (invparity[rp2] << 2) |
			  (invparity[rp1] << 1) | (invparity[rp0]);
		code[1] = (invparity[rp15] << 7) | (invparity[rp14] << 6) |
			  (invparity[rp13] << 5) | (invparity[rp12] << 4) |
			  (invparity[rp11] << 3) | (invparity[rp10] << 2) |
			  (invparity[rp9] << 1) | (invparity[rp8]);
	} else {
		code[1] = (invparity[rp7] << 7) | (invparity[rp6] << 6) |
			  (invparity[rp5] << 5) | (invparity[rp4] << 4) |
			  (invparity[rp3] << 3) | (invparity[rp2] << 2) |
			  (invparity[rp1] << 1) | (invparity[rp0]);
		code[0] = (invparity[rp15] << 7) | (invparity[rp14] << 6) |
			  (invparity[rp13] << 5) | (invparity[rp12] << 4) |
			  (invparity[rp11] << 3) | (invparity[rp10] << 2) |
			  (invparity[rp9] << 1) | (invparity[rp8]);
	}

	if (eccsize_mult == 1)
		code[2] = (invparity[par & 0xf0] << 7) |
			  (invparity[par & 0x0f] << 6) |
			  (invparity[par & 0xcc] << 5) |
			  (invparity[par & 0x33] << 4) |
			  (invparity[par & 0xaa] << 3) |
			  (invparity[par & 0x55] << 2) | 3;
	else
		code[2] = (invparity[par & 0xf0] << 7) |
			  (invparity[par & 0x0f] << 6) |
			  (invparity[par & 0xcc] << 5) |
			  (invparity[par & 0x33] << 4) |
			  (invparity[par & 0xaa] << 3) |
			  (invparity[par & 0x55] << 2) |
			  (invparity[rp17] << 1) | (invparity[rp16] << 0);

	return 0;
}

static int ecc_sw_hamming_correct(unsigned char *buf, unsigned char *read_ecc,
    unsigned char *calc_ecc, unsigned int step_size, bool sm_order)
{
	const u32 eccsize_mult = step_size >> 8;
	unsigned char b0, b1, b2, bit_addr;
	unsigned int byte_addr;

	/*
	 * b0 to b2 indicate which bit is faulty (if any)
	 * we might need the xor result  more than once,
	 * so keep them in a local var
	 */
	if (sm_order) {
		b0 = read_ecc[0] ^ calc_ecc[0];
		b1 = read_ecc[1] ^ calc_ecc[1];
	} else {
		b0 = read_ecc[1] ^ calc_ecc[1];
		b1 = read_ecc[0] ^ calc_ecc[0];
	}

	b2 = read_ecc[2] ^ calc_ecc[2];

	/* check if there are any bitfaults */

	/* repeated if statements are slightly more efficient than switch ... */
	/* ordered in order of likelihood */

	if ((b0 | b1 | b2) == 0)
		return 0; /* no error */

	if ((((b0 ^ (b0 >> 1)) & 0x55) == 0x55) &&
	    (((b1 ^ (b1 >> 1)) & 0x55) == 0x55) &&
	    ((eccsize_mult == 1 && ((b2 ^ (b2 >> 1)) & 0x54) == 0x54) ||
		(eccsize_mult == 2 && ((b2 ^ (b2 >> 1)) & 0x55) == 0x55))) {
		/* single bit error */
		/*
		 * rp17/rp15/13/11/9/7/5/3/1 indicate which byte is the faulty
		 * byte, cp 5/3/1 indicate the faulty bit.
		 * A lookup table (called addressbits) is used to filter
		 * the bits from the byte they are in.
		 * A marginal optimisation is possible by having three
		 * different lookup tables.
		 * One as we have now (for b0), one for b2
		 * (that would avoid the >> 1), and one for b1 (with all values
		 * << 4). However it was felt that introducing two more tables
		 * hardly justify the gain.
		 *
		 * The b2 shift is there to get rid of the lowest two bits.
		 * We could also do addressbits[b2] >> 1 but for the
		 * performance it does not make any difference
		 */
		if (eccsize_mult == 1)
			byte_addr = (addressbits[b1] << 4) + addressbits[b0];
		else
			byte_addr = (addressbits[b2 & 0x3] << 8) +
				    (addressbits[b1] << 4) + addressbits[b0];
		bit_addr = addressbits[b2 >> 2];
		/* flip the bit */
		buf[byte_addr] ^= (1 << bit_addr);
		return 1;
	}
	/* count nr of bits; use table lookup, faster than calculating it */
	if ((bitsperbyte[b0] + bitsperbyte[b1] + bitsperbyte[b2]) == 1)
		return 1; /* error in ECC data; no action needed */

	return -EBADMSG;
}

struct hamming_codec_priv {
	size_t step_size;
	size_t steps;
};

static bool verify_has_enough_oob_storage_for_hamming_ecc(
    size_t datalen, size_t step_size, size_t ooblen)
{
	return round_up(datalen, step_size) <= (ooblen / 3);
}

static codec_transform_rc_t hamming_encode(
    struct pipeline_codec *codec, write_codec_context_t *context)
{
	codec_transform_rc_t ret;

	size_t ecc_step_idx;
	size_t datalen;
	size_t ooblen;
	struct hamming_codec_priv *priv;
	u8 *eccbuf;
	u8 *databuf;
	unsigned char ecccalc[3];

	databuf = (u8 *)context->srcbuf;
	eccbuf = (u8 *)context->destbuf;
	priv = codec->priv;
	datalen = context->srclen;
	ooblen = context->destlen;

	if (!verify_has_enough_oob_storage_for_hamming_ecc(
		datalen, priv->step_size, ooblen)) {
		CODEC_ANSWER_NACK_WITH_RC(ret, CODEC_RC_BOUNDARIES_INVALID_ERR);
		goto exit;
	}

	for (ecc_step_idx = 0;
	    ecc_step_idx < round_up(datalen, priv->step_size); ecc_step_idx++) {
		if (ecc_sw_hamming_calculate(
			databuf, priv->step_size, ecccalc, false) < 0) {
			CODEC_ANSWER_NACK_WITH_RC(
			    ret, CODEC_RC_WILL_CORRUPT_DATA_ERR);
			goto exit;
		}
		memcpy(eccbuf, ecccalc, 3);

		VLOG(2, stderr, "%s: Data dump:\n", __func__);
		VHEXDUMP(2, stderr, databuf, priv->step_size);
		VLOG(2, stderr, "%s: calculated ECC is:\n", __func__);
		VHEXDUMP(2, stderr, ecccalc, 3);
		eccbuf += 3;
		databuf += priv->step_size;
	}

	CODEC_ANSWER_ACK(ret);
exit:
	return ret;
}

static codec_transform_rc_t hamming_decode(
    struct pipeline_codec *codec, read_codec_context_t *context)
{
	int stat;
	codec_transform_rc_t ret;

	size_t ecc_step_idx;
	size_t datalen;
	size_t ooblen;
	struct hamming_codec_priv *priv;
	u8 *eccbuf;
	u8 *databuf;
	unsigned char ecccalc[3];

	databuf = (u8 *)context->databuf;
	eccbuf = (u8 *)context->oobbuf;
	priv = codec->priv;
	datalen = context->datalen;
	ooblen = context->ooblen;

	if (!verify_has_enough_oob_storage_for_hamming_ecc(
		datalen, priv->step_size, ooblen)) {
		CODEC_ANSWER_NACK_WITH_RC(ret, CODEC_RC_BOUNDARIES_INVALID_ERR);
		goto exit;
	}

	for (ecc_step_idx = 0; ecc_step_idx < priv->steps; ecc_step_idx++) {
		stat = ecc_sw_hamming_calculate(
		    databuf, priv->step_size, ecccalc, false);
		if (stat < 0) {
			CODEC_ANSWER_NACK_WITH_RC(
			    ret, CODEC_RC_DATA_RECOVERY_FAILED_ERR);
			goto exit;
		}
		stat = ecc_sw_hamming_correct(
		    databuf, eccbuf, ecccalc, priv->step_size, false);
		if (stat < 0) {
			VLOG(1, stderr,
			    "%s: uncorrectable ECC error, data dump:\n",
			    __func__);
			VHEXDUMP(1, stderr, databuf, priv->step_size);
			VLOG(1, stderr, "%s: calculated ECC is:\n", __func__);
			VHEXDUMP(1, stderr, ecccalc, 3);
			VLOG(1, stderr, "%s: stored ECC is:\n", __func__);
			VHEXDUMP(1, stderr, eccbuf, 3);
			CODEC_ANSWER_NACK_WITH_RC(
			    ret, CODEC_RC_DATA_RECOVERY_FAILED_ERR);
			goto exit;
		}

		eccbuf += 3;
		databuf += priv->step_size;
	}

	CODEC_ANSWER_ACK(ret);
exit:
	return ret;
}

static void hamming_deinit(struct pipeline_codec *codec)
{
	free(codec->priv);
}

static bool hamming_needs_source_span(void)
{
	return true;
}

static bool hamming_needs_oob_span(void)
{
	return true;
}

static bool hamming_validate_write_spans_size_sufficient(
    struct pipeline_codec *codec, size_t src_span_size, size_t dest_span_size)
{
	struct hamming_codec_priv *priv = codec->priv;
	return verify_has_enough_oob_storage_for_hamming_ecc(
	    src_span_size, priv->step_size, dest_span_size);
}

static bool hamming_validate_read_spans_size_sufficient(
    struct pipeline_codec *codec, size_t data_span_size, size_t oob_span_size)
{
	struct hamming_codec_priv *priv = codec->priv;
	return verify_has_enough_oob_storage_for_hamming_ecc(
	    data_span_size, priv->step_size, oob_span_size);
}

static struct write_ops hamming_write_ops = {
    .on_write = hamming_encode,
    .validate_spans_size_sufficient =
	hamming_validate_write_spans_size_sufficient,

    /* Static methods */
    .needs_source_span = hamming_needs_source_span,
};

static struct read_ops hamming_read_ops = {
    .on_read = hamming_decode,
    .validate_spans_size_sufficient =
	hamming_validate_read_spans_size_sufficient,

    /* Static methods */
    .needs_oob_span = hamming_needs_oob_span,
};

static ufedmd_rc_t handle_steps_entry(pipeline_codec_t *codec, void *value)
{
	ufedmd_rc_t ret;
	unsigned int steps = *(unsigned int *)value;
	struct hamming_codec_priv *priv;

	/* We don't limit the amount of steps. If user is trying to fool us, it
	 * will fail later when we validate this against the NAND geometry. And
	 * to be clear, 0 is not a valid option.
	 */
	if (steps == 0) {
		UFEDMD_RC_SET(ret, UFEDMD_RC_INVALID_CODEC_PROPERTY_VALUE);
		goto exit;
	}

	/* If we are about to overflow, reject the value now */
	if (multiplication_size_t_would_overflow(steps, 3)) {
		UFEDMD_RC_SET(ret, UFEDMD_RC_MULTIPLICATION_WOULD_OVERFLOW);
		goto exit;
	}

	priv = codec->priv;
	priv->steps = steps;

	UFEDMD_RC_SET_SUCCESS(ret);
exit:
	return ret;
}

static ufedmd_rc_t handle_step_size_entry(pipeline_codec_t *codec, void *value)
{
	ufedmd_rc_t ret;
	unsigned int step_size = *(unsigned int *)value;
	struct hamming_codec_priv *priv;

	/* A valid step size is either 128 bytes or 256 bytes for now. */
	if (step_size != 128 && step_size != 256) {
		UFEDMD_RC_SET(ret, UFEDMD_RC_INVALID_CODEC_PROPERTY_VALUE);
		goto exit;
	}

	priv = codec->priv;
	priv->step_size = step_size;

	UFEDMD_RC_SET_SUCCESS(ret);
exit:
	return ret;
}

static struct codec_entry_parser parsers[] = {
    {.key = "steps",
	.type = CODEC_ENTRY_PARSER_VALUE_TYPE_UINT,
	.handle = handle_steps_entry},
    {.key = "step_size",
	.type = CODEC_ENTRY_PARSER_VALUE_TYPE_UINT,
	.handle = handle_step_size_entry},
};

static bool validate_mtd_page_size_against_ecc_steps(
    struct proxy_mtd_info *mtd_info, struct hamming_codec_priv *params)
{
	assert(!(multiplication_size_t_would_overflow(params->steps, 3)));

	/* IMPORTANT: This is a best effort check against completely bogus
	 * configuration NAND scheme. Actual strong validation is taken place
	 * during pipeline execution (or pipeline construction, if possible),
	 * because we don't know in which pipeline page layout we are going
	 * to encounter.
	 *
	 * Each step is 3 bytes for the Linux Hamming SECDEC algorithm,
	 * regardless of its size.
	 * We store those bytes in the OOB area, so it should have enough
	 * space in that area. We could technically subtract 2 bytes for a
	 * possible bad block marker, but that's a validation that should
	 * happen in runtime, because we don't on which page index the BBM
	 * is actually stored.
	 */
	if (3 * params->steps > (mtd_info->flash_oob_size))
		return false;

	return true;
}

ufedmd_rc_t init_hamming_codec(pipeline_codec_t *base,
    struct proxy_mtd_info *mtd_info, struct cfg_dict *config)
{
	ufedmd_rc_t ret;

	ALLOCATE_PRIVATE_DATA_OR_FAIL(base->priv, struct hamming_codec_priv);

	ret = create_standard_codec(base, hamming_deinit, config, parsers,
	    ARRAY_SIZE(parsers), &hamming_write_ops, &hamming_read_ops);
	if (UFEDMD_RC_CHECK_SUCCESS(ret)) {
		goto exit;
	}

	if (!validate_mtd_page_size_against_ecc_steps(
		mtd_info, (struct hamming_codec_priv *)base->priv)) {
		UFEDMD_RC_SET(ret, UFEDMD_RC_INVALID_CODEC_PROPERTY_VALUE);
		goto exit;
	}

	UFEDMD_RC_SET_SUCCESS(ret);
exit:
	return ret;
}
