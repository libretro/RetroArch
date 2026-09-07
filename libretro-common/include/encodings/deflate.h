/* Copyright  (C) 2010-2024 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (deflate.h).
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

#ifndef _LIBRETRO_ENCODINGS_DEFLATE_H
#define _LIBRETRO_ENCODINGS_DEFLATE_H

#include <stdint.h>
#include <stddef.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* Clean-room, zlib-free implementation of RFC 1951 (raw DEFLATE) and
 * RFC 1950 (zlib-wrapped) compression and decompression.
 *
 * Both directions are non-blocking and resumable in the style of the
 * image/audio transfer APIs: create a stream, point it at an input buffer
 * and an output buffer, then call process() repeatedly.  Each call consumes
 * as much input and produces as much output as it can, then returns:
 *
 *   RDEFLATE_PROCESS_NEXT  (0) - suspended; provide more input and/or output
 *                                room and call again
 *   RDEFLATE_PROCESS_END   (1) - the stream is complete
 *   RDEFLATE_PROCESS_ERROR (-2)- the stream is malformed / an error occurred
 *
 * The `window_bits` argument selects the container format, matching zlib's
 * convention:
 *
 *   < 0        raw DEFLATE - no header, no checksum
 *   0 .. 15    zlib wrapper - 2-byte header + adler32 (RFC 1950)
 *   16 .. 31   gzip wrapper - RFC 1952 header + crc32/isize
 *   32 .. 47   decompression only: detect zlib or gzip from the stream
 *
 * so the values a zlib caller passes as -15, 15, 31 and 47 mean here what
 * they mean there.  When compressing, 32..47 writes gzip.  The gzip reader
 * skips the optional FEXTRA/FNAME/FCOMMENT fields and verifies both the
 * crc32 and the length in the trailer; the writer emits a minimal header
 * with no mtime and OS 255.
 */

enum
{
   RDEFLATE_PROCESS_ERROR = -2,
   RDEFLATE_PROCESS_END   =  1,
   /* stop-at-block: a deflate block just ended and another follows */
   RDEFLATE_PROCESS_BLOCK = 2,
   RDEFLATE_PROCESS_NEXT  =  0
};

/* -------- decompression (inflate) -------- */

void *rinflate_new(int window_bits);
void  rinflate_free(void *stream);

/**
 * rinflate_reset:
 * @stream       : stream returned by rinflate_new()
 * @window_bits  : as for rinflate_new()
 *
 * Returns @stream to its freshly-created state so it can decode an
 * unrelated stream, without freeing and reallocating. Equivalent to a
 * rinflate_free()/rinflate_new() pair, and cheaper: callers decoding
 * many independent streams (CHD hunks, for instance) would otherwise
 * pay a ~42 KiB clear per stream.
 **/
void  rinflate_reset(void *stream, int window_bits);
void  rinflate_set_in(void *stream, const uint8_t *in, size_t in_size);
void  rinflate_set_out(void *stream, uint8_t *out, size_t out_size);

/**
 * rinflate_set_dictionary:
 *
 * Primes the 32KB back-reference window with the tail of @dict, so a
 * raw-deflate decode can resume mid-stream given the preceding
 * plaintext - the indexed-gzip random access pattern. Only the last
 * 32768 bytes of @dict are used; the call replaces any existing
 * history. Raw streams (negative window_bits) only.
 */
void  rinflate_set_dictionary(void *stream, const uint8_t *dict, size_t len);

/** Report RDEFLATE_PROCESS_BLOCK at each interior block boundary. */
void     rinflate_set_stop_at_block(void *stream, int stop);
/** Exact input position in bits, relative to the current input window. */
uint64_t rinflate_tell_bits(const void *stream);
/** Discard 0..7 bits at stream start: resume mid-byte at a boundary. */
void     rinflate_set_start_bit(void *stream, int bits);

/* Decompress.  Writes the number of input bytes consumed to *read and the
 * number of output bytes produced to *wrote (either may be NULL).  Returns
 * one of the RDEFLATE_PROCESS_* codes. */
int   rinflate_process(void *stream, size_t *read, size_t *wrote);

/* -------- compression (deflate) -------- */

/* level: 0 (store) .. 9 (maximum). */
void *rdeflate_new(int level, int window_bits);
void  rdeflate_free(void *stream);
void  rdeflate_set_in(void *stream, const uint8_t *in, size_t in_size);
void  rdeflate_set_out(void *stream, uint8_t *out, size_t out_size);

/* Signal that no more input will be supplied after the current input buffer
 * is consumed, so the final block and trailer can be emitted. */
void  rdeflate_finish(void *stream);

int   rdeflate_process(void *stream, size_t *read, size_t *wrote);

RETRO_END_DECLS

#endif
