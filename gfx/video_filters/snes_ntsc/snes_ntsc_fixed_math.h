/* Deterministic Q32.32 arithmetic for SNES NTSC setup.
 * LGPL-2.1-or-later, like snes_ntsc_impl.h. No floating-point arithmetic.
 * Products and quotients round to nearest, ties away from zero. */
#ifndef SNES_NTSC_FIXED_MATH_H
#define SNES_NTSC_FIXED_MATH_H
#include <stdint.h>
#include <string.h>
#include <limits.h>
#include <float.h>
#if FLT_RADIX != 2 || DBL_MANT_DIG != 53 || FLT_MANT_DIG != 24
#error "SNES NTSC setup requires IEEE binary32/binary64 controls"
#endif
typedef char snes_fixed_double_size[(sizeof(double) == 8) ? 1 : -1];
typedef char snes_fixed_float_size[(sizeof(float) == 4) ? 1 : -1];
typedef int64_t snes_fixed_t;
#define SNES_F_ONE INT64_C(4294967296)
#define SNES_F_PI INT64_C(13493037705)
#define SNES_F_LN2 INT64_C(2977044472)
#define SNES_F(n,d) ((SNES_F_ONE * (n) + (d)/2) / (d))
static uint64_t snes_f_abs(snes_fixed_t a)
{ return a < 0 ? (uint64_t)(-(a + 1)) + 1 : (uint64_t)a; }
static snes_fixed_t snes_f_sign(uint64_t a, int neg)
{
   uint64_t limit = (uint64_t)INT64_MAX + (neg != 0);
   if (a > limit) a = limit;
   if (!neg) return (snes_fixed_t)a;
   return a == limit ? INT64_MIN : -(snes_fixed_t)a;
}
/* Explicit truncation toward zero, including C89 implementations. */
static snes_fixed_t snes_f_idiv(snes_fixed_t a, unsigned b)
{ return snes_f_sign(snes_f_abs(a)/b, a < 0); }
static snes_fixed_t snes_f_mul(snes_fixed_t a, snes_fixed_t b)
{
   uint64_t x = snes_f_abs(a), y = snes_f_abs(b), r;
   int neg = (a < 0) != (b < 0);
   uint64_t limit = (uint64_t)INT64_MAX + neg;
#if defined(__SIZEOF_INT128__) && !defined(SNES_NTSC_FIXED_PORTABLE)
   __extension__ typedef unsigned __int128 snes_wide_t;
   snes_wide_t v = ((snes_wide_t)x * y + UINT64_C(2147483648)) >> 32;
   r = v > limit ? limit : (uint64_t)v;
#else
   uint64_t lo = (uint64_t)(uint32_t)x * (uint32_t)y;
   uint64_t hi = (x >> 32) * (y >> 32);
   uint64_t terms[4];
   unsigned j;
   if (hi > (limit >> 32)) return snes_f_sign(limit, neg);
   r = hi << 32;
   terms[0] = (x >> 32) * (uint32_t)y;
   terms[1] = (uint64_t)(uint32_t)x * (y >> 32);
   terms[2] = lo >> 32;
   terms[3] = (uint32_t)lo >= UINT32_C(2147483648);
   for (j = 0; j < 4; j++)
   {
      if (terms[j] > limit - r) return snes_f_sign(limit, neg);
      r += terms[j];
   }
#endif
   return snes_f_sign(r, neg);
}
static snes_fixed_t snes_f_div(snes_fixed_t a, snes_fixed_t b)
{
   uint64_t x = snes_f_abs(a), y = snes_f_abs(b), r;
   int neg = (a < 0) != (b < 0);
   uint64_t limit = (uint64_t)INT64_MAX + neg;
#if defined(__SIZEOF_INT128__) && !defined(SNES_NTSC_FIXED_PORTABLE)
   __extension__ typedef unsigned __int128 snes_wide_t;
   snes_wide_t v;
#else
   uint64_t rem;
   unsigned j;
#endif
   if (!y) return a ? snes_f_sign(limit, neg) : 0;
#if defined(__SIZEOF_INT128__) && !defined(SNES_NTSC_FIXED_PORTABLE)
   v = (((snes_wide_t)x << 32) + y/2) / y;
   r = v > limit ? limit : (uint64_t)v;
#else
   rem = x % y;
   r = x / y;
   for (j = 0; j < 32; j++)
   {
      if (r > (limit >> 1)) return snes_f_sign(limit, neg);
      r <<= 1;
      rem <<= 1;
      if (rem >= y) { rem -= y; r++; }
      if (r > limit) return snes_f_sign(limit, neg);
   }
   if (rem >= y - rem && r < limit) r++;
#endif
   return snes_f_sign(r, neg);
}
/* Decode the public IEEE setup values by bits. Subnormals round to zero;
 * non-finite settings become zero. The public controls are limited to [-1,1]. */
static snes_fixed_t snes_f_bits(uint64_t mant, int shift, int neg)
{
   uint64_t r;
   if (shift >= 0)
   {
      if (shift >= 63 || mant > ((uint64_t)INT64_MAX >> shift))
         return snes_f_sign((uint64_t)INT64_MAX + neg, neg);
      r = mant << shift;
   }
   else
   {
      unsigned n = (unsigned)-shift;
      r = n >= 64 ? 0 : (mant >> n) + ((mant >> (n-1)) & 1);
   }
   return snes_f_sign(r, neg);
}
static snes_fixed_t snes_f_setting(double a)
{
   uint64_t bits;
   unsigned exp;
   snes_fixed_t r;
   memcpy(&bits, &a, sizeof bits);
   exp = (unsigned)((bits >> 52) & 2047);
   if (!exp || exp == 2047) return 0;
   r = snes_f_bits((bits & UINT64_C(4503599627370495)) |
         UINT64_C(4503599627370496), (int)exp - 1023 - 52 + 32,
         (int)(bits >> 63));
   if (r > SNES_F_ONE) r = SNES_F_ONE;
   if (r < -SNES_F_ONE) r = -SNES_F_ONE;
   return r;
}
static snes_fixed_t snes_f_decoder(float a)
{
   uint32_t bits;
   unsigned exp;
   snes_fixed_t r;
   memcpy(&bits, &a, sizeof bits);
   exp = (bits >> 23) & 255;
   if (!exp || exp == 255) return 0;
   r = snes_f_bits((bits & UINT32_C(8388607)) | UINT32_C(8388608),
         (int)exp - 127 - 23 + 32, (int)(bits >> 31));
   /* More than enough for a YIQ decoder; bounds all subsequent sums. */
   if (r > 16 * SNES_F_ONE) r = 16 * SNES_F_ONE;
   if (r < -16 * SNES_F_ONE) r = -16 * SNES_F_ONE;
   return r;
}
static int snes_f_int(snes_fixed_t a)
{
   uint64_t magnitude = snes_f_abs(a) >> 32;
   if (a < 0)
      return magnitude > INT_MAX ? INT_MIN : -(int)magnitude;
   return magnitude > INT_MAX ? INT_MAX : (int)magnitude;
}
static snes_fixed_t snes_f_exp(snes_fixed_t x)
{
   snes_fixed_t r, term = SNES_F_ONE, sum = SNES_F_ONE;
   int k, n;
   if (x < -32 * SNES_F_ONE) return 0;
   if (x > 21 * SNES_F_ONE) return INT64_MAX;
   k = (int)snes_f_idiv(x, (unsigned)SNES_F_LN2);
   r = x - k * SNES_F_LN2;
   for (n = 1; n <= 18; n++)
   { term = snes_f_idiv(snes_f_mul(term, r), (unsigned)n); sum += term; }
   if (k < 0) return (sum + (INT64_C(1) << (-k-1))) >> -k;
   if (sum > (INT64_MAX >> k)) return INT64_MAX;
   return sum << k;
}
static snes_fixed_t snes_f_log(snes_fixed_t x)
{
   snes_fixed_t y, yy, term, sum = 0;
   int k = 0, n;
   while (x < SNES_F_ONE) { x *= 2; k--; }
   while (x >= 2 * SNES_F_ONE) { x /= 2; k++; }
   y = snes_f_div(x - SNES_F_ONE, x + SNES_F_ONE);
   yy = snes_f_mul(y, y);
   term = y;
   for (n = 1; n < 41; n += 2)
   { sum += term / n; term = snes_f_mul(term, yy); }
   return 2 * sum + k * SNES_F_LN2;
}
static snes_fixed_t snes_f_pow(snes_fixed_t x, snes_fixed_t y)
{ return x ? snes_f_exp(snes_f_mul(snes_f_log(x), y)) : 0; }
static snes_fixed_t snes_f_cos(snes_fixed_t x)
{
   snes_fixed_t term = SNES_F_ONE, sum = SNES_F_ONE, xx;
   int n, neg = 0;
   x = (snes_fixed_t)(snes_f_abs(x) % (2 * (uint64_t)SNES_F_PI));
   if (x > SNES_F_PI) x = 2 * SNES_F_PI - x;
   if (x > SNES_F_PI/2) { x = SNES_F_PI - x; neg = 1; }
   xx = snes_f_mul(x, x);
   for (n = 1; n <= 12; n++)
   { term = snes_f_idiv(-snes_f_mul(term, xx), (unsigned)((2*n-1)*(2*n))); sum += term; }
   return neg ? -sum : sum;
}
static snes_fixed_t snes_f_sin(snes_fixed_t x)
{ return snes_f_cos(SNES_F_PI/2 - x); }
#endif
