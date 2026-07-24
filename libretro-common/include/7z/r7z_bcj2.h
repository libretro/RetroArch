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
 * This decoder takes all four streams whole rather than resumably. 7z
 * hands over complete folders, and a resumable version would need to
 * carry a partially-scanned candidate across the boundary for no
 * benefit here.
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

/* One context for a non-E8 candidate, one for E9, and 256 for E8
 * selected by the preceding byte. */
#define RBCJ2_NUM_PROBS (2 + 256)

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
