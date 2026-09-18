/*  RetroArch - A frontend for libretro.
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

/* A clock driven by vblank timestamps, and the beam position it
 * implies. Pure arithmetic, no platform calls: the caller feeds it the
 * time of each vblank it observes and asks it where the beam is at a
 * given moment. Scanline Sync uses it on Windows, fed from
 * D3DKMTWaitForVerticalBlankEvent, in place of polling
 * D3DKMTGetScanLine; samples/gfx/vblank_clock tests it on its own.
 *
 * A waiter only ever wakes late, so the clock tracks the lower edge of
 * the timestamps: it follows an early arrival strongly and a late one
 * barely, and corrects the period from the same phase error. A gap that is a whole number of periods is
 * missed vblanks and only moves the phase; anything else starts over
 * from the nominal rate. A period that will not settle - variable
 * refresh - never reaches VBLANK_CLOCK_SETTLE good vblanks in a row,
 * so the clock is never used for it. */

#ifndef __VBLANK_CLOCK_H
#define __VBLANK_CLOCK_H

#include <math.h>
#include <stdint.h>
#include <string.h>

#include <boolean.h>
#include <retro_inline.h>

/* Vblanks that must fit the period before the beam is reported: how
 * long the phase and period take to converge from the mode's nominal
 * rate. Measured against simulated displays up to 600 ppm off nominal
 * with a waiter waking up to 460 us late: 8 let the beam be reported
 * 54 lines out while still converging, 120 keeps it within 5
 * (samples/gfx/vblank_clock). Two seconds at 60 Hz. */
#define VBLANK_CLOCK_SETTLE 120

typedef struct vblank_clock
{
   int64_t  last_us;     /* phase: the time of the last vblank */
   double   period_us;   /* smoothed */
   double   nominal_us;  /* the mode's rate, to start from */
   unsigned good;        /* consecutive vblanks that fit the period */
} vblank_clock_t;

static INLINE void vblank_clock_init(vblank_clock_t *c, double nominal_us)
{
   memset(c, 0, sizeof(*c));
   c->nominal_us = nominal_us;
}

/* A vblank wait that did not end in a vblank (a timeout, an error):
 * the run of good vblanks is broken. */
static INLINE void vblank_clock_miss(vblank_clock_t *c)
{
   c->good = 0;
}

/* A vblank observed at @now_us. */
static INLINE void vblank_clock_feed(vblank_clock_t *c, int64_t now_us)
{
   double gap, k, off;

   if (!c->last_us || c->period_us <= 0.0)
   {
      c->period_us = c->nominal_us;
      c->good      = 0;
      c->last_us   = now_us;
      return;
   }

   gap = (double)(now_us - c->last_us);
   k   = floor(gap / c->period_us + 0.5);
   off = gap - k * c->period_us;

   /* Less than half a period after the last one: the same vblank
    * reported twice, not a new one. */
   if (k < 1.0)
      return;

   if (fabs(off) < 0.1 * c->period_us)
   {
      /* A waiter wakes late, never early, so the timestamps are the
       * true vblanks plus a one-sided lateness, and the estimate to
       * track is their lower edge. An arrival earlier than predicted
       * says the prediction runs late: follow it by half. One later
       * than predicted is mostly the waiter's own lateness: follow it
       * by a sixty-fourth, which is what lets a period that is really
       * longer creep in. The period is corrected from the same phase
       * error, spread over the k periods it built up over. */
      double predicted = (double)c->last_us + k * c->period_us;
      double corr      = (off < 0.0) ? off / 2.0 : off / 64.0;
      c->last_us       = (int64_t)(predicted + corr);
      c->period_us    += corr / (k * 8.0);
      /* Missed vblanks - a whole number of periods - still fit: they
       * keep the count, only a clean one adds to it. Resetting on them
       * left the clock usable 8.5% of the time with 2% missed. */
      if (k == 1.0)
         c->good++;
   }
   else
   {
      c->period_us = c->nominal_us;
      c->good      = 0;
      c->last_us   = now_us;
   }
}

/* The beam line at @now_us for a mode of @active of @total lines,
 * counting from the first blanking line, where the vblank event fires;
 * or -1 while the clock has not settled, or when it has seen no vblank
 * for two periods (display asleep, a mode change in flight). */
static INLINE int vblank_clock_beam(const vblank_clock_t *c, int64_t now_us,
      unsigned active, unsigned total)
{
   double since;
   int line;

   if (     c->good < VBLANK_CLOCK_SETTLE
         || c->period_us <= 0.0
         || !c->last_us
         || total <= active)
      return -1;
   since = (double)(now_us - c->last_us);
   if (since < 0.0 || since > 2.0 * c->period_us)
      return -1;
   line = (int)active
      + (int)(fmod(since, c->period_us) / c->period_us * (double)total);
   if (line >= (int)total)
      line -= (int)total;
   return line;
}

/* Microseconds from @now_us until the beam reaches @target_line, or -1
 * when the clock cannot say. 0 when the beam is at or past it in this
 * frame, as a counter polled until it reached the line would find. */
static INLINE int64_t vblank_clock_until_line(const vblank_clock_t *c,
      int64_t now_us, unsigned active, unsigned total, int target_line)
{
   int beam = vblank_clock_beam(c, now_us, active, total);
   if (beam < 0)
      return -1;
   if (beam >= target_line)
      return 0;
   return (int64_t)((double)(target_line - beam) * c->period_us
         / (double)total);
}

#endif
