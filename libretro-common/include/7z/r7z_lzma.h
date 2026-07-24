/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (r7z_lzma.h).
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

/* Reimplementation of the LZMA decoder for libretro-common, derived from
 * the public-domain LZMA SDK by Igor Pavlov.
 *
 * This is deliberately not a general-purpose LZMA library. It implements
 * exactly the subset libretro-common needs:
 *
 *   - single-call decode of a complete LZMA stream whose uncompressed
 *     size is known up front by the caller
 *   - no end-of-stream marker handling beyond accepting one if present
 *   - no chunked/streaming input, no dictionary carried between calls
 *
 * The caller's output buffer doubles as the dictionary window, so there
 * is no window allocation and no copy-out step. This is valid because
 * every consumer decodes one self-contained block whose size is <= the
 * dictionary size recorded in the stream properties.
 */

#ifndef __LIBRETRO_SDK_R7Z_LZMA_H
#define __LIBRETRO_SDK_R7Z_LZMA_H

#include <stddef.h>
#include <stdint.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

#define RLZMA_PROPS_SIZE 5

/* Return codes. */
#define RLZMA_OK           0
#define RLZMA_ERROR_DATA (-1)
#define RLZMA_ERROR_PARAM (-2)

/* Number of probability entries for the largest legal lc/lp combination
 * we accept. Sized for lc + lp <= RLZMA_LCLP_MAX so the probability array
 * can live inside the decoder struct with no allocation at all. */
/* Largest lc + lp this decoder accepts. CHD always uses lc=3, lp=0, and
 * the 7z streams libretro-common reads do not exceed 3 either, so 3 is
 * what the probability array is sized for. Streams asking for more are
 * rejected at init rather than silently mis-decoded. Raising this costs
 * 0x300 * 2 bytes of struct per increment. */
#define RLZMA_LCLP_MAX 3
/* Base tables (1984, including the layout bias the decoder relies on)
 * plus the literal coder, sized for the largest lc+lp we accept. */
#define RLZMA_NUM_PROBS (1984 + ((uint32_t)0x300 << RLZMA_LCLP_MAX))

typedef struct rlzma_dec
{
   /* Probability model. Kept inline: at RLZMA_LCLP_MAX this is ~29 KiB,
    * which is cheap next to the hunk buffers the callers already hold,
    * and it removes the only heap allocation the decoder would need. */
   uint16_t probs[RLZMA_NUM_PROBS];
   uint32_t lc;
   uint32_t lp;
   uint32_t pb;
   uint32_t dict_size;
} rlzma_dec_t;

/**
 * rlzma_dec_init:
 * @dec        : decoder state, supplied by the caller
 * @props      : RLZMA_PROPS_SIZE bytes of LZMA stream properties
 *
 * Parses @props into @dec. Performs no allocation. Must be called once
 * before rlzma_dec_decode(); the same @dec may then be reused for any
 * number of independent blocks.
 *
 * Returns: RLZMA_OK, or RLZMA_ERROR_PARAM if @props encodes an lc/lp/pb
 * combination outside the supported range.
 */
int rlzma_dec_init(rlzma_dec_t *dec, const uint8_t *props);

/**
 * rlzma_dec_decode:
 * @dec        : initialized decoder state
 * @dst        : output buffer, also used as the dictionary window
 * @dst_len    : exact number of bytes to produce
 * @src        : compressed input
 * @src_len    : exact number of compressed bytes available
 *
 * Decodes one complete LZMA block. The decoder is reset internally, so
 * no state carries over between calls.
 *
 * Succeeds only if exactly @dst_len bytes are produced. Trailing input
 * is tolerated (an end-of-stream marker may or may not be present) but
 * running out of input early is an error.
 *
 * Returns: RLZMA_OK, or RLZMA_ERROR_DATA on malformed input.
 */
int rlzma_dec_decode(rlzma_dec_t *dec,
      uint8_t *dst, size_t dst_len,
      const uint8_t *src, size_t src_len);


RETRO_END_DECLS

#endif
