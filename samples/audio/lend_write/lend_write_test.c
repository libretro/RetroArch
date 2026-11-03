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
/* a core that sends its own 5.1, each channel its own signal */
static int16_t source_51[FRAMES * 6];
static bool    core_surround;
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
/* 0: a flat area; else the device is a ring of this many bytes, whose
 * span stops where it wraps (the capture stays flat behind it) */
static size_t  ring_bytes;
/* ring_frames of the device's frames, plus ring_cut bytes: a ring set
 * per case in frames, whatever the device's format */
static size_t  ring_frames, ring_cut;
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
   if (ring_frames)
      ring_bytes = ring_frames * audio_layout_channels(dev_layout)
            * (device_float ? sizeof(float) : sizeof(int16_t)) + ring_cut;
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
   if (ring_bytes && len > ring_bytes - captured_bytes % ring_bytes)
      len = ring_bytes - captured_bytes % ring_bytes;
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
   if (core_surround)
      audio_driver_sample_batch_multi_int16(source_51, FRAMES, 6,
            AUDIO_LAYOUT_5POINT1);
   else
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

/* Each resampler's fit() is exact: for a stream at any state, after any
 * run, the input it names produces no more than the frames asked for,
 * and one input frame more produces more - so a span is filled to
 * within one frame's output, and the copy left at a wrap is that or
 * nothing. Sinc at every quality, with and without HQ oversampling;
 * nearest and cc made at one ratio and run at others, which takes cc
 * from its downsampler to its upsampler mid-stream. Ratios above and
 * below one, caps from one frame up. */
static unsigned fit_bad, fit_checked;

static void fit_backend(const char *ident, unsigned q, bool hq,
      double made_at, const double *ratios, unsigned n_ratios)
{
   static float in[4096 * 2], out[8192 * 2];
   static const unsigned caps[] = { 1, 2, 7, 25, 100, 999, 1200 };
   unsigned r, k, c, warm;
   for (k = 0; k < 4096 * 2; k++)
      in[k] = (float)((int)(k % 777) - 388) / 400.0f;
   for (r = 0; r < n_ratios; r++)
   {
      void *re = NULL;
      const retro_resampler_t *backend = NULL;
      struct resampler_data d;
      if (!retro_resampler_realloc_hq(&re, &backend, ident,
               (enum resampler_quality)q, made_at ? made_at : ratios[r], hq))
         continue;
      if (strcmp(backend->short_ident, ident))
      {
         printf("FAIL: asked for the %s resampler, got %s\n", ident,
               backend->short_ident);
         failures++;
         backend->free(re);
         return;
      }
      if (!backend->fit)
      {
         printf("FAIL: the %s resampler has no fit()\n", ident);
         failures++;
         backend->free(re);
         return;
      }
      memset(&d, 0, sizeof(d));
      d.ratio = ratios[r];
      for (c = 0; c < sizeof(caps) / sizeof(caps[0]); c++)
         for (warm = 0; warm < 4; warm++)
         {
            size_t fits, a, b;
            /* a run of odd length first: the state is wherever it lands;
             * none before the first fit, on the stream as made */
            d.data_in = in; d.data_out = out;
            d.input_frames = c || warm ? 131 + warm * 37 + c : 0;
            if (d.input_frames)
               backend->process(re, &d);
            fits = backend->fit(re, d.ratio, caps[c], 800);
            d.input_frames = fits;
            d.output_frames = 0;
            if (fits)
               backend->process(re, &d);
            a = d.output_frames;
            d.input_frames = 1;
            backend->process(re, &d);
            b = a + d.output_frames;
            fit_checked++;
            if (a > caps[c] || (fits < 800 && b <= caps[c]))
            {
               if (fit_bad++ < 5)
                  printf("FAIL fit: %s quality %u ratio %.4f cap %u: fit %u made %u, one more %u\n",
                        ident, q, ratios[r], caps[c], (unsigned)fits,
                        (unsigned)a, (unsigned)b);
            }
         }
      backend->free(re);
   }
}

static void fit_case(void)
{
   static const double ratios[] = { 1.5, 48000.0 / 44100.0, 0.98, 2.0 / 3.0, 1.0 / 1.1 };
   static const double wide[]   = { 1.5, 48000.0 / 44100.0, 0.98, 2.0 / 3.0,
      1.0 / 1.1, 0.5, 0.3, 3.7 };
   unsigned q, hq, m;
   static const double made_at[] = { 0.5, 1.0 };
   for (hq = 0; hq < 2; hq++)
      for (q = RESAMPLER_QUALITY_LOWEST; q <= RESAMPLER_QUALITY_HIGHEST; q++)
         fit_backend("sinc", q, hq != 0, 0.0, ratios,
               sizeof(ratios) / sizeof(ratios[0]));
   for (m = 0; m < 2; m++)
   {
      fit_backend("nearest", RESAMPLER_QUALITY_NORMAL, false, made_at[m],
            wide, sizeof(wide) / sizeof(wide[0]));
      fit_backend("cc", RESAMPLER_QUALITY_NORMAL, false, made_at[m],
            wide, sizeof(wide) / sizeof(wide[0]));
   }
   if (fit_bad)
   {
      printf("FAIL: %u of %u fits were not exact\n", fit_bad, fit_checked);
      failures++;
   }
   else
      printf("resampler fit: %u fits exact (sinc, nearest, cc), the span filled to within one frame's output\n",
            fit_checked);
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
   for (i = 0; i < FRAMES * 6; i++)
      source_51[i] = (int16_t)((int)((i * (7 + i % 6)) % 2000) - 1000);

   fit_case();

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

   /* A device whose ring wraps inside the span: the stereo int16
    * device's narrowing, the upmix and the binaural render are made
    * span by span either side of the wrap, and a float stereo device's
    * resampler takes the input that fills the first span and carries on
    * in the next; write() is not called - and a ring that cuts a frame
    * at its end sends that frame through write() with the stream
    * intact. The flush is 800 frames resampled to 1200; the ring wraps
    * 1000 frames in. */
   {
      size_t r;
      for (r = 0; r < 2; r++)
         for (i = 0; i < 2; i++)
         {
            char what[96];
            bool whole  = r == 0;
            threaded    = i == 0;
            ring_frames = 1000;
            ring_cut    = whole ? 0 : 2;
            snprintf(what, sizeof(what), "%swrapping ring%s, s16 device",
                  threaded ? "" : "inline, ", whole ? "" : " cutting a frame");
            compare_case(what, false, 32000, 0, !whole);
            snprintf(what, sizeof(what), "%swrapping ring%s, float device",
                  threaded ? "" : "inline, ", whole ? "" : " cutting a frame");
            compare_case(what, true,  32000, 0, !whole);
            dev_layout = AUDIO_LAYOUT_5POINT1;
            snprintf(what, sizeof(what), "%swrapping ring%s, 5.1 float",
                  threaded ? "" : "inline, ", whole ? "" : " cutting a frame");
            compare_case(what, true,  32000, 0, !whole);
            snprintf(what, sizeof(what), "%swrapping ring%s, 5.1 s16",
                  threaded ? "" : "inline, ", whole ? "" : " cutting a frame");
            compare_case(what, false, 32000, 0, !whole);
            dev_layout = AUDIO_LAYOUT_STEREO;
            virtual_surround = true;
            snprintf(what, sizeof(what), "%swrapping ring%s, headphones float",
                  threaded ? "" : "inline, ", whole ? "" : " cutting a frame");
            compare_case(what, true,  32000, 0, !whole);
            snprintf(what, sizeof(what), "%swrapping ring%s, headphones s16",
                  threaded ? "" : "inline, ", whole ? "" : " cutting a frame");
            compare_case(what, false, 32000, 0, !whole);
            virtual_surround = false;
         }
      /* The core's own surround channels, merged over the upmix span by
       * span: each span takes its own frames of them */
      core_surround = true;
      ring_cut      = 0;
      for (i = 0; i < 2; i++)
      {
         char what[96];
         threaded   = i == 0;
         dev_layout = AUDIO_LAYOUT_5POINT1;
         snprintf(what, sizeof(what), "%swrapping ring, a 5.1 core, 5.1 float",
               threaded ? "" : "inline, ");
         compare_case(what, true,  32000, 0, false);
         snprintf(what, sizeof(what), "%swrapping ring, a 5.1 core, 5.1 s16",
               threaded ? "" : "inline, ");
         compare_case(what, false, 32000, 0, false);
         dev_layout = AUDIO_LAYOUT_STEREO;
         virtual_surround = true;
         snprintf(what, sizeof(what), "%swrapping ring, a 5.1 core, headphones",
               threaded ? "" : "inline, ");
         compare_case(what, true,  32000, 0, false);
         virtual_surround = false;
      }
      core_surround = false;
      /* A float stereo device at unity, the bypass's copy split likewise;
       * and a ring wrapping twice inside the flush, three spans */
      for (i = 0; i < 2; i++)
      {
         char what[96];
         threaded    = i == 0;
         ring_frames = 1000;
         snprintf(what, sizeof(what), "%swrapping ring, float device, unity",
               threaded ? "" : "inline, ");
         compare_case(what, true, 48000, 0, false);
         ring_frames = 500;
         snprintf(what, sizeof(what), "%sring wrapping twice, float device",
               threaded ? "" : "inline, ");
         compare_case(what, true, 32000, 0, false);
      }
      ring_frames = 0;
      ring_bytes  = 0;
   }

   if (failures)
   {
      printf("%u failure(s)\n", failures);
      return 1;
   }
   printf("lend write: the driver's buffer takes the output directly, and the bytes do not change\n");
   return 0;
}
