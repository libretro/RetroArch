/* reset() on every resampler backend that has one: after a reset the
 * stream continues as a freshly initialised backend would - the same
 * output for the same input, to the sample - and the reset touches no
 * allocator. Run at the ratios the frontend meets: unity, 32 to 48,
 * 44.1 to 48, 48 to 44.1, and rate control's small excursions. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <audio/audio_resampler.h>
#include <memalign.h>

static unsigned failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { printf("      FAIL: "); printf(__VA_ARGS__); printf("\n"); failures++; } } while (0)

extern retro_resampler_t sinc_resampler;
extern retro_resampler_t nearest_resampler;
extern retro_resampler_t CC_resampler;

/* Allocation is counted through the wrappers the Makefile installs. */
unsigned alloc_calls;
void *__real_malloc(size_t);
void *__real_calloc(size_t, size_t);
void *__wrap_malloc(size_t n)            { alloc_calls++; return __real_malloc(n); }
void *__wrap_calloc(size_t n, size_t m)  { alloc_calls++; return __real_calloc(n, m); }
void *__real_memalign_alloc(size_t, size_t);
void *__wrap_memalign_alloc(size_t a, size_t n) { alloc_calls++; return __real_memalign_alloc(a, n); }

#define IN_FRAMES  1000
#define OUT_CAP    (IN_FRAMES * 4 + 64)

static void fill(float *buf, unsigned frames, unsigned seed)
{
   unsigned i;
   uint32_t x = 0x9E3779B9u * (seed + 1);
   for (i = 0; i < frames * 2; i++)
   {
      x ^= x << 13; x ^= x >> 17; x ^= x << 5;
      buf[i] = ((float)(x & 0xFFFF) / 32768.0f - 1.0f) * 0.5f
             + 0.3f * sinf((float)i * 0.011f);
   }
}

static size_t run(const retro_resampler_t *r, void *state, double ratio,
      const float *in, float *out)
{
   struct resampler_data d;
   memset(&d, 0, sizeof(d));
   d.data_in       = in;
   d.data_out      = out;
   d.input_frames  = IN_FRAMES;
   d.ratio         = ratio;
   r->process(state, &d);
   return d.output_frames;
}

static void test_backend(const retro_resampler_t *r, double ratio)
{
   static float in_a[IN_FRAMES * 2], in_b[IN_FRAMES * 2];
   static float out_fresh[OUT_CAP * 2], out_reset[OUT_CAP * 2];
   struct resampler_config cfg;
   void *fresh, *reused;
   size_t n_fresh, n_reset, i;
   unsigned allocs_before;

   memset(&cfg, 0, sizeof(cfg));
   fill(in_a, IN_FRAMES, 1);
   fill(in_b, IN_FRAMES, 2);

   /* A backend that has been through some stream, then reset. */
   reused = r->init(&cfg, ratio, RESAMPLER_QUALITY_NORMAL, 0);
   fresh  = r->init(&cfg, ratio, RESAMPLER_QUALITY_NORMAL, 0);
   if (!reused || !fresh)
   {
      CHECK(0, "%s: init failed at ratio %.4f", r->ident, ratio);
      return;
   }
   run(r, reused, ratio, in_b, out_reset);
   run(r, reused, ratio, in_a, out_reset);

   allocs_before = alloc_calls;
   r->reset(reused);
   CHECK(alloc_calls == allocs_before, "%s: reset allocated", r->ident);

   n_reset = run(r, reused, ratio, in_a, out_reset);
   n_fresh = run(r, fresh,  ratio, in_a, out_fresh);
   CHECK(n_reset == n_fresh, "%s at %.4f: reset produced %u frames, fresh %u",
         r->ident, ratio, (unsigned)n_reset, (unsigned)n_fresh);
   for (i = 0; i < n_fresh * 2 && i < OUT_CAP * 2; i++)
      if (out_reset[i] != out_fresh[i])
      {
         CHECK(0, "%s at %.4f: sample %u differs after reset (%g vs %g)",
               r->ident, ratio, (unsigned)i, out_reset[i], out_fresh[i]);
         break;
      }
   r->free(reused);
   r->free(fresh);
}

/* What a caller sizing data_out gets to assume, since the API carries
 * no capacity: never more than the ratio asks for, plus the frame a
 * carried phase adds. */
static void test_output_bound(const retro_resampler_t *r, double ratio)
{
   static float in_a[IN_FRAMES * 2];
   static float out[OUT_CAP * 2];
   struct resampler_config cfg;
   void *re;
   size_t n, k;

   memset(&cfg, 0, sizeof(cfg));
   fill(in_a, IN_FRAMES, 3);
   if (!(re = r->init(&cfg, ratio, RESAMPLER_QUALITY_NORMAL, 0)))
   {
      CHECK(0, "%s: init failed at ratio %.4f", r->ident, ratio);
      return;
   }
   /* Across several calls, so a carried phase is in play. */
   for (k = 0; k < 4; k++)
   {
      n = run(r, re, ratio, in_a, out);
      CHECK(n <= (size_t)(IN_FRAMES * ratio) + 2,
            "%s at %.4f: call %u reported %u frames, ratio asks for %u",
            r->ident, ratio, (unsigned)k, (unsigned)n,
            (unsigned)((size_t)(IN_FRAMES * ratio) + 2));
   }
   r->free(re);
}

/* audio_driver_bound_ratio() returns the ratio untouched where the
 * caller reports no capacity, so a backend has to stop on its own
 * rather than walk out of data_out. data_out is exact and on the heap,
 * so a sanitizer build sees the write a padded static buffer would
 * swallow. */
static void test_unnameable_ratio(const retro_resampler_t *r)
{
   static const double bad[] = { 1.0e9, 1.0 / 0.0, 0.0 / 0.0, 0.0, -1.5 };
   struct resampler_config cfg;
   struct resampler_data d;
   float *in, *out;
   void *re;
   size_t k;

   memset(&cfg, 0, sizeof(cfg));
   for (k = 0; k < sizeof(bad) / sizeof(bad[0]); k++)
   {
      if (!(re = r->init(&cfg, 1.0, RESAMPLER_QUALITY_NORMAL, 0)))
      {
         CHECK(0, "%s: init failed", r->ident);
         return;
      }
      in  = (float*)calloc(IN_FRAMES * 2, sizeof(float));
      out = (float*)calloc(OUT_CAP * 2, sizeof(float));
      memset(&d, 0, sizeof(d));
      d.data_in      = in;
      d.data_out     = out;
      d.input_frames = IN_FRAMES;
      d.ratio        = bad[k];
      r->process(re, &d);
      CHECK(d.output_frames <= OUT_CAP,
            "%s at ratio %g: reported %u frames into room for %u",
            r->ident, bad[k], (unsigned)d.output_frames, (unsigned)OUT_CAP);
      r->free(re);
      free(in);
      free(out);
   }
}

/* audio_driver_effective_ratio() multiplies the ratio by the slowmotion
 * setting, so a backend that chose its direction from the nominal ratio
 * at init is handed one above 1.0 the moment slow motion starts. It has
 * to keep producing the frames the ratio asks for. */
static void test_ratio_above_init(const retro_resampler_t *r)
{
   static float in_a[IN_FRAMES * 2];
   static float out[OUT_CAP * 2];
   struct resampler_config cfg;
   void *re;
   size_t k, total = 0;
   double nominal = 0.5, fast = 2.0, expect;

   memset(&cfg, 0, sizeof(cfg));
   fill(in_a, IN_FRAMES, 4);
   if (!(re = r->init(&cfg, nominal, RESAMPLER_QUALITY_NORMAL, 0)))
   {
      CHECK(0, "%s: init failed at %.2f", r->ident, nominal);
      return;
   }
   for (k = 0; k < 8; k++)
      total += run(r, re, fast, in_a, out);
   r->free(re);

   expect = (double)(IN_FRAMES * 8) * fast;
   CHECK((double)total > expect * 0.99,
         "%s: inited at %.2f, run at %.2f: %u frames where %.0f are due",
         r->ident, nominal, fast, (unsigned)total, expect);
}

int main(void)
{
   const retro_resampler_t *backends[] = { &sinc_resampler, &nearest_resampler, &CC_resampler };
   const double ratios[] = { 1.0, 48000.0 / 32000.0, 48000.0 / 44100.0, 44100.0 / 48000.0, 1.0025, 0.9975 };
   size_t b, k;
   printf("resampler reset:\n");
   for (b = 0; b < sizeof(backends) / sizeof(backends[0]); b++)
   {
      printf("   %s\n", backends[b]->ident);
      CHECK(backends[b]->reset != NULL, "%s has no reset()", backends[b]->ident);
      if (!backends[b]->reset)
         continue;
      for (k = 0; k < sizeof(ratios) / sizeof(ratios[0]); k++)
      {
         test_backend(backends[b], ratios[k]);
         test_output_bound(backends[b], ratios[k]);
      }
   }
   for (b = 0; b < sizeof(backends) / sizeof(backends[0]); b++)
   {
      printf("   %s, ratios no rate pair can name\n", backends[b]->ident);
      test_unnameable_ratio(backends[b]);
   }
   printf("   CC, a ratio above the one init chose from\n");
   test_ratio_above_init(&CC_resampler);
   if (failures)
   {
      printf("%u failure(s)\n", failures);
      return 1;
   }
   printf("resampler reset: every backend resumes as a fresh one would, to the sample, with no allocation\n");
   printf("resampler output: no backend reports past what the ratio asks for, and CC stops on a ratio it cannot use\n");
   return 0;
}
