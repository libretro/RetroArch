/* The three console drivers' ring, driven the way the frontend drives
 * it: one writer calling write() while the driver's own worker hands
 * windows to the device.
 *
 * Two orderings the ring depends on are what this pins. A period is
 * the writer's only once the device has taken it, so read_pos may not
 * be published before the output call; and a full ring and an empty
 * one must not both read as write_pos == read_pos, or the writer laps
 * over the window being played. Either one shows up here as a window
 * whose frames do not continue the last, and under TSan as a race on
 * the ring itself. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <rthreads/rthreads.h>
#include <retro_atomic.h>

#include "../../../audio/audio_driver.h"
#include "device_mock.h"

#define CHECK(cond, what) \
   do { if (!(cond)) { printf("FAIL  %s: %s\n", name, what); return 1; } } while (0)

#define WRITE_FRAMES 384u
#define RUN_PERIODS  600u

static const audio_driver_t *drv;
static void                 *handle;
static retro_atomic_int_t    writer_go;
static uint32_t              writer_seq;
static unsigned long         writer_frames;
static unsigned long         writer_refused;

/* Frames carrying a running count, so the device can tell whether the
 * window it was handed continues the last one. */
static void writer_thread(void *unused)
{
   uint32_t chunk[WRITE_FRAMES];
   (void)unused;

   while (retro_atomic_load_acquire_int(&writer_go))
   {
      unsigned i;
      ssize_t  wrote;
      for (i = 0; i < WRITE_FRAMES; i++)
         chunk[i] = writer_seq + i;

      wrote = drv->write(handle, chunk, WRITE_FRAMES * sizeof(uint32_t));
      if (wrote < 0)
         break;
      if (wrote == 0)
      {
         /* The driver's wait is lap-bounded and hands the pass back;
          * the same frames go again rather than being skipped. */
         writer_refused++;
         continue;
      }
      writer_seq    += WRITE_FRAMES;
      writer_frames += WRITE_FRAMES;
   }
}

/* buffer_size() is what the frontend reads the driver's latency off,
 * and half of it is the rate control's setpoint. It has to track the
 * setting rather than a constant, within the period the sizing
 * rounds to and the floor it will not go under. */
static int check_latency(const audio_driver_t *d, const char *name)
{
   static const unsigned ms[] = { 8, 16, 32, 64, 128, 256 };
   unsigned i;
   size_t   last = 0;

   for (i = 0; i < sizeof(ms) / sizeof(ms[0]); i++)
   {
      unsigned new_rate = 0;
      void    *h        = d->init(NULL, 48000, ms[i], &new_rate);
      size_t   frames, got_ms;
      CHECK(h != NULL, "init returned NULL while sizing");
      frames = d->buffer_size(h) / sizeof(uint32_t);
      got_ms = frames * 1000u / 48000u;
      /* Never under the asked-for latency, and never wildly over:
       * the floor is four periods, the rounding one period. */
      CHECK(got_ms + 1 >= ms[i] || frames <= 512u * 5u,
            "buffer_size came in under the latency asked for");
      CHECK(frames <= (size_t)ms[i] * 48u + 512u * 5u,
            "buffer_size overshot the latency asked for");
      CHECK(frames >= last, "buffer_size did not grow with the setting");
      last = frames;
      d->free(h);
   }
   printf("ok    %-5s buffer_size tracks the latency setting\n", name);
   return 0;
}

static int exercise(const audio_driver_t *d, const char *name)
{
   unsigned   new_rate = 0;
   sthread_t *w;
   size_t     bufsz, avail;
   unsigned   spins;

   drv            = d;
   writer_seq     = 0;
   writer_frames  = 0;
   writer_refused = 0;
   mock_device_reset();

   handle = d->init(NULL, 48000, 64, &new_rate);
   CHECK(handle != NULL, "init returned NULL");
   CHECK(d->alive(handle), "not alive after init");

   bufsz = d->buffer_size(handle);
   avail = d->write_avail(handle);
   CHECK(bufsz > 0, "buffer_size is zero");
   CHECK(avail <= bufsz, "write_avail exceeds buffer_size");

   retro_atomic_int_init(&writer_go, 1);
   w = sthread_create(writer_thread, NULL);
   CHECK(w != NULL, "could not start the writer");

   for (spins = 0; spins < 20000u && MOCK_READ(mock_periods) < RUN_PERIODS; spins++)
      usleep(1000);

   retro_atomic_store_release_int(&writer_go, 0);
   sthread_join(w);

   CHECK(MOCK_READ(mock_periods) >= RUN_PERIODS, "the device never got its periods");
   CHECK(writer_frames > 0, "the writer never placed a frame");
   CHECK(d->write_avail(handle) <= d->buffer_size(handle),
         "write_avail exceeds buffer_size while running");

   /* The ring never handed the device a window that did not continue
    * the last: no lap, and no period freed before it was taken. */
   CHECK(MOCK_READ(mock_breaks) == 0, "the device was handed a discontinuous window");

   if (d->frames_consumed)
      CHECK(d->frames_consumed(handle) >= MOCK_READ(mock_periods) * 512u
            - 512u, "frames_consumed fell behind the device");
   if (d->underruns)
      CHECK(d->underruns(handle) >= MOCK_READ(mock_silent)
            || MOCK_READ(mock_silent) == 0, "underruns under-counted the silence");

   printf("ok    %-5s %lu periods (%lu silent), %lu frames written%s\n",
         name, (unsigned long)MOCK_READ(mock_periods),
         (unsigned long)MOCK_READ(mock_silent), writer_frames,
         writer_refused ? ", writer held off" : "");

   d->free(handle);
   handle = NULL;
   return 0;
}

int main(void)
{
   int bad = 0;
   printf("psp_ring: the console drivers' SPSC ring against a device\n");
   bad |= check_latency(&audio_psp,  "psp");
   bad |= check_latency(&audio_psp2, "vita");
   bad |= check_latency(&audio_ps4,  "ps4");
   bad |= exercise(&audio_psp,  "psp");
   bad |= exercise(&audio_psp2, "vita");
   bad |= exercise(&audio_ps4,  "ps4");
   if (bad)
      printf("psp_ring: FAILED\n");
   else
      printf("psp_ring: ok\n");
   return bad;
}
