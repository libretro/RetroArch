/* The NTSC-CRT video filter (gfx/video_filters/ntsc_crt.c) against output
 * hashes recorded from its earlier implementation: noise off, default
 * and heavy, scanlines, blend, monochrome, hue, contrast, brightness,
 * saturation, black and white point, both input formats (by turns: the
 * format only changes the conversion before modulation), several frame
 * sizes (smaller than the size it was created for too), one and seven
 * workers, and four frames in a row - field parity, frame parity and the
 * noise generator all carry from frame to frame. The whole table is
 * checked once per SIMD tier the CPU has - line by line, 8 lines in
 * AVX2 lanes, 16 in AVX-512 - since the tier comes from the SIMD mask
 * given to create. Any change to how it computes must keep every byte;
 * run with GOLDEN_PRINT=1 in the environment to print the table for a
 * deliberate change. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "../../../gfx/video_filters/ntsc_crt.c"

struct kv { const char *k; int v; };
static const struct kv *cur;
static int ncur;

static int gf(void *u, const char *k, float *v, float d) { (void)u; (void)k; *v = d; return 0; }
static int gi(void *u, const char *k, int *v, int d)
{
   int i;
   (void)u;
   *v = d;
   for (i = 0; i < ncur; i++)
      if (!strcmp(cur[i].k, k)) { *v = cur[i].v; return 1; }
   return 0;
}
static int gh(void *u, const char *k, unsigned *v, unsigned d) { (void)u; (void)k; *v = d; return 0; }
static int gfa(void *u, const char *k, float **v, unsigned *n, const float *d, unsigned dn)
{ (void)u; (void)k; (void)d; (void)dn; *v = NULL; *n = 0; return 0; }
static int gia(void *u, const char *k, int **v, unsigned *n, const int *d, unsigned dn)
{ (void)u; (void)k; (void)d; (void)dn; *v = NULL; *n = 0; return 0; }
static int gs(void *u, const char *k, char **v, const char *d) { (void)u; (void)k; (void)d; *v = NULL; return 0; }

static uint32_t fnv1a(const unsigned char *p, size_t n, uint32_t h)
{
   while (n--)
      h = (h ^ *p++) * 16777619u;
   return h;
}

static const struct kv scenarios[][5] = {
   { { "x", 0 } },
   { { "noise", 0 } },
   { { "noise", 12 } },
   { { "scanlines", 1 } },
   { { "blend", 1 } },
   { { "as_color", 0 } },
   { { "hue", 90 }, { "saturation", 18 } },
   { { "contrast", 255 }, { "brightness", 20 } },
   { { "black_point", 10 }, { "white_point", 90 }, { "noise", 4 } },
   { { "blend", 1 }, { "scanlines", 1 }, { "noise", 7 }, { "hue", 300 }, { "contrast", 120 } }
};
static const int nkv[] = { 1, 1, 1, 1, 1, 1, 2, 2, 3, 5 };
static const unsigned sizes[][2] = { { 1, 1 }, { 7, 5 }, { 256, 224 }, { 320, 240 } };
/* Workers and whether the filter is created larger than the frame. Every
 * CRT frame costs about the same whatever its size, so these are few:
 * one worker, and seven, whose bands of 34 and 35 lines leave lines over
 * at both lane counts, for the line-by-line decoder to finish. */
static const unsigned configs[][2] = { { 1, 0 }, { 7, 1 } };
#define NCONFIG (sizeof(configs) / sizeof(configs[0]))

#define NSCEN (sizeof(nkv) / sizeof(nkv[0]))
#define NSIZE (sizeof(sizes) / sizeof(sizes[0]))

static const uint32_t golden[NSCEN][NSIZE] = {
#include "golden.h"
};

/* The SIMD masks to create the filter with: scalar, then each wider
 * tier this CPU and OS can run. */
static unsigned tier_masks(unsigned *masks)
{
   unsigned n = 0;
   masks[n++] = 0;
#if (defined(__GNUC__) || defined(__clang__)) \
   && (defined(__x86_64__) || defined(__i386__))
   __builtin_cpu_init();
   if (__builtin_cpu_supports("avx2"))
      masks[n++] = SOFTFILTER_SIMD_AVX2;
   if (     __builtin_cpu_supports("avx2")
         && __builtin_cpu_supports("avx512f")
         && __builtin_cpu_supports("avx512bw")
         && __builtin_cpu_supports("avx512dq")
         && __builtin_cpu_supports("avx512vl"))
      masks[n++] = SOFTFILTER_SIMD_AVX2 | SOFTFILTER_SIMD_AVX512;
#endif
   return n;
}

int main(void)
{
   struct softfilter_config cfg = { gf, gi, gh, gfa, gia, gs, free };
   unsigned masks[3], nmask = tier_masks(masks), mi;
   const struct softfilter_implementation *impl = softfilter_get_implementation(0);
   uint32_t *src32 = (uint32_t*)malloc(320 * 240 * 4);
   uint16_t *src16 = (uint16_t*)malloc(320 * 240 * 2);
   uint8_t  *out   = (uint8_t*)malloc(340 * 250 * 4);
   unsigned s, f, z, t, fr, i, bad = 0;
   uint32_t rng = 99u;
   int print = getenv("GOLDEN_PRINT") != NULL;

   for (i = 0; i < 320 * 240; i++)
   {
      unsigned x = i % 320, y = i / 320;
      uint32_t c = (y < 120)
         ? (((x / 20) & 1) ? 0xff2060e0u : 0xffe0c020u)
         : (((x + y) / 16) & 1) ? 0xff30a030u : 0xff806040u;
      rng = rng * 1103515245u + 12345u;
      if (x % 37 < 3)
         c = 0xff000000u | (rng >> 8);
      src32[i] = c;
      src16[i] = (uint16_t)(((c >> 19) & 31) << 11 | ((c >> 10) & 63) << 5 | ((c >> 3) & 31));
   }

   for (mi = 0; mi < (print ? 1u : nmask); mi++)
   for (s = 0; s < NSCEN; s++)
   {
      if (print) printf("{ ");
      f = s & 1;
      {
         for (z = 0; z < NSIZE; z++)
         {
            unsigned W = sizes[z][0], H = sizes[z][1];
            uint32_t h = 2166136261u;
            for (t = 0; t < NCONFIG; t++)
            {
               unsigned nthr = configs[t][0], bigger = configs[t][1];
               unsigned fmt = f ? SOFTFILTER_FMT_XRGB8888 : SOFTFILTER_FMT_RGB565;
               unsigned bpp = f ? 4 : 2, ow, oh, k;
               size_t is = (size_t)(W + 2) * bpp, os = (size_t)(W + 3) * 4;
               uint8_t *in = (uint8_t*)calloc(H, is);
               void *d;
               unsigned y;
               cur  = scenarios[s];
               ncur = nkv[s];
               d    = impl->create(&cfg, fmt, SOFTFILTER_FMT_XRGB8888,
                     bigger ? W + 9 : W, bigger ? H + 6 : H, nthr,
                     masks[mi], NULL);
               impl->query_output_size(d, &ow, &oh, W, H);
               for (y = 0; y < H; y++)
                  memcpy(in + y * is, f ? (void*)(src32 + y * 320)
                        : (void*)(src16 + y * 320), W * bpp);
               for (fr = 0; fr < 4; fr++)
               {
                  struct softfilter_work_packet pk[8];
                  memset(out, 0x11, 340 * 250 * 4);
                  impl->get_work_packets(d, pk, out, os, in, W, H, is);
                  for (k = 0; k < impl->query_num_threads(d); k++)
                     pk[k].work(d, pk[k].thread_data);
                  h = fnv1a(out, os * oh, h);
               }
               impl->destroy(d);
               free(in);
            }
            if (print)
               printf("0x%08xu%s", h, z + 1 < NSIZE ? ", " : " ");
            else if (h != golden[s][z])
            {
               printf("FAIL mask 0x%x scenario %u %s %ux%u: 0x%08x, recorded 0x%08x\n",
                     masks[mi], s, f ? "xrgb8888" : "rgb565", W, H,
                     (unsigned)h, (unsigned)golden[s][z]);
               bad++;
            }
         }
      }
      if (print) printf("},\n");
   }
   free(src32);
   free(src16);
   free(out);
   if (print)
      return 0;
   if (bad)
   {
      printf("crt_filter_golden: %u mismatches\n", bad);
      return 1;
   }
   printf("crt_filter_golden: output matches the recorded hashes "
         "at %u SIMD tier(s)\n", nmask);
   return 0;
}
