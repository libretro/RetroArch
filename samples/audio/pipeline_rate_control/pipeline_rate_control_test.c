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

/* The sink estimate runs short here - windows of 0.4 s, a baseline of
 * 3 s - so a seven-second real-time run reaches an application. */
#define AUDIO_SINK_WINDOW_USEC     400000
#define AUDIO_SINK_BASELINE_USEC  3000000
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
static size_t DEV_CAPACITY = 384; /* frames: 8 ms; 3072 for a 64 ms default */

static pthread_mutex_t dev_lock = PTHREAD_MUTEX_INITIALIZER;
static double dev_fill;           /* frames, fractional */
static double dev_last;           /* when dev_fill was last brought up to date */
static double dev_took;           /* frames accepted, total */
static double dev_adjust_sum;     /* rate adjustments handed to write_raw */
static unsigned dev_adjust_n;

/* Bring the fill up to now: the hardware drains at DEV_RATE. */
static double dev_underrun;       /* frames of silence the hardware played */
static size_t dev_underrun_events;/* times it ran dry, as a driver counts periods */

static void dev_drain_locked(void)
{
   double t = now_s();
   double before = dev_fill;
   dev_fill -= (t - dev_last) * DEV_RATE;
   if (dev_fill < 0.0)
   {
      dev_underrun -= dev_fill;
      if (before > 0.0)
         dev_underrun_events++;
      dev_fill = 0.0;
   }
   dev_last  = t;
}

static size_t dev_underruns(void *d)
{
   size_t n;
   (void)d;
   pthread_mutex_lock(&dev_lock);
   dev_drain_locked();
   n = dev_underrun_events;
   pthread_mutex_unlock(&dev_lock);
   return n;
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

/* What the hardware has consumed, for the sink estimate: the drain
 * counted in whole 96-frame periods, as a device counts it. */
static size_t dev_frames_consumed(void *d)
{
   double drained;
   (void)d;
   pthread_mutex_lock(&dev_lock);
   dev_drain_locked();
   drained = dev_took - dev_fill;
   pthread_mutex_unlock(&dev_lock);
   if (drained < 0.0)
      drained = 0.0;
   return ((size_t)drained / 96) * 96;
}

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
   dev_buffer_size, dev_write_raw, dev_wait_writable, dev_frames_consumed,
   dev_underruns
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

static double dbg_pipe, dbg_avail, dbg_eff; static unsigned dbg_n;
static bool sync_on = false;   /* audio sync: the producer's flag and the setting */
static bool jitter  = false;   /* a core that delivers late now and then */

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
   st->sink_bias            = 1.0;
   config_get_ptr()->bools.audio_sink_rate_estimation = true;
   config_get_ptr()->uints.audio_output_sample_rate   = 48000;
   config_get_ptr()->bools.audio_sync                 = sync_on;
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
   if (!sync_on)
      AUDIO_FLAGS_SET(st, AUDIO_FLAG_NONBLOCK);
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
   if (argc > 3 && strstr(argv[3], "sync"))
      sync_on = true;
   if (argc > 3 && strstr(argv[3], "jitter"))
      jitter = true;
   /* The 64 ms default buffer: the consumer's chunks are whole
    * publishes, and the pipe's fill swings by one with the phase
    * between the core and the consumer. A threshold on that fill
    * dropped healthy audio at some phases; nothing may be dropped
    * from a steady core at any phase. */
   if (argc > 3 && strstr(argv[3], "big"))
      DEV_CAPACITY = 3072;
   double    t0, produced, mean;

   /* The ring as the frontend sizes it: three publishes, and with audio
    * sync off a buffer's worth on top. */
   if (!pipeline_up((800 * 3 + (sync_on ? 0 : (DEV_CAPACITY > 800 ? DEV_CAPACITY : 800) * 2)) * 2 * sizeof(int16_t)))
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
      /* A late frame: one in every 120 takes 60 ms - three frames'
       * worth - as an unstable core does, and the loop, its schedule
       * kept, delivers the next two as soon as it can. */
      if (jitter && i % 120 == 60)
         sleep_until(t0 + (double)i / 60.0 + 0.060);
      else
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
         dev_underrun   = 0.0;
         pthread_mutex_unlock(&dev_lock);
      }
      if (i >= warm)
      {
         dbg_pipe  += (double)retro_spsc_read_avail(&audio_driver_st.pipe_ring) / 4.0;
         dbg_avail += (double)dev_write_avail(NULL) / 4.0;
         dbg_eff   += (double)retro_atomic_load_acquire_int(&audio_driver_st.pipe_ctrl_avail) / 4.0;
         dbg_n++;
      }
      audio_driver_submit(&audio_driver_st, 3.0f, frame_audio,
            sizeof(frame_audio) / sizeof(int16_t), false, false);
      audio_driver_pipeline_signal(&audio_driver_st);
   }
   if (dbg_n)
      printf("   pre-publish: pipe %.0f frames, device free %.0f frames, controller free %.0f frames (setpoint %u, target %u)\n",
            dbg_pipe / dbg_n, dbg_avail / dbg_n, dbg_eff / dbg_n, (unsigned)(DEV_CAPACITY / 2),
            (unsigned)audio_driver_pipe_target_frames(&audio_driver_st));
   sleep_until(t0 + (double)frames / 60.0 + 0.05);

   retro_atomic_store_release_int(&consumer_run, 0);
   audio_driver_pipeline_signal(&audio_driver_st);
   pthread_join(cons, NULL);

   produced = (double)(frames - warm) * 800.0;
   pthread_mutex_lock(&dev_lock);
   printf("   %u ms device, audio sync %s, %s: the hardware played %.1f ms of silence over %.0f s, ran dry %u times\n",
         (unsigned)(DEV_CAPACITY * 1000 / (size_t)DEV_RATE),
         sync_on ? "on " : "off", jitter ? "a core late by 60 ms every 2 s" : "a steady core",
         dev_underrun * 1000.0 / DEV_RATE, (double)(frames - warm) / 60.0, (unsigned)dev_underrun_events);
   mean = dev_adjust_n ? dev_adjust_sum / dev_adjust_n : 0.0;
   printf("   %u passes; mean rate adjustment %+.0f ppm; device took %.1f%% of %.0f frames\n",
         dev_adjust_n, (mean - 1.0) * 1e6, 100.0 * dev_took / produced, produced);
   CHECK(dev_adjust_n > 0, "the consumer ran");
   if (!jitter)
   {
      CHECK(fabs(mean - 1.0) < 500e-6,
            "the mean rate adjustment is within 500 ppm of 1.0: %+.0f ppm", (mean - 1.0) * 1e6);
      CHECK(dev_took >= produced * 0.995,
            "the device took at least 99.5%% of what was produced: %.2f%%", 100.0 * dev_took / produced);
      CHECK(dev_underrun < DEV_RATE * 0.002,
            "a steady core underran the device: %.1f ms of silence", dev_underrun * 1000.0 / DEV_RATE);
   }
   else
   {
      /* A late core: the silence is the stalls' own, less the margin,
       * and no more - the audio that arrives late is not kept, so the
       * output does not fall behind and the controller is not pinned
       * against a backlog. With audio sync on the writer waits instead,
       * the backlog is the game slowing, and nothing is dropped. */
      double stalls = (double)(frames - warm) / 120.0;
      CHECK(dev_underrun * 1000.0 / DEV_RATE < stalls * 60.0,
            "more silence than the stalls themselves: %.1f ms over %.0f stalls", dev_underrun * 1000.0 / DEV_RATE, stalls);
      /* Pinned high is the pipe refilling to its target after the
       * silence, at rate control's pace; pinned low is a backlog of
       * late audio it can never drain, which the discard prevents. */
      if (!sync_on)
         CHECK(mean - 1.0 > -4000e-6,
               "rate control pinned against a backlog of late audio: %+.0f ppm", (mean - 1.0) * 1e6);
      else
         CHECK(dev_took >= produced * 0.995,
               "with audio sync on, late audio was dropped: %.2f%%", 100.0 * dev_took / produced);
   }
   pthread_mutex_unlock(&dev_lock);

   /* The sink estimate, run on the producer with windows of whole
    * publishes: the clocks match, so it settles, applies, and finds
    * about nothing. Closed on the consumer it never settled - a window
    * held a fraction of a burst, thousands of ppm of phase noise - and
    * the bias stayed at zero for the session. */
   printf("   sink estimate: applied %u time(s), bias %+.0f ppm, source shown at %+.0f ppm\n",
         audio_driver_st.sink_applied, (audio_driver_st.sink_bias - 1.0) * 1e6,
         (audio_driver_st.sink_source_hz / 48000.0 - 1.0) * 1e6);
   if (!jitter)
      CHECK(audio_driver_st.sink_applied > 0, "the sink estimate never settled on the threaded pipeline");
   /* Within the device's period over the short baseline: 96 frames in
    * 3 s is 667 ppm of noise, which the real thirty seconds and the
    * session's sum reduce to tens. */
   CHECK(fabs(audio_driver_st.sink_bias - 1.0) < 700e-6,
         "the sink estimate found a clock that is not there: %+.0f ppm", (audio_driver_st.sink_bias - 1.0) * 1e6);

   if (failures)
   {
      printf("%u failure(s)\n", failures);
      return 1;
   }
   printf("pipeline rate control: on pipe and device together, the controller holds the ratio at the clocks and the pipe holds\n");
   return 0;
}
