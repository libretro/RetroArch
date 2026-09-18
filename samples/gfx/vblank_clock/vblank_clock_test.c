/* Test for gfx/common/vblank_clock.h, the arithmetic Scanline Sync on
 * Windows uses in place of polling D3DKMTGetScanLine: a vblank clock fed
 * the time of each vblank, and the beam position it implies.
 *
 * A simulated display - a true refresh period a little off its nominal
 * rate, as real ones are, and a mode of active/total lines - delivers
 * vblanks through a "waiter" that wakes late by random jitter, as the
 * clock thread's D3DKMTWaitForVerticalBlankEvent return does. The beam
 * the clock reports is checked against the true beam.
 *
 *   settles            -> no beam before VBLANK_CLOCK_SETTLE vblanks
 *   tracks             -> across 50 displays up to 600 ppm off nominal,
 *                         beam and wait landing within 6 lines
 *   missed vblanks     -> one in fifty never arriving keeps it usable
 *   duplicate          -> the same vblank reported twice is ignored
 *   stale              -> no beam once vblanks stop for two periods
 *   variable refresh   -> a period that wanders is never reported
 *   past the target    -> wait 0, as the counter loop would find
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "gfx/common/vblank_clock.h"

static unsigned failures = 0;

static void check(bool cond, const char *what)
{
   printf("  [%s] %s\n", cond ? "pass" : "FAIL", what);
   if (!cond)
      failures++;
}

/* 1920x1080p at "60 Hz": 1125 total lines, the true period 0.01% off
 * nominal. */
#define ACTIVE      1080u
#define TOTAL       1125u
#define NOMINAL_US  (1000000.0 / 60.0)
#define TRUE_US     (NOMINAL_US * 1.0001)

static double true_vblank(int n) { return 1000000.0 + n * TRUE_US; }

/* Wake-up lateness: mostly tens of us, sometimes a few hundred. */
static double jitter(void)
{
   double r = (double)rand() / RAND_MAX;
   return r < 0.95 ? r * 60.0 : 60.0 + r * 400.0;
}

static int line_err(int a, int b)
{
   int d = abs(a - b);
   return d > (int)TOTAL / 2 ? (int)TOTAL - d : d;
}

/* One simulated run: a true period @ppm off nominal, @frames vblanks
 * of which about one in @miss_every never arrives (0: none). Returns the
 * worst beam error and the worst wait landing, from the frame after the
 * clock settles, in lines. */
static void run(unsigned seed, double ppm, int frames, int miss_every,
      int *worst_beam, int *worst_wait, double *period_err, int *first_use)
{
   vblank_clock_t c;
   double true_us = NOMINAL_US * (1.0 + ppm / 1000000.0);
   int n, i;

   srand(seed);
   vblank_clock_init(&c, NOMINAL_US);
   *worst_beam = *worst_wait = 0;
   *first_use  = -1;
   for (n = 0; n < frames; n++)
   {
      double vb = 1000000.0 + n * true_us;
      if (miss_every && rand() % miss_every == 0)
         continue;
      vblank_clock_feed(&c, (int64_t)(vb + jitter()));
      for (i = 1; i < 10; i++)
      {
         double t   = vb + 500.0 + i * (true_us - 1000.0) / 10.0;
         int beam   = vblank_clock_beam(&c, (int64_t)t, ACTIVE, TOTAL);
         int target = 200 + 90 * i;
         int64_t w;
         double since;
         int truth;
         if (beam < 0)
            continue;
         if (*first_use < 0)
            *first_use = n;
         since = fmod(t - 1000000.0, true_us);
         truth = (int)ACTIVE + (int)(since / true_us * TOTAL);
         if (truth >= (int)TOTAL)
            truth -= (int)TOTAL;
         if (line_err(beam, truth) > *worst_beam)
            *worst_beam = line_err(beam, truth);
         w = vblank_clock_until_line(&c, (int64_t)t, ACTIVE, TOTAL, target);
         if (w > 0)
         {
            double tl = fmod(t + (double)w - 1000000.0, true_us);
            int landed = (int)ACTIVE + (int)(tl / true_us * TOTAL);
            if (landed >= (int)TOTAL)
               landed -= (int)TOTAL;
            if (line_err(landed, target) > *worst_wait)
               *worst_wait = line_err(landed, target);
         }
      }
   }
   *period_err = c.period_us - true_us;
}

int main(void)
{
   vblank_clock_t c;
   int n, i;
   unsigned seed;
   int wb = 0, ww = 0, first = 1 << 30, any_early = 0;
   double worst_period = 0.0;

   printf("tracks: 50 displays, -600..+600 ppm off nominal, waiter up to 460 us late\n");
   for (seed = 1; seed <= 50; seed++)
   {
      int b, w, f;
      double pe;
      run(seed, (double)(((int)seed % 7) - 3) * 200.0, 1200, 0, &b, &w, &pe, &f);
      if (b > wb) wb = b;
      if (w > ww) ww = w;
      if (fabs(pe) > worst_period) worst_period = fabs(pe);
      if (f >= 0 && f < first) first = f;
      if (f >= 0 && f < VBLANK_CLOCK_SETTLE) any_early = 1;
   }
   printf("  (worst beam error %d lines, worst wait landing %d lines, worst period "
          "error %.2f us; one D3DKMTGetScanLine call takes ~15 lines)\n", wb, ww, worst_period);
   check(!any_early, "no beam before it has settled");
   check(wb <= 6, "beam within 6 lines of the truth (~90 us)");
   check(ww <= 6, "a wait lands within 6 lines of its target");
   check(worst_period < 5.0, "period converges within 5 us");

   printf("missed vblanks: one in fifty never reported\n");
   {
      int b, w, f;
      double pe;
      run(99, 300.0, 3000, 50, &b, &w, &pe, &f);
      check(f >= 0 && b <= 6, "stays usable and on the beam");
   }

   /* a settled clock for the cases below */
   srand(7);
   vblank_clock_init(&c, NOMINAL_US);
   for (n = 0; n < 400; n++)
      vblank_clock_feed(&c, (int64_t)(true_vblank(n) + jitter()));

   printf("duplicate\n");
   {
      vblank_clock_t d = c;
      double t         = true_vblank(n - 1) + 5000.0;
      int before       = vblank_clock_beam(&d, (int64_t)t, ACTIVE, TOTAL);
      vblank_clock_feed(&d, d.last_us + 20);   /* the same vblank again */
      check(vblank_clock_beam(&d, (int64_t)t, ACTIVE, TOTAL) == before
            && d.good == c.good, "a vblank reported twice is ignored");
   }

   printf("stale\n");
   {
      double t = true_vblank(n - 1) + 2.5 * TRUE_US;
      check(vblank_clock_beam(&c, (int64_t)t, ACTIVE, TOTAL) < 0,
            "no beam after two periods without a vblank");
   }

   printf("variable refresh\n");
   {
      vblank_clock_t v;
      double t = 1000000.0;
      int reported = 0;
      vblank_clock_init(&v, NOMINAL_US);
      for (i = 0; i < 2000; i++)
      {
         /* 48-60 Hz, a new rate every frame */
         t += 1000000.0 / (48.0 + 12.0 * rand() / RAND_MAX);
         vblank_clock_feed(&v, (int64_t)t);
         if (vblank_clock_beam(&v, (int64_t)t + 3000, ACTIVE, TOTAL) >= 0)
            reported++;
      }
      check(reported == 0, "a wandering period is never reported");
   }

   printf("past the target\n");
   {
      double t = true_vblank(n - 1) + 0.9 * TRUE_US;   /* near the bottom */
      check(vblank_clock_until_line(&c, (int64_t)t, ACTIVE, TOTAL, 100) == 0,
            "wait is 0 when the beam is already past the line");
   }

   printf("scanline sampling (XDDM): readings in, vblanks out\n");
   {
      /* A counter read twice a frame at varying points, each read
       * taking 2-30 us, and a driver that reports blanking as an error.
       * The period the sampler is given starts at the mode's whole-Hz
       * rate, as EnumDisplaySettings reports it, and follows the clock
       * once that has settled. */
      vblank_clock_t   k;
      vblank_sampler_t smp;
      double true_us = 1000000.0 / 59.94;
      unsigned total_seen = 0;
      int worst = 0, used = 0;
      memset(&smp, 0, sizeof(smp));
      vblank_clock_init(&k, 1000000.0 / 60.0);
      srand(11);
      for (n = 0; n < 1500; n++)
      {
         int r;
         for (r = 0; r < 2; r++)
         {
            double frac  = 0.05 + 0.85 * rand() / (double)RAND_MAX;
            double t0    = 1000000.0 + n * true_us + frac * true_us;
            double cost  = 2.0 + 28.0 * rand() / (double)RAND_MAX;
            double at    = t0 + cost * rand() / (double)RAND_MAX;
            double since = fmod(at - 1000000.0, true_us);
            int line     = (int)ACTIVE + (int)(since / true_us * TOTAL);
            unsigned tot = 0;
            int64_t vb;
            double period = (k.good >= VBLANK_CLOCK_SETTLE) ? k.period_us : k.nominal_us;
            if (line >= (int)TOTAL)
               line -= (int)TOTAL;
            if (line >= (int)ACTIVE)
               line = -1;              /* DDERR_VERTICALBLANKINPROGRESS */
            vb = vblank_sampler_feed(&smp, (int64_t)(t0 + cost / 2.0),
                  line, ACTIVE, period, &tot);
            if (vb)
            {
               vblank_clock_feed(&k, vb);
               total_seen = tot;
            }
         }
         if (n > 300)
         {
            double t = 1000000.0 + n * true_us + 0.5 * true_us;
            int beam = total_seen ? vblank_clock_beam(&k, (int64_t)t, ACTIVE, total_seen) : -1;
            if (beam >= 0)
            {
               double since = fmod(t - 1000000.0, true_us);
               int truth    = (int)ACTIVE + (int)(since / true_us * TOTAL);
               if (truth >= (int)TOTAL)
                  truth -= (int)TOTAL;
               used++;
               if (line_err(beam, truth) > worst)
                  worst = line_err(beam, truth);
            }
         }
      }
      printf("  (total lines implied %u of %u, worst beam error %d lines, usable %d of 1199 frames)\n",
            total_seen, TOTAL, worst, used);
      check(total_seen >= TOTAL - 3 && total_seen <= TOTAL + 3,
            "the total line count comes out of the rate");
      check(used > 1100 && worst <= 8, "the beam follows from readings alone, within 8 lines");
   }

   if (failures)
   {
      printf("\n%u failure(s)\n", failures);
      return 1;
   }
   printf("\nall passed\n");
   return 0;
}
