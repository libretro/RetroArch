/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (encoding_rlz4.c).
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

#include <string.h>

#include <encodings/rlz4.h>

/* Read a saturating length extension: the nibble said 15, so bytes
 * follow, each adding its value, ending at the first byte below 255.
 * Returns 0 on truncation. */
static int rlz4_extend(const uint8_t *src, size_t limit, size_t *pos,
      size_t *len)
{
   uint8_t b;
   do
   {
      if (*pos >= limit)
         return 0;
      b     = src[(*pos)++];
      *len += b;
   } while (b == 255);
   return 1;
}

int64_t rlz4_decode(uint8_t *dst, size_t dst_len,
      const uint8_t *src, size_t src_len)
{
   size_t ip = 0; /* input position  */
   size_t op = 0; /* output position */

   if (src_len == 0)
      return 0;
   if (!src || (!dst && dst_len))
      return RLZ4_ERROR_PARAM;

   for (;;)
   {
      size_t lit_len, match_len, offset;
      uint8_t token;

      /* Input ending at a token boundary is the end of the block. */
      if (ip >= src_len)
         return (int64_t)op;
      token   = src[ip++];

      /* -- literals -- */
      lit_len = token >> 4;
      /* >= 15 declared literals imply >= 15 payload bytes after the
       * extension: its bytes may not reach the last 15 of input -
       * the reference's read_variable_length() margin, truncated
       * inputs included. */
      if (lit_len == 15 &&
            !rlz4_extend(src, src_len >= 15 ? src_len - 15 : 0, &ip, &lit_len))
         return RLZ4_ERROR_TRUNCATED;

      {
         /* Emit as many of the declared literals as input and output
          * allow.  Running out of OUTPUT is the partial-decode
          * contract; running out of INPUT mid-literals is how the
          * reference decoder treats a stream that simply stops - the
          * bytes so far are the answer.  A truncated STRUCTURE - a
          * length extension, an offset - is an error, matching it. */
         size_t avail    = src_len - ip;
         size_t emit     = lit_len;
         int    in_short = 0;
         if (emit > avail)
         {
            emit     = avail;
            in_short = 1;
         }
         if (emit > dst_len - op)
         {
            memcpy(dst + op, src + ip, dst_len - op);
            return (int64_t)dst_len;
         }
         memcpy(dst + op, src + ip, emit);
         op += emit;
         ip += emit;
         if (in_short)
            return (int64_t)op;
      }

      /* The final sequence is literals only, and the reference also
       * stops - successfully - when fewer than three bytes remain: an
       * offset cannot follow, so whatever is left cannot start a
       * sequence. */
      if (src_len - ip <= 2)
         return (int64_t)op;

      /* -- match -- */
      if (src_len - ip < 2)
         return RLZ4_ERROR_TRUNCATED;
      offset = (size_t)src[ip] | ((size_t)src[ip + 1] << 8);
      ip    += 2;
      if (offset == 0 || offset > op)
         return RLZ4_ERROR_OFFSET;

      match_len = (token & 15);
      /* after the extension, at least one token and the block's five
       * trailing literals must remain */
      if (match_len == 15 &&
            !rlz4_extend(src, src_len >= 6 ? src_len - 6 : 0, &ip, &match_len))
         return RLZ4_ERROR_TRUNCATED;
      match_len += 4; /* minimum match */

      if (match_len > dst_len - op)
         match_len = dst_len - op; /* partial stop inside the match */

      /* Byte-wise forward copy: offsets smaller than the length are the
       * RLE idiom (offset 1 repeats the last byte) and depend on the
       * copy reading bytes the same copy has just written. */
      {
         const uint8_t *from = dst + op - offset;
         uint8_t       *to   = dst + op;
         size_t         n    = match_len;
         while (n--)
            *to++ = *from++;
      }
      op += match_len;

      if (op == dst_len)
         return (int64_t)op;
   }
}
