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

/* Blind match copy for short-to-medium lengths, all offsets, one
 * branch: the reference decoder's increment tables re-seat the source
 * pointer so that after the first eight bytes it trails the write
 * cursor by at least eight regardless of offset, and everything after
 * is plain 8-byte strides.  Writes overshoot to the next multiple of
 * eight; the caller guarantees the room. */
static void rlz4_copy_match_blind(uint8_t *to, const uint8_t *from,
      size_t match_len, size_t offset)
{
   /* offset 1 would overlap the two-byte base copy (the reference
    * asserts it away and pre-filters upstream); one splat is both
    * legal and the fastest thing for it. */
   if (offset == 1)
   {
      memset(to, from[0], match_len);
      return;
   }

   static const unsigned inc32[8] = { 0, 1, 2,  1,  0, 4, 4, 4 };
   static const int      dec64[8] = { 0, 0, 0, -1, -4, 1, 2, 3 };
   uint8_t *end = to + match_len;
   if (offset < 8)
   {
      memcpy(to,     from,     2);
      memcpy(to + 2, from + 2, 2);
      from += inc32[offset];
      memcpy(to + 4, from, 4);
      from -= dec64[offset];
   }
   else
      memcpy(to, from, 8), from += 8;
   to += 8;
   while (to < end)
   {
      memcpy(to, from, 8);
      to += 8; from += 8;
   }
}

/* Exact match copy with no blind writes: used wherever margins are
 * not statically guaranteed.  match_len must already be clamped to
 * the output capacity. */
static void rlz4_copy_match_exact(uint8_t *dst, size_t dst_len,
      size_t op, size_t offset, size_t match_len)
{
   uint8_t *to = dst + op;
   if (offset >= 32 && dst_len - op >= match_len + 31)
   {
      const uint8_t *from = dst + op - offset;
      uint8_t       *end  = to + match_len;
      do
      {
         memcpy(to,      from,      16);
         memcpy(to + 16, from + 16, 16);
         to += 32; from += 32;
      } while (to < end);
   }
   else if (offset >= 16 && dst_len - op >= match_len + 15)
   {
      const uint8_t *from = dst + op - offset;
      uint8_t       *end  = to + match_len;
      do
      {
         memcpy(to, from, 16);
         to += 16; from += 16;
      } while (to < end);
   }
   else if (offset >= 8 && dst_len - op >= match_len + 15)
   {
      const uint8_t *from = dst + op - offset;
      uint8_t       *end  = to + match_len;
      do
      {
         memcpy(to, from, 8);
         to += 8; from += 8;
      } while (to < end);
   }
   else
   {
      size_t have = offset < match_len ? offset : match_len;
      memcpy(to, dst + op - offset, have);
      if (offset >= match_len)
      {
         if (match_len > have)
            memcpy(to + have, dst + op - offset + have, match_len - have);
      }
      else
      {
         while (have * 2 <= match_len)
         {
            memcpy(to + have, to, have);
            have *= 2;
         }
         if (have < match_len)
            memcpy(to + have, to, match_len - have);
      }
   }
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

   /* Fast loop: entered per-iteration only with at least 32 bytes of
    * input and output margin, which makes the token read, a blind
    * 16-byte short-literal copy, and the offset read unconditionally
    * safe (reads and scratch writes land inside the caller's buffers;
    * only the returned length is contractual).  Long literals take the
    * exact path inline - with the reference's stop rules - and the
    * match section guards every step individually, so a sequence never
    * straddles loop boundaries.  The moment either margin is gone the
    * careful tail loop below owns the block, including every
    * truncation rule. */
   if (src_len >= 64 && dst_len >= 64)
   {
      /* The reference's fast-loop safe distance: with 64 bytes of
       * margin, a 16-byte literal stripe, the offset read, and a
       * short match's blind copy (at most 24 written) are all
       * unconditionally in bounds even after the stripe advances the
       * cursors. */
      const size_t in_fast  = src_len - 64;
      const size_t out_fast = dst_len - 64;

      while (ip < in_fast && op < out_fast)
      {
         size_t lit_len, match_len, offset;
         uint8_t token = src[ip++];

         /* -- literals -- */
         lit_len = token >> 4;
         if (lit_len < 15)
         {
            memcpy(dst + op, src + ip, 16); /* blind stripe, <= 14 used */
            op += lit_len;
            ip += lit_len;
         }
         else
         {
            /* Long literals: exact copy with the reference's stop
             * rules, then this sequence's match runs on the exact
             * path - the stripe guarantees are gone. */
            if (!rlz4_extend(src, src_len - 15, &ip, &lit_len))
               return RLZ4_ERROR_TRUNCATED;
            {
               size_t avail = src_len - ip;
               size_t emit  = lit_len;
               int in_short = 0;
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
            if (src_len - ip <= 2)
               return (int64_t)op;

            {
               uint16_t off16;
               memcpy(&off16, src + ip, 2);
#ifdef MSB_FIRST
               offset = (size_t)((off16 >> 8) | (off16 << 8)) & 0xffff;
#else
               offset = off16;
#endif
            }
            ip += 2;
            if (offset - 1 >= op) /* 0 wraps; one compare */
               return RLZ4_ERROR_OFFSET;

            match_len = token & 15;
            if (match_len == 15 &&
                  !rlz4_extend(src, src_len >= 6 ? src_len - 6 : 0,
                        &ip, &match_len))
               return RLZ4_ERROR_TRUNCATED;
            match_len += 4;
            if (match_len > dst_len - op)
               match_len = dst_len - op;
            rlz4_copy_match_exact(dst, dst_len, op, offset, match_len);
            op += match_len;
            if (op == dst_len)
               return (int64_t)op;
            continue;
         }

         /* -- offset (margins hold: at most 16 + 2 consumed) -- */
         {
            uint16_t off16;
            memcpy(&off16, src + ip, 2);
#ifdef MSB_FIRST
            offset = (size_t)((off16 >> 8) | (off16 << 8)) & 0xffff;
#else
            offset = off16;
#endif
         }
         ip += 2;
         if (offset - 1 >= op) /* catches 0 via wrap and > op in one compare */
            return RLZ4_ERROR_OFFSET;

         /* -- match -- */
         match_len = token & 15;
         if (match_len != 15)
         {
            match_len += 4; /* 4..18 */
            if (offset >= 8)
            {
               /* Reference short-match exit: flat 18, no length
                * branch. */
               uint8_t       *to   = dst + op;
               const uint8_t *from = dst + op - offset;
               memcpy(to,      from,      8);
               memcpy(to + 8,  from + 8,  8);
               memcpy(to + 16, from + 16, 2);
            }
            else
               rlz4_copy_match_blind(dst + op, dst + op - offset,
                     match_len, offset);
            op += match_len;
            continue;
         }

         if (!rlz4_extend(src, src_len >= 6 ? src_len - 6 : 0, &ip, &match_len))
            return RLZ4_ERROR_TRUNCATED;
         match_len += 4;

         if (offset >= 32 && match_len + 31 <= dst_len - op)
         {
            /* Single-compare fast path for the long matches that
             * carry most of the bytes on match-dense data. */
            uint8_t       *to   = dst + op;
            const uint8_t *from = dst + op - offset;
            uint8_t       *end  = to + match_len;
            do
            {
               memcpy(to,      from,      16);
               memcpy(to + 16, from + 16, 16);
               to += 32; from += 32;
            } while (to < end);
            op += match_len;
            continue;
         }

         if (match_len > dst_len - op)
            match_len = dst_len - op;
         rlz4_copy_match_exact(dst, dst_len, op, offset, match_len);
         op += match_len;
         if (op == dst_len)
            return (int64_t)op;
      }
   }

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

      if (lit_len && lit_len <= 16 &&
            lit_len <= src_len - ip && lit_len <= dst_len - op)
      {
         /* Hot tier: short literal runs between matches dominate
          * match-dense data, where copy-routine setup costs more than
          * the bytes. */
         const uint8_t *from = src + ip;
         uint8_t       *to   = dst + op;
         size_t         n    = lit_len;
         while (n--)
            *to++ = *from++;
         op += lit_len;
         ip += lit_len;
      }
      else
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
      if (offset - 1 >= op) /* 0 wraps; one compare */
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

      /* Match copy.  Offsets smaller than the length are the RLE idiom
       * (offset 1 repeats the last byte): semantically a byte-wise
       * forward copy that reads bytes it has just written.  Executed
       * as region doubling for short offsets - each memcpy source is
       * the fully-written prefix, so nothing overlaps - and as plain
       * chunked copies once the gap is at least eight bytes.  Every
       * write stays inside [op, op + match_len): unlike the reference
       * fast path there is no wildcopy overrun, because the caller's
       * buffer is exactly the frame it asked for. */
      {
         uint8_t *to = dst + op;
         if (offset >= 8 && dst_len - op >= match_len + 15)
         {
            /* The common case: enough room after the match that an
             * 8-byte-stride copy may overshoot by up to seven bytes.
             * The scratch lands inside the caller's buffer and is
             * overwritten by whatever the block produces next; only
             * the returned length is contractual, exactly as with the
             * reference wildcopy.  Reads trail writes by >= 8, so no
             * iteration reads its own output. */
            const uint8_t *from = dst + op - offset;
            uint8_t       *end  = to + match_len;
            do
            {
               memcpy(to, from, 8);
               to += 8; from += 8;
            } while (to < end);
         }
         else if (offset >= 8 || offset >= match_len)
         {
            /* Near the end of the buffer: exact chunks, no overshoot. */
            const uint8_t *from = dst + op - offset;
            size_t n = match_len;
            while (n >= 8)
            {
               memcpy(to, from, 8);
               to += 8; from += 8; n -= 8;
            }
            while (n--)
               *to++ = *from++;
         }
         else if (match_len <= 16)
         {
            /* Short overlapped match: a byte loop beats copy setup. */
            const uint8_t *from = dst + op - offset;
            size_t n = match_len;
            while (n--)
               *to++ = *from++;
         }
         else
         {
            /* Long short-offset match (the RLE idiom): seed one
             * period, then double the written region - every memcpy
             * source is the fully-written prefix, so nothing
             * overlaps. */
            size_t have = offset;
            memcpy(to, dst + op - offset, offset);
            while (have * 2 <= match_len)
            {
               memcpy(to + have, to, have);
               have *= 2;
            }
            if (have < match_len)
               memcpy(to + have, to, match_len - have);
         }
      }
      op += match_len;

      if (op == dst_len)
         return (int64_t)op;
   }
}
