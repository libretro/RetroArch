/* Checks rgba8_pack() (gfx/common/rgba8_pack.h) against a reference.
 *
 * The vertex colour streams hand the GPU this word, so a wrong byte is
 * a wrong colour on screen and nothing else notices. Held here:
 *  - every 8-bit level k/255 packs back to k, which is every theme
 *    colour and every alpha step the menus use;
 *  - red is the lowest-addressed byte, which is what R8G8B8A8_UNORM
 *    reads, whatever the host's endianness;
 *  - the rounding boundaries either side of each (k + 0.5) / 255;
 *  - out-of-range input saturates: negatives, -0, past 1, past int's
 *    range, both infinities; a NaN is 0;
 *  - a million random colours across -0.5..1.5, each channel checked.
 * The same test runs on the scalar path (RGBA8_PACK_NO_SIMD) and on
 * whichever vector path the target compiles, so the paths agree. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>

#include "gfx/common/rgba8_pack.h"

static unsigned failures;
static unsigned long checks;

static unsigned ref(float c)
{
   float t = c * 255.0f;
   t      += 0.5f;
   if (!(t > 0.0f))
      return 0;
   if (t >= 255.0f)
      return 255;
   return (unsigned)t;
}

static void check4(const float *c, const char *what)
{
   uint32_t      w = rgba8_pack(c);
   unsigned char b[4];
   int           i;
   memcpy(b, &w, 4);
   checks++;
   for (i = 0; i < 4; i++)
   {
      if (b[i] != ref(c[i]))
      {
         if (failures < 20)
            printf("   FAIL %s: channel %d of (%.9g %.9g %.9g %.9g)"
                  " packed %u, want %u\n", what, i,
                  c[0], c[1], c[2], c[3], b[i], ref(c[i]));
         failures++;
      }
   }
}

/* One value in every channel position, the others held at distinct
 * levels so a channel landing in the wrong byte shows. */
static void check1(float v, const char *what)
{
   float c[4];
   int   i;
   for (i = 0; i < 4; i++)
   {
      c[0] = 10.0f / 255.0f;
      c[1] = 70.0f / 255.0f;
      c[2] = 130.0f / 255.0f;
      c[3] = 190.0f / 255.0f;
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

static unsigned long long rng = 0x9E3779B97F4A7C15ULL;
static float rnd(void)
{
   rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17;
   return (float)((double)(rng >> 11) / 9007199254740992.0) * 2.0f - 0.5f;
}

int main(void)
{
   int  k;
   long n;
   const char *path =
#if defined(RGBA8_PACK_SSE2)
      "sse2";
#elif defined(RGBA8_PACK_NEON)
      "neon";
#else
      "scalar";
#endif
   printf("rgba8_pack, %s path\n", path);

   /* The layout: bytes in memory are r, g, b, a. */
   {
      float         c[4];
      uint32_t      w;
      unsigned char b[4];
      c[0] = 1.0f / 255.0f; c[1] = 2.0f / 255.0f;
      c[2] = 3.0f / 255.0f; c[3] = 4.0f / 255.0f;
      w    = rgba8_pack(c);
      memcpy(b, &w, 4);
      if (b[0] != 1 || b[1] != 2 || b[2] != 3 || b[3] != 4)
      {
         printf("   FAIL layout: bytes %u %u %u %u, want 1 2 3 4\n",
               b[0], b[1], b[2], b[3]);
         failures++;
      }
      c[0] = c[1] = c[2] = c[3] = 1.0f;
      if (rgba8_pack(c) != RGBA8_WHITE)
      {
         printf("   FAIL white: 1,1,1,1 is not RGBA8_WHITE\n");
         failures++;
      }
   }

   /* Every level, and both sides of every boundary. */
   for (k = 0; k < 256; k++)
   {
      float level = (float)k / 255.0f;
      float mid   = ((float)k + 0.5f) / 255.0f;
      float c[4];
      uint32_t      w;
      unsigned char b[4];
      c[0] = c[1] = c[2] = c[3] = level;
      w    = rgba8_pack(c);
      memcpy(b, &w, 4);
      if (b[0] != k || b[1] != k || b[2] != k || b[3] != k)
      {
         printf("   FAIL level %d packed to %u\n", k, b[0]);
         failures++;
      }
      check1(level, "level");
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
   check1(256.0f, "256");
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
      printf("rgba8_pack (%s): %u failure(s) in %lu packs\n", path,
            failures, checks);
      return 1;
   }
   printf("rgba8_pack (%s): %lu packs match the reference\n", path, checks);
   return 0;
}
