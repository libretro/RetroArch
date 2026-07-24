/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (r7z_filters.h).
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

/* Branch converters and delta decoding for 7z archive members, derived
 * from the public-domain LZMA SDK by Igor Pavlov.
 *
 * These are compression preprocessors, not architecture-specific code:
 * a "PPC filter" rewrites PowerPC branch operands and runs identically
 * on any host. Executable code is full of relative branches whose
 * operands differ between otherwise-identical call sequences, which
 * defeats the LZ match finder; the encoder converts those operands to
 * absolute form so the sequences match, and the decoder converts them
 * back. Delta does the same job for data with a fixed stride, such as
 * uncompressed audio samples or bitmap rows.
 *
 * Decode direction only: nothing in libretro-common writes archives.
 * Every function converts in place and returns the number of bytes it
 * consumed, which can be less than the buffer size when a trailing
 * partial instruction is present.
 */

#ifndef __LIBRETRO_SDK_R7Z_FILTERS_H
#define __LIBRETRO_SDK_R7Z_FILTERS_H

#include <stddef.h>
#include <stdint.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* Delta keeps one byte of history per distance unit. */
#define RFILTERS_DELTA_STATE_SIZE 256

/**
 * rfilters_delta_decode:
 * @state    : RFILTERS_DELTA_STATE_SIZE bytes of running history, zeroed
 *             before the first call
 * @distance : stride in bytes, 1..RFILTERS_DELTA_STATE_SIZE
 * @data     : buffer converted in place
 * @len      : number of bytes
 *
 * Adds each byte to the one @distance positions earlier in the stream.
 * State carries across calls so a member can be delivered in chunks.
 */
void rfilters_delta_decode(uint8_t *state, unsigned distance,
      uint8_t *data, size_t len);

/**
 * rfilters_x86_decode:
 * @state    : running state, zeroed before the first call
 * @ip       : stream offset of @data
 * @data     : buffer converted in place
 * @len      : number of bytes
 *
 * x86 CALL/JMP relative-operand converter (the BCJ filter).
 *
 * Returns: number of bytes converted.
 */
size_t rfilters_x86_decode(uint32_t *state, uint32_t ip,
      uint8_t *data, size_t len);

/* The RISC converters below are stateless: each instruction is a fixed
 * width and self-contained, so only the stream offset is needed. */

size_t rfilters_arm_decode  (uint32_t ip, uint8_t *data, size_t len);
size_t rfilters_armt_decode (uint32_t ip, uint8_t *data, size_t len);
size_t rfilters_ppc_decode  (uint32_t ip, uint8_t *data, size_t len);
size_t rfilters_sparc_decode(uint32_t ip, uint8_t *data, size_t len);
size_t rfilters_ia64_decode (uint32_t ip, uint8_t *data, size_t len);

RETRO_END_DECLS

#endif
