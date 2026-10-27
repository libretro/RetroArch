/* The NTSC-Multi-System video filter (gfx/video_filters/ntsc.c) against
 * output hashes recorded from its original per-tap implementation: every
 * system mode (NES, PAL, Atari, C64, C64 + PAL), sharpness, hue and
 * saturation, both artifact extremes, both pixel formats, widths from 1
 * to past the 1024 cap, one and three worker threads, and three frames
 * in a row (the NES carrier phase flips each frame). Any change to how
 * it computes must keep every byte; run with GOLDEN_PRINT=1 in the
 * environment to print the table for a deliberate change of output. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "../../../gfx/video_filters/ntsc.c"

struct kv { const char *k; float v; };
static const struct kv *cur;
static int ncur;

static int gf(void *u, const char *k, float *v, float d)
{
   int i;
   (void)u;
   *v = d;
   for (i = 0; i < ncur; i++)
      if (!strcmp(cur[i].k, k)) { *v = cur[i].v; return 1; }
   return 0;
}
static int gi(void *u, const char *k, int *v, int d) { (void)u; (void)k; *v = d; return 0; }
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

static const struct kv scenarios[][4] = {
   { { "x", 0 } },
   { { "pal_mode", 1 } },
   { { "atari_mode", 1 } },
   { { "c64_mode", 1 } },
   { { "c64_mode", 1 }, { "pal_mode", 1 } },
   { { "sharpness", 0.5f } },
   { { "hue", 30 }, { "saturation", 1.4f } },
   { { "artifacts", 0 } },
   { { "artifacts", 1 } },
   { { "atari_mode", 1 }, { "sharpness", 1 }, { "hue", -45 }, { "saturation", 0.3f } }
};
static const int nkv[] = { 1, 1, 1, 1, 2, 1, 2, 1, 1, 4 };
static const unsigned widths[] = { 1, 2, 7, 64, 255, 320, 1024, 1100 };

#define NSCEN  (sizeof(nkv) / sizeof(nkv[0]))
#define NWIDTH (sizeof(widths) / sizeof(widths[0]))

static const uint32_t golden[NSCEN][2][NWIDTH] = {
#include "golden.h"
};

int main(void)
{
   struct softfilter_config cfg = { gf, gi, gh, gfa, gia, gs, free };
   const struct softfilter_implementation *impl = softfilter_get_implementation(0);
   unsigned char *in  = (unsigned char*)malloc(1100 * 40 * 4);
   unsigned char *out = (unsigned char*)malloc(2048 * 40 * 4);
   unsigned s, f, w, t, fr, i, bad = 0;
   int print = getenv("GOLDEN_PRINT") != NULL;

   for (i = 0; i < 1100 * 40 * 4; i++)
      in[i] = (unsigned char)(i * 131u ^ (i >> 7) * 29u);

   for (s = 0; s < NSCEN; s++)
   {
      if (print) printf("{ ");
      for (f = 0; f < 2; f++)
      {
         if (print) printf("{ ");
         for (w = 0; w < NWIDTH; w++)
         {
            uint32_t h = 2166136261u;
            for (t = 1; t <= 3; t += 2)
            {
               unsigned fmt = f ? SOFTFILTER_FMT_XRGB8888 : SOFTFILTER_FMT_RGB565;
               unsigned bpp = f ? 4 : 2, W = widths[w], H = 37, ow, oh;
               void *d;
               cur  = scenarios[s];
               ncur = nkv[s];
               d    = impl->create(&cfg, fmt, fmt, 1100, 40, t, 0, NULL);
               impl->query_output_size(d, &ow, &oh, W, H);
               for (fr = 0; fr < 3; fr++)
               {
                  struct softfilter_work_packet pk[4];
                  size_t os = (ow > 2048 ? 2048 : ow) * bpp + 16;
                  unsigned k;
                  memset(out, 0x11, 2048 * 40 * 4);
                  impl->get_work_packets(d, pk, out, os, in, W, H, W * bpp + bpp);
                  for (k = 0; k < impl->query_num_threads(d); k++)
                     pk[k].work(d, pk[k].thread_data);
                  h = fnv1a(out, os * H, h);
               }
               impl->destroy(d);
            }
            if (print)
               printf("0x%08xu%s", h, w + 1 < NWIDTH ? ", " : " ");
            else if (h != golden[s][f][w])
            {
               printf("FAIL scenario %u format %s width %u: 0x%08x, recorded 0x%08x\n",
                     s, f ? "xrgb8888" : "rgb565", widths[w],
                     (unsigned)h, (unsigned)golden[s][f][w]);
               bad++;
            }
         }
         if (print) printf("}%s", f ? " " : ", ");
      }
      if (print) printf("},\n");
   }
   free(in);
   free(out);
   if (print)
      return 0;
   if (bad)
   {
      printf("ntsc_filter_golden: %u mismatches\n", bad);
      return 1;
   }
   printf("ntsc_filter_golden: output matches the recorded hashes\n");
   return 0;
}
