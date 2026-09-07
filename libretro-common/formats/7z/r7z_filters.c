/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (r7z_filters.c).
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

/* Branch converters and delta decoding for 7z archive members, derived
 * from the public-domain LZMA SDK by Igor Pavlov. See rfilters.h.
 *
 * Byte order is handled explicitly throughout: the ARM and x86 filters
 * read little-endian operands, PPC and SPARC big-endian, regardless of
 * the host. Nothing here dereferences a multi-byte type through a
 * cast, so there are no alignment or aliasing constraints on the
 * caller's buffer.
 */

#include <string.h>

#include <7z/r7z_filters.h>

/* --------------------------------------------------------------------
 * Byte access helpers
 * -------------------------------------------------------------------- */

static uint32_t rf_get_le32(const uint8_t *p)
{
   return  (uint32_t)p[0]
        | ((uint32_t)p[1] << 8)
        | ((uint32_t)p[2] << 16)
        | ((uint32_t)p[3] << 24);
}

static void rf_set_le32(uint8_t *p, uint32_t v)
{
   p[0] = (uint8_t)v;
   p[1] = (uint8_t)(v >> 8);
   p[2] = (uint8_t)(v >> 16);
   p[3] = (uint8_t)(v >> 24);
}

static uint32_t rf_get_be32(const uint8_t *p)
{
   return ((uint32_t)p[0] << 24)
        | ((uint32_t)p[1] << 16)
        | ((uint32_t)p[2] << 8)
        |  (uint32_t)p[3];
}

static void rf_set_be32(uint8_t *p, uint32_t v)
{
   p[0] = (uint8_t)(v >> 24);
   p[1] = (uint8_t)(v >> 16);
   p[2] = (uint8_t)(v >> 8);
   p[3] = (uint8_t)v;
}

/* --------------------------------------------------------------------
 * Delta
 * -------------------------------------------------------------------- */

void rfilters_delta_decode(uint8_t *state, unsigned distance,
      uint8_t *data, size_t len)
{
   uint8_t  buf[RFILTERS_DELTA_STATE_SIZE];
   unsigned j = 0;
   size_t   i;

   if (distance == 0 || distance > RFILTERS_DELTA_STATE_SIZE)
      return;

   memcpy(buf, state, distance);

   for (i = 0; i < len;)
   {
      for (j = 0; j < distance && i < len; i++, j++)
      {
         buf[j]  = (uint8_t)(buf[j] + data[i]);
         data[i] = buf[j];
      }
   }

   /* Rotate the history so the next call resumes at the right phase. */
   if (j == distance)
      j = 0;
   memcpy(state, buf + j, distance - j);
   memcpy(state + distance - j, buf, j);
}

/* --------------------------------------------------------------------
 * x86 (BCJ)
 *
 * The only converter with running state: a CALL/JMP opcode can be
 * split across a chunk boundary, and the mask records how far into
 * such a sequence the previous call ended.
 * -------------------------------------------------------------------- */

#define RF_TEST_86_MS_BYTE(b) ((((b) + 1) & 0xFE) == 0)

size_t rfilters_x86_decode(uint32_t *state, uint32_t ip,
      uint8_t *data, size_t len)
{
   size_t   pos  = 0;
   uint32_t mask = *state & 7;

   if (len < 5)
      return 0;
   len -= 4;
   ip  += 5;

   for (;;)
   {
      uint8_t       *p     = data + pos;
      const uint8_t *limit = data + len;

      while (p < limit && (*p & 0xFE) != 0xE8)
         p++;

      {
         size_t d = (size_t)(p - data) - pos;
         pos      = (size_t)(p - data);

         if (p >= limit)
         {
            *state = (d > 2 ? 0 : mask >> (unsigned)d);
            return pos;
         }

         if (d > 2)
            mask = 0;
         else
         {
            mask >>= (unsigned)d;
            if (     mask != 0
                  && (   mask > 4
                      || mask == 3
                      || RF_TEST_86_MS_BYTE(p[(size_t)(mask >> 1) + 1])))
            {
               mask = (mask >> 1) | 4;
               pos++;
               continue;
            }
         }
      }

      if (RF_TEST_86_MS_BYTE(p[4]))
      {
         uint32_t v   = ((uint32_t)p[4] << 24)
                      | ((uint32_t)p[3] << 16)
                      | ((uint32_t)p[2] << 8)
                      |  (uint32_t)p[1];
         uint32_t cur = ip + (uint32_t)pos;

         pos += 5;
         v   -= cur;

         if (mask != 0)
         {
            unsigned sh = (mask & 6) << 2;
            if (RF_TEST_86_MS_BYTE((uint8_t)(v >> sh)))
            {
               v ^= (((uint32_t)0x100 << sh) - 1);
               v -= cur;
            }
            mask = 0;
         }

         p[1] = (uint8_t)v;
         p[2] = (uint8_t)(v >> 8);
         p[3] = (uint8_t)(v >> 16);
         p[4] = (uint8_t)(0 - ((v >> 24) & 1));
      }
      else
      {
         mask = (mask >> 1) | 4;
         pos++;
      }
   }
}

/* --------------------------------------------------------------------
 * ARM / ARM Thumb
 * -------------------------------------------------------------------- */

size_t rfilters_arm_decode(uint32_t ip, uint8_t *data, size_t len)
{
   uint8_t       *p;
   const uint8_t *lim;

   len &= ~(size_t)3;
   ip  += 4;
   p    = data;
   lim  = data + len;

   for (;;)
   {
      /* BL is encoded with 0xEB in the top byte. */
      for (;;)
      {
         if (p >= lim)
            return (size_t)(p - data);
         p += 4;
         if (p[-1] == 0xEB)
            break;
      }
      {
         uint32_t v = rf_get_le32(p - 4);
         v <<= 2;
         v  -= ip + (uint32_t)(p - data);
         v >>= 2;
         v  &= 0x00FFFFFF;
         v  |= 0xEB000000;
         rf_set_le32(p - 4, v);
      }
   }
}

size_t rfilters_armt_decode(uint32_t ip, uint8_t *data, size_t len)
{
   uint8_t       *p;
   const uint8_t *lim;

   len &= ~(size_t)1;
   if (len < 4)
      return 0;
   p   = data;
   lim = data + len - 4;

   for (;;)
   {
      uint32_t b1;

      /* A Thumb BL is a pair of halfwords, both with the top five bits
       * set in the pattern tested here. */
      for (;;)
      {
         uint32_t b3;
         if (p > lim)
            return (size_t)(p - data);
         b1  = p[1];
         b3  = p[3];
         p  += 2;
         b1 ^= 8;
         if ((b3 & b1) >= 0xF8)
            break;
      }
      {
         uint32_t v = ((uint32_t)b1 << 19)
                    + (((uint32_t)p[1] & 0x7) << 8)
                    + ((uint32_t)p[-2] << 11)
                    + (uint32_t)p[0];

         p += 2;
         v -= (ip + (uint32_t)(p - data)) >> 1;

         p[-4] = (uint8_t)(v >> 11);
         p[-3] = (uint8_t)(0xF0 | ((v >> 19) & 0x7));
         p[-2] = (uint8_t)v;
         p[-1] = (uint8_t)(0xF8 | (v >> 8));
      }
   }
}

/* --------------------------------------------------------------------
 * PowerPC / SPARC
 * -------------------------------------------------------------------- */

size_t rfilters_ppc_decode(uint32_t ip, uint8_t *data, size_t len)
{
   uint8_t       *p;
   const uint8_t *lim;

   len &= ~(size_t)3;
   ip  -= 4;
   p    = data;
   lim  = data + len;

   for (;;)
   {
      /* (v & 0xFC000003) == 0x48000001: branch-and-link, absolute off. */
      for (;;)
      {
         if (p >= lim)
            return (size_t)(p - data);
         p += 4;
         if ((p[-4] & 0xFC) == 0x48 && (p[-1] & 3) == 1)
            break;
      }
      {
         uint32_t v = rf_get_be32(p - 4);
         v -= ip + (uint32_t)(p - data);
         v &= 0x03FFFFFF;
         v |= 0x48000000;
         rf_set_be32(p - 4, v);
      }
   }
}

size_t rfilters_sparc_decode(uint32_t ip, uint8_t *data, size_t len)
{
   uint8_t       *p;
   const uint8_t *lim;

   len &= ~(size_t)3;
   ip  -= 4;
   p    = data;
   lim  = data + len;

   for (;;)
   {
      for (;;)
      {
         if (p >= lim)
            return (size_t)(p - data);
         p += 4;
         if (   (p[-4] == 0x40 && (p[-3] & 0xC0) == 0)
             || (p[-4] == 0x7F &&  p[-3] >= 0xC0))
            break;
      }
      {
         uint32_t v = rf_get_be32(p - 4);
         v <<= 2;
         v  -= ip + (uint32_t)(p - data);
         v  &= 0x01FFFFFF;
         v  -= (uint32_t)1 << 24;
         v  ^= 0xFF000000;
         v >>= 2;
         v  |= 0x40000000;
         rf_set_be32(p - 4, v);
      }
   }
}

/* --------------------------------------------------------------------
 * IA-64
 *
 * Instructions come in 16-byte bundles holding three 41-bit slots; the
 * template in the low byte says which slots are branches.
 * -------------------------------------------------------------------- */

size_t rfilters_ia64_decode(uint32_t ip, uint8_t *data, size_t len)
{
   size_t i;

   if (len < 16)
      return 0;
   len -= 16;
   i    = 0;

   do
   {
      unsigned m = ((uint32_t)0x334B0000 >> (data[i] & 0x1E)) & 3;

      if (m)
      {
         m++;
         do
         {
            uint8_t *p = data + (i + (size_t)m * 5 - 8);

            if (      ((p[3] >> m) & 15) == 5
                  && (((uint32_t)p[-1] | ((uint32_t)p[0] << 8)) >> m & 0x70) == 0)
            {
               uint32_t raw = rf_get_le32(p);
               uint32_t v   = raw >> m;

               v   = (v & 0xFFFFF) | ((v & (1 << 23)) >> 3);
               v <<= 4;
               v  -= ip + (uint32_t)i;
               v >>= 4;
               v  &= 0x1FFFFF;
               v  += 0x700000;
               v  &= 0x8FFFFF;

               raw &= ~((uint32_t)0x8FFFFF << m);
               raw |= (v << m);
               rf_set_le32(p, raw);
            }
         } while (++m <= 4);
      }
      i += 16;
   } while (i <= len);

   return i;
}
