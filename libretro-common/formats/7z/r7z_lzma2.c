/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (r7z_lzma2.c).
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

/* LZMA2 decoder. See r7z_lzma2.h for the chunk header layout.
 *
 * This is a framing layer, not a codec: it parses chunk headers, works
 * out how much decoder state each chunk wants reset, and hands the
 * payload to r7z_lzma_stream. All the actual LZMA decoding lives there.
 *
 * The header parser is a byte-at-a-time state machine rather than a
 * "wait until the whole header has arrived" loop, because a chunk
 * header can be split across calls at any byte and the caller is under
 * no obligation to present it whole.
 */

#include <string.h>

#include <7z/r7z_lzma2.h>

/* Header parse states, in the order the bytes arrive. */
#define ST_CONTROL   0
#define ST_UNPACK0   1
#define ST_UNPACK1   2
#define ST_PACK0     3
#define ST_PACK1     4
#define ST_PROP      5
#define ST_DATA      6
#define ST_DATA_CONT 7
#define ST_FINISHED  8
#define ST_ERROR     9

/* A control byte with the top bit clear is an uncompressed chunk. */
#define IS_UNCOMPRESSED(ctl) (((ctl) & 0x80) == 0)

/* The only uncompressed control byte that resets the dictionary. */
#define CONTROL_COPY_RESET_DIC 1

/* need_init_level gates what the next chunk is allowed to be. It starts
 * at 0xE0, meaning "the first chunk must reset both dictionary and
 * state", and drops to 0 once that has happened. A stream that opens
 * with a continuation chunk is rejected rather than decoded against
 * whatever the window happened to contain. */
#define NEED_INIT_FULL 0xE0
#define NEED_INIT_DIC  0xC0

int rlzma2_dec_dic_size_from_prop(uint8_t prop, uint32_t *dic_size)
{
   if (!dic_size)
      return RLZMA_ERROR_PARAM;
   if (prop > 40)
      return RLZMA_ERROR_PARAM;
   if (prop == 40)
      *dic_size = 0xFFFFFFFFu;
   else
      *dic_size = ((uint32_t)2 | ((uint32_t)prop & 1))
         << ((uint32_t)prop / 2 + 11);
   return RLZMA_OK;
}

int rlzma2_dec_init(rlzma2_dec_t *p, uint8_t prop,
      uint16_t *probs, uint8_t *dic, size_t dic_size)
{
   uint32_t stream_dic_size;
   uint8_t  props[RLZMA_PROPS_SIZE];

   if (!p || !probs || !dic || dic_size == 0)
      return RLZMA_ERROR_PARAM;
   if (rlzma2_dec_dic_size_from_prop(prop, &stream_dic_size) != RLZMA_OK)
      return RLZMA_ERROR_PARAM;

   memset(p, 0, sizeof(*p));

   p->dic      = dic;
   p->dic_size = dic_size;
   p->probs    = probs;

   /* Start the underlying decoder from the widest lc/lp LZMA2 permits,
    * so the probability array the caller sized for RLZMA2_NUM_PROBS is
    * always big enough whatever props a later chunk sets. The real
    * values arrive with the first chunk that carries a props byte. */
   props[0] = (uint8_t)RLZMA2_LCLP_MAX;
   props[1] = (uint8_t)( stream_dic_size        & 0xFF);
   props[2] = (uint8_t)((stream_dic_size >>  8) & 0xFF);
   props[3] = (uint8_t)((stream_dic_size >> 16) & 0xFF);
   props[4] = (uint8_t)((stream_dic_size >> 24) & 0xFF);

   if (rlzma_stream_init(&p->lzma, props, probs, dic, dic_size) != RLZMA_OK)
      return RLZMA_ERROR_PARAM;

   memcpy(p->props, props, RLZMA_PROPS_SIZE);
   p->have_props = 1;

   rlzma2_dec_reset(p);
   return RLZMA_OK;
}

void rlzma2_dec_reset(rlzma2_dec_t *p)
{
   if (!p)
      return;

   p->state           = ST_CONTROL;
   p->control         = 0;
   p->unpack_size     = 0;
   p->pack_size       = 0;
   p->need_init_level = NEED_INIT_FULL;

   rlzma_stream_reset(&p->lzma);
}

/* Feed one header byte to the state machine, returning the next state.
 * Mirrors the reference's Lzma2Dec_UpdateState. */
static uint32_t lzma2_update_state(rlzma2_dec_t *p, uint8_t b)
{
   switch (p->state)
   {
      case ST_CONTROL:
         p->control = (uint32_t)b;

         if (b == 0)
            return ST_FINISHED;

         if (IS_UNCOMPRESSED(b))
         {
            if (b == CONTROL_COPY_RESET_DIC)
               p->need_init_level = NEED_INIT_DIC;
            else if (b > 2 || p->need_init_level == NEED_INIT_FULL)
               return ST_ERROR;
         }
         else
         {
            /* A chunk may not ask for less reset than the stream still
             * owes. This is what rejects a stream opening mid-flight. */
            if ((uint32_t)b < p->need_init_level)
               return ST_ERROR;
            p->need_init_level = 0;
            p->unpack_size     = ((uint32_t)b & 0x1F) << 16;
         }
         return ST_UNPACK0;

      case ST_UNPACK0:
         p->unpack_size |= (uint32_t)b << 8;
         return ST_UNPACK1;

      case ST_UNPACK1:
         p->unpack_size |= (uint32_t)b;
         /* Both sizes are stored biased by one. */
         p->unpack_size++;
         return IS_UNCOMPRESSED(p->control) ? ST_DATA : ST_PACK0;

      case ST_PACK0:
         p->pack_size = (uint32_t)b << 8;
         return ST_PACK1;

      case ST_PACK1:
         p->pack_size |= (uint32_t)b;
         p->pack_size++;
         return (p->control & 0x40) ? ST_PROP : ST_DATA;

      case ST_PROP:
      {
         uint32_t d = (uint32_t)b;
         uint32_t lc, lp;

         if (d >= 9 * 5 * 5)
            return ST_ERROR;
         lc = d % 9;
         d /= 9;
         lp = d % 5;

         /* LZMA2 is stricter than bare LZMA here, and the probability
          * array the caller supplied is sized on that promise. */
         if (lc + lp > RLZMA2_LCLP_MAX)
            return ST_ERROR;

         p->props[0] = b;
         if (rlzma_stream_set_props(&p->lzma, p->props) != RLZMA_OK)
            return ST_ERROR;
         return ST_DATA;
      }
   }
   return ST_ERROR;
}

int rlzma2_dec_decode(rlzma2_dec_t *p, size_t dic_limit,
      const uint8_t *src, size_t *src_len, int finish, int *status)
{
   size_t in_size;
   size_t consumed = 0;

   if (!p || !src || !src_len || !status)
      return RLZMA_ERROR_PARAM;

   in_size  = *src_len;
   *src_len = 0;
   *status  = RLZMA2_STATUS_NOT_FINISHED;

   if (dic_limit > p->dic_size)
      return RLZMA_ERROR_PARAM;

   while (p->state != ST_ERROR)
   {
      size_t dic_pos = p->lzma.dic_pos;

      if (p->state == ST_FINISHED)
      {
         *src_len = consumed;
         *status  = RLZMA2_STATUS_FINISHED;
         return RLZMA_OK;
      }

      if (dic_pos == dic_limit && !finish)
      {
         *src_len = consumed;
         *status  = RLZMA2_STATUS_NOT_FINISHED;
         return RLZMA_OK;
      }

      /* Header bytes, one at a time: a header can be split anywhere. */
      if (p->state != ST_DATA && p->state != ST_DATA_CONT)
      {
         if (consumed == in_size)
         {
            *src_len = consumed;
            *status  = RLZMA2_STATUS_NEEDS_MORE_INPUT;
            return RLZMA_OK;
         }
         p->state = lzma2_update_state(p, src[consumed]);
         consumed++;

         if (dic_pos == dic_limit && p->state != ST_FINISHED)
         {
            /* No room to decode the chunk just announced. */
            p->state = ST_ERROR;
            break;
         }
         continue;
      }

      {
         size_t in_cur  = in_size - consumed;
         size_t out_cur = dic_limit - dic_pos;

         /* Never decode past the end of the chunk's declared output. */
         if (out_cur >= (size_t)p->unpack_size)
            out_cur = (size_t)p->unpack_size;

         if (IS_UNCOMPRESSED(p->control))
         {
            if (in_cur == 0)
            {
               *src_len = consumed;
               *status  = RLZMA2_STATUS_NEEDS_MORE_INPUT;
               return RLZMA_OK;
            }

            if (p->state == ST_DATA)
            {
               /* 0x01 resets the dictionary; 0x02 continues it. Neither
                * touches the range coder, but the next LZMA chunk must
                * still re-init it, since raw bytes carry no coder
                * state. */
               int init_dic = (p->control == CONTROL_COPY_RESET_DIC);
               rlzma_stream_reset_parts(&p->lzma, init_dic, 0);
               p->lzma.need_init  = 1;
               p->lzma.remain_len = 0;
               p->lzma.temp_size  = 0;
            }

            if (in_cur > out_cur)
               in_cur = out_cur;
            if (in_cur == 0)
            {
               p->state = ST_ERROR;
               break;
            }

            if (rlzma_stream_put_uncompressed(&p->lzma, src + consumed,
                     in_cur) != RLZMA_OK)
            {
               p->state = ST_ERROR;
               break;
            }

            consumed       += in_cur;
            p->unpack_size -= (uint32_t)in_cur;
            p->state = (p->unpack_size == 0) ? ST_CONTROL : ST_DATA_CONT;
         }
         else
         {
            size_t got;
            size_t produced;
            int    sub_status;
            int    res;

            if (p->state == ST_DATA)
            {
               /* 0xE0 and up reset the dictionary; 0xA0 and up reset
                * the probability model and match history.
                *
                * The range coder is different: every LZMA chunk starts
                * a fresh one, whatever the control byte says. Each
                * chunk's packed data begins with its own five-byte
                * range coder prologue, so "no reset" means the
                * dictionary and probabilities carry over, not the
                * coder. The reference does this unconditionally in
                * LzmaDec_InitDicAndState(), which always sets
                * remainLen to at least kMatchSpecLenStart + 1 ("need
                * init range coder") and only raises it to + 2 ("and
                * state") when initState is set.
                *
                * Carrying the coder across the boundary instead makes
                * the next chunk decode its prologue bytes as symbols,
                * which corrupts output from the chunk's very first
                * byte. */
               /* Of the four combinations these two comparisons can
                * produce, three are reachable with streams liblzma
                * emits: 0x8x gives (0, 0), 0xEx gives (1, 1), and an
                * uncompressed 0x01 chunk gives (1, 0). No encoder here
                * emits 0xA0-0xDF, so (0, 1) is never selected by a test
                * stream and is verified directly against
                * rlzma_stream_reset_parts() instead. See the coverage
                * note in r7z_lzma2.h before changing this. */
               int init_dic   = (p->control >= 0xE0);
               int init_state = (p->control >= 0xA0);

               rlzma_stream_reset_parts(&p->lzma, init_dic, init_state);

               /* Force the prologue read even when neither reset
                * applied, since reset_parts() only sets need_init as
                * part of a state reset. */
               p->lzma.need_init  = 1;
               p->lzma.remain_len = 0;
               p->lzma.temp_size  = 0;
               p->lzma.range      = 0;
               p->lzma.code       = 0;

               p->state = ST_DATA_CONT;
            }

            got = in_cur;
            if (got > (size_t)p->pack_size)
               got = (size_t)p->pack_size;

            /* The streaming decoder's `finish` means "this is the last
             * input of the stream", which unlocks its tail path: final
             * symbols decoded out of the range coder's buffered state
             * with no input left. That is wrong for a chunk in the
             * middle of an LZMA2 stream. The bytes after this chunk are
             * the next chunk's header, not more of this payload, and
             * letting the decoder reach for them desyncs the range
             * coder and produces nonsense distances a few symbols
             * later.
             *
             * chunk_finish says only that the output limit covers the
             * whole chunk, which is a different claim. Signal
             * end-of-input to the LZMA layer solely when this really is
             * the caller's last input and the chunk is the last one. */
            res = rlzma_stream_decode(&p->lzma, dic_pos + out_cur,
                  src + consumed, &got,
                  (finish && got >= (size_t)p->pack_size) ? 1 : 0,
                  &sub_status);

            consumed      += got;
            p->pack_size  -= (uint32_t)got;
            produced       = p->lzma.dic_pos - dic_pos;
            p->unpack_size -= (uint32_t)produced;

            if (res != RLZMA_OK)
            {
               p->state = ST_ERROR;
               break;
            }

            if (sub_status == RLZMA_STATUS_NEEDS_MORE_INPUT)
            {
               if (p->pack_size == 0)
               {
                  /* The chunk's packed bytes are used up but it still
                   * owes output. That is normal, not corruption: the
                   * range coder holds four bytes internally, so the
                   * last symbols of a chunk decode with no input left.
                   * The decoder only says NEEDS_MORE_INPUT because it
                   * cannot know the chunk ended here.
                   *
                   * Tell it this is the end of its input by calling
                   * again with finish set, so it takes its buffered
                   * tail path. If that still produces nothing, the
                   * declared sizes really are wrong. */
                  size_t none = 0;
                  size_t before = p->lzma.dic_pos;
                  size_t extra;
                  int    res2;

                  res2 = rlzma_stream_decode(&p->lzma, dic_pos + out_cur,
                        src + consumed, &none, 1, &sub_status);

                  extra = p->lzma.dic_pos - before;

                  if (res2 != RLZMA_OK || extra == 0)
                  {
                     p->state = ST_ERROR;
                     break;
                  }

                  p->unpack_size -= (uint32_t)extra;
                  if (p->unpack_size == 0)
                     p->state = ST_CONTROL;
                  continue;
               }
               *src_len = consumed;
               *status  = RLZMA2_STATUS_NEEDS_MORE_INPUT;
               return RLZMA_OK;
            }

            if (got == 0 && produced == 0 && p->unpack_size != 0)
            {
               /* No progress and output still owed: the declared sizes
                * disagree with the data. */
               p->state = ST_ERROR;
               break;
            }

            if (p->unpack_size == 0)
            {
               /* The chunk produced everything it promised. Its packed
                * size may still have bytes left: the range coder holds
                * four bytes internally and does not need to read the
                * last normalization byte to finish the final symbol.
                * Requiring pack_size == 0 would reject nearly every
                * real stream.
                *
                * Those bytes must still be stepped over, or the next
                * chunk header is read starting one byte late and the
                * whole rest of the stream is misparsed. This applies
                * however the chunk finished, including when the final
                * call made no progress at all, which is why it is not
                * folded into the branch above. */
               if (p->pack_size != 0)
               {
                  size_t left = in_size - consumed;
                  size_t skip = (size_t)p->pack_size;

                  if (skip > left)
                     skip = left;
                  consumed     += skip;
                  p->pack_size -= (uint32_t)skip;

                  if (p->pack_size != 0)
                  {
                     /* Tail of this chunk is in the next input block. */
                     *src_len = consumed;
                     *status  = RLZMA2_STATUS_NEEDS_MORE_INPUT;
                     return RLZMA_OK;
                  }
               }
               p->state = ST_CONTROL;
            }
         }
      }

      /* Output limit reached: the caller must drain before more can be
       * decoded, whatever the input situation. */
      if (p->lzma.dic_pos >= dic_limit && p->state != ST_FINISHED)
      {
         *src_len = consumed;
         *status  = RLZMA2_STATUS_NOT_FINISHED;
         return RLZMA_OK;
      }

      /* Input exhausted. More may still be decodable from the range
       * coder's buffered state, but only inside a chunk's data phase;
       * a header needs real bytes. Returning here when the decoder
       * could still make progress would end the stream early, which is
       * how a chunk split across calls loses its tail. */
      if (consumed >= in_size)
      {
         if (p->state != ST_DATA && p->state != ST_DATA_CONT)
         {
            *src_len = consumed;
            *status  = (p->state == ST_FINISHED)
               ? RLZMA2_STATUS_FINISHED
               : RLZMA2_STATUS_NEEDS_MORE_INPUT;
            return RLZMA_OK;
         }
         if (!finish)
         {
            *src_len = consumed;
            *status  = RLZMA2_STATUS_NEEDS_MORE_INPUT;
            return RLZMA_OK;
         }
      }
   }

   *src_len = consumed;
   return RLZMA_ERROR_DATA;
}
