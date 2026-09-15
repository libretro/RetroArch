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
         test_backend(backends[b], ratios[k]);
   }
   if (failures)
   {
      printf("%u failure(s)\n", failures);
      return 1;
   }
   printf("resampler reset: every backend resumes as a fresh one would, to the sample, with no allocation\n");
   return 0;
}
