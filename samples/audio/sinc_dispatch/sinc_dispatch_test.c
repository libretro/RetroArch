/* Which kernel sinc init() selects for every mask, quality and HQ
 * shape. Outputs of the vector kernels match the scalar one within
 * rounding, so only the selection itself shows a fallback. */

#include <stdio.h>
#include <stdlib.h>

#include "../../../libretro-common/audio/resampler/drivers/sinc_resampler.c"

typedef void (*sinc_kernel_t)(void *, struct resampler_data *);

static unsigned failures;

/* The widest arm this build compiles that the mask allows. */
static sinc_kernel_t expected(resampler_simd_mask_t mask, int kaiser,
      int avx_tier)
{
   (void)avx_tier;
#if defined(__AVX__)
   if ((mask & RESAMPLER_SIMD_AVX) && avx_tier)
      return kaiser ? resampler_sinc_process_avx_kaiser
                    : resampler_sinc_process_avx;
#endif
#if defined(__SSE__)
   if (mask & RESAMPLER_SIMD_SSE)
      return kaiser ? resampler_sinc_process_sse_kaiser
                    : resampler_sinc_process_sse;
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
#if defined(__AVX__)
   if (k == resampler_sinc_process_avx_kaiser) return "avx_kaiser";
   if (k == resampler_sinc_process_avx)        return "avx";
#endif
#if defined(__SSE__)
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
      { "neon",    RESAMPLER_SIMD_NEON },
      { "all",     ~(resampler_simd_mask_t)0 }
   };
   unsigned s, m;

   for (s = 0; s < sizeof(shapes) / sizeof(shapes[0]); s++)
      for (m = 0; m < sizeof(masks) / sizeof(masks[0]); m++)
      {
         sinc_kernel_t want = expected(masks[m].mask, shapes[s].kaiser,
               shapes[s].avx_tier);
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
         /* Stages run the one-phase layout through the same choice;
          * the shadow is the kernel's twin. */
         if (     re->dec[0].process
               != expected(masks[m].mask, 0, shapes[s].avx_tier)
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

   printf("Sinc dispatch: %u failures\n", failures);
   return failures ? 1 : 0;
}
