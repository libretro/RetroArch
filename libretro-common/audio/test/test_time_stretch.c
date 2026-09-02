/* Unit tests for the WSOLA time-stretcher.
 *
 * Build:  cc -O2 -std=c89 -Wall -Wextra test_time_stretch.c \
 *            ../audio_time_stretch.c -I ../../include -lm -o test_time_stretch */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <audio/audio_time_stretch.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static int failures = 0;

static void ok(const char *what, int cond)
{
   if (!cond)
   {
      printf("FAIL %s\n", what);
      failures++;
   }
   else
      printf("ok   %s\n", what);
}

/* Interleaved stereo sine at a fixed frequency, both channels identical. */
static void fill_sine(int16_t *buf, int frames, double hz, double rate,
      double *phase)
{
   int i;
   for (i = 0; i < frames; i++)
   {
      double v      = sin(*phase) * 12000.0;
      buf[(i*2)+0]  = (int16_t)v;
      buf[(i*2)+1]  = (int16_t)v;
      *phase       += (2.0 * M_PI * hz) / rate;
   }
}

/* Zero crossings per frame, a cheap proxy for pitch: WSOLA must preserve it
 * across ratios, which is the whole point of using it over a resampler. */
static double crossing_rate(const int16_t *buf, int frames)
{
   int i;
   int crossings = 0;
   for (i = 1; i < frames; i++)
      if ((buf[(i*2)] >= 0) != (buf[((i-1)*2)] >= 0))
         crossings++;
   return frames > 1 ? (double)crossings / (double)(frames - 1) : 0.0;
}

/* xorshift32 - small, seedable, deterministic. Used only to build the
 * pumping test's noise input below. */
static uint32_t noise_next(uint32_t *state)
{
   uint32_t x = *state;
   x ^= x << 13;
   x ^= x >> 17;
   x ^= x << 5;
   *state = x;
   return x;
}

/* Interleaved stereo noise, both channels identical, lightly lowpass
 * filtered so it carries short-range correlation instead of being pure
 * IID - which would give the similarity search nothing to find. Real
 * "noisy" game audio (explosions, percussion, engine noise) has the same
 * broadband-but-structured shape, which is what the search exploits. */
static void fill_noise(int16_t *buf, int frames, uint32_t *rng, float *lp)
{
   const float alpha = 0.85f;
   int         i;
   for (i = 0; i < frames; i++)
   {
      int32_t r = (int32_t)(noise_next(rng) & 0xFFFF) - 32768;
      float   x = (float)r / 32768.0f;
      int16_t v;
      *lp = (alpha * *lp) + ((1.0f - alpha) * x);
      v   = (int16_t)(*lp * 12000.0f * 4.0f);
      buf[(i * 2) + 0] = v;
      buf[(i * 2) + 1] = v;
   }
}

/* Goertzel magnitude of x[0..n-1] at digital frequency f (cycles/sample):
 * a single-bin DFT, cheaper than a full transform when only a few known
 * frequencies matter. */
static double goertzel_mag(const double *x, int n, double f)
{
   double w     = 2.0 * M_PI * f;
   double coeff = 2.0 * cos(w);
   double q0;
   double q1    = 0.0;
   double q2    = 0.0;
   double real;
   double imag;
   int    i;
   for (i = 0; i < n; i++)
   {
      q0 = (coeff * q1) - q2 + x[i];
      q2 = q1;
      q1 = q0;
   }
   real = q1 - (q2 * cos(w));
   imag = q2 * sin(w);
   return sqrt((real * real) + (imag * imag));
}

static void test_lifecycle(void)
{
   audio_time_stretch_t ts;
   ok("init succeeds", audio_time_stretch_init(&ts));
   ok("starts empty",  audio_time_stretch_input_fill(&ts) == 0);
   ok("no output yet", audio_time_stretch_output_fill(&ts) == 0);
   audio_time_stretch_free(&ts);
}

static void test_write_accounting(void)
{
   audio_time_stretch_t ts;
   /* One frame past the per-call cap, so the cap can actually be
    * exercised below. */
   static int16_t buf[(AUDIO_STRETCH_MAX_WRITE + 1) * 2];
   int accepted;

   memset(buf, 0, sizeof(buf));
   audio_time_stretch_init(&ts);

   accepted = audio_time_stretch_write(&ts, buf, 1024);
   ok("accepts a full write", accepted == 1024);
   ok("fill tracks writes",   audio_time_stretch_input_fill(&ts) == 1024);

   /* One call never takes more than the per-call cap, however much is
    * offered - that bound is what keeps the ring's own bounds provable. */
   accepted = audio_time_stretch_write(&ts, buf, AUDIO_STRETCH_MAX_WRITE + 1);
   ok("write is capped",  accepted == AUDIO_STRETCH_MAX_WRITE);
   ok("fill accumulates",
         audio_time_stretch_input_fill(&ts) == 1024 + AUDIO_STRETCH_MAX_WRITE);

   audio_time_stretch_reset(&ts);
   ok("reset empties input",  audio_time_stretch_input_fill(&ts) == 0);
   ok("reset empties output", audio_time_stretch_output_fill(&ts) == 0);

   audio_time_stretch_free(&ts);
}

static void test_write_refuses_when_full(void)
{
   audio_time_stretch_t ts;
   int16_t *buf   = (int16_t*)calloc(AUDIO_STRETCH_MAX_WRITE * 2,
         sizeof(int16_t));
   int total      = 0;
   int accepted   = 0;
   int guard      = 0;

   audio_time_stretch_init(&ts);

   /* Fill the ring without ever reading. It must stop accepting rather
    * than lapping itself; the guard catches an implementation that always
    * returns the full count. */
   do
   {
      accepted = audio_time_stretch_write(&ts, buf, AUDIO_STRETCH_MAX_WRITE);
      total   += accepted;
   } while (accepted > 0 && ++guard < 100);

   ok("write eventually refuses", accepted == 0);
   ok("never exceeds capacity",   total <= AUDIO_STRETCH_INPUT_CAPACITY);
   ok("fill never exceeds ring",
         audio_time_stretch_input_fill(&ts) <= AUDIO_STRETCH_INPUT_CAPACITY);

   audio_time_stretch_free(&ts);
   free(buf);
}

static void test_starved_read_is_short(void)
{
   audio_time_stretch_t ts;
   int16_t in[128 * 2];
   int16_t out[512 * 2];
   int n;

   memset(in, 0, sizeof(in));
   audio_time_stretch_init(&ts);

   /* Less than one analysis window in: there is nothing to synthesise from,
    * so the read must come up short rather than emit noise. */
   audio_time_stretch_write(&ts, in, 128);
   n = audio_time_stretch_read(&ts, out, 512, 1.0);
   ok("starved read is short", n < 512);

   audio_time_stretch_free(&ts);
}

/* Drive the stretcher the way the driver does: write a chunk, read a chunk,
 * repeat. Returns frames of input consumed for a fixed output count. */
static int64_t run_at_ratio(double ratio, int out_total, int16_t *collect)
{
   audio_time_stretch_t ts;
   int16_t in[512 * 2];
   int16_t out[512 * 2];
   double phase   = 0.0;
   int64_t written = 0;
   int got         = 0;

   audio_time_stretch_init(&ts);

   while (got < out_total)
   {
      int n;
      int want = out_total - got;
      if (want > 512)
         want = 512;

      /* Keep the ring comfortably fed so the search always has its full
       * radius available; starvation is covered by its own test. */
      while (audio_time_stretch_input_fill(&ts)
            < AUDIO_STRETCH_SEARCH_RADIUS + (4 * AUDIO_STRETCH_FRAME_SIZE))
      {
         int acc;
         fill_sine(in, 512, 440.0, 48000.0, &phase);
         acc = audio_time_stretch_write(&ts, in, 512);
         if (acc <= 0)
            break;
         written += acc;
      }

      n = audio_time_stretch_read(&ts, out, want, ratio);
      if (n <= 0)
         break;
      if (collect)
         memcpy(collect + (got * 2), out, (size_t)n * 2 * sizeof(int16_t));
      got += n;
   }

   audio_time_stretch_free(&ts);
   return written;
}

static void test_consumption_tracks_ratio(void)
{
   int64_t at_1x = run_at_ratio(1.0, 8192, NULL);
   int64_t at_3x = run_at_ratio(3.0, 8192, NULL);
   int64_t at_half = run_at_ratio(0.5, 8192, NULL);

   /* Compressing at 3x must eat markedly more input for the same output,
    * expanding at 0.5x markedly less. Bounds are loose because the ring is
    * topped up in 512-frame lumps, which quantises the totals. */
   ok("3x consumes more than 1x",   at_3x > (at_1x * 2));
   ok("0.5x consumes less than 1x", at_half < at_1x);
   printf("     consumed: 0.5x=%ld  1x=%ld  3x=%ld\n",
         (long)at_half, (long)at_1x, (long)at_3x);
}

static void test_pitch_is_preserved(void)
{
   const int frames = 8192;
   int16_t *ref     = (int16_t*)calloc((size_t)frames * 2, sizeof(int16_t));
   int16_t *fast    = (int16_t*)calloc((size_t)frames * 2, sizeof(int16_t));
   double r_ref, r_fast;

   run_at_ratio(1.0, frames, ref);
   run_at_ratio(3.0, frames, fast);

   /* The whole reason for WSOLA over a resampler: at 3x the output must
    * still cross zero at the input's rate. A resampler would triple it. */
   r_ref  = crossing_rate(ref  + (1024 * 2), frames - 1024);
   r_fast = crossing_rate(fast + (1024 * 2), frames - 1024);

   printf("     crossing rate: 1x=%.6f  3x=%.6f\n", r_ref, r_fast);
   ok("pitch held at 3x", fabs(r_fast - r_ref) < (r_ref * 0.25));

   free(ref);
   free(fast);
}

/* Measures the amplitude pumping the search suppresses, on noise, where a
 * bad splice cannot hide behind periodicity as it can on the sine above. */
static void test_pumping_needs_the_search(void)
{
   const int      out_total    = 200000;
   const int      warmup       = 8192;
   const int      n_harmonics  = 4;
   audio_time_stretch_t ts;
   int16_t  in[512 * 2];
   int16_t *out;
   uint32_t rng = 0xC0FFEEu;
   float    lp  = 0.0f;
   int      got = 0;
   int      n_power;
   int      i;
   double  *power;
   double   mean_power  = 0.0;
   double   pump_energy = 0.0;
   double   pumping;

   out = (int16_t*)calloc((size_t)out_total * 2, sizeof(int16_t));
   audio_time_stretch_init(&ts);

   while (got < out_total)
   {
      int n;
      int want = out_total - got;
      if (want > 512)
         want = 512;

      /* Keep the ring comfortably fed, same as run_at_ratio() above. */
      while (audio_time_stretch_input_fill(&ts)
            < AUDIO_STRETCH_SEARCH_RADIUS + (4 * AUDIO_STRETCH_FRAME_SIZE))
      {
         int acc;
         fill_noise(in, 512, &rng, &lp);
         acc = audio_time_stretch_write(&ts, in, 512);
         if (acc <= 0)
            break;
      }

      n = audio_time_stretch_read(&ts, out + (got * 2), want, 3.0);
      if (n <= 0)
         break;
      got += n;
   }

   audio_time_stretch_free(&ts);

   if (got <= warmup + AUDIO_STRETCH_SYNTHESIS_HOP)
   {
      ok("pumping test collected enough output", 0);
      free(out);
      return;
   }

   /* Each hop emits exactly AUDIO_STRETCH_SYNTHESIS_HOP frames, so a splice
    * artefact synchronised with the overlap-add boundaries is periodic at
    * exactly that period. A plain envelope stddev/mean would be swamped
    * by the noise source's own broadband wobble; a Goertzel magnitude at
    * the hop frequency (and its first few harmonics) isolates just the
    * part that recurs every hop. */
   n_power = got - warmup;
   power   = (double*)malloc((size_t)n_power * sizeof(double));

   for (i = 0; i < n_power; i++)
   {
      double v = (double)out[((warmup + i) * 2) + 0];
      power[i] = v * v;
      mean_power += power[i];
   }
   mean_power /= n_power;

   for (i = 1; i <= n_harmonics; i++)
   {
      double f   = (double)i / (double)AUDIO_STRETCH_SYNTHESIS_HOP;
      double mag = goertzel_mag(power, n_power, f);
      double amp = (2.0 * mag) / (double)n_power;
      pump_energy += amp * amp;
   }

   pumping = mean_power > 1.0e-9 ? sqrt(pump_energy) / mean_power : 0.0;

   printf("     pumping at ratio 3, noise, search on: %.6f\n", pumping);
   /* Well clear of both search-off and worst-match. */
   ok("pumping stays low with the search on", pumping < 0.10);

   free(power);
   free(out);
}

/* Without audio_time_stretch_idle() the ring is neither read nor written
 * while the driver bypasses the stretcher at normal speed, and the next
 * engage overlap-adds across the gap. Measured on the ring rather than the
 * output: a stationary tone is exactly what the search can splice
 * invisibly. The samples carry a counter, so any break is exact. */
#define IDLE_RAMP_PERIOD 20000

static void fill_ramp(int16_t *buf, int frames, long *k)
{
   int i;
   for (i = 0; i < frames; i++)
   {
      int16_t v     = (int16_t)((*k % IDLE_RAMP_PERIOD) - (IDLE_RAMP_PERIOD / 2));
      buf[(i*2)+0]  = v;
      buf[(i*2)+1]  = v;
      (*k)++;
   }
}

/* Breaks in the frames the next synthesis will read, from the read point up
 * to the write head. */
static int idle_ring_breaks(const audio_time_stretch_t *ts)
{
   int64_t pos;
   int     breaks = 0;
   for (pos = ts->analysis_pos; pos + 1 < ts->write_pos; pos++)
   {
      int a = ts->in_l[(int)( pos      & (AUDIO_STRETCH_INPUT_CAPACITY - 1))];
      int b = ts->in_l[(int)((pos + 1) & (AUDIO_STRETCH_INPUT_CAPACITY - 1))];
      if ((b - a) != 1 && (b - a) != -(IDLE_RAMP_PERIOD - 1))
         breaks++;
   }
   return breaks;
}

static int idle_gap_breaks(int feed_while_idle, int *fill_out)
{
   audio_time_stretch_t ts;
   int16_t in[512 * 2];
   int16_t out[512 * 2];
   long    k       = 0;
   int     block   = 512;
   int     reserve = AUDIO_STRETCH_FRAME_SIZE * 8;
   int     breaks;
   int     i;

   audio_time_stretch_init(&ts);

   /* Fast-forward: written and synthesised. */
   for (i = 0; i < 40; i++)
   {
      fill_ramp(in, block, &k);
      audio_time_stretch_write(&ts, in, block);
      audio_time_stretch_read(&ts, out, 170, 3.0);
   }

   /* Normal speed: the core's audio keeps coming either way. */
   for (i = 0; i < 40; i++)
   {
      fill_ramp(in, block, &k);
      if (feed_while_idle)
      {
         audio_time_stretch_write(&ts, in, block);
         audio_time_stretch_idle(&ts, reserve);
      }
   }

   /* Re-engage: one block, which is where the join lands. */
   fill_ramp(in, block, &k);
   audio_time_stretch_write(&ts, in, block);

   breaks    = idle_ring_breaks(&ts);
   *fill_out = audio_time_stretch_input_fill(&ts);
   audio_time_stretch_free(&ts);
   return breaks;
}

static void test_idle_keeps_the_ring_continuous(void)
{
   int unfed_fill = 0;
   int fed_fill   = 0;
   int unfed      = idle_gap_breaks(0, &unfed_fill);
   int fed        = idle_gap_breaks(1, &fed_fill);

   printf("     ring at re-engage: unfed breaks=%d fill=%d | fed breaks=%d fill=%d\n",
         unfed, unfed_fill, fed, fed_fill);
   ok("an unfed ring carries the gap into the re-engage", unfed > 0);
   ok("idle feed leaves the ring continuous", fed == 0);
   /* And with a reserve already built, so the re-engage is a ratio change
    * rather than a cold start. */
   ok("idle feed keeps a reserve", fed_fill >= AUDIO_STRETCH_FRAME_SIZE * 8);
}

int main(void)
{
   test_lifecycle();
   test_write_accounting();
   test_write_refuses_when_full();
   test_starved_read_is_short();
   test_consumption_tracks_ratio();
   test_pitch_is_preserved();
   test_pumping_needs_the_search();
   test_idle_keeps_the_ring_continuous();

   printf("\n%s (%d failures)\n", failures ? "FAILED" : "PASSED", failures);
   return failures ? 1 : 0;
}
