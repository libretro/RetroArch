/* Copyright (C) 2026 The RetroArch team. MIT license, see COPYING.MIT. */
#ifndef LIBRETRO_RWMA_INTERNAL_H
#define LIBRETRO_RWMA_INTERNAL_H
#include <float.h>
#include <formats/rwma.h>
#include <limits.h>
#include <math.h>
#include <retro_inline.h>
#include <stdlib.h>
#include <string.h>
#define RWMA_PI 3.14159265358979323846
#define RWMA_MIN(a, b) ((a) < (b) ? (a) : (b))
int rwma_std_create(void **out, const rwma_config *config);
void rwma_std_free(void *decoder);
void rwma_std_reset(void *decoder);
int rwma_std_decode(void *decoder, const uint8_t *data, size_t bytes, size_t *consumed,
                    rwma_frame *out);
typedef struct rwma_reader
{
   const uint8_t *p;
   size_t pos, end;
   int error;
} rwma_reader;
typedef struct rwma_book
{
   int32_t edge[2];
} rwma_book;
static INLINE unsigned rwma_read(rwma_reader *r, unsigned n)
{
   unsigned value = 0;
   if (n > 32 || r->pos > r->end || n > r->end - r->pos)
   {
      r->error = 1;
      return 0;
   }
   while (n)
   {
      unsigned k = RWMA_MIN(n, 8 - (unsigned)(r->pos & 7));
      value = (value << k) |
              ((r->p[r->pos / 8] >> (8 - (r->pos & 7) - k)) & ((1u << k) - 1));
      r->pos += k;
      n -= k;
   }
   return value;
}
static INLINE int rwma_symbol(rwma_reader *r, const rwma_book *book)
{
   unsigned depth;
   int32_t node = 0;
   for (depth = 0; depth < 32; depth++)
   {
      node = book[node].edge[rwma_read(r, 1)];
      if (r->error || !node)
         break;
      if (node < 0)
         return -1 - node;
   }
   r->error = 1;
   return -1;
}
static INLINE unsigned rwma_log2(unsigned value)
{
   unsigned bits = 0;
   while (value >>= 1)
      bits++;
   return bits;
}
typedef char rwma_binary32_float[(sizeof(float) == sizeof(uint32_t) &&
      FLT_RADIX == 2 && FLT_MANT_DIG == 24 && FLT_MAX_EXP == 128) ? 1 : -1];
static INLINE int rwma_finite(float v)
{
   uint32_t bits;
   memcpy(&bits, &v, sizeof(bits));
   return (bits & 0x7f800000u) != 0x7f800000u;
}
static INLINE int rwma_append(uint8_t *dst, size_t *end, size_t capacity,
                              rwma_reader *r, size_t count)
{
   if (*end > capacity || count > capacity - *end || r->pos > r->end ||
       count > r->end - r->pos)
      return RWMA_BAD_DATA;
   while (count)
   {
      unsigned bit = (unsigned)(*end & 7);
      if (!bit && !(r->pos & 7) && count >= 8)
      {
         size_t bytes = count / 8;
         memcpy(dst + *end / 8, r->p + r->pos / 8, bytes);
         *end += bytes * 8;
         r->pos += bytes * 8;
         count -= bytes * 8;
      }
      else
      {
         unsigned n = (unsigned)RWMA_MIN(count, 8 - bit), shift = 8 - bit - n;
         unsigned mask = ((1u << n) - 1) << shift;
         unsigned value = rwma_read(r, n) << shift;
         dst[*end / 8] = (uint8_t)((dst[*end / 8] & ~mask) | value);
         *end += n;
         count -= n;
      }
   }
   return r->error ? RWMA_BAD_DATA : RWMA_OK;
}
#endif
