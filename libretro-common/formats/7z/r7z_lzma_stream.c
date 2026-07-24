/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (r7z_lzma_stream.c).
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

/* Streaming LZMA decoder. See r7z_lzma_stream.h for why this exists
 * alongside the one-shot decoder in r7z_lzma.c.
 *
 * ------------------------------------------------------------------
 * How input bounds are handled, and why it is not a check in
 * RC_NORMALIZE
 * ------------------------------------------------------------------
 *
 * The one-shot decoder can test the input pointer inside its normalize
 * step and bail out, because for it running out of input is simply a
 * corrupt stream. A streaming decoder cannot do that: running out of
 * input mid-symbol is the normal case, and it has to be resumable.
 *
 * Testing the pointer between symbols instead does not work either. A
 * symbol may need up to RLZMA_REQUIRED_INPUT_MAX bytes, so one that
 * starts just below the limit normalizes straight past it and decodes
 * from whatever follows. The result is not a detected error; it is a
 * plausible-looking wrong match distance, which is exactly the kind of
 * failure that is expensive to trace back to its cause.
 *
 * So this follows the reference's contract instead:
 *
 *   - the main loop runs only while buf < buf_limit, where buf_limit is
 *     src + src_len - RLZMA_REQUIRED_INPUT_MAX. Past that point at
 *     least RLZMA_REQUIRED_INPUT_MAX + 1 bytes are still readable, which
 *     covers the worst-case symbol. RC_NORMALIZE therefore needs no
 *     check at all, and has none.
 *
 *   - the tail, where that guarantee no longer holds, is handled by
 *     try_dummy(): a decode pass that touches no state and uses a
 *     checked normalize, purely to find out whether a whole symbol is
 *     available. If it is not, the remaining bytes are stashed in
 *     temp_buf and the caller is asked for more input.
 *
 * The two normalize macros below are deliberately different, and the
 * difference is load-bearing. RC_NORMALIZE is unchecked and is used only
 * where the buf_limit contract holds. RC_NORMALIZE_CHECK is checked and
 * is used only inside try_dummy().
 */

#include <string.h>

#include <7z/r7z_lzma_stream.h>

/* --------------------------------------------------------------------
 * Model constants
 *
 * These mirror r7z_lzma.c exactly. The layout is a single interlocking
 * design (the length coder aliases its own sub-tables, SPEC_POS is
 * negative relative to the biased base) and both decoders index it the
 * same way, so the two sets must not drift apart.
 * -------------------------------------------------------------------- */

#define NUM_POS_BITS_MAX   4
#define NUM_POS_STATES_MAX (1 << NUM_POS_BITS_MAX)

#define LEN_NUM_LOW_BITS     3
#define LEN_NUM_LOW_SYMBOLS  (1 << LEN_NUM_LOW_BITS)
#define LEN_NUM_HIGH_BITS    8
#define LEN_NUM_HIGH_SYMBOLS (1 << LEN_NUM_HIGH_BITS)

#define LEN_LOW       0
#define LEN_HIGH      (LEN_LOW + 2 * (NUM_POS_STATES_MAX << LEN_NUM_LOW_BITS))
#define NUM_LEN_PROBS (LEN_HIGH + LEN_NUM_HIGH_SYMBOLS)

#define LEN_CHOICE    LEN_LOW
#define LEN_CHOICE_2  (LEN_LOW + (1 << LEN_NUM_LOW_BITS))

#define NUM_STATES     12
#define NUM_STATES_2   16
#define NUM_LIT_STATES  7

#define START_POS_MODEL_INDEX 4
#define END_POS_MODEL_INDEX   14
#define NUM_FULL_DISTANCES    (1 << (END_POS_MODEL_INDEX >> 1))

#define NUM_POS_SLOT_BITS     6
#define NUM_LEN_TO_POS_STATES 4

#define NUM_ALIGN_BITS   4
#define ALIGN_TABLE_SIZE (1 << NUM_ALIGN_BITS)

#define MATCH_MIN_LEN 2

#define START_OFFSET 1664

#define SPEC_POS       (-START_OFFSET)
#define IS_REP0_LONG   (SPEC_POS + NUM_FULL_DISTANCES)
#define REP_LEN_CODER  (IS_REP0_LONG + (NUM_STATES_2 << NUM_POS_BITS_MAX))
#define LEN_CODER      (REP_LEN_CODER + NUM_LEN_PROBS)
#define IS_MATCH       (LEN_CODER + NUM_LEN_PROBS)
#define ALIGN_OFFS     (IS_MATCH + (NUM_STATES_2 << NUM_POS_BITS_MAX))
#define IS_REP         (ALIGN_OFFS + ALIGN_TABLE_SIZE)
#define IS_REP_G0      (IS_REP + NUM_STATES)
#define IS_REP_G1      (IS_REP_G0 + NUM_STATES)
#define IS_REP_G2      (IS_REP_G1 + NUM_STATES)
#define POS_SLOT       (IS_REP_G2 + NUM_STATES)
#define LITERAL        (POS_SLOT + (NUM_LEN_TO_POS_STATES << NUM_POS_SLOT_BITS))
#define NUM_BASE_PROBS (LITERAL + START_OFFSET)

#if ALIGN_OFFS != 0
#error rlzma_stream: bad probability layout (ALIGN_OFFS must be 0)
#endif
#if NUM_BASE_PROBS != 1984
#error rlzma_stream: bad probability layout (NUM_BASE_PROBS must be 1984)
#endif

#define LIT_SIZE 0x300

#define NUM_MODEL_BITS  11
#define BIT_MODEL_TOTAL (1 << NUM_MODEL_BITS)
#define NUM_MOVE_BITS   5

#define TOP_VALUE ((uint32_t)1 << 24)

/* Sentinel length meaning "the end-of-stream marker was decoded". Kept
 * above every real match length so the copy loop never sees it. */
#define MATCH_SPEC_LEN_START \
   (MATCH_MIN_LEN + LEN_NUM_LOW_SYMBOLS * 2 + LEN_NUM_HIGH_SYMBOLS)

#define COMBINED_PS_STATE (pos_state + state)
#define GET_LEN_STATE     pos_state

/* --------------------------------------------------------------------
 * Range decoder
 * -------------------------------------------------------------------- */

/* Unchecked. Valid only under the buf_limit contract described at the
 * top of this file. Do not add a bounds test here: the tail is
 * try_dummy()'s job, and a test here would either reject valid
 * starved input or silently truncate a symbol. */
#define RC_NORMALIZE \
   if (range < TOP_VALUE) \
   { \
      range <<= 8; \
      code = (code << 8) | (*buf++); \
   }

#define RC_IF_BIT_0(p) \
   ttt = *(p); \
   RC_NORMALIZE \
   bound = (range >> NUM_MODEL_BITS) * (uint32_t)ttt; \
   if (code < bound)

#define RC_UPDATE_0(p) \
   range = bound; \
   *(p)  = (uint16_t)(ttt + ((BIT_MODEL_TOTAL - ttt) >> NUM_MOVE_BITS));

#define RC_UPDATE_1(p) \
   range -= bound; \
   code  -= bound; \
   *(p)   = (uint16_t)(ttt - (ttt >> NUM_MOVE_BITS));

#define RC_GET_BIT_2(p, dest, a0, a1) \
   RC_IF_BIT_0(p) \
   { \
      RC_UPDATE_0(p) \
      dest = (dest + dest); \
      a0; \
   } \
   else \
   { \
      RC_UPDATE_1(p) \
      dest = (dest + dest) + 1; \
      a1; \
   }

#define RC_GET_BIT(p, dest) RC_GET_BIT_2(p, dest, ; , ;)

#define TREE_GET_BIT(base, dest) { RC_GET_BIT((base) + (dest), dest); }

#define TREE_DECODE(base, limit, dest) \
   { \
      dest = 1; \
      do { TREE_GET_BIT(base, dest); } while (dest < limit); \
      dest -= limit; \
   }

#define TREE_REV_DECODE(base, limit, dest) \
   { \
      uint32_t rv_i = 1; \
      uint32_t rv_m = 1; \
      dest = 0; \
      do { \
         RC_GET_BIT_2((base) + rv_i, rv_i, ; , dest |= rv_m) \
         rv_m <<= 1; \
      } while (--limit); \
   }

/* Checked counterpart, used only by try_dummy(). Returns from the
 * enclosing function when input is exhausted, which is how "a whole
 * symbol is not available yet" is signalled. */
#define RC_NORMALIZE_CHECK \
   if (range < TOP_VALUE) \
   { \
      if (buf >= buf_lim) \
         return DUMMY_INPUT_EOF; \
      range <<= 8; \
      code = (code << 8) | (*buf++); \
   }

#define RC_IF_BIT_0_CHECK(p) \
   ttt = *(p); \
   RC_NORMALIZE_CHECK \
   bound = (range >> NUM_MODEL_BITS) * (uint32_t)ttt; \
   if (code < bound)

#define RC_UPDATE_0_CHECK  range = bound;
#define RC_UPDATE_1_CHECK  range -= bound; code -= bound;

#define RC_GET_BIT_2_CHECK(p, dest, a0, a1) \
   RC_IF_BIT_0_CHECK(p) \
   { \
      RC_UPDATE_0_CHECK \
      dest = (dest + dest); \
      a0; \
   } \
   else \
   { \
      RC_UPDATE_1_CHECK \
      dest = (dest + dest) + 1; \
      a1; \
   }

#define RC_GET_BIT_CHECK(p, dest) RC_GET_BIT_2_CHECK(p, dest, ; , ;)

#define TREE_DECODE_CHECK(base, limit, dest) \
   { \
      dest = 1; \
      do { RC_GET_BIT_CHECK((base) + dest, dest); } while (dest < limit); \
      dest -= limit; \
   }

typedef enum
{
   DUMMY_INPUT_EOF,
   DUMMY_LIT,
   DUMMY_MATCH,
   DUMMY_REP
} dummy_res_t;

/* --------------------------------------------------------------------
 * try_dummy
 *
 * Decodes one symbol against a copy of the range coder state, updating
 * nothing. The only question it answers is whether the input holds a
 * complete symbol. Probability entries are read but never written, and
 * the dictionary is not touched.
 * -------------------------------------------------------------------- */

static dummy_res_t try_dummy(const rlzma_stream_t *s,
      const uint8_t *buf, const uint8_t **buf_out)
{
   const uint8_t   *buf_lim = *buf_out;
   uint32_t         range   = s->range;
   uint32_t         code    = s->code;
   uint32_t         state   = s->state;
   const uint16_t  *probs   = s->probs + START_OFFSET;
   uint32_t         pos_state;
   uint32_t         ttt;
   uint32_t         bound;
   const uint16_t  *prob;
   dummy_res_t      res;

   pos_state = ((uint32_t)(s->dic_pos) & (((uint32_t)1 << s->pb) - 1)) << 4;

   prob = probs + IS_MATCH + COMBINED_PS_STATE;
   RC_IF_BIT_0_CHECK(prob)
   {
      uint32_t sym  = 1;
      uint32_t offs = (uint32_t)0x100;
      uint32_t lit_mask;
      uint32_t lit_state;

      RC_UPDATE_0_CHECK

      lit_mask  = ((uint32_t)0x100 << s->lp) - ((uint32_t)0x100 >> s->lc);
      lit_state = 0;
      if (s->dic_pos != 0)
         lit_state = ((((uint32_t)s->dic_pos << 8)
                  + (uint32_t)s->dic[s->dic_pos - 1]) & lit_mask) << s->lc;

      prob = probs + LITERAL + (uint32_t)3 * lit_state;

      if (state >= NUM_LIT_STATES)
      {
         /* Matched literal, mirroring decode_real exactly. */
         uint32_t        dist = s->rep0;
         size_t          mp;
         uint32_t        match_byte;
         uint32_t        bit;
         const uint16_t *pb2;

         /* Same window check as decode_real. try_dummy only probes
          * whether a symbol is complete, but it still dereferences the
          * dictionary, so it must not wrap out of bounds either. */
         if ((size_t)dist > s->dic_size)
            return DUMMY_INPUT_EOF;
         mp = (s->dic_pos >= dist)
            ? s->dic_pos - dist
            : s->dic_pos + s->dic_size - dist;
         match_byte = (uint32_t)s->dic[mp];

         do
         {
            match_byte += match_byte;
            bit         = offs;
            offs       &= match_byte;
            pb2         = prob + (offs + bit + sym);
            RC_GET_BIT_2_CHECK(pb2, sym, offs ^= bit, ;)
         } while (sym < 0x100);
      }
      while (sym < 0x100)
      {
         RC_GET_BIT_CHECK(prob + sym, sym)
      }
      res = DUMMY_LIT;
   }
   else
   {
      uint32_t len;

      RC_UPDATE_1_CHECK

      prob = probs + IS_REP + state;
      RC_IF_BIT_0_CHECK(prob)
      {
         RC_UPDATE_0_CHECK
         state = 0;
         prob  = probs + LEN_CODER;
         res   = DUMMY_MATCH;
      }
      else
      {
         RC_UPDATE_1_CHECK
         res  = DUMMY_REP;
         prob = probs + IS_REP_G0 + state;
         RC_IF_BIT_0_CHECK(prob)
         {
            RC_UPDATE_0_CHECK
            prob = probs + IS_REP0_LONG + COMBINED_PS_STATE;
            RC_IF_BIT_0_CHECK(prob)
            {
               RC_UPDATE_0_CHECK
               /* Short rep: one byte, no length coder. */
               *buf_out = buf;
               return DUMMY_REP;
            }
            else
            {
               RC_UPDATE_1_CHECK
            }
         }
         else
         {
            RC_UPDATE_1_CHECK
            prob = probs + IS_REP_G1 + state;
            RC_IF_BIT_0_CHECK(prob)
            {
               RC_UPDATE_0_CHECK
            }
            else
            {
               RC_UPDATE_1_CHECK
               prob = probs + IS_REP_G2 + state;
               RC_IF_BIT_0_CHECK(prob)
               {
                  RC_UPDATE_0_CHECK
               }
               else
               {
                  RC_UPDATE_1_CHECK
               }
            }
         }
         state = NUM_LIT_STATES;
         prob  = probs + REP_LEN_CODER;
      }

      /* Length coder, shared between match and rep. */
      {
         const uint16_t *prob_len = prob + LEN_CHOICE;
         RC_IF_BIT_0_CHECK(prob_len)
         {
            RC_UPDATE_0_CHECK
            prob_len = prob + LEN_LOW + GET_LEN_STATE;
            TREE_DECODE_CHECK(prob_len, 1 << LEN_NUM_LOW_BITS, len)
         }
         else
         {
            RC_UPDATE_1_CHECK
            prob_len = prob + LEN_CHOICE_2;
            RC_IF_BIT_0_CHECK(prob_len)
            {
               RC_UPDATE_0_CHECK
               prob_len = prob + LEN_LOW
                  + ((uint32_t)1 << LEN_NUM_LOW_BITS) + GET_LEN_STATE;
               TREE_DECODE_CHECK(prob_len, 1 << LEN_NUM_LOW_BITS, len)
               len += LEN_NUM_LOW_SYMBOLS;
            }
            else
            {
               RC_UPDATE_1_CHECK
               prob_len = prob + LEN_HIGH;
               TREE_DECODE_CHECK(prob_len, 1 << LEN_NUM_HIGH_BITS, len)
               len += LEN_NUM_LOW_SYMBOLS * 2;
            }
         }
      }

      if (state < 4)
      {
         uint32_t pos_slot;
         prob = probs + POS_SLOT +
            ((len < NUM_LEN_TO_POS_STATES ? len : NUM_LEN_TO_POS_STATES - 1)
             << NUM_POS_SLOT_BITS);
         TREE_DECODE_CHECK(prob, 1 << NUM_POS_SLOT_BITS, pos_slot)

         if (pos_slot >= START_POS_MODEL_INDEX)
         {
            uint32_t num_direct = (pos_slot >> 1) - 1;

            if (pos_slot >= END_POS_MODEL_INDEX)
            {
               /* Direct bits, then the four aligned bits. */
               num_direct -= NUM_ALIGN_BITS;
               do
               {
                  RC_NORMALIZE_CHECK
                  range >>= 1;
                  code -= range & (((code - range) >> 31) - 1);
               } while (--num_direct);
               num_direct = NUM_ALIGN_BITS;
            }
            {
               uint32_t i = 1;
               uint32_t m = 1;
               if (pos_slot >= END_POS_MODEL_INDEX)
                  prob = probs + ALIGN_OFFS;
               else
               {
                  uint32_t base = ((2 | (pos_slot & 1)) << num_direct);
                  prob = probs + SPEC_POS + (int32_t)base
                     - (int32_t)pos_slot - 1;
               }
               do
               {
                  RC_GET_BIT_2_CHECK(prob + i, i, ; , ;)
                  m <<= 1;
               } while (--num_direct);
            }
         }
      }
   }

   *buf_out = buf;
   return res;
}

/* --------------------------------------------------------------------
 * Decode core
 *
 * Decodes symbols while buf < buf_limit and dic_pos < dic_limit. On
 * return, s->range/code/state/rep* and the probability model reflect
 * every symbol fully decoded, and *buf_out points just past the input
 * consumed.
 * -------------------------------------------------------------------- */

static int decode_real(rlzma_stream_t *s, size_t dic_limit,
      const uint8_t *buf, const uint8_t *buf_limit, const uint8_t **buf_out)
{
   uint16_t *probs          = s->probs + START_OFFSET;
   uint16_t *probs_is_match = probs + IS_MATCH;
   uint16_t *probs_literal  = probs + LITERAL;
   uint8_t  *dic            = s->dic;
   size_t    dic_size       = s->dic_size;
   size_t    dic_pos        = s->dic_pos;
   uint64_t  total_pos      = s->total_pos;
   uint32_t  range          = s->range;
   uint32_t  code           = s->code;
   uint32_t  state          = s->state;
   uint32_t  rep0           = s->rep0;
   uint32_t  rep1           = s->rep1;
   uint32_t  rep2           = s->rep2;
   uint32_t  rep3           = s->rep3;
   uint32_t  pb_mask        = ((uint32_t)1 << s->pb) - 1;
   uint32_t  lc             = s->lc;
   uint32_t  lit_mask       = ((uint32_t)0x100 << s->lp)
                            - ((uint32_t)0x100 >> lc);
   uint32_t  ttt;
   uint32_t  bound;
   int       ret            = RLZMA_OK;

   do
   {
      uint16_t *prob;
      uint32_t  pos_state = ((uint32_t)dic_pos & pb_mask) << 4;
      uint32_t  len;

      prob = probs_is_match + COMBINED_PS_STATE;
      RC_IF_BIT_0(prob)
      {
         uint32_t sym  = 1;
         uint32_t offs = (uint32_t)0x100;
         uint32_t lit_state;

         RC_UPDATE_0(prob)

         /* The first byte of the window has no predecessor; its context
          * is zero, exactly as in the one-shot decoder. */
         lit_state = 0;
         if (dic_pos != 0)
            lit_state = ((((uint32_t)dic_pos << 8)
                     + (uint32_t)dic[dic_pos - 1]) & lit_mask) << lc;

         /* The masked context is already scaled by 0x100, so the stride
          * is 3 (one literal slot), not the full 0x300 table. */
         prob = probs_literal + (uint32_t)3 * lit_state;

         if (state >= NUM_LIT_STATES)
         {
            uint32_t  dist = rep0;
            size_t    mp;
            uint32_t  match_byte;
            uint32_t  bit;
            uint16_t *pb2;

            if ((size_t)dist > dic_size)
            {
               ret = RLZMA_ERROR_DATA;
               goto done;
            }
            mp = (dic_pos >= (size_t)dist)
               ? dic_pos - dist
               : dic_pos + dic_size - dist;
            match_byte = (uint32_t)dic[mp];

            /* Matched literal: the byte one match-distance back steers
             * the context until the first bit disagrees. offs collapses
             * to zero at that point and the rest decodes as a plain
             * literal, so this always runs the full eight bits.
             *
             * The order matters: bit takes the *old* mask, then offs is
             * folded with match_byte before it is used to index. */
            do
            {
               match_byte += match_byte;
               bit         = offs;
               offs       &= match_byte;
               pb2         = prob + (offs + bit + sym);
               RC_GET_BIT_2(pb2, sym, offs ^= bit, ;)
            } while (sym < 0x100);
         }
         while (sym < 0x100)
         {
            RC_GET_BIT(prob + sym, sym)
         }

         dic[dic_pos++] = (uint8_t)sym;
         total_pos++;

         /* Literal state transition. */
         if (state < 4)
            state = 0;
         else if (state < 10)
            state -= 3;
         else
            state -= 6;

         continue;
      }

      RC_UPDATE_1(prob)

      prob = probs + IS_REP + state;
      RC_IF_BIT_0(prob)
      {
         RC_UPDATE_0(prob)
         state += NUM_STATES;
         prob   = probs + LEN_CODER;
      }
      else
      {
         RC_UPDATE_1(prob)
         prob = probs + IS_REP_G0 + state;
         RC_IF_BIT_0(prob)
         {
            RC_UPDATE_0(prob)
            prob = probs + IS_REP0_LONG + COMBINED_PS_STATE;
            RC_IF_BIT_0(prob)
            {
               size_t mp;
               RC_UPDATE_0(prob)
               /* Short rep: a single byte at the current distance.
                * rep0 is the distance itself, so the source is
                * dic_pos - rep0 (not rep0 - 1).
                *
                * rep0 was validated when it was decoded, but a reused
                * rep distance is only as good as the window it is
                * replayed into, so re-check before wrapping. */
               if ((size_t)rep0 > dic_size)
               {
                  ret = RLZMA_ERROR_DATA;
                  goto done;
               }
               mp = (dic_pos >= (size_t)rep0)
                  ? dic_pos - rep0
                  : dic_pos + dic_size - rep0;
               dic[dic_pos] = dic[mp];
               dic_pos++;
               total_pos++;
               state = (state < NUM_LIT_STATES) ? 9 : 11;
               continue;
            }
            RC_UPDATE_1(prob)
         }
         else
         {
            uint32_t dist;
            RC_UPDATE_1(prob)
            prob = probs + IS_REP_G1 + state;
            RC_IF_BIT_0(prob)
            {
               RC_UPDATE_0(prob)
               dist = rep1;
            }
            else
            {
               RC_UPDATE_1(prob)
               prob = probs + IS_REP_G2 + state;
               RC_IF_BIT_0(prob)
               {
                  RC_UPDATE_0(prob)
                  dist = rep2;
               }
               else
               {
                  RC_UPDATE_1(prob)
                  dist = rep3;
                  rep3 = rep2;
               }
               rep2 = rep1;
            }
            rep1 = rep0;
            rep0 = dist;
         }
         state = (state < NUM_LIT_STATES) ? 8 : 11;
         prob  = probs + REP_LEN_CODER;
      }

      /* Length coder. */
      {
         uint16_t *prob_len = prob + LEN_CHOICE;
         RC_IF_BIT_0(prob_len)
         {
            RC_UPDATE_0(prob_len)
            prob_len = prob + LEN_LOW + GET_LEN_STATE;
            TREE_DECODE(prob_len, (1 << LEN_NUM_LOW_BITS), len)
         }
         else
         {
            RC_UPDATE_1(prob_len)
            prob_len = prob + LEN_CHOICE_2;
            RC_IF_BIT_0(prob_len)
            {
               RC_UPDATE_0(prob_len)
               prob_len = prob + LEN_LOW
                  + ((uint32_t)1 << LEN_NUM_LOW_BITS) + GET_LEN_STATE;
               TREE_DECODE(prob_len, (1 << LEN_NUM_LOW_BITS), len)
               len += LEN_NUM_LOW_SYMBOLS;
            }
            else
            {
               RC_UPDATE_1(prob_len)
               prob_len = prob + LEN_HIGH;
               TREE_DECODE(prob_len, (1 << LEN_NUM_HIGH_BITS), len)
               len += LEN_NUM_LOW_SYMBOLS * 2;
            }
         }
      }

      /* A fresh match (state was biased by NUM_STATES above) also needs
       * its distance decoded. */
      if (state >= NUM_STATES)
      {
         uint32_t dist;
         uint32_t pos_slot;

         state -= NUM_STATES;
         state  = (state < NUM_LIT_STATES) ? 7 : 10;

         prob = probs + POS_SLOT +
            ((len < NUM_LEN_TO_POS_STATES
              ? len : NUM_LEN_TO_POS_STATES - 1) << NUM_POS_SLOT_BITS);
         TREE_DECODE(prob, 1 << NUM_POS_SLOT_BITS, pos_slot)

         if (pos_slot < START_POS_MODEL_INDEX)
            dist = pos_slot;
         else
         {
            uint32_t num_direct = (pos_slot >> 1) - 1;
            dist = (2 | (pos_slot & 1));

            if (pos_slot < END_POS_MODEL_INDEX)
            {
               uint32_t rev;
               dist <<= num_direct;
               prob   = probs + SPEC_POS + (int32_t)dist
                  - (int32_t)pos_slot - 1;
               TREE_REV_DECODE(prob, num_direct, rev)
               dist += rev;
            }
            else
            {
               uint32_t rev;
               num_direct -= NUM_ALIGN_BITS;
               do
               {
                  uint32_t t;
                  RC_NORMALIZE
                  range >>= 1;
                  /* Branchless direct bit: subtract, then add back on
                   * borrow, taking the bit from the sign. */
                  code    -= range;
                  t        = 0 - ((uint32_t)code >> 31);
                  code    += range & t;
                  dist     = (dist << 1) + (t + 1);
               } while (--num_direct);

               prob = probs + ALIGN_OFFS;
               dist <<= NUM_ALIGN_BITS;
               {
                  uint32_t lim = NUM_ALIGN_BITS;
                  TREE_REV_DECODE(prob, lim, rev)
               }
               dist += rev;

               if (dist == (uint32_t)0xFFFFFFFF)
               {
                  /* End-of-stream marker. */
                  len = MATCH_SPEC_LEN_START;
                  state -= NUM_STATES;
                  break;
               }
            }
         }

         rep3 = rep2;
         rep2 = rep1;
         rep1 = rep0;
         rep0 = dist + 1;

         /* Mirrors the reference's
          *   distance >= (checkDicSize == 0 ? processedPos : checkDicSize)
          *
          * Until the stream has produced dict_size bytes, a distance
          * may not reach back further than what has actually been
          * produced; after that, the dictionary bounds it.
          *
          * The effective dictionary is the larger of what the stream
          * declared and the window the caller supplied. liblzma treats
          * an LZMA2 dict_size as a floor rather than a cap and emits
          * distances well beyond it, so bounding on the declared size
          * alone rejects valid streams. Bounding on the window is safe:
          * it is the memory that actually exists, and the wrap
          * arithmetic below is defined for any distance within it. */
         {
            uint64_t reach = (uint64_t)s->dict_size;

            if ((uint64_t)dic_size > reach)
               reach = (uint64_t)dic_size;
            if (total_pos < reach)
               reach = total_pos;

            if ((uint64_t)dist >= reach)
            {
               ret = RLZMA_ERROR_DATA;
               goto done;
            }
         }
      }

      len += MATCH_MIN_LEN;

      /* Copy the match, clipped to the output limit. Whatever does not
       * fit is carried in remain_len and resumed on the next call. */
      {
         size_t   rem  = dic_limit - dic_pos;
         uint32_t curr = len;
         size_t   mp;

         if ((size_t)curr > rem)
            curr = (uint32_t)rem;

         if ((size_t)rep0 > dic_size)
         {
            ret = RLZMA_ERROR_DATA;
            goto done;
         }
         mp = (dic_pos >= (size_t)rep0)
            ? dic_pos - rep0
            : dic_pos + dic_size - rep0;

         s->remain_len = len - curr;

         while (curr--)
         {
            dic[dic_pos++] = dic[mp++];
            total_pos++;
            if (mp == dic_size)
               mp = 0;
         }

         if (s->remain_len != 0)
            break;
      }
   } while (dic_pos < dic_limit && buf < buf_limit);

done:
   /* No normalization here. The range coder is normalized lazily, at
    * the point each symbol needs it. Normalizing on exit would consume
    * an input byte that the next call is going to need, which desyncs
    * the decoder and starves the tail of the block. */
   s->range   = range;
   s->code    = code;
   s->state   = state;
   s->rep0    = rep0;
   s->rep1    = rep1;
   s->rep2    = rep2;
   s->rep3    = rep3;
   s->dic_pos   = dic_pos;
   s->total_pos = total_pos;
   *buf_out     = buf;
   return ret;
}

/* --------------------------------------------------------------------
 * Public entry points
 * -------------------------------------------------------------------- */

/* Parse the five props bytes into the fields they set. Shared by
 * rlzma_stream_init() and rlzma_stream_set_props() so the two cannot
 * disagree about what a props byte means. */
static int stream_parse_props(rlzma_stream_t *s, const uint8_t *props)
{
   uint32_t d;
   uint32_t lc;
   uint32_t lp;
   uint32_t pb;

   d = (uint32_t)props[0];
   if (d >= 9 * 5 * 5)
      return RLZMA_ERROR_PARAM;

   lc = d % 9;
   d /= 9;
   lp = d % 5;
   pb = d / 5;

   if (lc + lp > RLZMA_STREAM_LCLP_MAX)
      return RLZMA_ERROR_PARAM;

   s->dict_size = (uint32_t)props[1]
                | ((uint32_t)props[2] << 8)
                | ((uint32_t)props[3] << 16)
                | ((uint32_t)props[4] << 24);

   s->lc        = lc;
   s->lp        = lp;
   s->pb        = pb;
   s->num_probs = (uint32_t)LITERAL + ((uint32_t)LIT_SIZE << (lc + lp));

   return RLZMA_OK;
}

int rlzma_stream_init(rlzma_stream_t *s, const uint8_t *props,
      uint16_t *probs, uint8_t *dic, size_t dic_size)
{
   if (!s || !props || !probs || !dic || dic_size == 0)
      return RLZMA_ERROR_PARAM;

   memset(s, 0, sizeof(*s));

   if (stream_parse_props(s, props) != RLZMA_OK)
      return RLZMA_ERROR_PARAM;

   s->probs     = probs;
   s->dic       = dic;
   s->dic_size  = dic_size;

   return RLZMA_OK;
}

int rlzma_stream_set_props(rlzma_stream_t *s, const uint8_t *props)
{
   if (!s || !props)
      return RLZMA_ERROR_PARAM;
   return stream_parse_props(s, props);
}

int rlzma_stream_put_uncompressed(rlzma_stream_t *s,
      const uint8_t *src, size_t len)
{
   if (!s || !src)
      return RLZMA_ERROR_PARAM;
   if (len > s->dic_size - s->dic_pos)
      return RLZMA_ERROR_PARAM;

   /* An LZMA2 uncompressed chunk is literal bytes that still become
    * part of the dictionary, so later matches can refer back into
    * them. total_pos must advance too, or the distance check would
    * treat those bytes as never having been produced. */
   memcpy(s->dic + s->dic_pos, src, len);
   s->dic_pos   += len;
   s->total_pos += (uint64_t)len;
   return RLZMA_OK;
}

void rlzma_stream_reset_parts(rlzma_stream_t *s, int init_dic, int init_state)
{
   if (!s)
      return;

   /* LZMA2 needs these two independently. A chunk may reset the decoder
    * state while continuing the previous chunk's dictionary, or reset
    * the dictionary while the range coder carries on. Collapsing them
    * into one operation would make those chunk types undecodable. */
   if (init_dic)
   {
      s->dic_pos    = 0;
      s->total_pos  = 0;
      s->got_marker = 0;
   }

   if (init_state)
   {
      uint32_t i;

      s->need_init  = 1;
      s->remain_len = 0;
      s->temp_size  = 0;
      s->state      = 0;
      s->rep0       = 1;
      s->rep1       = 1;
      s->rep2       = 1;
      s->rep3       = 1;
      s->range      = 0;
      s->code       = 0;

      for (i = 0; i < s->num_probs + START_OFFSET; i++)
         s->probs[i] = BIT_MODEL_TOTAL >> 1;
   }
}

void rlzma_stream_reset(rlzma_stream_t *s)
{
   rlzma_stream_reset_parts(s, 1, 1);
}

/* Consume the five-byte range coder prologue. */
static int stream_init_rc(rlzma_stream_t *s, const uint8_t *src)
{
   if (src[0] != 0)
      return RLZMA_ERROR_DATA;

   s->code  = ((uint32_t)src[1] << 24)
            | ((uint32_t)src[2] << 16)
            | ((uint32_t)src[3] << 8)
            |  (uint32_t)src[4];
   s->range     = 0xFFFFFFFF;
   s->need_init = 0;
   return RLZMA_OK;
}

/* Resume a match that did not fit in the previous output window. */
static void stream_copy_remain(rlzma_stream_t *s, size_t dic_limit)
{
   size_t   rem  = dic_limit - s->dic_pos;
   uint32_t curr = s->remain_len;
   size_t   mp;

   if ((size_t)curr > rem)
      curr = (uint32_t)rem;

   if ((size_t)s->rep0 > s->dic_size)
   {
      /* Cannot happen for a stream that got this far, since the
       * distance was checked when the match was decoded; belt and
       * braces so the wrap below can never underflow. */
      s->remain_len = 0;
      return;
   }

   mp = (s->dic_pos >= (size_t)s->rep0)
      ? s->dic_pos - s->rep0
      : s->dic_pos + s->dic_size - s->rep0;

   s->remain_len -= curr;
   while (curr--)
   {
      s->dic[s->dic_pos++] = s->dic[mp++];
      s->total_pos++;
      if (mp == s->dic_size)
         mp = 0;
   }
}

int rlzma_stream_decode(rlzma_stream_t *s, size_t dic_limit,
      const uint8_t *src, size_t *src_len, int finish, int *status)
{
   size_t in_size;
   size_t consumed = 0;

   if (!s || !src || !src_len || !status)
      return RLZMA_ERROR_PARAM;

   in_size = *src_len;
   *src_len = 0;
   *status  = RLZMA_STATUS_NOT_FINISHED;

   if (dic_limit > s->dic_size)
      return RLZMA_ERROR_PARAM;

   /* The prologue may itself be split across calls. */
   if (s->need_init)
   {
      while (s->temp_size < 5)
      {
         if (consumed == in_size)
         {
            *src_len = consumed;
            *status  = RLZMA_STATUS_NEEDS_MORE_INPUT;
            return RLZMA_OK;
         }
         s->temp_buf[s->temp_size++] = src[consumed++];
      }
      if (stream_init_rc(s, s->temp_buf) != RLZMA_OK)
      {
         *src_len = consumed;
         return RLZMA_ERROR_DATA;
      }
      s->temp_size = 0;
   }

   for (;;)
   {
      if (s->got_marker)
      {
         *src_len = consumed;
         *status  = RLZMA_STATUS_FINISHED;
         return RLZMA_OK;
      }

      /* Finish any match left over from the previous call first. */
      if (s->remain_len != 0)
      {
         stream_copy_remain(s, dic_limit);
         if (s->remain_len != 0)
         {
            /* Output filled before the match completed. */
            *src_len = consumed;
            *status  = RLZMA_STATUS_NOT_FINISHED;
            return RLZMA_OK;
         }
      }

      if (s->dic_pos >= dic_limit)
      {
         *src_len = consumed;
         *status  = RLZMA_STATUS_NOT_FINISHED;
         return RLZMA_OK;
      }

      if (s->temp_size == 0)
      {
         const uint8_t *cur   = src + consumed;
         size_t         avail = in_size - consumed;
         const uint8_t *buf_limit;
         const uint8_t *buf_out;
         int            res;

         if (avail == 0)
         {
            /* Input is exhausted, but that does not mean decoding is
             * stuck. The range coder carries four bytes in `code`, so
             * the last symbols of a block routinely decode without
             * consuming anything new: `range` is already above
             * TOP_VALUE and no normalization is required.
             *
             * Returning here unconditionally would leave the block one
             * symbol short. But decoding straight from `src` is not
             * safe either: a corrupt stream can present a symbol that
             * does need a byte, and the main loop's RC_NORMALIZE is
             * unchecked by design, so it would read past the end.
             *
             * So decode out of temp_buf instead, which is
             * RLZMA_REQUIRED_INPUT_MAX bytes and zero-filled here.
             * A well-formed final symbol never touches the padding; a
             * malformed one reads zeros and fails as bad data rather
             * than overrunning the caller's buffer. */
            if (!finish)
            {
               *src_len = consumed;
               *status  = RLZMA_STATUS_NEEDS_MORE_INPUT;
               return RLZMA_OK;
            }
            memset(s->temp_buf, 0, sizeof(s->temp_buf));
            {
               const uint8_t *buf_out2;
               int            res2 = decode_real(s, dic_limit,
                     s->temp_buf, s->temp_buf, &buf_out2);

               if (res2 != RLZMA_OK)
               {
                  *src_len = consumed;
                  return res2;
               }
               if ((size_t)(buf_out2 - s->temp_buf) != 0)
               {
                  /* The symbol needed input that does not exist. */
                  *src_len = consumed;
                  return RLZMA_ERROR_DATA;
               }
               if (s->remain_len == MATCH_SPEC_LEN_START)
               {
                  s->got_marker = 1;
                  s->remain_len = 0;
               }
               continue;
            }
         }
         else if (avail < RLZMA_REQUIRED_INPUT_MAX)
         {
            /* Too close to the end to guarantee a whole symbol. Ask
             * try_dummy() whether one is actually there. */
            const uint8_t *dummy_end = cur + avail;
            dummy_res_t    dres      = try_dummy(s, cur, &dummy_end);

            if (dres == DUMMY_INPUT_EOF)
            {
               size_t i;
               if (!finish)
               {
                  for (i = 0; i < avail; i++)
                     s->temp_buf[i] = cur[i];
                  s->temp_size = (uint32_t)avail;
                  consumed    += avail;
                  *src_len     = consumed;
                  *status      = RLZMA_STATUS_NEEDS_MORE_INPUT;
                  return RLZMA_OK;
               }
               *src_len = consumed;
               return RLZMA_ERROR_DATA;
            }
            /* A whole symbol is present: decode exactly one. */
            buf_limit = cur;
         }
         else
            buf_limit = cur + avail - RLZMA_REQUIRED_INPUT_MAX;

         res = decode_real(s, dic_limit, cur, buf_limit, &buf_out);
         consumed += (size_t)(buf_out - cur);

         if (res != RLZMA_OK)
         {
            *src_len = consumed;
            return res;
         }
         if (s->remain_len == MATCH_SPEC_LEN_START)
         {
            s->got_marker = 1;
            s->remain_len = 0;
         }
      }
      else
      {
         /* A partial symbol is stashed. Top temp_buf up from the new
          * input until a whole symbol is present, then decode from
          * temp_buf and re-sync the consumed count. */
         size_t         avail = in_size - consumed;
         const uint8_t *dummy_end;
         dummy_res_t    dres;
         size_t         want;
         size_t         i;
         size_t         have = s->temp_size;

         want = RLZMA_REQUIRED_INPUT_MAX - have;
         if (want > avail)
            want = avail;
         for (i = 0; i < want; i++)
            s->temp_buf[have + i] = src[consumed + i];
         have += want;

         dummy_end = s->temp_buf + have;
         dres      = try_dummy(s, s->temp_buf, &dummy_end);

         if (dres == DUMMY_INPUT_EOF)
         {
            if (!finish)
            {
               s->temp_size = (uint32_t)have;
               consumed    += want;
               *src_len     = consumed;
               *status      = RLZMA_STATUS_NEEDS_MORE_INPUT;
               return RLZMA_OK;
            }
            *src_len = consumed;
            return RLZMA_ERROR_DATA;
         }
         else
         {
            const uint8_t *buf_out;
            int            res = decode_real(s, dic_limit, s->temp_buf,
                  s->temp_buf, &buf_out);
            size_t         used = (size_t)(buf_out - s->temp_buf);

            if (res != RLZMA_OK)
            {
               *src_len = consumed;
               return res;
            }
            if (used < s->temp_size)
            {
               /* Should not happen: the stashed bytes are always part
                * of the symbol just decoded. */
               *src_len = consumed;
               return RLZMA_ERROR_DATA;
            }
            consumed    += used - s->temp_size;
            s->temp_size = 0;

            if (s->remain_len == MATCH_SPEC_LEN_START)
            {
               s->got_marker = 1;
               s->remain_len = 0;
            }
         }
      }

      if (consumed >= in_size && s->dic_pos < dic_limit)
      {
         /* Input consumed but output still wanted. If this is the final
          * input, keep looping: the remaining symbols may decode from
          * the range coder's buffered state alone. The loop still
          * terminates, because decode_real with buf_limit == cur makes
          * progress on every pass (it decodes at least one symbol, or
          * fills the output, or errors). */
         if (!finish)
         {
            *src_len = consumed;
            *status  = RLZMA_STATUS_NEEDS_MORE_INPUT;
            return RLZMA_OK;
         }
      }
   }
}
