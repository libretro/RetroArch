/* The threaded audio pipeline against a device with a clock.
 *
 * Every other pipeline harness answers a contract question with a
 * scripted device: it takes what it is given, when it is asked. That
 * cannot show an underrun, because nothing in it runs to a clock. This
 * one does. The device drains its ring in real time, a period at a
 * time, and counts the pulls that found less than a period there -
 * which is what a buzz is.
 *
 * The fixture is the reported CoreAudio configuration on a MacBook Pro:
 * 48 kHz stereo float out, a ring of the latency setting, a HAL IO
 * period of a quarter of that ring, a core publishing one video frame's
 * worth of int16 at a time, Audio Sync on and rate control on. The
 * driver's write, wait_writable and render callback are modelled on
 * coreaudio.c rather than simplified: whole frames only, a semaphore
 * the callback signals, a bounded wait.
 *
 * Sweeping the latency setting is the point. If the underruns fall off
 * a cliff somewhere, that number is the pipeline's real floor and can
 * be compared against the one measured by ear on hardware.
 *
 * Includes audio/audio_driver.c so the shipping producer and consumer
 * run.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

#include <boolean.h>
#include <retro_atomic.h>

#include "../../../audio/audio_driver.c"

#define OUT_RATE      48000
#define CORE_RATE     48000
#define FPS           60.0
#define CHANNELS      2

/* --- the device ------------------------------------------------------ */

static float              *dev_ring;
static size_t              dev_usable;      /* samples the ring may hold */
static size_t              dev_capacity;    /* power of two container */
static size_t              dev_period;      /* frames per pull */
static size_t              dev_write_ptr;
static size_t              dev_read_ptr;
static retro_atomic_size_t dev_filled;
static retro_atomic_size_t dev_underruns;
static retro_atomic_size_t dev_pulls;
static retro_atomic_size_t dev_silent_samples;
static pthread_mutex_t     dev_wake_lock  = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t      dev_wake_cond  = PTHREAD_COND_INITIALIZER;
static retro_atomic_int_t  dev_waiters    = RETRO_ATOMIC_INT_INITIALIZER(0);
static retro_atomic_int_t  dev_running    = RETRO_ATOMIC_INT_INITIALIZER(1);

static size_t dev_rb_write_avail(void)
{
   size_t filled = retro_atomic_load_acquire_size(&dev_filled);
   return (filled < dev_usable) ? dev_usable - filled : 0;
}

static void dev_signal(void)
{
   if (retro_atomic_load_acquire_int(&dev_waiters))
   {
      pthread_mutex_lock(&dev_wake_lock);
      pthread_cond_broadcast(&dev_wake_cond);
      pthread_mutex_unlock(&dev_wake_lock);
   }
}

static void dev_wait(size_t want, unsigned ms)
{
   struct timespec ts;
   retro_atomic_fetch_add_int(&dev_waiters, 1);
   pthread_mutex_lock(&dev_wake_lock);
   if (dev_rb_write_avail() < want)
   {
      clock_gettime(CLOCK_REALTIME, &ts);
      ts.tv_nsec += (long)ms * 1000000L;
      ts.tv_sec  += ts.tv_nsec / 1000000000L;
      ts.tv_nsec %= 1000000000L;
      pthread_cond_timedwait(&dev_wake_cond, &dev_wake_lock, &ts);
   }
   pthread_mutex_unlock(&dev_wake_lock);
   retro_atomic_fetch_sub_int(&dev_waiters, 1);
}

/* The render callback, as coreaudio_audio_write_cb() does it: take what
 * is there in whole frames, pad the rest with silence, count it. */
static void dev_render(void)
{
   size_t needed = dev_period * CHANNELS;
   size_t avail  = retro_atomic_load_acquire_size(&dev_filled);
   size_t take;

   if (avail > needed)
      avail = needed;
   avail -= avail % CHANNELS;
   take   = avail;

   if (take)
   {
      dev_read_ptr = (dev_read_ptr + take) & (dev_capacity - 1);
      retro_atomic_fetch_sub_size(&dev_filled, take);
   }
   if (take < needed)
   {
      retro_atomic_fetch_add_size(&dev_underruns, 1);
      retro_atomic_fetch_add_size(&dev_silent_samples, needed - take);
   }
   retro_atomic_fetch_add_size(&dev_pulls, 1);
   dev_signal();
}

/* How late the device's own wakes come, while dev_measure is set: a
 * device woken late pulls its overdue periods back to back, which
 * drains the cushion as surely as a late core does. */
static retro_atomic_int_t dev_measure;
static long               dev_worst_late_ns;

/* How long the pipeline's consumer takes to answer a frame, while
 * dev_measure is set: the core thread stamps each signal, in
 * microseconds from the window's start, unless one is still waiting,
 * and the next write to the device takes the stamp. A consumer held off - descheduled, or slowed
 * several-fold by a sanitizer - starves the device whatever the pipe
 * does, so that run says nothing about priming. */
static struct timespec    measure_t0;
static retro_atomic_int_t signal_at_us;
static long               consumer_worst_ns;

static int measure_now_us(void)
{
   struct timespec now;
   clock_gettime(CLOCK_MONOTONIC, &now);
   return (int)((now.tv_sec - measure_t0.tv_sec) * 1000000L
         + (now.tv_nsec - measure_t0.tv_nsec) / 1000L) + 1;
}

static void *dev_thread(void *arg)
{
   /* The HAL's IO thread: one pull every period, on the clock. */
   struct timespec next;
   long step_ns = (long)((double)dev_period * 1e9 / (double)OUT_RATE);
   (void)arg;
   /* A unit that starts before there is anything to play spends the
    * priming wait pulling silence. With defer_start the device holds
    * off until the ring first has a period in it, which is what the
    * driver does. EAGER_START in the environment restores the old
    * behaviour, for comparing the two. */
   if (!getenv("EAGER_START"))
      while (retro_atomic_load_acquire_int(&dev_running)
            && retro_atomic_load_acquire_size(&dev_filled) < dev_period * CHANNELS)
         usleep(200);
   clock_gettime(CLOCK_MONOTONIC, &next);
   while (retro_atomic_load_acquire_int(&dev_running))
   {
      next.tv_nsec += step_ns;
      next.tv_sec  += next.tv_nsec / 1000000000L;
      next.tv_nsec %= 1000000000L;
      clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, NULL);
      if (retro_atomic_load_acquire_int(&dev_measure))
      {
         struct timespec now;
         long late;
         clock_gettime(CLOCK_MONOTONIC, &now);
         late = (now.tv_sec - next.tv_sec) * 1000000000L
            + (now.tv_nsec - next.tv_nsec);
         if (late > dev_worst_late_ns)
            dev_worst_late_ns = late;
      }
      dev_render();
   }
   return NULL;
}

/* --- the driver interface, as coreaudio.c implements it -------------- */

static void *cdev_init(const char *device, unsigned rate, unsigned latency,
      unsigned *new_rate)
{
   static int handle = 1;
   (void)device; (void)latency;
   if (new_rate) *new_rate = rate;
   return &handle;
}

static ssize_t cdev_write(void *data, const void *buf, size_t len)
{
   size_t samples = len / sizeof(float);
   size_t written = 0;
   int    laps    = 8;
   (void)data; (void)buf;

   if (retro_atomic_load_acquire_int(&dev_measure))
   {
      int at = retro_atomic_exchange_int(&signal_at_us, 0);
      if (at)
      {
         long late = (long)(measure_now_us() - at) * 1000L;
         if (late > consumer_worst_ns)
            consumer_worst_ns = late;
      }
   }

   while (samples > 0)
   {
      size_t avail    = dev_rb_write_avail();
      size_t to_write = (avail < samples) ? avail : samples;
      to_write       -= to_write % CHANNELS;
      if (to_write > 0)
      {
         dev_write_ptr = (dev_write_ptr + to_write) & (dev_capacity - 1);
         retro_atomic_fetch_add_size(&dev_filled, to_write);
         written += to_write;
         samples -= to_write;
      }
      if (samples > 0)
      {
         if (--laps < 0)
            break;
         dev_wait(1, 100);
      }
   }
   return (ssize_t)(written * sizeof(float));
}

static size_t cdev_wait_writable(void *data, size_t len)
{
   size_t want = len / sizeof(float);
   int    laps = 8;
   (void)data;
   if (want % CHANNELS)
      want += CHANNELS - want % CHANNELS;
   if (want > dev_usable)
      want = dev_usable;
   for (;;)
   {
      size_t avail = dev_rb_write_avail();
      if (avail >= want)
         return avail * sizeof(float);
      if (--laps < 0)
         break;
      dev_wait(want, 100);
   }
   return 0;
}

static bool   cdev_stop(void *d)                { (void)d; return true; }
static bool   cdev_start(void *d, bool s)       { (void)d; (void)s; return true; }
static bool   cdev_alive(void *d)               { (void)d; return true; }
static void   cdev_set_nonblock(void *d, bool s){ (void)d; (void)s; }
static void   cdev_free(void *d)                { (void)d; }
static bool   cdev_use_float(void *d)           { (void)d; return true; }
static size_t cdev_write_avail(void *d)         { (void)d; return dev_rb_write_avail() * sizeof(float); }
static size_t cdev_buffer_size(void *d)         { (void)d; return dev_usable * sizeof(float); }
static size_t cdev_underruns(void *d)           { (void)d; return retro_atomic_load_acquire_size(&dev_underruns); }
static size_t cdev_frames_consumed(void *d)
{
   (void)d;
   return retro_atomic_load_acquire_size(&dev_pulls) * dev_period;
}

static audio_driver_t clocked_driver = {
   cdev_init, cdev_write, cdev_stop, cdev_start, cdev_alive,
   cdev_set_nonblock, cdev_free, cdev_use_float, "clocked", NULL, NULL,
   cdev_write_avail, cdev_buffer_size, NULL /* write_raw */,
   cdev_wait_writable, cdev_frames_consumed, cdev_underruns
};

/* --- threads --------------------------------------------------------- */

static retro_atomic_int_t consumer_run = RETRO_ATOMIC_INT_INITIALIZER(1);
static unsigned stall_failures;

static void *consumer(void *arg)
{
   (void)arg;
   while (retro_atomic_load_acquire_int(&consumer_run))
      audio_driver_pipeline_consume(&audio_driver_st);
   return NULL;
}

static int16_t frame_audio[4096 * 2];

/* --- fixture --------------------------------------------------------- */

static bool pipeline_up(unsigned latency_ms)
{
   audio_driver_state_t *st = &audio_driver_st;
   size_t per_frame = (size_t)(CORE_RATE / FPS);
   size_t ring_bytes;

   /* The device, sized as coreaudio.c sizes it. */
   dev_usable   = (size_t)((latency_ms * OUT_RATE) / 1000) * CHANNELS;
   dev_period   = (size_t)((latency_ms * OUT_RATE) / 1000 / 4);
   dev_capacity = 1;
   while (dev_capacity < dev_usable)
      dev_capacity <<= 1;
   dev_ring      = (float*)calloc(dev_capacity, sizeof(float));
   dev_write_ptr = dev_read_ptr = 0;
   retro_atomic_size_init(&dev_filled, 0);
   retro_atomic_size_init(&dev_underruns, 0);
   retro_atomic_size_init(&dev_pulls, 0);
   retro_atomic_size_init(&dev_silent_samples, 0);

   memset(st, 0, sizeof(*st));
   st->current_audio        = &clocked_driver;
   st->context_audio_data   = clocked_driver.init(NULL, OUT_RATE, latency_ms, NULL);
   st->input                = (double)CORE_RATE;
   st->src_ratio_orig       = (double)OUT_RATE / (double)CORE_RATE;
   st->src_ratio_curr       = st->src_ratio_orig;
   st->cached_rate_adjust   = 1.0;
   st->volume_gain          = 1.0f;
   st->out_channels         = CHANNELS;
   st->buffer_size          = clocked_driver.buffer_size(st->context_audio_data);
   st->output_samples_buf   = (float*)malloc(1 << 20);
   st->output_samples_buf_length = 1 << 20;
   st->input_data           = (float*)malloc(1 << 20);
   st->input_data_length    = 1 << 20;
   st->synth_buf            = (float*)calloc(1, 1 << 20);
   st->output_samples_int16 = (int16_t*)malloc(1 << 20);
   st->output_samples_int16_length = 1 << 20;
   st->pipe_scratch         = (uint8_t*)malloc(1 << 20);
   st->pipe_conv            = (uint8_t*)malloc(1 << 20);
   st->pipe_pass_frames     = per_frame;
   st->pipe_float           = false;
   st->pipe_frame_bytes     = CHANNELS * sizeof(int16_t);
   AUDIO_FLAGS_SET(st, AUDIO_FLAG_USE_FLOAT);
   strcpy(st->resampler_ident, "sinc");
   st->resampler_quality    = RESAMPLER_QUALITY_NORMAL;
   if (!retro_resampler_realloc(&st->resampler_data, &st->resampler,
            st->resampler_ident, st->resampler_quality, st->src_ratio_orig))
      return false;
   retro_atomic_store_release_int(&st->pipe_ctrl_avail, -1);
   st->rate_control_delta   = 0.005f;
   st->drc_threshold_int16s = 1600;
   st->sink_bias            = 1.0;
   config_get_ptr()->bools.audio_sink_rate_estimation = true;
   config_get_ptr()->uints.audio_output_sample_rate   = OUT_RATE;
   st->out_rate = OUT_RATE;
   config_get_ptr()->bools.audio_sync                 = true;
   audio_driver_publish_runloop();

   ring_bytes = per_frame * 6 * st->pipe_frame_bytes;
   if (!retro_spsc_init(&st->pipe_ring, ring_bytes))
      return false;
   retro_eventcount_init(&st->pipe_space);
   retro_eventcount_init(&st->pipe_data);
   st->state_lock     = slock_new();
   st->pipe_park_ready = true;
   st->pipe_threaded  = true;
   st->pipe_priming   = true;
   AUDIO_FLAGS_SET(st, AUDIO_FLAG_ACTIVE | AUDIO_FLAG_STARTED
         | AUDIO_FLAG_PIPELINE_THREADED | AUDIO_FLAG_CONTROL);
   return st->state_lock && st->output_samples_buf && st->pipe_scratch
      && st->input_data && st->synth_buf && dev_ring;
}

static void pipeline_down(void)
{
   audio_driver_state_t *st = &audio_driver_st;
   /* The resampler is built per run by retro_resampler_realloc and is
    * the driver's to release, the way audio_driver_deinit releases
    * it; seven runs of this oracle left seven of them behind. */
   if (st->resampler && st->resampler_data)
      st->resampler->free(st->resampler_data);
   st->resampler      = NULL;
   st->resampler_data = NULL;
   retro_spsc_free(&st->pipe_ring);
   retro_eventcount_free(&st->pipe_space);
   retro_eventcount_free(&st->pipe_data);
   slock_free(st->state_lock);
   free(st->output_samples_buf);
   free(st->pipe_scratch);
   free(st->pipe_conv);
   free(st->input_data);
   free(st->synth_buf);
   free(st->output_samples_int16);
   free(dev_ring);
   dev_ring = NULL;
}

/* --- one run --------------------------------------------------------- */

static void run_one(unsigned latency_ms, double seconds)
{
   pthread_t cons, dev;
   size_t    per_frame = (size_t)(CORE_RATE / FPS);
   size_t    i, frames = (size_t)(seconds * FPS);
   struct timespec next;
   long      step_ns   = (long)(1e9 / FPS);
   size_t    under, pulls, silent;
   size_t    warm_under = 0, warm_pulls = 0, warm_silent = 0;

   if (!pipeline_up(latency_ms))
   {
      printf("  %3u ms: fixture failed\n", latency_ms);
      return;
   }

   retro_atomic_store_release_int(&dev_running, 1);
   retro_atomic_store_release_int(&consumer_run, 1);
   pthread_create(&dev,  NULL, dev_thread, NULL);
   pthread_create(&cons, NULL, consumer,   NULL);

   clock_gettime(CLOCK_MONOTONIC, &next);
   for (i = 0; i < frames; i++)
   {
      next.tv_nsec += step_ns;
      next.tv_sec  += next.tv_nsec / 1000000000L;
      next.tv_nsec %= 1000000000L;
      clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, NULL);
      audio_driver_submit(&audio_driver_st, 1.0f, frame_audio,
            per_frame * 2, false, false, false, true);
      audio_driver_pipeline_signal(&audio_driver_st);
      /* One second in, take the startup transient out of the count so
       * what remains is steady state. */
      if (i == (size_t)FPS)
      {
         warm_under  = retro_atomic_load_acquire_size(&dev_underruns);
         warm_pulls  = retro_atomic_load_acquire_size(&dev_pulls);
         warm_silent = retro_atomic_load_acquire_size(&dev_silent_samples);
      }
   }

   /* Let the tail drain, then stop. */
   usleep(50000);
   retro_atomic_store_release_int(&consumer_run, 0);
   retro_atomic_store_release_int(&dev_running, 0);
   audio_driver_pipeline_wake();
   dev_signal();
   pthread_join(cons, NULL);
   pthread_join(dev,  NULL);

   under  = retro_atomic_load_acquire_size(&dev_underruns);
   pulls  = retro_atomic_load_acquire_size(&dev_pulls);
   silent = retro_atomic_load_acquire_size(&dev_silent_samples);
   printf("  %3u ms  ring %5u  period %4u | startup: short %4u, %6.1f ms silence"
          " | steady: %5u pulls, short %4u (%5.2f%%), %6.2f ms silence\n",
         latency_ms,
         (unsigned)(dev_usable / CHANNELS), (unsigned)dev_period,
         (unsigned)warm_under,
         (double)warm_silent / CHANNELS * 1000.0 / OUT_RATE,
         (unsigned)(pulls - warm_pulls), (unsigned)(under - warm_under),
         (pulls - warm_pulls) ? 100.0 * (double)(under - warm_under)
               / (double)(pulls - warm_pulls) : 0.0,
         (double)(silent - warm_silent) / CHANNELS * 1000.0 / OUT_RATE);
   pipeline_down();
}

/* --- a stall, then jitter -------------------------------------------- */
/* Audio Sync and rate control off: the pipe's fill is regulated by
 * nothing, so the cushion it holds ahead of the device is what priming
 * put there.  One stall - the core away for longer than that cushion,
 * and, as a core paced to the display does, not catching up afterwards
 * - burns it.  From then on the core's ordinary jitter, well inside the
 * cushion, reaches the device as silence unless the pipe primes again.
 *
 * The device holds 8 ms; the jitter is up to 12 ms, past the device and
 * well short of the cushion of a frame plus the device.  Passes when
 * the tail after the stall has no short pull. */

/* A thread that only sleeps a millisecond at a time and notes how late
 * each wake comes. The fixture's own threads - the pipeline's consumer
 * above all, which a sanitizer slows several-fold - run on the same
 * cores; when this is held off past a quarter frame, so were they, and
 * an underrun in that window is the host's, not the pipe's. */
static retro_atomic_int_t canary_run;
static long               canary_worst_ns;

static void *canary(void *arg)
{
   (void)arg;
   while (retro_atomic_load_acquire_int(&canary_run))
   {
      struct timespec at, now;
      long late;
      clock_gettime(CLOCK_MONOTONIC, &at);
      at.tv_nsec += 1000000L;
      at.tv_sec  += at.tv_nsec / 1000000000L;
      at.tv_nsec %= 1000000000L;
      clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &at, NULL);
      clock_gettime(CLOCK_MONOTONIC, &now);
      late = (now.tv_sec - at.tv_sec) * 1000000000L
         + (now.tv_nsec - at.tv_nsec);
      if (late > canary_worst_ns)
         canary_worst_ns = late;
   }
   return NULL;
}

static bool stall_case_once(void)
{
   audio_driver_state_t *st = &audio_driver_st;
   long      host_stall_ns  = 0;
   pthread_t cons, dev, can;
   size_t    per_frame = (size_t)(CORE_RATE / FPS);
   size_t    i;
   struct timespec next;
   long      step_ns   = (long)(1e9 / FPS);
   size_t    before_under, before_silent, after_under, after_silent;
   unsigned  seed = 12345;

   if (!pipeline_up(8))
   {
      printf("stall: fixture failed\n");
      stall_failures++;
      return true;
   }
   /* The unregulated path: no sync, no rate control. */
   config_get_ptr()->bools.audio_sync = false;
   AUDIO_FLAGS_CLEAR(st, AUDIO_FLAG_CONTROL);
   audio_driver_publish_runloop();

   retro_atomic_store_release_int(&dev_running, 1);
   retro_atomic_store_release_int(&consumer_run, 1);
   pthread_create(&dev,  NULL, dev_thread, NULL);
   pthread_create(&cons, NULL, consumer,   NULL);

   clock_gettime(CLOCK_MONOTONIC, &next);
   /* Steady for a second */
   for (i = 0; i < (size_t)FPS; i++)
   {
      next.tv_nsec += step_ns;
      next.tv_sec  += next.tv_nsec / 1000000000L;
      next.tv_nsec %= 1000000000L;
      clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, NULL);
      audio_driver_submit(st, 1.0f, frame_audio, per_frame * 2,
            false, false, false, true);
      audio_driver_pipeline_signal(st);
   }
   /* The stall: 60 ms away, and the schedule moves with it */
   next.tv_nsec += 60000000L;
   next.tv_sec  += next.tv_nsec / 1000000000L;
   next.tv_nsec %= 1000000000L;
   clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, NULL);
   /* Half a second for the stall's own silence and the re-prime to
    * settle, then the count that matters */
   for (i = 0; i < (size_t)(FPS / 2); i++)
   {
      next.tv_nsec += step_ns;
      next.tv_sec  += next.tv_nsec / 1000000000L;
      next.tv_nsec %= 1000000000L;
      clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, NULL);
      audio_driver_submit(st, 1.0f, frame_audio, per_frame * 2,
            false, false, false, true);
      audio_driver_pipeline_signal(st);
   }
   before_under  = retro_atomic_load_acquire_size(&dev_underruns);
   before_silent = retro_atomic_load_acquire_size(&dev_silent_samples);

   canary_worst_ns   = 0;
   dev_worst_late_ns = 0;
   consumer_worst_ns = 0;
   clock_gettime(CLOCK_MONOTONIC, &measure_t0);
   retro_atomic_store_release_int(&signal_at_us, 0);
   retro_atomic_store_release_int(&dev_measure, 1);
   retro_atomic_store_release_int(&canary_run, 1);
   pthread_create(&can, NULL, canary, NULL);

   /* Three seconds of jitter: frames up to 12 ms late, the schedule
    * kept, so each late frame is made up by the next */
   for (i = 0; i < (size_t)(FPS * 3); i++)
   {
      struct timespec at = next;
      long late_ns;
      next.tv_nsec += step_ns;
      next.tv_sec  += next.tv_nsec / 1000000000L;
      next.tv_nsec %= 1000000000L;
      at = next;
      seed    = seed * 1103515245u + 12345u;
      late_ns = (long)((seed >> 16) % 12001) * 1000L;
      at.tv_nsec += late_ns;
      at.tv_sec  += at.tv_nsec / 1000000000L;
      at.tv_nsec %= 1000000000L;
      clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &at, NULL);
      /* How late this thread actually woke past the lateness it
       * meant: a host that descheduled the whole process for a frame
       * or more makes the device's count meaningless, and the run is
       * retried rather than read. */
      {
         struct timespec now;
         long overshoot_ns;
         clock_gettime(CLOCK_MONOTONIC, &now);
         overshoot_ns = (now.tv_sec - at.tv_sec) * 1000000000L
            + (now.tv_nsec - at.tv_nsec);
         if (overshoot_ns > host_stall_ns)
            host_stall_ns = overshoot_ns;
      }
      audio_driver_submit(st, 1.0f, frame_audio, per_frame * 2,
            false, false, false, true);
      /* The oldest frame the consumer has not answered keeps its
       * stamp: a consumer frames behind is measured from the first. */
      retro_atomic_cas_int(&signal_at_us, 0, measure_now_us());
      audio_driver_pipeline_signal(st);
   }
   /* Counted at the last publish, before the tail drains: the drain
    * is the run ending, not the core being late. */
   after_under  = retro_atomic_load_acquire_size(&dev_underruns);
   after_silent = retro_atomic_load_acquire_size(&dev_silent_samples);
   retro_atomic_store_release_int(&canary_run, 0);
   retro_atomic_store_release_int(&dev_measure, 0);
   pthread_join(can, NULL);
   usleep(50000);

   retro_atomic_store_release_int(&consumer_run, 0);
   retro_atomic_store_release_int(&dev_running, 0);
   audio_driver_pipeline_wake();
   dev_signal();
   pthread_join(cons, NULL);
   pthread_join(dev,  NULL);

   pipeline_down();

   /* Any of the fixture's threads held off for more than a quarter
    * frame - the core's wake, the device's, a canary's on the same
    * cores, or the consumer's answer to a frame - is the host, not the
    * pipe: inconclusive, whichever way the count went. Reads after the
    * joins above. */
   if (     host_stall_ns     > 4000000L
         || dev_worst_late_ns > 4000000L
         || canary_worst_ns   > 4000000L
         || consumer_worst_ns > 4000000L)
   {
      printf("stall: the host held a fixture thread off (core %.1f ms, device"
            " %.1f ms, canary %.1f ms, consumer %.1f ms); retrying\n",
            host_stall_ns / 1e6, dev_worst_late_ns / 1e6,
            canary_worst_ns / 1e6, consumer_worst_ns / 1e6);
      return false;
   }

   printf("stall: %u short pull%s through the stall; after it, with jitter"
         " to 12 ms: %u short pull%s, %.2f ms silence\n",
         (unsigned)before_under, before_under == 1 ? "" : "s",
         (unsigned)(after_under - before_under),
         (after_under - before_under) == 1 ? "" : "s",
         (double)(after_silent - before_silent) / CHANNELS * 1000.0 / OUT_RATE);
   if (!before_under)
   {
      printf("FAIL: the stall did not reach the device; the lane is not exercised\n");
      stall_failures++;
   }
   if (after_under != before_under)
   {
      printf("FAIL: the pipe did not prime again after the stall - jitter inside"
            " the cushion reached the device\n");
      stall_failures++;
   }
   else
      printf("ok: the pipe primed again after the stall\n");
   return true;
}

static void stall_case(void)
{
   int attempt;
   for (attempt = 0; attempt < 10; attempt++)
      if (stall_case_once())
         return;
   printf("stall: the host never left the fixture alone for a run; inconclusive\n");
}

int main(int argc, char **argv)
{
   static const unsigned sweep[] = { 8, 12, 16, 24, 32, 40, 48, 64 };
   double seconds = (argc > 1) ? atof(argv[1]) : 4.0;
   size_t i;

   memset(frame_audio, 0, sizeof(frame_audio));
   for (i = 0; i < 4096 * 2; i++)
      frame_audio[i] = (int16_t)(8000.0 * sin((double)i * 0.05));

   printf("threaded pipeline against a clocked device, %.0f s per setting, %g fps core\n",
         seconds, FPS);
   for (i = 0; i < sizeof(sweep) / sizeof(sweep[0]); i++)
      run_one(sweep[i], seconds);
   stall_case();
   return stall_failures ? 1 : 0;
}
