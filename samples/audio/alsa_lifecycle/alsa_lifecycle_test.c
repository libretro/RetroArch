/* Harness for the ALSA driver's stop/start and its blocking write.
 *
 * stop() and start() are the frontend's notion, and alive() must
 * follow them on every device: one that can pause, one that cannot,
 * and one that says it can and refuses when asked. The null PCM is
 * opened for real; the libasound calls that decide the outcome -
 * can_pause, pause, drop, prepare - are interposed with -Wl,--wrap and
 * scripted, and each one records that it was called.
 *
 * The blocking write is exercised the same way: writei and wait are
 * scripted so the harness can count how often the driver waits, hand
 * it EAGAIN, and hand it an XRUN, and assert that the write returns.
 * A watchdog thread turns any unbounded wait into an abort. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>

#include <alsa/asoundlib.h>
#include <boolean.h>
#include <retro_atomic.h>

#include "../../../audio/audio_driver.h"

extern audio_driver_t audio_alsa;

static unsigned failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { printf("      FAIL: "); printf(__VA_ARGS__); printf("\n"); failures++; } } while (0)

/* --- the script ------------------------------------------------------- */

static int      scr_can_pause  = 1;   /* what hw_params reports */
static int      scr_pause_rc   = 0;   /* what snd_pcm_pause() returns */
static unsigned n_pause_on, n_pause_off, n_drop, n_prepare;

/* The write loop: what writei returns each call, and how many waits. */
static int      scr_writei_seq[16];   /* >0 frames accepted; <0 an error; 0 = accept all */
static unsigned scr_writei_i, scr_writei_n;
static unsigned n_writei, n_wait, n_recover;
static int      scr_wait_rc = 1;

int __real_snd_pcm_hw_params_can_pause(const snd_pcm_hw_params_t *params);
int __wrap_snd_pcm_hw_params_can_pause(const snd_pcm_hw_params_t *params)
{
   (void)params;
   return scr_can_pause;
}

int __real_snd_pcm_pause(snd_pcm_t *pcm, int enable);
int __wrap_snd_pcm_pause(snd_pcm_t *pcm, int enable)
{
   (void)pcm;
   if (enable) n_pause_on++; else n_pause_off++;
   return scr_pause_rc;
}

int __real_snd_pcm_drop(snd_pcm_t *pcm);
int __wrap_snd_pcm_drop(snd_pcm_t *pcm)
{
   n_drop++;
   return __real_snd_pcm_drop(pcm);
}

int __real_snd_pcm_prepare(snd_pcm_t *pcm);
int __wrap_snd_pcm_prepare(snd_pcm_t *pcm)
{
   n_prepare++;
   return __real_snd_pcm_prepare(pcm);
}

snd_pcm_sframes_t __real_snd_pcm_writei(snd_pcm_t *pcm, const void *buffer, snd_pcm_uframes_t size);
snd_pcm_sframes_t __wrap_snd_pcm_writei(snd_pcm_t *pcm, const void *buffer, snd_pcm_uframes_t size)
{
   int r;
   (void)pcm; (void)buffer;
   n_writei++;
   if (scr_writei_i < scr_writei_n)
   {
      r = scr_writei_seq[scr_writei_i++];
      if (r == 0)
         return (snd_pcm_sframes_t)size;
      if (r < 0)
         return r;
      return r < (int)size ? r : (snd_pcm_sframes_t)size;
   }
   return (snd_pcm_sframes_t)size;
}

int __real_snd_pcm_wait(snd_pcm_t *pcm, int timeout);
int __wrap_snd_pcm_wait(snd_pcm_t *pcm, int timeout)
{
   (void)pcm;
   n_wait++;
   CHECK(timeout >= 0, "the blocking write waited without a bound (timeout %d)", timeout);
   return scr_wait_rc;
}

int __real_snd_pcm_recover(snd_pcm_t *pcm, int err, int silent);
int __wrap_snd_pcm_recover(snd_pcm_t *pcm, int err, int silent)
{
   (void)pcm; (void)err; (void)silent;
   n_recover++;
   return 0;
}

/* --- watchdog ---------------------------------------------------------- */

static void *watchdog(void *arg)
{
   (void)arg;
   sleep(20);
   printf("      FAIL: watchdog: a call did not return within 20 s\n");
   fflush(stdout);
   abort();
   return NULL;
}

/* --- scenarios ---------------------------------------------------------- */

static void reset_counts(void)
{
   n_pause_on = n_pause_off = n_drop = n_prepare = 0;
   n_writei = n_wait = n_recover = 0;
   scr_writei_i = scr_writei_n = 0;
   scr_wait_rc = 1;
}

static void *open_null(void)
{
   unsigned new_rate = 0;
   void *ctx = audio_alsa.init("null", 48000, 64, &new_rate);
   CHECK(ctx != NULL, "the null PCM did not open");
   return ctx;
}

/* stop() then start() on each kind of device: alive() follows, and the
 * PCM sees the right call. */
static void s_stop_start(int can_pause, int pause_rc, const char *what,
      unsigned want_pause_on, unsigned want_drop, unsigned want_pause_off, unsigned want_prepare)
{
   void *ctx;
   printf("   %s\n", what);
   scr_can_pause = can_pause;
   scr_pause_rc  = pause_rc;
   reset_counts();
   ctx = open_null();
   if (!ctx)
      return;
   CHECK(audio_alsa.alive(ctx), "alive() false right after init");
   CHECK(audio_alsa.stop(ctx), "stop() reported failure");
   CHECK(!audio_alsa.alive(ctx), "alive() true after stop()");
   CHECK(n_pause_on == want_pause_on && n_drop == want_drop,
         "stop() made pause(1) x%u, drop x%u; wanted %u and %u", n_pause_on, n_drop, want_pause_on, want_drop);
   CHECK(audio_alsa.stop(ctx), "a second stop() reported failure");
   CHECK(n_pause_on == want_pause_on && n_drop == want_drop, "a second stop() touched the PCM again");
   CHECK(audio_alsa.start(ctx, false), "start() reported failure");
   CHECK(audio_alsa.alive(ctx), "alive() false after start()");
   CHECK(n_pause_off == want_pause_off && n_prepare >= want_prepare,
         "start() made pause(0) x%u, prepare x%u; wanted %u and at least %u", n_pause_off, n_prepare, want_pause_off, want_prepare);
   audio_alsa.free(ctx);
}

/* A hundred stop/start rounds on a device that drops: nothing leaks,
 * nothing wedges, and the state is right at every step. */
static void s_repeated(void)
{
   void *ctx;
   unsigned i;
   printf("   a hundred stop/start rounds on a device that drops\n");
   scr_can_pause = 0;
   reset_counts();
   ctx = open_null();
   if (!ctx)
      return;
   for (i = 0; i < 100; i++)
   {
      if (!audio_alsa.stop(ctx) || audio_alsa.alive(ctx)) { CHECK(0, "round %u: stop", i); break; }
      if (!audio_alsa.start(ctx, false) || !audio_alsa.alive(ctx)) { CHECK(0, "round %u: start", i); break; }
   }
   CHECK(n_drop == 100 && n_prepare >= 100, "drops %u, prepares %u", n_drop, n_prepare);
   audio_alsa.free(ctx);
}

/* The blocking write: writei first, wait only when the device says
 * EAGAIN, and each wait bounded. */
static void s_blocking_write(void)
{
   void *ctx;
   int16_t buf[256 * 2];
   ssize_t w;
   printf("   the blocking write\n");
   scr_can_pause = 1;
   scr_pause_rc  = 0;
   reset_counts();
   ctx = open_null();
   if (!ctx)
      return;
   audio_alsa.set_nonblock_state(ctx, false);
   memset(buf, 0, sizeof(buf));

   /* Accepted at once: no wait at all. */
   reset_counts();
   w = audio_alsa.write(ctx, buf, sizeof(buf));
   CHECK(w == (ssize_t)sizeof(buf), "a write the device took whole returned %ld", (long)w);
   CHECK(n_wait == 0, "the device took the write whole and the driver waited %u times first", n_wait);

   /* Half now, EAGAIN, then the rest: one wait, for the EAGAIN. */
   reset_counts();
   scr_writei_seq[0] = 128; scr_writei_seq[1] = -EAGAIN; scr_writei_seq[2] = 0; scr_writei_n = 3;
   w = audio_alsa.write(ctx, buf, sizeof(buf));
   CHECK(w == (ssize_t)sizeof(buf), "a write completed across an EAGAIN returned %ld", (long)w);
   CHECK(n_wait == 1, "one EAGAIN, %u waits", n_wait);
   CHECK(n_writei == 3, "three writei calls expected, %u made", n_writei);

   /* An XRUN mid-write: recovered, the write returns what got through. */
   reset_counts();
   scr_writei_seq[0] = 64; scr_writei_seq[1] = -EPIPE; scr_writei_n = 2;
   w = audio_alsa.write(ctx, buf, sizeof(buf));
   CHECK(n_recover == 1, "an XRUN and %u recoveries", n_recover);
   CHECK(w >= 0, "a recovered XRUN returned an error: %ld", (long)w);

   /* EAGAIN forever with a wait that reports nothing: the write must
    * still return - bounded - rather than hold the audio thread. */
   reset_counts();
   scr_writei_seq[0] = -EAGAIN; scr_writei_seq[1] = -EAGAIN; scr_writei_seq[2] = -EAGAIN;
   scr_writei_seq[3] = -EAGAIN; scr_writei_seq[4] = -EAGAIN; scr_writei_seq[5] = -EAGAIN;
   scr_writei_n = 6;
   scr_wait_rc  = 0;
   w = audio_alsa.write(ctx, buf, sizeof(buf));
   CHECK(w >= 0 && w < (ssize_t)sizeof(buf), "a device that never accepts: write returned %ld", (long)w);
   CHECK(n_wait >= 1 && n_wait <= 4, "a device that never accepts was waited on %u times", n_wait);

   /* Non-blocking: EAGAIN returns at once, no wait. */
   reset_counts();
   audio_alsa.set_nonblock_state(ctx, true);
   scr_writei_seq[0] = -EAGAIN; scr_writei_n = 1;
   w = audio_alsa.write(ctx, buf, sizeof(buf));
   CHECK(w == 0 && n_wait == 0, "non-blocking EAGAIN: returned %ld after %u waits", (long)w, n_wait);

   audio_alsa.free(ctx);
}

int main(void)
{
   pthread_t wd;
   pthread_create(&wd, NULL, watchdog, NULL);
   pthread_detach(wd);

   printf("alsa lifecycle:\n");
   s_stop_start(1,  0,    "a device that can pause: stop pauses, start unpauses",         1, 0, 1, 0);
   s_stop_start(0,  0,    "a device that cannot pause: stop drops, start prepares",       0, 1, 0, 1);
   s_stop_start(1, -EIO,  "a device that says it can pause and refuses: stop drops",      1, 1, 0, 1);
   s_repeated();
   s_blocking_write();

   if (failures)
   {
      printf("%u failure(s)\n", failures);
      return 1;
   }
   printf("alsa lifecycle: alive() follows stop and start on every device, and the blocking write waits only when told to, and never without a bound\n");
   return 0;
}
