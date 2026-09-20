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

/* A failed check releases the driver: a worker left running would
 * hand the device windows while the next one is being measured. */
#define CHECK(cond, what) \
   do { if (!(cond)) { \
      printf("FAIL  %s: %s\n", name, what); \
      writer_deliver = 0; \
      if (handle) { d->free(handle); handle = NULL; } \
      return 1; } } while (0)

#define WRITE_FRAMES   384u
#define RUN_PERIODS    600u
/* A video frame of audio at 48 kHz, for 60 Hz content and for 30 Hz,
 * and the period the mock device takes over a window. The ring's
 * floor is sized for the larger of the two. */
#define DELIVERY_60HZ   800u
#define DELIVERY_30HZ  1600u
#define MOCK_PERIOD_US 1000u
/* The ring's floor plus the period the device holds. */
#define FLOOR_FRAMES   (512u * 6u)

static const audio_driver_t *drv;
static void                 *handle;
static retro_atomic_int_t    writer_go;
static uint32_t              writer_seq;
static unsigned long         writer_frames;
static unsigned long         writer_refused;
static unsigned              writer_deliver;

/* Frames carrying a running count, so the device can tell whether the
 * window it was handed continues the last one. */
static void writer_thread(void *unused)
{
   uint32_t chunk[DELIVERY_30HZ];
   (void)unused;

   while (retro_atomic_load_acquire_int(&writer_go))
   {
      unsigned i;
      ssize_t  wrote;
      unsigned n = writer_deliver ? writer_deliver : WRITE_FRAMES;
      for (i = 0; i < n; i++)
         chunk[i] = writer_seq + i;

      wrote = drv->write(handle, chunk, n * sizeof(uint32_t));
      if (wrote < 0)
         break;
      if (wrote == 0)
      {
         /* The driver's wait is lap-bounded and hands the pass back;
          * the same frames go again rather than being skipped. */
         writer_refused++;
         continue;
      }
      writer_seq    += n;
      writer_frames += n;
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
      size_t   frames, got_ms;
      handle = d->init(NULL, 48000, ms[i], &new_rate);
      CHECK(handle != NULL, "init returned NULL while sizing");
      frames = d->buffer_size(handle) / sizeof(uint32_t);
      got_ms = frames * 1000u / 48000u;
      /* Never under the asked-for latency, and never wildly over.
       * The floor is five periods, and buffer_size counts the
       * period the device holds on top of it. */
      CHECK(got_ms + 1 >= ms[i] || frames <= FLOOR_FRAMES,
            "buffer_size came in under the latency asked for");
      CHECK(frames <= (size_t)ms[i] * 48u + FLOOR_FRAMES,
            "buffer_size overshot the latency asked for");
      CHECK(frames >= last, "buffer_size did not grow with the setting");
      last = frames;
      d->free(handle);
      handle = NULL;
   }
   printf("ok    %-5s buffer_size tracks the latency setting\n", name);
   return 0;
}

/* Starvation at the lowest setting, where the ring is at its floor
 * and there is least room to absorb the frontend's delivery size.
 *
 * A worker that holds a period back as a reserve rather than playing
 * it hands the device silence while the ring has audio in it, which
 * at this size is a quarter of every period.
 *
 * Some starvation here is geometry rather than a defect, so the bound
 * is set against it. A delivery needs RING_FREE >= its own size, so at
 * a 2560-frame floor a 1600-frame delivery waits until held <= 959;
 * draining a period at a time from the previous refill, the values the
 * writer is released at cycle 448, 512, ... 896, and the first of those
 * eight is below a period. One refill in eight therefore has to let the
 * ring dip under a period before it can be refilled at all - 24 of 600
 * periods, which is the 4% the floor was chosen against and is exact
 * rather than noisy. The bound is twice that, which still sits well
 * under the 14% a four-period floor gives and the quarter a reserving
 * worker gives. A six-period floor would not dip at all (the released
 * values start at 960); that is a latency decision, not this lane's. */
static int check_starvation(const audio_driver_t *d, const char *name,
      unsigned deliver, unsigned hz)
{
   unsigned   new_rate = 0;
   sthread_t *w;
   unsigned   spins;
   size_t     silent, periods;

   drv            = d;
   writer_seq     = 0;
   writer_frames  = 0;
   writer_refused = 0;
   mock_device_reset();

   handle = d->init(NULL, 48000, 8, &new_rate);
   CHECK(handle != NULL, "init returned NULL");

   writer_deliver = deliver;
   retro_atomic_int_init(&writer_go, 1);
   w = sthread_create(writer_thread, NULL);
   CHECK(w != NULL, "could not start the writer");

   for (spins = 0; spins < 20000u && MOCK_READ(mock_periods) < RUN_PERIODS;
         spins++)
      usleep(1000);

   retro_atomic_store_release_int(&writer_go, 0);
   sthread_join(w);
   writer_deliver = 0;

   periods = MOCK_READ(mock_periods);
   silent  = MOCK_READ(mock_silent);
   CHECK(periods >= RUN_PERIODS, "the device never got its periods");
   CHECK(MOCK_READ(mock_breaks) == 0,
         "the device was handed a discontinuous window");
   CHECK(silent * 12u <= periods,
         "the device was starved with audio in the ring");

   printf("ok    %-5s starves %lu of %lu periods at the floor, %u Hz\n",
         name, (unsigned long)silent, (unsigned long)periods, hz);

   d->free(handle);
   handle = NULL;
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

/* A driver whose worker never starts has nothing consuming its ring, so
 * init has to report that rather than hand back something that says it
 * is alive: writes would wait out their laps and return zero, and free
 * would act on a state that never held. */
static int check_worker_failure(const audio_driver_t *d, const char *name)
{
   unsigned new_rate = 0;
   void    *h;

   mock_device_reset();
   mock_thread_fail = 1;
   h                = d->init(NULL, 48000, 64, &new_rate);
   mock_thread_fail = 0;

   if (h)
   {
      printf("FAIL  %s: init succeeded with no worker (alive=%d)\n",
            name, (int)d->alive(h));
      d->free(h);
      return 1;
   }
   printf("ok    %-5s init fails when the worker cannot start\n", name);
   return 0;
}

/* Writes the frontend's output scratch can hold but the ring cannot
 * describe. The scratch is sized at max_buffer_samples * AUDIO_MAX_RATIO
 * * slowmotion_ratio, so a byte length whose frame count does not fit a
 * uint16_t is reachable; narrowed, the count wrapped small, the room
 * check waved it through and the copy took the full byte length. Every
 * write here must report no more than it was given, and never more
 * frames than the ring holds. */
static int check_write_bounds(const audio_driver_t *d, const char *name)
{
   unsigned  new_rate = 0;
   void     *h;
   size_t    bufsz;
   uint32_t *big;
   int       bad      = 0;
   unsigned  i;
   /* One past the ring, one past what a uint16_t frame count holds, and
    * a length that is not whole frames. */
   static const size_t frames[] = { 0, 1, 65535, 65536, 65537, 131072 };

   mock_device_reset();
   h = d->init(NULL, 48000, 64, &new_rate);
   if (!h)
   {
      printf("FAIL  %s: init returned NULL\n", name);
      return 1;
   }
   bufsz = d->buffer_size(h);

   /* Heap, not stack: the tree's frame budget is 4 KiB. */
   big = (uint32_t*)calloc(frames[sizeof(frames)/sizeof(frames[0]) - 1] + 1,
         sizeof(uint32_t));
   if (!big)
   {
      printf("ok    %-5s (no memory for the oversized-write check)\n", name);
      d->free(h);
      return 0;
   }

   for (i = 0; i < sizeof(frames) / sizeof(frames[0]); i++)
   {
      size_t  len = frames[i] * sizeof(uint32_t);
      ssize_t w   = d->write(h, big, len);
      if (w < 0 || (size_t)w > len)
      {
         printf("FAIL  %s: write of %u frames returned %ld for %u bytes\n",
               name, (unsigned)frames[i], (long)w, (unsigned)len);
         bad = 1;
      }
      else if ((size_t)w / sizeof(uint32_t) >= bufsz)
      {
         printf("FAIL  %s: write of %u frames took %u, past the ring\n",
               name, (unsigned)frames[i],
               (unsigned)((size_t)w / sizeof(uint32_t)));
         bad = 1;
      }
   }

   /* A length that is not whole frames must not publish a frame it did
    * not copy, so what it reports back is whole frames too. */
   {
      ssize_t w = d->write(h, big, 6);
      if (w < 0 || (w % (ssize_t)sizeof(uint32_t)) != 0)
      {
         printf("FAIL  %s: a 6-byte write reported %ld\n", name, (long)w);
         bad = 1;
      }
   }

   if (!bad)
      printf("ok    %-5s write refuses what the ring cannot describe\n", name);
   free(big);
   d->free(h);
   return bad;
}

int main(void)
{
   int bad = 0;
   printf("psp_ring: the console drivers' SPSC ring against a device\n");
   bad |= check_starvation(&audio_psp,   "psp", DELIVERY_60HZ, 60);
   bad |= check_starvation(&audio_psp,   "psp", DELIVERY_30HZ, 30);
   bad |= check_latency(&audio_psp,  "psp");
   bad |= check_starvation(&audio_psp2,  "vita", DELIVERY_60HZ, 60);
   bad |= check_starvation(&audio_psp2,  "vita", DELIVERY_30HZ, 30);
   bad |= check_latency(&audio_psp2, "vita");
   bad |= check_starvation(&audio_ps4,   "ps4", DELIVERY_60HZ, 60);
   bad |= check_starvation(&audio_ps4,   "ps4", DELIVERY_30HZ, 30);
   bad |= check_latency(&audio_ps4,  "ps4");
   bad |= exercise(&audio_psp,  "psp");
   bad |= exercise(&audio_psp2, "vita");
   bad |= exercise(&audio_ps4,  "ps4");
   bad |= check_worker_failure(&audio_psp,  "psp");
   bad |= check_worker_failure(&audio_psp2, "vita");
   bad |= check_worker_failure(&audio_ps4,  "ps4");
   bad |= check_write_bounds(&audio_psp,  "psp");
   bad |= check_write_bounds(&audio_psp2, "vita");
   bad |= check_write_bounds(&audio_ps4,  "ps4");
   if (bad)
      printf("psp_ring: FAILED\n");
   else
      printf("psp_ring: ok\n");
   return bad;
}
