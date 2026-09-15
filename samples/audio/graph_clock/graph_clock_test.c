/* The two conversions the JACK and PipeWire device-clock fits add.
 *
 * The fit itself is shared with the ASIO, CoreAudio and ALSA ones and
 * is checked there; the ALSA harness drives the shipping driver on a
 * real PCM. Neither a JACK server nor a PipeWire daemon can be started
 * here, so this mirrors the two pieces of arithmetic that are new
 * rather than the drivers - which is stated plainly because it is a
 * weaker check than the ALSA one, and these are the parts where being
 * wrong would be silent.
 *
 * JACK: current_frames is jack_nframes_t, which is 32 bits and wraps
 * in about a day at 48 kHz - sooner than that if the server has been
 * up a while before RetroArch starts, since the counter is the
 * server's and not ours. The difference between cycles is taken in 32
 * bits, where the wrap cancels, and accumulated into 64. Getting that
 * wrong gives a position that jumps by four billion frames once and
 * then reads as a wildly wrong clock for the rest of the session.
 *
 * PipeWire: ticks are in units of pw_time.rate, a fraction that is
 * 1/samplerate for an audio stream. Multiplying by num/denom turns
 * them into seconds, which is what makes both axes seconds and the
 * slope a plain ratio. A rate of 1/44100 and a rate of 1/48000 have to
 * come out at the same drift for the same clock. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static int failures;

static void fail(const char *what, const char *detail)
{
   printf("   FAIL %s: %s\n", what, detail);
   failures++;
}

/* ── JACK: the wrapping frame counter ─────────────────────────────── */

typedef struct
{
   uint32_t last_frames;
   uint64_t frames_total;
   uint64_t anchor_usec;
   int      have_anchor;
   double   sx, sy, sxx, sxy, n;
   unsigned rate;
   int      ppm;
   int      valid;
} jack_clk_t;

static void jack_cycle(jack_clk_t *c, uint32_t cur_f, uint64_t cur_u)
{
   if (!c->have_anchor)
   {
      c->last_frames  = cur_f;
      c->frames_total = 0;
      c->anchor_usec  = cur_u;
      c->have_anchor  = 1;
      c->sx = c->sy = c->sxx = c->sxy = c->n = 0.0;
      return;
   }
   if (cur_u > c->anchor_usec)
   {
      /* The whole point: this subtraction is in 32 bits, so the wrap
       * cancels and the step is the true one. */
      uint32_t step = cur_f - c->last_frames;
      double   x, y, d;

      c->last_frames   = cur_f;
      c->frames_total += (uint64_t)step;

      x = (double)(cur_u - c->anchor_usec) / 1000000.0;
      y = (double)c->frames_total;

      c->sx  += x;
      c->sy  += y;
      c->sxx += x * x;
      c->sxy += x * y;
      c->n   += 1.0;

      d = c->n * c->sxx - c->sx * c->sx;
      if (x >= 1.0 && d > 0.0)
      {
         double slope = (c->n * c->sxy - c->sx * c->sy) / d;
         double ppm   = (slope / (double)c->rate - 1.0) * 1000000.0;
         if (ppm > -100000.0 && ppm < 100000.0)
         {
            c->ppm   = (int)ppm;
            c->valid = 1;
         }
      }
   }
   else
   {
      c->last_frames  = cur_f;
      c->frames_total = 0;
      c->anchor_usec  = cur_u;
      c->sx = c->sy = c->sxx = c->sxy = c->n = 0.0;
   }
}

/* Runs a server at true_rate for a number of seconds, starting its
 * frame counter wherever asked - including just short of the wrap. */
static int jack_run(double true_rate, unsigned nominal, double seconds,
      uint32_t start_frames, int *valid, uint64_t *wraps_seen)
{
   jack_clk_t c;
   uint32_t   period = 1024;
   uint32_t   f      = start_frames;
   uint64_t   cycles = (uint64_t)(seconds * true_rate / period), i;
   uint32_t   prev;

   memset(&c, 0, sizeof(c));
   c.rate      = nominal;
   *wraps_seen = 0;

   for (i = 0; i < cycles; i++)
   {
      /* The server's microsecond clock, which is the wall clock. */
      uint64_t usec = (uint64_t)((double)i * period * 1000000.0 / true_rate);
      prev = f;
      jack_cycle(&c, f, usec);
      f += period;
      if (f < prev)
         (*wraps_seen)++;
   }

   *valid = c.valid;
   return c.ppm;
}

/* ── PipeWire: ticks in units of a fraction ───────────────────────── */

typedef struct
{
   int64_t  anchor_now;
   uint64_t anchor_ticks;
   int      have_anchor;
   double   sx, sy, sxx, sxy, n;
   int      ppm;
   int      valid;
} pw_clk_t;

static void pw_report(pw_clk_t *c, int64_t now, uint64_t ticks,
      uint32_t num, uint32_t denom)
{
   if (!denom || now <= 0 || !ticks)
      return;

   if (!c->have_anchor)
   {
      c->anchor_now   = now;
      c->anchor_ticks = ticks;
      c->have_anchor  = 1;
      c->sx = c->sy = c->sxx = c->sxy = c->n = 0.0;
      return;
   }
   if (now > c->anchor_now && ticks >= c->anchor_ticks)
   {
      double x = (double)(now - c->anchor_now) / 1000000000.0;
      double y = (double)(ticks - c->anchor_ticks)
         * (double)num / (double)denom;
      double d;

      c->sx  += x;
      c->sy  += y;
      c->sxx += x * x;
      c->sxy += x * y;
      c->n   += 1.0;

      d = c->n * c->sxx - c->sx * c->sx;
      if (x >= 1.0 && d > 0.0)
      {
         double slope = (c->n * c->sxy - c->sx * c->sy) / d;
         double ppm   = (slope - 1.0) * 1000000.0;
         if (ppm > -100000.0 && ppm < 100000.0)
         {
            c->ppm   = (int)ppm;
            c->valid = 1;
         }
      }
   }
   else
   {
      c->anchor_now   = now;
      c->anchor_ticks = ticks;
      c->sx = c->sy = c->sxx = c->sxy = c->n = 0.0;
   }
}

static int pw_run(double true_rate, uint32_t denom, double seconds,
      int *valid)
{
   pw_clk_t c;
   uint64_t quantum = 1024;
   uint64_t ticks   = 0;
   uint64_t i, cycles = (uint64_t)(seconds * true_rate / quantum);

   memset(&c, 0, sizeof(c));
   for (i = 0; i < cycles; i++)
   {
      /* now is the wall clock; ticks advance with the graph, which is
       * running at true_rate against a nominal of denom. */
      int64_t now = (int64_t)((double)ticks * 1000000000.0 / true_rate);
      pw_report(&c, now, ticks ? ticks : 1, 1, denom);
      ticks += quantum;
   }
   *valid = c.valid;
   return c.ppm;
}

int main(void)
{
   printf("1. JACK: a frame counter that wraps mid-session\n");
   {
      struct { const char *name; uint32_t start; } s[] = {
         { "from zero",          0u },
         /* Two seconds short of the wrap at 48 kHz, so it goes round
          * during the run and more than once is not possible. */
         { "just short of wrap", 0xFFFFFFFFu - 96000u },
         { "well past 2^31",     0xC0000000u }
      };
      unsigned i;
      for (i = 0; i < sizeof(s) / sizeof(*s); i++)
      {
         int      valid = 0;
         uint64_t wraps = 0;
         int      got   = jack_run(48002.4, 48000, 10.0, s[i].start,
               &valid, &wraps);
         if (!valid)
            fail(s[i].name, "no estimate");
         else if (got < 48 || got > 52)
         {
            char d[128];
            snprintf(d, sizeof(d), "read %+d ppm, expected +50"
                  " (%u wrap%s during the run)", got, (unsigned)wraps,
                  wraps == 1 ? "" : "s");
            fail(s[i].name, d);
         }
         else
            printf("   ok   %-19s %+d ppm, %u wrap%s during the run\n",
                  s[i].name, got, (unsigned)wraps, wraps == 1 ? "" : "s");
      }
   }

   printf("2. JACK: what a 64-bit subtraction would have done\n");
   {
      /* The same step, taken the wrong way: widen first, subtract
       * after, and the wrap becomes a four-billion-frame jump. */
      uint32_t before = 0xFFFFFF00u;
      uint32_t after  = before + 1024u;   /* wraps */
      uint32_t right  = after - before;
      uint64_t wrong  = (uint64_t)after - (uint64_t)before;
      printf("   in 32 bits the step is %u; widened first it is %llu\n",
            right, (unsigned long long)wrong);
      if (right != 1024u)
         fail("wrap", "the 32-bit step is not the true one");
      else if (wrong == 1024ull)
         fail("wrap", "the demonstration is not demonstrating anything");
      else
         printf("   ok   which is why the difference is taken before"
                " it is widened\n");
   }

   printf("3. PipeWire: ticks through the rate fraction\n");
   {
      struct { const char *name; double rate; uint32_t denom; int want; } d[] = {
         { "48000 exact",   48000.0,   48000,   0 },
         { "48000 +50ppm",  48002.4,   48000,  50 },
         { "44100 +50ppm",  44102.205, 44100,  50 },
         { "44100 -80ppm",  44096.472, 44100, -80 },
         { "96000 +10ppm",  96000.96,  96000,  10 }
      };
      unsigned i;
      for (i = 0; i < sizeof(d) / sizeof(*d); i++)
      {
         int valid = 0;
         int got   = pw_run(d[i].rate, d[i].denom, 10.0, &valid);
         if (!valid)
            fail(d[i].name, "no estimate");
         else if (got < d[i].want - 2 || got > d[i].want + 2)
         {
            char det[96];
            snprintf(det, sizeof(det), "read %+d ppm, expected %+d",
                  got, d[i].want);
            fail(d[i].name, det);
         }
         else
            printf("   ok   %-14s %+d ppm\n", d[i].name, got);
      }
      /* The same physical drift has to read the same at either rate,
       * which is the property the fraction is there to give. */
      {
         int v1 = 0, v2 = 0;
         int a = pw_run(48002.4,   48000, 10.0, &v1);
         int b = pw_run(44102.205, 44100, 10.0, &v2);
         if (!v1 || !v2 || a != b)
         {
            char det[96];
            snprintf(det, sizeof(det), "48k read %+d, 44.1k read %+d", a, b);
            fail("rate independence", det);
         }
         else
            printf("   ok   the same drift reads %+d ppm at both rates\n", a);
      }
   }

   if (failures)
   {
      printf("graph clock: %d failure(s)\n", failures);
      return 1;
   }
   printf("graph clock: the wrap cancels and the fraction converts\n");
   return 0;
}
