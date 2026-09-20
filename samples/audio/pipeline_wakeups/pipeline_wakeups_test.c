/* What the threaded audio pipeline's handshake costs, as publish
 * granularity varies.
 *
 * The other pipeline harnesses answer correctness questions: does the
 * consumer take only what it can deliver, does the producer drop once
 * stalled, does a clocked device underrun. None of them can tell a
 * handshake that wakes the consumer once a frame from one that wakes it
 * once a publish, because both produce the same audio. The difference
 * is a cost, and only a count of wakes can see it.
 *
 * The producer hands over one video frame's audio as N publishes and
 * signals at the frame end - N = 1 is a core that calls audio_batch_cb
 * once per retro_run, N = 262 is one that calls it per scanline. Every
 * setting submits identical audio at an identical rate; only the
 * granularity differs.
 *
 * Reported per setting:
 *
 *   wakes/frame    returns from audio_driver_callback(). What
 *                  the handshake costs the consumer thread.
 *   writes/frame   calls that reached the device, as against wakes that
 *                  found nothing.
 *   wake latency   frame-end signal to the next device write, p50/p99.
 *   producer       microseconds the main thread spent inside
 *                  audio_driver_submit(), and how many frames it
 *                  blocked in at all.
 *   short pulls    the device's own count, so a cheaper handshake that
 *                  buys its cheapness with underruns is visible here
 *                  rather than only in pipeline_clocked.
 *
 * In the legacy consumer, three things had to be true before a per-publish notify cost
 * anything measurable, which is worth knowing before reading a flat
 * column as proof that nothing is wrong:
 *
 *   - the device must not be the pacer. With backpressure the consumer
 *     parks inside wait_writable(), not on the data handshake, and
 *     extra notifies land on a thread that is not asleep. That is the
 *     steady state of Audio Sync against a real card, and a wake count
 *     taken only there says nothing about the handshake at all. Both
 *     regimes are run.
 *   - the publishes must be spread across the frame. Emitted in a
 *     burst, as a fast core's retro_run emits them, all N are in the
 *     ring before the consumer finishes its first pass and it takes
 *     the lot in one go - N notifies, one wake. SPREAD=1 paces them
 *     across the frame, which is the slow core, and then every notify
 *     finds an empty ring and a parked consumer.
 *   - the notify must actually be per publish. NOTIFY_PER_PUBLISH=1.
 *
 * With all three: 3.0 wakes/frame becomes 211 at N = 312, and the
 * producer's own cost goes up about sixteenfold with it. With any one
 * of them missing the column stays flat. So the trap is real and it is
 * narrower than "a core that batches per scanline" - it wants a core
 * that batches per scanline and is slow enough to spread them, against
 * a driver that is not applying backpressure.
 *
 * There is no performance pass/fail threshold. The numbers compare a
 * changed handshake against, and the comparison is the test - run it
 * before and after. It is a measurement, so expect the last digit to
 * move between runs and run it on an idle machine.
 *
 * Includes audio/audio_driver.c so the shipping producer and consumer
 * run, and reads none of their internals: every number is taken at the
 * harness's own boundary - its consumer loop, its device, its calls
 * into the producer. A change of primitive underneath does not change
 * what is being counted, which is the point of measuring it from out
 * here.
 *
 * The fixture defaults to 48 kHz stereo float out, a ring of
 * the latency setting, a device period of a quarter of it, Audio Sync
 * on, rate control on. LAYOUT=5.1 or 7.1 exercises the canonical wide
 * source ring and discrete device output through the real wrapper.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

#include <boolean.h>
#include <retro_atomic.h>
#include <audio/conversion/float_to_s16.h>
#include <audio/conversion/s16_to_float.h>

static bool track_conversions;
static retro_atomic_size_t to_float;
static retro_atomic_size_t to_int16;

static void counted_to_float(float *out, const int16_t *in, size_t n, float gain)
{
   if (track_conversions) retro_atomic_fetch_add_size(&to_float, n);
   convert_s16_to_float(out, in, n, gain);
}

static void counted_to_int16(int16_t *out, const float *in, size_t n)
{
   if (track_conversions) retro_atomic_fetch_add_size(&to_int16, n);
   convert_float_to_s16(out, in, n);
}

#define audio_driver_callback pipeline_callback_impl
#define convert_s16_to_float counted_to_float
#define convert_float_to_s16 counted_to_int16
#include "../../../audio/audio_driver.c"
#undef audio_driver_callback
#undef convert_s16_to_float
#undef convert_float_to_s16

static retro_atomic_size_t int16_src_frames;

static void counted_int16_src(void *state, struct resampler_data_int16 *data)
{
   if (track_conversions)
      retro_atomic_fetch_add_size(&int16_src_frames, data->input_frames);
   sinc_resampler_int16_process(state, data);
}

#define OUT_RATE      48000
#define CORE_RATE     48000
#define FPS           60.0
static unsigned channels = 2;
static unsigned source_channels = 2;
static uint32_t source_layout = AUDIO_LAYOUT_STEREO;
static bool live_layouts;
static bool speed_lowpass;
static bool runloop_policy;
static bool auto_runloop;
#define LATENCY_MS    32
#define MAX_SAMPLES   65536

/* --- the device ------------------------------------------------------ */

static float              *dev_ring;
static bool                device_int16;
static size_t              device_sample_bytes = sizeof(float);
static size_t              dev_usable;
static size_t              dev_capacity;
static size_t              dev_period;
static size_t              dev_write_ptr;
static size_t              dev_read_ptr;
static retro_atomic_size_t dev_filled;
static retro_atomic_size_t dev_underruns;
static retro_atomic_size_t dev_pulls;
static retro_atomic_size_t dev_silent_samples;
static pthread_mutex_t     dev_wake_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t      dev_wake_cond = PTHREAD_COND_INITIALIZER;
static retro_atomic_int_t  dev_waiters   = RETRO_ATOMIC_INT_INITIALIZER(0);
static retro_atomic_int_t  dev_running   = RETRO_ATOMIC_INT_INITIALIZER(1);

/* --- the counters ---------------------------------------------------- */

static retro_atomic_size_t cnt_wakes;      /* returns from consume()      */
static retro_atomic_size_t cnt_writes;     /* calls that reached write()  */

/* Frame-end signal, published by the main thread for the consumer's
 * first write to time itself against. One writer and one reader, so the
 * release store of the sequence after the timestamp and the acquire
 * load of it before reading the timestamp are the whole protocol: a
 * reader that sees the new sequence sees the timestamp that went with
 * it. Zero means "already answered", so a pass that writes twice for
 * one signal only records the first. */
static retro_atomic_size_t sig_us;   /* retro_time_t as size_t: the
                                      * overwrite for the next signal
                                      * must not race the read of the
                                      * last one */
static retro_atomic_size_t sig_seq;
static size_t              sig_seen;       /* consumer thread only        */

static retro_time_t       *lat_us;         /* one per answered signal     */
static retro_atomic_size_t lat_count;

/* Producer cost, main thread only - no atomics needed. */
static retro_time_t        prod_total_us;
static size_t              prod_blocked_frames;
static retro_time_t        prod_worst_us;

/* NOTIFY_PER_PUBLISH=1 in the environment. */
static bool                notify_per_publish;

/* The device as a pacer, or not. With backpressure the device's
 * wait_writable() is what the consumer parks in, and the data handshake
 * is almost never the thing holding it - which is the steady state of
 * Audio Sync against a real card, and the reason a wake-count taken
 * only there says nothing about the handshake. Without it the consumer
 * parks on data alone and the handshake is the only pacer left. Both
 * are run: the first is the configuration people use, the second is the
 * one that can see the handshake at all. */
static bool                dev_backpressure = true;
/* Set to make the device refuse every further write, which is how the
 * wrapper is made to leave its loop on its own rather than because the
 * main thread tore it down. */
static retro_atomic_int_t  dev_fail_now = RETRO_ATOMIC_INT_INITIALIZER(0);

/* Whether the frame's publishes arrive in a burst or spread across the
 * frame. A core's retro_run emits the whole frame's audio inside one
 * call, so a burst is the honest default - but how long that call takes
 * decides whether the publishes land microseconds or milliseconds
 * apart, and a notify only costs a wake when it finds the consumer
 * parked. SPREAD=1 is the slow core: publishes paced across most of the
 * frame period, every one of them arriving at an empty ring. */
static bool                spread_publishes;

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

static void dev_render(void)
{
   size_t needed = dev_period * channels;
   size_t avail  = retro_atomic_load_acquire_size(&dev_filled);
   size_t take;

   if (avail > needed)
      avail = needed;
   avail -= avail % channels;
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
   struct timespec next;
   long step_ns = (long)((double)dev_period * 1e9 / (double)OUT_RATE);
   (void)arg;
   while (retro_atomic_load_acquire_int(&dev_running)
         && retro_atomic_load_acquire_size(&dev_filled) < dev_period * channels)
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

/* --- the driver interface -------------------------------------------- */

static void *cdev_init(const char *device, unsigned rate, unsigned latency,
      unsigned *new_rate)
{
   static int handle = 1;
   (void)device; (void)latency;
   if (new_rate) *new_rate = rate;
   return &handle;
}

/* The only place the harness times the consumer: a write is the first
 * point at which audio the producer signalled for has reached the
 * device, so the gap from the signal to here is what the handshake's
 * batching costs in delay. */
static void note_write(void)
{
   size_t seq = retro_atomic_load_acquire_size(&sig_seq);
   retro_atomic_fetch_add_size(&cnt_writes, 1);
   if (seq != sig_seen)
   {
      retro_time_t at = (retro_time_t)retro_atomic_load_relaxed_size(&sig_us);
      size_t       n  = retro_atomic_load_relaxed_size(&lat_count);
      sig_seen        = seq;
      if (at && n < MAX_SAMPLES)
      {
         retro_time_t now = cpu_features_get_time_usec();
         lat_us[n]        = (now > at) ? now - at : 0;
         retro_atomic_store_release_size(&lat_count, n + 1);
      }
   }
}

/* Content tap for the pause-boundary fixture: what actually reaches the
 * device, as magnitudes. tap_stale counts samples at the stale content's
 * level arriving after the pause; tap_head keeps the first samples after
 * the resume, where the ramp must be. */
#define TAP_HEAD_MAX 512
static retro_atomic_int_t  tap_on;
static retro_atomic_int_t  tap_record;
static retro_atomic_int_t  tap_stale;
static float               tap_head[TAP_HEAD_MAX];
static retro_atomic_size_t tap_head_n;

static void tap_samples(const void *buf, size_t samples)
{
   int on  = retro_atomic_load_acquire_int(&tap_on);
   int rec = retro_atomic_load_acquire_int(&tap_record);
   size_t i;
   if (!on && !rec)
      return;
   for (i = 0; i < samples; i++)
   {
      float m = (device_sample_bytes == sizeof(int16_t))
            ? (float)((const int16_t*)buf)[i] / 32768.0f
            : ((const float*)buf)[i];
      if (m < 0.0f) m = -m;
      if (on && m > 0.6f)
         retro_atomic_fetch_add_int(&tap_stale, 1);
      if (rec)
      {
         size_t n = retro_atomic_load_relaxed_size(&tap_head_n);
         /* The resume top-up writes the device full of silence first;
          * the ramp is on the stream behind it, so the window opens at
          * the first audible sample. */
         if (!n && m < 0.005f)
            continue;
         if (n < TAP_HEAD_MAX)
         {
            tap_head[n] = m;
            retro_atomic_store_release_size(&tap_head_n, n + 1);
         }
      }
   }
}

static ssize_t cdev_write(void *data, const void *buf, size_t len)
{
   size_t samples = len / device_sample_bytes;
   size_t written = 0;
   int    laps    = 8;
   (void)data;

   if (retro_atomic_load_acquire_int(&dev_fail_now))
      return -1;

   note_write();
   tap_samples(buf, samples);

   /* No backpressure: take it all, keep what the ring has room for and
    * drop the rest. A driver that never makes the caller wait. */
   if (!dev_backpressure)
   {
      size_t room = dev_rb_write_avail();
      size_t take = (room < samples) ? room : samples;
      take -= take % channels;
      if (take)
      {
         dev_write_ptr = (dev_write_ptr + take) & (dev_capacity - 1);
         retro_atomic_fetch_add_size(&dev_filled, take);
      }
      return (ssize_t)len;
   }

   while (samples > 0)
   {
      size_t avail    = dev_rb_write_avail();
      size_t to_write = (avail < samples) ? avail : samples;
      to_write       -= to_write % channels;
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
   return (ssize_t)(written * device_sample_bytes);
}

static size_t cdev_wait_writable(void *data, size_t len)
{
   size_t want = len / device_sample_bytes;
   int    laps = 8;
   (void)data;
   if (want % channels)
      want += channels - want % channels;
   if (want > dev_usable)
      want = dev_usable;
   if (!dev_backpressure)
      return len;
   for (;;)
   {
      size_t avail = dev_rb_write_avail();
      if (avail >= want)
         return avail * device_sample_bytes;
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
static bool   cdev_use_float(void *d)           { (void)d; return !device_int16; }
static size_t cdev_write_avail(void *d)         { (void)d; return dev_rb_write_avail() * device_sample_bytes; }
static size_t cdev_buffer_size(void *d)         { (void)d; return dev_usable * device_sample_bytes; }
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
/* TRANSPORT=dry|lpf|stretch compares the optional native consumer. */
static unsigned transport_mode;
static unsigned fixture_failures;
static bool use_wrapper;
static bool source_float;
static bool live_controls;
static double source_tempo = 1.0;
static uint32_t tempo_q16 = 65536;
static retro_atomic_int_t in_callback = RETRO_ATOMIC_INT_INITIALIZER(0);

/* The consumer must take everything it consults from the published
 * snapshot; a settings read inside the callback is a read of main-owned
 * state from the audio thread. The config_get_ptr() stub counts calls
 * made while this flag is up, whichever thread runs the callback, and
 * the count is a fixture failure. */
extern __thread int   consumer_context;
extern retro_atomic_size_t consumer_settings_reads;

bool audio_driver_callback(void)
{
   bool result;
   if (use_wrapper) retro_atomic_store_release_int(&in_callback, 1);
   consumer_context = 1;
   result = pipeline_callback_impl();
   consumer_context = 0;
   retro_atomic_fetch_add_size(&cnt_wakes, 1);
   if (use_wrapper) retro_atomic_store_release_int(&in_callback, 0);
   return result;
}

/* Every return from the callback is one wake: the call either parked and
 * came back, or found work without parking. Counting the loop rather
 * than anything inside the driver is what keeps this number meaningful
 * across a change of primitive. */
static void *consumer(void *arg)
{
   (void)arg;
   while (retro_atomic_load_acquire_int(&consumer_run))
   {
      audio_driver_callback();
   }
   return NULL;
}

static int16_t frame_audio[32768 * 8];
static float frame_audio_float[32768 * 8];

/* --- fixture --------------------------------------------------------- */

static bool prepare_transport(bool reset)
{
   if (auto_runloop)
   {
      config_get_ptr()->bools.audio_time_stretch = true;
      config_get_ptr()->bools.audio_time_stretch_lowpass = speed_lowpass;
      return audio_driver_transport_configure(config_get_ptr());
   }
   if (runloop_policy)
      return audio_driver_pipeline_transport_prepare_runloop(CORE_RATE, 3, speed_lowpass);
   return audio_driver_pipeline_transport_prepare(CORE_RATE, 3)
      && audio_driver_pipeline_transport_request(tempo_q16,
            transport_mode == 3, reset, transport_mode == 2 ? 1000 : 0);
}

static bool pipeline_up(unsigned latency_ms)
{
   audio_driver_state_t *st = &audio_driver_st;
   size_t per_frame = (size_t)(CORE_RATE / FPS);
   size_t ring_bytes;
   source_layout = channels == 8 ? AUDIO_LAYOUT_7POINT1
      : channels == 6 ? AUDIO_LAYOUT_5POINT1 : AUDIO_LAYOUT_STEREO;
   source_channels = channels;

   dev_usable   = (size_t)((latency_ms * OUT_RATE) / 1000) * channels;
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

   retro_atomic_size_init(&cnt_wakes, 0);
   retro_atomic_size_init(&to_float, 0);
   retro_atomic_size_init(&to_int16, 0);
   retro_atomic_size_init(&int16_src_frames, 0);
   retro_atomic_size_init(&cnt_writes, 0);
   retro_atomic_size_init(&sig_seq, 0);
   retro_atomic_size_init(&lat_count, 0);
   retro_atomic_size_init(&sig_us, 0);
   sig_seen            = 0;
   prod_total_us       = 0;
   prod_blocked_frames = 0;
   prod_worst_us       = 0;

   memset(st, 0, sizeof(*st));
   st->current_audio        = &clocked_driver;
   st->context_audio_data   = clocked_driver.init(NULL, OUT_RATE, latency_ms, NULL);
   st->input                = (double)CORE_RATE;
   st->src_ratio_orig       = (double)OUT_RATE / (double)CORE_RATE;
   st->src_ratio_curr       = st->src_ratio_orig;
   st->cached_rate_adjust   = 1.0;
   st->volume_gain          = 1.0f;
   st->out_channels         = channels;
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
   st->pipe_float           = source_float;
   st->out_layout          = source_layout;
   st->pipe_channels       = channels > 2 ? AUDIO_PIPE_CANON_CHANNELS : 2;
   st->pipe_frame_bytes    = st->pipe_channels * (source_float ? sizeof(float) : sizeof(int16_t));
   audio_pipeline_layout_init(&st->pipe_layouts, source_layout);
   if (channels > 2)
   {
      st->pipe_wide_bytes = per_frame * AUDIO_PIPE_CANON_CHANNELS * sizeof(float);
      st->pipe_wide = (uint8_t*)malloc(st->pipe_wide_bytes);
      st->upmix_frames = (1 << 20) / (2 * sizeof(float));
      st->upmix_buf = (float*)malloc(st->upmix_frames * channels * sizeof(float));
      st->upmix_i16 = (int16_t*)malloc(st->upmix_frames * channels * sizeof(int16_t));
      if (!st->pipe_wide || !st->upmix_buf || !st->upmix_i16
            || !audio_upmix_init(&st->upmix, source_layout, OUT_RATE))
         return false;
   }
   if (!device_int16) AUDIO_FLAGS_SET(st, AUDIO_FLAG_USE_FLOAT);
   strcpy(st->resampler_ident, "sinc");
   st->resampler_quality    = RESAMPLER_QUALITY_NORMAL;
   if (!retro_resampler_realloc(&st->resampler_data, &st->resampler,
            st->resampler_ident, st->resampler_quality, st->src_ratio_orig))
      return false;
   config_get_ptr()->bools.audio_fastpath_s16 = device_int16 && !source_float;
   if (device_int16 && !source_float)
   {
      st->resampler_data_int16 = audio_driver_int16_resampler_new(st);
      st->resampler_int16_process = counted_int16_src;
      st->resampler_int16_free = sinc_resampler_int16_free;
      st->resampler_int16_reset = sinc_resampler_int16_reset;
      if (!st->resampler_data_int16) return false;
   }
   retro_atomic_store_release_int(&st->pipe_ctrl_avail, -1);
   st->rate_control_delta   = 0.005f;
   st->drc_threshold_int16s = 1600;
   st->sink_bias            = 1.0;
   st->out_rate             = OUT_RATE;
   config_get_ptr()->bools.audio_sink_rate_estimation = true;
   config_get_ptr()->uints.audio_output_sample_rate   = OUT_RATE;
   config_get_ptr()->bools.audio_sync                 = true;
   config_get_ptr()->floats.slowmotion_ratio          = 1;
   audio_driver_publish_runloop();

   ring_bytes = per_frame * 3 * st->pipe_frame_bytes;
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
   if (   !st->state_lock || !st->output_samples_buf || !st->pipe_scratch
         || !st->input_data || !st->synth_buf || !dev_ring)
      return false;
   if (use_wrapper && !audio_init_thread(&st->current_audio,
            &st->context_audio_data, NULL, OUT_RATE, NULL, latency_ms,
            false, false, &clocked_driver))
      return false;
   if (transport_mode && !prepare_transport(false))
      return false;
   return true;
}

static void pipeline_down(void)
{
   audio_driver_state_t *st = &audio_driver_st;
   /* Both worker threads have joined. */
   audio_driver_pipeline_transport_release();
   audio_driver_extra_free(st);
   if (st->resampler && st->resampler_data)
      st->resampler->free(st->resampler_data);
   if (st->resampler_data_int16 && st->resampler_int16_free)
      st->resampler_int16_free(st->resampler_data_int16);
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
   free(st->pipe_wide);
   free(st->upmix_buf);
   free(st->upmix_i16);
   free(dev_ring);
   dev_ring = NULL;
}

/* Called through the real wrapper's parked control transaction. */
static void discard_parked(void *userdata)
{
   audio_driver_state_t *st = &audio_driver_st;
   (void)userdata;
   if (retro_atomic_load_acquire_int(&in_callback)) fixture_failures++;
   if (st->pipe_transport && !audio_driver_pipeline_transport_discard(
            retro_spsc_read_avail(&st->pipe_ring) / st->pipe_frame_bytes))
      fixture_failures++;
}

static void submit_frame(size_t per_frame, unsigned publishes);
static void pause_boundary_run(void);
static bool pause_boundary_mode;
static void consumer_exit_run(void);
static bool consumer_exit_mode;

struct live_control_check
{
   uint32_t serial, control, cutoff, layout;
   bool initial;
};

static void check_live_control(void *userdata)
{
   struct live_control_check *check = (struct live_control_check*)userdata;
   audio_pipeline_layout_t *q = &audio_driver_st.pipe_layouts;
   if (retro_atomic_load_acquire_int(&in_callback)) fixture_failures++;
   if (check->initial) check->serial = q->reset_serial;
   else if (q->reset_serial != check->serial || q->current_control != check->control
         || q->current_cutoff != check->cutoff)
   {
      fprintf(stderr, "control mismatch: reset %u/%u, control %u/%u, cutoff %u/%u\n",
            q->reset_serial, check->serial, q->current_control, check->control,
            q->current_cutoff, check->cutoff);
      fixture_failures++;
   }
   if (!check->initial && live_layouts)
   {
      unsigned extras = audio_layout_channels(check->layout & ~AUDIO_LAYOUT_STEREO);
      if (q->current_layout != check->layout
            || (extras && (audio_driver_st.extra.channels != extras
                  || audio_driver_st.extra.positions != (check->layout & ~AUDIO_LAYOUT_STEREO))))
         fixture_failures++;
   }
}

static void check_fallback_drained(void *userdata)
{
   audio_driver_state_t *st = &audio_driver_st;
   if (retro_atomic_load_acquire_int(&in_callback)) fixture_failures++;
   *(bool*)userdata = !retro_spsc_read_avail(&st->pipe_ring) && !st->pipe_pending_bytes;
}

static void wrapper_live_controls(unsigned publishes)
{
   static const double tempos[] = { 0.25, 1, 32, 0.5, 1, 16, 2, 4 };
   static const uint32_t cutoffs[] = { 0, 0, 675, 0, 0, 1350, 10800, 5400 };
   static const uint32_t layouts[] = { AUDIO_LAYOUT_5POINT1, AUDIO_LAYOUT_7POINT1,
      AUDIO_LAYOUT_STEREO, AUDIO_LAYOUT_5POINT1_SURROUND, AUDIO_LAYOUT_7POINT1,
      AUDIO_LAYOUT_STEREO, AUDIO_LAYOUT_5POINT1, AUDIO_LAYOUT_7POINT1 };
   audio_driver_state_t *st = &audio_driver_st;
   struct live_control_check check;
   audio_pipeline_stretch_t *retained = st->pipe_transport;
   void *retained_output = st->pipe_transport_output;
   unsigned step;
   check.initial = true;
   audio_thread_apply_control(st->context_audio_data, check_live_control, &check);
   check.initial = false;
   for (step = 0; step < 8; step++)
   {
      bool active = step != 1 && step != 4;
      size_t boundary, before = retro_atomic_load_acquire_size(&cnt_writes);
      unsigned retry;
      uint32_t tempo = (uint32_t)(tempos[step] * 65536.0);
      check.control = active ? tempo | AUDIO_PIPELINE_STRETCH : 65536;
      check.cutoff = speed_lowpass ? cutoffs[step] : step & 1 ? 1000 : 0;
      if (live_layouts)
      {
         source_layout = layouts[step];
         source_channels = audio_layout_channels(source_layout);
      }
      check.layout = source_layout;
      if (runloop_policy)
      {
         runloop_state_get_ptr()->flags = tempos[step] < 1 ? RUNLOOP_FLAG_SLOWMOTION
            : tempos[step] > 1 ? RUNLOOP_FLAG_FASTMOTION : 0;
         config_get_ptr()->floats.slowmotion_ratio = (float)(1.0 / tempos[step]);
         config_get_ptr()->bools.audio_fastforward_speedup = true;
         retro_atomic_store_release_int(&st->pipe_ff_mult_q16,
               (int)(65536.0 / tempos[step]));
         audio_driver_publish_runloop();
      }
      if (!(auto_runloop || (runloop_policy
               ? audio_driver_pipeline_transport_request_runloop(false, true)
               : speed_lowpass
               ? audio_driver_pipeline_transport_request_speed(tempo, active, false, true)
               : audio_driver_pipeline_transport_request(tempo, active, false, check.cutoff))))
      {
         fixture_failures++;
         return;
      }
      for (retry = 0; retry < 3; retry++)
      {
         if (auto_runloop)
            retro_atomic_store_release_int(&st->pipe_ff_mult_q16,
                  (int)(65536.0 / tempos[step]));
         submit_frame((size_t)(CORE_RATE / FPS *
                  (tempos[step] < 1 ? 1 : tempos[step])), publishes);
      }
      boundary = retro_atomic_load_relaxed_size(&st->pipe_layouts.head);
      for (retry = 0; retry < 2000; retry++)
      {
         if (retro_spsc_read_avail(&st->pipe_ring) == 0
               && retro_atomic_load_acquire_size(&st->pipe_layouts.tail) == boundary
               && retro_atomic_load_acquire_size(&cnt_writes) != before) break;
         usleep(1000);
      }
      if (retry == 2000)
      { fprintf(stderr, "control drain timed out at step %u\n", step); fixture_failures++; }
      /* Observe consumer-owned metadata only while the real worker is parked. */
      audio_thread_apply_control(st->context_audio_data, check_live_control, &check);
   }
   if (runloop_policy)
   {
      if (auto_runloop)
      {
         runloop_state_get_ptr()->flags = RUNLOOP_FLAG_SLOWMOTION;
         config_get_ptr()->floats.slowmotion_ratio = 8;
         audio_driver_publish_runloop();
         submit_frame((size_t)(CORE_RATE / FPS), publishes);
         if (st->pipe_transport || !st->pipe_transport_follow
               || st->pipe_transport_suspended != retained) fixture_failures++;
      }
      runloop_state_get_ptr()->flags = 0;
      config_get_ptr()->floats.slowmotion_ratio = 1;
      config_get_ptr()->bools.audio_fastforward_speedup = false;
      audio_driver_publish_runloop();
      if (auto_runloop)
      {
         bool drained = false;
         audio_driver_frame_end();
         for (step = 0; step < 2000; step++)
         {
            if (!retro_spsc_read_avail(&st->pipe_ring))
               audio_thread_apply_control(st->context_audio_data, check_fallback_drained, &drained);
            if (drained) break;
            usleep(1000);
         }
         if (!drained)
         { fprintf(stderr, "legacy fallback did not drain source/device output\n"); fixture_failures++; }
         audio_driver_frame_end();
         submit_frame((size_t)(CORE_RATE / FPS), publishes);
         if (st->pipe_transport != retained || st->pipe_transport_suspended
               || st->pipe_transport_output != retained_output
               || st->pipe_layouts.published_control != 65536
               || st->pipe_layouts.published_cutoff != 0)
         { fprintf(stderr, "automatic recovery did not reuse native storage\n"); fixture_failures++; }
      }
   }
}

static void wrapper_restart(void)
{
   audio_driver_state_t *st = &audio_driver_st;
   unsigned cycle;
   for (cycle = 0; cycle < 8; cycle++)
   {
      size_t before;
      unsigned retry;
      audio_thread_apply_control(st->context_audio_data, discard_parked, NULL);
      if (cycle & 1)
      {
         if (!audio_driver_stop() || st->last_flush_time || st->pipe_ff_frames
               || retro_atomic_load_acquire_int(&st->pipe_ff_mult_q16) != 65536)
         { fprintf(stderr, "frontend stop retained source cadence\n"); fixture_failures++; }
      }
      else if (!st->current_audio->stop(st->context_audio_data)) fixture_failures++;
      before = retro_atomic_load_acquire_size(&cnt_wakes);
      audio_thread_apply_control(st->context_audio_data, discard_parked, NULL);
      if (transport_mode)
      {
         audio_driver_pipeline_transport_release();
         if (!prepare_transport(true))
         { fprintf(stderr, "restart preparation failed\n"); fixture_failures++; }
      }
      if (before != retro_atomic_load_acquire_size(&cnt_wakes)) fixture_failures++;
      before = retro_atomic_load_acquire_size(&cnt_writes);
      if (cycle & 1)
      {
         if (!audio_driver_start(false)) fixture_failures++;
      }
      else if (!st->current_audio->start(st->context_audio_data, false)) fixture_failures++;
      for (retry = 0; retry < 3; retry++)
         /* Prime the fixed-size source ring even below nominal tempo. */
         submit_frame((size_t)(CORE_RATE / FPS *
                  (source_tempo < 1.0 ? 1.0 : source_tempo)), 1);
      for (retry = 0; retry < 1000 && before ==
            retro_atomic_load_acquire_size(&cnt_writes); retry++) usleep(1000);
      if (before == retro_atomic_load_acquire_size(&cnt_writes)) fixture_failures++;
   }
}

/* --- one run --------------------------------------------------------- */

static int cmp_time(const void *a, const void *b)
{
   retro_time_t x = *(const retro_time_t*)a;
   retro_time_t y = *(const retro_time_t*)b;
   return (x > y) - (x < y);
}

/* One video frame's audio, handed over as @publishes calls. The split is
 * by whole frames and the remainder goes with the last call, so every
 * setting submits exactly the same samples at exactly the same rate and
 * only the granularity differs. */
static void submit_frame(size_t per_frame, unsigned publishes)
{
   size_t chunk = per_frame / publishes;
   size_t done  = 0;
   unsigned k;
   retro_time_t t0, t1, slept = 0;
   struct timespec spread_start;

   if (!chunk)
      chunk = 1;

   t0 = cpu_features_get_time_usec();
   if (spread_publishes) clock_gettime(CLOCK_MONOTONIC, &spread_start);
   for (k = 0; k < publishes && done < per_frame; k++)
   {
      size_t n = (k + 1 == publishes) ? per_frame - done : chunk;
      if (done + n > per_frame)
         n = per_frame - done;
      if (channels > 2)
      {
         if (!audio_driver_multi_pipe(&audio_driver_st,
                  source_float ? (const void*)(frame_audio_float + done * source_channels)
                               : (const void*)(frame_audio + done * source_channels),
                  n, source_channels, source_layout, source_float))
            fixture_failures++;
      }
      else audio_driver_submit(&audio_driver_st, 1.0f,
            source_float ? (const void*)(frame_audio_float + done * 2)
                         : (const void*)(frame_audio + done * 2), n * 2,
            source_float, false, false, true);
      done += n;
      if (spread_publishes && publishes > 1 && k + 1 < publishes)
      {
         /* The model's own delay, not the producer's cost: timed and
          * subtracted below, or the producer column in SPREAD mode
          * would just be reporting these sleeps back. */
         retro_time_t s0 = cpu_features_get_time_usec();
         struct timespec deadline = spread_start;
         /* Absolute deadlines prevent short-sleep rounding accumulating
          * hundreds of times in a scanline-sized publish sweep. */
         deadline.tv_nsec += (long)(1e9 / FPS * 0.8 * (k + 1) / publishes);
         deadline.tv_sec += deadline.tv_nsec / 1000000000L;
         deadline.tv_nsec %= 1000000000L;
         clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &deadline, NULL);
         slept += cpu_features_get_time_usec() - s0;
      }
      /* The failure mode this harness exists to catch, on demand. A
       * conversion that turns each publish into a notify produces
       * exactly this, and the audio it produces is identical - which is
       * why no correctness harness can see it and the wake count here
       * can. Kept as a mode rather than described in a comment so the
       * bad column can be measured rather than imagined. */
      if (notify_per_publish)
         audio_driver_pipeline_signal(&audio_driver_st);
   }
   t1 = cpu_features_get_time_usec();

   /* Publish the signal's timestamp before the sequence that advertises
    * it, so a consumer that sees the sequence sees the time with it. */
   retro_atomic_store_release_size(&sig_us, (size_t)t1);
   retro_atomic_store_release_size(&sig_seq,
         retro_atomic_load_relaxed_size(&sig_seq) + 1);
   if (auto_runloop) audio_driver_frame_end();
   else audio_driver_pipeline_signal(&audio_driver_st);

   if (t1 > t0 + slept)
   {
      retro_time_t spent = t1 - t0 - slept;
      prod_total_us += spent;
      /* A frame's audio is 16.7 ms of work for nobody; anything past a
       * tenth of that is the producer waiting on the ring, not
       * converting. */
      if (spent > 1666)
      {
         prod_blocked_frames++;
         if (spent > prod_worst_us)
            prod_worst_us = spent;
      }
   }
}

static void run_one(unsigned publishes, double seconds, bool backpressure)
{
   pthread_t cons, dev;
   size_t    per_frame = (size_t)(CORE_RATE / FPS * source_tempo);
   size_t    i, frames = (size_t)(seconds * FPS);
   struct timespec next;
   long      step_ns = (long)(1e9 / FPS);
   size_t    warm_frames = 0;
   size_t    warm_wakes = 0, warm_writes = 0, warm_under = 0, warm_pulls = 0;
   size_t    wakes, writes, under, pulls, nlat;
   double    p50 = 0.0, p99 = 0.0, worst = 0.0;

   dev_backpressure = backpressure;

   if (!pipeline_up(LATENCY_MS))
   {
      printf("  %4u: fixture failed\n", publishes);
      fixture_failures++;
      return;
   }

   retro_atomic_store_release_int(&dev_running, 1);
   retro_atomic_store_release_int(&consumer_run, 1);
   pthread_create(&dev,  NULL, dev_thread, NULL);
   if (use_wrapper)
   {
      if (!audio_driver_st.current_audio->start(audio_driver_st.context_audio_data, false))
         fixture_failures++;
   }
   else pthread_create(&cons, NULL, consumer, NULL);

   if (pause_boundary_mode)
   {
      pause_boundary_run();
      frames = 0;
   }
   if (consumer_exit_mode)
   {
      consumer_exit_run();
      frames = 0;
   }
   clock_gettime(CLOCK_MONOTONIC, &next);
   for (i = 0; i < frames; i++)
   {
      next.tv_nsec += step_ns;
      next.tv_sec  += next.tv_nsec / 1000000000L;
      next.tv_nsec %= 1000000000L;
      clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, NULL);
      submit_frame(per_frame, publishes);
      /* A second in, drop the startup transient: priming does not
       * repeat, and averaging it into a steady-state rate hides what
       * the steady state costs. */
      if (i == (size_t)FPS)
      {
         warm_wakes  = retro_atomic_load_acquire_size(&cnt_wakes);
         warm_writes = retro_atomic_load_acquire_size(&cnt_writes);
         warm_under  = retro_atomic_load_acquire_size(&dev_underruns);
         warm_pulls  = retro_atomic_load_acquire_size(&dev_pulls);
         warm_frames = i;
         prod_total_us       = 0;
         prod_blocked_frames = 0;
         prod_worst_us       = 0;
         retro_atomic_store_release_size(&lat_count, 0);
      }
   }

   usleep(50000);
   if (use_wrapper)
   {
      audio_driver_state_t *st = &audio_driver_st;
      if (live_controls && !pause_boundary_mode) wrapper_live_controls(publishes);
      if (!pause_boundary_mode) wrapper_restart();
      if (!st->current_audio->stop(st->context_audio_data)) fixture_failures++;
      if (channels > 2 && (st->extra.channels != channels - 2
               || st->extra.positions != (source_layout & ~AUDIO_LAYOUT_STEREO)
               || st->extra.res_int16 != (device_int16 && !source_float)))
      {
         fprintf(stderr, "wide transport lost native extra-channel state\n");
         fixture_failures++;
      }
      audio_driver_pipeline_transport_release();
      st->current_audio->free(st->context_audio_data);
      st->current_audio = &clocked_driver;
      st->context_audio_data = NULL;
   }
   retro_atomic_store_release_int(&consumer_run, 0);
   retro_atomic_store_release_int(&dev_running, 0);
   audio_driver_pipeline_wake();
   dev_signal();
   if (!use_wrapper) pthread_join(cons, NULL);
   pthread_join(dev,  NULL);

   wakes  = retro_atomic_load_acquire_size(&cnt_wakes)  - warm_wakes;
   writes = retro_atomic_load_acquire_size(&cnt_writes) - warm_writes;
   under  = retro_atomic_load_acquire_size(&dev_underruns) - warm_under;
   pulls  = retro_atomic_load_acquire_size(&dev_pulls) - warm_pulls;
   if (use_wrapper && (!writes || !wakes)) fixture_failures++;
   if (use_wrapper && !pause_boundary_mode)
   {
      size_t f = retro_atomic_load_acquire_size(&to_float);
      size_t n = retro_atomic_load_acquire_size(&to_int16);
      size_t native_frames = retro_atomic_load_acquire_size(&int16_src_frames);
      /* Matching lanes stay native; mixed lanes convert only toward the
       * sink. A conversion-volume invariant of the load sweep: the
       * boundary scenario's few frames mostly leave through the discard
       * and prove nothing about lane purity either way. */
      if (source_float == !device_int16)
      {
         if (f || n) fixture_failures++;
         if (device_int16 && !native_frames) fixture_failures++;
      }
      else if (source_float ? (f || !n) : (n || !f)) fixture_failures++;
      printf("converted samples: int16-to-float=%u float-to-int16=%u; int16 SRC frames=%u\n",
            (unsigned)f, (unsigned)n, (unsigned)native_frames);
   }
   nlat   = retro_atomic_load_acquire_size(&lat_count);
   frames = frames - warm_frames;

   if (nlat)
   {
      qsort(lat_us, nlat, sizeof(lat_us[0]), cmp_time);
      p50   = (double)lat_us[nlat / 2];
      p99   = (double)lat_us[(nlat * 99) / 100];
      worst = (double)lat_us[nlat - 1];
   }

   if (use_wrapper)
      printf("wrapper pubs=%u backpressure=%u: %u callbacks, %u writes, 8 restarts\n",
            publishes, backpressure, (unsigned)wakes, (unsigned)writes);
   else printf("  %4u  %8.2f  %8.2f | %7.0f %7.0f %7.0f | %8.1f %5u %7.0f | %5u %5u\n",
         publishes,
         frames ? (double)wakes  / (double)frames : 0.0,
         frames ? (double)writes / (double)frames : 0.0,
         p50, p99, worst,
         frames ? (double)prod_total_us / (double)frames : 0.0,
         (unsigned)prod_blocked_frames,
         (double)prod_worst_us,
         (unsigned)under, (unsigned)pulls);
   pipeline_down();
}


/* --- the pause boundary on the ring -------------------------------- */

/* What audio_driver_pause_fade() promises the threaded pipeline: the
 * frames the ring held at a pause are stale behind the tail and are
 * never played, and the core's first audio after the resume comes up
 * under the ramp. Sequenced through the wrapper's parked control
 * transactions, so the pause lands with the consumer provably not
 * mid-callback and the ring provably holding unplayed source. */

static void pause_boundary_fill(float amp)
{
   size_t i;
   for (i = 0; i < 32768u * channels; i++)
   {
      frame_audio[i]       = (int16_t)(amp * 32767.0f);
      frame_audio_float[i] = amp;
   }
}

static void pause_boundary_pause_parked(void *userdata)
{
   (void)userdata;
   /* Content the consumer has provably not touched: the submit lands
    * with the callback parked, or drops against a ring already full of
    * the same stale level - unplayed source either way. */
   submit_frame((size_t)(CORE_RATE / FPS), 1);
   audio_driver_pause_fade(true);
   /* The tail just written is the stream's own; everything at the
    * stale level from here on is a protocol failure. */
   retro_atomic_store_release_int(&tap_stale, 0);
   retro_atomic_store_release_int(&tap_on, 1);
}

static void pause_boundary_resume_parked(void *userdata)
{
   (void)userdata;
   retro_atomic_store_release_int(&tap_on, 0);
   audio_driver_pause_fade(false);
   retro_atomic_store_release_size(&tap_head_n, 0);
   retro_atomic_store_release_int(&tap_record, 1);
}

static void pause_boundary_case(void)
{
   audio_driver_state_t *st = &audio_driver_st;
   size_t per_frame = (size_t)(CORE_RATE / FPS);
   unsigned frame, waited;
   double head_mean = 0.0, tail_mean = 0.0;
   size_t n, i;

   /* Played audio at the stale level, so the history and the device are
    * primed the way a session's would be. */
   pause_boundary_fill(0.9f);
   for (frame = 0; frame < 8; frame++)
      submit_frame(per_frame, 1);

   audio_thread_apply_control(st->context_audio_data,
         pause_boundary_pause_parked, NULL);

   /* The consumer takes the stale frames out unplayed. */
   for (waited = 0; waited < 2000; waited++)
   {
      if (!retro_spsc_read_avail(&st->pipe_ring))
         break;
      audio_driver_pipeline_wake();
      usleep(1000);
   }
   if (retro_spsc_read_avail(&st->pipe_ring))
      fixture_failures++;
   if (retro_atomic_load_acquire_int(&tap_stale))
      fixture_failures++;

   audio_thread_apply_control(st->context_audio_data,
         pause_boundary_resume_parked, NULL);

   /* The core's first audio after the resume. */
   for (frame = 0; frame < 8 && retro_atomic_load_relaxed_size(&tap_head_n)
         < TAP_HEAD_MAX; frame++)
      submit_frame(per_frame, 1);
   for (waited = 0; waited < 2000
         && retro_atomic_load_relaxed_size(&tap_head_n) < TAP_HEAD_MAX;
         waited++)
      usleep(1000);
   retro_atomic_store_release_int(&tap_record, 0);

   /* Acquire pairs with the release that published each element. */
   n = retro_atomic_load_acquire_size(&tap_head_n);
   if (n < TAP_HEAD_MAX)
   {
      fixture_failures++;
      return;
   }
   for (i = 0; i < 64; i++)
   {
      head_mean += tap_head[i];
      tail_mean += tap_head[n - 64 + i];
   }
   head_mean /= 64.0;
   tail_mean /= 64.0;
   /* The ramp: the resumed stream opens well below its level and is
    * climbing by the end of the window. */
   if (!(head_mean < 0.12))
      fixture_failures++;
   if (!(tail_mean > head_mean * 3.0 + 0.02))
      fixture_failures++;
}

/* The consumer leaving its loop while the producer is parked in the
 * wait for it. audio_thread_write() clears alive under thr->lock when
 * the device refuses, and the loop reads alive under the same lock -
 * but what the wrapper publishes on its way out, pipe_consumer_gone,
 * is under no lock, and the parked producer reads it under none
 * either. Nothing joins between the two, because the producer is
 * waiting rather than tearing the driver down, so there is no edge to
 * order them. That pair is what this lane exists to put in front of
 * ThreadSanitizer. */
static void consumer_exit_case(void)
{
   size_t   per_frame = (size_t)(CORE_RATE / FPS);
   unsigned frame;
   int64_t  began, spent;

   /* Enough in flight that the producer has to wait on the consumer
    * rather than sail through. */
   for (frame = 0; frame < 16; frame++)
      submit_frame(per_frame, 1);

   /* From here the device refuses, so the wrapper clears alive and
    * leaves its loop under its own steam. */
   retro_atomic_store_release_int(&dev_fail_now, 1);

   began = (int64_t)cpu_features_get_time_usec();
   for (frame = 0; frame < 64; frame++)
      submit_frame(per_frame, 1);
   spent = (int64_t)cpu_features_get_time_usec() - began;

   /* A consumer that has gone is one the producer must stop waiting
    * for. Each wait is lap bounded, so sitting through even one full
    * set of laps for every frame here would take far longer than this;
    * the bound is loose on purpose, since it is the race and not the
    * timing this lane is for. */
   if (spent > 4000000)
   {
      fprintf(stderr,
            "the producer waited %lld us on a consumer that had gone\n",
            (long long)spent);
      fixture_failures++;
   }
}

static void consumer_exit_run(void)
{
   unsigned f0 = fixture_failures;
   consumer_exit_case();
   printf("consumer exit: the producer stops waiting on a departed consumer, %u failures\n",
         fixture_failures - f0);
}

static void pause_boundary_run(void)
{
   unsigned f0 = fixture_failures;
   pause_boundary_case();
   printf("pause boundary: stale ring discarded, resume under the ramp, %u failures\n",
         fixture_failures - f0);
}

int main(int argc, char **argv)
{
   /* 1 is a core that hands over a frame at a time. 262 is a scanline
    * of an NTSC frame; 312 is PAL. The ones between are there so a
    * handshake whose cost grows with N shows the growth rather than
    * only its endpoints. */
   static const unsigned sweep[] = { 1, 2, 4, 8, 16, 64, 262, 312 };
   double seconds = (argc > 1) ? atof(argv[1]) : 4.0;
   const char *transport = getenv("TRANSPORT");
   const char *tempo = getenv("TEMPO");
   const char *layout = getenv("LAYOUT");
   size_t i;
   use_wrapper = getenv("WRAPPER") != NULL;
   source_float = getenv("SOURCE_FLOAT") != NULL;
   device_int16 = getenv("DEVICE_INT16") != NULL;
   device_sample_bytes = device_int16 ? sizeof(int16_t) : sizeof(float);
   track_conversions = use_wrapper;
   live_controls = getenv("LIVE_CONTROLS") != NULL;
   live_layouts = getenv("LIVE_LAYOUTS") != NULL;
   speed_lowpass = getenv("SPEED_LPF") != NULL;
   runloop_policy = getenv("RUNLOOP_POLICY") != NULL;
   auto_runloop = getenv("AUTO_RUNLOOP") != NULL;
   pause_boundary_mode = getenv("PAUSE_BOUNDARY") != NULL;
   consumer_exit_mode  = getenv("CONSUMER_EXIT") != NULL;
   if (pause_boundary_mode && (!use_wrapper || !transport))
   {
      fprintf(stderr, "PAUSE_BOUNDARY requires WRAPPER and a TRANSPORT\n");
      return 1;
   }
   if (layout)
   {
      if (!strcmp(layout, "5.1")) source_layout = AUDIO_LAYOUT_5POINT1;
      else if (!strcmp(layout, "7.1")) source_layout = AUDIO_LAYOUT_7POINT1;
      else { fprintf(stderr, "LAYOUT must be 5.1 or 7.1\n"); return 1; }
      channels = audio_layout_channels(source_layout);
   }

   if (transport)
   {
      if (!strcmp(transport, "dry")) transport_mode = 1;
      else if (!strcmp(transport, "lpf")) transport_mode = 2;
      else if (!strcmp(transport, "stretch")) transport_mode = 3;
      else { fprintf(stderr, "TRANSPORT must be dry, lpf or stretch\n"); return 1; }
   }
   if (tempo)
   {
      char *end;
      source_tempo = strtod(tempo, &end);
      if (end == tempo || *end || transport_mode != 3
            || (source_tempo != 0.25 && source_tempo != 0.5
               && source_tempo != 1 && source_tempo != 2
               && source_tempo != 4 && source_tempo != 8
               && source_tempo != 16 && source_tempo != 32))
      {
         fprintf(stderr, "TEMPO requires stretch and one of 0.25, 0.5, 1, 2, 4, 8, 16, 32\n");
         return 1;
      }
      tempo_q16 = (uint32_t)(source_tempo * 65536.0);
   }
   if (live_controls && (!use_wrapper || transport_mode != 3))
   {
      fprintf(stderr, "LIVE_CONTROLS requires WRAPPER and TRANSPORT=stretch\n");
      return 1;
   }
   if (live_layouts && (!live_controls || channels != 8))
   {
      fprintf(stderr, "LIVE_LAYOUTS requires LIVE_CONTROLS and LAYOUT=7.1\n");
      return 1;
   }
   if (speed_lowpass && !live_controls)
   {
      fprintf(stderr, "SPEED_LPF requires LIVE_CONTROLS\n");
      return 1;
   }
   if (runloop_policy && !speed_lowpass)
   {
      fprintf(stderr, "RUNLOOP_POLICY requires SPEED_LPF\n");
      return 1;
   }
   if (auto_runloop && !runloop_policy)
   {
      fprintf(stderr, "AUTO_RUNLOOP requires RUNLOOP_POLICY\n");
      return 1;
   }

   lat_us = (retro_time_t*)malloc(MAX_SAMPLES * sizeof(retro_time_t));
   if (!lat_us)
      return 1;

   for (i = 0; i < 32768 * channels; i++)
   {
      frame_audio[i] = (int16_t)(8000.0 * sin((double)i * 0.05));
      frame_audio_float[i] = frame_audio[i] / 32768.0f;
   }

   notify_per_publish = getenv("NOTIFY_PER_PUBLISH") ? true : false;
   spread_publishes   = getenv("SPREAD")             ? true : false;

   printf("threaded pipeline handshake cost, %.0f s per setting, %g fps core,"
         " %u ms device\n", seconds, FPS, LATENCY_MS);
   printf("publishes per frame swept; the audio submitted is identical at every setting\n");
   printf("consumer: %s\n", transport ? transport : "legacy");
   printf("source: %s\n", source_float ? "float" : "int16");
   printf("layout: %u channels\n", channels);
   printf("device: %s\n", device_int16 ? "int16" : "float");
   printf("source tempo: %g; source frames/video frame: %u\n", source_tempo,
         (unsigned)(CORE_RATE / FPS * source_tempo));
   if (use_wrapper) printf("real wrapper: restart/rebuild stress; not steady-state timing\n");
   printf("publishes %s\n", spread_publishes
         ? "paced across the frame (SPREAD) - a core whose retro_run fills its budget"
         : "in a burst, as one retro_run call emits them");
   printf("signalling %s\n\n", notify_per_publish
         ? "once per publish (NOTIFY_PER_PUBLISH) - the shape a mechanical conversion produces"
         : "once per frame, as the tree does");
   printf("-- device applies backpressure (Audio Sync against a real card) --\n");
   if (!use_wrapper)
   {
   printf("  pubs     wakes/f  writes/f |   wake latency us     |  producer us/frame   | short  pulls\n");
   printf("                             |    p50     p99     max |   mean  blkd   worst |\n");
   }
   for (i = 0; i < (pause_boundary_mode
            ? 1 : sizeof(sweep) / sizeof(sweep[0])); i++)
      run_one(sweep[i], seconds, true);
   if (pause_boundary_mode)
      goto report;

   printf("\n-- device applies none; the data handshake is the only pacer --\n");
   if (!use_wrapper)
   {
   printf("  pubs     wakes/f  writes/f |   wake latency us     |  producer us/frame   | short  pulls\n");
   printf("                             |    p50     p99     max |   mean  blkd   worst |\n");
   }
   for (i = 0; i < sizeof(sweep) / sizeof(sweep[0]); i++)
      run_one(sweep[i], seconds, false);

report:
   free(lat_us);
   {
      size_t reads = retro_atomic_load_acquire_size(&consumer_settings_reads);
      if (reads)
      {
         fprintf(stderr, "consumer read settings %u times\n", (unsigned)reads);
         fixture_failures++;
      }
   }
   if (use_wrapper) printf("native wrapper: 16 runs, 128 restart transactions, %u failures\n", fixture_failures);
   if (live_controls) printf("live transport: 128 processing changes without metadata reset, %u failures\n", fixture_failures);
   if (live_layouts) printf("live layouts: 128 source layout changes on a fixed 7.1 device, %u failures\n", fixture_failures);
   if (speed_lowpass) printf("speed LPF: 128 coherent tempo/cutoff requests, %u failures\n", fixture_failures);
   if (runloop_policy) printf("runloop policy: 128 speed-state requests, %u failures\n", fixture_failures);
   if (auto_runloop) printf("automatic transport: 128 producer updates, 16 fallbacks/recoveries, %u failures\n", fixture_failures);
   printf("pipeline wakeups: %u fixture failures\n", fixture_failures);
   return fixture_failures ? 1 : 0;
}
