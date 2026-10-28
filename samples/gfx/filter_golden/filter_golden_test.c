/* A video filter (gfx/video_filters/<name>.c, chosen with FILTER_C and
 * its table with GOLDEN_H at build time; the Makefile builds one binary
 * per filter it lists) against output hashes recorded from its earlier
 * implementation: game-like frames (flat areas, banded
 * gradients, tiles, outlined sprites, checkerboard dither and dither in
 * alternating rows) and noise, each pixel
 * format the filter takes, widths and heights from 1 up, frames
 * smaller than the size
 * the filter was created for, and one to four workers. Two, three and
 * four workers must also give one worker's output byte for byte: the
 * neighbourhoods that reach rows below must clamp against the frame,
 * not the worker's slice. One worker created with the SIMD mask
 * RetroArch would pass on this machine must give the same bytes as one
 * created with none, so a filter's vector paths are held to its plain
 * ones. Any change to how
 * it computes must keep every byte; run with GOLDEN_PRINT=1 in the
 * environment to print the table for a deliberate change. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include FILTER_C

static int gf(void *u, const char *k, float *v, float d) { (void)u; (void)k; *v = d; return 0; }
static int gi(void *u, const char *k, int *v, int d) { (void)u; (void)k; *v = d; return 0; }
static int gh(void *u, const char *k, unsigned *v, unsigned d) { (void)u; (void)k; *v = d; return 0; }
static int gfa(void *u, const char *k, float **v, unsigned *n, const float *d, unsigned dn)
{ (void)u; (void)k; (void)d; (void)dn; *v = NULL; *n = 0; return 0; }
static int gia(void *u, const char *k, int **v, unsigned *n, const int *d, unsigned dn)
{ (void)u; (void)k; (void)d; (void)dn; *v = NULL; *n = 0; return 0; }
/* GOLDEN_STR, when built with it, answers every string setting (the
 * Blargg NTSC SNES tvtype), so a filter's other setups get tables of
 * their own. */
static int gs(void *u, const char *k, char **v, const char *d)
{
   (void)u; (void)k; (void)d;
#ifdef GOLDEN_STR
   *v = strdup(GOLDEN_STR);
   return *v != NULL;
#else
   *v = NULL;
   return 0;
#endif
}

/* The SIMD bits RetroArch's cpu_features_get() would report here, for
 * the ones filters look at; 0 where the compiler cannot tell. */
static unsigned host_simd_mask(void)
{
   unsigned mask = 0;
#if (defined(__GNUC__) || defined(__clang__)) \
   && (defined(__x86_64__) || defined(__i386__))
   __builtin_cpu_init();
   if (__builtin_cpu_supports("sse2"))
      mask |= SOFTFILTER_SIMD_SSE | SOFTFILTER_SIMD_SSE2;
   if (__builtin_cpu_supports("avx2"))
      mask |= SOFTFILTER_SIMD_AVX2;
   if (     __builtin_cpu_supports("avx2")
         && __builtin_cpu_supports("avx512f")
         && __builtin_cpu_supports("avx512bw")
         && __builtin_cpu_supports("avx512dq")
         && __builtin_cpu_supports("avx512vl"))
      mask |= SOFTFILTER_SIMD_AVX512;
#endif
   return mask;
}

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
         /* Rows alternating between two colours: dither that runs
          * down but not across */
         if (y > 2 * H / 3 && y < 2 * H / 3 + 12 && x > W / 2 && x + 4 < W)
            c = (y & 1) ? 0x39e7 : 0x6b2d;
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
/* Room for the largest frame at the largest scale (4x of 320x240, five
 * pixels of slack a row, 4 bytes a pixel). */
#define GOLDEN_OUT_BYTES (1290 * 960 * 4)

static const uint32_t golden[2][2][NSIZE] = {
#include GOLDEN_H
};

int main(void)
{
   struct softfilter_config cfg = { gf, gi, gh, gfa, gia, gs, free };
   const struct softfilter_implementation *impl = softfilter_get_implementation(0);
   uint16_t *src16 = (uint16_t*)malloc(330 * 250 * 2);
   uint32_t *src32 = (uint32_t*)malloc(330 * 250 * 4);
   uint8_t  *out   = (uint8_t*)malloc(GOLDEN_OUT_BYTES);
   /* One worker's output, which more workers must match. */
   uint8_t  *one   = (uint8_t*)malloc(GOLDEN_OUT_BYTES);
   unsigned content, f, s, t, bad = 0;
   unsigned simd = host_simd_mask();
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
            if (!(impl->query_input_formats() & (f ? SOFTFILTER_FMT_XRGB8888
                        : SOFTFILTER_FMT_RGB565)))
               h = 0; /* a format the filter does not take */
            else
            /* One to four workers, then (t == 5) one worker created
             * with the host's SIMD mask, which must also give the
             * first run's output byte for byte; the hash covers the
             * first four. */
            for (t = 1; t <= (simd ? 5u : 4u); t++)
            {
               unsigned fmt = f ? SOFTFILTER_FMT_XRGB8888 : SOFTFILTER_FMT_RGB565;
               unsigned outs = impl->query_output_formats(fmt);
               unsigned ofmt = (outs & fmt) ? fmt
                  : (outs & SOFTFILTER_FMT_XRGB8888) ? SOFTFILTER_FMT_XRGB8888
                  : SOFTFILTER_FMT_RGB565;
               unsigned bpp = f ? 4 : 2, ow, oh, k, max_w, max_h;
               unsigned obpp = (ofmt == SOFTFILTER_FMT_XRGB8888) ? 4 : 2;
               struct softfilter_work_packet pk[4];
               size_t is = (W + 3) * bpp, os;
               void *d;
               /* Created larger than the frame, as RetroArch does for a
                * core whose size varies; every second run exactly. */
               memset(pk, 0, sizeof(pk));
               max_w = (t & 1) ? W + 13 : W;
               max_h = (t & 1) ? H + 7  : H;
               d     = impl->create(&cfg, fmt, ofmt, max_w, max_h,
                     t == 5 ? 1 : t, t == 5 ? simd : 0, NULL);
               impl->query_output_size(d, &ow, &oh, W, H);
               /* Rows of the output five pixels wider than the filter
                * writes, which the hash covers, so a write past a row
                * shows. */
               os    = (size_t)(ow + 5) * obpp;
               /* Only what the hash and the comparison cover: clearing
                * the whole 5 MB buffer for every run was most of the
                * suite's time under the sanitizers. */
               memset(out, 0x11, os * oh);
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
               if (t < 5)
                  h = fnv1a(out, os * oh, h);
               if (t == 1)
                  memcpy(one, out, os * oh);
               else if (memcmp(one, out, os * oh))
               {
                  if (t == 5)
                     printf("FAIL %s %s %ux%u: SIMD mask 0x%x differs from 0\n",
                           content ? "noise" : "game", f ? "xrgb8888" : "rgb565",
                           W, H, simd);
                  else
                     printf("FAIL %s %s %ux%u: %u workers differ from one\n",
                           content ? "noise" : "game", f ? "xrgb8888" : "rgb565",
                           W, H, t);
                  bad++;
               }
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
   free(one);
   if (print)
      return 0;
   if (bad)
   {
      printf("%s: %u mismatches\n", FILTER_NAME, bad);
      return 1;
   }
   printf("%s: output matches the recorded hashes\n", FILTER_NAME);
   return 0;
}
