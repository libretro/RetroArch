/* Checks rgba16_pack() (gfx/common/rgba16_pack.h) against a reference.
 *
 * The vertex colour streams hand the GPU this word, so a wrong channel
 * is a wrong colour on screen and nothing else notices. Held here:
 *  - every 10-bit level k/1023 comes back as k when the channel is read
 *    at 10 bits, which is what a fade on a 10-bit or HDR swapchain sees;
 *    an 8-bit vertex colour fails this (the menus' high-precision text
 *    colour is only worth anything if this holds);
 *  - every 8-bit level k/255 packs to k * 257, exactly;
 *  - red is the lowest-addressed channel, each channel in native byte
 *    order, which is what R16G16B16A16_UNORM reads;
 *  - the rounding boundaries either side of (k + 0.5) / 65535;
 *  - rgba16_white(a) is rgba16_pack(1, 1, 1, a);
 *  - out-of-range input saturates: negatives, -0, past 1, past int's
 *    range, both infinities; a NaN is 0;
 *  - a million random colours across -0.5..1.5, each channel checked.
 * The same test runs on the scalar path (RGBA16_PACK_NO_SIMD) and on
 * whichever vector path the target compiles, so the paths agree. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>

#include "gfx/common/rgba16_pack.h"

static unsigned failures;
static unsigned long checks;

static unsigned ref(float c)
{
   float t = c * 65535.0f;
   t      += 0.5f;
   if (!(t > 0.0f))
      return 0;
   if (t >= 65535.0f)
      return 65535;
   return (unsigned)t;
}

static void unpack(const float *c, uint16_t *ch)
{
   uint64_t w = rgba16_pack(c);
   memcpy(ch, &w, 8);
}

static void check4(const float *c, const char *what)
{
   uint16_t ch[4];
   int      i;
   unpack(c, ch);
   checks++;
   for (i = 0; i < 4; i++)
   {
      if (ch[i] != ref(c[i]))
      {
         if (failures < 20)
            printf("   FAIL %s: channel %d of (%.9g %.9g %.9g %.9g)"
                  " packed %u, want %u\n", what, i,
                  c[0], c[1], c[2], c[3], ch[i], ref(c[i]));
         failures++;
      }
   }
}

/* One value in every channel position, the others held at distinct
 * levels so a channel landing in the wrong place shows. */
static void check1(float v, const char *what)
{
   float c[4];
   int   i;
   for (i = 0; i < 4; i++)
   {
      c[0] = 1000.0f  / 65535.0f;
      c[1] = 17000.0f / 65535.0f;
      c[2] = 33000.0f / 65535.0f;
      c[3] = 49000.0f / 65535.0f;
      c[i] = v;
      check4(c, what);
   }
}

/* The float one step away from a positive v; C89 has no nextafterf. */
static float ulp_step(float v, int dir)
{
   uint32_t bits;
   memcpy(&bits, &v, 4);
   bits = (uint32_t)(bits + dir);
   memcpy(&v, &bits, 4);
   return v;
}

static uint32_t rng_hi = 0x9E3779B9u, rng_lo = 0x7F4A7C15u;
static float rnd(void)
{
   /* xorshift64 on two 32-bit halves: C89 has no 64-bit literal. */
   uint32_t hi = rng_hi, lo = rng_lo, thi, tlo;
   thi = (hi << 13) | (lo >> 19); tlo = lo << 13; hi ^= thi; lo ^= tlo;
   tlo = (lo >> 7) | (hi << 25);  thi = hi >> 7;  hi ^= thi; lo ^= tlo;
   thi = (hi << 17) | (lo >> 15); tlo = lo << 17; hi ^= thi; lo ^= tlo;
   rng_hi = hi; rng_lo = lo;
   return (float)((double)(hi >> 8) / 16777216.0) * 2.0f - 0.5f;
}

int main(void)
{
   int  k;
   long n;
   const char *path =
#if defined(RGBA16_PACK_SSE2)
      "sse2";
#elif defined(RGBA16_PACK_NEON)
      "neon";
#else
      "scalar";
#endif
   printf("rgba16_pack, %s path\n", path);

   /* The layout: channels in memory are r, g, b, a. */
   {
      float    c[4];
      uint16_t ch[4];
      c[0] = 1.0f / 65535.0f; c[1] = 2.0f / 65535.0f;
      c[2] = 3.0f / 65535.0f; c[3] = 4.0f / 65535.0f;
      unpack(c, ch);
      if (ch[0] != 1 || ch[1] != 2 || ch[2] != 3 || ch[3] != 4)
      {
         printf("   FAIL layout: channels %u %u %u %u, want 1 2 3 4\n",
               ch[0], ch[1], ch[2], ch[3]);
         failures++;
      }
      c[0] = c[1] = c[2] = c[3] = 1.0f;
      if (rgba16_pack(c) != RGBA16_WHITE)
      {
         printf("   FAIL white: 1,1,1,1 is not RGBA16_WHITE\n");
         failures++;
      }
   }

   /* rgba16_white(a) is rgba16_pack(1, 1, 1, a), specials included. */
   {
      static const float special[] = { 0.0f, -0.0f, 1.0f, -1.0f, 1.5f,
         1e10f, -1e10f, FLT_MAX };
      float c[4];
      c[0] = c[1] = c[2] = 1.0f;
      for (k = 0; k < 70000 + 8 + 3; k++)
      {
         if (k < 70000)
            c[3] = ((float)k - 2000.0f) / 65535.0f;
         else if (k < 70008)
            c[3] = special[k - 70000];
         else if (k == 70008)
            c[3] = (float)HUGE_VAL;
         else if (k == 70009)
            c[3] = -(float)HUGE_VAL;
         else
            c[3] = (float)(0.0 * HUGE_VAL);
         checks++;
         if (rgba16_white(c[3]) != rgba16_pack(c))
         {
            if (failures < 20)
               printf("   FAIL white at alpha %.9g\n", c[3]);
            failures++;
         }
      }
   }

   /* A 10-bit target reads the channel as round(v * 1023 / 65535). */
   for (k = 0; k < 1024; k++)
   {
      float    c[4];
      uint16_t ch[4];
      unsigned back;
      c[0] = c[1] = c[2] = c[3] = (float)k / 1023.0f;
      unpack(c, ch);
      back = (unsigned)((ch[3] * 1023.0 / 65535.0) + 0.5);
      checks++;
      if (back != (unsigned)k)
      {
         if (failures < 20)
            printf("   FAIL 10-bit level %d reads back as %u\n", k, back);
         failures++;
      }
   }

   /* Every 8-bit level exactly, the 16-bit boundaries around each, and
    * a sweep of the 16-bit levels. */
   for (k = 0; k < 256; k++)
   {
      float    c[4];
      uint16_t ch[4];
      c[0] = c[1] = c[2] = c[3] = (float)k / 255.0f;
      unpack(c, ch);
      checks++;
      if (ch[0] != k * 257 || ch[3] != k * 257)
      {
         printf("   FAIL 8-bit level %d packed to %u\n", k, ch[0]);
         failures++;
      }
   }
   for (k = 0; k < 65536; k += 7)
   {
      float mid = ((float)k + 0.5f) / 65535.0f;
      check1((float)k / 65535.0f, "level");
      check1(mid, "boundary");
      check1(ulp_step(mid, -1), "below boundary");
      check1(ulp_step(mid, 1), "above boundary");
   }

   /* Out of range. */
   check1(-0.0f, "-0");
   check1(-1e-9f, "just negative");
   check1(-1.0f, "-1");
   check1(1.0f, "1");
   check1(ulp_step(1.0f, 1), "just past 1");
   check1(1.5f, "1.5");
   check1(65536.0f, "65536");
   check1(1e10f, "past int");
   check1(-1e10f, "past -int");
   check1(FLT_MAX, "FLT_MAX");
   check1(-FLT_MAX, "-FLT_MAX");
   check1((float)HUGE_VAL, "+inf");
   check1(-(float)HUGE_VAL, "-inf");
   check1((float)(0.0 * HUGE_VAL), "NaN");

   for (n = 0; n < 1000000; n++)
   {
      float c[4];
      c[0] = rnd(); c[1] = rnd(); c[2] = rnd(); c[3] = rnd();
      check4(c, "random");
   }

   if (failures)
   {
      printf("rgba16_pack (%s): %u failure(s) in %lu checks\n", path,
            failures, checks);
      return 1;
   }
   printf("rgba16_pack (%s): %lu checks match the reference\n", path,
         checks);
   return 0;
}
