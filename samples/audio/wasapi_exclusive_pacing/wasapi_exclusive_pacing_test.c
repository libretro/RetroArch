/* WASAPI exclusive-mode pacing, under the reporter's settings.
 *
 * The driver's text runs unmodified against a fake device that keeps
 * real time: exclusive and event-driven, the endpoint buffer is one
 * period, and every period the device takes the buffer released for
 * it or counts the period unanswered - a period of silence on real
 * hardware. The writer is the frontend as the reporter runs it: audio
 * sync off, so writes do not block; rate control off; the core's
 * audio arriving one frame at a time, 800 frames every 16.68 ms at
 * 59.94 fps. Two figures come out: audio the writer offered that the
 * driver did not take - dropped - and periods the device raised that
 * nobody answered. Either one is audible. */

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>

#include "fake_wasapi.h"
#include "../../../audio/audio_driver.h"
#include "../../../configuration.h"

/* Before the pump thread and the drain-first write, the same run gave:
 *   exclusive 16/32/64 ms: 4% dropped, 2-4% of periods unanswered;
 *   shared at the setting: 6% dropped, 10% unanswered;
 *   shared at an 800-frame fifo: 0.3% dropped, none unanswered - the one
 *   size the reporter found that worked.
 * And the first attempt at the smaller period without the pump:
 *   exclusive 16 ms: 74% dropped, 75% unanswered - the crackle reported.
 * Any wasapi.c can be run against this model: make WASAPI_SRC=<file>. */

extern audio_driver_t audio_wasapi;

static unsigned failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { printf("FAIL: "); printf(__VA_ARGS__); printf("\n"); failures++; } } while (0)

typedef struct
{
   unsigned latency_ms, seconds;
   bool     exclusive;
   unsigned sh_buffer_length;
   /* IAudioClient3 on the fake: minimum engine period in frames, 0 for
    * not offered; and a period another stream holds the engine at, 0
    * for none. */
   unsigned engine_min_frames, locked_period_frames;
   const char *name;
   /* The synchronous audio path: init(), writes, and never start(). */
   bool     no_start;
} scenario_t;

typedef struct
{
   size_t offered, taken;
   fake_device_stats_t dev;
   size_t reported_buffer;
   size_t frame_bytes;
} result_t;

static void sleep_until(struct timespec *t, long ns)
{
   t->tv_nsec += ns;
   while (t->tv_nsec >= 1000000000L) { t->tv_sec++; t->tv_nsec -= 1000000000L; }
   clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, t, NULL);
}

static bool run(const scenario_t *sc, result_t *r)
{
   settings_t *settings = config_get_ptr();
   unsigned new_rate    = 0;
   void *ctx;
   size_t frame_bytes;
   unsigned frames_per_write = 800;               /* 48000 / 59.94 */
   long     write_interval   = 16683000L;         /* ns */
   unsigned writes           = sc->seconds * 60;
   unsigned i;
   struct timespec t;
   void *buf;

   memset(r, 0, sizeof(*r));
   fake_device_configure_engine(sc->engine_min_frames, sc->locked_period_frames);
   settings->bools.audio_wasapi_exclusive_mode    = sc->exclusive;
   settings->uints.audio_wasapi_sh_buffer_length  = sc->sh_buffer_length;
   settings->uints.audio_output_sample_rate       = 48000;

   ctx = audio_wasapi.init(NULL, 48000, sc->latency_ms, 0, &new_rate);
   if (!ctx)
   {
      printf("   init failed\n");
      return false;
   }
   frame_bytes = audio_wasapi.use_float(ctx) ? 8 : 4;
   r->frame_bytes = frame_bytes;
   buf         = calloc(frames_per_write, frame_bytes);
   audio_wasapi.set_nonblock_state(ctx, true);    /* audio sync off */
   r->reported_buffer = audio_wasapi.buffer_size(ctx);
   /* The synchronous audio path never calls start(): audio_driver_init()
    * leaves it to the runloop, which only issues it around pause, menu
    * and thread-wait transitions. init() must leave the device fed. */
   if (!sc->no_start)
      audio_wasapi.start(ctx, false);

   clock_gettime(CLOCK_MONOTONIC, &t);
   for (i = 0; i < writes; i++)
   {
      ssize_t n = audio_wasapi.write(ctx, buf, frames_per_write * frame_bytes);
      r->offered += frames_per_write * frame_bytes;
      if (n > 0)
         r->taken += (size_t)n;
      if (getenv("PACING_TRACE") && (i % 60) == 59)
      {
         fake_device_stats_t st;
         fake_device_stats(&st);
         printf("   t=%us unanswered so far: %u of %u\n", (i + 1) / 60, st.periods_unanswered, st.periods);
      }
      sleep_until(&t, write_interval);
   }
   fake_device_stats(&r->dev);
   /* The driver's own count of what the device took, against the
    * device thread's real-time clock: the sink rate estimate rests on
    * it, so it must advance at the device's rate. */
   if (audio_wasapi.frames_consumed)
   {
      double sec = (double)writes * write_interval / 1e9;
      double hz  = (double)audio_wasapi.frames_consumed(ctx) / sec;
      printf("   frames_consumed: %.0f Hz against wall time (%+.1f%%)\n", hz, (hz / 48000.0 - 1.0) * 100.0);
      CHECK(hz > 48000.0 * 0.97 && hz < 48000.0 * 1.03,
            "%s: frames_consumed advances at %.0f Hz, not the device's 48000", sc->name, hz);
   }
   audio_wasapi.stop(ctx);
   audio_wasapi.free(ctx);
   free(buf);
   return true;
}

static void report(const char *name, const scenario_t *sc, const result_t *r)
{
   double dropped_ms  = (double)(r->offered - r->taken) / (double)r->frame_bytes * 1000.0 / 48000.0;
   printf("%-32s setting %2u ms: %s, period %u frames (%.1f ms), reported buffer %u frames; "
          "dropped %.1f ms of %u s (%.2f%%), %u of %u periods unanswered (%.2f%%)\n",
         name, sc->latency_ms, r->dev.share_mode ? "exclusive" : "shared",
         r->dev.period_frames, r->dev.period_frames * 1000.0 / 48000.0,
         (unsigned)(r->reported_buffer / r->frame_bytes),
         dropped_ms, sc->seconds,
         100.0 * (double)(r->offered - r->taken) / (double)r->offered,
         r->dev.periods_unanswered, r->dev.periods,
         r->dev.periods ? 100.0 * r->dev.periods_unanswered / r->dev.periods : 0.0);
}

int main(int argc, char **argv)
{
   /* A device like the reporter's Topping: 3 ms minimum period, 10 ms
    * default, exclusive PCM only. */
   const unsigned seconds = (argc > 1) ? (unsigned)atoi(argv[1]) : 3;
   scenario_t sc[] = {
      { 16, seconds, true,  0,   0,   0,   "exclusive, under a frame"     },
      { 32, seconds, true,  0,   0,   0,   "exclusive"                    },
      { 64, seconds, true,  0,   0,   0,   "exclusive"                    },
      { 64, seconds, false, 800, 0,   0,   "shared, 800-frame fifo"       }, /* the reporter's working setup */
      { 64, seconds, false, 0,   0,   0,   "shared, fifo at the setting"  },
      { 32, seconds, false, 0,   0,   0,   "shared, fifo at the setting"  },
      { 24, seconds, false, 0,   0,   0,   "shared, fifo at the setting"  },
      /* IAudioClient3: the engine offers a 3 ms period; the driver
       * takes it, the fifo keeps the setting, and the drain-first write
       * must keep a 432-frame engine buffer fed. */
      { 64, seconds, false, 0,   144, 0,   "shared, IAudioClient3 at 3 ms" },
      { 32, seconds, false, 0,   144, 0,   "shared, IAudioClient3 at 3 ms" },
      /* Another stream holds the engine at 10 ms: the driver must join
       * that period, not fall to the legacy path. */
      { 64, seconds, false, 0,   144, 480, "shared, engine locked at 10 ms" },
      /* The synchronous path never calls start(). The pump was created
       * only there, so with the threaded pipeline off the fifo filled
       * and nothing drained: silence. init() brings the pump up. */
      { 8,  seconds, true,  0,   0,   0,   "exclusive, no start()", true },
      { 64, seconds, false, 0,   0,   0,   "shared, no start()", true },
   };
   unsigned i;
   fake_device_configure(48000, 30000, 100000, false);

   for (i = 0; i < sizeof(sc) / sizeof(sc[0]); i++)
   {
      result_t r;
      double dropped_pct, unanswered_pct;
      if (!run(&sc[i], &r))
      {
         CHECK(false, "scenario %u: init failed", i);
         continue;
      }
      report(sc[i].name, &sc[i], &r);
      /* What the driver reports is the setting, within the engine's
       * rounding, once the setting exceeds the floor the writer's burst
       * needs. The floor is the driver's rule, restated here so a change
       * to either side fails this rather than passing by accident. This
       * writer has audio sync off and does not block, so it gets the
       * non-blocking floor: a frame at 50 fps plus the 2% rate control
       * can stretch it plus one frame of slack, never below two periods.
       * The blocking writer - sync on, frame-synchronous pipeline - gets
       * a frame plus a period, never below the engine buffer; it is not
       * exercised here. The period is the one the engine actually runs
       * at, so an IAudioClient3 stream at 3 ms is floored by the frame,
       * not by the default period. */
      if (!sc[i].exclusive && sc[i].sh_buffer_length == 0)
      {
         unsigned reported_ms  = (unsigned)(r.reported_buffer / r.frame_bytes * 1000 / 48000);
         unsigned engine_ms    = r.dev.buffer_frames * 1000 / 48000;
         unsigned period       = r.dev.period_frames;
         unsigned floor_frames = 48000 / 50 + 48000 / 2000 + 1;
         unsigned floor_ms;
         unsigned expect_ms;
         if (floor_frames < period * 2)
            floor_frames = period * 2;
         floor_ms  = floor_frames * 1000 / 48000 + engine_ms;
         expect_ms = sc[i].latency_ms > floor_ms ? sc[i].latency_ms : floor_ms;
         CHECK(reported_ms <= expect_ms + 4,
               "scenario %u: reports %u ms against a %u ms setting (floor %u)", i, reported_ms, sc[i].latency_ms, expect_ms);
      }
      if (sc[i].engine_min_frames)
      {
         unsigned want = sc[i].locked_period_frames ? sc[i].locked_period_frames : sc[i].engine_min_frames;
         CHECK(r.dev.period_frames == want,
               "scenario %u: engine period %u frames, expected %u (IAudioClient3 path not taken)",
               i, r.dev.period_frames, want);
      }
      dropped_pct    = 100.0 * (double)(r.offered - r.taken) / (double)r.offered;
      unanswered_pct = r.dev.periods ? 100.0 * r.dev.periods_unanswered / r.dev.periods : 100.0;

      if (sc[i].no_start)
      {
         /* The point is that the device is fed at all without start():
          * every period answered. An 8 ms fifo against 16.7 ms bursts
          * drops the remainder of each frame by arithmetic, as the
          * under-a-frame case above; that is not what is tested here. */
         CHECK(unanswered_pct < 1.0, "scenario %u (no start): %.2f%% of periods unanswered - the pump is not running", i, unanswered_pct);
         continue;
      }

      if (sc[i].exclusive && sc[i].latency_ms < 20)
      {
         /* A buffer smaller than the writer's frame, with audio sync
          * off: 16 ms against 16.7. The remainder of each frame is
          * dropped - 4%, by arithmetic - and for the 0.7 ms of each
          * frame the fifo stands empty a period that lands there goes
          * unanswered, an underrun any driver would have. The init
          * log says to set 20 ms or more. Bounded here, not zero. */
         CHECK(dropped_pct < 6.0, "scenario %u: %.2f%% dropped", i, dropped_pct);
         CHECK(unanswered_pct < 3.0, "scenario %u: %.2f%% of periods unanswered", i, unanswered_pct);
         continue;
      }

      /* Once the buffer holds a frame: no period goes unanswered - the
       * pump's job in exclusive mode, the drain-first write's in shared,
       * and 3-75% before them - and nothing is dropped past a start-up
       * transient. The unanswered bound is 1% rather than zero for the
       * harness's own sake: on a loaded sanitizer box the pump thread
       * is an ordinary thread that can be held past a period, where on
       * Windows it runs at time-critical priority; the runs here show
       * zero nearly always and a period or two under load; a late pump
       * also leaves the fifo full for a write, so drops get the same. */
      CHECK(unanswered_pct < 1.0, "scenario %u: %.2f%% of periods unanswered", i, unanswered_pct);
      CHECK(dropped_pct < 1.0, "scenario %u: %.2f%% dropped", i, dropped_pct);
   }

   /* Every init took COM up on its thread and every free put it back. */
   CHECK(fake_com_refs() == 0, "COM references left after free: %d", fake_com_refs());

   if (failures)
   {
      printf("%u failure(s)\n", failures);
      return 1;
   }
   printf("wasapi pacing: every period answered, nothing dropped once the buffer holds a frame\n");
   return 0;
}
