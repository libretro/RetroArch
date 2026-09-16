#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <audio/wsola_search.h>
#include <libretro_dspfilter.h>

static unsigned failures;
#define CHECK(x) do { if (!(x)) { printf("FAIL line %d: %s\n", __LINE__, #x); failures++; } } while (0)
#ifdef WSOLA_REFERENCE
extern const struct dspfilter_implementation *reference_implementation(unsigned);
#endif

static int get_pitch(void *u, const char *key, float *value, float def)
{
   (void)key;
   (void)def;
   *value = *(float*)u;
   return 1;
}

static void kernels(void)
{
   float a[261], b[261];
   int32_t ai[4096], bi[4096];
   unsigned i, n;
   wsola_corr_func_t corr = wsola_corr_get(WSOLA_SIMD_SSE2);
   for (i = 0; i < 261; i++)
   {
      a[i] = (float)sin(i * 0.171);
      b[i] = (float)cos(i * 0.237);
   }
   /* Unaligned starts, SIMD tails, negative correlation and silence. */
   for (n = 0; n <= 257; n++)
   {
      double energy = 0.0;
      for (i = 0; i < n; i++)
         energy += (double)a[i + 1] * a[i + 1];
      CHECK(fabs(corr(a + 1, b + 1, n, energy)
               - wsola_corr_scalar(a + 1, b + 1, n, energy)) < 1e-6);
   }
   for (i = 0; i < 4096; i++)
   {
      ai[i] = 65536;
      bi[i] = -65536;
   }
   CHECK(wsola_corr_i(ai, bi, 4096) == -(int64_t)274877906944);
   memset(bi, 0, sizeof(bi));
   CHECK(wsola_corr_i(ai, bi, 4096) == 0);
   memset(a, 0, sizeof(a));
   CHECK(corr(a, b, 257, 0.0) == 0.0);
   CHECK(wsola_isqrt64(0) == 0);
   CHECK(wsola_isqrt64(15) == 3);
   CHECK(wsola_isqrt64(16) == 4);
}

static void stream(float rate, float pitch, unsigned mask)
{
   struct dspfilter_info info;
   struct dspfilter_config cfg;
   const struct dspfilter_implementation *impl = dspfilter_get_implementation(mask);
   void *state;
   unsigned block, i;
   uint32_t noise = 1;
   float f[2048];
   int16_t s[2048];
#ifdef WSOLA_REFERENCE
   const struct dspfilter_implementation *ref = reference_implementation(mask);
   void *baseline;
#endif
   memset(&cfg, 0, sizeof(cfg));
   cfg.get_float = get_pitch;
   info.input_rate = rate;
   state = impl->init(&info, &cfg, &pitch);
   CHECK(state != NULL);
   if (!state) return;
#ifdef WSOLA_REFERENCE
   baseline = ref->init(&info, &cfg, &pitch);
   CHECK(baseline != NULL);
   if (!baseline) { impl->free(state); return; }
#endif
   for (block = 0; block < 120; block++)
   {
      struct dspfilter_input in;
      struct dspfilter_output out;
      struct dspfilter_input_i16 ini;
      struct dspfilter_output_i16 outi;
      unsigned frames = block % 3 == 0 ? 1 : (block % 3 == 1 ? 127 : 1024);
      for (i = 0; i < frames * 2; i++)
      {
         noise = noise * 1664525u + 1013904223u;
         s[i] = block < 8 ? 0 : (int16_t)(noise >> 16);
         if (block >= 8 && block < 40)
            s[i] = (int16_t)(12000 * sin((block * 2048 + i / 2) * 0.1));
         if ((i & 1) && block >= 40 && block < 60)
            s[i] = (int16_t)-s[i - 1];
         f[i] = s[i] / 32768.0f;
      }
      in.samples = f; in.frames = frames;
      ini.samples = s; ini.frames = frames;
      impl->process(state, &out, &in);
      impl->process_i16(state, &outi, &ini);
      if (pitch == 0)
      {
         CHECK(out.samples == f && out.frames == frames);
         CHECK(outi.samples == s && outi.frames == frames);
      }
#ifdef WSOLA_REFERENCE
      {
         struct dspfilter_output ro;
         struct dspfilter_output_i16 ri;
         ref->process(baseline, &ro, &in);
         ref->process_i16(baseline, &ri, &ini);
         CHECK(out.frames == ro.frames);
         CHECK(outi.frames == ri.frames);
         CHECK(memcmp(out.samples, ro.samples, out.frames * 2 * sizeof(float)) == 0);
         CHECK(memcmp(outi.samples, ri.samples, outi.frames * 2 * sizeof(int16_t)) == 0);
      }
#endif
   }
   impl->free(state);
#ifdef WSOLA_REFERENCE
   ref->free(baseline);
#endif
}

int main(void)
{
   unsigned i, j, m;
   const float rates[] = {32000, 48000, 96000};
   const float pitches[] = {-12, -3.25f, 0, 3.25f, 12};
   const unsigned masks[] = {0, DSPFILTER_SIMD_SSE2, DSPFILTER_SIMD_NEON};
   kernels();
   for (i = 0; i < 3; i++)
      for (j = 0; j < 5; j++)
         for (m = 0; m < 3; m++)
            stream(rates[i], pitches[j], masks[m]);
   printf("WSOLA: %u failures\n", failures);
   return failures ? 1 : 0;
}
