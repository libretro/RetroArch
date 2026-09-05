/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rstrtod.c).
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

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

#include <retro_endianness.h>
#include <string/rstrtod.h>

/* Three paths, tried in order, each slower and more certain than the
 * last:
 *
 *   1. The exact path. Up to 15 significant digits with a small decimal
 *      exponent is a case where the mantissa and the power of ten are
 *      both exact doubles, so one multiply or divide is already
 *      correctly rounded. Most numbers a person writes land here.
 *
 *   2. The 128-bit path. Multiply the mantissa by a 128-bit power of
 *      five and read the rounded result off the top of the product.
 *      This is exact whenever the product is far enough from a rounding
 *      boundary to tell, which is nearly always, and it reports failure
 *      rather than guessing when it is not.
 *
 *   3. The decimal path. Shift a decimal digit string one bit at a time
 *      until the value is normalised, then read the mantissa out. No
 *      approximation anywhere, so it settles the cases the 128-bit path
 *      declined. It is far slower and runs for a vanishing fraction of
 *      inputs.
 *
 * The tables below are powers of five, not powers of ten: a power of
 * ten is a power of five times a power of two, and the power of two is
 * free because it lands in the exponent.
 */

#if defined(_MSC_VER)
#define RSTRTOD_U64(x) ((uint64_t)(x##ui64))
#else
#define RSTRTOD_U64(x) ((uint64_t)(x##ULL))
#endif

/* The public entry points are ordinary functions, but inside the TU the
 * scan is folded into them: the parse result then lives in registers
 * instead of taking a trip through a stack struct that char reads
 * cannot be reordered around. A hint is not enough at this size, so the
 * GNU attribute is used where it exists; elsewhere this is an ordinary
 * static function and everything still works, just with a call. */
#if defined(__GNUC__)
#define RSTRTOD_FLATTEN __attribute__((always_inline)) __inline__
#else
#define RSTRTOD_FLATTEN
#endif

/* 64x64->128 multiply. Every target has some way to do this in one or
 * two instructions; the portable fallback is four 32-bit multiplies. */
#if defined(__SIZEOF_INT128__)
static void rstrtod_mul128(uint64_t a, uint64_t b, uint64_t *hi, uint64_t *lo)
{
   __extension__ typedef unsigned __int128 rstrtod_u128;
   rstrtod_u128 r = (rstrtod_u128)a * (rstrtod_u128)b;
   *lo = (uint64_t)r;
   *hi = (uint64_t)(r >> 64);
}
#elif defined(_MSC_VER) && defined(_M_X64)
#include <intrin.h>
#pragma intrinsic(_umul128)
static void rstrtod_mul128(uint64_t a, uint64_t b, uint64_t *hi, uint64_t *lo)
{
   *lo = _umul128(a, b, hi);
}
#else
static void rstrtod_mul128(uint64_t a, uint64_t b, uint64_t *hi, uint64_t *lo)
{
   uint32_t alo = (uint32_t)a;
   uint32_t ahi = (uint32_t)(a >> 32);
   uint32_t blo = (uint32_t)b;
   uint32_t bhi = (uint32_t)(b >> 32);
   uint64_t p00 = (uint64_t)alo * blo;
   uint64_t p01 = (uint64_t)alo * bhi;
   uint64_t p10 = (uint64_t)ahi * blo;
   uint64_t p11 = (uint64_t)ahi * bhi;
   uint64_t mid = (p00 >> 32) + (uint32_t)p01 + (uint32_t)p10;

   *lo = (p00 & RSTRTOD_U64(0xFFFFFFFF)) | (mid << 32);
   *hi = p11 + (p01 >> 32) + (p10 >> 32) + (mid >> 32);
}
#endif

/* Count of leading zero bits. One instruction where the compiler
 * offers it; the shift ladder costs enough that Eisel-Lemire feels it
 * on every call. Callers never pass zero, but the builtins leave zero
 * undefined, so it is handled rather than assumed. */
#if defined(__GNUC__)
static int rstrtod_clz64(uint64_t x)
{
   return x ? __builtin_clzll(x) : 64;
}
#elif defined(_MSC_VER) && (defined(_M_X64) || defined(_M_ARM64))
#include <intrin.h>
static int rstrtod_clz64(uint64_t x)
{
   unsigned long i;
   if (!_BitScanReverse64(&i, x))
      return 64;
   return 63 - (int)i;
}
#else
static int rstrtod_clz64(uint64_t x)
{
   int n = 0;
   if (x == 0)
      return 64;
   if (!(x & RSTRTOD_U64(0xFFFFFFFF00000000))) { n += 32; x <<= 32; }
   if (!(x & RSTRTOD_U64(0xFFFF000000000000))) { n += 16; x <<= 16; }
   if (!(x & RSTRTOD_U64(0xFF00000000000000))) { n +=  8; x <<=  8; }
   if (!(x & RSTRTOD_U64(0xF000000000000000))) { n +=  4; x <<=  4; }
   if (!(x & RSTRTOD_U64(0xC000000000000000))) { n +=  2; x <<=  2; }
   if (!(x & RSTRTOD_U64(0x8000000000000000))) { n +=  1; }
   return n;
}
#endif

#include "rstrtod_pow5.inc"

#define RSTRTOD_POW5_MIN (-342)
#define RSTRTOD_POW5_MAX (308)

/* Exact powers of ten, for the fast path. 10^22 is the last one that is
 * exactly a double. */
static const double rstrtod_pow10[23] =
{
   1e0,  1e1,  1e2,  1e3,  1e4,  1e5,  1e6,  1e7,
   1e8,  1e9,  1e10, 1e11, 1e12, 1e13, 1e14, 1e15,
   1e16, 1e17, 1e18, 1e19, 1e20, 1e21, 1e22
};

/* Per-format constants. Keeping them in a struct rather than in the
 * preprocessor means the two formats share one body. */
struct rstrtod_format
{
   uint64_t max_mantissa_fast;
   int      mantissa_bits;       /* explicit mantissa bits            */
   int      min_exponent;        /* exponent of the smallest normal   */
   int      infinite_power;      /* biased exponent meaning infinity  */
   int      max_exponent_fast;   /* largest exact power of ten        */
   int      min_round_to_even;
   int      max_round_to_even;
   int      smallest_power;      /* below this the value is zero      */
   int      largest_power;       /* above this the value is infinite  */
};

static const struct rstrtod_format rstrtod_fmt_double =
{
   RSTRTOD_U64(0x20000000000000), /* 2^53 */
   52, -1023, 0x7FF, 22, -4, 23, -342, 308
};

static const struct rstrtod_format rstrtod_fmt_float =
{
   RSTRTOD_U64(0x1000000),        /* 2^24 */
   23, -127, 0xFF, 10, -17, 10, -65, 38
};

/* The binary exponent of 10^q, to within the one bit that the caller
 * corrects for. 152170/65536 is log2(10) - 3 in fixed point. */
static int rstrtod_power_of_ten_exp(int q)
{
   return (((152170 + 65536) * q) >> 16) + 63;
}

/* ---------------------------------------------------------------- */
/* the 128-bit path                                                 */
/* ---------------------------------------------------------------- */

/* Multiply the normalised mantissa by the 128-bit power of five,
 * keeping enough of the product to round by. When the top of the
 * product is all ones the answer is on a knife edge, so the low half of
 * the power is brought in and the product extended. */
static void rstrtod_product(int q, uint64_t w, int precision,
      uint64_t *hi, uint64_t *lo)
{
   int      index = q - RSTRTOD_POW5_MIN;
   uint64_t mask  = (precision < 64)
      ? (RSTRTOD_U64(0xFFFFFFFFFFFFFFFF) >> precision)
      : RSTRTOD_U64(0xFFFFFFFFFFFFFFFF);
   uint64_t fh, fl;

   rstrtod_mul128(w, rstrtod_pow5[index][0], &fh, &fl);

   if ((fh & mask) == mask)
   {
      uint64_t sh, sl;
      rstrtod_mul128(w, rstrtod_pow5[index][1], &sh, &sl);
      fl += sh;
      if (sh > fl)
         fh++;
   }
   *hi = fh;
   *lo = fl;
}

/* Returns 1 and fills @out with the raw bit pattern, or 0 when the
 * product landed too close to a boundary to decide. */
static RSTRTOD_FLATTEN int rstrtod_eisel_lemire(const struct rstrtod_format *fmt,
      uint64_t w, int q, uint64_t *out)
{
   uint64_t hi, lo, mantissa;
   int      lz, upperbit, power2, shift;

   if (w == 0 || q < fmt->smallest_power)
   {
      *out = 0;
      return 1;
   }
   if (q > fmt->largest_power)
   {
      *out = (uint64_t)fmt->infinite_power << fmt->mantissa_bits;
      return 1;
   }

   lz = rstrtod_clz64(w);
   w <<= lz;

   rstrtod_product(q, w, fmt->mantissa_bits + 3, &hi, &lo);

   /* the low word saturated: the extended product still cannot say
    * which side of the boundary this falls on */
   if (lo == RSTRTOD_U64(0xFFFFFFFFFFFFFFFF)
         && (hi & ((RSTRTOD_U64(0xFFFFFFFFFFFFFFFF)) >> (fmt->mantissa_bits + 3)))
            == ((RSTRTOD_U64(0xFFFFFFFFFFFFFFFF)) >> (fmt->mantissa_bits + 3)))
      return 0;

   upperbit = (int)(hi >> 63);
   shift    = upperbit + 64 - fmt->mantissa_bits - 3;
   mantissa = hi >> shift;

   power2   = rstrtod_power_of_ten_exp(q) + upperbit - lz - fmt->min_exponent;

   if (power2 <= 0)
   {
      /* subnormal, or smaller than the smallest subnormal */
      if (-power2 + 1 >= 64)
      {
         *out = 0;
         return 1;
      }
      mantissa >>= (-power2 + 1);
      mantissa += (mantissa & 1);
      mantissa >>= 1;
      /* Rounding can carry a subnormal up into the smallest normal, in
       * which case the mantissa still holds the now-implicit bit -- at
       * exactly the position of the exponent field's low bit. OR-ing
       * folds it into the exponent; adding would double the value. */
      power2 = (mantissa < (RSTRTOD_U64(1) << fmt->mantissa_bits)) ? 0 : 1;
      *out = ((uint64_t)power2 << fmt->mantissa_bits) | mantissa;
      return 1;
   }

   /* A tie between two representable values is only genuine when the
    * product is exact; otherwise the low bits decide it. */
   if (lo <= 1
         && q >= fmt->min_round_to_even
         && q <= fmt->max_round_to_even
         && (mantissa & 3) == 1
         && (mantissa << shift) == hi)
      mantissa &= ~(uint64_t)1;

   mantissa += (mantissa & 1);
   mantissa >>= 1;

   if (mantissa >= (RSTRTOD_U64(2) << fmt->mantissa_bits))
   {
      mantissa = (RSTRTOD_U64(1) << fmt->mantissa_bits);
      power2++;
   }
   mantissa &= ~(RSTRTOD_U64(1) << fmt->mantissa_bits);

   if (power2 >= fmt->infinite_power)
   {
      *out = (uint64_t)fmt->infinite_power << fmt->mantissa_bits;
      return 1;
   }
   *out = mantissa | ((uint64_t)power2 << fmt->mantissa_bits);
   return 1;
}

/* ---------------------------------------------------------------- */
/* the decimal path                                                 */
/* ---------------------------------------------------------------- */

/* Enough digits to separate any two adjacent doubles: the widest gap
 * needing resolution is around 767 significant digits. */
#define RSTRTOD_MAX_DIGITS 800

struct rstrtod_decimal
{
   int     num_digits;
   int     decimal_point;   /* digits[] scaled by 10^decimal_point */
   int     truncated;
   uint8_t digits[RSTRTOD_MAX_DIGITS];
};

/* The decimal is held as digits most significant first, the value being
 * 0.d0d1... scaled by 10^decimal_point.
 *
 * Both shifts move up to RSTRTOD_MAX_SHIFT bits per pass. That bound is
 * what keeps the 64-bit accumulator from overflowing: it carries less
 * than 2^k, and the next step forms n*10 + 9, so 10 * 2^60 has to stay
 * under 2^64. Shifting a bit at a time is correct but costs a pass over
 * every digit per bit, and normalising a value near 1e300 needs about a
 * thousand bits of shift -- that is the difference between microseconds
 * and milliseconds on a long input. */
#define RSTRTOD_MAX_SHIFT 60


/* One subtract and one unsigned compare instead of two compares. */
#define RSTRTOD_IS_DIGIT(c) \
   ((unsigned)((unsigned char)(c) - (unsigned char)'0') < 10u)

static void rstrtod_dec_trim(struct rstrtod_decimal *d)
{
   while (d->num_digits > 0 && d->digits[d->num_digits - 1] == 0)
      d->num_digits--;
   if (d->num_digits == 0)
      d->decimal_point = 0;
}

/* Divide by 2^k. */
static void rstrtod_dec_rshift(struct rstrtod_decimal *d, int k)
{
   int      r    = 0;
   int      w    = 0;
   uint64_t n    = 0;
   uint64_t mask = (RSTRTOD_U64(1) << k) - 1;

   /* Pull in leading digits until there is something above the point. */
   while ((n >> k) == 0)
   {
      if (r >= d->num_digits)
      {
         if (n == 0)
         {
            d->num_digits    = 0;
            d->decimal_point = 0;
            return;
         }
         while ((n >> k) == 0)
         {
            n *= 10;
            r++;
         }
         break;
      }
      n = n * 10 + d->digits[r];
      r++;
   }
   d->decimal_point -= r - 1;

   for (; r < d->num_digits; r++)
   {
      uint64_t c   = d->digits[r];
      uint64_t dig = n >> k;
      n &= mask;
      d->digits[w++] = (uint8_t)dig;
      n = n * 10 + c;
   }
   while (n > 0)
   {
      uint64_t dig = n >> k;
      n &= mask;
      if (w < RSTRTOD_MAX_DIGITS)
         d->digits[w++] = (uint8_t)dig;
      else if (dig > 0)
         d->truncated = 1;
      n *= 10;
   }
   d->num_digits = w;
   rstrtod_dec_trim(d);
}

/* Multiply by 2^k. The result is built back to front in a scratch
 * buffer, since how many digits it grows by is only known once the
 * carry runs out. */
static void rstrtod_dec_lshift(struct rstrtod_decimal *d, int k)
{
   uint8_t  tmp[RSTRTOD_MAX_DIGITS + RSTRTOD_MAX_SHIFT];
   int      r     = d->num_digits - 1;
   int      w     = (int)sizeof(tmp);
   int      extra = 0;
   int      len;
   uint64_t n     = 0;

   for (; r >= 0; r--)
   {
      uint64_t q;
      n += (uint64_t)d->digits[r] << k;
      q  = n / 10;
      tmp[--w] = (uint8_t)(n - q * 10);
      n  = q;
   }
   while (n > 0)
   {
      uint64_t q = n / 10;
      tmp[--w]   = (uint8_t)(n - q * 10);
      n          = q;
      extra++;
   }

   len = (int)sizeof(tmp) - w;
   d->decimal_point += extra;
   if (len > RSTRTOD_MAX_DIGITS)
   {
      d->truncated = 1;
      len          = RSTRTOD_MAX_DIGITS;
   }
   memcpy(d->digits, tmp + w, (size_t)len);
   d->num_digits = len;
   rstrtod_dec_trim(d);
}

/* Shift by any amount, a pass at a time. */
static void rstrtod_dec_shift(struct rstrtod_decimal *d, int k)
{
   while (k > 0 && d->num_digits > 0)
   {
      int n = (k > RSTRTOD_MAX_SHIFT) ? RSTRTOD_MAX_SHIFT : k;
      rstrtod_dec_lshift(d, n);
      k -= n;
   }
   while (k < 0 && d->num_digits > 0)
   {
      int n = (-k > RSTRTOD_MAX_SHIFT) ? RSTRTOD_MAX_SHIFT : -k;
      rstrtod_dec_rshift(d, n);
      k += n;
   }
}

/* Step sizes for normalising into [0.5, 1).
 *
 * A shift that overshoots the target range leaves the value outside it
 * with no further pass to correct it, so both bounds are derived rather
 * than guessed. Digits are trimmed, so d0 >= 1 and the value lies in
 * [10^(dp-1), 10^dp).
 *
 * Dividing:    10^(dp-1) / 2^n >= 1/2  =>  n <= 1 + (dp-1)*log2(10)
 * Multiplying: 10^dp * 2^n     <  1    =>  n <= -dp*log2(10)
 *
 * log2(10) is used as 3401/1024, which is just under the true value, so
 * the bound is never overstated. */
#define RSTRTOD_LOG2_10_NUM 3401
#define RSTRTOD_LOG2_10_DEN 1024

static int rstrtod_step_down(int dp)
{
   int n;
   if (dp <= 1)
      return 1;
   if (dp - 1 >= 19)
      return RSTRTOD_MAX_SHIFT;
   n = 1 + ((dp - 1) * RSTRTOD_LOG2_10_NUM) / RSTRTOD_LOG2_10_DEN;
   return (n > RSTRTOD_MAX_SHIFT) ? RSTRTOD_MAX_SHIFT : n;
}

static int rstrtod_step_up(int dp)
{
   int m = -dp;
   int n;
   if (m <= 0)
      return 1;
   if (m >= 19)
      return RSTRTOD_MAX_SHIFT;
   n = (m * RSTRTOD_LOG2_10_NUM) / RSTRTOD_LOG2_10_DEN;
   if (n < 1)
      n = 1;
   return (n > RSTRTOD_MAX_SHIFT) ? RSTRTOD_MAX_SHIFT : n;
}

/* Take the integer part, rounding to nearest with ties to even. */
static uint64_t rstrtod_dec_round(const struct rstrtod_decimal *d)
{
   uint64_t n = 0;
   int      i;
   int      round_up;

   if (d->num_digits == 0 || d->decimal_point < 0)
      return 0;
   if (d->decimal_point > 18)
      return RSTRTOD_U64(0xFFFFFFFFFFFFFFFF);

   for (i = 0; i < d->decimal_point; i++)
      n = n * 10 + (uint64_t)((i < d->num_digits) ? d->digits[i] : 0);

   if (d->decimal_point >= d->num_digits)
      return n;                            /* nothing after the point */

   round_up = (d->digits[d->decimal_point] >= 5);
   if (round_up && d->digits[d->decimal_point] == 5
         && (d->decimal_point + 1 == d->num_digits))
   {
      /* exactly one half: ties to even, unless digits were dropped, in
       * which case the true value is above the half */
      if (!d->truncated)
         round_up = (n & 1) != 0;
   }
   return round_up ? n + 1 : n;
}

static uint64_t rstrtod_decimal_to_bits(const struct rstrtod_format *fmt,
      struct rstrtod_decimal *d)
{
   uint64_t mantissa;
   int      exp2 = 0;
   int      power2;

   if (d->num_digits == 0)
      return 0;
   if (d->decimal_point > fmt->largest_power + 1)
      return (uint64_t)fmt->infinite_power << fmt->mantissa_bits;
   if (d->decimal_point < fmt->smallest_power - 30)
      return 0;

   /* Bring the value into [0.5, 1), counting the halvings. One loop
    * rather than two, so a step that lands on the wrong side is simply
    * corrected on the next pass instead of escaping. */
   for (;;)
   {
      int n;
      if (d->decimal_point > 0)
      {
         n = rstrtod_step_down(d->decimal_point);
         rstrtod_dec_rshift(d, n);
         exp2 += n;
      }
      else if (d->decimal_point < 0 || d->digits[0] < 5)
      {
         n = rstrtod_step_up(d->decimal_point);
         rstrtod_dec_lshift(d, n);
         exp2 -= n;
      }
      else
         break;

      if (d->num_digits == 0)
         return 0;
   }

   /* The value is now v * 2^exp2 with v in [0.5, 1). A normal number
    * has biased exponent exp2 - min_exponent - 1 and mantissa v * 2^p,
    * p being the explicit bits plus the implicit one. */
   power2 = exp2 - fmt->min_exponent - 1;

   if (power2 <= 0)
   {
      /* subnormal: shift the value down instead of the exponent */
      int n = 1 - power2;
      if (n > fmt->mantissa_bits + 3)
         return 0;
      rstrtod_dec_shift(d, -n);
      power2 = 0;
   }

   if (power2 >= fmt->infinite_power)
      return (uint64_t)fmt->infinite_power << fmt->mantissa_bits;

   rstrtod_dec_shift(d, fmt->mantissa_bits + 1);
   mantissa = rstrtod_dec_round(d);

   if (mantissa >= (RSTRTOD_U64(2) << fmt->mantissa_bits))
   {
      /* rounding carried into the next binade */
      mantissa >>= 1;
      power2++;
      if (power2 >= fmt->infinite_power)
         return (uint64_t)fmt->infinite_power << fmt->mantissa_bits;
   }
   if (mantissa < (RSTRTOD_U64(1) << fmt->mantissa_bits))
      power2 = 0;                         /* stayed subnormal */
   else if (power2 == 0)
      power2 = 1;                         /* a subnormal that rounded up to
                                           * exactly the smallest normal */

   mantissa &= ~(RSTRTOD_U64(1) << fmt->mantissa_bits);
   return mantissa | ((uint64_t)power2 << fmt->mantissa_bits);
}

/* ---------------------------------------------------------------- */
/* eight digits at a time                                           */
/* ---------------------------------------------------------------- */

/* Scanning a digit at a time costs a compare and a branch per byte.
 * Eight bytes can be tested and converted with a handful of arithmetic
 * ops instead, which is most of the difference against a tuned parser
 * on ordinary input. The word is read with memcpy, so no alignment is
 * assumed, and it is byte-reversed on a big-endian host because the
 * arithmetic below is written for ascending significance. */

static uint64_t rstrtod_read8(const char *p)
{
   uint64_t v;
   memcpy(&v, p, 8);
#ifdef MSB_FIRST
   v =   ((v & RSTRTOD_U64(0x00000000000000FF)) << 56)
       | ((v & RSTRTOD_U64(0x000000000000FF00)) << 40)
       | ((v & RSTRTOD_U64(0x0000000000FF0000)) << 24)
       | ((v & RSTRTOD_U64(0x00000000FF000000)) <<  8)
       | ((v & RSTRTOD_U64(0x000000FF00000000)) >>  8)
       | ((v & RSTRTOD_U64(0x0000FF0000000000)) >> 24)
       | ((v & RSTRTOD_U64(0x00FF000000000000)) >> 40)
       | ((v & RSTRTOD_U64(0xFF00000000000000)) >> 56);
#endif
   return v;
}

/* True when all eight bytes are ASCII digits. */
static int rstrtod_is_eight_digits(uint64_t v)
{
   return ((  (v & RSTRTOD_U64(0xF0F0F0F0F0F0F0F0))
            | (((v + RSTRTOD_U64(0x0606060606060606))
                  & RSTRTOD_U64(0xF0F0F0F0F0F0F0F0)) >> 4))
         == RSTRTOD_U64(0x3333333333333333));
}

/* Fold eight ASCII digits into their value, pairing them up three
 * times rather than multiplying eight times. */
static uint32_t rstrtod_parse_eight_digits(uint64_t v)
{
   v -= RSTRTOD_U64(0x3030303030303030);
   v  = (v * 10) + (v >> 8);
   v  = (((v & RSTRTOD_U64(0x000000FF000000FF))
            * RSTRTOD_U64(0x000F424000000064))
      +  (((v >> 16) & RSTRTOD_U64(0x000000FF000000FF))
            * RSTRTOD_U64(0x0000271000000001))) >> 32;
   return (uint32_t)v;
}

/* Fold up to 19-have digits from [r, seg_end) into the mantissa. The
 * caller guarantees the range holds nothing but digits, so the eight-
 * wide step needs no validation. */
static RSTRTOD_FLATTEN const char *rstrtod_take19(const char *r, const char *seg_end,
      uint64_t *mant, int *have)
{
   uint64_t m    = *mant;
   int      n    = (int)(seg_end - r);
   int      want = 19 - *have;

   if (n > want)
      n = want;
   *have += n;

   while (n >= 8)
   {
      m  = m * RSTRTOD_U64(100000000)
         + rstrtod_parse_eight_digits(rstrtod_read8(r));
      r += 8;
      n -= 8;
   }
   while (n-- > 0)
   {
      m = m * 10 + (uint64_t)(*r - '0');
      r++;
   }
   *mant = m;
   return r;
}

/* True when any byte in [p, end) is a digit other than '0'. The ranges
 * this sees were already verified to be digits. */
static int rstrtod_range_nonzero(const char *p, const char *end)
{
   while ((end - p) >= 8)
   {
      uint64_t w = rstrtod_read8(p);
      if (w != RSTRTOD_U64(0x3030303030303030))
         return 1;
      p += 8;
   }
   while (p < end)
   {
      if (*p != '0')
         return 1;
      p++;
   }
   return 0;
}

/* ---------------------------------------------------------------- */
/* parsing                                                          */
/* ---------------------------------------------------------------- */

struct rstrtod_parsed
{
   uint64_t    mantissa;      /* first 19 significant digits         */
   int         exponent;      /* power of ten applying to mantissa   */
   int         negative;
   int         truncated;     /* digits beyond the 19 were dropped   */
   int         valid;
   int         special;       /* 0 none, 1 infinity, 2 nan           */
};

static int rstrtod_lower_eq(const char *s, size_t len, const char *word,
      size_t wlen)
{
   size_t i;
   if (len < wlen)
      return 0;
   for (i = 0; i < wlen; i++)
   {
      char c = s[i];
      if (c >= 'A' && c <= 'Z')
         c = (char)(c - 'A' + 'a');
      if (c != word[i])
         return 0;
   }
   return 1;
}

static RSTRTOD_FLATTEN void rstrtod_parse(const char *s, size_t len, struct rstrtod_parsed *p,
      size_t *consumed)
{
   const char *start = s;
   const char *end   = s + len;
   const char *q;
   const char *int_start;
   const char *int_end;
   const char *frac_start;
   const char *frac_end;
   int         digit_count;
   /* The accumulators live in locals rather than in *p for the duration
    * of the scan. Reading the input through a char pointer may alias
    * anything, so a store into the struct forces the compiler to spill
    * and reload on every digit -- visible in the generated code as a
    * load and a store around each multiply-add. */
   uint64_t    mantissa    = 0;
   int         exponent    = 0;
   int         truncated   = 0;

   /* Every field a caller can reach is set here, once, before the
    * scan: valid gates the rest, the sign and the special tag are
    * read on the inf/nan paths, and the accumulators are written
    * again on the way out of the digit path. Seeding them costs
    * three stores outside the digit loop and lets a compiler that
    * cannot follow the valid gate see the struct as fully defined
    * rather than warn about it. The caller initialises *consumed. */
   p->mantissa    = 0;
   p->exponent    = 0;
   p->negative    = 0;
   p->truncated   = 0;
   p->valid       = 0;
   p->special     = 0;

   q = s;

   /* A leading digit rules out every one of leading space, a sign,
    * "inf" and "nan" at once, so the whole prologue can be stepped
    * over. Most callers hand over a bare number and pay one test for
    * the eight the general case needs. */
   if (q < end && RSTRTOD_IS_DIGIT(*q))
      goto digits;

   while (q < end && (*q == ' ' || *q == '\t' || *q == '\n' || *q == '\r'
            || *q == '\v' || *q == '\f'))
      q++;

   if (q < end && (*q == '+' || *q == '-'))
   {
      p->negative = (*q == '-');
      q++;
   }

   if (q < end && (*q == 'i' || *q == 'I'))
   {
      if (rstrtod_lower_eq(q, (size_t)(end - q), "infinity", 8))
      {
         p->special = 1;
         p->valid   = 1;
         *consumed  = (size_t)(q + 8 - start);
         return;
      }
      if (rstrtod_lower_eq(q, (size_t)(end - q), "inf", 3))
      {
         p->special = 1;
         p->valid   = 1;
         *consumed  = (size_t)(q + 3 - start);
         return;
      }
      return;
   }
   if (q < end && (*q == 'n' || *q == 'N'))
   {
      if (rstrtod_lower_eq(q, (size_t)(end - q), "nan", 3))
      {
         p->special = 2;
         p->valid   = 1;
         *consumed  = (size_t)(q + 3 - start);
         return;
      }
      return;
   }

   /* The hot loops accumulate every digit and test nothing else: the
    * mantissa is allowed to wrap, which unsigned arithmetic defines,
    * and the digit count falls out of pointer differences afterwards.
    * Only when more than nineteen significant digits turn out to exist
    * -- the sole case where the wrap could have happened -- is the
    * mantissa rebuilt, and that rescan also learns whether anything
    * non-zero was dropped. Config files never take that path; hostile
    * input pays one extra pass over at most the digits it supplied. */
digits:
   int_start = q;
   while ((end - q) >= 8)
   {
      uint64_t w = rstrtod_read8(q);
      if (!rstrtod_is_eight_digits(w))
         break;
      mantissa = mantissa * RSTRTOD_U64(100000000)
         + rstrtod_parse_eight_digits(w);
      q += 8;
   }
   while (q < end && RSTRTOD_IS_DIGIT(*q))
   {
      mantissa = mantissa * 10 + (uint64_t)(*q - '0');
      q++;
   }
   int_end = q;

   if (q < end && *q == '.')
   {
      q++;
      frac_start = q;
      while ((end - q) >= 8)
      {
         uint64_t w = rstrtod_read8(q);
         if (!rstrtod_is_eight_digits(w))
            break;
         mantissa = mantissa * RSTRTOD_U64(100000000)
            + rstrtod_parse_eight_digits(w);
         q += 8;
      }
      while (q < end && RSTRTOD_IS_DIGIT(*q))
      {
         mantissa = mantissa * 10 + (uint64_t)(*q - '0');
         q++;
      }
      frac_end = q;
      exponent = (int)(frac_start - frac_end);
   }
   else
   {
      frac_start = q;
      frac_end   = q;
   }

   digit_count = (int)(int_end - int_start) + (int)(frac_end - frac_start);
   if (!digit_count)
      return;

   if (digit_count > 19)
   {
      /* Leading zeros were counted above but are not significant. */
      const char *pp = int_start;
      while (pp < frac_end && (*pp == '0' || *pp == '.'))
      {
         if (*pp == '0')
            digit_count--;
         pp++;
      }

      if (digit_count > 19)
      {
         /* The accumulator wrapped. Rebuild the first nineteen
          * significant digits and let the ranges left over say what
          * the dropped tail contained. */
         const char *r;
         int         have = 0;

         mantissa = 0;
         r = rstrtod_take19(pp, (int_end > pp) ? int_end : pp,
               &mantissa, &have);
         if (have < 19)
         {
            /* The point was crossed: continue in the fraction. (r may
             * already sit inside it when the whole integer part was
             * zeros.) */
            if (r < frac_start)
               r = frac_start;
            r = rstrtod_take19(r, frac_end, &mantissa, &have);
            exponent = (int)(frac_start - r);
            if (rstrtod_range_nonzero(r, frac_end))
               truncated = 1;
         }
         else
         {
            /* Nineteen digits inside the integer part alone: whatever
             * remains of it scales the value, and the whole fraction
             * is dropped. */
            exponent = (int)(int_end - r);
            if (rstrtod_range_nonzero(r, int_end)
                  || rstrtod_range_nonzero(frac_start, frac_end))
               truncated = 1;
         }
      }
   }

   p->mantissa  = mantissa;
   p->truncated = truncated;

   *consumed = (size_t)(q - start);

   if (q < end && (*q == 'e' || *q == 'E'))
   {
      const char *e     = q + 1;
      int         esign = 0;
      int         eval  = 0;
      int         edig  = 0;

      if (e < end && (*e == '+' || *e == '-'))
      {
         esign = (*e == '-');
         e++;
      }
      /* Eight digits already exceed any float's range, so the value
       * loop can stop clamping and run clean; anything longer only
       * needs consuming. */
      {
         const char *estart = e;
         const char *vstart;
         while (e < end && *e == '0')
            e++;
         vstart = e;
         while (e < end && (e - vstart) < 8 && RSTRTOD_IS_DIGIT(*e))
         {
            eval = eval * 10 + (*e - '0');
            e++;
         }
         while (e < end && RSTRTOD_IS_DIGIT(*e))
         {
            eval = 100000000;
            e++;
         }
         edig = (int)(e - estart);
      }
      /* an 'e' with no digits after it is not part of the number */
      if (edig)
      {
         exponent += esign ? -eval : eval;
         *consumed    = (size_t)(e - start);
      }
   }

   p->exponent = exponent;
   p->valid    = 1;
}

/* Rebuilds the digit string for the decimal path. The parse above kept
 * only 19 digits; here every digit matters. */
static void rstrtod_fill_decimal(const char *s, size_t len,
      struct rstrtod_decimal *d)
{
   const char *q   = s;
   const char *end = s + len;
   int         after_point = 0;
   int         seen_nonzero = 0;

   d->num_digits    = 0;
   d->decimal_point = 0;
   d->truncated     = 0;

   while (q < end && (*q == ' ' || *q == '\t' || *q == '\n' || *q == '\r'
            || *q == '\v' || *q == '\f'))
      q++;
   if (q < end && (*q == '+' || *q == '-'))
      q++;

   while (q < end)
   {
      char c = *q;
      if (c == '.' && !after_point)
      {
         after_point = 1;
         q++;
         continue;
      }
      if (c < '0' || c > '9')
         break;
      if (c == '0' && !seen_nonzero)
      {
         if (after_point)
            d->decimal_point--;
      }
      else
      {
         seen_nonzero = 1;
         if (d->num_digits < RSTRTOD_MAX_DIGITS)
            d->digits[d->num_digits++] = (uint8_t)(c - '0');
         else if (c != '0')
            d->truncated = 1;
         if (!after_point)
            d->decimal_point++;
      }
      q++;
   }
   if (seen_nonzero && !after_point)
   {
      /* decimal_point counted the integer digits already */
   }
   else if (seen_nonzero && after_point)
   {
      /* decimal_point holds the negative offset of the first digit */
   }

   if (q < end && (*q == 'e' || *q == 'E'))
   {
      const char *e = q + 1;
      int esign = 0, eval = 0, edig = 0;
      if (e < end && (*e == '+' || *e == '-'))
      {
         esign = (*e == '-');
         e++;
      }
      /* Eight digits already exceed any float's range, so the value
       * loop can stop clamping and run clean; anything longer only
       * needs consuming. */
      {
         const char *estart = e;
         const char *vstart;
         while (e < end && *e == '0')
            e++;
         vstart = e;
         while (e < end && (e - vstart) < 8 && RSTRTOD_IS_DIGIT(*e))
         {
            eval = eval * 10 + (*e - '0');
            e++;
         }
         while (e < end && RSTRTOD_IS_DIGIT(*e))
         {
            eval = 100000000;
            e++;
         }
         edig = (int)(e - estart);
      }
      if (edig)
         d->decimal_point += esign ? -eval : eval;
   }

   while (d->num_digits > 0 && d->digits[d->num_digits - 1] == 0)
      d->num_digits--;
}

/* ---------------------------------------------------------------- */
/* assembly                                                         */
/* ---------------------------------------------------------------- */

static double rstrtod_bits_to_double(uint64_t bits, int negative)
{
   double d;
   if (negative)
      bits |= RSTRTOD_U64(0x8000000000000000);
   memcpy(&d, &bits, sizeof(d));
   return d;
}

static float rstrtod_bits_to_float(uint64_t bits, int negative)
{
   float    f;
   uint32_t b = (uint32_t)bits;
   if (negative)
      b |= 0x80000000u;
   memcpy(&f, &b, sizeof(f));
   return f;
}

static RSTRTOD_FLATTEN uint64_t rstrtod_convert(const struct rstrtod_format *fmt,
      const struct rstrtod_parsed *p, const char *s, size_t len)
{
   uint64_t bits;
   struct rstrtod_decimal dec;

   if (!p->truncated)
   {
      if (rstrtod_eisel_lemire(fmt, p->mantissa, p->exponent, &bits))
         return bits;
   }
   else
   {
      /* Digits were dropped, so the true value sits between mantissa
       * and mantissa+1 scaled by the same power. When both ends round
       * to the same double the dropped digits cannot matter, which is
       * the common case and keeps the decimal path for genuinely
       * ambiguous input only. */
      uint64_t lo;
      uint64_t hi;
      if (rstrtod_eisel_lemire(fmt, p->mantissa, p->exponent, &lo)
            && rstrtod_eisel_lemire(fmt, p->mantissa + 1, p->exponent, &hi)
            && lo == hi)
         return lo;
   }

   rstrtod_fill_decimal(s, len, &dec);
   return rstrtod_decimal_to_bits(fmt, &dec);
}

double rstrtod_len(const char *str, size_t len, size_t *end)
{
   struct rstrtod_parsed p;
   size_t                consumed = 0;
   uint64_t              bits;

   if (!str)
   {
      if (end)
         *end = 0;
      return 0.0;
   }

   rstrtod_parse(str, len, &p, &consumed);
   if (end)
      *end = p.valid ? consumed : 0;
   if (!p.valid)
      return 0.0;

   if (p.special == 1)
      return p.negative ? -HUGE_VAL : HUGE_VAL;
   if (p.special == 2)
   {
      uint64_t nan_bits = RSTRTOD_U64(0x7FF8000000000000);
      return rstrtod_bits_to_double(nan_bits, p.negative);
    }

   /* the exact path */
   if (!p.truncated
         && p.mantissa <= rstrtod_fmt_double.max_mantissa_fast
         && p.exponent >= -rstrtod_fmt_double.max_exponent_fast
         && p.exponent <= rstrtod_fmt_double.max_exponent_fast)
   {
      double v = (double)p.mantissa;
      if (p.exponent < 0)
         v = v / rstrtod_pow10[-p.exponent];
      else
         v = v * rstrtod_pow10[p.exponent];
      return p.negative ? -v : v;
   }

   bits = rstrtod_convert(&rstrtod_fmt_double, &p, str, len);
   return rstrtod_bits_to_double(bits, p.negative);
}

float rstrtof_len(const char *str, size_t len, size_t *end)
{
   struct rstrtod_parsed p;
   size_t                consumed = 0;
   uint64_t              bits;

   if (!str)
   {
      if (end)
         *end = 0;
      return 0.0f;
   }

   rstrtod_parse(str, len, &p, &consumed);
   if (end)
      *end = p.valid ? consumed : 0;
   if (!p.valid)
      return 0.0f;

   if (p.special == 1)
      return p.negative ? -(float)HUGE_VAL : (float)HUGE_VAL;
   if (p.special == 2)
      return rstrtod_bits_to_float(0x7FC00000u, p.negative);

   if (!p.truncated
         && p.mantissa <= rstrtod_fmt_float.max_mantissa_fast
         && p.exponent >= -rstrtod_fmt_float.max_exponent_fast
         && p.exponent <= rstrtod_fmt_float.max_exponent_fast)
   {
      float v = (float)p.mantissa;
      if (p.exponent < 0)
         v = v / (float)rstrtod_pow10[-p.exponent];
      else
         v = v * (float)rstrtod_pow10[p.exponent];
      return p.negative ? -v : v;
   }

   bits = rstrtod_convert(&rstrtod_fmt_float, &p, str, len);
   return rstrtod_bits_to_float(bits, p.negative);
}

double rstrtod(const char *str, char **end)
{
   size_t n = 0;
   double v;
   if (!str)
   {
      if (end)
         *end = NULL;
      return 0.0;
   }
   v = rstrtod_len(str, strlen(str), &n);
   if (end)
      *end = (char*)str + n;
   return v;
}

float rstrtof(const char *str, char **end)
{
   size_t n = 0;
   float  v;
   if (!str)
   {
      if (end)
         *end = NULL;
      return 0.0f;
   }
   v = rstrtof_len(str, strlen(str), &n);
   if (end)
      *end = (char*)str + n;
   return v;
}
