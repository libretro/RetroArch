/* Exact box decimation transfers its image; no second destination is needed. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <formats/image.h>

static struct { void *ptr; size_t bytes; } buffers[8];
static unsigned calls, source_frees;
static size_t live, peak;
static int budget = -1;
static void *source_pixels;

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
#include SCALING_SOURCE
#undef malloc
#undef free

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

int main(int argc, char **argv)
{
   int pix10;
   (void)argv;
   if (argc > 1)
   {
      benchmark(0);
      benchmark(1);
      return 0;
   }
   for (pix10 = 0; pix10 < 2; pix10++)
   {
      exact(16, 8, 8, pix10);
      exact(32, 16, 8, pix10);
   }
   failures();
   puts("PASS: 8/10-bit exact box pixels, one-allocation ownership transfer, failure/retry, sinc fallback");
   return 0;
}
