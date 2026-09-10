/* The discrete multi-channel path: a core's 5.1 through the batch
 * entry to a 5.1 device without the fold and the upmix in between.
 *
 * A scripted 6-channel device captures every write. The core sends a
 * frame set with a different tone on every channel, through
 * audio_driver_sample_batch_multi_float and _int16, at a resampling
 * ratio off unity so the extras go through their own resampler
 * instances. Each device channel must carry its core channel's tone
 * and none of the others' - which the fold-and-upmix cannot do, its
 * rears being the fronts at -3 dB. Then the same to a stereo device:
 * the fold, both channels carrying every tone at the BS.775 gains.
 *
 * Includes audio/audio_driver.c so the shipping flush runs. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <pthread.h>

#include <rthreads/rthreads.h>
#include <formats/rac3.h>
#include <formats/iec61937.h>
#include "fake_wasapi.h"

#include "../../../audio/audio_driver.c"

extern audio_driver_t audio_wasapi;

static unsigned failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { printf("      FAIL: "); printf(__VA_ARGS__); printf("\n"); failures++; } } while (0)

/* --- the device: N channels, captures what it is given ---------------- */
static unsigned dev_channels = 6;
static uint32_t dev_layout   = AUDIO_LAYOUT_5POINT1;
static bool     dev_float    = true;
static float   *cap          = NULL;
static size_t   cap_frames   = 0, cap_cap = 0;
/* The consumer thread appends to the capture and the test resets it
 * between phases; both go under this. */
static pthread_mutex_t cap_lock = PTHREAD_MUTEX_INITIALIZER;

static void  *dev_init(const char *d, unsigned r, unsigned l, unsigned *n)
{ static int h; (void)d; (void)r; (void)l; if (n) *n = 48000; return &h; }
static ssize_t dev_write(void *d, const void *buf, size_t size)
{
   size_t frames = size / (dev_channels * (dev_float ? sizeof(float) : sizeof(int16_t))), f, c;
   (void)d;
   pthread_mutex_lock(&cap_lock);
   if (cap_frames + frames > cap_cap)
   {
      cap_cap = (cap_frames + frames) * 2 + 4096;
      cap     = (float*)realloc(cap, cap_cap * dev_channels * sizeof(float));
   }
   for (f = 0; f < frames; f++)
      for (c = 0; c < dev_channels; c++)
         cap[(cap_frames + f) * dev_channels + c] = dev_float
               ? ((const float*)buf)[f * dev_channels + c]
               : ((const int16_t*)buf)[f * dev_channels + c] / 32768.0f;
   cap_frames += frames;
   pthread_mutex_unlock(&cap_lock);
   return (ssize_t)size;
}
static bool     dev_stop(void *d) { (void)d; return true; }
static bool     dev_start(void *d, bool s) { (void)d; (void)s; return true; }
static bool     dev_alive(void *d) { (void)d; return true; }
static void     dev_nonblock(void *d, bool s) { (void)d; (void)s; }
static void     dev_free(void *d) { (void)d; }
static bool     dev_use_float(void *d) { (void)d; return dev_float; }
static size_t   dev_write_avail(void *d) { (void)d; return 1 << 20; }
static size_t   dev_buffer_size(void *d) { (void)d; return 1 << 20; }
static uint32_t dev_layout_hook(void *d) { (void)d; return dev_layout; }
static audio_driver_t scripted = {
   dev_init, dev_write, dev_stop, dev_start, dev_alive, dev_nonblock,
   dev_free, dev_use_float, "scripted", NULL, NULL, dev_write_avail,
   dev_buffer_size, NULL, NULL, NULL, NULL, dev_layout_hook
};

/* the frontend's stand-up, as audio_driver_init does it, for the
 * non-threaded pipeline with a wide device */
static bool up(bool core_float, uint32_t layout, bool float_dev)
{
   audio_driver_state_t *st = &audio_driver_st;
   audio_driver_deinit_internal(true);
   memset(st, 0, sizeof(*st));
   dev_layout   = layout;
   dev_channels = audio_layout_channels(layout);
   dev_float    = float_dev;
   cap_frames   = 0;
   st->current_audio          = &scripted;
   st->context_audio_data     = scripted.init(NULL, 48000, 64, NULL);
   st->input                  = 44100.0;                  /* off unity: the extras resample */
   st->src_ratio_orig         = 48000.0 / 44100.0;
   st->src_ratio_curr         = st->src_ratio_orig;
   st->cached_rate_adjust     = 1.0;
   st->volume_gain            = 1.0f;
   st->buffer_size            = scripted.buffer_size(st->context_audio_data);
   st->output_samples_buf_length   = 1 << 18;
   st->output_samples_int16_length = 1 << 17;
   st->output_samples_buf     = (float*)malloc(st->output_samples_buf_length);
   st->output_samples_int16   = (int16_t*)malloc(st->output_samples_int16_length);
   st->input_data             = (float*)malloc(1 << 18);
   st->input_data_int16       = (int16_t*)malloc(1 << 17);
   st->core_float             = core_float;
   st->sink_bias              = 1.0;
   strcpy(st->resampler_ident, "sinc");
   st->resampler_quality      = RESAMPLER_QUALITY_NORMAL;
   if (!retro_resampler_realloc(&st->resampler_data, &st->resampler,
            st->resampler_ident, st->resampler_quality, st->src_ratio_orig))
      return false;
   config_get_ptr()->uints.audio_output_sample_rate = 48000;
   config_get_ptr()->bools.audio_fastpath_s16 = false;
   if (float_dev)
      AUDIO_FLAGS_SET(st, AUDIO_FLAG_USE_FLOAT);
   AUDIO_FLAGS_SET(st, AUDIO_FLAG_ACTIVE | AUDIO_FLAG_STARTED | AUDIO_FLAG_NONBLOCK);
   /* the layout as audio_driver_init picks it up */
   st->out_layout   = AUDIO_LAYOUT_STEREO;
   st->out_channels = 2;
   st->core_layout  = AUDIO_LAYOUT_STEREO;
   if (dev_channels > 2)
   {
      size_t frames = st->output_samples_buf_length / (2 * sizeof(float));
      st->upmix_buf = (float*)malloc(frames * dev_channels * sizeof(float));
      st->upmix_i16 = (int16_t*)malloc(frames * dev_channels * sizeof(int16_t));
      audio_upmix_init(&st->upmix, layout, 48000);
      st->out_layout   = layout;
      st->out_channels = dev_channels;
      st->upmix_frames = frames;
   }
   return true;
}

/* energy of a tone in a channel: the sample energy at frequency hz */
static double tone_energy(const float *x, size_t n, unsigned stride, unsigned ch, double hz)
{
   double re = 0, im = 0; size_t i;
   for (i = 0; i < n; i++)
   {
      /* Hann-windowed, so a strong tone does not leak into the bins
       * of the others over a span that is not whole cycles of it */
      double w = 0.5 - 0.5 * cos(2 * M_PI * i / (double)n);
      double v = x[i * stride + ch] * w * 2.0;
      re += v * cos(2 * M_PI * hz * i / 48000.0);
      im += v * sin(2 * M_PI * hz * i / 48000.0);
   }
   return (re * re + im * im) / ((double)n * n / 4.0);
}

static const double tone_hz[6] = { 220, 330, 440, 60, 550, 660 };   /* FL FR FC LFE BL BR */

/* --- the threaded pipeline: a consumer thread over the ring --------- */
/* The contract returns the bytes the device can take, not a flag. */
static size_t dev_wait_writable(void *d, size_t len) { (void)d; return len; }
static audio_driver_t scripted_threaded;
static retro_atomic_int_t consumer_run = RETRO_ATOMIC_INT_INITIALIZER(1);
static void consumer(void *arg)
{
   (void)arg;
   while (retro_atomic_load_acquire_int(&consumer_run))
      audio_driver_pipeline_consume(&audio_driver_st);
}
static bool pipe_up(bool core_float, bool float_dev)
{
   audio_driver_state_t *st = &audio_driver_st;
   if (!up(core_float, AUDIO_LAYOUT_5POINT1, float_dev))
      return false;
   scripted_threaded = scripted;
   scripted_threaded.wait_writable = dev_wait_writable;
   st->current_audio     = &scripted_threaded;
   st->pipe_scratch      = (uint8_t*)malloc(65536);
   st->pipe_conv         = (uint8_t*)malloc(65536);
   st->pipe_pass_frames  = 800;
   st->pipe_float        = float_dev;
   /* the core negotiated before the driver came up: the canonical
    * wide frame, as audio_driver_init sets the ring up */
   st->core_multi        = true;
   st->pipe_channels     = AUDIO_PIPE_CANON_CHANNELS;
   st->pipe_frame_bytes  = AUDIO_PIPE_CANON_CHANNELS * (float_dev ? sizeof(float) : sizeof(int16_t));
   st->pipe_wide_bytes   = st->pipe_pass_frames * AUDIO_PIPE_CANON_CHANNELS * sizeof(float);
   st->pipe_wide         = (uint8_t*)malloc(st->pipe_wide_bytes);
   retro_atomic_store_release_int(&st->pipe_layout, (int)AUDIO_LAYOUT_STEREO);
   retro_atomic_store_release_int(&st->pipe_ctrl_avail, -1);
   if (!retro_spsc_init(&st->pipe_ring, 1 << 22))
      return false;
   st->pipe_lock      = slock_new();
   st->pipe_cond      = scond_new();
   st->pipe_data_cond = scond_new();
   st->state_lock     = slock_new();
   st->pipe_threaded  = true;
   st->pipe_priming   = false;
   AUDIO_FLAGS_SET(st, AUDIO_FLAG_PIPELINE_THREADED);
   return st->pipe_lock && st->pipe_cond && st->pipe_data_cond && st->state_lock;
}

static void discrete_case(bool core_float, bool float_dev)
{
   size_t frames = 4410 * 2, f; unsigned c, d;
   float   *inf = (float*)malloc(frames * 6 * sizeof(float));
   int16_t *ini = (int16_t*)malloc(frames * 6 * sizeof(int16_t));
   size_t   took;
   printf("   5.1 core (%s) to a 5.1 device (%s): each channel its own\n",
         core_float ? "float" : "int16", float_dev ? "float" : "int16");
   CHECK(up(core_float, AUDIO_LAYOUT_5POINT1, float_dev), "stand-up");
   for (f = 0; f < frames; f++)
      for (c = 0; c < 6; c++)
      {
         inf[f * 6 + c] = 0.3f * (float)sin(2 * M_PI * tone_hz[c] * f / 44100.0);
         ini[f * 6 + c] = (int16_t)(inf[f * 6 + c] * 32767.0f);
      }
   /* in batches, as a core would */
   for (f = 0; f < frames; f += 735)
   {
      size_t n = frames - f < 735 ? frames - f : 735;
      took = core_float
            ? audio_driver_sample_batch_multi_float(inf + f * 6, n, 6, AUDIO_LAYOUT_5POINT1)
            : audio_driver_sample_batch_multi_int16(ini + f * 6, n, 6, AUDIO_LAYOUT_5POINT1);
      CHECK(took == n, "batch of %u took %u", (unsigned)n, (unsigned)took);
   }
   CHECK(cap_frames > frames * 48000 / 44100 * 9 / 10, "the device got %u frames of %u expected", (unsigned)cap_frames, (unsigned)(frames * 48000 / 44100));
   /* every device channel: its own tone present, the others' absent */
   for (d = 0; d < 6 && cap_frames; d++)
   {
      double own = tone_energy(cap, cap_frames, 6, d, tone_hz[d]), worst_other = 0; unsigned o;
      for (o = 0; o < 6; o++)
      {
         double e;
         if (o == d) continue;
         e = tone_energy(cap, cap_frames, 6, d, tone_hz[o]);
         if (e > worst_other) worst_other = e;
      }
      printf("      device ch%u: own tone %.3f, loudest other %.5f\n", d, own, worst_other);
      CHECK(own > 0.05, "device channel %u lost its tone (%.4f)", d, own);
      CHECK(worst_other < own / 100.0, "device channel %u carries another channel's tone (%.4f of %.4f)", d, worst_other, own);
   }
   free(inf); free(ini);
}

static void threaded_case(bool core_float, bool float_dev)
{
   size_t frames = 4410 * 4, f; unsigned c, d;
   float   *inf = (float*)malloc(frames * 6 * sizeof(float));
   int16_t *ini = (int16_t*)malloc(frames * 6 * sizeof(int16_t));
   sthread_t *th;
   size_t skip;
   printf("   5.1 core (%s) to a 5.1 device (%s) over the threaded ring\n",
         core_float ? "float" : "int16", float_dev ? "float" : "int16");
   CHECK(pipe_up(core_float, float_dev), "stand-up");
   for (f = 0; f < frames; f++)
      for (c = 0; c < 6; c++)
      {
         inf[f * 6 + c] = 0.3f * (float)sin(2 * M_PI * tone_hz[c] * f / 44100.0);
         ini[f * 6 + c] = (int16_t)(inf[f * 6 + c] * 32767.0f);
      }
   retro_atomic_store_release_int(&consumer_run, 1);
   th = sthread_create(consumer, NULL);
   /* batches as a core delivers them, the consumer draining beside.
    * The ring's layout is published per batch and read once a pass,
    * so the pass that straddles the first 5.1 batch takes its extra
    * slots from the stereo side and the upmix fills them - one pass,
    * documented, and not what this case is about. The capture is
    * dropped once the first batches are through, so what is judged
    * is the steady state whatever the consumer's phase. */
   for (f = 0; f < frames; f += 735)
   {
      size_t n = frames - f < 735 ? frames - f : 735;
      if (core_float)
         audio_driver_sample_batch_multi_float(inf + f * 6, n, 6, AUDIO_LAYOUT_5POINT1);
      else
         audio_driver_sample_batch_multi_int16(ini + f * 6, n, 6, AUDIO_LAYOUT_5POINT1);
      usleep(1000);
      if (f == 735 * 4)
      {
         usleep(20000);
         pthread_mutex_lock(&cap_lock);
         cap_frames = 0;
         pthread_mutex_unlock(&cap_lock);
      }
   }
   usleep(200000);
   retro_atomic_store_release_int(&consumer_run, 0);
   slock_lock(audio_driver_st.pipe_lock);
   scond_signal(audio_driver_st.pipe_data_cond);
   slock_unlock(audio_driver_st.pipe_lock);
   sthread_join(th);
   skip = 0;
   for (d = 0; d < 6 && cap_frames > skip; d++)
   {
      double own = tone_energy(cap + skip * 6, cap_frames - skip, 6, d, tone_hz[d]), worst_other = 0; unsigned o;
      for (o = 0; o < 6; o++)
      {
         double e;
         if (o == d) continue;
         e = tone_energy(cap + skip * 6, cap_frames - skip, 6, d, tone_hz[o]);
         if (e > worst_other) worst_other = e;
      }
      printf("      device ch%u: own tone %.3f, loudest other %.5f\n", d, own, worst_other);
      CHECK(own > 0.05, "device channel %u lost its tone (%.4f)", d, own);
      CHECK(worst_other < own / 100.0, "device channel %u carries another channel's tone (%.4f of %.4f)", d, worst_other, own);
   }
   free(inf); free(ini);
}

/* A stereo batch through the classic entry on the wide ring: widened
 * into the canonical frame with the rest zero, the consumer takes the
 * fronts alone and the upmix fills the rest - the rears carry the
 * fronts at -3 dB, as for a stereo core. */
static void stereo_on_wide_ring_case(void)
{
   size_t frames = 4410 * 4, f;
   float *inf = (float*)malloc(frames * 2 * sizeof(float));
   sthread_t *th;
   size_t skip;
   double fl, bl;
   printf("   stereo through the classic entry on the wide ring: the upmix fills the rears\n");
   CHECK(pipe_up(true, true), "stand-up");
   for (f = 0; f < frames; f++)
   {
      inf[2 * f]     = 0.3f * (float)sin(2 * M_PI * tone_hz[0] * f / 44100.0);
      inf[2 * f + 1] = 0.3f * (float)sin(2 * M_PI * tone_hz[1] * f / 44100.0);
   }
   retro_atomic_store_release_int(&consumer_run, 1);
   th = sthread_create(consumer, NULL);
   for (f = 0; f < frames; f += 735)
   {
      size_t n = frames - f < 735 ? frames - f : 735;
      audio_driver_sample_batch_float(inf + f * 2, n);
      usleep(1000);
   }
   usleep(200000);
   retro_atomic_store_release_int(&consumer_run, 0);
   slock_lock(audio_driver_st.pipe_lock);
   scond_signal(audio_driver_st.pipe_data_cond);
   slock_unlock(audio_driver_st.pipe_lock);
   sthread_join(th);
   skip = cap_frames / 2;
   fl = tone_energy(cap + skip * 6, cap_frames - skip, 6, 0, tone_hz[0]);
   bl = tone_energy(cap + skip * 6, cap_frames - skip, 6, 4, tone_hz[0]);
   printf("      FL %.3f in the front left, %.3f in the back left (-3 dB is %.3f)\n", fl, bl, fl / 2);
   CHECK(fl > 0.05, "the front lost its tone");
   CHECK(fabs(bl / fl - 0.5) < 0.1, "the back left is not the upmix's -3 dB of the front");
   free(inf);
}

/* --- the recorder: takes what the frontend pushes ---------------------- */
static int16_t *rec_cap = NULL; static size_t rec_frames = 0, rec_cap_frames = 0; static unsigned rec_channels = 2;
static bool rec_push_audio(void *d, const struct record_audio_data *a)
{
   (void)d;
   if (rec_frames + a->frames > rec_cap_frames)
   {
      rec_cap_frames = (rec_frames + a->frames) * 2 + 4096;
      rec_cap = (int16_t*)realloc(rec_cap, rec_cap_frames * rec_channels * sizeof(int16_t));
   }
   memcpy(rec_cap + rec_frames * rec_channels, a->data, a->frames * rec_channels * sizeof(int16_t));
   rec_frames += a->frames;
   return true;
}
static record_driver_t rec_driver = { NULL, NULL, NULL, rec_push_audio, NULL, "capture" };

/* A 5.1 core recorded: the recorder opened in the core's layout gets
 * its six channels as they are; a stereo batch meanwhile lands in the
 * fronts with the rest silent; a recorder opened in stereo gets the
 * fold. The push is the frontend's, through the wide entry on the
 * threaded ring and the classic entries. */
static void record_case(void)
{
   size_t frames = 4410, f; unsigned c;
   float *inf = (float*)malloc(frames * 6 * sizeof(float));
   float *st  = (float*)malloc(frames * 2 * sizeof(float));
   recording_state_t *rs = recording_state_get_ptr();
   sthread_t *th;
   printf("   recording: the core's layout when the recorder has it, the fold when it does not\n");
   CHECK(pipe_up(true, true), "stand-up");
   for (f = 0; f < frames; f++)
   {
      for (c = 0; c < 6; c++) inf[f * 6 + c] = 0.3f * (float)sin(2 * M_PI * tone_hz[c] * f / 44100.0);
      st[2 * f] = inf[f * 6]; st[2 * f + 1] = inf[f * 6 + 1];
   }
   /* the recorder opened in 5.1, as recording_init does for a 5.1 core */
   rs->driver = &rec_driver; rs->data = rs; rs->layout = AUDIO_LAYOUT_5POINT1; rs->channels = 6;
   rec_channels = 6; rec_frames = 0;
   retro_atomic_store_release_int(&consumer_run, 1);
   th = sthread_create(consumer, NULL);
   audio_driver_sample_batch_multi_float(inf, frames, 6, AUDIO_LAYOUT_5POINT1);
   audio_driver_sample_batch_float(st, frames);
   retro_atomic_store_release_int(&consumer_run, 0);
   slock_lock(audio_driver_st.pipe_lock); scond_signal(audio_driver_st.pipe_data_cond); slock_unlock(audio_driver_st.pipe_lock);
   sthread_join(th);
   CHECK(rec_frames == frames * 2, "the recorder got %u frames of %u", (unsigned)rec_frames, (unsigned)(frames * 2));
   if (rec_frames == frames * 2)
   {
      float *first = (float*)malloc(frames * 6 * sizeof(float)), *second = (float*)malloc(frames * 6 * sizeof(float));
      double e;
      for (f = 0; f < frames * 6; f++) { first[f] = rec_cap[f] / 32768.0f; second[f] = rec_cap[frames * 6 + f] / 32768.0f; }
      /* the recording is at the source's 44.1 kHz; the measure assumes 48 */
#define REC_HZ(h) ((h) * 48000.0 / 44100.0)
      e = tone_energy(first, frames, 6, 4, REC_HZ(tone_hz[4]));
      CHECK(e > 0.05, "the 5.1 batch's BL is not in the recording's BL (%.4f)", e);
      e = tone_energy(first, frames, 6, 4, REC_HZ(tone_hz[0]));
      CHECK(e < 0.001, "the 5.1 batch's FL leaked into the recording's BL (%.4f)", e);
      e = tone_energy(second, frames, 6, 0, REC_HZ(tone_hz[0]));
      CHECK(e > 0.05, "the stereo batch's FL is not in the recording's FL (%.4f)", e);
      for (f = 0; f < frames; f++)
         if (rec_cap[frames * 6 + f * 6 + 4] || rec_cap[frames * 6 + f * 6 + 2]) { CHECK(0, "the stereo batch did not leave the recording's other channels silent"); break; }
      free(first); free(second);
   }
   /* the recorder opened in stereo: the fold */
   rs->layout = AUDIO_LAYOUT_STEREO; rs->channels = 2; rec_channels = 2; rec_frames = 0;
   retro_atomic_store_release_int(&consumer_run, 1);
   th = sthread_create(consumer, NULL);
   audio_driver_sample_batch_multi_float(inf, frames, 6, AUDIO_LAYOUT_5POINT1);
   retro_atomic_store_release_int(&consumer_run, 0);
   slock_lock(audio_driver_st.pipe_lock); scond_signal(audio_driver_st.pipe_data_cond); slock_unlock(audio_driver_st.pipe_lock);
   sthread_join(th);
   CHECK(rec_frames == frames, "the stereo recorder got %u frames", (unsigned)rec_frames);
   if (rec_frames == frames)
   {
      float *r = (float*)malloc(frames * 2 * sizeof(float));
      double fl, bl;
      for (f = 0; f < frames * 2; f++) r[f] = rec_cap[f] / 32768.0f;
      fl = tone_energy(r, frames, 2, 0, REC_HZ(tone_hz[0]));
      bl = tone_energy(r, frames, 2, 0, REC_HZ(tone_hz[4]));
      CHECK(fabs(bl / fl - 0.5) < 0.1, "the fold to the stereo recording is not -3 dB on BL (%.3f of %.3f)", bl, fl);
      free(r);
   }
   rs->driver = NULL; rs->data = NULL;
   free(inf); free(st);
}

/* A 5.1 core through the batch entry to a device that takes no PCM
 * wider than stereo but decodes Dolby Digital: the frontend's merge
 * builds the 5.1 device frame and the driver encodes that frame to
 * AC-3 over IEC 61937. The bursts the device is given are decoded
 * back here, and each channel has to carry its own tone - which is
 * what says the encoder was handed the core's channels and not the
 * upmix's copies of the fronts. */
static void ac3_bitstream_case(void)
{
   settings_t *settings = config_get_ptr();
   audio_driver_state_t *st = &audio_driver_st;
   size_t frames = 4410 * 4, f;
   unsigned c, d;
   float *inf = (float*)malloc(frames * 6 * sizeof(float));
   unsigned new_rate = 0;
   void *ctx;
   const uint8_t *bur = NULL;
   size_t bur_len, at = 0, decoded = 0;
   float *pcm;
   rac3_decoder_t *dec;

   printf("   5.1 core to a device that decodes Dolby Digital: the bursts carry the core's channels\n");
   audio_driver_deinit_internal(true);
   memset(st, 0, sizeof(*st));
   fake_device_configure_engine(0, 0);
   fake_device_configure_channels(2, true);
   settings->bools.audio_wasapi_exclusive_mode   = true;
   settings->uints.audio_wasapi_sh_buffer_length = 0;
   settings->uints.audio_output_sample_rate      = 48000;
   settings->uints.audio_output_layout           = 3;   /* 5.1, surrounds at the sides */

   ctx = audio_wasapi.init(NULL, 48000, 64, &new_rate);
   CHECK(ctx != NULL, "the AC-3 driver did not open");
   if (!ctx) { free(inf); return; }

   st->current_audio      = &audio_wasapi;
   st->context_audio_data = ctx;
   st->input              = 48000.0;
   st->src_ratio_orig     = 1.0;
   st->src_ratio_curr     = 1.0;
   st->cached_rate_adjust = 1.0;
   st->volume_gain        = 1.0f;
   st->buffer_size        = audio_wasapi.buffer_size(ctx);
   st->output_samples_buf_length   = 1 << 18;
   st->output_samples_int16_length = 1 << 17;
   st->output_samples_buf   = (float*)malloc(st->output_samples_buf_length);
   st->output_samples_int16 = (int16_t*)malloc(st->output_samples_int16_length);
   st->input_data           = (float*)malloc(1 << 18);
   st->input_data_int16     = (int16_t*)malloc(1 << 17);
   st->core_float           = true;
   st->sink_bias            = 1.0;
   st->core_layout          = AUDIO_LAYOUT_STEREO;
   AUDIO_FLAGS_SET(st, AUDIO_FLAG_USE_FLOAT | AUDIO_FLAG_ACTIVE | AUDIO_FLAG_STARTED);
   /* the layout the driver opened, as audio_driver_init picks it up */
   st->out_layout   = audio_wasapi.layout(ctx);
   st->out_channels = audio_layout_channels(st->out_layout);
   CHECK(st->out_channels == 6, "the driver reports %u channels", st->out_channels);
   {
      size_t up = st->output_samples_buf_length / (2 * sizeof(float));
      st->upmix_buf    = (float*)malloc(up * st->out_channels * sizeof(float));
      st->upmix_i16    = (int16_t*)malloc(up * st->out_channels * sizeof(int16_t));
      st->upmix_frames = up;
      audio_upmix_init(&st->upmix, st->out_layout, 48000);
   }

   for (f = 0; f < frames; f++)
      for (c = 0; c < 6; c++)
         inf[f * 6 + c] = 0.4f * (float)sin(2 * M_PI * tone_hz[c] * f / 48000.0);

   audio_wasapi.set_nonblock_state(ctx, false);
   fake_device_capture(true);
   audio_wasapi.start(ctx, false);
   for (f = 0; f < frames; f += 735)
   {
      size_t n = frames - f < 735 ? frames - f : 735;
      size_t took = audio_driver_sample_batch_multi_float(inf + f * 6, n, 6, AUDIO_LAYOUT_5POINT1);
      CHECK(took == n, "the entry took %u of %u frames", (unsigned)took, (unsigned)n);
   }
   /* Drained until the bursts the feed produced have reached the
    * device, or until it stops taking, or until a deadline: a
    * sanitized run is an order of magnitude slower than a plain one,
    * so a fixed wait ends it after a burst or two, and the device's
    * pump never goes quiet for good, so waiting for quiet alone does
    * not end at all. */
   {
      size_t   want  = (frames / 1536) * IEC61937_AC3_BURST_BYTES;
      size_t   last  = 0, got = 0;
      unsigned lap   = 0, quiet = 0;
      while (lap++ < 250 && quiet < 15)
      {
         usleep(20000);
         got   = fake_device_captured(&bur);
         quiet = (got == last) ? quiet + 1 : 0;
         last  = got;
         if (got >= want)
            break;
      }
   }
   audio_wasapi.stop(ctx);
   fake_device_capture(false);
   bur_len = fake_device_captured(&bur);

   /* the bursts, decoded */
   dec = rac3_decoder_new();
   pcm = (float*)calloc(frames * 6 + 1536 * 6, sizeof(float));
   {
      unsigned type, bursts = 0;
      size_t payload;
      while (at + 8 <= bur_len
            && !(iec61937_probe(bur + at, bur_len - at, &type, &payload) && type == IEC61937_AC3))
         at += 2;
      while (at + IEC61937_AC3_BURST_BYTES <= bur_len)
      {
         uint8_t frame[RAC3_MAX_FRAME_BYTES];
         rac3_frame_info_t info;
         size_t k;
         /* Silence between bursts: the device's pump plays whatever
          * the driver had ready, and a slow run leaves gaps. Stepped
          * over a frame at a time rather than taken for the end of
          * the stream. */
         if (!iec61937_probe(bur + at, bur_len - at, &type, &payload)
               || type != IEC61937_AC3 || payload > sizeof(frame))
         {
            at += 4;
            continue;
         }
         for (k = 0; k + 1 < payload; k += 2)
         {
            frame[k]     = bur[at + 8 + k + 1];
            frame[k + 1] = bur[at + 8 + k];
         }
         if (payload & 1)
            frame[payload - 1] = bur[at + 8 + payload - 1];
         k = rac3_decode_frame(dec, frame, payload, pcm + decoded * 6, &info);
         if (!k)
            break;
         decoded += k;
         bursts++;
         at += IEC61937_AC3_BURST_BYTES;
      }
      printf("      %u byte(s) to the device, %u burst(s), %u frames decoded\n",
            (unsigned)bur_len, bursts, (unsigned)decoded);
      /* a burst is 1536 frames; the feed is 17640 of them */
      CHECK(bursts >= (unsigned)(frames / 1536) - 1,
            "only %u of about %u bursts reached the device", bursts, (unsigned)(frames / 1536));
   }
   for (d = 0; d < 6 && decoded > 4096; d++)
   {
      double own = tone_energy(pcm + 1536 * 6, decoded - 1536, 6, d, tone_hz[d]);
      double worst = 0;
      unsigned o;
      for (o = 0; o < 6; o++)
      {
         double e;
         if (o == d) continue;
         e = tone_energy(pcm + 1536 * 6, decoded - 1536, 6, d, tone_hz[o]);
         if (e > worst) worst = e;
      }
      printf("      decoded ch%u: own tone %.3f, loudest other %.4f\n", d, own, worst);
      CHECK(own > 0.02, "channel %u lost its tone through the bitstream (%.4f)", d, own);
      CHECK(worst < own / 10.0,
            "channel %u carries another channel's tone (%.4f of %.4f): the encoder was handed the upmix, not the core",
            d, worst, own);
   }
   rac3_decoder_free(dec);
   free(pcm);
   free(inf);
   audio_wasapi.free(ctx);
   st->current_audio = NULL;
   st->context_audio_data = NULL;
}

static void fold_case(void)
{
   size_t frames = 4410, f; unsigned c;
   float *inf = (float*)malloc(frames * 6 * sizeof(float));
   double own, l, r;
   printf("   5.1 core to a stereo device: the fold\n");
   CHECK(up(true, AUDIO_LAYOUT_STEREO, true), "stand-up");
   for (f = 0; f < frames; f++)
      for (c = 0; c < 6; c++)
         inf[f * 6 + c] = 0.3f * (float)sin(2 * M_PI * tone_hz[c] * f / 44100.0);
   audio_driver_sample_batch_multi_float(inf, frames, 6, AUDIO_LAYOUT_5POINT1);
   CHECK(cap_frames > frames, "the device got %u frames", (unsigned)cap_frames);
   own = tone_energy(cap, cap_frames, 2, 0, tone_hz[0]);
   l   = tone_energy(cap, cap_frames, 2, 0, tone_hz[4]);   /* BL into left at -3 dB */
   r   = tone_energy(cap, cap_frames, 2, 1, tone_hz[4]);   /* and not right */
   printf("      left: FL %.3f, BL %.3f (-3 dB is %.3f); right has BL %.5f\n", own, l, own / 2, r);
   CHECK(fabs(l / own - 0.5) < 0.1, "BL is not -3 dB into left in the fold");
   CHECK(r < own / 100, "BL leaked into the right in the fold");
   CHECK(tone_energy(cap, cap_frames, 2, 0, tone_hz[3]) < own / 1000, "the LFE was not dropped in the fold");
   free(inf);
}

int main(void)
{
   /* One case at a time, for when a single one is being worked on:
    * DM_ONLY=ac3 runs the bitstream case alone. */
   const char *only = getenv("DM_ONLY");
#define RUN(tag, call) do { if (!only || strstr(only, tag)) { call; } } while (0)
   printf("discrete multi-channel:\n");
   RUN("discrete", discrete_case(true, true));
   RUN("discrete", discrete_case(false, false));
   RUN("discrete", discrete_case(false, true));
   RUN("discrete", discrete_case(true, false));
   RUN("threaded", threaded_case(true, true));
   RUN("threaded", threaded_case(false, false));
   RUN("threaded", stereo_on_wide_ring_case());
   RUN("record",   record_case());
   RUN("ac3",      ac3_bitstream_case());
   RUN("fold",     fold_case());
   audio_driver_deinit_internal(true);
   free(cap); free(rec_cap);
   if (failures) { printf("%u failure(s)\n", failures); return 1; }
   printf("discrete multi-channel: a core's channels reach their speakers as they are\n");
   return 0;
}
