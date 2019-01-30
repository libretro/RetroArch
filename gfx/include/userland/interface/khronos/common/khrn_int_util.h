/*
Copyright (c) 2012, Broadcom Europe Ltd
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef KHRN_INT_UTIL_H
#define KHRN_INT_UTIL_H

#include <ctype.h>
#include <float.h>
#include <math.h>

#include "interface/khronos/common/khrn_int_common.h"
#if !defined(__VIDEOCORE__) && !defined(WIN32)  // threadsx/nucleus define LONG which clashses
#include "interface/vcos/vcos.h"
#endif

/******************************************************************************
replacements for videocore intrinsics
******************************************************************************/

#ifdef _VIDEOCORE
#include <vc/intrinsics.h>
#define _minf(x,y) _min((float)(x),(float)(y))
#define _maxf(x,y) _max((float)(x),(float)(y))
#else
static INLINE int32_t _bmask(int32_t x, int32_t y)
{
   return x & ((1 << (y & 0x1f)) - 1);
}

static INLINE int32_t _min(int32_t x, int32_t y)
{
   return x < y ? x : y;
}

static INLINE int32_t _max(int32_t x, int32_t y)
{
   return x > y ? x : y;
}

#if defined(_MSC_VER)
static INLINE int32_t _msb(uint32_t x)
{
   int32_t l = -1;

   if (x)
      __asm {
         bsr eax, x
         mov l, eax
      }

   return l;
}
#elif defined __CC_ARM
static INLINE int32_t _msb(uint32_t x)
{
   return 31 - __clz(x);
}
#elif defined(__GNUC__)
static INLINE int32_t _msb(uint32_t x)
{
   return x ? (31 - __builtin_clz(x)) : -1;
}
#else
static INLINE int32_t _msb(uint32_t x) /* unsigned to get lsr */
{
   int32_t msb = -1;
   while (x != 0) {
      ++msb;
      x >>= 1;
   }
   return msb;
}
#endif

static INLINE uint32_t _count(uint32_t x)
{
   uint32_t count = 0;
   while (x != 0) {
      x &= x - 1;
      ++count;
   }
   return count;
}

#if defined __CC_ARM && __TARGET_ARCH_THUMB >= 4
static INLINE uint32_t _bitrev(uint32_t x, uint32_t y)
{
   return __rbit(x) >> (32-y);
}
#else
static INLINE uint32_t _bitrev(uint32_t x, uint32_t y)
{
   uint32_t bitrev = 0;
   uint32_t i;
   for (i = 0; i != y; ++i) {
      bitrev |= ((x >> i) & 1) << (y - i - 1);
   }
   return bitrev;
}
#endif

#ifdef __CC_ARM
static INLINE int32_t _adds(int32_t x, int32_t y)
{
   return __qadd(x, y);
}

static INLINE int32_t _subs(int32_t x, int32_t y)
{
   return __qsub(x, y);
}

static INLINE uint32_t _ror(uint32_t x, uint32_t y)
{
   return __ror(x, y);
}
#else
static INLINE int32_t _adds(int32_t x, int32_t y)
{
   int32_t z = x + y;
   return (y > 0) ? ((z < x) ? (int32_t)0x7fffffff : z) : ((z > x) ? (int32_t)0x80000000 : z);
}

static INLINE int32_t _subs(int32_t x, int32_t y)
{
   int32_t z = x - y;
   return (y > 0) ? ((z > x) ? (int32_t)0x80000000 : z) : ((z < x) ? (int32_t)0x7fffffff : z);
}

static INLINE uint32_t _ror(uint32_t x, uint32_t y)
{
   return (x << (32 - y)) | (x >> y);
}
#endif // __CC_ARM

static INLINE int32_t _abs(int32_t x)
{
   return x > 0 ? x : -x;
}

static INLINE float _minf(float x, float y)
{
   return x < y ? x : y;
}

static INLINE float _maxf(float x, float y)
{
   return x > y ? x : y;
}

#endif // !_VIDEOCORE


/******************************************************************************
misc stuff
******************************************************************************/

#define ARR_COUNT(ARR) (sizeof(ARR) / sizeof(*(ARR)))

/* sign-extend 16-bit value with range [-0x4000, 0xbfff] */
static INLINE int32_t s_ext_off16(int32_t x)
{
   return ((int32_t)(int16_t)(x - 0x4000)) + 0x4000;
}

static INLINE bool is_power_of_2(uint32_t x)
{
   return (x != 0) && ((x & (x - 1)) == 0);
}

static INLINE uint32_t next_power_of_2(uint32_t x)
{
   return is_power_of_2(x) ? x : (uint32_t)(1 << (_msb(x) + 1));
}

static INLINE uint32_t round_up(uint32_t x, uint32_t y)
{
   vcos_assert(is_power_of_2(y));
   return (x + (y - 1)) & ~(y - 1);
}

static INLINE void *round_up_ptr(void *x, uint32_t y)
{
   vcos_assert(is_power_of_2(y));
   return (void *)(((uintptr_t)x + (uintptr_t)(y - 1)) & ~(uintptr_t)(y - 1));
}

static INLINE uint32_t mod(int32_t x, int32_t y)
{
   int32_t m = x % y;
   return (m < 0) ? (m + y) : m;
}

extern int khrn_get_type_size(int type /* GLenum*/);

static INLINE int find_max(int count, int size, const void *indices)
{
   int i;
   int32_t max = -1;

   switch (size) {
   case 1:
   {
      uint8_t *u = (uint8_t *)indices;

      for (i = 0; i < count; i++)
         max = _max( max, (int32_t) u[i]);

      break;
   }
   case 2:
   {
      uint16_t *u = (uint16_t *)indices;

      for (i = 0; i < count; i++)
         max = _max( max, (int32_t) u[i]);

      break;
   }
   default:
      UNREACHABLE();
      break;
   }

   return (int) max;
}

/******************************************************************************
for poking around inside floats (we assume ieee-754)
******************************************************************************/

typedef union {
   float f;
   uint32_t bits;
} KHRN_FLOAT_BITS_T;

static INLINE uint32_t float_to_bits(float f)
{
   KHRN_FLOAT_BITS_T t;
   t.f = f;
   return t.bits;
}

static INLINE float float_from_bits(uint32_t bits)
{
   KHRN_FLOAT_BITS_T t;
   t.bits = bits;
   return t.f;
}

/******************************************************************************
input cleaning stuff
******************************************************************************/

#include "interface/khronos/common/khrn_int_util_cr.h"

static INLINE void clean_floats(float *dst, const float *src, uint32_t count)
{
   uint32_t i;
   for (i = 0; i != count; ++i) {
      dst[i] = clean_float(src[i]);
   }
}

/******************************************************************************
float to int conversions
******************************************************************************/

static INLINE float r2ni_to_r2n_bias(float f, int32_t shift)
{
   vcos_assert((shift >= -129) && (shift <= 124));
   return f + float_from_bits(((127 - (shift + 2)) << 23) | 0x7fffff);
}

/*
   convert float to integer value with shift
   saturating, round to nearest

   on videocore, we support shifts in [-32, 31]. we only need to support shifts
   of 0 and 16 for client-side code
*/

static INLINE int32_t float_to_int_shift(float f, int32_t shift)
{
#ifdef _VIDEOCORE
   /* floattouint is wrapping, round to negative infinity. shift should be in [-32, 31] */
   vcos_assert((shift >= -32) && (shift <= 31));
   f = r2ni_to_r2n_bias(f, shift);
   if (f < float_from_bits((1 << 31) | ((127 + (31 - shift)) << 23))) { return 0x80000000; }
   if (f > float_from_bits(((127 + (30 - shift)) << 23) | 0x7fffff))  { return 0x7fffffff; }
   return _floattouint(f, shift);
#else
   vcos_assert((shift >= 0) && (shift <= 31));
   f *= (float)(uint32_t)(1 << shift);
   f += (f < 0.0f) ? -0.49999997f : 0.49999997f; /* assume float -> int conversion is round to zero */
   if (f < -2.14748365e9f) { return 0x80000000; }
   if (f > 2.14748352e9f)  { return 0x7fffffff; }
   return (int32_t)f;
#endif
}

/*
   convert float to 48-bit integer value with shift
   saturating, round to nearest

   this is only supported on videocore. shift should be in [-16, 31]
*/

#ifdef _VIDEOCORE
static INLINE int64_t float_to_int48_shift(float f, int32_t shift)
{
   /* floattouint is wrapping, round to negative infinity. shift should be in [-32, 31] */
   vcos_assert((shift >= -16) && (shift <= 31));
   f = r2ni_to_r2n_bias(f, shift);
   if (f < float_from_bits((1 << 31) | ((127 + (47 - shift)) << 23))) { return 0xffff800000000000ll; }
   if (f > float_from_bits(((127 + (46 - shift)) << 23) | 0x7fffff))  { return 0x00007fffffffffffll; }
   return ((int64_t)(int32_t)_floattouint(f, shift - 16) << 16) | _floattouint(f, shift);
}
#endif

/*
   convert float to integer value
   saturating, round to nearest
*/

static INLINE int32_t float_to_int(float f)
{
   return float_to_int_shift(f, 0);
}

/*
   convert float to integer value
   saturating, round to negative inf
*/

static INLINE int32_t float_to_int_floor(float f)
{
   /*
      special-case handling of small negative floats
      this is so we return -1 for negative denormals (which the vg cts requires)
      (we shouldn't need this if the fp library/hw properly handle denormals)
   */

   uint32_t u = float_to_bits(f);
   if (((u & (1 << 31)) && (u + u)) && (f > -1.0f)) {
      return -1;
   }

   f = floorf(f); /* assume float -> int conversion is round to zero */
   if (f < -2.14748365e9f) { return 0x80000000; }
   if (f > 2.14748352e9f) { return 0x7fffffff; }
   return (int32_t)f;
}

/*
   convert float to integer value
   saturating, round to zero
*/

static INLINE int32_t float_to_int_zero(float f)
{
   /* assume float -> int conversion is round to zero */
   if (f < -2.14748365e9f) { return 0x80000000; }
   if (f > 2.14748352e9f) { return 0x7fffffff; }
   return (int32_t)f;
}

/*
   convert float to 16.16 fixed point value
   saturating, round to nearest

   Khronos documentation:

   If a value is so large in magnitude that it cannot be represented with the
   requested type, then the nearest value representable using the requested type
   is returned.
*/

static INLINE int32_t float_to_fixed(float f)
{
   return float_to_int_shift(f, 16);
}

/******************************************************************************
exact float tests (in case fp library/hw don't handle denormals correctly)
******************************************************************************/

static INLINE bool floats_identical(float x, float y)
{
   return float_to_bits(x) == float_to_bits(y);
}

static INLINE bool is_zero(float f)
{
   uint32_t u = float_to_bits(f);
   return !(u + u);
}

static INLINE bool is_le_zero(float f)
{
   uint32_t u = float_to_bits(f);
   return (u & (1 << 31)) || !u;
}

/******************************************************************************
alignment stuff
******************************************************************************/

#ifdef _MSC_VER
   #define alignof(T) __alignof(T)
#elif defined(__CC_ARM)
   #define alignof(T) __alignof__(T)
#else
   #define alignof(T) (sizeof(struct { T t; char ch; }) - sizeof(T))
#endif

/*
   must use both ALIGNED and ALIGN_TO...
   ALIGNED(16) int align_me[10];
   ALIGN_TO(align_me, 16);
*/

#ifdef _MSC_VER
   #define ALIGNED(ALIGNMENT) __declspec(align(ALIGNMENT))
   #define ALIGN_TO(X, ALIGNMENT)
#elif defined(__GNUC__)
   #define ALIGNED(ALIGNMENT) __attribute__ ((aligned(ALIGNMENT)))
   #define ALIGN_TO(X, ALIGNMENT)
#elif defined(__HIGHC__)
   #define ALIGNED(ALIGMENT)
   #define ALIGN_TO(X, ALIGNMENT) pragma Align_to(ALIGNMENT, X)
#else
   /* leave undefined (will get error on use) */
#endif

/******************************************************************************
range/rect intersect stuff
******************************************************************************/

extern void khrn_clip_range(
   int32_t *x0, int32_t *l0,
   int32_t x1, int32_t l1);

extern void khrn_clip_range2(
   int32_t *ax0, int32_t *bx0, int32_t *l0,
   int32_t ax1, int32_t al1,
   int32_t bx1, int32_t bl1);

extern void khrn_clip_rect(
   int32_t *x0, int32_t *y0, int32_t *w0, int32_t *h0,
   int32_t x1, int32_t y1, int32_t w1, int32_t h1);

extern void khrn_clip_rect2(
   int32_t *ax0, int32_t *ay0, int32_t *bx0, int32_t *by0, int32_t *w0, int32_t *h0,
   int32_t ax1, int32_t ay1, int32_t aw1, int32_t ah1,
   int32_t bx1, int32_t by1, int32_t bw1, int32_t bh1);

static INLINE bool khrn_ranges_intersect(
   int32_t x0, int32_t l0,
   int32_t x1, int32_t l1)
{
   return (x0 < (x1 + l1)) && (x1 < (x0 + l0));
}

static INLINE bool khrn_rects_intersect(
   int32_t x0, int32_t y0, int32_t w0, int32_t h0,
   int32_t x1, int32_t y1, int32_t w1, int32_t h1)
{
   return khrn_ranges_intersect(x0, w0, x1, w1) && khrn_ranges_intersect(y0, h0, y1, h1);
}

/******************************************************************************
memory barrier
******************************************************************************/

#ifdef KHRN_SINGLE_THREADED
   /* everything is done in one thread, no need for barriers */
   static INLINE void khrn_barrier(void) {}
#elif defined(_VIDEOCORE)
   /* don't need a real memory barrier
    * extern function should do as a compiler barrier, but todo: is there a better way? */
   extern void khrn_barrier(void);
#else
   /* leave undefined (will get error on use) */
#endif

#endif
