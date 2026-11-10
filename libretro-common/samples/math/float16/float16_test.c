/* math/float16.h against the definition and against the hardware.
 *
 * Every half widens to the value it encodes (computed with ldexp),
 * signs, infinities and NaNs included, and narrows back to itself (a
 * signalling NaN quiet). Floats narrow as F16C's VCVTPS2PH does (round
 * to nearest even) where the CPU has it: every one whose exponent
 * lands in or near a half's range, of either sign, and a stride of the
 * rest; a stride is also checked against the nearest half found by
 * search. The functions use integer
 * arithmetic only, so they are checked again with the FPU flushing
 * subnormals where SSE allows it. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>

#include <math/float16.h>

#if defined(__SSE2__) || defined(__x86_64__)
#include <xmmintrin.h>
#define HAVE_MXCSR 1
#endif
#if (defined(__x86_64__) || defined(__i386__)) && (defined(__GNUC__) || defined(__clang__))
#include <immintrin.h>
#define HAVE_F16C_PROBE 1
#endif

static int fails;

static void check(const char *what, int ok)
{
   printf("  %-60s %s\n", what, ok ? "ok" : "FAIL");
   if (!ok)
      fails++;
}

static uint32_t bits_of(float f)
{
   uint32_t u;
   memcpy(&u, &f, sizeof(u));
   return u;
}

static float float_of(uint32_t u)
{
   float f;
   memcpy(&f, &u, sizeof(f));
   return f;
}

/* The value a half encodes, from the definition. */
static double half_value(uint16_t h)
{
   int    exp  = (h >> 10) & 0x1F;
   int    mant = h & 0x3FF;
   double v    = exp ? ldexp(1024 + mant, exp - 25) : ldexp(mant, -24);
   return (h & 0x8000) ? -v : v;
}

static int widen_all(void)
{
   unsigned h;
   int bad = 0;
   for (h = 0; h < 0x10000; h++)
   {
      float f   = retro_half_to_float((uint16_t)h);
      int   exp = (h >> 10) & 0x1F;
      if (exp == 0x1F)
      {
         uint32_t want = ((uint32_t)(h & 0x8000) << 16) | 0x7F800000u
               | ((uint32_t)(h & 0x3FF) << 13);
         if (bits_of(f) != want)
            bad++;
      }
      else if ((double)f != half_value((uint16_t)h)
            || ((h & 0x8000) != 0) != ((bits_of(f) >> 31) != 0))
         bad++;
   }
   return bad;
}

static int round_trip_all(void)
{
   unsigned h;
   int bad = 0;
   for (h = 0; h < 0x10000; h++)
   {
      /* A signalling NaN comes back quiet, as through F16C. */
      unsigned want = ((h & 0x7C00) == 0x7C00 && (h & 0x3FF)) ? (h | 0x200) : h;
      if (retro_float_to_half(retro_half_to_float((uint16_t)h)) != want)
         bad++;
   }
   return bad;
}

/* The nearest half by search over the positive finite halves, which
 * are ordered as their bit patterns are; ties to the even one. */
static uint16_t nearest_half(float f)
{
   uint32_t u = bits_of(f);
   uint16_t sign = (uint16_t)((u >> 16) & 0x8000u);
   double   v = fabs((double)f);
   unsigned lo = 0, hi = 0x7BFF;
   if (f != f)
      return (uint16_t)(sign | 0x7E00u | ((u & 0x7FFFFFu) >> 13));
   if (v >= 65520.0) /* half of the way past the largest, 65504 */
      return (uint16_t)(sign | 0x7C00u);
   while (lo < hi)
   {
      unsigned mid = (lo + hi + 1) / 2;
      if (half_value((uint16_t)mid) <= v)
         lo = mid;
      else
         hi = mid - 1;
   }
   if (lo < 0x7BFF)
   {
      double below = v - half_value((uint16_t)lo);
      double above = half_value((uint16_t)(lo + 1)) - v;
      if (above < below || (above == below && (lo & 1)))
         lo++;
   }
   return (uint16_t)(sign | lo);
}

#ifdef HAVE_F16C_PROBE
__attribute__((target("f16c")))
static uint16_t hw_half(float f)
{
   return (uint16_t)_cvtss_sh(f, 0); /* round to nearest even */
}

static int narrow_one(uint32_t u)
{
   float f = float_of(u);
   if (retro_float_to_half(f) == hw_half(f))
      return 0;
   printf("    %08x: %04x, F16C %04x\n", (unsigned)u, retro_float_to_half(f),
         hw_half(f));
   return 1;
}

static int narrow_all_hw(void)
{
   uint64_t i;
   int bad = 0;
   /* Exponents 101 to 143: below them all round to zero, above to
    * infinity. Both signs. */
   for (i = (uint64_t)101 << 23; i < (uint64_t)144 << 23 && bad < 4; i++)
      bad += narrow_one((uint32_t)i) + narrow_one((uint32_t)i | 0x80000000u);
   for (i = 0; i <= 0xFFFFFFFFu && bad < 4; i += 257)
      bad += narrow_one((uint32_t)i);
   return bad;
}
#endif

static int narrow_stride(void)
{
   uint64_t i;
   int bad = 0;
   for (i = 0; i <= 0xFFFFFFFFu; i += 4099)
   {
      float f = float_of((uint32_t)i);
      if (retro_float_to_half(f) != nearest_half(f) && bad++ < 4)
         printf("    %08x: %04x, nearest %04x\n", (unsigned)i,
               retro_float_to_half(f), nearest_half(f));
   }
   return bad;
}

int main(void)
{
   int hw = 0;

   check("every half widens to its value", widen_all() == 0);
   check("every half narrows back to itself", round_trip_all() == 0);
   check("a stride of floats rounds to the nearest half", narrow_stride() == 0);
   check("ties go to even: 2^-25 to zero, 2049 to 2048",
         retro_float_to_half(ldexpf(1.0f, -25)) == 0
         && retro_float_to_half(2049.0f) == 0x6800
         && retro_float_to_half(2051.0f) == 0x6802);
   check("the largest half, and infinity just past it",
         retro_float_to_half(65504.0f) == 0x7BFF
         && retro_float_to_half(65519.0f) == 0x7BFF
         && retro_float_to_half(65520.0f) == 0x7C00
         && retro_float_to_half(-1e30f) == 0xFC00);
   check("a NaN stays a NaN",
         (retro_float_to_half(float_of(0x7F800001u)) & 0x7FFF) > 0x7C00);

#ifdef HAVE_F16C_PROBE
   hw = __builtin_cpu_supports("f16c");
   if (hw)
      check("floats in and near the half range narrow as F16C does", narrow_all_hw() == 0);
#endif
   if (!hw)
      printf("  (no F16C here: the full comparison is skipped)\n");

#ifdef HAVE_MXCSR
   /* Flush-to-zero and denormals-are-zero change nothing. */
   _mm_setcsr(_mm_getcsr() | 0x8040);
   check("with FTZ and DAZ: halves widen the same",
         widen_all() == 0 && round_trip_all() == 0);
   check("with FTZ and DAZ: floats narrow the same",
         narrow_stride() == 0);
#endif

   printf("float16_test: %s (%d failure%s)\n", fails ? "FAIL" : "PASS",
         fails, fails == 1 ? "" : "s");
   return fails ? 1 : 0;
}
