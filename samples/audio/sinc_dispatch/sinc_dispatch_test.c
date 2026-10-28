/* Which kernel sinc init() selects for every mask, quality and HQ
 * shape. Outputs of the vector kernels match the scalar one within
 * rounding, so only the selection itself shows a fallback. */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <features/features_cpu.h>

#include "../../../libretro-common/audio/resampler/drivers/sinc_resampler.c"

typedef void (*sinc_kernel_t)(void *, struct resampler_data *);

static unsigned failures;

/* The widest arm this build compiles that the mask allows; on x86 its
 * direct-read form up to that tier's crossover tap count. */
static sinc_kernel_t expected(resampler_simd_mask_t mask, int kaiser,
      int avx_tier, unsigned taps)
{
   (void)avx_tier;
   (void)taps;
#ifdef SINC_HAVE_AVX512
   if ((mask & RESAMPLER_SIMD_AVX512) && avx_tier)
      return (taps <= SINC_IL_TAPS_AVX512)
         ? (kaiser ? resampler_sinc_process_avx512_il_kaiser
                   : resampler_sinc_process_avx512_il)
         : (kaiser ? resampler_sinc_process_avx512_kaiser
                   : resampler_sinc_process_avx512);
#endif
#ifdef SINC_HAVE_AVX
   if ((mask & RESAMPLER_SIMD_AVX) && avx_tier)
      return (taps <= SINC_IL_TAPS_AVX)
         ? (kaiser ? resampler_sinc_process_avx_il_kaiser
                   : resampler_sinc_process_avx_il)
         : (kaiser ? resampler_sinc_process_avx_kaiser
                   : resampler_sinc_process_avx);
#endif
#if defined(__SSE__)
   if (mask & RESAMPLER_SIMD_SSE)
      return (taps <= SINC_IL_TAPS_SSE)
         ? (kaiser ? resampler_sinc_process_sse_il_kaiser
                   : resampler_sinc_process_sse_il)
         : (kaiser ? resampler_sinc_process_sse_kaiser
                   : resampler_sinc_process_sse);
#endif
#if (defined(__ARM_NEON) || defined(__ARM_NEON__) || defined(HAVE_NEON))
   if (mask & RESAMPLER_SIMD_NEON)
      return kaiser ? resampler_sinc_process_neon_kaiser
                    : resampler_sinc_process_neon;
#endif
   return kaiser ? resampler_sinc_process_c_kaiser
                 : resampler_sinc_process_c;
}

static const char *kernel_name(sinc_kernel_t k)
{
#ifdef SINC_HAVE_AVX512
   if (k == resampler_sinc_process_avx512_il_kaiser) return "avx512_il_kaiser";
   if (k == resampler_sinc_process_avx512_il)        return "avx512_il";
   if (k == resampler_sinc_process_avx512_kaiser) return "avx512_kaiser";
   if (k == resampler_sinc_process_avx512)        return "avx512";
#endif
#ifdef SINC_HAVE_AVX
   if (k == resampler_sinc_process_avx_il_kaiser) return "avx_il_kaiser";
   if (k == resampler_sinc_process_avx_il)        return "avx_il";
   if (k == resampler_sinc_process_avx_kaiser) return "avx_kaiser";
   if (k == resampler_sinc_process_avx)        return "avx";
#endif
#if defined(__SSE__)
   if (k == resampler_sinc_process_sse_il_kaiser) return "sse_il_kaiser";
   if (k == resampler_sinc_process_sse_il)        return "sse_il";
   if (k == resampler_sinc_process_sse_kaiser) return "sse_kaiser";
   if (k == resampler_sinc_process_sse)        return "sse";
#endif
#if (defined(__ARM_NEON) || defined(__ARM_NEON__) || defined(HAVE_NEON))
   if (k == resampler_sinc_process_neon_kaiser) return "neon_kaiser";
   if (k == resampler_sinc_process_neon)        return "neon";
#endif
   if (k == resampler_sinc_process_c_kaiser)   return "c_kaiser";
   if (k == resampler_sinc_process_c)          return "c";
   return "?";
}

int main(void)
{
   static const struct
   {
      const char *name;
      enum resampler_quality quality;
      int hq;
      int kaiser;
      int avx_tier;
   } shapes[] = {
      { "lowest",  RESAMPLER_QUALITY_LOWEST,  0, 0, 0 },
      { "lower",   RESAMPLER_QUALITY_LOWER,   0, 0, 0 },
      { "normal",  RESAMPLER_QUALITY_NORMAL,  0, 1, 0 },
      { "higher",  RESAMPLER_QUALITY_HIGHER,  0, 1, 1 },
      { "highest", RESAMPLER_QUALITY_HIGHEST, 0, 1, 1 },
      { "hq",      RESAMPLER_QUALITY_NORMAL,  1, 1, 1 }
   };
   static const struct
   {
      const char *name;
      resampler_simd_mask_t mask;
   } masks[] = {
      { "none",    0 },
      { "sse",     RESAMPLER_SIMD_SSE },
      { "avx",     RESAMPLER_SIMD_AVX },
      { "sse+avx", RESAMPLER_SIMD_SSE | RESAMPLER_SIMD_SSE2
                 | RESAMPLER_SIMD_AVX },
      { "avx512",  RESAMPLER_SIMD_SSE | RESAMPLER_SIMD_AVX
                 | RESAMPLER_SIMD_AVX512 },
      { "neon",    RESAMPLER_SIMD_NEON },
      { "all",     ~(resampler_simd_mask_t)0 }
   };
   unsigned s, m;

   for (s = 0; s < sizeof(shapes) / sizeof(shapes[0]); s++)
      for (m = 0; m < sizeof(masks) / sizeof(masks[0]); m++)
      {
         sinc_kernel_t want;
         rarch_sinc_resampler_t *re = (rarch_sinc_resampler_t*)
            sinc_resampler_init_hq(4.0, shapes[s].quality, masks[m].mask,
                  shapes[s].hq);
         if (!re)
         {
            printf("FAIL %s/%s: init failed\n", shapes[s].name,
                  masks[m].name);
            failures++;
            continue;
         }
         want = expected(masks[m].mask, shapes[s].kaiser,
               shapes[s].avx_tier, re->taps);
         /* Stages run the one-phase layout through the same choice;
          * the shadow is the kernel's twin. */
         if (     re->dec[0].process
               != expected(masks[m].mask, 0, shapes[s].avx_tier,
                  re->dec[0].taps)
               || re->dec[SINC_DEC_STAGES].process != re->process)
         {
            printf("FAIL %s/%s: stage %s, shadow %s\n", shapes[s].name,
                  masks[m].name, kernel_name(re->dec[0].process),
                  kernel_name(re->dec[SINC_DEC_STAGES].process));
            failures++;
         }
         if (re->process != want)
         {
            printf("FAIL %s/%s: %s, expected %s\n", shapes[s].name,
                  masks[m].name, kernel_name(re->process),
                  kernel_name(want));
            failures++;
         }
         else
            printf("   %-7s %-7s %s\n", shapes[s].name,
                  masks[m].name, kernel_name(re->process));
         resampler_sinc_free(re);
      }

#if defined(__SSE__)
   /* A requested arm this build lacks must not hide one it has. */
   {
      rarch_sinc_resampler_t *re = (rarch_sinc_resampler_t*)
         sinc_resampler_init_hq(4.0, RESAMPLER_QUALITY_NORMAL,
               RESAMPLER_SIMD_SSE | RESAMPLER_SIMD_AVX, 1);
      if (!re || re->process == resampler_sinc_process_c_kaiser)
      {
         printf("FAIL hq with SSE|AVX fell back to scalar\n");
         failures++;
      }
      resampler_sinc_free(re);
   }
#endif

   /* Every kernel the CPU can run must produce what the scalar one
    * does, to rounding (the FMA kernels round once per tap, not twice).
    * Each SIMD tier is pinned in turn, and the input is fed both as one
    * batch and as a stream of uneven batches, some shorter than the
    * filter, so the vector kernels' seam carries history across calls.
    * Kernels the CPU lacks are skipped rather than run. */
   {
      static float in[4096 * 2], ref[4096 * 8 * 2], out[4096 * 8 * 2];
      uint64_t cpu = cpu_features_get();
      static const resampler_simd_mask_t tiers[] = {
         0,
         RESAMPLER_SIMD_NEON,
         RESAMPLER_SIMD_SSE,
         RESAMPLER_SIMD_SSE | RESAMPLER_SIMD_AVX,
         RESAMPLER_SIMD_SSE | RESAMPLER_SIMD_AVX | RESAMPLER_SIMD_AVX512
      };
      static const double ratios[] = { 48000.0 / 32040.5, 48000.0 / 44100.0,
         44100.0 / 48000.0 };
      /* 0.95 widens the kernel to a tap count that is an odd multiple
       * of 8, which runs the AVX-512 kernels' tail block. */
      static const double bands[] = { 4.0, 0.95 };
      unsigned i, r, b, t, chunked, odd_taps = 0;
      for (i = 0; i < 4096; i++)
      {
         in[2 * i + 0] = (float)sin(i * 0.0137) * 0.7f;
         in[2 * i + 1] = (float)cos(i * 0.0211) * 0.5f;
      }
      for (t = 0; t < sizeof(tiers) / sizeof(tiers[0]); t++)
      {
         if ((tiers[t] & (resampler_simd_mask_t)cpu) != tiers[t])
            continue;
         for (s = 0; s < sizeof(shapes) / sizeof(shapes[0]); s++)
         for (b = 0; b < sizeof(bands) / sizeof(bands[0]); b++)
         for (r = 0; r < sizeof(ratios) / sizeof(ratios[0]); r++)
         for (chunked = 0; chunked < 2; chunked++)
         {
            size_t n_ref = 0, n_out = 0, at = 0;
            float worst = 0.0f;
            void *c = sinc_resampler_init_hq(bands[b], shapes[s].quality, 0,
                  shapes[s].hq);
            void *v = sinc_resampler_init_hq(bands[b], shapes[s].quality,
                  tiers[t], shapes[s].hq);
            uint32_t seed = 12345u + s * 7 + r;
            if (!c || !v)
            {
               printf("FAIL %s band %.2f: compare init\n", shapes[s].name,
                     bands[b]);
               failures++;
               resampler_sinc_free(c);
               resampler_sinc_free(v);
               continue;
            }
            if (((rarch_sinc_resampler_t*)v)->taps & 8)
               odd_taps++;
            while (at < 4096)
            {
               struct resampler_data d;
               size_t n = 4096 - at;
               if (chunked)
               {
                  seed = seed * 1103515245u + 12345u;
                  n    = 1 + (seed >> 16) % 700;
                  if ((seed >> 8) & 1)
                     n = 1 + (seed >> 20) % 24;
                  if (n > 4096 - at)
                     n = 4096 - at;
               }
               d.data_in      = in + 2 * at;
               d.input_frames = n;
               d.ratio        = ratios[r];
               d.data_out     = ref + 2 * n_ref;
               resampler_sinc_process(c, &d);
               n_ref         += d.output_frames;
               d.data_out     = out + 2 * n_out;
               resampler_sinc_process(v, &d);
               n_out         += d.output_frames;
               at            += n;
            }
            if (n_out != n_ref)
            {
               printf("FAIL %s: %u frames, scalar %u\n", shapes[s].name,
                     (unsigned)n_out, (unsigned)n_ref);
               failures++;
            }
            else
               for (i = 0; i < n_ref * 2; i++)
               {
                  float e = (float)fabs(out[i] - ref[i]);
                  if (e > worst)
                     worst = e;
               }
            if (worst > 1e-5f)
            {
               printf("FAIL %s band %.2f ratio %.4f %s: %s off scalar by %g\n",
                     shapes[s].name, bands[b], ratios[r],
                     chunked ? "chunked" : "one batch",
                     kernel_name(((rarch_sinc_resampler_t*)v)->process),
                     worst);
               failures++;
            }
            resampler_sinc_free(c);
            resampler_sinc_free(v);
         }
      }
      if (!odd_taps)
      {
         printf("FAIL no shape reached a tap count off a multiple of 16\n");
         failures++;
      }
   }

   printf("Sinc dispatch: %u failures\n", failures);
   return failures ? 1 : 0;
}
