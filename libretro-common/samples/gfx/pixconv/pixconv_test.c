/* Exhaustive check of the pixconv converters against per-channel
 * reference conversions.  Every 16-bit input value is converted, in
 * rows whose width is not a multiple of any SIMD block so both the
 * vector body and the scalar tail are exercised.  The Makefile
 * also builds it without SSE2 and without any SIMD, so the MMX and
 * whole-row scalar paths are covered too.  Returns 0 when every
 * pixel matches. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <gfx/scaler/pixconv.h>

#define ROW_W 263
#define ROWS  ((0x10000 + ROW_W - 1) / ROW_W)
#define NPIX  (ROW_W * ROWS)

static unsigned failures;

static uint32_t x5(uint32_t v) { return (v << 3) | (v >> 2); }
static uint32_t x6(uint32_t v) { return (v << 2) | (v >> 4); }
static uint32_t x4(uint32_t v) { return (v << 4) | v; }

static uint32_t ref_565_rgb(uint32_t c)
{
   return (x5((c >> 11) & 0x1f) << 16) | (x6((c >> 5) & 0x3f) << 8)
         | x5(c & 0x1f);
}

static uint32_t ref_565_bgr(uint32_t c)
{
   return (x5(c & 0x1f) << 16) | (x6((c >> 5) & 0x3f) << 8)
         | x5((c >> 11) & 0x1f);
}

static uint32_t ref_1555_rgb(uint32_t c)
{
   return (x5((c >> 10) & 0x1f) << 16) | (x5((c >> 5) & 0x1f) << 8)
         | x5(c & 0x1f);
}

static uint32_t ref_1555_bgr(uint32_t c)
{
   return (x5(c & 0x1f) << 16) | (x5((c >> 5) & 0x1f) << 8)
         | x5((c >> 10) & 0x1f);
}

static uint32_t ref_rgba4444_argb(uint32_t c)
{
   return (x4(c & 0xf) << 24) | (x4((c >> 12) & 0xf) << 16)
         | (x4((c >> 8) & 0xf) << 8) | x4((c >> 4) & 0xf);
}

static uint32_t ref_argb4444_argb(uint32_t c)
{
   return (x4((c >> 12) & 0xf) << 24) | (x4((c >> 8) & 0xf) << 16)
         | (x4((c >> 4) & 0xf) << 8) | x4(c & 0xf);
}

static uint32_t ref_argb4444_abgr(uint32_t c)
{
   return (x4((c >> 12) & 0xf) << 24) | (x4(c & 0xf) << 16)
         | (x4((c >> 4) & 0xf) << 8) | x4((c >> 8) & 0xf);
}

static void check(const char *what, unsigned idx,
      uint32_t got, uint32_t want)
{
   if (got == want)
      return;
   if (failures < 16)
      printf("FAIL %s: input %u got %08lx want %08lx\n", what, idx,
            (unsigned long)got, (unsigned long)want);
   failures++;
}

static void test_helpers(void)
{
   uint32_t c;
   for (c = 0; c < 0x10000; c++)
   {
      check("pixconv_rgb565_to_xrgb8888", c,
            pixconv_rgb565_to_xrgb8888(c), ref_565_rgb(c));
      check("pixconv_rgb565_to_xbgr8888", c,
            pixconv_rgb565_to_xbgr8888(c), ref_565_bgr(c));
      check("pixconv_0rgb1555_to_xrgb8888", c,
            pixconv_0rgb1555_to_xrgb8888(c), ref_1555_rgb(c));
      check("pixconv_0rgb1555_to_xbgr8888", c,
            pixconv_0rgb1555_to_xbgr8888(c), ref_1555_bgr(c));
      check("pixconv_rgba4444_to_argb8888", c,
            pixconv_rgba4444_to_argb8888(c), ref_rgba4444_argb(c));
      check("pixconv_argb4444_to_argb8888", c,
            pixconv_argb4444_to_argb8888(c), ref_argb4444_argb(c));
      check("pixconv_argb4444_to_abgr8888", c,
            pixconv_argb4444_to_abgr8888(c), ref_argb4444_abgr(c));
   }
}

typedef void (*conv_fn)(void *, const void *, int, int, int, int);

static void test_16_to_32(const char *name, conv_fn fn,
      const uint16_t *in, uint32_t *out,
      uint32_t (*ref)(uint32_t), uint32_t alpha)
{
   unsigned i;
   memset(out, 0x5a, NPIX * sizeof(*out));
   fn(out, in, ROW_W, ROWS, ROW_W * 4, ROW_W * 2);
   for (i = 0; i < NPIX; i++)
      check(name, i, out[i], alpha | ref(in[i]));
}

static void test_16_to_24(const char *name, conv_fn fn,
      const uint16_t *in, uint8_t *out, uint32_t (*ref)(uint32_t))
{
   unsigned i;
   memset(out, 0x5a, NPIX * 3);
   fn(out, in, ROW_W, ROWS, ROW_W * 3, ROW_W * 2);
   for (i = 0; i < NPIX; i++)
   {
      const uint8_t *p = out + i * 3;
      check(name, i, (uint32_t)p[0] | ((uint32_t)p[1] << 8)
            | ((uint32_t)p[2] << 16), ref(in[i]));
   }
}

static void test_rgba4444_rgb565(const uint16_t *in, uint16_t *out)
{
   unsigned i;
   conv_rgba4444_rgb565(out, in, ROW_W, ROWS, ROW_W * 2, ROW_W * 2);
   for (i = 0; i < NPIX; i++)
   {
      uint32_t c = in[i];
      uint32_t want = (((c >> 12) & 0xf) << 12)
            | (((c >> 8) & 0xf) << 7) | (((c >> 4) & 0xf) << 1);
      check("conv_rgba4444_rgb565", i, out[i], want);
   }
}

static void test_32_to_16(uint32_t *in, uint16_t *out)
{
   unsigned i;
   uint32_t seed = 0x12345678u;

   /* Every value of each byte lane, then pseudo-random fill. */
   for (i = 0; i < NPIX; i++)
   {
      if (i < 256 * 4)
         in[i] = (uint32_t)(i & 0xff) << ((i >> 8) * 8);
      else
      {
         seed   = seed * 1664525u + 1013904223u;
         in[i]  = seed;
      }
   }

   conv_argb8888_rgba4444(out, in, ROW_W, ROWS, ROW_W * 2, ROW_W * 4);
   for (i = 0; i < NPIX; i++)
   {
      uint32_t c    = in[i];
      uint32_t want = (((c >> 20) & 0xf) << 12) | (((c >> 12) & 0xf) << 8)
            | (((c >> 4) & 0xf) << 4) | ((c >> 28) & 0xf);
      check("conv_argb8888_rgba4444", i, out[i], want);
   }

   conv_argb8888_0rgb1555(out, in, ROW_W, ROWS, ROW_W * 2, ROW_W * 4);
   for (i = 0; i < NPIX; i++)
   {
      uint32_t c    = in[i];
      uint32_t want = (((c >> 19) & 0x1f) << 10) | (((c >> 11) & 0x1f) << 5)
            | ((c >> 3) & 0x1f);
      check("conv_argb8888_0rgb1555", i, out[i], want);
   }
}

int main(void)
{
   unsigned i;
   uint16_t *in16  = (uint16_t*)malloc(NPIX * sizeof(uint16_t));
   uint16_t *out16 = (uint16_t*)malloc(NPIX * sizeof(uint16_t));
   uint32_t *in32  = (uint32_t*)malloc(NPIX * sizeof(uint32_t));
   uint32_t *out32 = (uint32_t*)malloc(NPIX * sizeof(uint32_t));
   uint8_t  *out24 = (uint8_t*)malloc(NPIX * 3);

   if (!in16 || !out16 || !in32 || !out32 || !out24)
   {
      printf("FAIL: out of memory\n");
      return 1;
   }

   for (i = 0; i < NPIX; i++)
      in16[i] = (uint16_t)i;

   test_helpers();

   test_16_to_32("conv_rgb565_argb8888", conv_rgb565_argb8888,
         in16, out32, ref_565_rgb, 0xff000000u);
   test_16_to_32("conv_rgb565_abgr8888", conv_rgb565_abgr8888,
         in16, out32, ref_565_bgr, 0xff000000u);
   test_16_to_32("conv_rgba4444_argb8888", conv_rgba4444_argb8888,
         in16, out32, ref_rgba4444_argb, 0);
   test_16_to_24("conv_rgb565_bgr24", conv_rgb565_bgr24,
         in16, out24, ref_565_rgb);
   test_rgba4444_rgb565(in16, out16);

   /* The 0RGB1555 converters take the top bit as padding. */
   for (i = 0; i < NPIX; i++)
      in16[i] = (uint16_t)(i & 0x7fff);
   test_16_to_32("conv_0rgb1555_argb8888", conv_0rgb1555_argb8888,
         in16, out32, ref_1555_rgb, 0xff000000u);
   test_16_to_24("conv_0rgb1555_bgr24", conv_0rgb1555_bgr24,
         in16, out24, ref_1555_rgb);

   test_32_to_16(in32, out16);

   free(in16);
   free(out16);
   free(in32);
   free(out32);
   free(out24);

   if (failures)
   {
      printf("pixconv_test: %u mismatches\n", failures);
      return 1;
   }
   printf("pixconv_test: all conversions match\n");
   return 0;
}
