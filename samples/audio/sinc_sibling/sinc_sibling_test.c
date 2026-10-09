/* A sibling streams as a fresh instance would, whatever its parent does,
 * over the parent's tables: it allocates only its own state, and the
 * tables outlive whichever of the two goes first. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <audio/audio_resampler.h>
#include <audio/sinc_resampler.h>
#include <audio/sinc_resampler_int16.h>

static unsigned failures;
#define CHECK(x) do { if (!(x)) { printf("FAIL line %d: %s\n", __LINE__, \
   #x); failures++; } } while (0)

/* Bytes asked of the allocator, under the GNU ld wraps. */
#ifdef SINC_TRACK_ALLOCATIONS
static size_t requested;
void *__real_malloc(size_t);
void *__real_calloc(size_t, size_t);
void *__wrap_malloc(size_t n) { requested += n; return __real_malloc(n); }
void *__wrap_calloc(size_t n, size_t s) { requested += n * s; return __real_calloc(n, s); }
#endif

#define FRAMES 24000
#define CAP    (FRAMES * 9 + 1024)
static float in_f[FRAMES * 2], a_f[CAP * 2], b_f[CAP * 2], junk_f[CAP * 2];
static int16_t in_i[FRAMES * 2], a_i[CAP * 2], b_i[CAP * 2], junk_i[CAP * 2];

/* Nominal, rate control, then fast-forward through stage changes. */
static const double scales[] = { 1.0, 0.9995, 1.0005, 0.5, 0.25, 0.3, 1.0 };

static size_t run_f(void *re, const float *in, float *out, double nominal)
{
   size_t i, n = 0;
   for (i = 0; i + 1000 <= FRAMES; i += 1000)
   {
      struct resampler_data d;
      d.data_in = in + 2 * i; d.input_frames = 1000;
      d.data_out = out + 2 * n; d.output_frames = 0;
      d.ratio = nominal * scales[(i / 1000) % 7];
      sinc_resampler.process(re, &d);
      n += d.output_frames;
   }
   return n;
}

static size_t run_i(void *re, const int16_t *in, int16_t *out, double nominal)
{
   size_t i, n = 0;
   for (i = 0; i + 1000 <= FRAMES; i += 1000)
   {
      struct resampler_data_int16 d;
      d.data_in = in + 2 * i; d.input_frames = 1000;
      d.data_out = out + 2 * n; d.output_frames = 0;
      d.ratio = nominal * scales[(i / 1000) % 7];
      sinc_resampler_int16_process(re, &d);
      n += d.output_frames;
   }
   return n;
}

int main(void)
{
   static const double nominals[] = { 0.5, 1.0, 1.5, 4.0 };
   unsigned q, n, order;
   size_t i;
   unsigned long x = 12345;
   for (i = 0; i < FRAMES * 2; i++)
   {
      x = (x * 1664525ul + 1013904223ul) & 0xfffffffful;
      in_i[i] = (int16_t)((long)(x >> 18) - 8192);
      in_f[i] = in_i[i] / 32768.0f;
   }
   for (q = RESAMPLER_QUALITY_LOWEST; q <= RESAMPLER_QUALITY_HIGHEST; q++)
      for (n = 0; n < 4; n++)
         for (order = 0; order < 2; order++)
         {
            int hq = nominals[n] >= 2.0;
            enum sinc_int16_quality iq = (enum sinc_int16_quality)(q - 1);
            void *parent   = sinc_resampler_init_hq(nominals[n],
                  (enum resampler_quality)q, RESAMPLER_SIMD_SSE, hq);
            void *parent_i = sinc_resampler_int16_init_hq(nominals[n], iq, hq);
            void *fresh    = sinc_resampler_init_hq(nominals[n],
                  (enum resampler_quality)q, RESAMPLER_SIMD_SSE, hq);
            void *fresh_i  = sinc_resampler_int16_init_hq(nominals[n], iq, hq);
            size_t before, own, own_i, table, na, nb;
            void *sib, *sib_i;
            if (!parent || !parent_i || !fresh || !fresh_i)
               exit(2);
#ifdef SINC_TRACK_ALLOCATIONS
            before = requested;
            sinc_resampler.free(sinc_resampler_init_hq(nominals[n],
                  (enum resampler_quality)q, RESAMPLER_SIMD_SSE, hq));
            table  = requested - before;
#else
            before = table = 0;
#endif
            /* The parent mid-stream: none of it reaches the sibling. */
            run_f(parent, in_f, junk_f, nominals[n]);
            run_i(parent_i, in_i, junk_i, nominals[n]);
#ifdef SINC_TRACK_ALLOCATIONS
            before = requested;
#endif
            sib    = sinc_resampler.sibling(parent);
#ifdef SINC_TRACK_ALLOCATIONS
            own    = requested - before;
            before = requested;
#endif
            sib_i  = sinc_resampler_int16_sibling(parent_i);
#ifdef SINC_TRACK_ALLOCATIONS
            own_i  = requested - before;
#else
            own = own_i = 0;
#endif
            CHECK(sib && sib_i);
            if (!sib || !sib_i)
               exit(2);
            /* Either may go first; the tables stay for the other. */
            if (order)
            {
               sinc_resampler.free(parent);
               sinc_resampler_int16_free(parent_i);
            }
            else
            {
               run_f(parent, in_f, junk_f, nominals[n]);
               run_i(parent_i, in_i, junk_i, nominals[n]);
            }
            na = run_f(sib, in_f, a_f, nominals[n]);
            nb = run_f(fresh, in_f, b_f, nominals[n]);
            CHECK(na == nb && memcmp(a_f, b_f, na * 2 * sizeof(float)) == 0);
            na = run_i(sib_i, in_i, a_i, nominals[n]);
            nb = run_i(fresh_i, in_i, b_i, nominals[n]);
            CHECK(na == nb && memcmp(a_i, b_i, na * 2 * sizeof(int16_t)) == 0);
            /* Its own state only: under a third of a whole instance for
             * every preset past the smallest. */
            if (q >= RESAMPLER_QUALITY_NORMAL)
               CHECK(own * 3 < table);
            (void)own_i;
            sinc_resampler.reset(sib);
            na = run_f(sib, in_f, a_f, nominals[n]);
            CHECK(memcmp(a_f, b_f, na * 2 * sizeof(float)) == 0);
            if (!order)
            {
               sinc_resampler.free(parent);
               sinc_resampler_int16_free(parent_i);
            }
            sinc_resampler.free(sib);
            sinc_resampler_int16_free(sib_i);
            sinc_resampler.free(fresh);
            sinc_resampler_int16_free(fresh_i);
         }
   CHECK(!sinc_resampler.sibling(NULL) && !sinc_resampler_int16_sibling(NULL));
   printf("Sinc siblings: %u failures\n", failures);
   return failures ? 1 : 0;
}
