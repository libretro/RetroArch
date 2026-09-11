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
   config_get_ptr()->bools.audio_sync                 = true;

   ring_bytes = per_frame * 3 * st->pipe_frame_bytes;
   if (!retro_spsc_init(&st->pipe_ring, ring_bytes))
      return false;
   st->pipe_lock      = slock_new();
   st->pipe_cond      = scond_new();
   st->pipe_data_cond = scond_new();
   st->state_lock     = slock_new();
   st->pipe_threaded  = true;
   st->pipe_priming   = true;
   AUDIO_FLAGS_SET(st, AUDIO_FLAG_ACTIVE | AUDIO_FLAG_STARTED
         | AUDIO_FLAG_PIPELINE_THREADED | AUDIO_FLAG_CONTROL);
   return st->pipe_lock && st->pipe_cond && st->pipe_data_cond
      && st->state_lock && st->output_samples_buf && st->pipe_scratch
      && st->input_data && st->synth_buf && dev_ring;
}

static void pipeline_down(void)
{
   audio_driver_state_t *st = &audio_driver_st;
   retro_spsc_free(&st->pipe_ring);
   slock_free(st->pipe_lock);
   scond_free(st->pipe_cond);
   scond_free(st->pipe_data_cond);
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
            per_frame * 2, false, false, false);
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
   return 0;
}
