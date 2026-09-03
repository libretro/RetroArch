/* The threaded audio pipeline against a device that stops taking audio.
 *
 * The producer (the core's thread, in audio_driver_submit()) publishes
 * into a ring; the consumer (the audio thread, in
 * audio_driver_pipeline_consume()) waits for the device to have room,
 * takes a chunk from the ring, and writes it. The contract when the
 * device stops:
 *
 *  - The consumer takes nothing from the ring it cannot deliver, so
 *    the ring stays full, and it does not report progress it did not
 *    make. Taking the chunk and then finding no room threw it away
 *    while telling the producer the ring had drained; the producer
 *    then waited its full bound again on the next frame, pacing the
 *    frontend at one frame per failed pass.
 *  - The producer waits its bound once, finds the ring undrained, sets
 *    pipe_stalled, and from then on drops at once - a frame costs it
 *    nothing beyond the wait that found the stall.
 *  - When the device takes audio again the consumer completes a pass,
 *    clears the stall, and the producer goes back to blocking on the
 *    ring as it should.
 *
 * Includes audio/audio_driver.c so the shipping producer and consumer
 * run. The device is a scripted driver with write_raw(), which is the
 * flush path that reaches the device without the resampler or the
 * arenas, so the consumer runs end-to-end on nothing but the ring,
 * the flags and the driver. Each producer call is timed: the stalled
 * ones have to be immediate. A watchdog aborts a pass that does not
 * return. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

#include <boolean.h>
#include <retro_atomic.h>

#include "../../../audio/audio_driver.c"

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

/* --- scripted device ------------------------------------------------- */

/* While set, the device has no room and takes nothing: wait_writable()
 * reports no space is coming, as a driver does for a stream that is
 * not running. */
static retro_atomic_int_t dev_stalled     = RETRO_ATOMIC_INT_INITIALIZER(0);
static retro_atomic_int_t dev_frames_took = RETRO_ATOMIC_INT_INITIALIZER(0);
static retro_atomic_int_t dev_waits       = RETRO_ATOMIC_INT_INITIALIZER(0);

static void *dev_init(const char *device, unsigned rate, unsigned latency,
      unsigned block_frames, unsigned *new_rate)
{
   static int handle = 1;
   (void)device; (void)latency; (void)block_frames;
   if (new_rate) *new_rate = rate;
   return &handle;
}
static ssize_t dev_write(void *data, const void *buf, size_t size)
{
   (void)data; (void)buf;
   return (ssize_t)size;
}
static ssize_t dev_write_raw(void *data, const int16_t *samples,
      size_t frames, unsigned input_rate, double rate_adjust, float volume)
{
   (void)data; (void)samples; (void)input_rate; (void)rate_adjust;
   (void)volume;
   if (retro_atomic_load_acquire_int(&dev_stalled))
      return 0;
   retro_atomic_fetch_add_int(&dev_frames_took, (int)frames);
   return (ssize_t)frames;
}
static size_t dev_wait_writable(void *data, size_t len)
{
   (void)data;
   retro_atomic_fetch_add_int(&dev_waits, 1);
   if (retro_atomic_load_acquire_int(&dev_stalled))
   {
      /* A real driver sleeps its bound here before giving up; a short
       * sleep keeps that shape without slowing the suite. */
      usleep(2000);
      return 0;
   }
   return len;
}
static bool   dev_stop(void *d)               { (void)d; return true; }
static bool   dev_start(void *d, bool s)      { (void)d; (void)s; return true; }
static bool   dev_alive(void *d)              { (void)d; return true; }
static void   dev_set_nonblock(void *d, bool s){ (void)d; (void)s; }
static void   dev_free(void *d)               { (void)d; }
static bool   dev_use_float(void *d)          { (void)d; return false; }
static size_t dev_write_avail(void *d)        { (void)d; return 16384; }
static size_t dev_buffer_size(void *d)        { (void)d; return 32768; }

static audio_driver_t scripted_driver = {
   dev_init, dev_write, dev_stop, dev_start, dev_alive, dev_set_nonblock,
   dev_free, dev_use_float, "scripted", NULL, NULL, dev_write_avail,
   dev_buffer_size, dev_write_raw, dev_wait_writable
};

/* --- consumer thread ------------------------------------------------- */

static retro_atomic_int_t consumer_run = RETRO_ATOMIC_INT_INITIALIZER(1);

static void *consumer(void *arg)
{
   (void)arg;
   while (retro_atomic_load_acquire_int(&consumer_run))
      audio_driver_pipeline_consume(&audio_driver_st);
   return NULL;
}

/* --- watchdog -------------------------------------------------------- */

static retro_atomic_int_t stage = RETRO_ATOMIC_INT_INITIALIZER(0);
#define STAGE(v) retro_atomic_store_release_int(&stage, (v))

static void *watchdog(void *arg)
{
   int last = retro_atomic_load_acquire_int(&stage);
   int cur;
   (void)arg;
   for (;;)
   {
      sleep(10);
      cur = retro_atomic_load_acquire_int(&stage);
      if (cur == last && cur >= 0)
      {
         fprintf(stderr, "WATCHDOG: stage %d never returned\n", cur);
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

/* --- fixture --------------------------------------------------------- */

/* What audio_driver_init_internal() sets for the pipeline, on a driver
 * that has wait_writable(): the ring, the throttle, the pass size, and
 * the flags the producer and consumer gate on. */
static bool pipeline_up(size_t ring_bytes)
{
   audio_driver_state_t *st = &audio_driver_st;

   memset(st, 0, sizeof(*st));
   st->current_audio      = &scripted_driver;
   st->context_audio_data = scripted_driver.init(NULL, 48000, 64, 0, NULL);
   st->input              = 48000.0;
   st->src_ratio_orig     = 1.0;
   st->src_ratio_curr     = 1.0;
   st->cached_rate_adjust = 1.0;
   st->volume_gain        = 1.0f;
   st->buffer_size        = scripted_driver.buffer_size(st->context_audio_data);
   st->output_samples_buf = (float*)malloc(65536);
   st->pipe_scratch       = (int16_t*)malloc(65536);
   st->pipe_pass_int16s   = 1600;
   if (!retro_spsc_init(&st->pipe_ring, ring_bytes))
      return false;
   st->pipe_lock      = slock_new();
   st->pipe_cond      = scond_new();
   st->pipe_data_cond = scond_new();
   st->state_lock     = slock_new();
   st->pipe_threaded  = true;
   AUDIO_FLAGS_SET(st, AUDIO_FLAG_ACTIVE | AUDIO_FLAG_STARTED
         | AUDIO_FLAG_PIPELINE_THREADED);
   return st->pipe_lock && st->pipe_cond && st->pipe_data_cond
      && st->state_lock && st->output_samples_buf && st->pipe_scratch;
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
}

/* pipe_stalled is written by both threads under pipe_lock; read it the
 * same way, as the production code does. */
static bool stalled_now(void)
{
   bool v;
   slock_lock(audio_driver_st.pipe_lock);
   v = audio_driver_st.pipe_stalled;
   slock_unlock(audio_driver_st.pipe_lock);
   return v;
}

/* One frame's worth of core audio: 800 stereo frames at 48 kHz / 60. */
static int16_t frame_audio[800 * 2];

static double produce_frame(void)
{
   double t0 = now_ms();
   audio_driver_submit(&audio_driver_st, 3.0f, frame_audio,
         sizeof(frame_audio) / sizeof(int16_t), false, false);
   audio_driver_pipeline_signal(&audio_driver_st);
   return now_ms() - t0;
}

int main(void)
{
   pthread_t dog, cons;
   unsigned i;
   double   t, worst;
   int      took_before;

   pthread_create(&dog, NULL, watchdog, NULL);

   /* The ring holds three frames of core audio, as the real one does. */
   if (!pipeline_up(800 * 2 * sizeof(int16_t) * 3))
   {
      printf("FAIL: could not stand the pipeline up\n");
      return 1;
   }
   for (i = 0; i < 800 * 2; i++)
      frame_audio[i] = (int16_t)((i & 1) ? 3000 : -3000);

   pthread_create(&cons, NULL, consumer, NULL);

   /* 1. Healthy: frames flow, the device takes them, no stall. */
   STAGE(1);
   for (i = 0; i < 60; i++)
      produce_frame();
   usleep(20000);
   CHECK(retro_atomic_load_acquire_int(&dev_frames_took) > 0,
         "healthy: the device took nothing");
   CHECK(!stalled_now(), "healthy: pipeline reported a stall");

   /* 2. The device stops. The first frames may pay the producer's one
    *    bounded wait; after that, every frame must be immediate and
    *    the stall must stick - the consumer, finding no room, must not
    *    be clearing it by taking chunks it cannot deliver. */
   STAGE(2);
   retro_atomic_store_release_int(&dev_stalled, 1);
   took_before = retro_atomic_load_acquire_int(&dev_frames_took);
   /* The ring takes a few frames before the producer has to wait; the
    * first wait that finds it undrained is the one that records the
    * stall and costs the bound. Produce until that has happened. */
   for (i = 0; i < 40 && !stalled_now(); i++)
      produce_frame();
   CHECK(stalled_now(), "stalled device: stall not recorded");
   worst = 0.0;
   for (i = 0; i < 120; i++)
   {
      STAGE(3);
      t = produce_frame();
      if (t > worst)
         worst = t;
   }
   CHECK(worst < 50.0,
         "stalled device: a frame cost the producer %.1f ms; it should drop at once",
         worst);
   CHECK(stalled_now(), "stalled device: stall was cleared with no room made");
   CHECK(retro_atomic_load_acquire_int(&dev_frames_took) == took_before,
         "stalled device: the consumer delivered %d frames to a device with no room",
         retro_atomic_load_acquire_int(&dev_frames_took) - took_before);
   CHECK(retro_spsc_read_avail(&audio_driver_st.pipe_ring) > 0,
         "stalled device: the ring was emptied although nothing was delivered");

   /* 3. The device comes back: a pass completes, the stall clears,
    *    audio flows and the producer is throttled by the ring again. */
   STAGE(4);
   retro_atomic_store_release_int(&dev_stalled, 0);
   for (i = 0; i < 60; i++)
      produce_frame();
   usleep(20000);
   CHECK(!stalled_now(), "resumed device: stall not cleared");
   CHECK(retro_atomic_load_acquire_int(&dev_frames_took) > took_before,
         "resumed device: nothing delivered after the device came back");

   /* 4. Stall again, to be sure the second round behaves like the
    *    first and nothing latched. */
   STAGE(5);
   retro_atomic_store_release_int(&dev_stalled, 1);
   for (i = 0; i < 40 && !stalled_now(); i++)
      produce_frame();
   worst = 0.0;
   for (i = 0; i < 60; i++)
   {
      t = produce_frame();
      if (t > worst)
         worst = t;
   }
   CHECK(stalled_now(), "second stall: not recorded");
   CHECK(worst < 50.0, "second stall: a frame cost %.1f ms", worst);

   STAGE(6);
   retro_atomic_store_release_int(&dev_stalled, 0);
   retro_atomic_store_release_int(&consumer_run, 0);
   audio_driver_pipeline_wake();
   pthread_join(cons, NULL);
   pipeline_down();

   STAGE(-1);
   if (failures)
   {
      printf("%u failure(s)\n", failures);
      return 1;
   }
   printf("pipeline stall: consumer takes only what it can deliver; producer drops at once once stalled\n");
   return 0;
}
