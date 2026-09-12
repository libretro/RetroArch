/* Checks the CoreAudio device-clock fit off a Mac.
 *
 * The fit itself is the same shape as the ASIO one and is checked
 * there too. What is different here, and what this is mostly for, is
 * the time axis.
 *
 * CoreAudio's mHostTime is in mach absolute units, which are not
 * nanoseconds. mach_timebase_info gives the ratio: one to one on
 * Intel, where the distinction is invisible, and 125 to 3 on Apple
 * silicon, where ignoring it reads a 48 kHz device as running at about
 * 1.15 kHz. Anything that only ever ran on Intel would look perfect
 * and be wrong on every Mac sold since 2020, so both timebases are
 * driven through the same fit here.
 *
 * The rest is the guard behaviour: a device restarting resets
 * mSampleTime, a driver need not set either valid flag, and
 * mRateScalar is read only when the HAL says it means something. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define kAudioTimeStampSampleTimeValid 1u
#define kAudioTimeStampHostTimeValid   2u
#define kAudioTimeStampRateScalarValid 4u

typedef struct
{
   double   mSampleTime;
   uint64_t mHostTime;
   double   mRateScalar;
   uint32_t mFlags;
} AudioTimeStamp;

static int failures;

static void fail(const char *what, const char *detail)
{
   printf("   FAIL %s: %s\n", what, detail);
   failures++;
}

/* The estimator, as the render callback runs it. */
typedef struct
{
   double   ns_per_tick;
   double   anchor_sample;
   uint64_t anchor_host;
   int      have_anchor;
   double   sx, sy, sxx, sxy, n;
   int      ppm;
   int      valid;
   int      scalar_ppm;
   int      scalar_valid;
   unsigned output_rate;
} clk_t;

static void clk_feed(clk_t *c, const AudioTimeStamp *ts)
{
   uint32_t f = ts->mFlags;

   if (     (f & kAudioTimeStampRateScalarValid)
         && ts->mRateScalar > 0.9 && ts->mRateScalar < 1.1)
   {
      c->scalar_ppm   = (int)((ts->mRateScalar - 1.0) * 1000000.0);
      c->scalar_valid = 1;
   }

   if (     !(f & kAudioTimeStampSampleTimeValid)
         || !(f & kAudioTimeStampHostTimeValid)
         || c->ns_per_tick <= 0.0)
      return;

   if (!c->have_anchor)
   {
      c->anchor_sample = ts->mSampleTime;
      c->anchor_host   = ts->mHostTime;
      c->have_anchor   = 1;
      c->sx = c->sy = c->sxx = c->sxy = c->n = 0.0;
   }
   else if (ts->mHostTime > c->anchor_host
         && ts->mSampleTime >= c->anchor_sample)
   {
      double x = (double)(ts->mHostTime - c->anchor_host)
         * c->ns_per_tick / 1000000000.0;
      double y = ts->mSampleTime - c->anchor_sample;
      double d;

      c->sx  += x;
      c->sy  += y;
      c->sxx += x * x;
      c->sxy += x * y;
      c->n   += 1.0;

      d = c->n * c->sxx - c->sx * c->sx;
      if (x >= 1.0 && d > 0.0 && c->output_rate)
      {
         double measured = (c->n * c->sxy - c->sx * c->sy) / d;
         double ppm      = (measured / (double)c->output_rate - 1.0)
            * 1000000.0;
         if (ppm > -100000.0 && ppm < 100000.0)
         {
            c->ppm   = (int)ppm;
            c->valid = 1;
         }
      }
   }
   else
   {
      c->anchor_sample = ts->mSampleTime;
      c->anchor_host   = ts->mHostTime;
      c->sx = c->sy = c->sxx = c->sxy = c->n = 0.0;
   }
}

/* Runs a device at true_rate for a number of seconds against a given
 * timebase, with jitter on the host timestamps. */
static int run(double true_rate, unsigned nominal, double seconds,
      uint32_t numer, uint32_t denom, double jitter_ns, int *valid)
{
   clk_t    c;
   unsigned period = 512;
   double   sample = 0.0;
   unsigned n, total;

   memset(&c, 0, sizeof(c));
   c.ns_per_tick = (double)numer / (double)denom;
   c.output_rate = nominal;
   total         = (unsigned)(seconds * true_rate / period);

   for (n = 0; n < total; n++)
   {
      AudioTimeStamp ts;
      double ns  = sample * 1000000000.0 / true_rate
         + ((n & 1) ? jitter_ns : -jitter_ns);

      memset(&ts, 0, sizeof(ts));
      ts.mSampleTime = sample;
      /* Nanoseconds back into the device's own tick units, which is
       * what the HAL would have handed over. */
      ts.mHostTime   = (uint64_t)(ns / c.ns_per_tick);
      ts.mFlags      = kAudioTimeStampSampleTimeValid
                     | kAudioTimeStampHostTimeValid;
      clk_feed(&c, &ts);
      sample += period;
   }

   *valid = c.valid;
   return c.ppm;
}

int main(void)
{
   printf("1. the same clock through both timebases\n");
   {
      struct { const char *name; uint32_t num, den; } tb[] = {
         { "Intel  1/1",   1, 1 },
         { "Apple  125/3", 125, 3 },
         /* Not a shipping ratio; here so the arithmetic is not being
          * flattered by the two that are. */
         { "odd    7/9",   7, 9 }
      };
      unsigned i;
      for (i = 0; i < sizeof(tb) / sizeof(*tb); i++)
      {
         int valid = 0;
         int got   = run(48002.4, 48000, 10.0, tb[i].num, tb[i].den, 0.0, &valid);
         if (!valid)
            fail(tb[i].name, "no estimate");
         else if (got < 48 || got > 52)
         {
            char d[96];
            snprintf(d, sizeof(d), "read %+d ppm, expected +50", got);
            fail(tb[i].name, d);
         }
         else
            printf("   ok   %-13s read %+d ppm\n", tb[i].name, got);
      }
   }

   printf("2. what ignoring the timebase would have done\n");
   {
      /* The Apple ratio treated as one to one: this is the bug the
       * case above exists to prevent, and it is not subtle. */
      double wrong = 48000.0 * 3.0 / 125.0;
      printf("   a 48000 Hz device would read as %.0f Hz on Apple silicon\n",
            wrong);
      if (wrong > 47000.0)
         fail("timebase", "the demonstration is not demonstrating anything");
      else
         printf("   ok   which is why the ratio is asked for, not assumed\n");
   }

   printf("3. the fit against jitter on the host timestamps\n");
   {
      int valid = 0;
      int got   = run(48002.4, 48000, 10.0, 125, 3, 2000000.0, &valid);
      if (!valid)
         fail("jitter", "no estimate");
      else if (got < 48 || got > 52)
      {
         char d[96];
         snprintf(d, sizeof(d), "read %+d ppm with jitter, expected +50", got);
         fail("jitter", d);
      }
      else
         printf("   ok   +-2 ms of jitter, still read %+d ppm\n", got);
   }

   printf("4. rates a Mac actually runs at\n");
   {
      struct { const char *name; double rate; unsigned nom; int want; } d[] = {
         { "44100 exact",  44100.0,   44100,   0 },
         { "44100 +30ppm", 44101.323, 44100,  30 },
         { "48000 -80ppm", 47996.16,  48000, -80 },
         { "96000 +10ppm", 96000.96,  96000,  10 }
      };
      unsigned i;
      for (i = 0; i < sizeof(d) / sizeof(*d); i++)
      {
         int valid = 0;
         int got   = run(d[i].rate, d[i].nom, 10.0, 125, 3, 0.0, &valid);
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
            printf("   ok   %-12s read %+d ppm\n", d[i].name, got);
      }
   }

   printf("5. what the estimator refuses\n");
   {
      clk_t          c;
      AudioTimeStamp ts;

      /* No timebase: nothing to put on the time axis. */
      memset(&c, 0, sizeof(c)); c.output_rate = 48000;
      memset(&ts, 0, sizeof(ts));
      ts.mSampleTime = 0; ts.mHostTime = 0;
      ts.mFlags = kAudioTimeStampSampleTimeValid | kAudioTimeStampHostTimeValid;
      clk_feed(&c, &ts);
      ts.mSampleTime = 480000; ts.mHostTime = 10000000000ULL;
      clk_feed(&c, &ts);
      if (c.valid)
         fail("timebase", "fitted without a timebase");
      else
         printf("   ok   no timebase: nothing published\n");

      /* Host time valid, sample time not. */
      memset(&c, 0, sizeof(c));
      c.output_rate = 48000; c.ns_per_tick = 1.0;
      memset(&ts, 0, sizeof(ts));
      ts.mHostTime = 0;              ts.mFlags = kAudioTimeStampHostTimeValid;
      clk_feed(&c, &ts);
      ts.mHostTime = 10000000000ULL; ts.mSampleTime = 480000;
      clk_feed(&c, &ts);
      if (c.valid)
         fail("flags", "fitted on a sample time marked invalid");
      else
         printf("   ok   sample time invalid: nothing published\n");

      /* The device restarts and mSampleTime goes back to zero. */
      memset(&c, 0, sizeof(c));
      c.output_rate = 48000; c.ns_per_tick = 1.0;
      memset(&ts, 0, sizeof(ts));
      ts.mFlags = kAudioTimeStampSampleTimeValid | kAudioTimeStampHostTimeValid;
      ts.mSampleTime = 480000; ts.mHostTime = 10000000000ULL;
      clk_feed(&c, &ts);
      ts.mSampleTime = 0;      ts.mHostTime = 11000000000ULL;
      clk_feed(&c, &ts);
      if (c.valid)
         fail("restart", "published across a sample-time reset");
      else if (c.anchor_sample != 0.0)
         fail("restart", "did not re-anchor");
      else
         printf("   ok   device restart: re-anchored, nothing published\n");

      /* mRateScalar, taken only when flagged and only when plausible. */
      memset(&c, 0, sizeof(c));
      memset(&ts, 0, sizeof(ts));
      ts.mRateScalar = 1.00005;  /* +50 ppm */
      ts.mFlags      = kAudioTimeStampRateScalarValid;
      clk_feed(&c, &ts);
      if (!c.scalar_valid || c.scalar_ppm < 49 || c.scalar_ppm > 51)
         fail("mRateScalar", "a flagged +50 ppm scalar was not read back");
      else
         printf("   ok   mRateScalar +50 ppm read back as %+d\n",
               c.scalar_ppm);

      memset(&c, 0, sizeof(c));
      ts.mRateScalar = 0.0;  /* a device that has not started */
      clk_feed(&c, &ts);
      if (c.scalar_valid)
         fail("mRateScalar", "took a zero scalar");
      else
         printf("   ok   a zero scalar is not a rate\n");

      memset(&c, 0, sizeof(c));
      ts.mRateScalar = 1.00005;
      ts.mFlags      = 0;    /* not flagged valid */
      clk_feed(&c, &ts);
      if (c.scalar_valid)
         fail("mRateScalar", "read a scalar the HAL did not flag");
      else
         printf("   ok   an unflagged scalar is not read\n");
   }

   if (failures)
   {
      printf("coreaudio clock: %d failure(s)\n", failures);
      return 1;
   }
   printf("coreaudio clock: the fit survives the timebase, the jitter and"
          " the restarts\n");
   return 0;
}
