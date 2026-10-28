/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (encoding_xxh64.c).
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
#include <encodings/xxh64.h>

#define XXH64_P1 0x9E3779B185EBCA87ULL
#define XXH64_P2 0xC2B2AE3D27D4EB4FULL
#define XXH64_P3 0x165667B19E3779F9ULL
#define XXH64_P4 0x85EBCA77C2B2AE63ULL
#define XXH64_P5 0x27D4EB2F165667C5ULL

#if defined(_MSC_VER)
#define XXH64_ROTL(x, r) _rotl64((x), (r))
#else
#define XXH64_ROTL(x, r) (((x) << (r)) | ((x) >> (64 - (r))))
#endif

/* One lane's step: the octets in, multiplied, turned and multiplied */
#define XXH64_ROUND(acc, in) \
   ((acc) += (in) * XXH64_P2, (acc) = XXH64_ROTL((acc), 31), \
    (acc) *= XXH64_P1)

static INLINE uint64_t xxh64_rd64(const uint8_t *p)
{
   uint64_t v;
   memcpy(&v, p, 8);
   return swap_if_big64(v);
}

static INLINE uint32_t xxh64_rd32(const uint8_t *p)
{
   uint32_t v;
   memcpy(&v, p, 4);
   return swap_if_big32(v);
}

/* The whole stripes of @p up to @end, at least one, through the lanes
 * @v, which are held in registers for it; returns where the stripes
 * stop.
 *
 * The lanes are chains of multiplies, bound by the multiplier however
 * the loop is unrolled, so it takes a stripe a turn; what it saves is
 * the branch a varying length mispredicts: one stripe is taken without
 * the loop. */
static INLINE const uint8_t *xxh64_stripes(uint64_t *v, const uint8_t *p,
      const uint8_t *end)
{
   uint64_t       v1   = v[0];
   uint64_t       v2   = v[1];
   uint64_t       v3   = v[2];
   uint64_t       v4   = v[3];
   size_t         n    = (size_t)(end - p);
   const uint8_t *stop = p + (n & ~(size_t)31);

   if (n < 64)
   {
      XXH64_ROUND(v1, xxh64_rd64(p));
      XXH64_ROUND(v2, xxh64_rd64(p + 8));
      XXH64_ROUND(v3, xxh64_rd64(p + 16));
      XXH64_ROUND(v4, xxh64_rd64(p + 24));
      p += 32;
   }
   else
      do
      {
         XXH64_ROUND(v1, xxh64_rd64(p));
         XXH64_ROUND(v2, xxh64_rd64(p + 8));
         XXH64_ROUND(v3, xxh64_rd64(p + 16));
         XXH64_ROUND(v4, xxh64_rd64(p + 24));
         p += 32;
      } while (p != stop);
   v[0] = v1;
   v[1] = v2;
   v[2] = v3;
   v[3] = v4;
   return p;
}

static INLINE uint64_t xxh64_merge(uint64_t h, uint64_t v)
{
   uint64_t k = 0;
   XXH64_ROUND(k, v);
   h ^= k;
   return h * XXH64_P1 + XXH64_P4;
}

/* The four lanes folded into one */
static INLINE uint64_t xxh64_converge(const uint64_t *v)
{
   uint64_t h =  XXH64_ROTL(v[0], 1)  + XXH64_ROTL(v[1], 7)
               + XXH64_ROTL(v[2], 12) + XXH64_ROTL(v[3], 18);
   h = xxh64_merge(h, v[0]);
   h = xxh64_merge(h, v[1]);
   h = xxh64_merge(h, v[2]);
   return xxh64_merge(h, v[3]);
}

#define XXH64_TAKE8(h, p) do { \
   uint64_t k_ = 0; \
   XXH64_ROUND(k_, xxh64_rd64(p)); \
   (h) ^= k_; \
   (h)  = XXH64_ROTL((h), 27) * XXH64_P1 + XXH64_P4; \
} while (0)

#define XXH64_TAKE1(h, c) do { \
   (h) ^= (uint64_t)(c) * XXH64_P5; \
   (h)  = XXH64_ROTL((h), 11) * XXH64_P1; \
} while (0)

/* The last @len (under 32) octets at @p, and the avalanche: eights,
 * then a four, then single octets */
static INLINE uint64_t xxh64_finish(uint64_t h, const uint8_t *p, size_t len)
{
   for (; len >= 8; len -= 8, p += 8)
      XXH64_TAKE8(h, p);
   if (len >= 4)
   {
      h ^= (uint64_t)xxh64_rd32(p) * XXH64_P1;
      h  = XXH64_ROTL(h, 23) * XXH64_P2 + XXH64_P3;
      p   += 4;
      len -= 4;
   }
   for (; len; len--)
      XXH64_TAKE1(h, *p++);
   h ^= h >> 33;
   h *= XXH64_P2;
   h ^= h >> 29;
   h *= XXH64_P3;
   h ^= h >> 32;
   return h;
}

static void xxh64_lanes(uint64_t *v, uint64_t seed)
{
   v[0] = seed + XXH64_P1 + XXH64_P2;
   v[1] = seed + XXH64_P2;
   v[2] = seed;
   v[3] = seed - XXH64_P1;
}

uint64_t xxh64(const void *data, size_t len, uint64_t seed)
{
   const uint8_t *p = (const uint8_t*)data;
   uint64_t       h;

   if (len >= 32)
   {
      uint64_t v[4];
      xxh64_lanes(v, seed);
      p = xxh64_stripes(v, p, p + len);
      h = xxh64_converge(v);
   }
   else
      h = seed + XXH64_P5;
   return xxh64_finish(h + (uint64_t)len, p, len & 31);
}

void xxh64_reset(xxh64_state_t *s, uint64_t seed)
{
   xxh64_lanes(s->v, seed);
   s->total   = 0;
   s->memsize = 0;
}

void xxh64_update(xxh64_state_t *s, const void *data, size_t len)
{
   const uint8_t *p   = (const uint8_t*)data;
   const uint8_t *end = p + len;

   if (!len)
      return;
   s->total += len;

   /* not yet a stripe */
   if (s->memsize + len < 32)
   {
      memcpy(s->mem + s->memsize, p, len);
      s->memsize += (uint32_t)len;
      return;
   }

   /* the stripe begun before, made whole */
   if (s->memsize)
   {
      size_t fill = 32 - s->memsize;
      memcpy(s->mem + s->memsize, p, fill);
      xxh64_stripes(s->v, s->mem, s->mem + 32);
      p         += fill;
      s->memsize = 0;
   }

   if (end - p >= 32)
      p = xxh64_stripes(s->v, p, end);
   if (p != end)
   {
      memcpy(s->mem, p, (size_t)(end - p));
      s->memsize = (uint32_t)(end - p);
   }
}

uint64_t xxh64_digest(const xxh64_state_t *s)
{
   uint64_t h = s->total >= 32
      ? xxh64_converge(s->v)
      : s->v[2] + XXH64_P5;
   return xxh64_finish(h + s->total, s->mem, s->memsize);
}
