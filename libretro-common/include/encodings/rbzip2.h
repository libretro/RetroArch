/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rbzip2.h).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* Clean-room decoder for the bzip2 stream format.
 *
 * A stream is "BZh", a block size digit, then blocks: each the
 * Burrows-Wheeler transform of up to 100 000 x digit octets, coded as
 * move-to-front ranks with runs of rank 0 in bijective base 2 and two
 * to six Huffman tables, under a run-length coding of the input
 * applied before the transform. Each block carries the CRC of what it
 * decodes to, and the end of the stream the combination of them all;
 * both are checked.
 *
 * Decoding only, of one whole stream held in memory: into a buffer
 * already sized for it, as the WIA disc image format and zip archives
 * store theirs, or into one grown as it goes, for a .bz2 file. Blocks marked randomised, which no bzip2 has written since
 * 0.9.5, are refused. No file I/O. */

#ifndef __LIBRETRO_SDK_FORMAT_RBZIP2_H__
#define __LIBRETRO_SDK_FORMAT_RBZIP2_H__

#include <stddef.h>
#include <stdint.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

#define RBZIP2_PENDING     1    /* rbzip2_step(): a block, and more to come */
#define RBZIP2_OK          0
#define RBZIP2_ERROR_DATA  -1   /* not a stream, corrupt, or a CRC fails */
#define RBZIP2_ERROR_SPACE -2   /* decodes to more than the buffer holds */
#define RBZIP2_ERROR_MEM   -3

typedef struct rbzip2 rbzip2_t;

/* A decoder; its block buffer is allocated at the first block and kept
 * for the next stream. NULL when out of memory. */
rbzip2_t *rbzip2_new(void);

void rbzip2_free(rbzip2_t *bz);

/**
 * rbzip2_decode:
 * @dst      : where the stream decodes to
 * @dst_len  : capacity of @dst
 * @src      : the stream, from "BZh" on
 * @src_len  : octets at @src; anything after the stream's end is ignored
 * @wrote    : receives the octets decoded (may be NULL)
 *
 * Returns: RBZIP2_OK, or an RBZIP2_ERROR_* value.
 **/
int rbzip2_decode(rbzip2_t *bz, uint8_t *dst, size_t dst_len,
      const uint8_t *src, size_t src_len, size_t *wrote);

/**
 * rbzip2_begin:
 * @dst      : where the stream decodes to
 * @dst_len  : capacity of @dst
 * @src      : the stream, from "BZh" on, which stays where it is
 *             until the stream is done
 * @src_len  : octets at @src
 *
 * The same, a block at a time: binds the stream and its output, for
 * rbzip2_step() to decode. A block is at most 900 000 octets.
 *
 * Returns: RBZIP2_OK, or an RBZIP2_ERROR_* value.
 **/
int rbzip2_begin(rbzip2_t *bz, uint8_t *dst, size_t dst_len,
      const uint8_t *src, size_t src_len);

/**
 * rbzip2_step:
 * @wrote    : receives the octets decoded so far (may be NULL), what is
 *             in @dst before them final
 *
 * Decodes the next block of the stream rbzip2_begin() bound, its CRC
 * checked, or reads the end of the stream and checks the CRC of them
 * all.
 *
 * Returns: RBZIP2_PENDING after a block, RBZIP2_OK at the end, or an
 * RBZIP2_ERROR_* value.
 **/
int rbzip2_step(rbzip2_t *bz, size_t *wrote);

/**
 * rbzip2_begin_blocks:
 * @src      : the stream, from "BZh" on, which stays where it is until
 *             the stream is done
 * @src_len  : octets at @src
 *
 * The same, each block into the decoder's own buffer, for a stream
 * whose whole output is not wanted in one place. The buffer grows to
 * the largest block, which the run-length coding before the transform
 * lets reach 51 times the block size.
 *
 * Returns: RBZIP2_OK, or an RBZIP2_ERROR_* value.
 **/
int rbzip2_begin_blocks(rbzip2_t *bz, const uint8_t *src, size_t src_len);

/**
 * rbzip2_begin_ring:
 * @a        : the first of the stream, at least its four-octet header
 * @b        : and what comes after @a, as where a ring buffer wraps; or
 *             none, @b_len 0
 *
 * As rbzip2_begin_blocks(), for a stream given a part at a time, in a
 * ring buffer the caller reads into: before each rbzip2_step_block()
 * the input is either the rest of the stream or at least
 * RBZIP2_AHEAD octets of it, handed over again with rbzip2_input().
 *
 * Returns: RBZIP2_OK, or an RBZIP2_ERROR_* value.
 **/
int rbzip2_begin_ring(rbzip2_t *bz, const uint8_t *a, size_t a_len,
      const uint8_t *b, size_t b_len);

/* The most of the stream one block can take up: 900000 symbols of up to
 * twenty bits, the tables and the selectors before them, and the
 * stream's end */
#define RBZIP2_AHEAD (2304u * 1024u)

/**
 * rbzip2_input:
 *
 * The input from where rbzip2_taken() says the stream has been taken
 * to: @a, then @b where the ring wraps (@b_len 0 for none).
 **/
void rbzip2_input(rbzip2_t *bz, const uint8_t *a, size_t a_len,
      const uint8_t *b, size_t b_len);

/**
 * rbzip2_taken:
 *
 * Returns: how many octets of the stream the decoder has taken, which
 * it does not need again.
 **/
uint64_t rbzip2_taken(const rbzip2_t *bz);

/**
 * rbzip2_step_block:
 * @block    : receives where the block's octets are, until the next call
 * @len      : receives how many
 *
 * The next block of the stream rbzip2_begin_blocks() bound, its CRC
 * checked, or the end of the stream and the CRC of them all.
 *
 * Returns: RBZIP2_PENDING with a block, RBZIP2_OK at the end, or an
 * RBZIP2_ERROR_* value.
 **/
int rbzip2_step_block(rbzip2_t *bz, const uint8_t **block, size_t *len);

/**
 * rbzip2_decode_alloc:
 * @dst      : receives the decoded octets, allocated with malloc() and
 *             the caller's to free; one octet past them is room to spare
 * @wrote    : receives how many there are
 *
 * The same, for a stream whose size is not known: the buffer grows as
 * blocks are decoded into it.
 *
 * Returns: RBZIP2_OK, or an RBZIP2_ERROR_* value, with *@dst NULL.
 **/
int rbzip2_decode_alloc(rbzip2_t *bz, const uint8_t *src, size_t src_len,
      uint8_t **dst, size_t *wrote);

RETRO_END_DECLS

#endif
