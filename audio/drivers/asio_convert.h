/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2026 - libretro contributors
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

/* Sample conversion for the ASIO driver: interleaved float stereo to
 * two hardware halves in the device's ASIOSampleType. Pure C with no
 * platform dependency, so it is included by audio/drivers/asio.c and
 * by samples/audio/asio_convert, which runs it on any host against
 * fixtures the specification fixes.
 *
 * Types and their storage, from the ASIO 2.3 specification:
 *
 *   Int16LSB / MSB          16-bit two's complement, little / big endian
 *   Int24LSB / MSB          24-bit packed in 3 bytes, little / big endian
 *   Int32LSB / MSB          32-bit two's complement
 *   Int32LSB16..24 / MSB..  32-bit word with 16, 18, 20 or 24 significant
 *                           bits aligned to the least significant bit -
 *                           the value in the low bits, sign extended
 *                           above them - stored little or big endian.
 *                           The number is the alignment, not a shift.
 *   Float32LSB / MSB        IEEE 32-bit, +-1.0 full scale
 *   Float64LSB / MSB        IEEE 64-bit, +-1.0 full scale
 *
 * Every integer conversion is symmetric full scale (2^(bits-1)), round
 * half away from zero, saturating - the contract the Int16 and Int32
 * paths already kept, extended to the rest. Anything not listed is
 * silence, and asio_convert_known() tells the caller so it can say so
 * once. */

#ifndef __ASIO_CONVERT_H
#define __ASIO_CONVERT_H

#include <stdint.h>
#include <string.h>

#include <boolean.h>
#include <retro_inline.h>

typedef long ASIOSampleType;

#define ASIOSTInt16MSB       0L
#define ASIOSTInt24MSB       1L
#define ASIOSTInt32MSB       2L
#define ASIOSTFloat32MSB     3L
#define ASIOSTFloat64MSB     4L
#define ASIOSTInt32MSB16     8L
#define ASIOSTInt32MSB18     9L
#define ASIOSTInt32MSB20    10L
#define ASIOSTInt32MSB24    11L
#define ASIOSTInt16LSB      16L
#define ASIOSTInt24LSB      17L
#define ASIOSTInt32LSB      18L
#define ASIOSTFloat32LSB    19L
#define ASIOSTFloat64LSB    20L
#define ASIOSTInt32LSB16    24L
#define ASIOSTInt32LSB18    25L
#define ASIOSTInt32LSB20    26L
#define ASIOSTInt32LSB24    27L

static INLINE size_t asio_bytes_per_sample(ASIOSampleType type)
{
   switch (type)
   {
      case ASIOSTInt16LSB:
      case ASIOSTInt16MSB:
         return 2;
      case ASIOSTInt24LSB:
      case ASIOSTInt24MSB:
         return 3;
      case ASIOSTInt32LSB:
      case ASIOSTInt32MSB:
      case ASIOSTInt32LSB16:
      case ASIOSTInt32LSB18:
      case ASIOSTInt32LSB20:
      case ASIOSTInt32LSB24:
      case ASIOSTInt32MSB16:
      case ASIOSTInt32MSB18:
      case ASIOSTInt32MSB20:
      case ASIOSTInt32MSB24:
      case ASIOSTFloat32LSB:
      case ASIOSTFloat32MSB:
         return 4;
      case ASIOSTFloat64LSB:
      case ASIOSTFloat64MSB:
         return 8;
      default:
         return 4;
   }
}

/* Whether asio_convert_frames() writes audio for this type, or silence. */
static INLINE bool asio_convert_known(ASIOSampleType type)
{
   switch (type)
   {
      case ASIOSTInt16LSB:   case ASIOSTInt16MSB:
      case ASIOSTInt24LSB:   case ASIOSTInt24MSB:
      case ASIOSTInt32LSB:   case ASIOSTInt32MSB:
      case ASIOSTInt32LSB16: case ASIOSTInt32LSB18:
      case ASIOSTInt32LSB20: case ASIOSTInt32LSB24:
      case ASIOSTInt32MSB16: case ASIOSTInt32MSB18:
      case ASIOSTInt32MSB20: case ASIOSTInt32MSB24:
      case ASIOSTFloat32LSB: case ASIOSTFloat32MSB:
      case ASIOSTFloat64LSB: case ASIOSTFloat64MSB:
         return true;
      default:
         return false;
   }
}

/* A float to a two's complement integer of bits significant bits:
 * symmetric full scale, round half away from zero, saturating. In
 * double so that 32-bit steps are addressable and the half-bias is not
 * absorbed at float precision. */
static INLINE int32_t asio_float_to_int(float v, unsigned bits)
{
   double scale = (double)((int64_t)1 << (bits - 1));
   double max   = scale - 1.0;
   double d     = (double)v * scale;
   d += (d >= 0.0) ? 0.5 : -0.5;
   if (d > max)
      return (int32_t)max;
   if (d < -scale)
      return (int32_t)(-scale);
   return (int32_t)d;
}

static INLINE void asio_store_u16(uint8_t *p, uint32_t v, bool big_endian)
{
   if (big_endian) { p[0] = (uint8_t)(v >> 8); p[1] = (uint8_t)v; }
   else            { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
}

static INLINE void asio_store_u24(uint8_t *p, uint32_t v, bool big_endian)
{
   if (big_endian) { p[0] = (uint8_t)(v >> 16); p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)v; }
   else            { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); }
}

static INLINE void asio_store_u32(uint8_t *p, uint32_t v, bool big_endian)
{
   if (big_endian)
   {
      p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16);
      p[2] = (uint8_t)(v >> 8);  p[3] = (uint8_t)v;
   }
   else
   {
      p[0] = (uint8_t)v;         p[1] = (uint8_t)(v >> 8);
      p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
   }
}

static INLINE void asio_store_u64(uint8_t *p, uint64_t v, bool big_endian)
{
   if (big_endian)
   {
      asio_store_u32(p,     (uint32_t)(v >> 32), true);
      asio_store_u32(p + 4, (uint32_t)v,         true);
   }
   else
   {
      asio_store_u32(p,     (uint32_t)v,         false);
      asio_store_u32(p + 4, (uint32_t)(v >> 32), false);
   }
}

/* Frames over which audio is faded to silence when it runs out mid
 * period, and up from silence when it returns: a step at either edge
 * is a click. Under a millisecond at 48 kHz, so an underrun is not
 * hidden, only its edges. */
#define ASIO_FADE_FRAMES 32

/* Writes have frames of src - interleaved float stereo - to buf_l and
 * buf_r in type, then silence to frames. Types asio_convert_known()
 * rejects get silence for all frames. Audio that stops short of frames
 * is faded out over its last ASIO_FADE_FRAMES; with fade_in set, the
 * caller having had silence last period, it is faded in over its first.
 * The fades are applied to src in place. */
static INLINE void asio_convert_frames(ASIOSampleType type,
      float *src, long have, long frames, void *buf_l, void *buf_r,
      bool fade_in)
{
   uint8_t *dl = (uint8_t*)buf_l;
   uint8_t *dr = (uint8_t*)buf_r;
   size_t   bps = asio_bytes_per_sample(type);
   long     i;
   unsigned bits;
   bool     big;

   if (!asio_convert_known(type))
      have = 0;
   if (have > frames)
      have = frames;

   if (have > 0)
   {
      long fade = have < ASIO_FADE_FRAMES ? have : ASIO_FADE_FRAMES;
      if (fade_in)
         for (i = 0; i < fade; i++)
         {
            float g = (float)(i + 1) / (float)fade;
            src[i * 2 + 0] *= g;
            src[i * 2 + 1] *= g;
         }
      if (have < frames)
         for (i = 0; i < fade; i++)
         {
            float g = (float)(fade - i - 1) / (float)fade;
            src[(have - fade + i) * 2 + 0] *= g;
            src[(have - fade + i) * 2 + 1] *= g;
         }
   }

   switch (type)
   {
      case ASIOSTFloat32LSB:
      case ASIOSTFloat32MSB:
         big = (type == ASIOSTFloat32MSB);
         for (i = 0; i < have; i++)
         {
            union { float f; uint32_t u; } l, r;
            l.f = src[i * 2 + 0];
            r.f = src[i * 2 + 1];
            asio_store_u32(dl + i * 4, l.u, big);
            asio_store_u32(dr + i * 4, r.u, big);
         }
         break;

      case ASIOSTFloat64LSB:
      case ASIOSTFloat64MSB:
         big = (type == ASIOSTFloat64MSB);
         for (i = 0; i < have; i++)
         {
            union { double f; uint64_t u; } l, r;
            l.f = (double)src[i * 2 + 0];
            r.f = (double)src[i * 2 + 1];
            asio_store_u64(dl + i * 8, l.u, big);
            asio_store_u64(dr + i * 8, r.u, big);
         }
         break;

      case ASIOSTInt16LSB:
      case ASIOSTInt16MSB:
         big = (type == ASIOSTInt16MSB);
         for (i = 0; i < have; i++)
         {
            asio_store_u16(dl + i * 2, (uint32_t)asio_float_to_int(src[i * 2 + 0], 16), big);
            asio_store_u16(dr + i * 2, (uint32_t)asio_float_to_int(src[i * 2 + 1], 16), big);
         }
         break;

      case ASIOSTInt24LSB:
      case ASIOSTInt24MSB:
         big = (type == ASIOSTInt24MSB);
         for (i = 0; i < have; i++)
         {
            asio_store_u24(dl + i * 3, (uint32_t)asio_float_to_int(src[i * 2 + 0], 24), big);
            asio_store_u24(dr + i * 3, (uint32_t)asio_float_to_int(src[i * 2 + 1], 24), big);
         }
         break;

      case ASIOSTInt32LSB:   case ASIOSTInt32MSB:
      case ASIOSTInt32LSB16: case ASIOSTInt32MSB16:
      case ASIOSTInt32LSB18: case ASIOSTInt32MSB18:
      case ASIOSTInt32LSB20: case ASIOSTInt32MSB20:
      case ASIOSTInt32LSB24: case ASIOSTInt32MSB24:
         switch (type)
         {
            case ASIOSTInt32LSB16: case ASIOSTInt32MSB16: bits = 16; break;
            case ASIOSTInt32LSB18: case ASIOSTInt32MSB18: bits = 18; break;
            case ASIOSTInt32LSB20: case ASIOSTInt32MSB20: bits = 20; break;
            case ASIOSTInt32LSB24: case ASIOSTInt32MSB24: bits = 24; break;
            default:                                      bits = 32; break;
         }
         big = (   type == ASIOSTInt32MSB   || type == ASIOSTInt32MSB16
                || type == ASIOSTInt32MSB18 || type == ASIOSTInt32MSB20
                || type == ASIOSTInt32MSB24);
         for (i = 0; i < have; i++)
         {
            /* The value in the low bits of the word, sign extended: the
             * int32_t cast to uint32_t is exactly that. */
            asio_store_u32(dl + i * 4, (uint32_t)asio_float_to_int(src[i * 2 + 0], bits), big);
            asio_store_u32(dr + i * 4, (uint32_t)asio_float_to_int(src[i * 2 + 1], bits), big);
         }
         break;

      default:
         break;
   }

   if (have < frames)
   {
      memset(dl + (size_t)have * bps, 0, (size_t)(frames - have) * bps);
      memset(dr + (size_t)have * bps, 0, (size_t)(frames - have) * bps);
   }
}

#endif
