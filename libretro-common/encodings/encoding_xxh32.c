/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (encoding_xxh32.c).
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
#if defined(_MSC_VER)
#include <stdlib.h>
#endif

#include <retro_inline.h>
#include <retro_endianness.h>
#include <encodings/xxh32.h>

#define XXH32_P1 0x9E3779B1U
#define XXH32_P2 0x85EBCA77U
#define XXH32_P3 0xC2B2AE3DU
#define XXH32_P4 0x27D4EB2FU
#define XXH32_P5 0x165667B1U

#if defined(_MSC_VER)
#define XXH32_ROTL(x, r) _rotl((x), (r))
#else
#define XXH32_ROTL(x, r) (((x) << (r)) | ((x) >> (32 - (r))))
#endif

#define XXH32_ROUND(acc, in) \
   ((acc) += (in) * XXH32_P2, (acc) = XXH32_ROTL((acc), 13), \
    (acc) *= XXH32_P1)

static INLINE uint32_t xxh32_rd32(const uint8_t *p)
{
   uint32_t v;
   memcpy(&v, p, 4);
   return swap_if_big32(v);
}

/* The whole stripes of @p up to @end, at least one, through the lanes
 * @v; returns where the stripes stop. */
static INLINE const uint8_t *xxh32_stripes(uint32_t *v, const uint8_t *p,
      const uint8_t *end)
{
   uint32_t       v1   = v[0];
   uint32_t       v2   = v[1];
   uint32_t       v3   = v[2];
   uint32_t       v4   = v[3];
   const uint8_t *stop = p + ((size_t)(end - p) & ~(size_t)15);

   do
   {
      XXH32_ROUND(v1, xxh32_rd32(p));
      XXH32_ROUND(v2, xxh32_rd32(p + 4));
      XXH32_ROUND(v3, xxh32_rd32(p + 8));
      XXH32_ROUND(v4, xxh32_rd32(p + 12));
      p += 16;
   } while (p != stop);
   v[0] = v1;
   v[1] = v2;
   v[2] = v3;
   v[3] = v4;
   return p;
}

static INLINE uint32_t xxh32_converge(const uint32_t *v)
{
   return XXH32_ROTL(v[0], 1)  + XXH32_ROTL(v[1], 7)
        + XXH32_ROTL(v[2], 12) + XXH32_ROTL(v[3], 18);
}

/* The last @len (under 16) octets at @p, and the avalanche: fours, then
 * single octets */
static INLINE uint32_t xxh32_finish(uint32_t h, const uint8_t *p, size_t len)
{
   for (; len >= 4; len -= 4, p += 4)
   {
      h += xxh32_rd32(p) * XXH32_P3;
      h  = XXH32_ROTL(h, 17) * XXH32_P4;
   }
   for (; len; len--)
   {
      h += (uint32_t)*p++ * XXH32_P5;
      h  = XXH32_ROTL(h, 11) * XXH32_P1;
   }
   h ^= h >> 15;
   h *= XXH32_P2;
   h ^= h >> 13;
   h *= XXH32_P3;
   h ^= h >> 16;
   return h;
}

static void xxh32_lanes(uint32_t *v, uint32_t seed)
{
   v[0] = seed + XXH32_P1 + XXH32_P2;
   v[1] = seed + XXH32_P2;
   v[2] = seed;
   v[3] = seed - XXH32_P1;
}

uint32_t xxh32(const void *data, size_t len, uint32_t seed)
{
   const uint8_t *p = (const uint8_t*)data;
   uint32_t       h;

   if (len >= 16)
   {
      uint32_t v[4];
      xxh32_lanes(v, seed);
      p = xxh32_stripes(v, p, p + len);
      h = xxh32_converge(v);
   }
   else
      h = seed + XXH32_P5;
   return xxh32_finish(h + (uint32_t)len, p, len & 15);
}

void xxh32_reset(xxh32_state_t *s, uint32_t seed)
{
   xxh32_lanes(s->v, seed);
   s->total   = 0;
   s->large   = 0;
   s->memsize = 0;
}

void xxh32_update(xxh32_state_t *s, const void *data, size_t len)
{
   const uint8_t *p   = (const uint8_t*)data;
   const uint8_t *end = p + len;

   if (!len)
      return;
   s->total += (uint32_t)len;
   s->large |= (len >= 16) | (s->total >= 16);

   /* not yet a stripe */
   if (s->memsize + len < 16)
   {
      memcpy(s->mem + s->memsize, p, len);
      s->memsize += (uint32_t)len;
      return;
   }

   /* the stripe begun before, made whole */
   if (s->memsize)
   {
      size_t fill = 16 - s->memsize;
      memcpy(s->mem + s->memsize, p, fill);
      xxh32_stripes(s->v, s->mem, s->mem + 16);
      p         += fill;
      s->memsize = 0;
   }

   if (end - p >= 16)
      p = xxh32_stripes(s->v, p, end);
   if (p != end)
   {
      memcpy(s->mem, p, (size_t)(end - p));
      s->memsize = (uint32_t)(end - p);
   }
}

uint32_t xxh32_digest(const xxh32_state_t *s)
{
   uint32_t h = s->large
      ? xxh32_converge(s->v)
      : s->v[2] + XXH32_P5;
   return xxh32_finish(h + s->total, s->mem, s->memsize);
}
