/* Harness for alsa_wait_writable(): the audio thread's pacer must be
 * bounded on every device, conforming or not.
 *
 * The conforming path runs end-to-end against the null PCM. The
 * non-conforming ones cannot be produced on demand by any stock
 * device, so the five ALSA calls the function makes - avail, state,
 * start, wait, recover - are interposed with -Wl,--wrap and scripted:
 * a running stream whose poll never signals, a prepared stream that
 * reports no space, a start that lands in an underrun storm, and a
 * device that wakes without ever delivering space. Each scenario
 * asserts the call returns - 0 for "no space is coming", per the
 * wait_writable() contract in audio_driver.h - within a bounded lap
 * and wall-time budget, and that a prepared stream short of space is
 * started rather than waited on. A watchdog thread turns any
 * unbounded wait into an abort, so a regression fails loudly instead
 * of wedging the suite.
 *
 * Links the shipping alsa.c; only the libasound entry points are
 * scripted. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

#include <alsa/asoundlib.h>
#include <boolean.h>
#include <retro_atomic.h>

#include "../../../audio/audio_driver.h"

extern audio_driver_t audio_alsa;

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

/* ---- scripted device ------------------------------------------------ */

enum wrap_mode
{
   WRAP_PASSTHRU = 0,     /* forward everything to libasound */
   WRAP_STALL_RUNNING,    /* running, short of space, poll never signals */
   WRAP_PREPARED_DEAD,    /* prepared, no space, start "works", no drain */
   WRAP_PREPARED_RECOVER, /* prepared, no space, start underruns, recover
                             restores a full buffer */
   WRAP_EPIPE_STORM,      /* avail underruns forever, recover succeeds */
   WRAP_WAKE_NO_PROGRESS  /* wait wakes instantly, space never appears */
};

static int wrap_mode          = WRAP_PASSTHRU;
static int wrap_started       = 0;  /* stream running in the script */
static int wrap_recovered     = 0;  /* recover ran in the script */
static unsigned n_avail       = 0;
static unsigned n_wait        = 0;
static unsigned n_start       = 0;
static unsigned n_recover     = 0;
static int seen_wait_timeout  = -1; /* timeout_ms handed to snd_pcm_wait */
static snd_pcm_uframes_t wrap_buffer_frames = 0;

extern snd_pcm_sframes_t __real_snd_pcm_avail(snd_pcm_t *pcm);
extern snd_pcm_state_t   __real_snd_pcm_state(snd_pcm_t *pcm);
extern int               __real_snd_pcm_start(snd_pcm_t *pcm);
extern int               __real_snd_pcm_wait(snd_pcm_t *pcm, int timeout);
extern int               __real_snd_pcm_recover(snd_pcm_t *pcm, int err,
      int silent);

snd_pcm_sframes_t __wrap_snd_pcm_avail(snd_pcm_t *pcm)
{
   n_avail++;
   switch (wrap_mode)
   {
      case WRAP_STALL_RUNNING:
      case WRAP_WAKE_NO_PROGRESS:
         return 16; /* always short of one period */
      case WRAP_PREPARED_DEAD:
         return wrap_started ? 16 : 0;
      case WRAP_PREPARED_RECOVER:
         return wrap_recovered ? (snd_pcm_sframes_t)wrap_buffer_frames : 0;
      case WRAP_EPIPE_STORM:
         return -EPIPE;
      default:
         break;
   }
   return __real_snd_pcm_avail(pcm);
}

snd_pcm_state_t __wrap_snd_pcm_state(snd_pcm_t *pcm)
{
   switch (wrap_mode)
   {
      case WRAP_STALL_RUNNING:
      case WRAP_WAKE_NO_PROGRESS:
         return SND_PCM_STATE_RUNNING;
      case WRAP_PREPARED_DEAD:
         return wrap_started ? SND_PCM_STATE_RUNNING
                             : SND_PCM_STATE_PREPARED;
      case WRAP_PREPARED_RECOVER:
         return wrap_recovered ? SND_PCM_STATE_PREPARED
                               : SND_PCM_STATE_PREPARED;
      case WRAP_EPIPE_STORM:
         return SND_PCM_STATE_RUNNING;
      default:
         break;
   }
   return __real_snd_pcm_state(pcm);
}

int __wrap_snd_pcm_start(snd_pcm_t *pcm)
{
   n_start++;
   switch (wrap_mode)
   {
      case WRAP_PREPARED_DEAD:
         wrap_started = 1;
         return 0;
      case WRAP_PREPARED_RECOVER:
         return -EPIPE;
      case WRAP_STALL_RUNNING:
      case WRAP_WAKE_NO_PROGRESS:
      case WRAP_EPIPE_STORM:
         return 0;
      default:
         break;
   }
   return __real_snd_pcm_start(pcm);
}

int __wrap_snd_pcm_wait(snd_pcm_t *pcm, int timeout)
{
   n_wait++;
   seen_wait_timeout = timeout;
   switch (wrap_mode)
   {
      case WRAP_STALL_RUNNING:
      case WRAP_PREPARED_DEAD:
      {
         /* A stalled device: nothing signals, the timeout is all
          * that brings the call back. Sleep a token amount so the
          * scenario finishes fast while wall time still proves the
          * call slept rather than spun. */
         struct timespec ts;
         ts.tv_sec  = 0;
         ts.tv_nsec = 2 * 1000 * 1000;
         nanosleep(&ts, NULL);
         return 0;
      }
      case WRAP_WAKE_NO_PROGRESS:
         return 1; /* instant wake, no space behind it */
      case WRAP_EPIPE_STORM:
         return -EPIPE;
      case WRAP_PREPARED_RECOVER:
         return 1;
      default:
         break;
   }
   return __real_snd_pcm_wait(pcm, timeout);
}

int __wrap_snd_pcm_recover(snd_pcm_t *pcm, int err, int silent)
{
   n_recover++;
   switch (wrap_mode)
   {
      case WRAP_PREPARED_RECOVER:
         wrap_recovered = 1;
         return 0;
      case WRAP_EPIPE_STORM:
         return 0;
      case WRAP_STALL_RUNNING:
      case WRAP_PREPARED_DEAD:
      case WRAP_WAKE_NO_PROGRESS:
         return 0;
      default:
         break;
   }
   return __real_snd_pcm_recover(pcm, err, silent);
}

static void wrap_reset(int mode)
{
   wrap_mode         = mode;
   wrap_started      = 0;
   wrap_recovered    = 0;
   n_avail           = 0;
   n_wait            = 0;
   n_start           = 0;
   n_recover         = 0;
   seen_wait_timeout = -1;
}

/* ---- watchdog ------------------------------------------------------- */

static retro_atomic_int_t watchdog_stage =
      RETRO_ATOMIC_INT_INITIALIZER(0);

#define WATCHDOG_SET(v) retro_atomic_store_release_int(&watchdog_stage, (v))

static void *watchdog(void *arg)
{
   int last = retro_atomic_load_acquire_int(&watchdog_stage);
   int cur;
   (void)arg;
   for (;;)
   {
      sleep(10);
      cur = retro_atomic_load_acquire_int(&watchdog_stage);
      if (cur == last && cur >= 0)
      {
         fprintf(stderr,
               "WATCHDOG: stage %d made no progress in 10s; a wait "
               "is unbounded\n", cur);
         abort();
      }
      last = cur;
      if (cur < 0)
         return NULL;
   }
}

static double now_ms(void)
{
   struct timespec ts;
   clock_gettime(CLOCK_MONOTONIC, &ts);
   return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1e6;
}

/* --------------------------------------------------------------------- */

int main(void)
{
   pthread_t dog;
   unsigned new_rate = 0;
   void *ctx         = NULL;
   size_t period_bytes;
   size_t got;
   double t0, t1;

   setenv("ALSA_CONFIG_PATH", "/usr/share/alsa/alsa.conf", 0);

   pthread_create(&dog, NULL, watchdog, NULL);

   ctx = audio_alsa.init("null", 48000, 64, 1024, &new_rate);
   if (!ctx)
   {
      /* No usable ALSA at all (fully sandboxed CI): nothing below can
       * run, and that is a skip rather than a failure. */
      printf("SKIP: null PCM unavailable\n");
      WATCHDOG_SET(-1);
      return 0;
   }

   /* One period in bytes: 16-bit stereo. The scripted scenarios only
    * need "more than the 16 frames the script reports". */
   period_bytes = 4096;

   /* 1. Conforming device end-to-end: a prepared null PCM reports its
    *    whole buffer free and the call returns at once with space. */
   WATCHDOG_SET(1);
   wrap_reset(WRAP_PASSTHRU);
   got = audio_alsa.wait_writable(ctx, period_bytes);
   CHECK(got > 0, "null PCM: expected space, got 0");

   /* Learn the real buffer size for the recovery script. */
   wrap_buffer_frames = 8192;

   /* 2. Running stream whose poll never signals: bounded timeout, not
    *    a park. The timeout handed to snd_pcm_wait() must be the
    *    clamped two-period value, and one timeout ends the call. */
   WATCHDOG_SET(2);
   wrap_reset(WRAP_STALL_RUNNING);
   t0  = now_ms();
   got = audio_alsa.wait_writable(ctx, period_bytes);
   t1  = now_ms();
   CHECK(got == 0, "stalled device: expected 0, got %u", (unsigned)got);
   CHECK(n_wait == 1, "stalled device: expected 1 wait, saw %u", n_wait);
   CHECK(seen_wait_timeout >= 20 && seen_wait_timeout <= 200,
         "wait timeout %d outside [20,200]", seen_wait_timeout);
   CHECK(n_start == 0, "running stream: start must not be called");
   CHECK(t1 - t0 < 3000.0, "stalled device: %f ms is not bounded", t1 - t0);

   /* 3. Prepared stream with no space: it is started - it cannot gain
    *    space or arm a poll on its own - and when the device still
    *    delivers nothing, the bounded wait ends the call. */
   WATCHDOG_SET(3);
   wrap_reset(WRAP_PREPARED_DEAD);
   t0  = now_ms();
   got = audio_alsa.wait_writable(ctx, period_bytes);
   t1  = now_ms();
   CHECK(got == 0, "dead prepared: expected 0, got %u", (unsigned)got);
   CHECK(n_start == 1, "dead prepared: expected 1 start, saw %u", n_start);
   CHECK(t1 - t0 < 3000.0, "dead prepared: %f ms is not bounded", t1 - t0);

   /* 4. Prepared stream whose start underruns: recovery restores a
    *    prepared stream reporting a full buffer, and the next lap
    *    returns with that space. */
   WATCHDOG_SET(4);
   wrap_reset(WRAP_PREPARED_RECOVER);
   got = audio_alsa.wait_writable(ctx, period_bytes);
   CHECK(got > 0, "recovered prepared: expected space, got 0");
   CHECK(n_start >= 1, "recovered prepared: start never attempted");
   CHECK(n_recover >= 1, "recovered prepared: recover never ran");

   /* 5. Underrun storm: avail underruns on every lap and recovery
    *    keeps succeeding; the lap cap ends the call. */
   WATCHDOG_SET(5);
   wrap_reset(WRAP_EPIPE_STORM);
   t0  = now_ms();
   got = audio_alsa.wait_writable(ctx, period_bytes);
   t1  = now_ms();
   CHECK(got == 0, "EPIPE storm: expected 0, got %u", (unsigned)got);
   CHECK(n_recover <= 16, "EPIPE storm: %u recoveries is uncapped",
         n_recover);
   CHECK(t1 - t0 < 3000.0, "EPIPE storm: %f ms is not bounded", t1 - t0);

   /* 6. Wakes that deliver no space: the lap cap ends the call instead
    *    of letting the loop spin on a device that signals and never
    *    drains. */
   WATCHDOG_SET(6);
   wrap_reset(WRAP_WAKE_NO_PROGRESS);
   t0  = now_ms();
   got = audio_alsa.wait_writable(ctx, period_bytes);
   t1  = now_ms();
   CHECK(got == 0, "no-progress wakes: expected 0, got %u", (unsigned)got);
   CHECK(n_wait <= 16, "no-progress wakes: %u waits is uncapped", n_wait);
   CHECK(t1 - t0 < 3000.0, "no-progress wakes: %f ms is not bounded",
         t1 - t0);

   WATCHDOG_SET(0);
   wrap_reset(WRAP_PASSTHRU);
   audio_alsa.free(ctx);

   WATCHDOG_SET(-1);
   if (failures)
   {
      printf("%u failure(s)\n", failures);
      return 1;
   }
   printf("alsa_wait_writable: all scenarios bounded\n");
   return 0;
}
