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

#define NUM_MODEL_BITS  11
#define BIT_MODEL_TOTAL (1 << NUM_MODEL_BITS)
#define NUM_MOVE_BITS   5
#define TOP_VALUE       ((uint32_t)1 << 24)

/* A byte is a branch candidate if it is E8 (call), E9 (jump), or the
 * second byte of a 0F 8x near jump. */
#define IS_E8_OR_E9(b) (((b) & 0xFE) == 0xE8)
#define IS_NEAR_JMP(prev, b) ((prev) == 0x0F && ((b) & 0xF0) == 0x80)

int r7z_bcj2_decode(uint8_t *dst, size_t dst_len,
      const uint8_t *main_buf, size_t main_len,
      const uint8_t *call_buf, size_t call_len,
      const uint8_t *jump_buf, size_t jump_len,
      const uint8_t *rc_buf,   size_t rc_len)
{
   uint16_t probs[RBCJ2_NUM_PROBS];
   size_t   main_pos = 0;
   size_t   call_pos = 0;
   size_t   jump_pos = 0;
   size_t   rc_pos   = 0;
   size_t   dst_pos  = 0;
   uint32_t range;
   uint32_t code;
   uint32_t ip       = 0;
   uint32_t i;
   uint8_t  prev     = 0;

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

   for (i = 0; i < RBCJ2_NUM_PROBS; i++)
      probs[i] = BIT_MODEL_TOTAL >> 1;

   /* Range coder prologue: one ignored byte then four value bytes. The
    * reference reads five and requires the resulting code to differ
    * from 0xFFFFFFFF. */
   if (rc_len < 5)
      return RBCJ2_ERROR_DATA;
   if (rc_buf[0] != 0)
      return RBCJ2_ERROR_DATA;
   code  = ((uint32_t)rc_buf[1] << 24)
         | ((uint32_t)rc_buf[2] << 16)
         | ((uint32_t)rc_buf[3] << 8)
         |  (uint32_t)rc_buf[4];
   if (code == 0xFFFFFFFFu)
      return RBCJ2_ERROR_DATA;
   range  = 0xFFFFFFFFu;
   rc_pos = 5;

   while (dst_pos < dst_len)
   {
      uint8_t  b;
      uint32_t idx;
      uint32_t bound;
      uint32_t ttt;
      int      converted;

      if (main_pos == main_len)
         return RBCJ2_ERROR_DATA;

      b = main_buf[main_pos++];
      dst[dst_pos++] = b;
      ip++;

      /* Copy through anything that cannot start a branch. */
      if (!IS_E8_OR_E9(b) && !IS_NEAR_JMP(prev, b))
      {
         prev = b;
         continue;
      }

      /* A candidate. Which context its bit is coded against depends on
       * the opcode, and for E8 on the byte before it. */
      if (b == 0xE8)
         idx = 2 + (uint32_t)prev;
      else if (b == 0xE9)
         idx = 1;
      else
         idx = 0;

      /* Normalize before reading the bit. */
      if (range < TOP_VALUE)
      {
         if (rc_pos == rc_len)
            return RBCJ2_ERROR_DATA;
         range <<= 8;
         code = (code << 8) | (uint32_t)rc_buf[rc_pos++];
      }

      ttt   = (uint32_t)probs[idx];
      bound = (range >> NUM_MODEL_BITS) * ttt;

      if (code < bound)
      {
         range     = bound;
         probs[idx] = (uint16_t)(ttt + ((BIT_MODEL_TOTAL - ttt) >> NUM_MOVE_BITS));
         converted = 0;
      }
      else
      {
         range     -= bound;
         code      -= bound;
         probs[idx] = (uint16_t)(ttt - (ttt >> NUM_MOVE_BITS));
         converted  = 1;
      }

      if (!converted)
      {
         prev = b;
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
            pos = &call_pos;
            len = call_len;
         }
         else
         {
            src = jump_buf;
            pos = &jump_pos;
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
         ip += 4;
         val -= ip;

         if (dst_len - dst_pos < 4)
            return RBCJ2_ERROR_DATA;

         dst[dst_pos]     = (uint8_t)( val        & 0xFF);
         dst[dst_pos + 1] = (uint8_t)((val >>  8) & 0xFF);
         dst[dst_pos + 2] = (uint8_t)((val >> 16) & 0xFF);
         dst[dst_pos + 3] = (uint8_t)((val >> 24) & 0xFF);
         dst_pos += 4;

         /* The context byte for whatever follows is the last byte
          * written, exactly as if it had come from the main stream. */
         prev = (uint8_t)((val >> 24) & 0xFF);
      }
   }

   /* Every byte of the target streams must have been used. Leftovers
    * mean the streams disagree about how many branches were
    * converted, which would have produced wrong output had the output
    * been longer. */
   if (call_pos != call_len || jump_pos != jump_len)
      return RBCJ2_ERROR_DATA;

   return RBCJ2_OK;
}
