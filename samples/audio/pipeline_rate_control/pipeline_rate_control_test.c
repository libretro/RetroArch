/* Rate control on the threaded audio pipeline, against a device whose
 * fill drains in real time.
 *
 * The consumer waits for half the device's buffer and writes half, so
 * the device's fill sits between half and full whatever the clocks do;
 * a controller reading it, at any point of the pass, sees a constant
 * error and pins the ratio at a bound - read after the wait it is
 * never above half, read before it is never below. What the clocks
 * move is the pipe ring in front of the device. Rate control's fill is
 * the two together, sampled by the producer before each publish; see
 * pipe_ctrl_avail in audio_driver.h.
 *
 * The device here is what a real driver's timing looks like: a 384-
 * frame buffer (8 ms at 48 kHz) that the "hardware" drains at exactly
 * 48000 frames a second of wall-clock time; write_raw() puts frames in,
 * scaled by the rate adjustment it is handed as a resampling driver
 * would; wait_writable() sleeps until there is room for min(len, half).
 * The producer publishes one 800-frame video frame every 1/60 s on an
 * absolute schedule, so production and consumption match to the clock
 * and the right ratio is 1.0.
 *
 * Includes audio/audio_driver.c so the shipping producer, consumer and
 * controller run. The device records every rate adjustment it is
 * handed; over three seconds after a four-second settle their mean
 * must be within 500 ppm of 1.0,
 * and the device must have taken at least 99.5% of what the producer
 * published - the rest being the start-up transient. Reading the
 * device alone, the mean sits at a bound - +delta after the wait,
 * -delta before it - and the pipe ring overflows behind it. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <math.h>

#include <boolean.h>
#include <retro_atomic.h>

#include "../../../audio/audio_driver.c"

static unsigned failures = 0;

#define CHECK(c, ...) do { if (!(c)) { printf("FAIL: "); printf(__VA_ARGS__); printf("\n"); failures++; } } while (0)

/* --- the clock ------------------------------------------------------- */

static double now_s(void)
{
   struct timespec ts;
   clock_gettime(CLOCK_MONOTONIC, &ts);
   return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

static void sleep_until(double t)
{
   struct timespec ts;
   double d = t - now_s();
   if (d <= 0.0)
      return;
   ts.tv_sec  = (time_t)d;
   ts.tv_nsec = (long)((d - (double)ts.tv_sec) * 1e9);
   nanosleep(&ts, NULL);
}

/* --- the device ------------------------------------------------------ */

#define DEV_RATE     48000.0
#define DEV_CAPACITY 384          /* frames: 8 ms */

static pthread_mutex_t dev_lock = PTHREAD_MUTEX_INITIALIZER;
static double dev_fill;           /* frames, fractional */
static double dev_last;           /* when dev_fill was last brought up to date */
static double dev_took;           /* frames accepted, total */
static double dev_adjust_sum;     /* rate adjustments handed to write_raw */
static unsigned dev_adjust_n;

/* Bring the fill up to now: the hardware drains at DEV_RATE. */
static void dev_drain_locked(void)
{
   double t = now_s();
   dev_fill -= (t - dev_last) * DEV_RATE;
   if (dev_fill < 0.0)
      dev_fill = 0.0;
   dev_last  = t;
}

static void *dev_init(const char *device, unsigned rate, unsigned latency,
      unsigned block_frames, unsigned *new_rate)
{
   static int handle = 1;
   (void)device; (void)rate; (void)latency; (void)block_frames; (void)new_rate;
   dev_last = now_s();
   return &handle;
}

static ssize_t dev_write(void *data, const void *buf, size_t size)
{
   (void)data; (void)buf; (void)size;
   return -1;
}

static ssize_t dev_write_raw(void *data, const int16_t *samples,
      size_t frames, unsigned input_rate, double rate_adjust, float gain)
{
   double want, room, put;
   (void)data; (void)samples; (void)input_rate; (void)gain;
   pthread_mutex_lock(&dev_lock);
   dev_drain_locked();
   want = (double)frames * rate_adjust;
   room = DEV_CAPACITY - dev_fill;
   put  = (want < room) ? want : room;
   if (put < 0.0)
      put = 0.0;
   dev_fill       += put;
   dev_took       += put;
   dev_adjust_sum += rate_adjust;
   dev_adjust_n++;
   pthread_mutex_unlock(&dev_lock);
   return (ssize_t)put;
}

static size_t dev_write_avail(void *d)
{
   double room;
   (void)d;
   pthread_mutex_lock(&dev_lock);
   dev_drain_locked();
   room = DEV_CAPACITY - dev_fill;
   pthread_mutex_unlock(&dev_lock);
   return (size_t)room * 4;
}

static size_t dev_buffer_size(void *d) { (void)d; return DEV_CAPACITY * 4; }

static size_t dev_wait_writable(void *data, size_t len)
{
   size_t want = len / 4;
   size_t half = DEV_CAPACITY / 2;
   size_t room;
   if (want > half)
      want = half;
   for (;;)
   {
      room = dev_write_avail(data) / 4;
      if (room >= want)
         return room * 4;
      usleep(100);
   }
}

static bool   dev_stop(void *d)               { (void)d; return true; }
static bool   dev_start(void *d, bool s)      { (void)d; (void)s; return true; }
static bool   dev_alive(void *d)              { (void)d; return true; }
static void   dev_set_nonblock(void *d, bool s){ (void)d; (void)s; }
static void   dev_free(void *d)               { (void)d; }
static bool   dev_use_float(void *d)          { (void)d; return false; }

static audio_driver_t scripted_driver = {
   dev_init, dev_write, dev_stop, dev_start, dev_alive, dev_set_nonblock,
   dev_free, dev_use_float, "scripted", NULL, NULL, dev_write_avail,
   dev_buffer_size, dev_write_raw, dev_wait_writable
};

/* --- the consumer thread ---------------------------------------------- */

static retro_atomic_int_t consumer_run = RETRO_ATOMIC_INT_INITIALIZER(1);

static void *consumer(void *arg)
{
   (void)arg;
   while (retro_atomic_load_acquire_int(&consumer_run))
      audio_driver_pipeline_consume(&audio_driver_st);
   return NULL;
}

/* --- fixture --------------------------------------------------------- */

static bool pipeline_up(size_t ring_bytes)
{
   audio_driver_state_t *st = &audio_driver_st;

   memset(st, 0, sizeof(*st));
   st->current_audio        = &scripted_driver;
   st->context_audio_data   = scripted_driver.init(NULL, 48000, 8, 0, NULL);
   st->input                = 48000.0;
   st->src_ratio_orig       = 1.0;
   st->src_ratio_curr       = 1.0;
   st->cached_rate_adjust   = 1.0;
   st->volume_gain          = 1.0f;
   st->buffer_size          = scripted_driver.buffer_size(st->context_audio_data);
   st->output_samples_buf   = (float*)malloc(65536);
   st->pipe_scratch         = (int16_t*)malloc(65536);
   st->pipe_pass_int16s     = 1600;
   retro_atomic_store_release_int(&st->pipe_ctrl_avail, -1);
   st->rate_control_delta   = 0.005f;
   st->drc_threshold_int16s = 1600;
   if (!retro_spsc_init(&st->pipe_ring, ring_bytes))
      return false;
   st->pipe_lock      = slock_new();
   st->pipe_cond      = scond_new();
   st->pipe_data_cond = scond_new();
   st->state_lock     = slock_new();
   st->pipe_threaded  = true;
   AUDIO_FLAGS_SET(st, AUDIO_FLAG_ACTIVE | AUDIO_FLAG_STARTED
         | AUDIO_FLAG_PIPELINE_THREADED | AUDIO_FLAG_CONTROL
         | AUDIO_FLAG_NONBLOCK);
   return st->pipe_lock && st->pipe_cond && st->pipe_data_cond
      && st->state_lock && st->output_samples_buf && st->pipe_scratch;
}

static int16_t frame_audio[800 * 2];

int main(int argc, char **argv)
{
   pthread_t cons;
   unsigned  i, frames = 420, warm = 240;
   if (argc > 1)
      frames = (unsigned)atoi(argv[1]) * 60;
   if (argc > 2)
      warm   = (unsigned)atoi(argv[2]) * 60;
   double    t0, produced, mean;

   if (!pipeline_up(800 * 2 * sizeof(int16_t) * 5))
   {
      printf("FAIL: could not stand the pipeline up\n");
      return 1;
   }
   for (i = 0; i < 800 * 2; i++)
      frame_audio[i] = (int16_t)((i & 1) ? 3000 : -3000);

   pthread_create(&cons, NULL, consumer, NULL);

   /* Three seconds of frames on an absolute 60 Hz schedule, audio sync
    * off: the producer never blocks, so anything the pipe cannot hold
    * is dropped, as it is in the frontend. */
   t0 = now_s();
   for (i = 0; i < frames; i++)
   {
      sleep_until(t0 + (double)i / 60.0);
      /* The settle: the device fills from empty, the controller pushes
       * to fill it, and the pipe - a large integrator against the
       * controller's gain - takes a few seconds to centre. Measured
       * from there on. */
      if (i == warm)
      {
         pthread_mutex_lock(&dev_lock);
         dev_adjust_sum = 0.0;
         dev_adjust_n   = 0;
         dev_took       = 0.0;
         pthread_mutex_unlock(&dev_lock);
      }
      audio_driver_submit(&audio_driver_st, 3.0f, frame_audio,
            sizeof(frame_audio) / sizeof(int16_t), false, false);
      audio_driver_pipeline_signal(&audio_driver_st);
   }
   sleep_until(t0 + (double)frames / 60.0 + 0.05);

   retro_atomic_store_release_int(&consumer_run, 0);
   audio_driver_pipeline_signal(&audio_driver_st);
   pthread_join(cons, NULL);

   produced = (double)(frames - warm) * 800.0;
   pthread_mutex_lock(&dev_lock);
   mean = dev_adjust_n ? dev_adjust_sum / dev_adjust_n : 0.0;
   printf("   %u passes; mean rate adjustment %+.0f ppm; device took %.1f%% of %.0f frames\n",
         dev_adjust_n, (mean - 1.0) * 1e6, 100.0 * dev_took / produced, produced);
   CHECK(dev_adjust_n > 0, "the consumer ran");
   CHECK(fabs(mean - 1.0) < 500e-6,
         "the mean rate adjustment is within 500 ppm of 1.0: %+.0f ppm", (mean - 1.0) * 1e6);
   CHECK(dev_took >= produced * 0.995,
         "the device took at least 99.5%% of what was produced: %.2f%%", 100.0 * dev_took / produced);
   pthread_mutex_unlock(&dev_lock);

   if (failures)
   {
      printf("%u failure(s)\n", failures);
      return 1;
   }
   printf("pipeline rate control: on pipe and device together, the controller holds the ratio at the clocks and the pipe holds\n");
   return 0;
}
