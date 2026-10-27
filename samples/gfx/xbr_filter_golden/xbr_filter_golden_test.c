/* The 2xBR video filter (gfx/video_filters/2xbr.c) against output hashes
 * recorded from its earlier implementation: game-like frames (flat
 * areas, banded gradients, tiles, outlined sprites, dither) and noise,
 * both pixel formats, widths and heights from 1 up, frames smaller than
 * the size the filter was created for, and one to four workers. Any
 * change to how it computes must keep every byte; run with GOLDEN_PRINT=1
 * in the environment to print the table for a deliberate change. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "../../../gfx/video_filters/2xbr.c"

static int gf(void *u, const char *k, float *v, float d) { (void)u; (void)k; *v = d; return 0; }
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

static uint32_t rng = 12345u;
static unsigned rnd(void) { rng = rng * 1103515245u + 12345u; return rng >> 8; }

/* A game-like RGB565 frame. */
static void game_frame(uint16_t *p, unsigned W, unsigned H)
{
   unsigned x, y, s;
   for (y = 0; y < H; y++)
      for (x = 0; x < W; x++)
      {
         uint16_t c;
         if (y < H / 2)
            c = (uint16_t)(((y / 12) & 31) | ((20 + (y / 24)) << 5) | (31 << 11));
         else
         {
            unsigned lx = x % 16, ly = y % 16;
            c = (((x / 16) + (y / 16)) & 1) ? 0x2c40 : 0x3460;
            if (lx == 0 || ly == 0)
               c = 0x1a20;
            if (((lx * 7 + ly * 3) % 13) == 0)
               c = 0x4c80;
         }
         if (y > H / 3 && y < H / 3 + 20 && ((x + y) & 1) && x > 4 && x < W / 2)
            c = 0x7bef;
         p[y * W + x] = c;
      }
   for (s = 0; s < 12 && W > 4 && H > 4; s++)
   {
      unsigned w  = 2 + rnd() % (W < 26 ? W - 2 : 24);
      unsigned h  = 2 + rnd() % (H < 26 ? H - 2 : 24);
      unsigned sx = rnd() % (W - w + 1), sy = rnd() % (H - h + 1);
      uint16_t c  = (uint16_t)rnd();
      for (y = sy; y < sy + h; y++)
         for (x = sx; x < sx + w; x++)
            p[y * W + x] = (x == sx || x == sx + w - 1 || y == sy
                  || y == sy + h - 1) ? 0 : c;
   }
}

static const unsigned sizes[][2] = {
   { 1, 1 }, { 2, 1 }, { 1, 3 }, { 3, 3 }, { 5, 2 }, { 7, 9 },
   { 64, 48 }, { 255, 31 }, { 256, 224 }, { 320, 240 }
};
#define NSIZE (sizeof(sizes) / sizeof(sizes[0]))

static const uint32_t golden[2][2][NSIZE] = {
#include "golden.h"
};

int main(void)
{
   struct softfilter_config cfg = { gf, gi, gh, gfa, gia, gs, free };
   const struct softfilter_implementation *impl = softfilter_get_implementation(0);
   uint16_t *src16 = (uint16_t*)malloc(330 * 250 * 2);
   uint32_t *src32 = (uint32_t*)malloc(330 * 250 * 4);
   uint8_t  *out   = (uint8_t*)malloc(700 * 500 * 4);
   unsigned content, f, s, t, bad = 0;
   int print = getenv("GOLDEN_PRINT") != NULL;

   for (content = 0; content < 2; content++)
   {
      if (print) printf("{ ");
      for (f = 0; f < 2; f++)
      {
         if (print) printf("{ ");
         for (s = 0; s < NSIZE; s++)
         {
            unsigned W = sizes[s][0], H = sizes[s][1], i;
            uint32_t h = 2166136261u;
            rng = 777u + s;
            if (content == 0)
               game_frame(src16, W, H);
            else
               for (i = 0; i < W * H; i++)
                  src16[i] = (uint16_t)rnd();
            for (i = 0; i < W * H; i++)
            {
               uint16_t c = src16[i];
               /* Low bits set too, so the 8888 path's full-precision
                * blends and 565-precision decisions both show. */
               src32[i] = 0xff000000u | ((uint32_t)(c >> 11) << 19)
                  | ((uint32_t)((c >> 5) & 63) << 10)
                  | ((uint32_t)(c & 31) << 3) | (rnd() & 0x070307);
            }
            for (t = 1; t <= 4; t++)
            {
               unsigned fmt = f ? SOFTFILTER_FMT_XRGB8888 : SOFTFILTER_FMT_RGB565;
               unsigned bpp = f ? 4 : 2, ow, oh, k, max_w, max_h;
               struct softfilter_work_packet pk[4];
               size_t is = (W + 3) * bpp, os = (2 * W + 5) * bpp;
               void *d;
               /* Created larger than the frame, as RetroArch does for a
                * core whose size varies; every second run exactly. */
               max_w = (t & 1) ? W + 13 : W;
               max_h = (t & 1) ? H + 7  : H;
               d     = impl->create(&cfg, fmt, fmt, max_w, max_h, t, 0, NULL);
               impl->query_output_size(d, &ow, &oh, W, H);
               memset(out, 0x11, 700 * 500 * 4);
               {
                  /* Rows at a stride wider than the frame. */
                  uint8_t *in = (uint8_t*)calloc(H, is);
                  unsigned y;
                  for (y = 0; y < H; y++)
                     memcpy(in + y * is, f ? (void*)(src32 + y * W)
                           : (void*)(src16 + y * W), W * bpp);
                  impl->get_work_packets(d, pk, out, os, in, W, H, is);
                  for (k = 0; k < impl->query_num_threads(d); k++)
                     pk[k].work(d, pk[k].thread_data);
                  free(in);
               }
               h = fnv1a(out, os * oh, h);
               impl->destroy(d);
            }
            if (print)
               printf("0x%08xu%s", h, s + 1 < NSIZE ? ", " : " ");
            else if (h != golden[content][f][s])
            {
               printf("FAIL %s %s %ux%u: 0x%08x, recorded 0x%08x\n",
                     content ? "noise" : "game", f ? "xrgb8888" : "rgb565",
                     W, H, (unsigned)h, (unsigned)golden[content][f][s]);
               bad++;
            }
         }
         if (print) printf("}%s", f ? " " : ", ");
      }
      if (print) printf("},\n");
   }
   free(src16);
   free(src32);
   free(out);
   if (print)
      return 0;
   if (bad)
   {
      printf("xbr_filter_golden: %u mismatches\n", bad);
      return 1;
   }
   printf("xbr_filter_golden: output matches the recorded hashes\n");
   return 0;
}
