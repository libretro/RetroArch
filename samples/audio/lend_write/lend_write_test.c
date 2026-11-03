/* The driver lend pair end to end: with write_begin/write_end on the
 * driver, the pipeline - threaded, or inline on the core's thread -
 * produces the resampler's output and
 * the format conversion inside the driver's own buffer and publishes
 * it, and write() is never called; the published stream is compared
 * bit for bit against the same input through the same driver with the
 * pair removed, so the lend changes where the bytes are produced and
 * nothing about the bytes.  A driver that cuts the span short is fed
 * the same input and must fall back to write() with the stream intact,
 * and every begin must be balanced by an end. The same holds for a
 * wider device, whose frames the upmix produces in the lent span, and
 * for Headphone Virtual Surround, whose binaural render does. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../../audio/audio_driver.c"

#define FRAMES   800
#define CAPTURE  (FRAMES * 2 + 256)
static int16_t source_i[FRAMES * 2];
/* room for a device of up to eight channels */
static uint8_t captured[CAPTURE * 8 * sizeof(float)];
static size_t  captured_bytes, device_bytes;
static bool    device_float;
static unsigned failures;

/* The scripted device: a flat capture area lent span by span. */
static size_t  lent_at, lent_len;   /* outstanding lend */
static bool    lending;
static unsigned begin_calls, end_calls, write_calls;
static size_t  span_cap;            /* 0: full spans; else cut to this */
/* the layout the device opens with, and Headphone Virtual Surround */
static uint32_t dev_layout = AUDIO_LAYOUT_STEREO;
static bool     virtual_surround;

static uint32_t test_layout(void *data) { (void)data; return dev_layout; }

static void *test_init(const char *device, unsigned rate, unsigned latency,
      unsigned *new_rate)
{
   (void)device;
   device_bytes = (size_t)latency * rate / 1000
         * audio_layout_channels(dev_layout)
         * (device_float ? sizeof(float) : sizeof(int16_t));
   *new_rate = rate;
   return &device_bytes;
}

static ssize_t test_write(void *data, const void *buf, size_t len)
{
   (void)data;
   if (lending)
      abort();          /* write() inside an outstanding lend */
   if (captured_bytes + len > sizeof(captured))
      abort();
   memcpy(captured + captured_bytes, buf, len);
   captured_bytes += len;
   write_calls++;
   return len;
}

static size_t test_begin(void *data, size_t len, void **region)
{
   (void)data;
   *region = NULL;
   if (lending)
      abort();
   begin_calls++;
   if (captured_bytes >= sizeof(captured))
      return 0;
   if (len > sizeof(captured) - captured_bytes)
      len = sizeof(captured) - captured_bytes;
   if (span_cap && len > span_cap)
      len = span_cap;
   if (!len)
      return 0;
   lending = true;
   lent_at = captured_bytes;
   lent_len = len;
   *region = captured + lent_at;
   return len;
}

static ssize_t test_end(void *data, size_t len)
{
   (void)data;
   if (!lending || len > lent_len)
      abort();
   lending = false;
   end_calls++;
   captured_bytes = lent_at + len;
   return (ssize_t)len;
}

static size_t test_avail(void *data) { (void)data; return device_bytes; }
static size_t test_wait(void *data, size_t len)
{ (void)data; (void)len; return device_bytes; }
static bool test_float(void *data) { (void)data; return device_float; }
static bool test_start(void *data, bool shutdown)
{ (void)data; (void)shutdown; return true; }
static void test_free(void *data) { (void)data; }

static bool threaded = true;

static void run_stream(bool dev_float, bool with_lend, unsigned in_rate,
      size_t cap, uint8_t *out, size_t *out_bytes)
{
   audio_driver_state_t *st = &audio_driver_st;
   settings_t *settings     = config_get_ptr();
   unsigned passes = 0;

   device_float = dev_float;
   span_cap     = cap;
   audio_null.write_begin = with_lend ? test_begin : NULL;
   audio_null.write_end   = with_lend ? test_end   : NULL;

   memset(settings, 0, sizeof(*settings));
   settings->bools.audio_enable             = true;
   settings->bools.audio_sync               = true;
   settings->bools.audio_threaded_pipeline  = threaded;
   settings->uints.audio_output_sample_rate = 48000;
   settings->uints.audio_latency            = 64;
   settings->floats.slowmotion_ratio        = 1.0f;
   strcpy(settings->arrays.audio_resampler, "sinc");
   settings->uints.audio_resampler_quality  = RESAMPLER_QUALITY_NORMAL;
   settings->bools.audio_headphone_virtual_surround = virtual_surround;

   st->input       = in_rate;
   st->volume_gain = 1.0f;
   audio_driver_set_core_float(false);
   video_state_get_ptr()->av_info.timing.fps         = 60.0;
   video_state_get_ptr()->av_info.timing.sample_rate = in_rate;
   if (!audio_driver_init_internal(settings, false))
      abort();
   audio_driver_publish_runloop();
   st->pipe_priming = false;
   /* the stage under test is the one running */
   if (     st->out_channels != audio_layout_channels(dev_layout)
         || st->virtualize != virtual_surround)
   {
      printf("FAIL: the device opened %u channels, virtual surround %s\n",
            st->out_channels, st->virtualize ? "on" : "off");
      failures++;
   }

   captured_bytes = 0;
   begin_calls = end_calls = write_calls = 0;
   memset(captured, 0, sizeof(captured));
   audio_driver_sample_batch(source_i, FRAMES);
   if (threaded != st->pipe_threaded)
   {
      printf("FAIL: the pipeline is %s, %s asked for\n",
            st->pipe_threaded ? "threaded" : "inline",
            threaded ? "threaded" : "inline");
      failures++;
   }
   /* Inline, the batch above was flushed as it came */
   while (st->pipe_threaded && retro_spsc_read_avail(&st->pipe_ring))
   {
      if (++passes > FRAMES)
         abort();
      audio_driver_pipeline_consume(st);
   }
   if (lending)
   {
      printf("FAIL: a lend was left outstanding\n");
      failures++;
      lending = false;
   }
   if (begin_calls != end_calls)
   {
      printf("FAIL: %u begins against %u ends\n", begin_calls, end_calls);
      failures++;
   }
   memcpy(out, captured, captured_bytes);
   *out_bytes = captured_bytes;
   audio_driver_deinit();
}

static uint8_t ref[sizeof(captured)], got[sizeof(captured)];

static void compare_case(const char *what, bool dev_float, unsigned in_rate,
      size_t cap, bool expect_writes)
{
   size_t ref_bytes = 0, got_bytes = 0;
   run_stream(dev_float, false, in_rate, 0,  ref, &ref_bytes);
   run_stream(dev_float, true,  in_rate, cap, got, &got_bytes);
   if (     got_bytes != ref_bytes
         || memcmp(got, ref, ref_bytes)
         || (expect_writes ? 0 : write_calls))
   {
      printf("FAIL %s: bytes %u vs %u, %s, write() %u times\n",
            what, (unsigned)got_bytes, (unsigned)ref_bytes,
            memcmp(got, ref, ref_bytes) ? "streams differ" : "streams equal",
            write_calls);
      failures++;
   }
   else
      printf("%s: the lent stream is the written stream, write() %u times\n",
            what, write_calls);
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
   audio_null.layout        = test_layout;
   for (i = 0; i < FRAMES * 2; i++)
      source_i[i] = (int16_t)((int)(i % 1000) - 500);

   /* Real resampling into the lent span, float and int16 devices. */
   compare_case("float device, 32k -> 48k", true,  32000, 0, false);
   compare_case("s16 device, 32k -> 48k",   false, 32000, 0, false);
   /* Rate-matched: the identity write abandons the lend and goes raw. */
   compare_case("s16 device, unity",        false, 48000, 0, true);
   /* A driver that cuts every span short: the staged write covers it. */
   compare_case("short spans",              true,  32000, 64, true);

   /* The same, inline: the flush on the core's thread borrows the
    * driver's buffer too, after waiting for the room write() would
    * have waited for. */
   threaded = false;
   compare_case("inline, float device, 32k -> 48k", true,  32000, 0, false);
   compare_case("inline, s16 device, 32k -> 48k",   false, 32000, 0, false);
   compare_case("inline, s16 device, unity",        false, 48000, 0, true);
   compare_case("inline, short spans",              true,  32000, 64, true);

   /* A wider device: the upmix produces its frames in the driver's
    * buffer, and the core's stereo through the binaural render does on
    * headphones - threaded and inline, float and int16 devices. */
   for (i = 0; i < 2; i++)
   {
      char what[96];
      threaded = i == 0;
      dev_layout = AUDIO_LAYOUT_5POINT1;
      snprintf(what, sizeof(what), "%s5.1 device, float", threaded ? "" : "inline, ");
      compare_case(what, true,  32000, 0, false);
      snprintf(what, sizeof(what), "%s5.1 device, s16", threaded ? "" : "inline, ");
      compare_case(what, false, 32000, 0, false);
      snprintf(what, sizeof(what), "%s5.1 device, s16, unity", threaded ? "" : "inline, ");
      compare_case(what, false, 48000, 0, false);
      snprintf(what, sizeof(what), "%s5.1 device, short spans", threaded ? "" : "inline, ");
      compare_case(what, true,  32000, 64, true);
      dev_layout = AUDIO_LAYOUT_STEREO;
      virtual_surround = true;
      snprintf(what, sizeof(what), "%sheadphone surround, float", threaded ? "" : "inline, ");
      compare_case(what, true,  32000, 0, false);
      snprintf(what, sizeof(what), "%sheadphone surround, s16", threaded ? "" : "inline, ");
      compare_case(what, false, 32000, 0, false);
      snprintf(what, sizeof(what), "%sheadphone surround, short spans", threaded ? "" : "inline, ");
      compare_case(what, false, 32000, 64, true);
      virtual_surround = false;
   }

   if (failures)
   {
      printf("%u failure(s)\n", failures);
      return 1;
   }
   printf("lend write: the driver's buffer takes the output directly, and the bytes do not change\n");
   return 0;
}
