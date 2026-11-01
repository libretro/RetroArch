/* The menu's silence feed at Menu Frame Rate 'Display Rate' behind a
 * core that uses an audio callback.
 *
 * Such a core runs behind the threaded wrapper, which ignores
 * set_nonblock_state(): the device stays blocking for the audio
 * thread. The menu feeds silence by the clock, so a write that waits
 * on the device comes back as time owed on the next feed, the next
 * write waits as long, and the menu stays held to the device at
 * whatever rate the first wait set. The feed therefore writes only
 * what the device takes now and keeps half its buffer, as the
 * blocking path does.
 *
 * The device here is a ring that counts a write larger than its free
 * space as one that would have blocked. Held on the shipping
 * audio_driver_menu_sample() from audio/audio_driver.c: a full device
 * takes nothing and is not waited on, a drained one is topped up to
 * its half, a steady run of menu frames neither blocks nor starves it,
 * and the recording keeps every frame of the time that passed. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../../audio/audio_driver.c"

#define OUT_RATE   48000
#define LATENCY_MS 64

static size_t   dev_capacity;
static size_t   dev_fill;
static unsigned dev_writes, dev_would_block;
static unsigned failures;
static size_t   recorded_frames;

static bool test_record(void *data, const struct record_audio_data *audio)
{
   (void)data;
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
   dev_capacity = (size_t)latency * rate / 1000 * 2 * sizeof(int16_t);
   dev_fill     = 0;
   *new_rate    = rate;
   return &dev_capacity;
}

static ssize_t test_write(void *data, const void *buf, size_t len)
{
   (void)data; (void)buf;
   dev_writes++;
   if (len > dev_capacity - dev_fill)
   {
      /* A blocking device would wait here for room */
      dev_would_block++;
      dev_fill = dev_capacity;
      return (ssize_t)len;
   }
   dev_fill += len;
   return (ssize_t)len;
}

static size_t test_avail(void *data)  { (void)data; return dev_capacity - dev_fill; }
static size_t test_size(void *data)   { (void)data; return dev_capacity; }
static size_t test_wait(void *data, size_t len)
{ (void)data; (void)len; return dev_capacity - dev_fill; }
static bool   test_stop(void *data)   { (void)data; return true; }
static bool   test_start(void *data, bool s) { (void)data; (void)s; return true; }
static bool   test_alive(void *data)  { (void)data; return true; }
static bool   test_float(void *data)  { (void)data; return false; }
static void   test_free(void *data)   { (void)data; }
/* The wrapper's: blocking state is the audio thread's, not the menu's */
static void   test_nonblock(void *data, bool state) { (void)data; (void)state; }

static void test_core_callback(void) { }

static size_t ms_bytes(unsigned ms)
{
   return (size_t)ms * OUT_RATE / 1000 * 2 * sizeof(int16_t);
}

static void drain(size_t bytes)
{
   dev_fill = (bytes > dev_fill) ? 0 : dev_fill - bytes;
}

/* One menu frame, @us after the last feed */
static void menu_frame(retro_time_t us)
{
   audio_driver_st.menu_feed_last = cpu_features_get_time_usec() - us;
   audio_driver_menu_sample(true);
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
   audio_driver_state_t *st     = &audio_driver_st;
   settings_t *settings         = config_get_ptr();
   recording_state_t *record_st = recording_state_get_ptr();
   size_t min_fill              = (size_t)-1;
   unsigned i, writes;

   audio_null.init               = test_init;
   audio_null.write              = test_write;
   audio_null.stop               = test_stop;
   audio_null.start              = test_start;
   audio_null.alive              = test_alive;
   audio_null.set_nonblock_state = test_nonblock;
   audio_null.free               = test_free;
   audio_null.use_float          = test_float;
   audio_null.write_avail        = test_avail;
   audio_null.buffer_size        = test_size;
   audio_null.wait_writable      = test_wait;

   memset(settings, 0, sizeof(*settings));
   settings->bools.audio_enable             = true;
   settings->bools.audio_sync               = true;
   settings->bools.audio_threaded_pipeline  = false;
   settings->uints.audio_output_sample_rate = OUT_RATE;
   settings->uints.audio_latency            = LATENCY_MS;
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
   /* What the wrapper stands in front of: a core with an audio callback,
    * paused behind the menu */
   st->callback.callback = test_core_callback;
   audio_driver_publish_runloop();
   record_st->data     = &recorded_frames;
   record_st->driver   = &test_recorder;
   record_st->channels = 2;
   record_st->layout   = AUDIO_LAYOUT_STEREO;

   printf("menu silence at display rate behind a callback core:\n");

   /* The core's audio thread left the device full when it paused */
   dev_fill = dev_capacity;
   menu_frame(0);
   writes = dev_writes;
   recorded_frames = 0;
   menu_frame(20000);
   expect("a full device takes nothing and is not waited on",
         dev_writes == writes && dev_would_block == 0);
   expect("the recording still gets the 20 ms that passed",
         recorded_frames >= 44100 / 50 - 1);

   drain(ms_bytes(LATENCY_MS / 4));
   writes = dev_writes;
   menu_frame(LATENCY_MS / 4 * 1000);
   expect("a device above half full is left to drain",
         dev_writes == writes && dev_would_block == 0);

   drain(dev_capacity);
   menu_frame(100000);
   expect("a drained device is topped up without a wait",
         dev_writes > writes && dev_would_block == 0);
   expect("and only to its half",
         dev_fill <= dev_capacity / 2 + ms_bytes(1));

   /* A 300 Hz menu: each frame the device plays what passed */
   for (i = 0; i < 3000; i++)
   {
      drain(ms_bytes(10) / 3);
      if (i > 30 && dev_fill < min_fill)
         min_fill = dev_fill;
      menu_frame(3333);
   }
   expect("a steady menu never waits on the device", dev_would_block == 0);
   expect("and keeps it fed", min_fill > ms_bytes(LATENCY_MS / 4));

   record_st->data   = NULL;
   record_st->driver = NULL;
   audio_driver_deinit();

   if (failures)
   {
      printf("menu silence behind a callback core: %u failure(s)\n", failures);
      return 1;
   }
   printf("menu silence behind a callback core: never blocks, keeps the device fed\n");
   return 0;
}
