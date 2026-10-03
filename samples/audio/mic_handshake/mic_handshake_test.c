/* What the microphone capture handshake costs, against a device with a
 * clock.
 *
 * The threaded capture path splits the same way the playback pipeline
 * does: a worker owns the blocking device read, the dual-mono
 * up-channel and the resampler, and the core's microphone_driver_read()
 * is left with a ring read. Between them sits one condition variable
 * used in both directions - the worker waits on it when the ring is too
 * full to take another slice, the core waits on it when the ring is too
 * empty to answer, and each signals it after doing its half.
 *
 * That is the last hand-rolled condvar handshake in the audio path, and
 * the shape retro_eventcount exists for: outgoing_samples is already a
 * retro_spsc, so the lock carries no data, only the parking. Before
 * converting it, this measures it.
 *
 * The awkward part, and the reason for measuring rather than
 * converting straight away: one condition variable, two predicates.
 * scond_signal() releases one waiter, an eventcount notify releases
 * every one, so a conversion either splits it in two or accepts that
 * each side may be woken by the other's announcement.
 *
 * The answer is that one is enough, and it is arithmetic rather than
 * luck. The ring is AUDIO_CHUNK_SIZE_NONBLOCKING * AUDIO_MAX_RATIO =
 * 32768 samples; the worker parks when it has room for less than one
 * 2048-sample slice, so above 30720 held, and the core parks when it
 * holds less than the frame it was asked for, which is at most a
 * slice. Both at once would need the ring to hold more than 30720 and
 * fewer than 2048 samples. The column below measures it anyway rather
 * than trusting the sum - 0.00% at every setting - because the sum
 * depends on three constants that a later change could move, and this
 * is where that would show up.
 *
 * Reported per setting:
 *
 *   device reads   calls the worker made into the device, per second,
 *                  and how many of its wait_readable() calls came back
 *                  empty.
 *   both parked    samples, taken on the device clock, where the worker
 *                  was inside its ring-full wait and the core inside
 *                  its ring-empty wait at the same moment. The two
 *                  predicates are near-complements, so this is expected
 *                  to be small; if it is zero, one eventcount will do.
 *   core read us   how long microphone_driver_read() took, p50/p99/max.
 *                  The core's wait is bounded at 10 ms, so a p99 near
 *                  that is the worker failing to keep up rather than
 *                  the handshake being slow.
 *   silence        frames the core was given as padding because the
 *                  ring was short. The device writes a non-zero
 *                  pattern, so a zero in the core's buffer is padding
 *                  and nothing else.
 *
 * No pass/fail line beyond the fixture coming up: the numbers are a
 * baseline to compare a converted handshake against, and the
 * comparison is the test.
 *
 * Includes audio/audio_driver.c so the shipping worker and the shipping
 * read run, and brings the microphone up through microphone_driver_open_mic()
 * rather than assembling the state by hand, so the resampler, the ring
 * sizing and the worker's own start-up are the real ones.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>

#include <boolean.h>
#include <retro_atomic.h>

#include "../../../audio/audio_driver.c"

#define DEV_RATE      48000
#define CORE_RATE     48000
#define FPS           60.0
#define MAX_SAMPLES   65536

/* --- the device ------------------------------------------------------ */

/* Credited on a clock: one period's worth of samples every period, as a
 * capture device fills its own buffer whether or not anyone reads. */
static retro_atomic_size_t dev_avail;       /* bytes ready to be read   */
static retro_atomic_size_t dev_overruns;    /* periods dropped, buffer full */
static retro_atomic_int_t  dev_running     = RETRO_ATOMIC_INT_INITIALIZER(1);
static size_t              dev_capacity;    /* bytes the device holds   */
static size_t              dev_period;      /* bytes credited per tick  */
static int16_t             dev_pattern     = 0;

static retro_atomic_size_t cnt_reads;
static retro_atomic_size_t cnt_read_bytes;
static retro_atomic_size_t cnt_waits;
static retro_atomic_size_t cnt_waits_empty;

/* Whether each side's park condition holds, sampled on the device
 * clock. Not a flag either side raises - the worker's park is inside
 * shipping code and there is no hook in it, and adding one would be
 * measuring the instrumentation. Both predicates are pure functions of
 * the ring, readable from any thread, so the sampler evaluates them
 * itself: the worker parks when the ring has no room for a slice, the
 * core parks when it holds less than a frame's worth. Whether those two
 * ever hold at once is the question a single eventcount stands or falls
 * on. */
static retro_atomic_size_t cnt_both_parked;
static retro_atomic_size_t cnt_worker_parks;
static retro_atomic_size_t cnt_core_parks;
static retro_atomic_size_t cnt_samples;
static size_t              core_want_bytes;

static retro_microphone_t *the_mic;

static pthread_mutex_t     dev_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t      dev_cond = PTHREAD_COND_INITIALIZER;

static void *dev_thread(void *arg)
{
   struct timespec next;
   long step_ns = (long)((double)(dev_period / sizeof(int16_t))
         * 1e9 / (double)DEV_RATE);
   (void)arg;
   clock_gettime(CLOCK_MONOTONIC, &next);
   while (retro_atomic_load_acquire_int(&dev_running))
   {
      size_t have;
      next.tv_nsec += step_ns;
      next.tv_sec  += next.tv_nsec / 1000000000L;
      next.tv_nsec %= 1000000000L;
      clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, NULL);

      have = retro_atomic_load_acquire_size(&dev_avail);
      if (have + dev_period <= dev_capacity)
         retro_atomic_fetch_add_size(&dev_avail, dev_period);
      else
         retro_atomic_fetch_add_size(&dev_overruns, 1);

      /* Sampled on the device's own clock rather than either
       * participant's, so neither side's timing biases the rate. */
      if (the_mic && the_mic->outgoing_init)
      {
         bool worker = microphone_outgoing_room(the_mic)
               < AUDIO_CHUNK_SIZE_NONBLOCKING * sizeof(int16_t);
         bool core   = retro_spsc_read_avail(&the_mic->outgoing_samples)
               < core_want_bytes;
         retro_atomic_fetch_add_size(&cnt_samples, 1);
         if (worker)
            retro_atomic_fetch_add_size(&cnt_worker_parks, 1);
         if (core)
            retro_atomic_fetch_add_size(&cnt_core_parks, 1);
         if (worker && core)
            retro_atomic_fetch_add_size(&cnt_both_parked, 1);
      }

      pthread_mutex_lock(&dev_lock);
      pthread_cond_broadcast(&dev_cond);
      pthread_mutex_unlock(&dev_lock);
   }
   return NULL;
}

/* --- the driver interface -------------------------------------------- */

static void *mdev_init(void)                 { static int h = 1; return &h; }
static void  mdev_free(void *d)              { (void)d; }
static bool  mdev_alive(const void *d, const void *m) { (void)d; (void)m; return true; }
static bool  mdev_start(void *d, void *m)    { (void)d; (void)m; return true; }
static bool  mdev_stop(void *d, void *m)     { (void)d; (void)m; return true; }
static bool  mdev_use_float(const void *d, const void *m)
{ (void)d; (void)m; return false; }
static void  mdev_close(void *d, void *m)    { (void)d; (void)m; }

static void *mdev_open(void *d, const char *dev, unsigned rate,
      unsigned latency, unsigned *new_rate)
{
   static int h = 2;
   (void)d; (void)dev; (void)latency;
   if (new_rate) *new_rate = rate;
   return &h;
}

/* Set by the teardown case: read() then honours the header's "all reads
 * should block until all requested frames are provided" instead of
 * returning what is there. The measurement runs above leave it off, so
 * their numbers are unchanged. */
static bool mdev_blocking;

/* The contract's read: it returns only once it has everything it was
 * asked for, so the only thing that ends the wait is more samples. */
static int mdev_read_blocking(void *buf, size_t size)
{
   size_t done = 0;
   int16_t *out = (int16_t*)buf;

   size -= size % sizeof(int16_t);
   while (done < size)
   {
      size_t i;
      size_t have = retro_atomic_load_acquire_size(&dev_avail);
      size_t take = (have < size - done) ? have : size - done;

      take -= take % sizeof(int16_t);
      if (!take)
      {
         pthread_mutex_lock(&dev_lock);
         pthread_cond_wait(&dev_cond, &dev_lock);
         pthread_mutex_unlock(&dev_lock);
         continue;
      }
      retro_atomic_fetch_sub_size(&dev_avail, take);
      for (i = 0; i < take / sizeof(int16_t); i++)
      {
         if (++dev_pattern == 0)
            dev_pattern = 1;
         out[done / sizeof(int16_t) + i] = dev_pattern;
      }
      done += take;
   }
   retro_atomic_fetch_add_size(&cnt_reads, 1);
   retro_atomic_fetch_add_size(&cnt_read_bytes, done);
   return (int)done;
}

static int mdev_read(void *d, void *m, void *buf, size_t size)
{
   size_t have, take, i;
   int16_t *out = (int16_t*)buf;
   (void)d; (void)m;

   if (mdev_blocking)
      return mdev_read_blocking(buf, size);

   have = retro_atomic_load_acquire_size(&dev_avail);
   take = (have < size) ? have : size;
   take -= take % sizeof(int16_t);
   if (!take)
      return 0;
   retro_atomic_fetch_sub_size(&dev_avail, take);

   /* Never zero: a zero in the core's buffer is then padding and can be
    * counted as one. */
   for (i = 0; i < take / sizeof(int16_t); i++)
   {
      if (++dev_pattern == 0)
         dev_pattern = 1;
      out[i] = dev_pattern;
   }
   retro_atomic_fetch_add_size(&cnt_reads, 1);
   retro_atomic_fetch_add_size(&cnt_read_bytes, take);
   return (int)take;
}

static size_t mdev_wait_readable(void *d, void *m, size_t len)
{
   struct timespec ts;
   size_t have;
   int laps = 8;
   (void)d; (void)m;

   retro_atomic_fetch_add_size(&cnt_waits, 1);
   /* One period is the most this will wait for, as alsa.c does: the
    * worker asks for a whole AUDIO_CHUNK_SIZE_NONBLOCKING slice, which
    * is larger than a small device's entire buffer, and a wait for more
    * than the device can ever hold can only time out. */
   if (len > dev_period)
      len = dev_period;
   for (;;)
   {
      have = retro_atomic_load_acquire_size(&dev_avail);
      if (have >= len)
         return have;
      if (--laps < 0)
         break;
      pthread_mutex_lock(&dev_lock);
      clock_gettime(CLOCK_REALTIME, &ts);
      ts.tv_nsec += 20L * 1000000L;
      ts.tv_sec  += ts.tv_nsec / 1000000000L;
      ts.tv_nsec %= 1000000000L;
      pthread_cond_timedwait(&dev_cond, &dev_lock, &ts);
      pthread_mutex_unlock(&dev_lock);
   }
   retro_atomic_fetch_add_size(&cnt_waits_empty, 1);
   return 0;
}

static microphone_driver_t clocked_mic = {
   mdev_init, mdev_free, mdev_read, "clocked",
   NULL /* device_list_new */, NULL /* device_list_free */,
   mdev_open, mdev_close, mdev_alive, mdev_start, mdev_stop,
   mdev_use_float, mdev_wait_readable
};

/* --- fixture --------------------------------------------------------- */

static bool mic_up(unsigned latency_ms)
{
   microphone_driver_state_t *mic_st = &mic_driver_st;
   settings_t *settings              = config_get_ptr();
   retro_microphone_params_t params;

   dev_capacity = (size_t)((double)DEV_RATE * latency_ms / 1000.0)
         * sizeof(int16_t);
   dev_period   = dev_capacity / 4;
   dev_period  -= dev_period % sizeof(int16_t);
   if (!dev_period)
      return false;
   retro_atomic_size_init(&dev_avail, 0);
   retro_atomic_size_init(&dev_overruns, 0);
   retro_atomic_size_init(&cnt_reads, 0);
   retro_atomic_size_init(&cnt_read_bytes, 0);
   retro_atomic_size_init(&cnt_waits, 0);
   retro_atomic_size_init(&cnt_waits_empty, 0);
   retro_atomic_size_init(&cnt_both_parked, 0);
   retro_atomic_size_init(&cnt_worker_parks, 0);
   retro_atomic_size_init(&cnt_core_parks, 0);
   retro_atomic_size_init(&cnt_samples, 0);

   memset(mic_st, 0, sizeof(*mic_st));
   settings->bools.microphone_enable         = true;
   settings->bools.audio_threaded_pipeline   = true;
   settings->uints.microphone_latency        = latency_ms;
   settings->uints.microphone_resampler_quality = RESAMPLER_QUALITY_NORMAL;
   settings->arrays.microphone_device[0]     = '\0';
   strlcpy(settings->arrays.microphone_resampler, "sinc",
         sizeof(settings->arrays.microphone_resampler));

   /* Everything microphone_driver_init_internal() would have set, short
    * of picking the driver out of the built-in table - the fake one is
    * not in it. From here down the shipping path runs. */
   mic_st->driver         = &clocked_mic;
   mic_st->driver_context = clocked_mic.init();
   strlcpy(mic_st->resampler_ident, "sinc", sizeof(mic_st->resampler_ident));
   mic_st->resampler_quality = RESAMPLER_QUALITY_NORMAL;
   mic_st->flags         |= MICROPHONE_DRIVER_FLAG_ACTIVE;

   params.rate = CORE_RATE;
   if (!(the_mic = microphone_driver_open_mic(&params)))
      return false;
   if (!microphone_driver_set_mic_state(the_mic, true))
      return false;
   return the_mic->capture_thread != NULL;
}

static void mic_down(void)
{
   microphone_driver_close_mic(the_mic);
   the_mic = NULL;
   microphone_driver_deinit(false);
}

/* --- one run --------------------------------------------------------- */

static retro_time_t *lat_us;
static size_t        lat_n;

static int cmp_time(const void *a, const void *b)
{
   retro_time_t x = *(const retro_time_t*)a;
   retro_time_t y = *(const retro_time_t*)b;
   return (x > y) - (x < y);
}

static void run_one(unsigned latency_ms, double seconds)
{
   pthread_t dev;
   size_t    per_frame = (size_t)(CORE_RATE / FPS);
   size_t    i, frames = (size_t)(seconds * FPS);
   size_t    warm = (size_t)FPS, silence = 0, asked = 0;
   struct timespec next;
   long      step_ns = (long)(1e9 / FPS);
   int16_t  *buf;
   double    p50 = 0.0, p99 = 0.0, worst = 0.0;
   size_t    reads, waits, empty, both, samples, wparks, cparks;

   if (!mic_up(latency_ms))
   {
      printf("  %3u ms: fixture failed\n", latency_ms);
      return;
   }

   buf             = (int16_t*)malloc(per_frame * sizeof(int16_t));
   core_want_bytes = per_frame * sizeof(int16_t);
   lat_n           = 0;
   retro_atomic_store_release_int(&dev_running, 1);
   pthread_create(&dev, NULL, dev_thread, NULL);

   clock_gettime(CLOCK_MONOTONIC, &next);
   for (i = 0; i < frames; i++)
   {
      retro_time_t t0, t1;
      size_t j;

      next.tv_nsec += step_ns;
      next.tv_sec  += next.tv_nsec / 1000000000L;
      next.tv_nsec %= 1000000000L;
      clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, NULL);

      memset(buf, 0, per_frame * sizeof(int16_t));
      t0 = cpu_features_get_time_usec();
      microphone_driver_read(the_mic, buf, per_frame);
      t1 = cpu_features_get_time_usec();

      if (i < warm)
         continue;

      if (t1 > t0 && lat_n < MAX_SAMPLES)
         lat_us[lat_n++] = t1 - t0;
      asked += per_frame;
      for (j = 0; j < per_frame; j++)
         if (!buf[j])
            silence++;
   }

   retro_atomic_store_release_int(&dev_running, 0);
   pthread_join(dev, NULL);

   reads   = retro_atomic_load_acquire_size(&cnt_reads);
   waits   = retro_atomic_load_acquire_size(&cnt_waits);
   empty   = retro_atomic_load_acquire_size(&cnt_waits_empty);
   both    = retro_atomic_load_acquire_size(&cnt_both_parked);
   wparks  = retro_atomic_load_acquire_size(&cnt_worker_parks);
   cparks  = retro_atomic_load_acquire_size(&cnt_core_parks);
   samples = retro_atomic_load_acquire_size(&cnt_samples);

   if (lat_n)
   {
      qsort(lat_us, lat_n, sizeof(lat_us[0]), cmp_time);
      p50   = (double)lat_us[lat_n / 2];
      p99   = (double)lat_us[(lat_n * 99) / 100];
      worst = (double)lat_us[lat_n - 1];
   }

   printf("  %3u ms  %6u %6u %6u | %5.1f%% %5.1f%% %5.2f%% (%u) | %6.0f %6.0f %7.0f | %5.2f%%\n",
         latency_ms,
         (unsigned)reads, (unsigned)waits, (unsigned)empty,
         samples ? 100.0 * (double)wparks / (double)samples : 0.0,
         samples ? 100.0 * (double)cparks / (double)samples : 0.0,
         samples ? 100.0 * (double)both   / (double)samples : 0.0,
         (unsigned)samples,
         p50, p99, worst,
         asked ? 100.0 * (double)silence / (double)asked : 0.0);

   free(buf);
   mic_down();
}

/* Teardown while the worker is inside the device read.
 *
 * wait_readable() reports what the device has now, which is regularly
 * less than the slice asked for - this device cannot hold a whole one,
 * and alsa returns a period for the same reason. The worker used to
 * take the report as a yes/no and read the full slice anyway, so it sat
 * in a read that only more samples could end. Teardown joins the worker
 * unconditionally, so a device that stopped delivering hung the join,
 * and with it mic close, driver switch and shutdown. */
static unsigned failures;

static void teardown_alarm(int sig)
{
   /* Async-signal-safe, and the join it is firing on will not return. */
   static const char msg[] = "FAIL mic teardown: join did not return;"
         " the worker is blocked in read()\n";
   (void)sig;
   if (write(2, msg, sizeof(msg) - 1)) {}
   _exit(1);
}

static void teardown_case(void)
{
   pthread_t dev;

   mdev_blocking = true;
   if (!mic_up(32))
   {
      printf("FAIL mic teardown: could not bring the microphone up\n");
      failures++;
      mdev_blocking = false;
      return;
   }

   retro_atomic_store_release_int(&dev_running, 1);
   pthread_create(&dev, NULL, dev_thread, NULL);
   usleep(200 * 1000);

   /* The device goes quiet first, so nothing can complete a read that
    * asked for more than it had. */
   retro_atomic_store_release_int(&dev_running, 0);
   pthread_join(dev, NULL);
   usleep(50 * 1000);

   signal(SIGALRM, teardown_alarm);
   alarm(5);
   mic_down();
   alarm(0);
   signal(SIGALRM, SIG_DFL);

   mdev_blocking = false;
   printf("mic teardown: the worker leaves the device read and joins\n");
}

int main(int argc, char **argv)
{
   static const unsigned sweep[] = { 8, 16, 32, 64 };
   double seconds = (argc > 1) ? atof(argv[1]) : 3.0;
   size_t i;

   if (!(lat_us = (retro_time_t*)malloc(MAX_SAMPLES * sizeof(retro_time_t))))
      return 1;

   printf("threaded microphone capture against a clocked device,"
         " %.0f s per setting, %g fps core\n", seconds, FPS);
   printf("  device                     | park conditions held      |"
         " core read us          | padding\n");
   printf("   lat     reads  waits  empty| worker  core   both (n)   |"
         "    p50    p99     max |\n");
   for (i = 0; i < sizeof(sweep) / sizeof(sweep[0]); i++)
      run_one(sweep[i], seconds);

   free(lat_us);
   teardown_case();
   printf("mic handshake: baseline taken\n");
   return failures ? 1 : 0;
}
