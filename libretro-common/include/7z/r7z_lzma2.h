/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (r7z_lzma2.h).
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

/* LZMA2 decoder for libretro-common, derived from the public-domain
 * LZMA SDK by Igor Pavlov.
 *
 * LZMA2 is a framing layer over LZMA: the stream is a sequence of
 * chunks, each with a header saying how big it is, whether it is
 * compressed at all, and how much decoder state to reset beforehand.
 * The chunk payloads are decoded by r7z_lzma_stream, which is why that
 * decoder is resumable: a chunk boundary can fall anywhere, including
 * mid-symbol, and a chunk may continue the previous chunk's dictionary
 * and range coder with no reset whatsoever.
 *
 * Chunk header, from the reference:
 *
 *   0x00                  end of stream
 *   0x01 U U              uncompressed, reset dictionary
 *   0x02 U U              uncompressed, no reset
 *   0x80 + u U U P P      LZMA, no reset
 *   0xA0 + u U U P P      LZMA, reset state
 *   0xC0 + u U U P P S    LZMA, reset state, new props
 *   0xE0 + u U U P P S    LZMA, reset state, new props, reset dictionary
 *
 * where u is the top five bits of the unpacked size, U U the remaining
 * sixteen, P P the packed size, and S the LZMA props byte. Both sizes
 * are stored biased by one.
 *
 * Note LZMA2 caps lc + lp at 4, not the 12 that bare LZMA allows, so
 * the probability array is far smaller here than r7z_lzma_stream's
 * worst case. The caller still supplies it, for the same reason: no
 * allocation happens in this file.
 */

#ifndef __LIBRETRO_SDK_R7Z_LZMA2_H
#define __LIBRETRO_SDK_R7Z_LZMA2_H

#include <stddef.h>
#include <stdint.h>

#include <retro_common_api.h>

#include <7z/r7z_lzma.h>
#include <7z/r7z_lzma_stream.h>

RETRO_BEGIN_DECLS

/* LZMA2 permits lc + lp <= 4. */
#define RLZMA2_LCLP_MAX 4

/* Probability entries needed for any legal LZMA2 chunk. Sized for the
 * maximum lc + lp the format allows, so one array serves a whole
 * stream however often the props change mid-flight. */
#define RLZMA2_NUM_PROBS (1984 + ((uint32_t)0x300 << RLZMA2_LCLP_MAX))

/* Status values, matching the streaming LZMA decoder's vocabulary. */
#define RLZMA2_STATUS_NEEDS_MORE_INPUT 1
#define RLZMA2_STATUS_FINISHED         2
#define RLZMA2_STATUS_NOT_FINISHED     3

typedef struct rlzma2_dec
{
   rlzma_stream_t lzma;

   uint32_t       state;
   uint32_t       control;
   uint32_t       unpack_size;
   uint32_t       pack_size;
   /* Gates the first chunk: a stream must begin by resetting both the
    * dictionary and the decoder state, so a stream that opens with a
    * continuation chunk is rejected rather than decoded against
    * whatever happened to be in the window. */
   uint32_t       need_init_level;

   uint8_t       *dic;
   size_t         dic_size;
   uint16_t      *probs;

   /* Props carried over from the last chunk that set them, so a chunk
    * that resets state without new props reuses the previous ones. */
   uint8_t        props[RLZMA_PROPS_SIZE];
   uint32_t       have_props;
} rlzma2_dec_t;

/**
 * rlzma2_dec_init:
 * @p          : decoder state, supplied by the caller
 * @prop       : the single LZMA2 property byte (dictionary size code)
 * @probs      : caller-owned array of RLZMA2_NUM_PROBS uint16_t entries
 * @dic        : caller-owned dictionary window
 * @dic_size   : size of @dic in bytes
 *
 * Parses @prop and binds the caller's buffers. Performs no allocation.
 *
 * Returns: RLZMA_OK, or RLZMA_ERROR_PARAM if @prop is out of range or a
 * buffer is missing.
 */
int rlzma2_dec_init(rlzma2_dec_t *p, uint8_t prop,
      uint16_t *probs, uint8_t *dic, size_t dic_size);

/**
 * rlzma2_dec_reset:
 * @p          : initialized decoder state
 *
 * Returns the decoder to the start of a stream, keeping the buffers
 * bound by rlzma2_dec_init().
 */
void rlzma2_dec_reset(rlzma2_dec_t *p);

/**
 * rlzma2_dec_decode:
 * @p          : initialized, reset decoder state
 * @dic_limit  : decode until the dictionary position reaches this
 * @src        : compressed input
 * @src_len    : in: bytes available. out: bytes consumed.
 * @finish     : non-zero if @src is the last input for this stream
 * @status     : out, one of the RLZMA2_STATUS_* values
 *
 * Decodes until the output reaches @dic_limit, the end-of-stream chunk
 * is seen, or the input runs out. Decoder state persists across calls.
 *
 * The caller reads decoded bytes from @dic between the position before
 * the call and @p->lzma.dic_pos after it, and is responsible for
 * wrapping its own read position when the window wraps.
 *
 * Returns: RLZMA_OK, or RLZMA_ERROR_DATA on malformed input.
 */
int rlzma2_dec_decode(rlzma2_dec_t *p, size_t dic_limit,
      const uint8_t *src, size_t *src_len, int finish, int *status);

/**
 * rlzma2_dec_dic_size_from_prop:
 * @prop       : the LZMA2 property byte
 * @dic_size   : out, dictionary size in bytes
 *
 * Decodes the dictionary size a stream asks for, so the caller can size
 * the window before initializing the decoder.
 *
 * Returns: RLZMA_OK, or RLZMA_ERROR_PARAM if @prop is out of range.
 */
int rlzma2_dec_dic_size_from_prop(uint8_t prop, uint32_t *dic_size);

RETRO_END_DECLS

#endif
