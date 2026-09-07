/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (r7z_bcj2.c).
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

/* BCJ2 decoder. See r7z_bcj2.h for what the four streams are and why
 * the format is shaped this way.
 *
 * The range coder is the same one LZMA uses: 11-bit probabilities,
 * 5-bit adaptation, normalize when range drops below 2^24. It is
 * reproduced here rather than shared with r7z_lzma_stream because that
 * one is built around a resumable input pointer with a bounds contract
 * this decoder does not need; the shared part would be three macros.
 *
 * Bounds are checked on every read. The 7z container hands this
 * attacker-controlled data with four independently-sized streams, and
 * the sizes are only consistent if the archive says so, so nothing here
 * assumes a stream is as long as the others imply.
 */

#include <string.h>

#include <7z/r7z_bcj2.h>

#undef NUM_MODEL_BITS
/* griffin compiles this alongside r7z_lzma.c and r7z_lzma_stream.c,
 * which define the same range coder constants to the same values.
 * #undef first so the repeat is silent rather than a warning. */
#define NUM_MODEL_BITS  11
#undef BIT_MODEL_TOTAL
#define BIT_MODEL_TOTAL (1 << NUM_MODEL_BITS)
#undef NUM_MOVE_BITS
#define NUM_MOVE_BITS   5
#undef TOP_VALUE
#define TOP_VALUE       ((uint32_t)1 << 24)

/* A byte is a branch candidate if it is E8 (call), E9 (jump), or the
 * second byte of a 0F 8x near jump. */
#define IS_E8_OR_E9(b) (((b) & 0xFE) == 0xE8)
#define IS_NEAR_JMP(prev, b) ((prev) == 0x0F && ((b) & 0xF0) == 0x80)

void r7z_bcj2_state_init(r7z_bcj2_state_t *st)
{
   uint32_t i;

   if (!st)
      return;

   memset(st, 0, sizeof(*st));
   for (i = 0; i < RBCJ2_NUM_PROBS; i++)
      st->probs[i] = BIT_MODEL_TOTAL >> 1;
   st->range   = 0xFFFFFFFFu;
   st->started = 0;
}

int r7z_bcj2_decode_part(r7z_bcj2_state_t *st,
      uint8_t *dst, size_t dst_len, size_t dst_limit,
      const uint8_t *main_buf, size_t main_len,
      const uint8_t *call_buf, size_t call_len,
      const uint8_t *jump_buf, size_t jump_len,
      const uint8_t *rc_buf,   size_t rc_len)
{
   if (!st)
      return RBCJ2_ERROR_PARAM;
   if (!dst && dst_len != 0)
      return RBCJ2_ERROR_PARAM;
   if (!main_buf && main_len != 0)
      return RBCJ2_ERROR_PARAM;
   if (!call_buf && call_len != 0)
      return RBCJ2_ERROR_PARAM;
   if (!jump_buf && jump_len != 0)
      return RBCJ2_ERROR_PARAM;
   if (!rc_buf && rc_len != 0)
      return RBCJ2_ERROR_PARAM;

   /* Target streams are arrays of 4-byte big-endian addresses. A
    * length that is not a multiple of four cannot be one. */
   if ((call_len & 3) != 0 || (jump_len & 3) != 0)
      return RBCJ2_ERROR_DATA;

   if (dst_len == 0)
      return RBCJ2_OK;

   if (!st->started)
   {
      /* Range coder prologue: one ignored byte then four value bytes.
       * The reference reads five and requires the resulting code to
       * differ from 0xFFFFFFFF. */
      if (rc_len < 5)
         return RBCJ2_ERROR_DATA;
      if (rc_buf[0] != 0)
         return RBCJ2_ERROR_DATA;
      st->code = ((uint32_t)rc_buf[1] << 24)
               | ((uint32_t)rc_buf[2] << 16)
               | ((uint32_t)rc_buf[3] << 8)
               |  (uint32_t)rc_buf[4];
      if (st->code == 0xFFFFFFFFu)
         return RBCJ2_ERROR_DATA;
      st->range   = 0xFFFFFFFFu;
      st->rc_pos  = 5;
      st->started = 1;
   }

   if (dst_limit > dst_len)
      dst_limit = dst_len;

   while (st->dst_pos < dst_len)
   {
      uint8_t  b;
      uint32_t idx;
      uint32_t bound;
      uint32_t ttt;
      int      converted;

      /* Checked at the top of a symbol, never inside one, so the
       * four-byte operand write below always completes within the
       * call that started it. That is why the limit is a floor and
       * a call may overshoot by up to three bytes. */
      if (st->dst_pos >= dst_limit)
         return RBCJ2_PENDING;

      if (st->main_pos == main_len)
         return RBCJ2_ERROR_DATA;

      b = main_buf[st->main_pos++];
      dst[st->dst_pos++] = b;
      st->ip++;

      /* Copy through anything that cannot start a branch. */
      if (!IS_E8_OR_E9(b) && !IS_NEAR_JMP(st->prev, b))
      {
         st->prev = b;
         continue;
      }

      /* A candidate. Which context its bit is coded against depends on
       * the opcode, and for E8 on the byte before it. */
      if (b == 0xE8)
         idx = 2 + (uint32_t)st->prev;
      else if (b == 0xE9)
         idx = 1;
      else
         idx = 0;

      /* Normalize before reading the bit. */
      if (st->range < TOP_VALUE)
      {
         if (st->rc_pos == rc_len)
            return RBCJ2_ERROR_DATA;
         st->range <<= 8;
         st->code = (st->code << 8) | (uint32_t)rc_buf[st->rc_pos++];
      }

      ttt   = (uint32_t)st->probs[idx];
      bound = (st->range >> NUM_MODEL_BITS) * ttt;

      if (st->code < bound)
      {
         st->range      = bound;
         st->probs[idx] = (uint16_t)(ttt
               + ((BIT_MODEL_TOTAL - ttt) >> NUM_MOVE_BITS));
         converted      = 0;
      }
      else
      {
         st->range     -= bound;
         st->code      -= bound;
         st->probs[idx] = (uint16_t)(ttt - (ttt >> NUM_MOVE_BITS));
         converted      = 1;
      }

      if (!converted)
      {
         st->prev = b;
         continue;
      }

      /* Converted: the four operand bytes were moved to the call or
       * jump stream as a big-endian absolute address, and have to be
       * turned back into the little-endian relative address the
       * instruction actually carries. */
      {
         const uint8_t *src;
         size_t        *pos;
         size_t         len;
         uint32_t       val;

         if (b == 0xE8)
         {
            src = call_buf;
            pos = &st->call_pos;
            len = call_len;
         }
         else
         {
            src = jump_buf;
            pos = &st->jump_pos;
            len = jump_len;
         }

         if (*pos + 4 > len)
            return RBCJ2_ERROR_DATA;

         val = ((uint32_t)src[*pos]     << 24)
             | ((uint32_t)src[*pos + 1] << 16)
             | ((uint32_t)src[*pos + 2] << 8)
             |  (uint32_t)src[*pos + 3];
         *pos += 4;

         /* ip already counts the opcode byte; the displacement is
          * relative to the end of the instruction, so advance past the
          * four operand bytes before subtracting. */
         st->ip += 4;
         val    -= st->ip;

         if (dst_len - st->dst_pos < 4)
            return RBCJ2_ERROR_DATA;

         dst[st->dst_pos]     = (uint8_t)( val        & 0xFF);
         dst[st->dst_pos + 1] = (uint8_t)((val >>  8) & 0xFF);
         dst[st->dst_pos + 2] = (uint8_t)((val >> 16) & 0xFF);
         dst[st->dst_pos + 3] = (uint8_t)((val >> 24) & 0xFF);
         st->dst_pos += 4;

         /* The context byte for whatever follows is the last byte
          * written, exactly as if it had come from the main stream. */
         st->prev = (uint8_t)((val >> 24) & 0xFF);
      }
   }

   /* Every byte of the target streams must have been used. Leftovers
    * mean the streams disagree about how many branches were
    * converted, which would have produced wrong output had the output
    * been longer. */
   if (st->call_pos != call_len || st->jump_pos != jump_len)
      return RBCJ2_ERROR_DATA;

   return RBCJ2_OK;
}

int r7z_bcj2_decode(uint8_t *dst, size_t dst_len,
      const uint8_t *main_buf, size_t main_len,
      const uint8_t *call_buf, size_t call_len,
      const uint8_t *jump_buf, size_t jump_len,
      const uint8_t *rc_buf,   size_t rc_len)
{
   /* The whole-stream form, for callers with nowhere to yield to. */
   r7z_bcj2_state_t st;

   r7z_bcj2_state_init(&st);

   return r7z_bcj2_decode_part(&st, dst, dst_len, dst_len,
         main_buf, main_len, call_buf, call_len,
         jump_buf, jump_len, rc_buf, rc_len);
}
