/* Every CC kernel this build has, over the same stream, against the
 * scalar reference. The mask init() is given picks the pair, so a run
 * with mask 0 is the reference itself and any other run is the arm
 * that mask names. Build for another ISA and run it there - the
 * comparison is the same one. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <audio/audio_resampler.h>

extern retro_resampler_t CC_resampler;

/* The driver's own condition: above 4 it builds the reference alone,
 * so there is no vector arm to compare against. */
#ifndef CC_RESAMPLER_PRECISION
#define CC_RESAMPLER_PRECISION 1
#endif
#if (CC_RESAMPLER_PRECISION > 4)
#define CC_VECTOR_ARMS 0
#else
#define CC_VECTOR_ARMS 1
#endif

static unsigned failures;
#define CHECK(cond, ...) do { if (!(cond)) { printf("      FAIL: "); printf(__VA_ARGS__); printf("\n"); failures++; } } while (0)

#define IN_FRAMES 509            /* prime, so chunk edges land anywhere */
#define CHUNKS    12
/* The widest ratio driven below is 2.0. */
#define OUT_CAP   (IN_FRAMES * CHUNKS * 2 + 256)

static float in_buf[IN_FRAMES * 2];
static float out_ref[OUT_CAP * 2];
static float out_simd[OUT_CAP * 2];

static void fill(unsigned seed)
{
   unsigned i;
   uint32_t x = 0x9E3779B9u * (seed + 1);
   for (i = 0; i < IN_FRAMES * 2; i++)
   {
      x ^= x << 13; x ^= x >> 17; x ^= x << 5;
      in_buf[i] = ((float)(x & 0xFFFF) / 32768.0f - 1.0f) * 0.5f
                + 0.3f * sinf((float)i * 0.011f);
   }
}

/* One stream through one instance, chunk by chunk, into out. */
static size_t stream(resampler_simd_mask_t mask, double nominal,
      const double *ratios, size_t nratios, float *out)
{
   struct resampler_config cfg;
   struct resampler_data d;
   void *re;
   size_t c, total = 0;

   memset(&cfg, 0, sizeof(cfg));
   if (!(re = CC_resampler.init(&cfg, nominal, RESAMPLER_QUALITY_NORMAL, mask)))
      return (size_t)-1;

   for (c = 0; c < CHUNKS; c++)
   {
      fill((unsigned)c);
      memset(&d, 0, sizeof(d));
      d.data_in      = in_buf;
      d.data_out     = out + total * 2;
      d.input_frames = IN_FRAMES;
      d.ratio        = ratios[c % nratios];
      CC_resampler.process(re, &d);
      total += d.output_frames;
   }
   CC_resampler.free(re);
   return total;
}

static void compare(const char *name, resampler_simd_mask_t mask,
      double nominal, const double *ratios, size_t nratios)
{
   size_t n_ref, n_simd, i, differing = 0;
   float worst = 0.0f;

   n_ref  = stream(0,    nominal, ratios, nratios, out_ref);
   n_simd = stream(mask, nominal, ratios, nratios, out_simd);

   if (n_ref == (size_t)-1 || n_simd == (size_t)-1)
   {
      CHECK(0, "%s: init failed", name);
      return;
   }
   CHECK(n_ref == n_simd, "%s: %u frames against the reference's %u",
         name, (unsigned)n_simd, (unsigned)n_ref);
   if (n_ref != n_simd)
      return;

   for (i = 0; i < n_ref * 2; i++)
   {
      float diff = fabsf(out_simd[i] - out_ref[i]);
      if (diff > worst)
         worst = diff;
      if (out_simd[i] != out_ref[i])
         differing++;
   }
   /* Not bit-exact by construction: cc_int()'s polynomial has double
    * literals, so the reference rounds through double where the vector
    * arms stay in float. That is a last-place difference. A wrong lane
    * or a wrong weight is not - it moves the sample itself. */
   CHECK(worst <= 1.0e-6f,
         "%s: %u of %u samples differ, worst %g",
         name, (unsigned)differing, (unsigned)(n_ref * 2), worst);
   printf("      %-5s %u frames, worst difference %g\n",
         name, (unsigned)n_simd, worst);
}

int main(void)
{
   /* Upsampling, downsampling, unity, and rate control's excursions;
    * the nominal picks the direction init starts on. */
   static const double up[]   = { 1.5, 1.4963, 1.5037, 2.0, 1.0 };
   static const double down[] = { 0.6687, 0.6670, 0.9187, 0.5, 0.75 };
   /* Only the arms this build has: an uncompiled one would fall back
    * to the reference and compare against itself. The predicates are
    * the driver's own. */
   struct { const char *name; resampler_simd_mask_t mask; } arms[] = {
#if CC_VECTOR_ARMS
#if defined(__SSE__)
      { "SSE",  RESAMPLER_SIMD_SSE },
#endif
#if (defined(__ARM_NEON) || defined(__ARM_NEON__) || defined(HAVE_NEON))
      { "NEON", RESAMPLER_SIMD_NEON },
#endif
#endif
      { NULL, 0 }
   };
   size_t a;

   printf("CC kernels against the scalar reference:\n");
   for (a = 0; arms[a].name; a++)
   {
      printf("   %s\n", arms[a].name);
      compare(arms[a].name, arms[a].mask, 1.5,  up,   sizeof(up)/sizeof(up[0]));
      compare(arms[a].name, arms[a].mask, 0.5,  down, sizeof(down)/sizeof(down[0]));
   }
   if (a == 0)
      printf("   (no vector arm in this build; the reference is all there is)\n");
   if (failures)
   {
      printf("%u failure(s)\n", failures);
      return 1;
   }
   printf("CC kernels: every arm this build has matches the reference sample for sample\n");
   return 0;
}
