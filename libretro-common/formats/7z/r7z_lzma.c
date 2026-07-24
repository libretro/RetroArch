/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (r7z_lzma.c).
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
 * The probability-array layout and the context arithmetic that indexes it
 * are kept identical to the reference decoder. They are a single
 * interlocking design: the length coder deliberately aliases its own
 * low-symbol array, several tables are sized for 16 rather than 12 states
 * because the match path uses a biased state as a live index, and the
 * whole arrangement is asserted to total 1984 entries. Rearranging any
 * part of it silently corrupts the contexts of the rest.
 *
 * What differs from the reference is everything above that layer: the
 * dictionary is the caller's own output buffer rather than a separate
 * allocation, so there is no window to allocate and no copy-out step;
 * there is no allocator indirection; and one call decodes one complete
 * block. See r7z_lzma.h.
 */

#include <string.h>

#include <7z/r7z_lzma.h>

/* --------------------------------------------------------------------
 * Model constants
 *
 * Each definition below is preceded by an #undef, for the same reason
 * as in r7z_lzma_stream.c: griffin builds this whole directory as one
 * translation unit. Guarding both files rather than just one means no
 * include order can leave a decoder holding the other's RC_NORMALIZE,
 * which differ in whether they bounds-check the input.
 * -------------------------------------------------------------------- */

#undef NUM_POS_BITS_MAX
#define NUM_POS_BITS_MAX   4
#undef NUM_POS_STATES_MAX
#define NUM_POS_STATES_MAX (1 << NUM_POS_BITS_MAX)

#undef LEN_NUM_LOW_BITS
#define LEN_NUM_LOW_BITS     3
#undef LEN_NUM_LOW_SYMBOLS
#define LEN_NUM_LOW_SYMBOLS  (1 << LEN_NUM_LOW_BITS)
#undef LEN_NUM_HIGH_BITS
#define LEN_NUM_HIGH_BITS    8
#undef LEN_NUM_HIGH_SYMBOLS
#define LEN_NUM_HIGH_SYMBOLS (1 << LEN_NUM_HIGH_BITS)

/* Length coder. LEN_CHOICE aliases LEN_LOW, and LEN_CHOICE_2 sits inside
 * the low-symbol array. The overlap is intentional. */
#undef LEN_LOW
#define LEN_LOW       0
#undef LEN_HIGH
#define LEN_HIGH      (LEN_LOW + 2 * (NUM_POS_STATES_MAX << LEN_NUM_LOW_BITS))
#undef NUM_LEN_PROBS
#define NUM_LEN_PROBS (LEN_HIGH + LEN_NUM_HIGH_SYMBOLS)

#undef LEN_CHOICE
#define LEN_CHOICE    LEN_LOW
#undef LEN_CHOICE_2
#define LEN_CHOICE_2  (LEN_LOW + (1 << LEN_NUM_LOW_BITS))

#undef NUM_STATES
#define NUM_STATES     12
/* Tables indexed by a match-biased state need 16 slots, not 12. */
#undef NUM_STATES_2
#define NUM_STATES_2   16
#undef NUM_LIT_STATES
#define NUM_LIT_STATES  7

#undef START_POS_MODEL_INDEX
#define START_POS_MODEL_INDEX 4
#undef END_POS_MODEL_INDEX
#define END_POS_MODEL_INDEX   14
#undef NUM_FULL_DISTANCES
#define NUM_FULL_DISTANCES    (1 << (END_POS_MODEL_INDEX >> 1))

#undef NUM_POS_SLOT_BITS
#define NUM_POS_SLOT_BITS     6
#undef NUM_LEN_TO_POS_STATES
#define NUM_LEN_TO_POS_STATES 4

#undef NUM_ALIGN_BITS
#define NUM_ALIGN_BITS   4
#undef ALIGN_TABLE_SIZE
#define ALIGN_TABLE_SIZE (1 << NUM_ALIGN_BITS)

#undef MATCH_MIN_LEN
#define MATCH_MIN_LEN 2

/* --------------------------------------------------------------------
 * Probability array layout
 *
 * Offsets are relative to a base biased by START_OFFSET, exactly as in
 * the reference. SPEC_POS is deliberately negative relative to that
 * base. Do not reorder.
 * -------------------------------------------------------------------- */

#undef START_OFFSET
#define START_OFFSET 1664

#undef SPEC_POS
#define SPEC_POS       (-START_OFFSET)
#undef IS_REP0_LONG
#define IS_REP0_LONG   (SPEC_POS + NUM_FULL_DISTANCES)
#undef REP_LEN_CODER
#define REP_LEN_CODER  (IS_REP0_LONG + (NUM_STATES_2 << NUM_POS_BITS_MAX))
#undef LEN_CODER
#define LEN_CODER      (REP_LEN_CODER + NUM_LEN_PROBS)
#undef IS_MATCH
#define IS_MATCH       (LEN_CODER + NUM_LEN_PROBS)
#undef ALIGN_OFFS
#define ALIGN_OFFS     (IS_MATCH + (NUM_STATES_2 << NUM_POS_BITS_MAX))
#undef IS_REP
#define IS_REP         (ALIGN_OFFS + ALIGN_TABLE_SIZE)
#undef IS_REP_G0
#define IS_REP_G0      (IS_REP + NUM_STATES)
#undef IS_REP_G1
#define IS_REP_G1      (IS_REP_G0 + NUM_STATES)
#undef IS_REP_G2
#define IS_REP_G2      (IS_REP_G1 + NUM_STATES)
#undef POS_SLOT
#define POS_SLOT       (IS_REP_G2 + NUM_STATES)
#undef LITERAL
#define LITERAL        (POS_SLOT + (NUM_LEN_TO_POS_STATES << NUM_POS_SLOT_BITS))
#undef NUM_BASE_PROBS
#define NUM_BASE_PROBS (LITERAL + START_OFFSET)

/* Guard the layout the way the reference does. If either of these fires,
 * the offsets above have drifted and every context is suspect. */
#if ALIGN_OFFS != 0
#error rlzma: bad probability layout (ALIGN_OFFS must be 0)
#endif
#if NUM_BASE_PROBS != 1984
#error rlzma: bad probability layout (NUM_BASE_PROBS must be 1984)
#endif

#undef LIT_SIZE
#define LIT_SIZE 0x300

#undef NUM_MODEL_BITS
#define NUM_MODEL_BITS  11
#undef BIT_MODEL_TOTAL
#define BIT_MODEL_TOTAL (1 << NUM_MODEL_BITS)
#undef NUM_MOVE_BITS
#define NUM_MOVE_BITS   5

#undef TOP_VALUE
#define TOP_VALUE ((uint32_t)1 << 24)

/* Position/state context, used to index IS_MATCH and IS_REP0_LONG. */
#undef COMBINED_PS_STATE
#define COMBINED_PS_STATE (pos_state + state)
#undef GET_LEN_STATE
#define GET_LEN_STATE     pos_state

/* --------------------------------------------------------------------
 * Range decoder
 * -------------------------------------------------------------------- */

#undef RC_NORMALIZE
#define RC_NORMALIZE \
   if (range < TOP_VALUE) \
   { \
      if (src >= src_end) \
         return RLZMA_ERROR_DATA; \
      range <<= 8; \
      code = (code << 8) | (*src++); \
   }

#undef RC_IF_BIT_0
#define RC_IF_BIT_0(p) \
   ttt = *(p); \
   RC_NORMALIZE \
   bound = (range >> NUM_MODEL_BITS) * (uint32_t)ttt; \
   if (code < bound)

#undef RC_UPDATE_0
#define RC_UPDATE_0(p) \
   range = bound; \
   *(p)  = (uint16_t)(ttt + ((BIT_MODEL_TOTAL - ttt) >> NUM_MOVE_BITS));

#undef RC_UPDATE_1
#define RC_UPDATE_1(p) \
   range -= bound; \
   code  -= bound; \
   *(p)   = (uint16_t)(ttt - (ttt >> NUM_MOVE_BITS));

#undef RC_GET_BIT_2
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

#undef RC_GET_BIT
#define RC_GET_BIT(p, dest) RC_GET_BIT_2(p, dest, ; , ;)

#undef TREE_GET_BIT
#define TREE_GET_BIT(base, dest) { RC_GET_BIT((base) + (dest), dest); }

/* Branchless tree-bit decode.
 *
 * Modelled on the reference's hand-written kernels, which decode a
 * literal bit without any conditional jump. Two observations make it
 * work:
 *
 *  - Both probability-update arms are "p - (x >> NUM_MOVE_BITS)" for a
 *    suitably chosen x: bit 0 wants x = p - BIT_MODEL_OFFSET (which is
 *    negative, hence the arithmetic shift), bit 1 wants x = p.
 *  - range and code updates are likewise a select between two values
 *    already computed.
 *
 * The decoded bit falls out of the same comparison as a 0/1 mask, so
 * the whole step is arithmetic. This matters because the bits of a
 * literal form a serial dependency chain -- symbol feeds the next
 * probability index -- so a mispredicted branch here cannot be hidden
 * by any amount of instruction-level parallelism. */
#define BIT_MODEL_OFFSET (BIT_MODEL_TOTAL - (1 << NUM_MOVE_BITS) + 1)

#define TREE_GET_BIT_BL(base, dest) \
   { \
      uint16_t *p_bl; \
      uint32_t  pv, bnd, m, upd; \
      p_bl = (base) + (dest); \
      pv   = *p_bl; \
      RC_NORMALIZE \
      bnd  = (range >> NUM_MODEL_BITS) * pv; \
      /* m = 0 when the bit is 0, ~0 when it is 1 */ \
      m    = (uint32_t)0 - (uint32_t)(code >= bnd); \
      upd  = pv - (BIT_MODEL_OFFSET & ~m); \
      *p_bl = (uint16_t)(pv - (uint32_t)((int32_t)upd >> NUM_MOVE_BITS)); \
      /* bit 0: range = bnd.  bit 1: range -= bnd, code -= bnd. */ \
      range = bnd + ((range - bnd - bnd) & m); \
      code -= bnd & m; \
      dest  = (dest + dest) + (m & 1); \
   }

/* One bit of a matched literal: the byte one match-distance back steers
 * the context until the first bit disagrees, after which offs goes to
 * zero and this degenerates to a plain literal decode. */
#define MATCHED_LITER_DEC \
   match_byte += match_byte; \
   bit         = offs; \
   offs       &= match_byte; \
   p2          = prob_lit + (offs + bit + symbol); \
   RC_GET_BIT_2(p2, symbol, offs ^= bit, ;)

/* Branchless matched-literal step, same construction as the plain
 * literal above. The offs update (which is what makes the matched
 * literal degenerate into a normal one once the bits disagree) becomes
 * a masked xor rather than a conditional. */
#define MATCHED_LITER_DEC_BL \
   { \
      uint32_t pv, bnd, m, upd; \
      match_byte += match_byte; \
      bit         = offs; \
      offs       &= match_byte; \
      p2          = prob_lit + (offs + bit + symbol); \
      pv          = *p2; \
      RC_NORMALIZE \
      bnd   = (range >> NUM_MODEL_BITS) * pv; \
      m     = (uint32_t)0 - (uint32_t)(code >= bnd); \
      upd   = pv - (BIT_MODEL_OFFSET & ~m); \
      *p2   = (uint16_t)(pv - (uint32_t)((int32_t)upd >> NUM_MOVE_BITS)); \
      range = bnd + ((range - bnd - bnd) & m); \
      code -= bnd & m; \
      symbol = (symbol + symbol) + (m & 1); \
      offs  ^= bit & ~m; \
   }

#undef TREE_DECODE
#define TREE_DECODE(base, limit, dest) \
   { \
      dest = 1; \
      do { TREE_GET_BIT(base, dest) } while (dest < (limit)); \
      dest -= (limit); \
   }

#define TREE_DECODE_BL(base, limit, dest) \
   { \
      dest = 1; \
      do { TREE_GET_BIT_BL(base, dest) } while (dest < (limit)); \
      dest -= (limit); \
   }

/* Reverse-order bit decode. The accumulator and the tree-node index are
 * the same variable, updated by differing amounts on each branch; this
 * is what lets the caller recover the value by subtracting the node
 * index at the end. */
#define REV_BIT(p, i, a0, a1) \
   RC_IF_BIT_0((p) + (i)) \
   { \
      RC_UPDATE_0((p) + (i)) \
      a0; \
   } \
   else \
   { \
      RC_UPDATE_1((p) + (i)) \
      a1; \
   }

#define REV_BIT_VAR(p, i, m)   REV_BIT(p, i, i += m; m += m, m += m; i += m; )
#define REV_BIT_CONST(p, i, m) REV_BIT(p, i, i += m;        , i += m * 2; )
#define REV_BIT_LAST(p, i, m)  REV_BIT(p, i, i -= m         , ; )

/* --------------------------------------------------------------------
 * Properties
 * -------------------------------------------------------------------- */

int rlzma_dec_init(rlzma_dec_t *dec, const uint8_t *props)
{
   uint32_t d;
   uint32_t dict_size;

   if (!dec || !props)
      return RLZMA_ERROR_PARAM;

   d = props[0];
   if (d >= 9 * 5 * 5)
      return RLZMA_ERROR_PARAM;

   dec->lc = d % 9;
   d      /= 9;
   dec->lp = d % 5;
   dec->pb = d / 5;

   if (dec->lc + dec->lp > RLZMA_LCLP_MAX)
      return RLZMA_ERROR_PARAM;

   dict_size = (uint32_t)props[1]
             | ((uint32_t)props[2] << 8)
             | ((uint32_t)props[3] << 16)
             | ((uint32_t)props[4] << 24);

   if (dict_size < (1 << 12))
      dict_size = (1 << 12);
   dec->dict_size = dict_size;

   return RLZMA_OK;
}


/* --------------------------------------------------------------------
 * Decode
 * -------------------------------------------------------------------- */

int rlzma_dec_decode(rlzma_dec_t *dec,
      uint8_t *dst, size_t dst_len,
      const uint8_t *src, size_t src_len)
{
   uint16_t      *probs;
   uint16_t      *probs_is_match;
   uint16_t      *probs_literal;
   const uint8_t *src_end;
   uint32_t       range;
   uint32_t       code;
   uint32_t       state;
   uint32_t       rep0;
   uint32_t       rep1;
   uint32_t       rep2;
   uint32_t       rep3;
   uint32_t       pb_mask;
   uint32_t       lit_mask;
   uint32_t       lc;
   uint32_t       num_probs;
   size_t         pos;
   uint32_t       i;
   uint32_t       ttt;
   uint32_t       bound;

   if (!dec || !dst || !src)
      return RLZMA_ERROR_PARAM;
   /* Nothing requested is trivially satisfied, whatever the input. */
   if (dst_len == 0)
      return RLZMA_OK;
   /* Positions are compared against 32-bit match distances, so a block
    * larger than 4 GiB would wrap and defeat those checks. No caller
    * comes close (CHD hunks are megabytes), but the API takes size_t,
    * so reject it rather than mis-decode. */
   if (dst_len > (size_t)0xFFFFFFFFu)
      return RLZMA_ERROR_PARAM;
   /* The range coder needs a five-byte prologue. */
   if (src_len < 5)
      return RLZMA_ERROR_DATA;

   /* Bias the working pointer so the negative SPEC_POS offset lands
    * inside the array. */
   probs    = dec->probs + START_OFFSET;
   /* Keep the hottest table bases in their own locals rather than
    * re-deriving them from probs on every symbol, mirroring how the
    * reference's hand-written kernels pin them to registers. */
   probs_is_match = probs + IS_MATCH;
   probs_literal  = probs + LITERAL;
   src_end  = src + src_len;
   lc       = dec->lc;
   pb_mask  = ((uint32_t)1 << dec->pb) - 1;
   /* Selects the low lp bits of the position together with the high lc
    * bits of the previous byte, from one (pos << 8) + prev_byte value. */
   lit_mask = ((uint32_t)0x100 << dec->lp) - ((uint32_t)0x100 >> lc);

   num_probs = (uint32_t)LITERAL + ((uint32_t)LIT_SIZE << (lc + dec->lp));
   for (i = 0; i < num_probs + START_OFFSET; i++)
      dec->probs[i] = BIT_MODEL_TOTAL >> 1;

   if (src[0] != 0)
      return RLZMA_ERROR_DATA;
   code  = ((uint32_t)src[1] << 24)
         | ((uint32_t)src[2] << 16)
         | ((uint32_t)src[3] << 8)
         |  (uint32_t)src[4];
   range = 0xFFFFFFFF;
   src  += 5;

   state = 0;
   rep0  = 1;
   rep1  = 1;
   rep2  = 1;
   rep3  = 1;
   pos   = 0;

   while (pos < dst_len)
   {
      uint16_t *prob;
      /* Pre-shifted by 4, matching the reference: this single value
       * indexes IS_MATCH/IS_REP0_LONG (as pos_state + state) and the
       * length coder (stride 16, two 8-entry arrays per position). */
      uint32_t  pos_state = (((uint32_t)pos) & pb_mask) << 4;

      prob = probs_is_match + COMBINED_PS_STATE;
      RC_IF_BIT_0(prob)
      {
         uint32_t  symbol;
         uint16_t *prob_lit;
         uint32_t  lit_state;

         RC_UPDATE_0(prob)

         /* The first byte has no predecessor; its context is zero. */
         lit_state = 0;
         if (pos != 0)
            lit_state = ((((uint32_t)pos << 8) + (uint32_t)dst[pos - 1])
                        & lit_mask) << lc;

         /* The masked context is already scaled by 0x100, so the stride
          * here is 3 (one literal slot), not the full 0x300 table. */
         prob_lit = probs_literal + (uint32_t)3 * lit_state;
         symbol   = 1;

         if (state < NUM_LIT_STATES)
         {
            state -= (state < 4) ? state : 3;
            /* A literal is always exactly eight bits; unrolling removes
             * the loop condition from the hottest path in the decoder. */
            TREE_GET_BIT_BL(prob_lit, symbol)
            TREE_GET_BIT_BL(prob_lit, symbol)
            TREE_GET_BIT_BL(prob_lit, symbol)
            TREE_GET_BIT_BL(prob_lit, symbol)
            TREE_GET_BIT_BL(prob_lit, symbol)
            TREE_GET_BIT_BL(prob_lit, symbol)
            TREE_GET_BIT_BL(prob_lit, symbol)
            TREE_GET_BIT_BL(prob_lit, symbol)
         }
         else
         {
            /* Matched literal: the byte one match-distance back steers
             * the context until the first bit disagrees. */
            uint32_t match_byte;
            uint32_t offs = 0x100;

            if (rep0 > pos)
               return RLZMA_ERROR_DATA;
            match_byte = (uint32_t)dst[pos - rep0];

            state -= (state < 10) ? 3 : 6;

            {
               uint32_t  bit;
               uint16_t *p2;

               MATCHED_LITER_DEC
               MATCHED_LITER_DEC
               MATCHED_LITER_DEC
               MATCHED_LITER_DEC
               MATCHED_LITER_DEC
               MATCHED_LITER_DEC
               MATCHED_LITER_DEC
               MATCHED_LITER_DEC
            }
         }

         dst[pos++] = (uint8_t)symbol;
         continue;
      }

      RC_UPDATE_1(prob)

      {
         uint32_t len;

         prob = probs + IS_REP + state;
         RC_IF_BIT_0(prob)
         {
            /* Simple match. Bias the state to record that a distance
             * still has to be decoded; the real state transition happens
             * once the length is known. */
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
                  /* Short rep: a single byte at the current distance. */
                  RC_UPDATE_0(prob)

                  if (rep0 > pos)
                     return RLZMA_ERROR_DATA;

                  dst[pos] = dst[pos - rep0];
                  pos++;
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

         /* ---- length ---- */
         {
            uint16_t *prob_len = prob + LEN_CHOICE;

            RC_IF_BIT_0(prob_len)
            {
               RC_UPDATE_0(prob_len)
               prob_len = prob + LEN_LOW + GET_LEN_STATE;
               TREE_DECODE(prob_len, LEN_NUM_LOW_SYMBOLS, len)
            }
            else
            {
               RC_UPDATE_1(prob_len)
               prob_len = prob + LEN_CHOICE_2;
               RC_IF_BIT_0(prob_len)
               {
                  RC_UPDATE_0(prob_len)
                  prob_len = prob + LEN_LOW + GET_LEN_STATE
                           + (1 << LEN_NUM_LOW_BITS);
                  TREE_DECODE(prob_len, LEN_NUM_LOW_SYMBOLS, len)
                  len += LEN_NUM_LOW_SYMBOLS;
               }
               else
               {
                  RC_UPDATE_1(prob_len)
                  prob_len = prob + LEN_HIGH;
                  TREE_DECODE(prob_len, LEN_NUM_HIGH_SYMBOLS, len)
                  len += LEN_NUM_LOW_SYMBOLS * 2;
               }
            }
         }

         /* ---- distance, simple matches only ---- */
         if (state >= NUM_STATES)
         {
            uint32_t  pos_slot;
            uint32_t  distance;
            uint32_t  len_to_pos_state = (len < NUM_LEN_TO_POS_STATES)
                                       ? len
                                       : (NUM_LEN_TO_POS_STATES - 1);
            uint16_t *prob_slot = probs + POS_SLOT
                                + (len_to_pos_state << NUM_POS_SLOT_BITS);

            TREE_DECODE(prob_slot, 1 << NUM_POS_SLOT_BITS, pos_slot)

            if (pos_slot < START_POS_MODEL_INDEX)
               distance = pos_slot;
            else
            {
               uint32_t num_direct = (pos_slot >> 1) - 1;

               distance = (2 | (pos_slot & 1));

               if (pos_slot < END_POS_MODEL_INDEX)
               {
                  uint32_t m = 1;

                  distance <<= num_direct;
                  prob       = probs + SPEC_POS;
                  distance++;
                  do
                  {
                     REV_BIT_VAR(prob, distance, m)
                  } while (--num_direct);
                  distance -= m;
               }
               else
               {
                  /* Direct bits: fixed 50/50, no probability model. */
                  num_direct -= NUM_ALIGN_BITS;
                  do
                  {
                     RC_NORMALIZE
                     range >>= 1;
                     {
                        /* Logical shift of the borrow bit gives 0 or 1;
                         * negating yields 0 or ~0, which serves both as
                         * the conditional fixup mask and, via t + 1, as
                         * the decoded bit. Branchless. */
                        uint32_t t;
                        code    -= range;
                        t        = 0 - ((uint32_t)code >> 31);
                        distance = (distance << 1) + (t + 1);
                        code    += range & t;
                     }
                  } while (--num_direct);

                  prob       = probs + ALIGN_OFFS;
                  distance <<= NUM_ALIGN_BITS;
                  {
                     uint32_t m = 1;
                     REV_BIT_CONST(prob, m, 1)
                     REV_BIT_CONST(prob, m, 2)
                     REV_BIT_CONST(prob, m, 4)
                     REV_BIT_LAST(prob, m, 8)
                     distance |= m;
                  }

                  if (distance == 0xFFFFFFFF)
                  {
                     /* End-of-stream marker: valid only once the caller
                      * has every byte it asked for. */
                     if (pos != dst_len)
                        return RLZMA_ERROR_DATA;
                     return RLZMA_OK;
                  }
               }
            }

            rep3  = rep2;
            rep2  = rep1;
            rep1  = rep0;
            rep0  = distance + 1;
            state = (state < NUM_STATES + NUM_LIT_STATES)
                  ? NUM_LIT_STATES
                  : NUM_LIT_STATES + 3;

            if (distance >= (uint32_t)pos || distance >= dec->dict_size)
               return RLZMA_ERROR_DATA;
         }

         len += MATCH_MIN_LEN;

         /* ---- copy ---- */
         {
            size_t   copy_pos;
            uint32_t copy_len = len;

            if (rep0 > pos)
               return RLZMA_ERROR_DATA;

            /* A match may legitimately run past the end of the block;
             * the reference truncates rather than failing. */
            if ((size_t)copy_len > dst_len - pos)
               copy_len = (uint32_t)(dst_len - pos);

            copy_pos = pos - rep0;

            if (rep0 >= copy_len)
            {
               /* Source and destination cannot overlap. */
               memcpy(dst + pos, dst + copy_pos, copy_len);
               pos += copy_len;
            }
            else if (rep0 == 1)
            {
               /* Run of a single repeated byte -- by far the most common
                * overlapping case, and memset handles it directly. */
               memset(dst + pos, dst[copy_pos], copy_len);
               pos += copy_len;
            }
            else
            {
               /* Overlapping copy: run-length semantics fall out of a
                * forward copy, so memcpy is not usable. Copying in
                * rep0-sized blocks lets each block be non-overlapping
                * and doubles the run each time round. */
               uint32_t done = 0;
               while (done < copy_len)
               {
                  uint32_t chunk = rep0;
                  if (chunk > copy_len - done)
                     chunk = copy_len - done;
                  memcpy(dst + pos + done, dst + copy_pos + done, chunk);
                  done += chunk;
               }
               pos += copy_len;
            }
         }
      }
   }

   return RLZMA_OK;
}
