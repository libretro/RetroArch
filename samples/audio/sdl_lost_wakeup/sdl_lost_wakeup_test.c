/* The SDL audio driver's producer-side waits, against SDL's own dummy
 * device.
 *
 * sdl2_audio.c carries its audio in a lock-free retro_spsc, so sdl->lock
 * guarded no data - only the waits a full ring puts the producer into.
 * The playback callback announced its pull without holding that lock,
 * and the driver's own comment said what that cost:
 *
 *   a signal raised between the avail test and this wait reaches no
 *   waiter. Normally the next callback covers that.
 *
 * This was built to find out how often "normally" is not, and what it
 * costs when it is not.
 *
 * The answer, on this fixture, is: not often enough to measure, and one
 * device period when it happens. That is worth stating plainly because
 * the obvious reading of the code says otherwise. The wait is bounded
 * at SDL_AUDIO_STALL_TIMEOUT_US, a quarter of a second, and a timeout
 * is acted on - sdl2_audio_write() breaks out and reports a short write,
 * sdl2_audio_wait_writable() returns 0 - so a lost signal looks like it
 * should cost a quarter second and some dropped audio. It does not,
 * because a device that is still running calls back again a period
 * later and that callback ends the park. The bound is only reachable
 * when the device has stopped calling back altogether, which is the
 * stall the bound exists for and not this window at all.
 *
 * So the columns below came out the same before and after the
 * eventcount conversion, across three runs each, and the conversion is
 * justified by closing the window rather than by anything here moving.
 * The harness is kept because that is a fact about the handshake worth
 * being able to re-establish, and because it is what would catch a
 * later change that did make the bound reachable.
 *
 *   waits          times the producer found the ring too full for the
 *                  block and parked.
 *   timed out      waits that ran the full bound. Each one is a quarter
 *                  second followed by dropped audio. Zero here, at
 *                  every setting, before and after.
 *   room at        of those, how many had room by the time the wait
 *   timeout        gave up - the signature of a wake that was raised
 *                  and reached nobody.
 *   dropped        frames sdl2_audio_write() refused to enqueue.
 *   wait us        how long a park lasted, p50/p99/max. A device period
 *                  is the honest figure.
 *
 * The ring is deliberately sized tight against the block the producer
 * offers, so the full-ring path is taken on nearly every frame at the
 * low latency settings rather than occasionally.
 *
 * Builds audio/drivers/sdl2_audio.c itself and drives it through its own
 * audio_driver_t and microphone_driver_t vtables, so the handshakes
 * under test are the shipping ones. SDL_AUDIODRIVER=dummy gives a
 * device that calls back on a clock without needing a sound card, which
 * is what CI has - and it supports capture as well as playback, so the
 * microphone half is exercised here too rather than left to a reading
 * of the code.
 *
 * What the dummy device does not do is behave like any particular
 * platform's real SDL backend. It establishes that the handshake is
 * right; it says nothing about CoreAudio, PulseAudio, WASAPI or
 * PipeWire underneath SDL, and a run here is not a substitute for one
 * on hardware.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include <boolean.h>
#include <retro_atomic.h>
#include <features/features_cpu.h>

#include "../../../audio/drivers/sdl2_audio.c"

#define OUT_RATE     48000
#define CHANNELS     2
#define FPS          60.0
#define MAX_SAMPLES  65536

static retro_time_t *wait_us;
static size_t        wait_n;

static int cmp_time(const void *a, const void *b)
{
   retro_time_t x = *(const retro_time_t*)a;
   retro_time_t y = *(const retro_time_t*)b;
   return (x > y) - (x < y);
}

static void run_one(unsigned latency_ms, double seconds)
{
   const audio_driver_t *drv = &audio_sdl2;
   void        *ctx;
   unsigned     out_rate     = OUT_RATE;
   size_t       per_frame    = (size_t)(OUT_RATE / FPS);
   size_t       block        = per_frame * CHANNELS * sizeof(int16_t);
   size_t       i, frames    = (size_t)(seconds * FPS);
   size_t       waits = 0, timeouts = 0, room_at_timeout = 0, dropped = 0;
   int16_t     *buf;
   struct timespec next;
   long         step_ns = (long)(1e9 / FPS);
   double       p50 = 0.0, p99 = 0.0, worst = 0.0;

   if (!(ctx = drv->init(NULL, OUT_RATE, latency_ms, &out_rate)))
   {
      printf("  %3u ms: device would not open\n", latency_ms);
      return;
   }
   drv->start(ctx, false);
   drv->set_nonblock_state(ctx, false);

   buf = (int16_t*)calloc(per_frame * CHANNELS, sizeof(int16_t));
   for (i = 0; i < per_frame * CHANNELS; i++)
      buf[i] = (int16_t)(i & 0x7fff);
   wait_n = 0;

   clock_gettime(CLOCK_MONOTONIC, &next);
   for (i = 0; i < frames; i++)
   {
      sdl2_audio_t *sdl = (sdl2_audio_t*)ctx;
      retro_time_t t0, t1;
      ssize_t      wrote;
      size_t       before;

      next.tv_nsec += step_ns;
      next.tv_sec  += next.tv_nsec / 1000000000L;
      next.tv_nsec %= 1000000000L;
      clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, NULL);

      before = sdl2_ring_room(&sdl->speaker_ring, sdl->speaker_ring_size);
      t0     = cpu_features_get_time_usec();
      wrote  = drv->write(ctx, buf, block);
      t1     = cpu_features_get_time_usec();

      /* A write that entered the wait is one that could not place the
       * whole block from the room it found. Timing it from out here
       * rather than instrumenting the driver keeps the measurement
       * independent of which primitive is underneath. */
      if (before < block)
      {
         waits++;
         if (wait_n < MAX_SAMPLES)
            wait_us[wait_n++] = (t1 > t0) ? t1 - t0 : 0;
         /* The bound is a quarter second; anything near it is a park
          * that ran out rather than a device period. */
         if (t1 - t0 >= SDL_AUDIO_STALL_TIMEOUT_US * 3 / 4)
         {
            timeouts++;
            if (sdl2_ring_room(&sdl->speaker_ring, sdl->speaker_ring_size))
               room_at_timeout++;
         }
      }
      if (wrote >= 0 && (size_t)wrote < block)
         dropped += (block - (size_t)wrote) / (CHANNELS * sizeof(int16_t));
   }

   if (wait_n)
   {
      qsort(wait_us, wait_n, sizeof(wait_us[0]), cmp_time);
      p50   = (double)wait_us[wait_n / 2];
      p99   = (double)wait_us[(wait_n * 99) / 100];
      worst = (double)wait_us[wait_n - 1];
   }

   printf("  %3u ms  %6u %6u %6u | %8u | %7.0f %7.0f %8.0f\n",
         latency_ms, (unsigned)waits, (unsigned)timeouts,
         (unsigned)room_at_timeout, (unsigned)dropped,
         p50, p99, worst);

   free(buf);
   drv->stop(ctx);
   drv->free(ctx);
}

/* --- the capture half ------------------------------------------------ */

/* The same question on the microphone path: the capture callback
 * notifies without a lock, the core's read parks when the ring is
 * empty, and the bound is the same quarter second. The core is not
 * handed a pattern it can check here - SDL's dummy capture device
 * delivers silence - so the short-read count is what stands in for it:
 * sdl2_microphone_read() returns what it managed to capture, and
 * anything less than what was asked for is a park that ran out. */
static void run_capture(unsigned latency_ms, double seconds)
{
   const microphone_driver_t *drv = &microphone_sdl;
   void        *ctx, *mic;
   unsigned     out_rate  = OUT_RATE;
   size_t       per_frame = (size_t)(OUT_RATE / FPS);
   size_t       want      = per_frame * sizeof(int16_t);
   size_t       i, frames = (size_t)(seconds * FPS);
   size_t       parks = 0, timeouts = 0, short_reads = 0, missing = 0;
   int16_t     *buf;
   struct timespec next;
   long         step_ns = (long)(1e9 / FPS);
   double       p50 = 0.0, p99 = 0.0, worst = 0.0;

   if (!(ctx = drv->init()))
   {
      printf("  %3u ms: capture driver would not init\n", latency_ms);
      return;
   }
   if (!(mic = drv->open_mic(ctx, NULL, OUT_RATE, latency_ms, &out_rate)))
   {
      printf("  %3u ms: capture device would not open\n", latency_ms);
      drv->free(ctx);
      return;
   }
   drv->start_mic(ctx, mic);

   buf    = (int16_t*)calloc(per_frame, sizeof(int16_t));
   wait_n = 0;

   clock_gettime(CLOCK_MONOTONIC, &next);
   for (i = 0; i < frames; i++)
   {
      sdl2_microphone_handle_t *h = (sdl2_microphone_handle_t*)mic;
      retro_time_t t0, t1;
      size_t       before;
      int          got;

      next.tv_nsec += step_ns;
      next.tv_sec  += next.tv_nsec / 1000000000L;
      next.tv_nsec %= 1000000000L;
      clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, NULL);

      before = retro_spsc_read_avail(&h->ring);
      t0     = cpu_features_get_time_usec();
      got    = drv->read(ctx, mic, buf, want);
      t1     = cpu_features_get_time_usec();

      if (before < want)
      {
         parks++;
         if (wait_n < MAX_SAMPLES)
            wait_us[wait_n++] = (t1 > t0) ? t1 - t0 : 0;
         if (t1 - t0 >= SDL_AUDIO_STALL_TIMEOUT_US * 3 / 4)
            timeouts++;
      }
      if (got >= 0 && (size_t)got < want)
      {
         short_reads++;
         missing += (want - (size_t)got) / sizeof(int16_t);
      }
   }

   if (wait_n)
   {
      qsort(wait_us, wait_n, sizeof(wait_us[0]), cmp_time);
      p50   = (double)wait_us[wait_n / 2];
      p99   = (double)wait_us[(wait_n * 99) / 100];
      worst = (double)wait_us[wait_n - 1];
   }

   printf("  %3u ms  %6u %6u %6u | %8u | %7.0f %7.0f %8.0f\n",
         latency_ms, (unsigned)parks, (unsigned)timeouts,
         (unsigned)short_reads, (unsigned)missing, p50, p99, worst);

   free(buf);
   drv->stop_mic(ctx, mic);
   drv->close_mic(ctx, mic);
   drv->free(ctx);
}

int main(int argc, char **argv)
{
   double seconds = (argc > 1) ? atof(argv[1]) : 3.0;
   static const unsigned sweep[] = { 8, 16, 32, 64 };
   size_t i;

   /* Before SDL_Init anywhere: a box with no sound card would otherwise
    * fail to open a device and the run would say nothing. */
   setenv("SDL_AUDIODRIVER", "dummy", 0);

   if (!(wait_us = (retro_time_t*)malloc(MAX_SAMPLES * sizeof(retro_time_t))))
      return 1;

   printf("sdl2_audio producer waits against SDL's dummy device,"
         " %.0f s per setting, %g fps\n", seconds, FPS);
   printf("-- playback: the core writes, SDL's callback pulls --\n");
   printf("   lat     waits  t/out   room | dropped  |     p50     p99      max\n");
   for (i = 0; i < sizeof(sweep) / sizeof(sweep[0]); i++)
      run_one(sweep[i], seconds);

   printf("\n-- capture: SDL's callback pushes, the core reads --\n");
   printf("   lat     parks  t/out  short |  missing |     p50     p99      max\n");
   for (i = 0; i < sizeof(sweep) / sizeof(sweep[0]); i++)
      run_capture(sweep[i], seconds);

   free(wait_us);
   printf("sdl lost wakeup: run complete\n");
   return 0;
}
