/* A failed converter must report failure and leave the destination alone. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <gfx/video_frame.h>

static void compare(int iw, int ih, int ow, int oh,
      enum scaler_pix_fmt fmt, int flip, int benchmark)
{
   struct scaler_ctx reference, fused;
   unsigned bpp = fmt == SCALER_FMT_BGR24 ? 3 : (fmt == SCALER_FMT_RGB565 ? 2 : 4);
   int ip = (iw * bpp + 15) & ~15;
   int op = ow * 3 + 12;
   size_t bytes = (size_t)op * oh;
   uint8_t *input = (uint8_t*)malloc((size_t)ip * ih);
   uint8_t *expected = (uint8_t*)malloc(bytes);
   uint8_t *actual = (uint8_t*)malloc(bytes);
   const uint8_t *src;
   uint8_t *expected_dst, *actual_dst;
   size_t i;
   clock_t start;
   double old_ms, new_ms;
   int repeat, iterations = benchmark ? 20 : 1;
   assert(input && expected && actual);
   for (i = 0; i < (size_t)ip * ih; i++)
      input[i] = (uint8_t)(i * 37 + i / 13);
   memset(expected, 0xa5, bytes);
   memset(actual, 0xa5, bytes);
   src = (flip & 1) ? input + (size_t)ip * (ih - 1) : input;
   if (flip & 1)
      ip = -ip;
   expected_dst = (flip & 2) ? expected + (size_t)op * (oh - 1) : expected;
   actual_dst = (flip & 2) ? actual + (size_t)op * (oh - 1) : actual;
   if (flip & 2)
      op = -op;
   memset(&reference, 0, sizeof(reference));
   memset(&fused, 0, sizeof(fused));
   reference.in_width = iw;
   reference.in_height = ih;
   reference.out_width = ow;
   reference.out_height = oh;
   reference.in_fmt = fmt;
   reference.out_fmt = SCALER_FMT_BGR24;
   reference.scaler_type = SCALER_TYPE_POINT;
   reference.in_stride = ip;
   reference.out_stride = op;
   fused.in_fmt = fmt;
   start = clock();
   for (repeat = 0; repeat < iterations; repeat++)
   {
      assert(scaler_ctx_gen_filter(&reference));
      scaler_ctx_scale(&reference, expected_dst, src);
      scaler_ctx_gen_reset(&reference);
   }
   old_ms = 1000.0 * (clock() - start) / CLOCKS_PER_SEC / iterations;
   start = clock();
   for (repeat = 0; repeat < iterations; repeat++)
      assert(video_frame_convert_to_bgr24(&fused, actual_dst, src, iw, ih, ip, ow, oh, op));
   new_ms = 1000.0 * (clock() - start) / CLOCKS_PER_SEC / iterations;
   assert(!memcmp(expected, actual, bytes));
   if (iw >= ow && ih >= oh)
      assert(!fused.frame_arena && !fused.filter_arena);
   if (benchmark)
      printf("%dx%d -> %dx%d format=%d: generic %.3f ms, helper %.3f ms (%.1fx)\n",
            iw, ih, ow, oh, (int)fmt, old_ms, new_ms, old_ms / new_ms);
   scaler_ctx_gen_reset(&fused);
   free(input);
   free(expected);
   free(actual);
}

int main(int argc, char **argv)
{
   struct scaler_ctx ctx;
   uint32_t input[4] = {0xff112233, 0xff445566, 0xff778899, 0xffaabbcc};
   uint8_t output[12];
   unsigned i;
   unsigned f, flip, size;
   static const int sizes[][4] = {
      {32, 24, 8, 6}, {7, 5, 13, 9}, {17, 9, 3, 2},
      {8, 8, 8, 4}, {8, 8, 4, 8}, {19, 17, 6, 7}
   };
   static const enum scaler_pix_fmt formats[] = {
      SCALER_FMT_ARGB8888, SCALER_FMT_RGB565, SCALER_FMT_BGR24
   };
   (void)argv;
   memset(&ctx, 0, sizeof(ctx));
   memset(output, 0xa5, sizeof(output));
   ctx.in_fmt = SCALER_FMT_ARGB8888;
   assert(!video_frame_convert_to_bgr24(&ctx, output, input,
            2, 2, 8, 0x40000000, 2, 6));
   for (i = 0; i < sizeof(output); i++)
      assert(output[i] == 0xa5);
   ctx.in_fmt = SCALER_FMT_YUYV;
   assert(!video_frame_convert_to_bgr24(&ctx, output, input,
            2, 2, 8, 1, 1, 3));
   assert(!ctx.frame_arena && !ctx.filter_arena);
   for (i = 0; i < sizeof(output); i++)
      assert(output[i] == 0xa5);
   ctx.in_fmt = SCALER_FMT_ARGB8888;
   assert(video_frame_convert_to_bgr24(&ctx, output, input,
            2, 2, 8, 2, 2, 6));
   assert(output[0] == 0x33 && output[1] == 0x22 && output[2] == 0x11);
   assert(!ctx.frame_arena);
   scaler_ctx_gen_reset(&ctx);
   assert(video_frame_convert_to_bgr24(&ctx, output, input,
            2, 2, 8, 1, 1, 3));
   assert(output[0] == 0x33 && output[1] == 0x22 && output[2] == 0x11);
   for (f = 0; f < 3; f++)
      for (flip = 0; flip < 4; flip++)
         for (size = 0; size < sizeof(sizes) / sizeof(sizes[0]); size++)
            compare(sizes[size][0], sizes[size][1], sizes[size][2], sizes[size][3], formats[f], flip, 0);
   if (argc > 1)
      for (f = 0; f < 3; f++)
      {
         compare(3840, 2160, 640, 480, formats[f], 1, 1);
         compare(320, 240, 1920, 1080, formats[f], 0, 1);
      }
   puts("PASS: failed conversion leaves output untouched; retry succeeds without intermediate frames");
   return 0;
}
