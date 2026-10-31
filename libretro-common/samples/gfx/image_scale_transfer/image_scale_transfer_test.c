/* Image scaling preserves pixels, ownership and allocation bounds. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <limits.h>
#include <formats/image.h>
#include <gfx/scaler/scaler.h>

static struct { void *ptr; size_t bytes; } buffers[8];
static unsigned calls, source_frees;
static size_t live, peak;
static int budget = -1;
static void *source_pixels;
static unsigned filter_calls;

static bool observed_filter(struct scaler_ctx *ctx);

static void *image_malloc(size_t bytes)
{
   unsigned i;
   void *p;
   calls++;
   if (budget >= 0 && calls > (unsigned)budget)
      return NULL;
   p = malloc(bytes);
   assert(p);
   for (i = 0; i < 8; i++)
      if (!buffers[i].ptr)
      {
         buffers[i].ptr = p;
         buffers[i].bytes = bytes;
         live += bytes;
         if (live > peak) peak = live;
         return p;
      }
   abort();
}

static void image_free(void *p)
{
   unsigned i;
   if (p && p == source_pixels)
   {
      source_frees++;
      source_pixels = NULL;
   }
   for (i = 0; p && i < 8; i++)
      if (p == buffers[i].ptr)
      {
         live -= buffers[i].bytes;
         buffers[i].ptr = NULL;
         break;
      }
   free(p);
}

#define malloc image_malloc
#define free image_free
#define scaler_ctx_gen_filter observed_filter
#include SCALING_SOURCE
#undef malloc
#undef free
#undef scaler_ctx_gen_filter

static bool observed_filter(struct scaler_ctx *ctx)
{
   filter_calls++;
   return scaler_ctx_gen_filter(ctx);
}

static void init(struct texture_image *img, unsigned w, unsigned h, int pix10)
{
   size_t i;
   memset(img, 0, sizeof(*img));
   img->width = w;
   img->height = h;
   img->pix10 = pix10 != 0;
   img->supports_rgba = true;
   img->pixels = (uint32_t*)malloc((size_t)w * h * 4);
   assert(img->pixels);
   source_pixels = img->pixels;
   source_frees = calls = 0;
   filter_calls = 0;
   assert(!live);
   peak = 0;
   for (i = 0; i < (size_t)w * h; i++)
      img->pixels[i] = (uint32_t)(i * 1234567u + 0x8193abcd);
}

static void exact(unsigned w, unsigned h, unsigned cap, int pix10)
{
   struct texture_image img;
   uint32_t expected[128];
   unsigned f = w / cap;
   unsigned dw = w / f, dh = h / f;
   unsigned x, y, i, j, c, n = f * f;
   assert(dw * dh <= 128);
   init(&img, w, h, pix10);
   for (y = 0; y < dh; y++)
      for (x = 0; x < dw; x++)
      {
         unsigned sums[4] = {0, 0, 0, 0};
         for (j = 0; j < f; j++)
            for (i = 0; i < f; i++)
            {
               uint32_t p = img.pixels[(size_t)(y * f + j) * w + x * f + i];
               if (pix10)
                  for (c = 0; c < 3; c++) sums[c] += (p >> (c * 10)) & 1023;
               else
                  for (c = 0; c < 4; c++) sums[c] += (p >> (c * 8)) & 255;
            }
         expected[y * dw + x] = pix10 ? 0xc0000000u : 0;
         for (c = 0; c < (pix10 ? 3u : 4u); c++)
            expected[y * dw + x] |= (sums[c] / n) << (c * (pix10 ? 10 : 8));
      }
   budget = 1;
   assert(image_texture_scale(&img, 0, cap));
   assert(calls == 1 && source_frees == 1);
   assert(img.width == dw && img.height == dh);
   assert(img.supports_rgba && img.pix10 == (pix10 != 0));
   assert(img.pixels == buffers[0].ptr && live == (size_t)dw * dh * 4);
   assert(!memcmp(img.pixels, expected, (size_t)dw * dh * 4));
   image_free(img.pixels);
   assert(!live);
   budget = -1;
}

static void failures(void)
{
   struct texture_image img;
   init(&img, 16, 8, 0);
   budget = 0;
   assert(!image_texture_scale(&img, 0, 8));
   assert(!source_frees && img.pixels == source_pixels);
   assert(img.width == 16 && img.height == 8 && !live);
   budget = -1;
   assert(image_texture_scale(&img, 0, 8));
   assert(source_frees == 1);
   image_free(img.pixels);
   assert(!live);
   init(&img, 19, 13, 0);
   assert(!image_texture_scale(&img, 0, 8));
   assert(img.width == 19 && img.height == 13 && calls == 2);
   assert(!source_frees && !live);
   image_free(img.pixels);
   init(&img, 39, 29, 0);
   assert(image_texture_scale(&img, 0, 16));
   assert(img.width == 16 && img.height == 11 && calls == 2);
   image_free(img.pixels);
   assert(!live);
}

static void benchmark(int pix10)
{
   struct texture_image img;
   double elapsed = 0.0;
   unsigned repeat;
   size_t maximum = 0;
   for (repeat = 0; repeat < 12; repeat++)
   {
      clock_t start;
      init(&img, 3840, 2160, pix10);
      start = clock();
      assert(image_texture_scale(&img, 0, 1920));
      elapsed += 1000.0 * (clock() - start) / CLOCKS_PER_SEC;
      if (peak > maximum) maximum = peak;
      image_free(img.pixels);
      assert(!live);
   }
   printf("4K -> 1920x1080 pix10=%d: additional image storage %lu bytes, downscale %.3f ms\n",
         pix10, (unsigned long)maximum, elapsed / 12);
}

static void upscale(unsigned factor, int pix10)
{
   struct texture_image img;
   uint32_t *expected;
   unsigned x, y, dw = 7 * factor, dh = 3 * factor;
   unsigned step = 65536 / factor;
   init(&img, 7, 3, pix10);
   expected = (uint32_t*)malloc((size_t)dw * dh * 4);
   assert(expected);
   for (y = 0; y < dh; y++)
      for (x = 0; x < dw; x++)
         expected[(size_t)y * dw + x] = img.pixels[
            (size_t)((y * step) >> 16) * 7 + ((x * step) >> 16)];
   assert(image_texture_scale(&img, 3 * factor, 0));
   assert(img.width == dw && img.height == dh);
   assert(img.pix10 == (pix10 != 0) && img.supports_rgba);
   assert(calls == 1 && source_frees == 1);
   assert(filter_calls == ((factor & (factor - 1)) != 0));
   assert(!memcmp(img.pixels, expected, (size_t)dw * dh * 4));
   image_free(img.pixels);
   free(expected);
   assert(!live);
}

static void upscale_failures(void)
{
   struct texture_image img, dst;
   uint32_t pixel = 0;
   init(&img, 7, 3, 0);
   budget = 0;
   assert(!image_texture_scale(&img, 6, 0));
   assert(img.width == 7 && img.height == 3 && !source_frees);
   assert(!filter_calls && !live);
   budget = 1;
   calls = 0;
   assert(image_texture_scale(&img, 6, 0));
   assert(source_frees == 1 && !filter_calls);
   image_free(img.pixels);
   assert(!live);
   budget = -1;
   memset(&img, 0, sizeof(img));
   memset(&dst, 0, sizeof(dst));
   img.pixels = &pixel;
   img.width = UINT_MAX / 2 + 2;
   img.height = 1;
   calls = filter_calls = 0;
   assert(!upscale_image(2, &img, &dst));
   assert(!calls && !filter_calls && !dst.pixels);
   img.width = img.height = 65537;
   assert(!upscale_image(2, &img, &dst));
   assert(!calls && !dst.pixels);
}

static void benchmark_upscale(unsigned factor, int pix10)
{
   struct texture_image img;
   unsigned repeat, generated = 0;
   double elapsed = 0.0;
   for (repeat = 0; repeat < 40; repeat++)
   {
      clock_t start;
      init(&img, 320, 240, pix10);
      start = clock();
      assert(image_texture_scale(&img, 240 * factor, 0));
      elapsed += 1000.0 * (clock() - start) / CLOCKS_PER_SEC;
      generated += filter_calls;
      image_free(img.pixels);
      assert(!live);
   }
   printf("320x240 upscale %ux pix10=%d: %.3f ms, %u filter setups / 40 calls\n",
         factor, pix10, elapsed / 40, generated);
}

int main(int argc, char **argv)
{
   int pix10;
   if (argc > 1)
   {
      if (!strcmp(argv[1], "--bench-upscale"))
      {
         for (pix10 = 0; pix10 < 2; pix10++)
         {
            benchmark_upscale(2, pix10);
            benchmark_upscale(4, pix10);
            benchmark_upscale(8, pix10);
         }
         return 0;
      }
      benchmark(0);
      benchmark(1);
      return 0;
   }
   for (pix10 = 0; pix10 < 2; pix10++)
   {
      exact(16, 8, 8, pix10);
      exact(32, 16, 8, pix10);
      upscale(2, pix10);
      upscale(3, pix10);
      upscale(4, pix10);
      upscale(5, pix10);
      upscale(8, pix10);
      upscale(16, pix10);
   }
   failures();
   upscale_failures();
   puts("PASS: 8/10-bit box and upscale pixels, ownership, bounds, failure/retry");
   return 0;
}
