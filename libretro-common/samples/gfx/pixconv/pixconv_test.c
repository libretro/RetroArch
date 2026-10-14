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
#include <gfx/scaler/scaler.h>
#include <gfx/video_frame.h>

#define ROW_W 263
#define ROWS  ((0x10000 + ROW_W - 1) / ROW_W)
#define NPIX  (ROW_W * ROWS)

static unsigned failures;

static void test_abgr_scaler(void)
{
   struct scaler_ctx ctx;
   struct scaler_ctx *scaler = &ctx;
   uint32_t input[27], output[30];
   unsigned alpha, x, y;
   memset(&ctx, 0, sizeof(ctx));
   ctx.in_width = ctx.out_width = 7;
   ctx.in_height = ctx.out_height = 3;
   ctx.in_fmt = SCALER_FMT_ABGR8888;
   ctx.out_fmt = SCALER_FMT_ARGB8888;
   ctx.in_stride = 9 * sizeof(uint32_t);
   ctx.out_stride = 10 * sizeof(uint32_t);
   if (!scaler_ctx_gen_filter(&ctx))
   {
      printf("FAIL: ABGR to ARGB scaler unavailable\n");
      failures++;
      return;
   }
   for (alpha = 0; alpha < 256; alpha++)
   {
      memset(input, 0xa5, sizeof(input));
      memset(output, 0x5a, sizeof(output));
      for (y = 0; y < 3; y++)
         for (x = 0; x < 7; x++)
            input[y * 9 + x] = (alpha << 24) | ((x + 17) << 16)
               | ((y + 33) << 8) | (x + y + 49);
      scaler_ctx_scale_direct(scaler, output, input);
      for (y = 0; y < 3; y++)
         for (x = 0; x < 10; x++)
         {
            uint32_t expected = x < 7 ? (alpha << 24)
               | ((x + y + 49) << 16) | ((y + 33) << 8) | (x + 17)
               : 0x5a5a5a5au;
            if (output[y * 10 + x] != expected)
               failures++;
         }
   }
   scaler_ctx_gen_reset(&ctx);
}

/* A same-format conversion at one size copies each row's own pixels:
 * into a rectangle inside a larger image the rest of every row stays
 * as it was, and the last row stops at the rectangle's edge rather than
 * running a stride past it. For each pixel size. */
static void test_copy_into_rect(void)
{
   static const enum scaler_pix_fmt fmts[3] = {
      SCALER_FMT_RGB565, SCALER_FMT_BGR24, SCALER_FMT_ARGB8888
   };
   static const unsigned bpps[3] = { 2, 3, 4 };
   unsigned f;
   for (f = 0; f < 3; f++)
   {
      struct scaler_ctx ctx;
      struct scaler_ctx *scaler = &ctx;
      unsigned bpp = bpps[f];
      /* 5x3 into an image 12x4 at column 2 */
      uint8_t input[3 * 16 * 4];
      uint8_t output[4 * 12 * 4];
      unsigned x, y, b;
      memset(&ctx, 0, sizeof(ctx));
      ctx.in_width   = ctx.out_width  = 5;
      ctx.in_height  = ctx.out_height = 3;
      ctx.in_fmt     = ctx.out_fmt    = fmts[f];
      ctx.in_stride  = (int)(16 * bpp);
      ctx.out_stride = (int)(12 * bpp);
      if (!scaler_ctx_gen_filter(&ctx))
      {
         printf("FAIL: same-format copy unavailable (%u bpp)\n", bpp);
         failures++;
         continue;
      }
      for (b = 0; b < sizeof(input); b++)
         input[b] = (uint8_t)(b * 7 + 1);
      memset(output, 0x5a, sizeof(output));
      scaler_ctx_scale_direct(scaler, output + 2 * bpp, input);
      for (y = 0; y < 4; y++)
         for (x = 0; x < 12; x++)
            for (b = 0; b < bpp; b++)
            {
               uint8_t got  = output[(y * 12 + x) * bpp + b];
               uint8_t want = (y < 3 && x >= 2 && x < 7)
                  ? input[y * 16 * bpp + (x - 2) * bpp + b] : 0x5a;
               if (got != want)
               {
                  printf("FAIL: %u bpp copy at (%u,%u) byte %u: %02x, "
                        "want %02x\n", bpp, x, y, b, got, want);
                  failures++;
                  x = 12;
                  y = 4;
                  break;
               }
            }
      scaler_ctx_gen_reset(&ctx);
   }
}

/* A converter made before its size is known, as the frontend makes its
 * 0RGB1555 one: generated at 0 x 0, then given each frame's size and
 * run. It must come up, own no intermediate frames (an unscaled
 * conversion runs between the caller's buffers), and convert. */
static void test_sizeless_converter(void)
{
   struct scaler_ctx ctx;
   struct scaler_ctx *scaler = &ctx;
   uint16_t input[5 * 3], output[6 * 3], expect;
   unsigned x, y;
   memset(&ctx, 0, sizeof(ctx));
   ctx.scaler_type = SCALER_TYPE_POINT;
   ctx.in_fmt      = SCALER_FMT_0RGB1555;
   ctx.out_fmt     = SCALER_FMT_RGB565;
   if (!scaler_ctx_gen_filter(&ctx))
   {
      printf("FAIL: 0RGB1555 to RGB565 converter refused at 0 x 0\n");
      failures++;
      return;
   }
   if (ctx.frame_arena || ctx.filter_arena)
   {
      printf("FAIL: an unscaled converter owns intermediate frames\n");
      failures++;
   }
   for (y = 0; y < 3; y++)
      for (x = 0; x < 5; x++)
         input[y * 5 + x] = (uint16_t)(0x1234 + y * 977 + x * 131);
   memset(output, 0x5a, sizeof(output));
   ctx.in_width   = ctx.out_width  = 5;
   ctx.in_height  = ctx.out_height = 3;
   ctx.in_stride  = 5 * sizeof(uint16_t);
   ctx.out_stride = 6 * sizeof(uint16_t);
   scaler_ctx_scale_direct(scaler, output, input);
   for (y = 0; y < 3; y++)
      for (x = 0; x < 6; x++)
      {
         if (x < 5)
            conv_0rgb1555_rgb565(&expect, &input[y * 5 + x], 1, 1, 2, 2);
         else
            expect = 0x5a5a;
         if (output[y * 6 + x] != expect)
         {
            printf("FAIL: sizeless converter pixel %u,%u: %04x, not %04x\n",
                  x, y, output[y * 6 + x], expect);
            failures++;
         }
      }
   scaler_ctx_gen_reset(&ctx);
}

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

static void test_16_to_16(const char *name, conv_fn fn,
      const uint16_t *in, uint16_t *out, uint32_t (*ref)(uint32_t))
{
   unsigned i;
   memset(out, 0x5a, NPIX * sizeof(*out));
   fn(out, in, ROW_W, ROWS, ROW_W * 2, ROW_W * 2);
   for (i = 0; i < NPIX; i++)
      check(name, i, out[i], ref(in[i]));
}

static uint32_t ref_565_1555(uint32_t c)
{
   return ((c >> 1) & 0x7fe0) | (c & 0x1f);
}

static uint32_t ref_1555_565(uint32_t c)
{
   return ((c << 1) & 0xffc0) | (c & 0x1f) | ((c >> 4) & 0x20);
}

/* 8-bit sources: pseudo-random bytes, so every byte lane sees every
 * value many times over. */
static void fill_random(uint8_t *buf, size_t len, uint32_t seed)
{
   size_t i;
   for (i = 0; i < len; i++)
   {
      seed   = seed * 1664525u + 1013904223u;
      buf[i] = (uint8_t)(seed >> 24);
   }
}

static void test_bgr24_sources(uint8_t *in24, uint32_t *out32,
      uint16_t *out16)
{
   unsigned i;
   fill_random(in24, NPIX * 3, 0xc0ffee11u);

   memset(out32, 0x5a, NPIX * sizeof(*out32));
   conv_bgr24_argb8888(out32, in24, ROW_W, ROWS, ROW_W * 4, ROW_W * 3);
   for (i = 0; i < NPIX; i++)
   {
      const uint8_t *p = in24 + i * 3;
      check("conv_bgr24_argb8888", i, out32[i], 0xff000000u
            | ((uint32_t)p[2] << 16) | ((uint32_t)p[1] << 8) | p[0]);
   }

   memset(out16, 0x5a, NPIX * sizeof(*out16));
   conv_bgr24_rgb565(out16, in24, ROW_W, ROWS, ROW_W * 2, ROW_W * 3);
   for (i = 0; i < NPIX; i++)
   {
      const uint8_t *p = in24 + i * 3;
      check("conv_bgr24_rgb565", i, out16[i],
            ((uint32_t)(p[2] & 0xf8) << 8) | ((uint32_t)(p[1] & 0xfc) << 3)
            | (p[0] >> 3));
   }
}

static void test_32_sources(uint32_t *in32, uint8_t *out24,
      uint32_t *out32)
{
   unsigned i;
   fill_random((uint8_t*)in32, NPIX * 4, 0x5eed1234u);

   memset(out24, 0x5a, NPIX * 3);
   conv_argb8888_bgr24(out24, in32, ROW_W, ROWS, ROW_W * 3, ROW_W * 4);
   for (i = 0; i < NPIX; i++)
   {
      const uint8_t *p = out24 + i * 3;
      check("conv_argb8888_bgr24", i, (uint32_t)p[0]
            | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16),
            in32[i] & 0x00ffffffu);
   }

   memset(out24, 0x5a, NPIX * 3);
   conv_abgr8888_bgr24(out24, in32, ROW_W, ROWS, ROW_W * 3, ROW_W * 4);
   for (i = 0; i < NPIX; i++)
   {
      const uint8_t *p = out24 + i * 3;
      uint32_t c       = in32[i];
      check("conv_abgr8888_bgr24", i, (uint32_t)p[0]
            | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16),
            ((c >> 16) & 0xff) | (c & 0xff00) | ((c & 0xff) << 16));
   }

   /* A negative output stride flips the image: the last row's start
    * as the destination, each row written above the one before. */
   memset(out24, 0x5a, NPIX * 3);
   conv_argb8888_bgr24(out24 + (size_t)(ROWS - 1) * ROW_W * 3, in32,
         ROW_W, ROWS, -(int)(ROW_W * 3), ROW_W * 4);
   for (i = 0; i < NPIX; i++)
   {
      unsigned       row = i / ROW_W;
      unsigned       x   = i % ROW_W;
      const uint8_t *p   = out24 + ((size_t)(ROWS - 1 - row) * ROW_W + x) * 3;
      check("conv_argb8888_bgr24 bottom-up", i, (uint32_t)p[0]
            | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16),
            in32[i] & 0x00ffffffu);
   }

   memset(out24, 0x5a, NPIX * 3);
   conv_abgr8888_bgr24(out24 + (size_t)(ROWS - 1) * ROW_W * 3, in32,
         ROW_W, ROWS, -(int)(ROW_W * 3), ROW_W * 4);
   for (i = 0; i < NPIX; i++)
   {
      unsigned       row = i / ROW_W;
      unsigned       x   = i % ROW_W;
      const uint8_t *p   = out24 + ((size_t)(ROWS - 1 - row) * ROW_W + x) * 3;
      uint32_t       c   = in32[i];
      check("conv_abgr8888_bgr24 bottom-up", i, (uint32_t)p[0]
            | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16),
            ((c >> 16) & 0xff) | (c & 0xff00) | ((c & 0xff) << 16));
   }

   memset(out32, 0x5a, NPIX * sizeof(*out32));
   conv_argb8888_abgr8888(out32, in32, ROW_W, ROWS, ROW_W * 4, ROW_W * 4);
   for (i = 0; i < NPIX; i++)
   {
      uint32_t c = in32[i];
      check("conv_argb8888_abgr8888", i, out32[i],
            (c & 0xff00ff00u) | ((c >> 16) & 0xff) | ((c & 0xff) << 16));
   }
}

static int clamp8(int v)
{
   return v < 0 ? 0 : (v > 255 ? 255 : v);
}

/* YUYV pairs need an even width; the stride stays at the odd row
 * width so rows still start at every alignment. */
static void test_yuyv(uint8_t *in, uint32_t *out)
{
   unsigned x, y;
   const unsigned w = ROW_W - 1;
   fill_random(in, NPIX * 2, 0x0badf00du);
   memset(out, 0x5a, NPIX * sizeof(*out));
   conv_yuyv_argb8888(out, in, (int)w, ROWS, ROW_W * 4, ROW_W * 2);
   for (y = 0; y < ROWS; y++)
   {
      for (x = 0; x < w; x++)
      {
         const uint8_t *s = in + y * ROW_W * 2 + (x & ~1u) * 2;
         int yy           = s[(x & 1) ? 2 : 0];
         int u            = s[1] - 128;
         int v            = s[3] - 128;
         int r            = clamp8((64 * yy + 90 * v + 32) >> 6);
         int g            = clamp8((64 * yy - 22 * u - 46 * v + 32) >> 6);
         int b            = clamp8((64 * yy + 113 * u + 32) >> 6);
         check("conv_yuyv_argb8888", y * ROW_W + x, out[y * ROW_W + x],
               0xff000000u | ((uint32_t)r << 16) | ((uint32_t)g << 8)
               | (uint32_t)b);
      }
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
   uint8_t  *out24_src = (uint8_t*)malloc(NPIX * 3);

   if (!in16 || !out16 || !in32 || !out32 || !out24 || !out24_src)
   {
      printf("FAIL: out of memory\n");
      return 1;
   }

   for (i = 0; i < NPIX; i++)
      in16[i] = (uint16_t)i;

   test_helpers();
   test_sizeless_converter();
   test_abgr_scaler();
   test_copy_into_rect();

   test_16_to_32("conv_rgb565_argb8888", conv_rgb565_argb8888,
         in16, out32, ref_565_rgb, 0xff000000u);
   test_16_to_32("conv_rgb565_abgr8888", conv_rgb565_abgr8888,
         in16, out32, ref_565_bgr, 0xff000000u);
   test_16_to_32("conv_rgba4444_argb8888", conv_rgba4444_argb8888,
         in16, out32, ref_rgba4444_argb, 0);
   test_16_to_24("conv_rgb565_bgr24", conv_rgb565_bgr24,
         in16, out24, ref_565_rgb);
   test_rgba4444_rgb565(in16, out16);
   test_16_to_16("conv_rgb565_0rgb1555", conv_rgb565_0rgb1555,
         in16, out16, ref_565_1555);

   /* The 0RGB1555 converters take the top bit as padding. */
   for (i = 0; i < NPIX; i++)
      in16[i] = (uint16_t)(i & 0x7fff);
   test_16_to_32("conv_0rgb1555_argb8888", conv_0rgb1555_argb8888,
         in16, out32, ref_1555_rgb, 0xff000000u);
   test_16_to_24("conv_0rgb1555_bgr24", conv_0rgb1555_bgr24,
         in16, out24, ref_1555_rgb);
   test_16_to_16("conv_0rgb1555_rgb565", conv_0rgb1555_rgb565,
         in16, out16, ref_1555_565);

   test_32_to_16(in32, out16);
   test_32_sources(in32, out24, out32);
   test_bgr24_sources(out24_src, out32, out16);
   test_yuyv(out24_src, out32);

   free(in16);
   free(out16);
   free(in32);
   free(out32);
   free(out24);
   free(out24_src);

   if (failures)
   {
      printf("pixconv_test: %u mismatches\n", failures);
      return 1;
   }
   printf("pixconv_test: all conversions match\n");
   return 0;
}
