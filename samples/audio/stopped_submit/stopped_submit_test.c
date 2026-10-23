/* Core audio against a stopped driver, on the direct (unthreaded)
 * pipeline with audio sync on - the blocking configuration.
 *
 * A driver that audio_driver_stop() has stopped drains nothing, so a
 * blocking write to it never returns; a core that emits audio from
 * retro_serialize() or retro_unload_game() after the driver was
 * stopped would hang the frontend there. The gates on the core entry
 * points drop that audio instead, and let it through again after
 * audio_driver_start(). Held here on the shipping functions from
 * audio/audio_driver.c, for the int16 batch, the float batch and the
 * per-sample entry, with a device that records every write and the
 * driver's running state at the time. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../../audio/audio_driver.c"

#define FRAMES 800
static int16_t  source_i[FRAMES * 2];
static float    source_f[FRAMES * 2];
static size_t   device_bytes;
static unsigned write_calls, writes_while_stopped;
static bool     device_running;
static unsigned failures;
static size_t recorded_frames, record_sample_index;
static bool record_bad;

static bool test_record(void *data, const struct record_audio_data *audio)
{
   const int16_t *samples = (const int16_t*)audio->data;
   size_t i;
   if (data != &recorded_frames || !samples || !audio->frames)
      record_bad = true;
   else
      for (i = 0; i < audio->frames * 2; i++, record_sample_index++)
         if (samples[i] != source_i[record_sample_index % (FRAMES * 2)])
            record_bad = true;
   recorded_frames += audio->frames;
   return true;
}

static const record_driver_t test_recorder = {
   NULL, NULL, NULL, test_record, NULL, "test"
};

static void *test_init(const char *device, unsigned rate, unsigned latency,
      unsigned *new_rate)
{
   (void)device;
   device_bytes   = (size_t)latency * rate / 1000 * 2 * sizeof(int16_t);
   device_running = true;
   *new_rate      = rate;
   return &device_bytes;
}

static ssize_t test_write(void *data, const void *buf, size_t len)
{
   (void)data; (void)buf;
   write_calls++;
   if (!device_running)
      writes_while_stopped++;
   return (ssize_t)len;
}

static bool test_stop(void *data)
{
   (void)data;
   device_running = false;
   return true;
}

static bool test_start(void *data, bool shutdown)
{
   (void)data; (void)shutdown;
   device_running = true;
   return true;
}

static bool test_alive(void *data) { (void)data; return device_running; }
static size_t test_avail(void *data) { (void)data; return device_bytes; }
static size_t test_wait(void *data, size_t len)
{ (void)data; (void)len; return device_bytes; }
static bool test_float(void *data) { (void)data; return false; }
static void test_free(void *data) { (void)data; }
static void test_nonblock(void *data, bool state) { (void)data; (void)state; }

static void submit_all(void)
{
   unsigned i;
   record_sample_index = 0;
   audio_driver_sample_batch(source_i, FRAMES);
   record_sample_index = 0;
   audio_driver_sample_batch_float(source_f, FRAMES);
   record_sample_index = 0;
   for (i = 0; i < AUDIO_SAMPLE_ACCUM_INT16S; i += 2)
      audio_driver_sample(source_i[i % (FRAMES * 2)],
            source_i[(i + 1) % (FRAMES * 2)]);
}

static void expect(const char *what, bool ok)
{
   if (ok)
      printf("   ok   %s\n", what);
   else
   {
      printf("   FAIL %s\n", what);
      failures++;
   }
}

int main(void)
{
   audio_driver_state_t *st = &audio_driver_st;
   settings_t *settings     = config_get_ptr();
   unsigned before;
   size_t i, before_recorded;
   size_t expected_frames = FRAMES * 2 + AUDIO_SAMPLE_ACCUM_INT16S / 2;
   recording_state_t *record_st = recording_state_get_ptr();

   audio_null.init              = test_init;
   audio_null.write             = test_write;
   audio_null.stop              = test_stop;
   audio_null.start             = test_start;
   audio_null.alive             = test_alive;
   audio_null.set_nonblock_state = test_nonblock;
   audio_null.free              = test_free;
   audio_null.use_float         = test_float;
   audio_null.write_avail       = test_avail;
   audio_null.buffer_size       = test_avail;
   audio_null.wait_writable     = test_wait;
   for (i = 0; i < FRAMES * 2; i++)
   {
      source_i[i] = (int16_t)((int)(i % 1000) - 500);
      source_f[i] = source_i[i] * (1.0f / 32768.0f);
   }

   memset(settings, 0, sizeof(*settings));
   settings->bools.audio_enable             = true;
   settings->bools.audio_sync               = true;
   settings->bools.audio_threaded_pipeline  = false;
   settings->uints.audio_output_sample_rate = 48000;
   settings->uints.audio_latency            = 64;
   settings->floats.slowmotion_ratio        = 1.0f;
   strcpy(settings->arrays.audio_resampler, "sinc");
   settings->uints.audio_resampler_quality  = RESAMPLER_QUALITY_NORMAL;

   st->input       = 44100;
   st->volume_gain = 1.0f;
   audio_driver_set_core_float(false);
   video_state_get_ptr()->av_info.timing.fps         = 60.0;
   video_state_get_ptr()->av_info.timing.sample_rate = 44100;
   if (!audio_driver_init_internal(settings, false))
      abort();
   audio_driver_publish_runloop();
   record_st->data = &recorded_frames;
   record_st->driver = &test_recorder;
   record_st->channels = 2;
   record_st->layout = AUDIO_LAYOUT_STEREO;

   printf("core audio against a stopped driver:\n");

   submit_all();
   expect("a driver that came up running takes core audio", write_calls > 0);
   expect("recording receives complete PCM from all core callback styles",
         recorded_frames == expected_frames && !record_bad);

   expect("audio_driver_stop() stops the driver", audio_driver_stop()
         && !device_running);
   before = write_calls;
   before_recorded = recorded_frames;
   submit_all();
   expect("core audio while stopped reaches no write",
         write_calls == before && writes_while_stopped == 0);
   expect("recording keeps complete PCM while device output is stopped",
         recorded_frames == before_recorded + expected_frames && !record_bad);

   expect("audio_driver_start() starts it again", audio_driver_start(false)
         && device_running);
   before = write_calls;
   before_recorded = recorded_frames;
   submit_all();
   expect("core audio flows again after the start", write_calls > before
         && writes_while_stopped == 0);

   expect("recording keeps complete PCM after device output resumes",
         recorded_frames == before_recorded + expected_frames && !record_bad);
   record_st->data = NULL;
   record_st->driver = NULL;
   audio_driver_deinit();

   if (failures)
   {
      printf("core audio against a stopped driver: %u failure(s)\n", failures);
      return 1;
   }
   printf("core audio against a stopped driver: dropped while stopped, written again once started\n");
   return 0;
}
