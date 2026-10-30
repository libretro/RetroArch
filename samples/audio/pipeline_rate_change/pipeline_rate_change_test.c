/* A core audio rate change taken by the running driver.
 *
 * audio_driver_set_input_rate() moves the driver to a new input rate
 * without reopening the device. A 441 Hz tone is fed at one rate, the
 * rate changes, and the same tone is fed at the new one: the device
 * must be opened once, and the tone must come out at 441 Hz on both
 * sides of the change, for the float and integer paths, threaded and
 * inline, and where the threaded pipeline's ring has to grow for the
 * new rate. A configuration the change cannot carry - time stretch -
 * must refuse it and leave the driver as it was. And a rate sent from
 * retro_load_game, which brings audio up before drivers_init() does
 * it again, must leave one instance: the first freed, not orphaned.
 * Across a content switch the device is kept and taken back when the
 * next init would open the same one, and freed when it would not; one
 * that will not start again is closed and opened afresh. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../../../audio/audio_driver.c"

#define OUT_RATE      48000
#define TONE_HZ       441.0
#define SECONDS       1
#define CAPTURE       (OUT_RATE * SECONDS * 2 + OUT_RATE)
#define FEED_MAX      2048

static float   captured[CAPTURE * 2];
static float   feed_f[FEED_MAX * 2];
static int16_t feed_i[FEED_MAX * 2];
static size_t  captured_frames, device_bytes;
static unsigned device_opens, device_frees, device_stops, device_starts, failures;
static unsigned start_refusals, open_refusals;
extern bool pipeline_consumer_running;
static unsigned handovers;
static bool change_parked;
extern unsigned pipeline_mixer_inits, pipeline_mixer_dones;

static void *test_init(const char *device, unsigned rate, unsigned latency,
      unsigned *new_rate)
{
   (void)device;
   device_opens++;
   if (open_refusals)
   {
      open_refusals--;
      return NULL;
   }
   device_bytes = (size_t)latency * rate / 1000 * 2 * sizeof(float);
   *new_rate    = rate;
   return &device_bytes;
}

static ssize_t test_write(void *data, const void *buf, size_t len)
{
   size_t frames = len / (2 * sizeof(float));
   (void)data;
   if (captured_frames + frames > CAPTURE)
      frames = CAPTURE - captured_frames;
   memcpy(captured + captured_frames * 2, buf, frames * 2 * sizeof(float));
   captured_frames += frames;
   return len;
}

static size_t test_avail(void *data) { (void)data; return device_bytes; }
static size_t test_wait(void *data, size_t len)
{ (void)data; (void)len; return device_bytes; }
static bool test_float(void *data) { (void)data; return true; }
static bool test_start(void *data, bool shutdown)
{
   (void)data; (void)shutdown;
   device_starts++;
   if (start_refusals)
   {
      start_refusals--;
      return false;
   }
   return true;
}
static bool test_stop(void *data) { (void)data; device_stops++; return true; }
static void test_free(void *data) { (void)data; device_frees++; }

static void setup(bool floating, bool threaded, bool fastpath, double rate)
{
   audio_driver_state_t *st = &audio_driver_st;
   settings_t *settings     = config_get_ptr();

   memset(settings, 0, sizeof(*settings));
   settings->bools.audio_enable             = true;
   settings->bools.audio_sync               = true;
   settings->bools.audio_threaded_pipeline  = threaded;
   settings->bools.audio_fastpath_s16       = fastpath;
   settings->uints.audio_output_sample_rate = OUT_RATE;
   settings->uints.audio_latency            = 64;
   settings->floats.slowmotion_ratio        = 1.0f;
   strcpy(settings->arrays.audio_resampler, "sinc");
   settings->uints.audio_resampler_quality  = RESAMPLER_QUALITY_NORMAL;

   st->input       = (float)rate;
   st->volume_gain = 1.0f;
   audio_driver_set_core_float(floating);
   video_state_get_ptr()->av_info.timing.fps         = 60.0;
   video_state_get_ptr()->av_info.timing.sample_rate = rate;
   if (!audio_driver_init_internal(settings, false))
      abort();
   st->pipe_priming = false;
   audio_driver_publish_runloop();
}

/* Under the threaded pipeline's wrapper with its consumer running: the
 * change is decided at once and installed by the consumer before its
 * next pass, parking nothing. Until then the old rate stays in force and
 * the producer writes nothing into the ring; @parked has it installed at
 * once instead, as for a stopped driver. */
static bool change_rate_as(double rate, bool threaded, bool parked)
{
   audio_driver_state_t *st    = &audio_driver_st;
   audio_driver_t wrapper      = *st->current_audio;
   const audio_driver_t *saved = st->current_audio;
   float before                = st->input;
   bool ok;
   video_state_get_ptr()->av_info.timing.sample_rate = rate;
   if (threaded)
   {
      wrapper.ident     = "audio-thread";
      st->current_audio = &wrapper;
   }
   pipeline_consumer_running = threaded && !parked;
   ok = audio_driver_set_input_rate(rate);
   pipeline_consumer_running = false;
   if (ok && threaded && !parked && audio_driver_rate_pending(st))
   {
      size_t head = retro_atomic_load_relaxed_size(&st->pipe_ring.head);
      handovers++;
      feed_f[0] = feed_f[1] = 0.25f;
      feed_i[0] = feed_i[1] = 8192;
      audio_driver_sample_batch_float(feed_f, 1);
      audio_driver_sample_batch(feed_i, 1);
      if (     st->input != before
            || retro_atomic_load_relaxed_size(&st->pipe_ring.head) != head)
      {
         printf("FAIL: the change took effect before the consumer's pass\n");
         failures++;
      }
      audio_driver_pipeline_pause_apply(st, true);
      if (audio_driver_rate_pending(st))
      {
         printf("FAIL: the consumer's pass left the change pending\n");
         failures++;
      }
   }
   st->current_audio = saved;
   return ok;
}

static bool change_rate(double rate, bool threaded)
{
   return change_rate_as(rate, threaded, change_parked);
}

/* One second of the tone at @rate, a video frame at a time. */
static void feed(double rate, bool floating, bool threaded, double *phase)
{
   audio_driver_state_t *st = &audio_driver_st;
   size_t per_frame         = (size_t)(rate / 60.0);
   unsigned frame, i;

   for (frame = 0; frame < 60 * SECONDS; frame++)
   {
      unsigned passes = 0;
      for (i = 0; i < per_frame; i++)
      {
         float v = (float)(0.5 * sin(*phase));
         *phase += 2.0 * M_PI * TONE_HZ / rate;
         feed_f[i * 2] = feed_f[i * 2 + 1] = v;
         feed_i[i * 2] = feed_i[i * 2 + 1] = (int16_t)(v * 32767.0f);
      }
      if (floating)
         audio_driver_sample_batch_float(feed_f, per_frame);
      else
         audio_driver_sample_batch(feed_i, per_frame);
      if (threaded)
         while (retro_spsc_read_avail(&st->pipe_ring))
         {
            if (++passes > 64)
               abort();
            audio_driver_pipeline_consume(st);
         }
   }
}

/* The tone's frequency over output frames [from, to), by zero crossings. */
static double measure(size_t from, size_t to)
{
   size_t i, crossings = 0;
   for (i = from + 1; i < to; i++)
      if ((captured[(i - 1) * 2] < 0.0f) != (captured[i * 2] < 0.0f))
         crossings++;
   return (double)crossings / 2.0 * OUT_RATE / (double)(to - from);
}

static void tone_case(bool floating, bool threaded, bool fastpath,
      double from_rate, double to_rate)
{
   audio_driver_state_t *st = &audio_driver_st;
   double phase             = 0.0;
   size_t split, quarter;
   double before, after;
   void *old_resampler;
   bool ok;

   device_opens           = 0;
   captured_frames        = 0;
   setup(floating, threaded, fastpath, from_rate);
   old_resampler = fastpath ? st->resampler_data_int16 : st->resampler_data;

   feed(from_rate, floating, threaded, &phase);
   split = captured_frames;
   ok    = change_rate(to_rate, threaded);
   feed(to_rate, floating, threaded, &phase);

   /* Leave out the resampler's settling at either end and the change. */
   quarter = split / 4;
   before  = measure(quarter, split - quarter);
   after   = measure(split + quarter, captured_frames - quarter);

   printf("%s %s%s %.0f -> %.0f Hz: %s, opens=%u ratio=%.4f"
         " ring=%u tone %.1f Hz then %.1f Hz\n",
         floating ? "float" : "int16", threaded ? "threaded" : "inline",
         fastpath ? " s16" : "", from_rate, to_rate,
         ok ? "taken" : "refused", device_opens,
         st->src_ratio_orig, threaded ? (unsigned)(st->pipe_ring.capacity
            / st->pipe_frame_bytes) : 0u, before, after);

   if (     !ok
         || device_opens != 1
         || st->input != (float)to_rate
         || fabs(st->src_ratio_orig - OUT_RATE / to_rate) > 1e-9
         || (fastpath ? st->resampler_data_int16 : st->resampler_data)
               == old_resampler
         || (threaded && st->pipe_pass_frames != (size_t)(to_rate / 60.0))
         || (threaded && st->pipe_ring.capacity
               < (size_t)(to_rate / 60.0) * AUDIO_PIPE_RING_FRAMES
                 * st->pipe_frame_bytes)
         || fabs(before - TONE_HZ) > TONE_HZ * 0.01
         || fabs(after  - TONE_HZ) > TONE_HZ * 0.01)
      failures++;
   audio_driver_deinit();
}

/* A change the configuration cannot carry is refused whole. */
static void refused_case(const char *what, bool threaded, bool stretch,
      double from_rate, double to_rate)
{
   audio_driver_state_t *st = &audio_driver_st;
   void *resampler;
   bool ok;

   setup(false, threaded, false, from_rate);
   if (stretch)
      config_get_ptr()->bools.audio_time_stretch = true;
   resampler = st->resampler_data;
   ok        = change_rate(to_rate, threaded);
   printf("%s: %s\n", what, ok ? "taken" : "refused");
   if (     ok
         || st->input != (float)from_rate
         || st->resampler_data != resampler
         || fabs(st->src_ratio_orig - OUT_RATE / from_rate) > 1e-9)
      failures++;
   config_get_ptr()->bools.audio_time_stretch = false;
   audio_driver_deinit();
}

/* A new frame rate at the same audio rate: the threaded ring and slice
 * are sized in video frames and follow it; the resamplers stay. */
static void fps_case(double rate, double from_fps, double to_fps)
{
   audio_driver_state_t *st = &audio_driver_st;
   audio_driver_t wrapper;
   const audio_driver_t *saved;
   void *resampler;
   double phase = 0.0;
   size_t split, per_frame = (size_t)(rate / to_fps);
   double after;
   bool ok;
   unsigned frame, i;

   device_opens           = 0;
   captured_frames        = 0;
   setup(true, true, false, rate);
   video_state_get_ptr()->av_info.timing.fps = from_fps;
   audio_driver_deinit();
   device_opens = 0;
   if (!audio_driver_init_internal(config_get_ptr(), false))
      abort();
   st->pipe_priming = false;
   audio_driver_publish_runloop();
   resampler = st->resampler_data;

   video_state_get_ptr()->av_info.timing.fps = to_fps;
   wrapper       = *st->current_audio;
   saved         = st->current_audio;
   wrapper.ident = "audio-thread";
   st->current_audio = &wrapper;
   pipeline_consumer_running = true;
   ok = audio_driver_set_input_rate(rate);
   pipeline_consumer_running = false;
   if (ok && audio_driver_rate_pending(st))
   {
      handovers++;
      audio_driver_pipeline_pause_apply(st, true);
   }
   st->current_audio = saved;

   /* A frame's worth of audio per video frame at the new rate. */
   split = captured_frames;
   for (frame = 0; frame < (unsigned)to_fps; frame++)
   {
      unsigned passes = 0;
      for (i = 0; i < per_frame; i++)
      {
         float v = (float)(0.5 * sin(phase));
         phase  += 2.0 * M_PI * TONE_HZ / rate;
         feed_f[i * 2] = feed_f[i * 2 + 1] = v;
      }
      audio_driver_sample_batch_float(feed_f, per_frame);
      while (retro_spsc_read_avail(&st->pipe_ring))
      {
         if (++passes > 64)
            abort();
         audio_driver_pipeline_consume(st);
      }
   }
   after = measure(split + (captured_frames - split) / 4, captured_frames);

   printf("%.0f Hz, %.0f -> %.0f fps: %s, opens=%u pass=%u ring=%u"
         " tone %.1f Hz\n", rate, from_fps, to_fps, ok ? "taken" : "refused",
         device_opens, (unsigned)st->pipe_pass_frames,
         (unsigned)(st->pipe_ring.capacity / st->pipe_frame_bytes), after);
   if (     !ok
         || device_opens != 1
         || st->resampler_data != resampler
         || st->pipe_pass_frames != (per_frame < AUDIO_PIPE_SLICE_INT16S / 2
               ? per_frame : AUDIO_PIPE_SLICE_INT16S / 2)
         || st->pipe_ring.capacity
               < per_frame * AUDIO_PIPE_RING_FRAMES * st->pipe_frame_bytes
         || fabs(after - TONE_HZ) > TONE_HZ * 0.01)
      failures++;
   audio_driver_deinit();
}

/* Audio brought up twice without a teardown between. */
static void second_init_case(bool threaded)
{
   audio_driver_state_t *st = &audio_driver_st;
   double phase             = 0.0;
   size_t split;
   double after;

   device_opens    = 0;
   device_frees    = 0;
   captured_frames = 0;
   setup(true, threaded, false, 22050.0);
   if (!audio_driver_init_internal(config_get_ptr(), false))
      abort();
   st->pipe_priming = false;
   audio_driver_publish_runloop();
   split = captured_frames;
   feed(22050.0, true, threaded, &phase);
   after = measure(split + (captured_frames - split) / 4, captured_frames);

   printf("second init, %s: opens=%u frees=%u tone %.1f Hz\n",
         threaded ? "threaded" : "inline", device_opens, device_frees, after);
   if (     device_opens != 2
         || device_frees != 1
         || fabs(after - TONE_HZ) > TONE_HZ * 0.01)
      failures++;
   audio_driver_deinit();
   if (device_frees != 2)
      failures++;
}

/* A content switch: kept, then taken back at the new core's rate. */
static void keep_case(bool threaded)
{
   audio_driver_state_t *st = &audio_driver_st;
   double phase             = 0.0;
   size_t split;
   double after;
   unsigned starts;

   device_opens = device_frees = device_stops = 0;
   captured_frames = 0;
   setup(true, threaded, false, 22050.0);
   feed(22050.0, true, threaded, &phase);

   audio_driver_keep_device(true);
   audio_driver_deinit();
   if (device_frees != 0 || device_stops != 1)
      failures++;

   starts = device_starts;
   st->input = 32000.0f;
   video_state_get_ptr()->av_info.timing.sample_rate = 32000.0;
   if (!audio_driver_init_internal(config_get_ptr(), false))
      abort();
   audio_driver_drop_kept();
   st->pipe_priming = false;
   audio_driver_publish_runloop();
   split = captured_frames;
   feed(32000.0, true, threaded, &phase);
   after = measure(split + (captured_frames - split) / 4, captured_frames);

   printf("kept, %s: opens=%u frees=%u stops=%u starts=%u ratio=%.4f tone %.1f Hz\n",
         threaded ? "threaded" : "inline", device_opens, device_frees,
         device_stops, device_starts - starts, st->src_ratio_orig, after);
   if (     device_opens != 1
         || device_frees != 0
         /* started again, as a device just opened is */
         || device_starts - starts != 1
         || fabs(st->src_ratio_orig - OUT_RATE / 32000.0) > 1e-9
         || fabs(after - TONE_HZ) > TONE_HZ * 0.01)
      failures++;
   audio_driver_deinit();
   if (device_frees != 1)
      failures++;
}

/* A kept device that will not start again is closed and opened afresh,
 * and when that open fails too, init goes on without audio. */
static void keep_restart_refused_case(bool reopen_fails)
{
   audio_driver_state_t *st = &audio_driver_st;
   bool active;

   device_opens = device_frees = device_stops = 0;
   setup(true, false, false, 22050.0);
   audio_driver_keep_device(true);
   audio_driver_deinit();

   start_refusals = 1;
   open_refusals  = reopen_fails ? 1 : 0;
   if (!audio_driver_init_internal(config_get_ptr(), false))
      abort();
   active = (AUDIO_FLAGS_GET(st) & AUDIO_FLAG_ACTIVE) != 0;

   printf("kept, restart refused%s: opens=%u frees=%u active=%d\n",
         reopen_fails ? ", reopen refused" : "", device_opens, device_frees,
         (int)active);
   if (     start_refusals
         || open_refusals
         || device_opens != 2
         || device_frees != 1
         || audio_driver_kept.driver
         || active == reopen_fails
         || (reopen_fails != (st->context_audio_data == NULL)))
      failures++;
   audio_driver_deinit();
   if (device_frees != (reopen_fails ? 1u : 2u))
      failures++;
}

/* What is not taken back is freed: a device the next init would open
 * differently, one nobody takes, and one not asked to be kept. */
static void keep_refused_case(void)
{
   settings_t *settings = config_get_ptr();
   unsigned ok = 1;

   /* another latency: opened afresh, the kept one freed */
   device_opens = device_frees = 0;
   setup(true, false, false, 22050.0);
   audio_driver_keep_device(true);
   audio_driver_deinit();
   settings->uints.audio_latency = 96;
   if (!audio_driver_init_internal(settings, false))
      abort();
   audio_driver_drop_kept();
   ok &= device_opens == 2 && device_frees == 1;
   audio_driver_deinit();

   /* nobody takes it */
   device_opens = device_frees = 0;
   setup(true, false, false, 22050.0);
   audio_driver_keep_device(true);
   audio_driver_deinit();
   audio_driver_drop_kept();
   ok &= device_opens == 1 && device_frees == 1;

   /* not asked */
   device_opens = device_frees = 0;
   setup(true, false, false, 22050.0);
   audio_driver_deinit();
   ok &= device_frees == 1;

   /* switched off */
   device_opens = device_frees = 0;
   putenv((char*)"RETROARCH_AUDIO_KEEP=0");
   setup(true, false, false, 22050.0);
   audio_driver_keep_device(true);
   audio_driver_deinit();
   putenv((char*)"RETROARCH_AUDIO_KEEP=");
   ok &= device_frees == 1;

   printf("keep refused: %s\n", ok ? "each freed" : "LEAKED OR REUSED");
   if (!ok)
      failures++;
}

static void mixer_session_case(bool threaded, bool playing)
{
   static unsigned char bytes[64];
   audio_driver_state_t *st = &audio_driver_st;
   audio_mixer_stream_params_t params;
   audio_mixer_sound_t *sound;
   audio_mixer_voice_t *voice;
   void *lock;
   char *name;
   unsigned inits, dones, cycle;
   int slot = -1;
   bool ok;

   setup(true, threaded, false, 22050.0);
   memset(&params, 0, sizeof(params));
   params.buf                 = bytes;
   params.bufsize             = sizeof(bytes);
   params.basename            = "persistent music";
   params.volume              = 0.375f;
   params.slot_selection_type = AUDIO_MIXER_SLOT_SELECTION_AUTOMATIC;
   params.stream_type         = AUDIO_STREAM_TYPE_USER;
   params.type                = AUDIO_MIXER_TYPE_WAV_STREAM;
   params.state               = playing ? AUDIO_STREAM_STATE_PLAYING_LOOPED
                                       : AUDIO_STREAM_STATE_STOPPED;
   params.out_slot            = &slot;
   if (!audio_driver_mixer_add_stream(&params) || slot < 0)
      abort();
   sound = st->mixer_streams[slot].handle;
   voice = st->mixer_streams[slot].voice;
   name  = st->mixer_streams[slot].name;
   lock  = st->state_lock;
   inits = pipeline_mixer_inits;
   dones = pipeline_mixer_dones;
   /* Only atomics without pointer operations make the state lock. */
#ifdef AUDIO_DRIVER_PASS_EPOCH
   ok    = sound != NULL && lock == NULL && (playing == (voice != NULL));
#else
   ok    = sound != NULL && lock != NULL && (playing == (voice != NULL));
#endif

   for (cycle = 0; cycle < 3; cycle++)
   {
      audio_driver_keep_device(true);
      audio_driver_deinit_for_reinit();
      ok = ok && st->state_lock == lock;
      st->input = 32000.0f;
      video_state_get_ptr()->av_info.timing.sample_rate = 32000.0;
      if (!audio_driver_init_internal(config_get_ptr(), false))
         abort();
      /* A core can initialize audio before drivers_init repeats it. */
      if (!audio_driver_init_internal(config_get_ptr(), false))
         abort();
      ok = ok && pipeline_mixer_inits == inits
         && pipeline_mixer_dones == dones
         && st->mixer_streams[slot].handle == sound
         && st->mixer_streams[slot].voice == voice
         && st->mixer_streams[slot].name == name
         && st->mixer_streams[slot].volume == params.volume
         && st->mixer_streams[slot].state == params.state
         && st->state_lock == lock
         && (AUDIO_FLAGS_GET(st) & AUDIO_FLAG_MIXER_INITED);
   }
   audio_driver_deinit();
   ok = ok && pipeline_mixer_dones == dones + 1
      && !(AUDIO_FLAGS_GET(st) & AUDIO_FLAG_MIXER_INITED)
      && !st->mixer_streams[slot].handle
      && !st->mixer_streams[slot].voice;
   ok = ok && !st->state_lock;
   printf("mixer session, %s, %s: %s\n",
         threaded ? "threaded" : "inline", playing ? "playing" : "stopped",
         ok ? "retained until shutdown" : "LOST");
   if (!ok)
      failures++;
}

int main(void)
{
   unsigned format, threaded;
   audio_null.init          = test_init;
   audio_null.write         = test_write;
   audio_null.free          = test_free;
   audio_null.start         = test_start;
   audio_null.stop          = test_stop;
   audio_null.use_float     = test_float;
   audio_null.write_avail   = test_avail;
   audio_null.buffer_size   = test_avail;
   audio_null.wait_writable = test_wait;

   mixer_session_case(false, false);
   mixer_session_case(false, true);
   mixer_session_case(true, false);
   mixer_session_case(true, true);

   for (format = 0; format < 2; format++)
      for (threaded = 0; threaded < 2; threaded++)
      {
         tone_case(format != 0, threaded != 0, false, 22050.0, 32000.0);
         tone_case(format != 0, threaded != 0, false, 22050.0, 11025.0);
      }
   /* The ring at 11025 Hz holds 1024 frames, short of three video
    * frames at either of these: the consumer moves it to a larger one
    * the caller made. */
   tone_case(true, true, false, 11025.0, 22050.0);
   tone_case(false, true, false, 11025.0, 48000.0);
   tone_case(true, false, false, 11025.0, 22050.0);
   tone_case(false, false, true, 11025.0, 32000.0);
   tone_case(false, true,  true, 32000.0, 22050.0);
   /* A stopped driver's consumer runs no pass: installed by the caller. */
   change_parked = true;
   tone_case(true, true, false, 11025.0, 22050.0);
   change_parked = false;

   refused_case("inline time stretch", false, true, 22050.0, 32000.0);
   refused_case("threaded time stretch", true, true, 22050.0, 32000.0);

   /* 22050 Hz at 60 fps holds 1024 frames; at 15 fps a frame is 1470. */
   fps_case(22050.0, 60.0, 15.0);
   fps_case(22050.0, 60.0, 50.0);

   keep_case(false);
   keep_case(true);
   keep_refused_case();
   keep_restart_refused_case(false);
   keep_restart_refused_case(true);

   second_init_case(false);
   second_init_case(true);

   if (handovers != 9)
   {
      printf("FAIL: %u changes handed to the consumer, not 9\n", handovers);
      failures++;
   }
   printf("%u failures\n", failures);
   return failures ? 1 : 0;
}
