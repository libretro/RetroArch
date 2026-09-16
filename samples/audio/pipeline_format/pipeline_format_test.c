/* Exercise real init, batch submission and consumption without a device clock. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../../audio/audio_driver.c"

#define FRAMES 800
static float source_f[FRAMES * 2];
static int16_t source_i[FRAMES * 2];
static float captured[FRAMES * 2];
static size_t captured_samples, device_bytes;
static unsigned failures;
extern unsigned pipeline_control_calls;

static void *test_init(const char *device, unsigned rate, unsigned latency,
      unsigned *new_rate)
{
   (void)device;
   device_bytes = (size_t)latency * rate / 1000 * 2 * sizeof(float);
   *new_rate = rate;
   return &device_bytes;
}

static ssize_t test_write(void *data, const void *buf, size_t len)
{
   (void)data;
   if (captured_samples + len / sizeof(float) > FRAMES * 2)
      abort();
   memcpy(captured + captured_samples, buf, len);
   captured_samples += len / sizeof(float);
   return len;
}

static size_t test_avail(void *data) { (void)data; return device_bytes; }
static size_t test_wait(void *data, size_t len)
{ (void)data; (void)len; return device_bytes; }
static bool test_float(void *data) { (void)data; return true; }
static bool test_start(void *data, bool shutdown)
{ (void)data; (void)shutdown; return true; }
static void test_free(void *data) { (void)data; }

static void run_case(bool floating, bool threaded, unsigned latency)
{
   audio_driver_state_t *st = &audio_driver_st;
   settings_t *settings = config_get_ptr();
   size_t i, mismatches = 0;
   unsigned passes = 0;
   bool format_ok;

   memset(settings, 0, sizeof(*settings));
   settings->bools.audio_enable = true;
   settings->bools.audio_sync = true;
   settings->bools.audio_threaded_pipeline = threaded;
   settings->uints.audio_output_sample_rate = 48000;
   settings->uints.audio_latency = latency;
   settings->floats.slowmotion_ratio = 1.0f;
   strcpy(settings->arrays.audio_resampler, "sinc");
   settings->uints.audio_resampler_quality = RESAMPLER_QUALITY_NORMAL;

   st->input = 48000;
   st->volume_gain = 1.0f;
   audio_driver_set_core_float(floating);
   video_state_get_ptr()->av_info.timing.fps = 60.0;
   video_state_get_ptr()->av_info.timing.sample_rate = 48000;
   if (!audio_driver_init_internal(settings, false))
      abort();

   format_ok = !threaded || (st->pipe_float == floating &&
         st->pipe_frame_bytes == 2 * (floating ? sizeof(float) : sizeof(int16_t)));
   /* Priming affects scheduling, not this one-batch sample identity test. */
   st->pipe_priming = false;
   captured_samples = 0;
   memset(captured, 0, sizeof(captured));
   /* Defined padding makes accidental reads beyond converted samples visible. */
   if (st->pipe_conv)
      memset(st->pipe_conv, 0, AUDIO_PIPE_SLICE_INT16S * sizeof(float));
   if (floating)
      audio_driver_sample_batch_float(source_f, FRAMES);
   else
      audio_driver_sample_batch(source_i, FRAMES);
   if (threaded)
      while (retro_spsc_read_avail(&st->pipe_ring))
      {
         if (++passes > FRAMES)
            abort();
         audio_driver_pipeline_consume(st);
      }
   for (i = 0; i < FRAMES * 2; i++)
      if (captured[i] != source_f[i])
         mismatches++;
   printf("%s %s %u ms: format=%s frames=%u mismatched_samples=%u passes=%u\n",
         floating ? "float" : "int16", threaded ? "threaded" : "inline",
         latency, format_ok ? "OK" : "BAD", (unsigned)(captured_samples / 2),
         (unsigned)mismatches, passes);
   if (!format_ok || captured_samples != FRAMES * 2 || mismatches)
      failures++;
   audio_driver_deinit();
}

/* The stride against the width, when the float negotiation lands after
 * the pipe came up wide.
 *
 * audio_driver_set_core_float() takes the format when the ring is
 * still empty, which it must - the negotiation can arrive after init.
 * But it recomputed the stride for a stereo frame while the ring had
 * been built for the canonical wide one, so the two disagreed: a
 * stride of 8 bytes against a frame 11 slots across. Every count on
 * the pipe is in frames and only the edge speaks bytes, so what that
 * disagreement produces is a ring read at a fraction of the frames it
 * holds.
 *
 * The order here is the one that reaches it: multi negotiated first,
 * the pipe built wide, float negotiated after, on an empty ring. */
static void wide_then_float_case(void)
{
   audio_driver_state_t *st = &audio_driver_st;
   settings_t *settings     = config_get_ptr();
   size_t want;

   memset(settings, 0, sizeof(*settings));
   settings->bools.audio_enable            = true;
   settings->bools.audio_sync              = true;
   settings->bools.audio_threaded_pipeline = true;
   settings->uints.audio_output_sample_rate = 48000;
   settings->uints.audio_latency           = 64;
   settings->floats.slowmotion_ratio       = 1.0f;
   strcpy(settings->arrays.audio_resampler, "sinc");
   settings->uints.audio_resampler_quality = RESAMPLER_QUALITY_NORMAL;

   st->input       = 48000;
   st->volume_gain = 1.0f;
   video_state_get_ptr()->av_info.timing.fps         = 60.0;
   video_state_get_ptr()->av_info.timing.sample_rate = 48000;

   /* A core that took the multi-channel entry and int16 output. */
   audio_driver_set_core_multi(true);
   audio_driver_set_core_float(false);
   if (!audio_driver_init_internal(settings, false))
      abort();

   if (st->pipe_channels != AUDIO_PIPE_CANON_CHANNELS)
   {
      printf("wide+float: the pipe did not come up wide (%u channels)\n",
            st->pipe_channels);
      failures++;
      audio_driver_set_core_multi(false);
      return;
   }

   /* And the float entry lands afterwards, on an empty ring. */
   {
      audio_driver_t wrapper = *st->current_audio;
      const audio_driver_t *saved = st->current_audio;
      wrapper.ident = "audio-thread";
      st->current_audio = &wrapper;
      audio_driver_set_core_float(true);
      if (pipeline_control_calls != 1) failures++;
      st->current_audio = saved;
   }

   want = (size_t)st->pipe_channels * sizeof(float);
   if (st->pipe_frame_bytes != want)
   {
      printf("wide+float: stride is %u bytes for a %u-slot frame,"
             " should be %u\n",
            (unsigned)st->pipe_frame_bytes, st->pipe_channels,
            (unsigned)want);
      failures++;
   }
   else
      printf("wide+float: stride %u bytes for a %u-slot frame\n",
            (unsigned)st->pipe_frame_bytes, st->pipe_channels);

   /* Queued native samples cannot be reinterpreted by negotiation. */
   {
      float frame[AUDIO_PIPE_CANON_CHANNELS];
      size_t bytes = st->pipe_frame_bytes;
      memset(frame, 0, sizeof(frame));
      if (retro_spsc_write(&st->pipe_ring, frame, bytes) != bytes) abort();
      audio_driver_set_core_float(false);
      if (!st->pipe_float || st->pipe_frame_bytes != bytes) failures++;
      retro_spsc_skip(&st->pipe_ring, bytes);
      audio_driver_set_core_float(false);
      if (st->pipe_float || st->pipe_frame_bytes != AUDIO_PIPE_CANON_CHANNELS * sizeof(int16_t)) failures++;
   }
   audio_driver_set_core_multi(false);
   audio_driver_set_core_float(false);
}

int main(void)
{
   static const unsigned latencies[] = {0, 8, 16, 32, 64, 66, 67, 68, 80};
   unsigned i, format, threaded;
   audio_null.init = test_init;
   audio_null.write = test_write;
   audio_null.free = test_free;
   audio_null.start = test_start;
   audio_null.use_float = test_float;
   audio_null.write_avail = test_avail;
   audio_null.buffer_size = test_avail;
   audio_null.wait_writable = test_wait;
   for (i = 0; i < FRAMES * 2; i++)
   {
      source_i[i] = (int16_t)((int)(i % 1000) - 500);
      source_f[i] = (float)source_i[i] / 32768.0f;
   }
   for (format = 0; format < 2; format++)
      for (threaded = 0; threaded < 2; threaded++)
         for (i = 0; i < sizeof(latencies) / sizeof(latencies[0]); i++)
            run_case(format != 0, threaded != 0, latencies[i]);
   wide_then_float_case();
   printf("36 cases plus the wide-then-float one, %u failures\n", failures);
   return failures ? 1 : 0;
}
