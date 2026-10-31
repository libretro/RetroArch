/* Point sampling pixels, conversion workspaces and failure handling. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <gfx/scaler/scaler.h>
#include <gfx/scaler/pixconv.h>

static void *frame_allocation;
static size_t frame_bytes_allocated;
static unsigned frame_calls;
static int fail_frame;

static void *frame_malloc(size_t size)
{
   frame_calls++;
   if (fail_frame)
      return NULL;
   assert(!frame_allocation);
   frame_allocation = malloc(size);
   assert(frame_allocation);
   frame_bytes_allocated = size;
   return frame_allocation;
}

static void frame_free(void *p)
{
   if (p == frame_allocation)
      frame_allocation = NULL;
   free(p);
}

#define malloc frame_malloc
#define free frame_free
#include SCALER_SOURCE
#undef malloc
#undef free

static void init(struct scaler_ctx *ctx, enum scaler_type type,
      enum scaler_pix_fmt in_fmt, enum scaler_pix_fmt out_fmt,
      int iw, int ih, int ow, int oh, int ip, int op)
{
   memset(ctx, 0, sizeof(*ctx));
   ctx->scaler_type = type;
   ctx->in_fmt = in_fmt;
   ctx->out_fmt = out_fmt;
   ctx->in_width = iw;
   ctx->in_height = ih;
   ctx->out_width = ow;
   ctx->out_height = oh;
   ctx->in_stride = ip;
   ctx->out_stride = op;
}

static void pixels(int iw, int ih, int ow, int oh, unsigned flip,
      enum scaler_pix_fmt in_fmt, enum scaler_pix_fmt out_fmt, int benchmark)
{
   struct scaler_ctx ctx;
   unsigned bpp_in = in_fmt == SCALER_FMT_BGR24 ? 3
      : (in_fmt == SCALER_FMT_RGB565 ? 2 : 4);
   unsigned bpp_out = out_fmt == SCALER_FMT_BGR24 ? 3
      : (out_fmt == SCALER_FMT_RGB565 ? 2 : 4);
   int ip = (iw * bpp_in + 15) & ~15;
   int op = (ow * bpp_out + 15) & ~15;
   size_t in_size = (size_t)ip * ih;
   size_t out_size = (size_t)op * oh;
   uint8_t *input = (uint8_t*)malloc(in_size);
   uint8_t *output = (uint8_t*)malloc(out_size);
   uint8_t *expected = (uint8_t*)malloc(out_size);
   const uint8_t *src;
   uint8_t *dst, *ref;
   int x, y, repeat, iterations = benchmark ? 30 : 1;
   size_t i, allocated = 0;
   clock_t start;
   double elapsed, scale_elapsed = 0.0;
   assert(input && output && expected);
   for (i = 0; i < in_size; i++) input[i] = (uint8_t)(i * 37 + i / 13);
   memset(output, 0xa5, out_size);
   memset(expected, 0xa5, out_size);
   src = (flip & 1) ? input + (size_t)ip * (ih - 1) : input;
   dst = (flip & 2) ? output + (size_t)op * (oh - 1) : output;
   ref = (flip & 2) ? expected + (size_t)op * (oh - 1) : expected;
   if (flip & 1) ip = -ip;
   if (flip & 2) op = -op;
   for (y = 0; y < oh; y++)
   {
      int64_t yp = ((int64_t)ih << 15) / oh - 32768;
      int64_t ys = ((int64_t)ih << 16) / oh;
      if (yp < 0) yp = 0;
      for (x = 0; x < ow; x++)
      {
         int64_t xp = ((int64_t)iw << 15) / ow - 32768;
         int64_t xs = ((int64_t)iw << 16) / ow;
         const void *p;
         void *q = ref + (ptrdiff_t)y * op + (size_t)x * bpp_out;
         uint32_t pixel;
         if (xp < 0) xp = 0;
         p = src + (ptrdiff_t)((yp + y * ys) >> 16) * ip
            + (size_t)((xp + x * xs) >> 16) * bpp_in;
         if (in_fmt == SCALER_FMT_RGB565)
            conv_rgb565_argb8888(&pixel, p, 1, 1, 4, 2);
         else if (in_fmt == SCALER_FMT_BGR24)
         {
            const uint8_t *bgr = (const uint8_t*)p;
            pixel = 0xff000000u | ((uint32_t)bgr[2] << 16)
               | ((uint32_t)bgr[1] << 8) | bgr[0];
         }
         else
            memcpy(&pixel, p, 4);
         if (out_fmt == SCALER_FMT_BGR24)
            conv_argb8888_bgr24(q, &pixel, 1, 1, 3, 4);
         else if (out_fmt == SCALER_FMT_RGB565)
         {
            uint16_t rgb = (uint16_t)((((pixel >> 19) & 0x1f) << 11)
                  | (((pixel >> 10) & 0x3f) << 5) | ((pixel >> 3) & 0x1f));
            memcpy(q, &rgb, 2);
         }
         else
            memcpy(q, &pixel, 4);
      }
   }
   start = clock();
   for (repeat = 0; repeat < iterations; repeat++)
   {
      init(&ctx, SCALER_TYPE_POINT, in_fmt, out_fmt, iw, ih, ow, oh, ip, op);
      frame_calls = 0;
      frame_bytes_allocated = 0;
      assert(scaler_ctx_gen_filter(&ctx));
      allocated = frame_bytes_allocated;
      if (!benchmark)
      {
         assert(!ctx.scaled.frame && !ctx.scaled.stride);
         if (((in_fmt == SCALER_FMT_ARGB8888 || in_fmt == SCALER_FMT_XRGB2101010)
                  && (out_fmt == SCALER_FMT_ARGB8888 || out_fmt == SCALER_FMT_XRGB2101010))
               || (ow <= iw && oh <= ih
                  && (uint64_t)ow * oh <= (uint64_t)iw * ih / 4))
            assert(!frame_calls && !ctx.frame_arena);
         else
            assert(frame_calls == 1 && ctx.frame_arena);
      }
      scaler_ctx_scale(&ctx, dst, src);
      scaler_ctx_gen_reset(&ctx);
      assert(!frame_allocation);
   }
   elapsed = 1000.0 * (clock() - start) / CLOCKS_PER_SEC / iterations;
   assert(!memcmp(output, expected, out_size));
   if (benchmark)
   {
      init(&ctx, SCALER_TYPE_POINT, in_fmt, out_fmt, iw, ih, ow, oh, ip, op);
      assert(scaler_ctx_gen_filter(&ctx));
      start = clock();
      for (repeat = 0; repeat < iterations; repeat++)
         scaler_ctx_scale(&ctx, dst, src);
      scale_elapsed = 1000.0 * (clock() - start) / CLOCKS_PER_SEC / iterations;
      scaler_ctx_gen_reset(&ctx);
      assert(!memcmp(output, expected, out_size));
      printf("%dx%d -> %dx%d in=%d out=%d: frame arena %lu bytes, setup+scale+reset %.3f ms, reused scale %.3f ms\n",
            iw, ih, ow, oh, (int)in_fmt, (int)out_fmt, (unsigned long)allocated, elapsed, scale_elapsed);
   }
   free(input);
   free(output);
   free(expected);
}

static void modes_and_failures(void)
{
   struct scaler_ctx ctx;
   unsigned type;
   uint32_t input[16 * 16], output[8 * 8];
   for (type = SCALER_TYPE_POINT; type <= SCALER_TYPE_SINC; type++)
   {
      init(&ctx, (enum scaler_type)type, SCALER_FMT_ARGB8888, SCALER_FMT_ARGB8888,
            16, 16, 8, 8, 64, 32);
      assert(scaler_ctx_gen_filter(&ctx));
      assert((ctx.scaled.frame != NULL) == (type != SCALER_TYPE_POINT));
      ctx.scaler_type = SCALER_TYPE_POINT;
      assert(scaler_ctx_gen_filter(&ctx));
      assert(!ctx.frame_arena && !ctx.scaled.frame);
      ctx.scaler_type = SCALER_TYPE_BILINEAR;
      assert(scaler_ctx_gen_filter(&ctx));
      assert(ctx.frame_arena && ctx.scaled.frame);
      scaler_ctx_gen_reset(&ctx);
      assert(!frame_allocation);
   }
   init(&ctx, SCALER_TYPE_POINT, SCALER_FMT_ARGB8888, SCALER_FMT_ARGB8888,
         16, 16, 8, 8, 64, 32);
   fail_frame = 1;
   frame_calls = 0;
   assert(scaler_ctx_gen_filter(&ctx) && !frame_calls);
   scaler_ctx_gen_reset(&ctx);
   ctx.in_fmt = SCALER_FMT_RGB565;
   assert(scaler_ctx_gen_filter(&ctx) && !frame_calls);
   scaler_ctx_gen_reset(&ctx);
   ctx.scaler_type = SCALER_TYPE_BILINEAR;
   assert(!scaler_ctx_gen_filter(&ctx));
   assert(!ctx.frame_arena && !ctx.filter_arena && !ctx.scaler_special);
   memset(input, 0, sizeof(input));
   memset(output, 0xa5, sizeof(output));
   scaler_ctx_scale(&ctx, output, input);
   assert(output[0] == 0xa5a5a5a5u);
   fail_frame = 0;
   assert(scaler_ctx_gen_filter(&ctx));
   scaler_ctx_gen_reset(&ctx);
   init(&ctx, SCALER_TYPE_POINT, SCALER_FMT_ARGB8888, SCALER_FMT_ARGB8888,
         16, 8192, 262144, 2, 64, 1048576);
   assert(!scaler_ctx_gen_filter(&ctx));
   assert(!ctx.frame_arena && !ctx.filter_arena);
}

static void all_rgb565_values(enum scaler_pix_fmt out_fmt)
{
   struct scaler_ctx ctx;
   unsigned x, y;
   size_t out_size = (size_t)256 * 256
      * (out_fmt == SCALER_FMT_BGR24 ? 3 : 4);
   uint16_t *input = (uint16_t*)malloc((size_t)256 * 1024 * 2);
   uint32_t *argb = (uint32_t*)malloc((size_t)256 * 256 * 4);
   uint8_t *output = (uint8_t*)malloc(out_size);
   uint8_t *expected = (uint8_t*)malloc(out_size);
   assert(input && argb && output && expected);
   for (y = 0; y < 1024; y++)
      for (x = 0; x < 256; x++)
         input[y * 256 + x] = (uint16_t)((y / 4) * 256 + x);
   conv_rgb565_argb8888(argb, input, 256, 256, 256 * 4, 256 * 8);
   if (out_fmt == SCALER_FMT_BGR24)
      conv_argb8888_bgr24(expected, argb, 256, 256, 256 * 3, 256 * 4);
   else
      memcpy(expected, argb, out_size);
   init(&ctx, SCALER_TYPE_POINT, SCALER_FMT_RGB565, out_fmt,
         256, 1024, 256, 256, 256 * 2,
         out_fmt == SCALER_FMT_BGR24 ? 256 * 3 : 256 * 4);
   assert(scaler_ctx_gen_filter(&ctx));
   assert(!ctx.frame_arena);
   scaler_ctx_scale(&ctx, output, input);
   assert(!memcmp(output, expected, out_size));
   scaler_ctx_gen_reset(&ctx);
   free(input);
   free(argb);
   free(output);
   free(expected);
}

int main(int argc, char **argv)
{
   static const enum scaler_pix_fmt fmts[][2] = {
      {SCALER_FMT_ARGB8888, SCALER_FMT_ARGB8888},
      {SCALER_FMT_RGB565, SCALER_FMT_BGR24},
      {SCALER_FMT_XRGB2101010, SCALER_FMT_XRGB2101010},
      {SCALER_FMT_RGB565, SCALER_FMT_ARGB8888},
      {SCALER_FMT_ARGB8888, SCALER_FMT_BGR24},
      {SCALER_FMT_BGR24, SCALER_FMT_ARGB8888},
      {SCALER_FMT_BGR24, SCALER_FMT_RGB565}
   };
   static const int sizes[][4] = {
      {32, 24, 8, 6}, {7, 5, 13, 9}, {17, 9, 3, 2},
      {8, 8, 8, 4}, {8, 8, 4, 8}, {2, 2, 1, 1},
      {9, 9, 4, 4}, {9, 9, 5, 5}, {16, 8, 8, 4},
      {16, 9, 16, 8}, {33, 1, 8, 1}
   };
   unsigned size, flip, fmt;
   (void)argv;
   if (argc > 1)
   {
      pixels(3840, 2160, 640, 480, 0, SCALER_FMT_ARGB8888, SCALER_FMT_ARGB8888, 1);
      pixels(1920, 1080, 3840, 2160, 0, SCALER_FMT_ARGB8888, SCALER_FMT_ARGB8888, 1);
      pixels(3840, 2160, 640, 480, 0, SCALER_FMT_RGB565, SCALER_FMT_BGR24, 1);
      pixels(3840, 2160, 640, 480, 0, SCALER_FMT_RGB565, SCALER_FMT_ARGB8888, 1);
      pixels(3840, 2160, 640, 480, 0, SCALER_FMT_ARGB8888, SCALER_FMT_BGR24, 1);
      pixels(1920, 1080, 1920, 1079, 0, SCALER_FMT_RGB565, SCALER_FMT_ARGB8888, 1);
      pixels(3840, 2160, 640, 480, 0, SCALER_FMT_BGR24, SCALER_FMT_ARGB8888, 1);
      pixels(3840, 2160, 640, 480, 0, SCALER_FMT_BGR24, SCALER_FMT_RGB565, 1);
      pixels(1920, 1080, 1920, 1079, 0, SCALER_FMT_BGR24, SCALER_FMT_RGB565, 1);
      return 0;
   }
   for (size = 0; size < sizeof(sizes) / sizeof(sizes[0]); size++)
      for (flip = 0; flip < 4; flip++)
         for (fmt = 0; fmt < sizeof(fmts) / sizeof(fmts[0]); fmt++)
            pixels(sizes[size][0], sizes[size][1], sizes[size][2], sizes[size][3], flip,
                  fmts[fmt][0], fmts[fmt][1], 0);
   modes_and_failures();
   all_rgb565_values(SCALER_FMT_ARGB8888);
   all_rgb565_values(SCALER_FMT_BGR24);
   puts("PASS: point pixels/strides, native and converted formats, mode changes, allocation failure and bounds");
   return 0;
}
