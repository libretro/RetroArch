#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gfx/scaler/scaler.h>

static unsigned allocations;
static unsigned successful_checks;
static void *rows_malloc(size_t size)
{
   allocations++;
   return malloc(size);
}
#define malloc rows_malloc
#include SCALER_SOURCE
#undef malloc

static uint32_t input[128 * 128], expected[128 * 128], scratch[130];
struct row_check
{
   const uint8_t *reference;
   const uint8_t *input;
   int stride, input_stride;
   unsigned width, count;
   bool borrowed;
};

static void check_row(void *user, const uint32_t *pixels, unsigned width, unsigned y)
{
   struct row_check *check = (struct row_check*)user;
   assert(y == check->count++ && width == check->width);
   assert(pixels == (check->borrowed
            ? (const uint32_t*)(check->input + (ptrdiff_t)y * check->input_stride)
            : scratch + 1));
   assert(!memcmp(pixels, check->reference + (ptrdiff_t)y * check->stride,
            width * sizeof(*pixels)));
}

static void one_case(const unsigned *size, enum scaler_type type, unsigned flip)
{
   struct scaler_ctx ctx, before;
   struct row_check check;
   uint32_t *src = input, *dst = expected;
   unsigned i, calls;
   memset(&ctx, 0, sizeof(ctx));
   ctx.in_width = size[0]; ctx.in_height = size[1];
   ctx.out_width = size[2]; ctx.out_height = size[3];
   ctx.in_stride = (ctx.in_width + 3) * 4;
   ctx.out_stride = (ctx.out_width + 5) * 4;
   ctx.in_fmt = ctx.out_fmt = SCALER_FMT_ARGB8888;
   ctx.scaler_type = type;
   for (i = 0; i < sizeof(input) / sizeof(*input); i++) input[i] = i * 2654435761u;
   memset(expected, 0xa5, sizeof(expected));
   for (i = 0; i < sizeof(scratch) / sizeof(*scratch); i++) scratch[i] = 0x12345678u;
   if (flip & 1)
   {
      src += (ctx.in_height - 1) * (ctx.in_stride / 4);
      ctx.in_stride = -ctx.in_stride;
   }
   if (flip & 2)
   {
      dst += (ctx.out_height - 1) * (ctx.out_stride / 4);
      ctx.out_stride = -ctx.out_stride;
   }
   if (!scaler_ctx_gen_filter(&ctx))
   {
      assert(size[0] == 1 || size[1] == 1);
      memset(&check, 0, sizeof(check));
      assert(!scaler_ctx_scale_rows(&ctx, scratch + 1, sizeof(scratch) - 4, src, check_row, &check));
      assert(!check.count);
      return;
   }
   successful_checks++;
   scaler_ctx_scale(&ctx, dst, src);
   check.reference = (const uint8_t*)dst;
   check.input = (const uint8_t*)src;
   check.stride = ctx.out_stride; check.input_stride = ctx.in_stride;
   check.width = ctx.out_width; check.count = 0; check.borrowed = ctx.unscaled;
   memset(&before, 0, sizeof(before));
   before = ctx;
   calls = allocations;
   assert(scaler_ctx_scale_rows(&ctx, scratch + 1, ctx.out_width * 4, src, check_row, &check));
   assert(check.count == (unsigned)ctx.out_height && allocations == calls);
   assert(!memcmp(&ctx, &before, sizeof(ctx)));
   assert(scratch[0] == 0x12345678u && scratch[ctx.out_width + 1] == 0x12345678u);
   check.count = 0;
   if (ctx.unscaled)
   {
      assert(scaler_ctx_scale_rows(&ctx, NULL, 0, src, check_row, &check));
      assert(check.count == (unsigned)ctx.out_height);
   }
   else
   {
      assert(!scaler_ctx_scale_rows(&ctx, scratch + 1, ctx.out_width * 4 - 1, src, check_row, &check));
      assert(!scaler_ctx_scale_rows(&ctx, NULL, 0, src, check_row, &check));
      assert(!scaler_ctx_scale_rows(&ctx, (uint32_t*)((uint8_t*)scratch + 1), sizeof(scratch) - 1, src, check_row, &check));
      assert(!check.count);
   }
   ctx.out_fmt = SCALER_FMT_RGB565;
   check.count = 0;
   assert(!scaler_ctx_scale_rows(&ctx, scratch + 1, sizeof(scratch) - 4, src, check_row, &check));
   assert(!check.count);
   scaler_ctx_gen_reset(&ctx);
}

int main(void)
{
   static const unsigned sizes[][4] = {
      {37, 39, 31, 29}, {65, 49, 47, 67}, {1, 5, 8, 3}, {5, 1, 3, 9},
      {9, 7, 9, 7}, {64, 48, 32, 24}, {33, 37, 33, 29}, {29, 35, 35, 35}
   };
   unsigned size, flip;
   enum scaler_type type;
   struct scaler_ctx point;
   struct row_check check;
   for (size = 0; size < sizeof(sizes) / sizeof(sizes[0]); size++)
      for (flip = 0; flip < 4; flip++)
         for (type = SCALER_TYPE_BILINEAR; type <= SCALER_TYPE_SINC; type++)
            one_case(sizes[size], type, flip);
   assert(successful_checks >= 32);
   memset(&point, 0, sizeof(point)); memset(&check, 0, sizeof(check));
   point.in_width = point.in_height = 32;
   point.out_width = point.out_height = 16;
   point.in_stride = 128; point.out_stride = 64;
   point.scaler_type = SCALER_TYPE_POINT;
   assert(scaler_ctx_gen_filter(&point));
   assert(!scaler_ctx_scale_rows(&point, scratch + 1, sizeof(scratch) - 4, input, check_row, &check));
   assert(!check.count);
   scaler_ctx_gen_reset(&point);
   puts("PASS: filtered row loans match whole-frame scaling, preserve context, borrow unscaled rows, reject invalid lends and allocate nothing");
   return 0;
}
