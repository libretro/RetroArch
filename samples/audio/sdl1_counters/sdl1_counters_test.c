/* The SDL 1.2 audio driver's device accounting, against SDL's own
 * dummy device.
 *
 * Two contracts are under test. frames_consumed() is device time: the
 * playback callback is the device asking for one period, so what it
 * asks for advances at the device's rate whether or not there was
 * audio to fill it. underruns() counts the periods it had to
 * zero-fill. Feeding at the frame rate should move the first and leave
 * the second alone; starving the driver should move both.
 *
 * The run under SANITIZER=thread is the other half of this suite.
 * SDL 1.2 opens the device with its thread already running and
 * unpauses it with a plain flag write, so the ring and the park set up
 * between those two points reached that thread unordered - a race that
 * holds on x86 by accident and not at all on the MIPS and ARM
 * handhelds this driver serves. Taking SDL's audio lock across the
 * unpause is what orders them, and removing that lock brings the
 * report straight back.
 *
 * Builds audio/drivers/sdl1_audio.c itself: the subject is the
 * driver's own accounting, so a copy of the arithmetic would prove
 * nothing.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <boolean.h>

#include "../../../audio/audio_driver.h"

extern audio_driver_t audio_sdl1;

#define OUT_RATE    48000
#define CHANNELS    2
#define FPS         60
#define LATENCY_MS  64

static unsigned failures;

#define CHECK(cond, ...) \
   do { \
      if (!(cond)) \
      { \
         printf("FAIL %s:%d: ", __FILE__, __LINE__); \
         printf(__VA_ARGS__); \
         printf("\n"); \
         failures++; \
      } \
   } while (0)

static void nap_ns(long ns)
{
   struct timespec ts;
   ts.tv_sec  = ns / 1000000000L;
   ts.tv_nsec = ns % 1000000000L;
   nanosleep(&ts, NULL);
}

int main(void)
{
   const audio_driver_t *d = &audio_sdl1;
   unsigned  out_rate      = OUT_RATE;
   size_t    per_frame     = OUT_RATE / FPS;
   size_t    block         = per_frame * CHANNELS * sizeof(int16_t);
   size_t    i, consumed_open, consumed_fed, consumed_starved;
   size_t    under_open, under_fed, under_starved;
   int16_t  *buf;
   void     *ctx;

   /* No hardware in a harness; SDL's dummy device paces on the host
    * clock, which is the clock this measures against. */
   setenv("SDL_AUDIODRIVER", "dummy", 1);

   if (!(ctx = d->init(NULL, OUT_RATE, LATENCY_MS, &out_rate)))
   {
      printf("SKIP: no SDL 1.2 audio device would open\n");
      return 0;
   }

   if (!d->frames_consumed || !d->underruns)
   {
      printf("FAIL: the driver reports neither device time nor underruns\n");
      d->free(ctx);
      return 1;
   }

   d->start(ctx, false);
   d->set_nonblock_state(ctx, false);

   buf = (int16_t*)calloc(per_frame * CHANNELS, sizeof(int16_t));
   if (!buf)
   {
      d->free(ctx);
      return 1;
   }

   consumed_open = d->frames_consumed(ctx);
   under_open    = d->underruns(ctx);
   printf("at open:       consumed %u frames, %u underruns\n",
         (unsigned)consumed_open, (unsigned)under_open);
   /* init prefills the ring, so nothing has gone short yet. */
   CHECK(under_open == 0, "%u underruns before the device asked for anything",
         (unsigned)under_open);

   /* A second of audio, handed over one frame at a time. */
   for (i = 0; i < FPS; i++)
   {
      d->write(ctx, buf, block);
      nap_ns(1000000000L / FPS);
   }

   consumed_fed = d->frames_consumed(ctx) - consumed_open;
   under_fed    = d->underruns(ctx)       - under_open;
   printf("fed for 1 s:   consumed %u frames (%.2f s of device time), %u underruns\n",
         (unsigned)consumed_fed, (double)consumed_fed / OUT_RATE,
         (unsigned)under_fed);
   /* A wide band: the harness's own pacing is a sleep loop, not a
    * clock. What it rules out is a count that runs at some multiple of
    * the device's rate, or does not run at all. */
   CHECK(consumed_fed > OUT_RATE / 2 && consumed_fed < OUT_RATE * 2,
         "%u frames of device time for a second at %d Hz",
         (unsigned)consumed_fed, OUT_RATE);
   CHECK(under_fed <= FPS / 2, "%u underruns while the driver was being fed",
         (unsigned)under_fed);

   /* Now starve it. The fifo drains, and every period after that is
    * one the device asked for and did not get. */
   consumed_fed  += consumed_open;
   under_fed     += under_open;
   nap_ns(500000000L);

   consumed_starved = d->frames_consumed(ctx) - consumed_fed;
   under_starved    = d->underruns(ctx)       - under_fed;
   printf("starved 0.5 s: consumed %u frames, %u underruns\n",
         (unsigned)consumed_starved, (unsigned)under_starved);
   CHECK(consumed_starved > 0,
         "device time stopped while the device was still running");
   CHECK(under_starved > 0, "no underruns counted while the driver was starved");

   d->stop(ctx);
   free(buf);
   d->free(ctx);

   if (failures)
   {
      printf("sdl1 counters: %u failure%s\n", failures, failures == 1 ? "" : "s");
      return 1;
   }
   printf("sdl1 counters: device time tracks the device, silence is counted where it happens\n");
   return 0;
}
