/* The latency contract, against real drivers.
 *
 * audio_driver.h says what init's latency argument, buffer_size() and
 * write_avail() mean: the driver holds about the setting between
 * write() returning and the device consuming it; buffer_size() is that
 * capacity in bytes of the output format; write_avail() is the room
 * now, in the same bytes, never above buffer_size(). The frontend
 * steers write_avail() to half of buffer_size(), so half the reported
 * size, in time, is the latency the user hears from the driver.
 *
 * Each driver here is opened on a backend that needs no hardware - the
 * ALSA null PCM, SDL's dummy audio, OpenAL Soft's null device - at two
 * settings, and checked: it opens; buffer_size() is the setting within
 * a tolerance for the driver's own granularity; write_avail() never
 * exceeds buffer_size(); a non-blocking write of a buffer's worth takes
 * room, and the room comes back while the device plays. A driver that
 * reports in frames, reports one stage of several, or reports a
 * constant fails the arithmetic here the way it misled the rate
 * control before. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>

#include <boolean.h>
#include <retro_miscellaneous.h>

#include "../../../audio/audio_driver.h"

extern audio_driver_t audio_alsa;
extern audio_driver_t audio_sdl;
extern audio_driver_t audio_openal;

static unsigned failures = 0;

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

typedef struct
{
   const audio_driver_t *driver;
   const char *device;
   /* The driver's own granularity, as a fraction of the setting the
    * report may miss it by: OpenAL queues fixed 1 KiB buffers, SDL
    * rounds its device buffer to a power of two. */
   double tolerance;
   /* Whether the null backend models a fill at all. ALSA's null PCM
    * consumes what is written the moment it is written, so its room is
    * always the whole buffer; the size and the bound are checked on
    * it, the dynamics are not. */
   bool models_fill;
} case_t;

static void exercise(const case_t *c, unsigned latency_ms)
{
   const audio_driver_t *d = c->driver;
   unsigned rate           = 48000;
   unsigned new_rate       = 0;
   void   *ctx;
   size_t  frame_bytes, buffer, avail_empty, avail_full, avail_later;
   double  buffer_ms;
   unsigned char *silence;

   printf("-- %s at %u ms\n", d->ident, latency_ms);

   ctx = d->init(c->device, rate, latency_ms, 0, &new_rate);
   CHECK(ctx != NULL, "%s: did not open on its null backend", d->ident);
   if (!ctx)
      return;
   if (!new_rate)
      new_rate = rate;

   frame_bytes = (d->use_float && d->use_float(ctx))
         ? 2 * sizeof(float) : 2 * sizeof(int16_t);

   CHECK(d->buffer_size != NULL && d->write_avail != NULL,
         "%s: no buffer_size() or write_avail()", d->ident);
   if (!d->buffer_size || !d->write_avail)
   {
      d->free(ctx);
      return;
   }

   /* 1. The buffer is the setting, in the driver's bytes. */
   buffer    = d->buffer_size(ctx);
   buffer_ms = (double)buffer / frame_bytes * 1000.0 / new_rate;
   printf("   buffer_size %u bytes = %.1f ms of %s at %u Hz\n",
         (unsigned)buffer, buffer_ms,
         (frame_bytes == 8) ? "float" : "int16", new_rate);
   CHECK(buffer > 0, "%s: reports a zero buffer", d->ident);
   CHECK(fabs(buffer_ms - latency_ms) <= latency_ms * c->tolerance,
         "%s: reports %.1f ms against a %u ms setting (tolerance %.0f%%)",
         d->ident, buffer_ms, latency_ms, c->tolerance * 100.0);

   /* 2. Room never exceeds the buffer. Where it starts is the driver's
    *    business: some prefill with silence and start full, some start
    *    empty. */
   avail_empty = d->write_avail(ctx);
   printf("   write_avail at open: %u bytes\n", (unsigned)avail_empty);
   CHECK(avail_empty <= buffer,
         "%s: write_avail %u exceeds buffer_size %u",
         d->ident, (unsigned)avail_empty, (unsigned)buffer);

   /* 3. A buffer's worth offered without blocking takes room, and
    *    never makes any: the driver takes what fits and no more. */
   d->set_nonblock_state(ctx, true);
   silence = (unsigned char*)calloc(1, buffer);
   d->write(ctx, silence, buffer);
   avail_full = d->write_avail(ctx);
   printf("   write_avail after offering a buffer's worth: %u bytes\n",
         (unsigned)avail_full);
   CHECK(avail_full <= buffer,
         "%s: write_avail %u exceeds buffer_size %u after the write",
         d->ident, (unsigned)avail_full, (unsigned)buffer);
   CHECK(avail_full <= avail_empty,
         "%s: room grew on a write (%u -> %u)",
         d->ident, (unsigned)avail_empty, (unsigned)avail_full);
   if (c->models_fill)
      CHECK(avail_full < buffer / 2,
            "%s: a buffer's worth offered left more than half the room (%u of %u)",
            d->ident, (unsigned)avail_full, (unsigned)buffer);

   /* 4. The device plays it out and the room comes back. Polled rather
    *    than slept for: a null backend drains in real time but through
    *    its own device buffering, so a fixed wait of twice the setting
    *    was marginal, and under TSan it was missed. The deadline is
    *    generous; a driver that drains at all returns well inside it. */
   d->start(ctx, false);
   {
      unsigned waited_ms = 0;
      avail_later = d->write_avail(ctx);
      while (   c->models_fill && avail_later <= avail_full
             && waited_ms < latency_ms * 20)
      {
         usleep(5000);
         waited_ms  += 5;
         avail_later = d->write_avail(ctx);
      }
      printf("   write_avail after the device played for %u ms: %u bytes\n",
            waited_ms, (unsigned)avail_later);
   }
   CHECK(avail_later <= buffer,
         "%s: write_avail %u exceeds buffer_size %u after playing",
         d->ident, (unsigned)avail_later, (unsigned)buffer);
   if (c->models_fill)
      CHECK(avail_later > avail_full,
            "%s: no room came back while the device played (%u -> %u)",
            d->ident, (unsigned)avail_full, (unsigned)avail_later);

   d->stop(ctx);
   free(silence);
   d->free(ctx);
}

int main(void)
{
   static const case_t cases[] = {
      { &audio_alsa,   "null", 0.05, false },
      { &audio_openal, NULL,   0.10, true  },
      { &audio_sdl,    NULL,   0.05, true  },
   };
   unsigned i;

   setenv("SDL_AUDIODRIVER", "dummy", 1);
   setenv("ALSOFT_DRIVERS", "null", 1);
   setenv("ALSOFT_LOGLEVEL", "0", 1);

   for (i = 0; i < ARRAY_SIZE(cases); i++)
   {
      exercise(&cases[i], 64);
      exercise(&cases[i], 32);
   }

   if (failures)
   {
      printf("%u failure(s)\n", failures);
      return 1;
   }
   printf("latency contract: every driver reports the setting in bytes, room within it, room that returns\n");
   return 0;
}
