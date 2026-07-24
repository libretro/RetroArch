/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (r7z_lzma_stream.h).
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

/* Streaming LZMA decoder for libretro-common, derived from the
 * public-domain LZMA SDK by Igor Pavlov.
 *
 * This is the resumable counterpart to r7z_lzma.h. That one-shot decoder
 * stays as it is: CHD hands it a whole block at once, its probability
 * array is inline, and it allocates nothing. This one exists because
 * LZMA2, BCJ2 and the 7z container all need to push input in pieces and
 * pull output in pieces, with decoder state surviving between calls.
 *
 * Differences from the one-shot decoder, all forced by those callers:
 *
 *   - input and output are both resumable; a call consumes what it can
 *     and reports how far it got
 *   - the dictionary is a caller-supplied window rather than the output
 *     buffer, since output is drained incrementally
 *   - 7z permits lc + lp <= 12, which is 0x300 << 12 probability
 *     entries (~6 MiB). That cannot live in the struct, so the caller
 *     supplies the probability array too
 *
 * Both decoders share one decode core; see r7z_lzma_stream.c.
 *
 * Nothing here allocates. The caller owns every buffer.
 */

#ifndef __LIBRETRO_SDK_R7Z_LZMA_STREAM_H
#define __LIBRETRO_SDK_R7Z_LZMA_STREAM_H

#include <stddef.h>
#include <stdint.h>

#include <retro_common_api.h>

#include <7z/r7z_lzma.h>

RETRO_BEGIN_DECLS

/* Largest lc + lp permitted by the 7z format. */
#define RLZMA_STREAM_LCLP_MAX 12

/* Worst-case input, in bytes, that decoding a single LZMA symbol can
 * consume. The decode loop is bounded so it never needs an input check
 * in the middle of a symbol; see the comment in r7z_lzma_stream.c. */
#define RLZMA_REQUIRED_INPUT_MAX 20

/* Probability entries needed for a given lc/lp. Callers size their array
 * with this and pass it to rlzma_stream_init(). */
#define RLZMA_STREAM_NUM_PROBS(lc, lp) \
   (1984 + ((uint32_t)0x300 << ((lc) + (lp))))

/* Result of a decode call, in addition to the RLZMA_* codes. */
#define RLZMA_STATUS_NEEDS_MORE_INPUT 1
#define RLZMA_STATUS_FINISHED         2
#define RLZMA_STATUS_NOT_FINISHED     3

typedef struct rlzma_stream
{
   /* Caller-owned probability model, RLZMA_STREAM_NUM_PROBS entries. */
   uint16_t      *probs;
   /* Caller-owned dictionary window. */
   uint8_t       *dic;
   size_t         dic_size;
   size_t         dic_pos;

   uint32_t       range;
   uint32_t       code;
   uint32_t       state;
   uint32_t       rep0;
   uint32_t       rep1;
   uint32_t       rep2;
   uint32_t       rep3;

   /* Bytes of a match still to be copied when output filled up. */
   uint32_t       remain_len;
   /* Set once an end-of-stream marker has been decoded. */
   uint32_t       got_marker;
   /* Range coder still needs its five-byte prologue. */
   uint32_t       need_init;

   uint32_t       lc;
   uint32_t       lp;
   uint32_t       pb;
   uint32_t       num_probs;
   /* Dictionary size declared by the stream properties. Distances are
    * validated against this, which is not the same thing as the size of
    * the window the caller supplied. */
   uint32_t       dict_size;
   /* Total bytes produced since the last reset. A distance may not
    * reach back further than this. */
   uint64_t       total_pos;

   /* Holds a partial symbol's input across a call boundary. */
   uint8_t        temp_buf[RLZMA_REQUIRED_INPUT_MAX];
   uint32_t       temp_size;
} rlzma_stream_t;

/**
 * rlzma_stream_init:
 * @s          : decoder state, supplied by the caller
 * @props      : RLZMA_PROPS_SIZE bytes of LZMA stream properties
 * @probs      : caller-owned array of RLZMA_STREAM_NUM_PROBS(lc, lp)
 *               uint16_t entries
 * @dic        : caller-owned dictionary window
 * @dic_size   : size of @dic in bytes
 *
 * Parses @props and binds the caller's buffers. Performs no allocation.
 *
 * Returns: RLZMA_OK, or RLZMA_ERROR_PARAM if @props encodes an lc/lp/pb
 * outside the supported range or a buffer is missing.
 */
int rlzma_stream_init(rlzma_stream_t *s, const uint8_t *props,
      uint16_t *probs, uint8_t *dic, size_t dic_size);

/**
 * rlzma_stream_reset:
 * @s          : initialized decoder state
 *
 * Resets the range coder, the probability model and the dictionary
 * position, keeping the buffers bound by rlzma_stream_init(). Call this
 * before the first block and at every LZMA2 state reset.
 */
void rlzma_stream_reset(rlzma_stream_t *s);

/**
 * rlzma_stream_reset_parts:
 * @s          : initialized decoder state
 * @init_dic   : non-zero to reset the dictionary position
 * @init_state : non-zero to reset the range coder, probabilities and
 *               match history
 *
 * The two halves of rlzma_stream_reset(), separately. LZMA2 chunks
 * choose which to apply: a chunk may reset decoder state while
 * continuing the previous chunk's dictionary, or reset the dictionary
 * while the range coder carries on.
 */
void rlzma_stream_reset_parts(rlzma_stream_t *s, int init_dic,
      int init_state);

/**
 * rlzma_stream_set_props:
 * @s          : initialized decoder state
 * @props      : RLZMA_PROPS_SIZE bytes of LZMA stream properties
 *
 * Re-parses @props into an already-initialized decoder, keeping the
 * bound buffers and the dictionary contents. LZMA2 chunks may change
 * lc/lp/pb mid-stream, which this exists for.
 *
 * The caller is responsible for having sized the probability array for
 * the largest lc + lp any chunk will ask for.
 *
 * Returns: RLZMA_OK, or RLZMA_ERROR_PARAM if @props is out of range.
 */
int rlzma_stream_set_props(rlzma_stream_t *s, const uint8_t *props);

/**
 * rlzma_stream_put_uncompressed:
 * @s          : initialized decoder state
 * @src        : bytes to append
 * @len        : number of bytes
 *
 * Appends literal bytes to the dictionary at the current position, as
 * an LZMA2 uncompressed chunk requires. The bytes become part of the
 * dictionary, so later matches may refer back into them.
 *
 * Returns: RLZMA_OK, or RLZMA_ERROR_PARAM if @len exceeds the space
 * left in the window.
 */
int rlzma_stream_put_uncompressed(rlzma_stream_t *s,
      const uint8_t *src, size_t len);

/**
 * rlzma_stream_decode:
 * @s          : initialized, reset decoder state
 * @dic_limit  : decode until @s->dic_pos reaches this, at most @dic_size
 * @src        : compressed input
 * @src_len    : in: bytes available. out: bytes consumed.
 * @finish     : non-zero if @src is the last input for this block
 * @status     : out, one of the RLZMA_STATUS_* values
 *
 * Decodes until the output reaches @dic_limit or the input runs out,
 * whichever comes first, then returns. Decoder state persists, so the
 * call may be repeated with more input or a higher limit.
 *
 * Returns: RLZMA_OK, or RLZMA_ERROR_DATA on malformed input.
 */
int rlzma_stream_decode(rlzma_stream_t *s, size_t dic_limit,
      const uint8_t *src, size_t *src_len, int finish, int *status);

RETRO_END_DECLS

#endif
