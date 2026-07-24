/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (r7z_bcj2.h).
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

/* BCJ2 decoder for libretro-common, derived from the public-domain
 * LZMA SDK by Igor Pavlov.
 *
 * BCJ2 is the x86 branch converter 7z uses for executables. Where the
 * plain x86 filter in r7z_filters rewrites branch targets in place,
 * BCJ2 splits the data into four streams:
 *
 *   main  everything except the operands of converted branches
 *   call  4-byte big-endian absolute targets of converted E8 calls
 *   jump  4-byte big-endian absolute targets of converted E9 and
 *         0F 8x jumps
 *   rc    a range-coded stream carrying one bit per branch candidate,
 *         saying whether that candidate was actually converted
 *
 * The split is what makes it work: branch targets are highly
 * compressible on their own but ruin the statistics of the byte stream
 * they are embedded in, so 7z compresses each of the four separately.
 *
 * The rc stream exists because not every E8 byte starts a call. The
 * encoder decides per candidate, and the decoder cannot tell without
 * being told, so a bit is coded for each one against a context chosen
 * by the opcode and the byte before it.
 *
 * Decoding is a strict interleave: the main stream is scanned for
 * candidates, and each candidate consumes a bit from rc and, if that
 * bit is set, four bytes from call or jump. Getting the streams out of
 * step corrupts everything after that point, so all four are advanced
 * together in one pass.
 *
 * The streams arrive whole - 7z hands over complete folders - but the
 * conversion itself can be done in bounded pieces, which matters
 * because a folder can be several megabytes and this is one linear
 * pass over all of it. r7z_bcj2_decode() does the whole thing;
 * r7z_bcj2_decode_part() does as much as a caller asks for and can be
 * resumed. See r7z_bcj2_state_t.
 */

#ifndef __LIBRETRO_SDK_R7Z_BCJ2_H
#define __LIBRETRO_SDK_R7Z_BCJ2_H

#include <stddef.h>
#include <stdint.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

#define RBCJ2_OK           0
#define RBCJ2_ERROR_DATA (-1)
#define RBCJ2_ERROR_PARAM (-2)

/* Not an error: the requested limit was reached and more calls are
 * needed. See r7z_bcj2_decode_part(). */
#define RBCJ2_PENDING       1

/* One context for a non-E8 candidate, one for E9, and 256 for E8
 * selected by the preceding byte. */
#define RBCJ2_NUM_PROBS (2 + 256)

/* A conversion in progress. Zero it, or use r7z_bcj2_state_init(),
 * before the first call to r7z_bcj2_decode_part(). */
typedef struct r7z_bcj2_state
{
   uint16_t probs[RBCJ2_NUM_PROBS];
   size_t   main_pos;
   size_t   call_pos;
   size_t   jump_pos;
   size_t   rc_pos;
   size_t   dst_pos;
   uint32_t range;
   uint32_t code;
   uint32_t ip;
   uint8_t  prev;
   uint8_t  started;
} r7z_bcj2_state_t;

/**
 * r7z_bcj2_state_init:
 * @st         : state to prepare
 *
 * Readies @st for a fresh conversion.
 */
void r7z_bcj2_state_init(r7z_bcj2_state_t *st);

/**
 * r7z_bcj2_decode_part:
 * @st         : conversion state, prepared with r7z_bcj2_state_init()
 * @dst        : output buffer
 * @dst_len    : total size of the output, the same on every call
 * @dst_limit  : produce at most this many bytes in total before
 *               returning; pass @dst_len to run to completion
 * @main_buf   : main stream
 * @main_len   : length of @main_buf
 * @call_buf   : call target stream, length must be a multiple of 4
 * @call_len   : length of @call_buf
 * @jump_buf   : jump target stream, length must be a multiple of 4
 * @jump_len   : length of @jump_buf
 * @rc_buf     : range coder stream
 * @rc_len     : length of @rc_buf
 *
 * Converts until @st->dst_pos reaches @dst_limit, then returns. The
 * limit is a floor rather than an exact stop: a converted branch
 * writes four bytes at once and is never split across calls, so a
 * call may overshoot by up to three bytes.
 *
 * Call again with a higher @dst_limit to continue. The stream
 * arguments must be identical on every call for one conversion.
 *
 * Returns: RBCJ2_OK when @dst_len bytes have been produced,
 * RBCJ2_PENDING when the limit was reached first, or a negative
 * RBCJ2_ERROR_* code.
 */
int r7z_bcj2_decode_part(r7z_bcj2_state_t *st,
      uint8_t *dst, size_t dst_len, size_t dst_limit,
      const uint8_t *main_buf, size_t main_len,
      const uint8_t *call_buf, size_t call_len,
      const uint8_t *jump_buf, size_t jump_len,
      const uint8_t *rc_buf,   size_t rc_len);

/**
 * r7z_bcj2_decode:
 * @dst        : output buffer
 * @dst_len    : exact number of bytes to produce
 * @main_buf   : main stream
 * @main_len   : length of @main_buf
 * @call_buf   : call target stream, length must be a multiple of 4
 * @call_len   : length of @call_buf
 * @jump_buf   : jump target stream, length must be a multiple of 4
 * @jump_len   : length of @jump_buf
 * @rc_buf     : range coder stream
 * @rc_len     : length of @rc_buf
 *
 * Reassembles one complete BCJ2 stream. Performs no allocation; the
 * probability model lives on the stack.
 *
 * Succeeds only if exactly @dst_len bytes are produced and no stream
 * is overrun.
 *
 * Returns: RBCJ2_OK, RBCJ2_ERROR_PARAM on a bad argument, or
 * RBCJ2_ERROR_DATA on malformed input.
 */
int r7z_bcj2_decode(uint8_t *dst, size_t dst_len,
      const uint8_t *main_buf, size_t main_len,
      const uint8_t *call_buf, size_t call_len,
      const uint8_t *jump_buf, size_t jump_len,
      const uint8_t *rc_buf,   size_t rc_len);

RETRO_END_DECLS

#endif
