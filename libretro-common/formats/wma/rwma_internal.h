/*
 * WMA compatible codec
 * Copyright (c) 2002-2007 The FFmpeg Project
 *
 * Adapted from FFmpeg for libretro-common; no libavcodec dependency.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/* LGPL-2.1-or-later. Private bounded bit reader and scalar DSP. */
#ifndef LIBRETRO_RWMA_INTERNAL_H
#define LIBRETRO_RWMA_INTERNAL_H
#include <assert.h>
#include <float.h>
#include <formats/rwma.h>
#include <limits.h>
#include <math.h>
#include <retro_inline.h>
#include <stdlib.h>
#include <string.h>
#define RWMA_PI 3.14159265358979323846
#define RWMA_MIN(a, b) ((a) < (b) ? (a) : (b))
#define RWMA_MAX(a, b) ((a) > (b) ? (a) : (b))
#define RWMA_ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
static INLINE int rwma_frame_valid(const rwma_frame *frame)
{
   unsigned c, i;
   for (c = 0; c < frame->channels; c++)
      for (i = 0; i < frame->samples; i++)
         if (!(frame->planes[c][i] <= FLT_MAX && frame->planes[c][i] >= -FLT_MAX))
            return 0;
   return 1;
}
#define WMAPRO_MAX_CHANNELS 8
#define MAX_SUBFRAMES 32
#define MAX_BANDS 29
#define WMAPRO_BLOCK_MIN_BITS 6
#define WMAPRO_BLOCK_MAX_BITS 13
#define WMAPRO_BLOCK_SIZES 8
#define WMAPRO_BLOCK_MAX_SIZE 8192
#define WMAPRO_BLOCK_MIN_SIZE 64
#define MAX_FRAMESIZE 32768
typedef struct rwma_packet
{
   const uint8_t *data;
   int size;
} rwma_packet;
typedef struct rwma_bits
{
   const uint8_t *data;
   int size, pos, error;
} rwma_bits;
typedef struct rwma_vlc_node
{
   int16_t next[2];
   int16_t symbol;
} rwma_vlc_node;
static INLINE unsigned get_bits(rwma_bits *b, int n)
{
   unsigned value = 0;
   if (n < 0 || n > 32 || b->pos > b->size - n)
   {
      b->error = 1;
      return 0;
   }
   while (n)
   {
      unsigned take = RWMA_MIN(n, 8 - (b->pos & 7));
      value = (value << take) | ((b->data[b->pos >> 3] >> (8 - (b->pos & 7) - take)) &
                                 ((1u << take) - 1));
      b->pos += take;
      n -= take;
   }
   return value;
}
static INLINE unsigned get_bits1(rwma_bits *b) { return get_bits(b, 1); }
static INLINE int get_sbits(rwma_bits *b, int n)
{
   unsigned v = get_bits(b, n), sign = 1u << (n - 1);
   return (int)(v ^ sign) - (int)sign;
}
static INLINE unsigned get_bitsz(rwma_bits *b, int n) { return get_bits(b, n); }
static INLINE int get_bits_count(const rwma_bits *b) { return b->pos; }
static INLINE int get_bits_left(const rwma_bits *b) { return b->size - b->pos; }
static INLINE int init_get_bits(rwma_bits *b, const uint8_t *data, int bits)
{
   b->data = data;
   b->size = bits;
   b->pos = b->error = 0;
   return bits < 0 ? RWMA_BAD_DATA : RWMA_OK;
}
static INLINE int init_get_bits8(rwma_bits *b, const uint8_t *data, int bytes)
{
   if (bytes < 0 || bytes > INT_MAX / 8)
      return RWMA_BAD_DATA;
   return init_get_bits(b, data, bytes * 8);
}
static INLINE unsigned show_bits(rwma_bits *b, int n)
{
   int pos = b->pos;
   unsigned value = get_bits(b, n);
   b->pos = pos;
   return value;
}
static INLINE void skip_bits(rwma_bits *b, int n)
{
   if (n < 0 || b->pos > b->size - n)
      b->error = 1;
   else
      b->pos += n;
}
#define skip_bits_long skip_bits
static INLINE int rwma_vlc(rwma_bits *b, const rwma_vlc_node *table)
{
   int node = 0, depth;
   for (depth = 0; depth < 32; depth++)
   {
      if (table[node].next[0] < 0 && table[node].next[1] < 0)
         return table[node].symbol;
      node = table[node].next[get_bits1(b)];
      if (b->error || node < 0)
         break;
   }
   b->error = 1;
   return 0;
}
static INLINE int rwma_log2(unsigned x)
{
   int n = 0;
   while (x >>= 1)
      n++;
   return n;
}
static INLINE uint32_t rwma_float_bits(float value)
{
   uint32_t bits;
   memcpy(&bits, &value, sizeof(bits));
   return bits;
}
static INLINE void rwma_store_float(float *dst, uint32_t bits)
{
   memcpy(dst, &bits, sizeof(bits));
}
static INLINE float rwma_exp10(double x) { return (float)pow(10.0, x); }
static INLINE unsigned rwma_large_value(rwma_bits *b)
{
   int n = 8;
   if (get_bits1(b))
   {
      n += 8;
      if (get_bits1(b))
      {
         n += 8;
         if (get_bits1(b))
            n += 7;
      }
   }
   return get_bits(b, n);
}
static INLINE int rwma_run_level(rwma_bits *b, const rwma_vlc_node *vlc,
                                 const float *levels, const uint16_t *runs, int version,
                                 float *dst, int offset, int count, int block,
                                 int escape_bits, int coefficient_bits)
{
   int code, sign;
   unsigned level;
   for (; offset < count; offset++)
   {
      code = rwma_vlc(b, vlc);
      if (b->error || code < 0)
         return RWMA_BAD_DATA;
      if (code == 1)
         break;
      if (code > 1)
      {
         offset += runs[code];
         sign = (int)get_bits1(b) - 1;
         if (offset >= count)
            return RWMA_BAD_DATA;
         rwma_store_float(dst + offset, rwma_float_bits(levels[code]) ^
                                            ((uint32_t)sign & 0x80000000u));
      }
      else
      {
         level = version ? rwma_large_value(b) : get_bits(b, coefficient_bits);
         if (!version)
            offset += get_bits(b, escape_bits);
         else if (get_bits1(b))
         {
            if (get_bits1(b))
            {
               if (get_bits1(b))
                  return RWMA_BAD_DATA;
               offset += get_bits(b, escape_bits) + 4;
            }
            else
               offset += get_bits(b, 2) + 1;
         }
         sign = (int)get_bits1(b) - 1;
         if (offset >= count)
            return RWMA_BAD_DATA;
         dst[offset] = sign ? -(float)level : (float)level;
      }
      if (b->error)
         return RWMA_BAD_DATA;
   }
   (void)block;
   return RWMA_OK;
}
int rwma_standard_create(void **out, const rwma_config *cfg);
void rwma_standard_free(void *);
void rwma_standard_reset(void *);
int rwma_standard_decode(void *, const uint8_t *, size_t, size_t *, rwma_frame *);
static INLINE void rwma_multiply(float *dst, const float *src, float scale, int n)
{
   int i;
   for (i = 0; i < n; i++)
      dst[i] = src[i] * scale;
}
static INLINE void rwma_window(float *dst, const float *src0, const float *src1,
                               const float *window, int n)
{
   int i, j;
   for (i = 0, j = n - 1; i < n; i++, j--)
   {
      float a = src0[i], b = src1[j];
      float wi = window[i], wj = window[2 * n - 1 - i];
      dst[i] = a * wj - b * wi;
      dst[2 * n - 1 - i] = a * wi + b * wj;
   }
}
#endif
