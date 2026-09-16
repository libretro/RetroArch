/* Checks the ASIO device-clock estimate off Windows.
 *
 * Two things are worth checking here without an interface plugged in.
 *
 * The first is the ABI. The driver hands the callback an ASIOTime, and
 * the two numbers wanted out of it - the sample position and the
 * system time - are 64-bit quantities that the SDK carries as two
 * 32-bit halves on Windows, because asiosys.h leaves NATIVE_INT64 at 0
 * there. Getting that layout wrong does not fail to compile; it reads
 * a real driver's numbers as garbage inside a real-time callback. So
 * the field offsets are asserted against the SDK's layout, and the
 * recombination is checked across the 32-bit boundary where a wrong
 * shift or a sign-extended half would show.
 *
 * The second is the estimate. A driver's reported rate is never
 * exactly nominal, and the whole point of preferring this over
 * callback arrival times is that it does not carry the OS's scheduling
 * jitter - so the test feeds it positions and timestamps for a clock
 * running at a known offset, with jitter on the timestamps, and checks
 * the answer is the clock and not the jitter. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

typedef double ASIOSampleRate;

/* The driver declares these with unsigned long, which is four bytes on
 * Windows and is what a Windows driver writes. It is eight here, so
 * this models the target's width explicitly - the offsets asserted
 * below are the ones a Windows driver sees, and checking them against
 * this host's long would be checking the wrong ABI. That the target
 * really does have a four-byte long is asserted inside asio.c itself,
 * where the mingw compile is the thing that checks it. */
typedef unsigned int asio_ulong;
typedef struct ASIOSamples   { asio_ulong hi; asio_ulong lo; } ASIOSamples;
typedef struct ASIOTimeStamp { asio_ulong hi; asio_ulong lo; } ASIOTimeStamp;

typedef struct ASIOTimeCode
{
   double        speed;
   ASIOSamples   timeCodeSamples;
   asio_ulong    flags;
   char          future[64];
} ASIOTimeCode;

typedef struct AsioTimeInfo
{
   double         speed;
   ASIOTimeStamp  systemTime;
   ASIOSamples    samplePosition;
   ASIOSampleRate sampleRate;
   asio_ulong     flags;
   char           reserved[12];
} AsioTimeInfo;

typedef struct ASIOTime
{
   int          reserved[4];
   AsioTimeInfo timeInfo;
   ASIOTimeCode timeCode;
} ASIOTime;

#define kSystemTimeValid     (1UL << 0)
#define kSamplePositionValid (1UL << 1)

static int failures;

static void fail(const char *what, const char *detail)
{
   printf("   FAIL %s: %s\n", what, detail);
   failures++;
}

/* The recombination, as audio/drivers/asio.c has it. */
static unsigned long long asio_int64(asio_ulong hi, asio_ulong lo)
{
   return ((unsigned long long)(unsigned int)hi << 32) | (unsigned int)lo;
}

/* The estimator, as the callback runs it. State and all, so what is
 * checked is the sequence of callbacks and not one calculation. */
typedef struct
{
   unsigned long long anchor_pos;
   unsigned long long anchor_ns;
   int                have_anchor;
   double             sx, sy, sxx, sxy, n;
   int                ppm;
   int                valid;
   unsigned           sample_rate;
} clock_t_;

static void clock_feed(clock_t_ *c, const ASIOTime *params)
{
   asio_ulong flags = params->timeInfo.flags;

   if (!(flags & kSamplePositionValid) || !(flags & kSystemTimeValid))
      return;

   {
      unsigned long long pos = asio_int64(params->timeInfo.samplePosition.hi,
                                          params->timeInfo.samplePosition.lo);
      unsigned long long ns  = asio_int64(params->timeInfo.systemTime.hi,
                                          params->timeInfo.systemTime.lo);

      if (!c->have_anchor)
      {
         c->anchor_pos  = pos;
         c->anchor_ns   = ns;
         c->have_anchor = 1;
         c->sx = c->sy = c->sxx = c->sxy = c->n = 0.0;
      }
      else if (pos >= c->anchor_pos && ns > c->anchor_ns)
      {
         double x = (double)(ns  - c->anchor_ns) / 1000000000.0;
         double y = (double)(pos - c->anchor_pos);
         double d;

         c->sx  += x;
         c->sy  += y;
         c->sxx += x * x;
         c->sxy += x * y;
         c->n   += 1.0;

         d = c->n * c->sxx - c->sx * c->sx;
         if (x >= 1.0 && d > 0.0 && c->sample_rate)
         {
            double measured = (c->n * c->sxy - c->sx * c->sy) / d;
            double ppm      = (measured / (double)c->sample_rate - 1.0)
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
         c->anchor_pos = pos;
         c->anchor_ns  = ns;
         c->sx = c->sy = c->sxx = c->sxy = c->n = 0.0;
      }
   }
}

static void split(unsigned long long v, asio_ulong *hi, asio_ulong *lo)
{
   *hi = (asio_ulong)(v >> 32);
   *lo = (asio_ulong)(v & 0xFFFFFFFFULL);
}

static void set_time(ASIOTime *t, unsigned long long pos,
      unsigned long long ns, asio_ulong flags)
{
   memset(t, 0, sizeof(*t));
   split(pos, &t->timeInfo.samplePosition.hi, &t->timeInfo.samplePosition.lo);
   split(ns,  &t->timeInfo.systemTime.hi,     &t->timeInfo.systemTime.lo);
   t->timeInfo.flags = flags;
}

/* Runs a device at true_rate for the given seconds and returns what the
 * estimator made of it. jitter_ns is added to and subtracted from the
 * timestamps alternately, which is the scheduling noise this estimate
 * is supposed to be immune to. */
static int run_device(double true_rate, unsigned nominal, double seconds,
      long long jitter_ns, int *valid)
{
   clock_t_ c;
   unsigned period = 256;
   unsigned long long pos = 0;
   double   t_ns = 0.0;
   unsigned n, total = (unsigned)(seconds * true_rate / period);

   memset(&c, 0, sizeof(c));
   c.sample_rate = nominal;

   for (n = 0; n < total; n++)
   {
      ASIOTime          t;
      long long         j  = (n & 1) ? jitter_ns : -jitter_ns;
      unsigned long long ns;

      t_ns = (double)pos * 1000000000.0 / true_rate;
      ns   = (unsigned long long)(t_ns + (double)j);
      set_time(&t, pos, ns, kSamplePositionValid | kSystemTimeValid);
      clock_feed(&c, &t);
      pos += period;
   }

   *valid = c.valid;
   return c.ppm;
}

int main(void)
{
   printf("1. the SDK's layout, which is an ABI and not ours to choose\n");
   {
      /* Offsets a Windows driver writes to, from the SDK's asio.h with
       * NATIVE_INT64 at 0. If any of these move, a real driver's
       * numbers come back as garbage. */
      struct { const char *name; size_t got; size_t want; } f[] = {
         { "AsioTimeInfo::speed",           offsetof(AsioTimeInfo, speed),           0  },
         { "AsioTimeInfo::systemTime",      offsetof(AsioTimeInfo, systemTime),      8  },
         { "AsioTimeInfo::samplePosition",  offsetof(AsioTimeInfo, samplePosition),  16 },
         { "AsioTimeInfo::sampleRate",      offsetof(AsioTimeInfo, sampleRate),      24 },
         { "AsioTimeInfo::flags",           offsetof(AsioTimeInfo, flags),           32 },
         { "AsioTimeInfo::reserved",        offsetof(AsioTimeInfo, reserved),        36 },
         { "ASIOTime::timeInfo",            offsetof(ASIOTime, timeInfo),            16 },
         { "ASIOTime::timeCode",            offsetof(ASIOTime, timeCode),            64 }
      };
      unsigned i;
      for (i = 0; i < sizeof(f) / sizeof(*f); i++)
         if (f[i].got != f[i].want)
         {
            char d[128];
            snprintf(d, sizeof(d), "%s is at %u, the SDK puts it at %u",
                  f[i].name, (unsigned)f[i].got, (unsigned)f[i].want);
            fail("layout", d);
         }
      if (sizeof(AsioTimeInfo) != 48)
         fail("layout", "AsioTimeInfo is not 48 bytes");
      if (!failures)
         printf("   ok   every field where the SDK puts it,"
                " AsioTimeInfo is 48 bytes\n");
   }

   printf("2. the two halves recombine, across the 32-bit boundary\n");
   {
      unsigned long long vals[] = {
         0ULL, 1ULL, 0xFFFFFFFFULL, 0x100000000ULL, 0x100000001ULL,
         0x7FFFFFFFFFFFFFFFULL, 0xFFFFFFFF00000000ULL,
         /* A day of samples at 48 kHz, which is where a 32-bit
          * position would have wrapped long since. */
         4147200000ULL
      };
      unsigned i;
      int bad = 0;
      for (i = 0; i < sizeof(vals) / sizeof(*vals); i++)
      {
         asio_ulong hi, lo;
         split(vals[i], &hi, &lo);
         if (asio_int64(hi, lo) != vals[i])
         {
            char d[96];
            snprintf(d, sizeof(d), "%llu came back as %llu",
                  vals[i], asio_int64(hi, lo));
            fail("recombine", d);
            bad = 1;
         }
      }
      if (!bad)
         printf("   ok   including 0xFFFFFFFF, the boundary, and a day"
                " of samples\n");
   }

   printf("3. a clock at a known offset, read back\n");
   {
      struct { const char *name; double rate; unsigned nominal; int want; } d[] = {
         { "exact",        48000.0,  48000,    0 },
         { "+50 ppm",      48002.4,  48000,   50 },
         { "-50 ppm",      47997.6,  48000,  -50 },
         { "+500 ppm",     48024.0,  48000,  500 },
         { "44.1k exact",  44100.0,  44100,    0 },
         { "44.1k -20ppm", 44099.118, 44100, -20 }
      };
      unsigned i;
      for (i = 0; i < sizeof(d) / sizeof(*d); i++)
      {
         int valid = 0;
         int got   = run_device(d[i].rate, d[i].nominal, 10.0, 0, &valid);
         if (!valid)
            fail(d[i].name, "no estimate after ten seconds");
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

   printf("4. scheduling jitter on the timestamps does not move it\n");
   {
      /* Two milliseconds either way is far worse than a real system,
       * and several periods wide at 256 frames. */
      int valid = 0;
      int got   = run_device(48002.4, 48000, 10.0, 2000000LL, &valid);
      if (!valid)
         fail("jitter", "no estimate");
      else if (got < 48 || got > 52)
      {
         char det[96];
         snprintf(det, sizeof(det), "read %+d ppm with jitter,"
               " expected +50", got);
         fail("jitter", det);
      }
      else
         printf("   ok   +-2 ms of jitter, still read %+d ppm\n", got);
   }

   printf("5. what the estimator refuses\n");
   {
      clock_t_  c;
      ASIOTime  t;

      /* Flags clear: a driver need not fill either number in. */
      memset(&c, 0, sizeof(c)); c.sample_rate = 48000;
      set_time(&t, 0, 0, 0);
      clock_feed(&c, &t);
      set_time(&t, 480000, 10000000000ULL, 0);
      clock_feed(&c, &t);
      if (c.valid)
         fail("flags", "produced an estimate from numbers marked invalid");
      else
         printf("   ok   flags clear: nothing published\n");

      /* Under a second of window. */
      memset(&c, 0, sizeof(c)); c.sample_rate = 48000;
      set_time(&t, 0, 0, kSamplePositionValid | kSystemTimeValid);
      clock_feed(&c, &t);
      set_time(&t, 24000, 500000000ULL, kSamplePositionValid | kSystemTimeValid);
      clock_feed(&c, &t);
      if (c.valid)
         fail("window", "published from half a second of window");
      else
         printf("   ok   half a second: nothing published\n");

      /* A position that goes backwards - a driver resetting - must
       * re-anchor and not produce a wild number. */
      memset(&c, 0, sizeof(c)); c.sample_rate = 48000;
      set_time(&t, 480000, 10000000000ULL, kSamplePositionValid | kSystemTimeValid);
      clock_feed(&c, &t);
      set_time(&t, 0, 11000000000ULL, kSamplePositionValid | kSystemTimeValid);
      clock_feed(&c, &t);
      if (c.valid)
         fail("reset", "published across a position reset");
      else if (c.anchor_pos != 0)
         fail("reset", "did not re-anchor on the new position");
      else
         printf("   ok   position reset: re-anchored, nothing published\n");

      /* Something far enough out that it is a misread, not a clock. */
      memset(&c, 0, sizeof(c)); c.sample_rate = 48000;
      set_time(&t, 0, 0, kSamplePositionValid | kSystemTimeValid);
      clock_feed(&c, &t);
      set_time(&t, 4800000, 10000000000ULL,
            kSamplePositionValid | kSystemTimeValid);
      clock_feed(&c, &t);
      if (c.valid)
         fail("absurd", "published a rate ten times nominal");
      else
         printf("   ok   ten times nominal: dropped\n");
   }

   if (failures)
   {
      printf("asio clock: %d failure(s)\n", failures);
      return 1;
   }
   printf("asio clock: the SDK layout holds and the estimate is the clock,"
          " not the jitter\n");
   return 0;
}
