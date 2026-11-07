/* The unity passthrough end to end: an int16 batch at a pinned ratio
 * of 1.0 reaches an s16 device with no conversion and no staging, on
 * the float path, the integer front pair and the threaded pipeline
 * alike.  The conversions are counted through linker wraps, so a pass
 * that slipped back in is a failure, not just a slowdown; the output is
 * compared to the source bit for bit, which also proves the resampler
 * never ran - its unity-ratio response is not an identity.  The staged
 * fallbacks (a float device, non-unity gain, the resume ramp, rate
 * control) are asserted to still take the conversions they need. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../../audio/audio_driver.c"

#define FRAMES  800
/* Room for rate control's excursion above unity. */
#define CAPTURE (FRAMES + 64)
static int16_t source_i[FRAMES * 2];
static float   source_f[FRAMES * 2];
static int16_t captured_i[CAPTURE * 2];
static float   captured_f[CAPTURE * 2];
static size_t  captured_samples, device_bytes;
static bool    device_float;
static unsigned failures;

static unsigned s16_to_float_calls, float_to_s16_calls;
void __real_convert_s16_to_float(float *out, const int16_t *in,
      size_t samples, float gain);
void __real_convert_float_to_s16(int16_t *out, const float *in,
      size_t samples);
void __wrap_convert_s16_to_float(float *out, const int16_t *in,
      size_t samples, float gain)
{
   s16_to_float_calls++;
   __real_convert_s16_to_float(out, in, samples, gain);
}
void __wrap_convert_float_to_s16(int16_t *out, const float *in,
      size_t samples)
{
   float_to_s16_calls++;
   __real_convert_float_to_s16(out, in, samples);
}

static void *test_init(const char *device, unsigned rate, unsigned latency,
      unsigned *new_rate)
{
   (void)device;
   device_bytes = (size_t)latency * rate / 1000 * 2
         * (device_float ? sizeof(float) : sizeof(int16_t));
   *new_rate = rate;
   return &device_bytes;
}

static ssize_t test_write(void *data, const void *buf, size_t len)
{
   size_t sample = device_float ? sizeof(float) : sizeof(int16_t);
   (void)data;
   if (captured_samples + len / sample > CAPTURE * 2)
      abort();
   if (device_float)
      memcpy(captured_f + captured_samples, buf, len);
   else
      memcpy(captured_i + captured_samples, buf, len);
   captured_samples += len / sample;
   return len;
}

static size_t test_avail(void *data) { (void)data; return device_bytes; }
static size_t test_wait(void *data, size_t len)
{ (void)data; (void)len; return device_bytes; }
static bool test_float(void *data) { (void)data; return device_float; }
static bool test_start(void *data, bool shutdown)
{ (void)data; (void)shutdown; return true; }
static void test_free(void *data) { (void)data; }

static void counters_reset(void)
{
   captured_samples   = 0;
   s16_to_float_calls = 0;
   float_to_s16_calls = 0;
   memset(captured_i, 0, sizeof(captured_i));
   memset(captured_f, 0, sizeof(captured_f));
}

static void setup(bool dev_float, bool threaded, bool rate_control,
      bool fastpath_s16, float volume)
{
   audio_driver_state_t *st = &audio_driver_st;
   settings_t *settings     = config_get_ptr();

   device_float = dev_float;
   memset(settings, 0, sizeof(*settings));
   settings->bools.audio_enable             = true;
   settings->bools.audio_sync               = true;
   settings->bools.audio_threaded_pipeline  = threaded;
   settings->bools.audio_rate_control       = rate_control;
   settings->bools.audio_fastpath_s16       = fastpath_s16;
   settings->uints.audio_output_sample_rate = 48000;
   settings->uints.audio_latency            = 64;
   settings->floats.slowmotion_ratio        = 1.0f;
   strcpy(settings->arrays.audio_resampler, "sinc");
   settings->uints.audio_resampler_quality  = RESAMPLER_QUALITY_NORMAL;

   st->input       = 48000;
   st->volume_gain = volume;
   audio_driver_set_core_float(false);
   video_state_get_ptr()->av_info.timing.fps         = 60.0;
   video_state_get_ptr()->av_info.timing.sample_rate = 48000;
   if (!audio_driver_init_internal(settings, false))
      abort();
   audio_driver_publish_runloop();
   st->pipe_priming       = false;
   st->rate_control_delta = rate_control ? 0.005f : 0.0f;
   counters_reset();
}

static void batch(bool threaded)
{
   audio_driver_state_t *st = &audio_driver_st;
   unsigned passes = 0;
   audio_driver_sample_batch(source_i, FRAMES);
   if (threaded)
      while (retro_spsc_read_avail(&st->pipe_ring))
      {
         if (++passes > FRAMES)
            abort();
         audio_driver_pipeline_consume(st);
      }
}

static unsigned mismatched_i(void)
{
   size_t i;
   unsigned m = 0;
   for (i = 0; i < FRAMES * 2; i++)
      if (captured_i[i] != source_i[i])
         m++;
   return m;
}

static void identity_case(bool threaded, bool fastpath_s16)
{
   audio_driver_state_t *st = &audio_driver_st;
   const char *what = fastpath_s16 ? "integer front pair"
         : (threaded ? "threaded pipeline" : "inline");
   setup(false, threaded, false, fastpath_s16, 1.0f);
   batch(threaded);
   if (     captured_samples != FRAMES * 2 || mismatched_i()
         || s16_to_float_calls || float_to_s16_calls
         || !st->resampler_bypassed)
   {
      printf("FAIL %s: frames=%u mismatches=%u s16->f=%u f->s16=%u bypassed=%d\n",
            what, (unsigned)(captured_samples / 2), mismatched_i(),
            s16_to_float_calls, float_to_s16_calls,
            (int)st->resampler_bypassed);
      failures++;
   }
   else
      printf("%s: the batch reached the device as it is\n", what);
   audio_driver_deinit();
}

static void float_device_case(void)
{
   size_t i;
   unsigned m = 0;
   setup(true, false, false, false, 1.0f);
   batch(false);
   for (i = 0; i < FRAMES * 2; i++)
      if (captured_f[i] != source_f[i])
         m++;
   if (     captured_samples != FRAMES * 2 || m
         || s16_to_float_calls != 1 || float_to_s16_calls)
   {
      printf("FAIL float device: frames=%u mismatches=%u s16->f=%u f->s16=%u\n",
            (unsigned)(captured_samples / 2), m,
            s16_to_float_calls, float_to_s16_calls);
      failures++;
   }
   else
      printf("float device: one conversion, straight to the output\n");
   audio_driver_deinit();
}

/* The integer front pair with a master gain, which now carries the batch
 * into the output scratch in one pass instead of a copy and a gain over
 * it: every sample must still be the Q16 multiply of the source - round
 * half away from zero, saturated - with no conversion on the way, at an
 * attenuation and at a boost that saturates. */
static void integer_gain_case(float volume)
{
   audio_driver_state_t *st = &audio_driver_st;
   int32_t gain_q16 = (int32_t)(volume * 65536.0f + 0.5f);
   unsigned m = 0;
   size_t i;
   setup(false, false, false, true, volume);
   batch(false);
   for (i = 0; i < FRAMES * 2; i++)
   {
      int64_t p = (int64_t)source_i[i] * gain_q16;
      int64_t v = (p >= 0) ? ((p + 0x8000) >> 16) : -(((-p + 0x8000) >> 16));
      if (v > 32767) v = 32767; else if (v < -32768) v = -32768;
      if (captured_i[i] != (int16_t)v)
         m++;
   }
   if (     captured_samples != FRAMES * 2 || m
         || s16_to_float_calls || float_to_s16_calls
         || !st->resampler_bypassed)
   {
      printf("FAIL integer gain %.2f: frames=%u mismatches=%u s16->f=%u f->s16=%u bypassed=%d\n",
            volume, (unsigned)(captured_samples / 2), m,
            s16_to_float_calls, float_to_s16_calls,
            (int)st->resampler_bypassed);
      failures++;
   }
   else
      printf("integer gain %.2f: the Q16 multiply bit for bit\n",
            volume);
   audio_driver_deinit();
}

extern bool test_synth_on, test_synth_silent;
static float synth_at(size_t i) { return (float)((int)(i % 64) - 32) / 256.0f; }

/* An in-process MIDI synth summed into the game audio. On the integer
 * front pair the sum is written from the game audio into the writable
 * view in one pass; for a float core in the pass that brings its audio
 * in. Both must give what the copy and then the add gave, and a synth
 * that renders nothing must leave the batch on its passthrough. */
static void synth_case(bool fastpath_s16, bool core_float, bool silent)
{
   const char *what = silent
         ? (fastpath_s16 ? "silent synth, integer front pair"
                         : "silent synth, float path")
         : core_float ? "synth, float core" : "synth, integer front pair";
   unsigned m = 0;
   size_t i;
   test_synth_on     = true;
   test_synth_silent = silent;
   setup(core_float, false, false, fastpath_s16, 1.0f);
   if (core_float)
   {
      audio_driver_set_core_float(true);
      audio_driver_sample_batch_float(source_f, FRAMES);
   }
   else
      batch(false);
   for (i = 0; i < FRAMES * 2; i++)
   {
      if (silent)
         m += captured_i[i] != source_i[i];
      else if (core_float)
         m += captured_f[i] != (float)(source_f[i] * 1.0f) + synth_at(i) * 1.0f;
      else
      {
         int32_t v = (int32_t)source_i[i] + audio_float_to_s16_sat(synth_at(i));
         if (v > 32767) v = 32767; else if (v < -32768) v = -32768;
         m += fastpath_s16 && captured_i[i] != (int16_t)v;
      }
   }
   if (captured_samples != FRAMES * 2 || m
         || (silent && (s16_to_float_calls || float_to_s16_calls)))
   {
      printf("FAIL %s: frames=%u mismatches=%u s16->f=%u f->s16=%u\n", what,
            (unsigned)(captured_samples / 2), m,
            s16_to_float_calls, float_to_s16_calls);
      failures++;
   }
   else
      printf("%s: %s\n", what, silent
            ? "the batch stays on its passthrough"
            : "the sum as the copy and the add left it");
   audio_driver_deinit();
   audio_driver_set_core_float(false);
   test_synth_on = test_synth_silent = false;
}

static void staged_case(const char *what, bool rate_control, float volume,
      unsigned fade_frames, bool expect_bypassed)
{
   audio_driver_state_t *st = &audio_driver_st;
   setup(false, false, rate_control, false, volume);
   /* The whole ramp, so its head attenuates the first sample. */
   st->fade_in_frames = fade_frames ? AUDIO_PAUSE_TAIL_FRAMES : 0;
   batch(false);
   if (     !captured_samples
         || (!s16_to_float_calls && !float_to_s16_calls)
         || st->resampler_bypassed != expect_bypassed)
   {
      printf("FAIL %s: frames=%u s16->f=%u f->s16=%u bypassed=%d\n",
            what, (unsigned)(captured_samples / 2),
            s16_to_float_calls, float_to_s16_calls,
            (int)st->resampler_bypassed);
      failures++;
   }
   else if (fade_frames && captured_i[0] == source_i[0] && source_i[0])
   {
      printf("FAIL %s: the ramp left the first sample untouched\n", what);
      failures++;
   }
   else
   {
      /* The ramp spent, the identity write resumes. */
      if (fade_frames)
      {
         counters_reset();
         batch(false);
         if (mismatched_i() || s16_to_float_calls || float_to_s16_calls)
         {
            printf("FAIL %s: identity did not resume after the ramp\n", what);
            failures++;
         }
      }
      printf("%s: staged, with the conversions it needs\n", what);
   }
   audio_driver_deinit();
}

int main(void)
{
   size_t i;
   audio_null.init          = test_init;
   audio_null.write         = test_write;
   audio_null.free          = test_free;
   audio_null.start         = test_start;
   audio_null.use_float     = test_float;
   audio_null.write_avail   = test_avail;
   audio_null.buffer_size   = test_avail;
   audio_null.wait_writable = test_wait;
   for (i = 0; i < FRAMES * 2; i++)
   {
      source_i[i] = (int16_t)((int)(i % 1000) - 500);
      source_f[i] = (float)source_i[i] / 32768.0f;
   }
   identity_case(false, false);
   identity_case(true,  false);
   identity_case(false, true);
   float_device_case();
   staged_case("gain",         false, 0.5f, 0, true);
   integer_gain_case(0.5f);
   integer_gain_case(3.98f);
   synth_case(true,  false, false);
   synth_case(false, true,  false);
   synth_case(true,  false, true);
   synth_case(false, false, true);
   staged_case("resume ramp",  false, 1.0f, 8, true);
   staged_case("rate control", true,  1.0f, 0, false);
   if (failures)
   {
      printf("%u failure(s)\n", failures);
      return 1;
   }
   printf("unity identity: the passthrough keeps one buffer end to end, and every fallback still converts\n");
   return 0;
}
