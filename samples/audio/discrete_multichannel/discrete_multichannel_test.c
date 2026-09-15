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

#include <memalign.h>
#include "../../../audio/audio_pipeline_stretch.h"
static bool transport_fail_output, transport_fail_stage, transport_track;
static unsigned transport_allocations, transport_frees;
static bool canonical_fail, canonical_track;
static unsigned canonical_reallocations;
static void *canonical_test_realloc(void *ptr, size_t bytes)
{
   if (canonical_track) canonical_reallocations++;
   if (canonical_fail) return NULL;
   return realloc(ptr, bytes);
}
static void *transport_test_alloc(size_t alignment, size_t size)
{
   void *p;
   if (transport_fail_output) return NULL;
   p = memalign_alloc(alignment, size);
   if (p && transport_track) transport_allocations++;
   return p;
}
static void transport_test_free(void *p)
{
   if (p && transport_track) transport_frees++;
   memalign_free(p);
}
static audio_pipeline_stretch_t *transport_test_new(unsigned rate,
      unsigned channels, bool floating, uint32_t search,
      retro_spsc_t *ring, audio_pipeline_layout_t *metadata,
      void *output, size_t frames)
{
   if (transport_fail_stage) return NULL;
   return audio_pipeline_stretch_new(rate, channels, floating, search,
         ring, metadata, output, frames);
}
#define memalign_alloc transport_test_alloc
#define memalign_free transport_test_free
#define audio_pipeline_stretch_new transport_test_new
#define realloc canonical_test_realloc
#include "../../../audio/audio_driver.c"

/* Pause and resume as the runloop would: the flags drive the publish,
 * so the snapshot keeps carrying the setting bits the fixture set. */
static void snap_pause(bool paused)
{
   runloop_state_get_ptr()->flags = paused ? RUNLOOP_FLAG_PAUSED : 0;
   audio_driver_publish_runloop();
}
#undef realloc
#undef memalign_alloc
#undef memalign_free
#undef audio_pipeline_stretch_new

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
static bool     dev_stop_fail(void *d) { (void)d; return false; }
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

/* Buffers the harness owns.
 *
 * In the frontend these six are slices of arena_int16, arena_float and
 * pipe_arena: audio_driver_deinit_internal() frees the arenas as units
 * and then only NULLs the named pointers, because none of them is a
 * separate allocation there. A stand-up below hands the state plain
 * malloc()s instead, so teardown clears the pointers with nothing
 * released - and since each stand-up deinits and memsets before it
 * installs its own, the previous case's blocks are unreachable by
 * then. That leaked a set per case: 466 MB over a full run, enough
 * that the test cannot be run under LeakSanitizer.
 *
 * The buffers the frontend really does free - upmix_buf, upmix_i16,
 * pipe_wide, multi_fold, record_remap - are not here, and must not be:
 * deinit frees them and this would double it. */
static void *owned[6];

static void owned_free(void)
{
   size_t i;
   for (i = 0; i < sizeof(owned) / sizeof(owned[0]); i++)
   {
      free(owned[i]);
      owned[i] = NULL;
   }
}

/* the frontend's stand-up, as audio_driver_init does it, for the
 * non-threaded pipeline with a wide device */
static bool up(bool core_float, uint32_t layout, bool float_dev)
{
   audio_driver_state_t *st = &audio_driver_st;
   owned_free();
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
   owned[0]                   = st->output_samples_buf;
   owned[1]                   = st->output_samples_int16;
   owned[2]                   = st->input_data;
   owned[3]                   = st->input_data_int16;
   st->core_float             = core_float;
   st->sink_bias              = 1.0;
   strcpy(st->resampler_ident, "sinc");
   st->resampler_quality      = RESAMPLER_QUALITY_NORMAL;
   if (!retro_resampler_realloc(&st->resampler_data, &st->resampler,
            st->resampler_ident, st->resampler_quality, st->src_ratio_orig))
      return false;
   config_get_ptr()->uints.audio_output_sample_rate = 48000;
   st->out_rate               = 48000;
   config_get_ptr()->bools.audio_fastpath_s16 = false;
   snap_pause(false);
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
   owned[4]              = st->pipe_scratch;
   owned[5]              = st->pipe_conv;
   st->pipe_pass_frames  = 800;
   st->pipe_float        = float_dev;
   /* the core negotiated before the driver came up: the canonical
    * wide frame, as audio_driver_init sets the ring up */
   st->core_multi        = true;
   st->pipe_channels     = AUDIO_PIPE_CANON_CHANNELS;
   st->pipe_frame_bytes  = AUDIO_PIPE_CANON_CHANNELS * (float_dev ? sizeof(float) : sizeof(int16_t));
   st->pipe_wide_bytes   = st->pipe_pass_frames * AUDIO_PIPE_CANON_CHANNELS * sizeof(float);
   st->pipe_wide         = (uint8_t*)malloc(st->pipe_wide_bytes);
   st->pipe_layout = AUDIO_LAYOUT_STEREO;
   audio_pipeline_layout_init(&st->pipe_layouts, AUDIO_LAYOUT_STEREO);
   retro_atomic_store_release_int(&st->pipe_ctrl_avail, -1);
   if (!retro_spsc_init(&st->pipe_ring, 1 << 22))
      return false;
   retro_eventcount_init(&st->pipe_space);
   retro_eventcount_init(&st->pipe_data);
   st->state_lock     = slock_new();
   st->pipe_park_ready = true;
   st->pipe_threaded  = true;
   st->pipe_priming   = false;
   AUDIO_FLAGS_SET(st, AUDIO_FLAG_PIPELINE_THREADED);
   return st->state_lock != NULL;
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
   audio_driver_pipeline_signal(&audio_driver_st);
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
   audio_driver_pipeline_signal(&audio_driver_st);
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
   audio_driver_pipeline_signal(&audio_driver_st);
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
   audio_driver_pipeline_signal(&audio_driver_st);
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
   owned_free();
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
   owned[0]                 = st->output_samples_buf;
   owned[1]                 = st->output_samples_int16;
   owned[2]                 = st->input_data;
   owned[3]                 = st->input_data_int16;
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
      /* Drains until the device stops taking bursts, not until it
       * pauses. quiet counts consecutive 20 ms laps that moved
       * nothing, and at fifteen of them - 300 ms - a writer thread
       * that merely lost its slice on a loaded machine looked like a
       * finished stream: the run ended two bursts short and the check
       * below failed. That is the whole of this test's flakiness.
       *
       * A second of silence is the give-up now, with ten seconds
       * overall, and neither costs anything when the device is
       * keeping up - the loop still leaves the moment it has what it
       * asked for. */
      while (lap++ < 500 && quiet < 50)
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
      /* Against what the device received, not against what was fed.
       * Those differ: the feed's last partial block is still in the
       * encoder, so it was never a burst at all, and the check used to
       * demand it. */
      {
         /* Capture length alone does not say how many bursts arrived.
          * The device's pump plays on after the content ends, so the
          * tail of the capture is silence, and how much of it there is
          * depends only on how long the drain ran - a TSan build
          * captures half again what a plain one does from the same
          * feed. Bounded by what the encoder could have produced, so
          * the number means bursts either way. */
         unsigned arrived = (unsigned)(bur_len / IEC61937_AC3_BURST_BYTES);
         unsigned encoded = (unsigned)(frames / 1536);
         if (arrived > encoded)
            arrived = encoded;
         /* Two bursts of slack, one for each end of the capture.
          *
          * The capture does not begin or end on a burst boundary: the
          * loop above seeks the first sync word, so whatever preceded
          * it is a partial burst the decode cannot use, and the decode
          * needs a whole burst remaining, so a partial tail is left
          * too. How much is lost at each end depends on where the
          * writer happened to be when capture opened - a plain build
          * decodes ten of eleven here and an ASan build nine, from
          * byte-identical captures.
          *
          * The count is a sanity check, not this case's subject: what
          * it is checking is that a 5.1 core's channels survive the
          * encode, and that is the per-channel tone comparison below,
          * which runs over whatever did decode. */
         CHECK(bursts + 2 >= arrived,
               "only %u of the %u bursts that reached the device decoded",
               bursts, arrived);
      }
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

/* Headphones: a stereo device with the virtual surround on. The
 * binaural stage renders a 5.1 to two ears, and that 5.1 was always
 * the upmix's guess - the stereo widened, its rears a copy of its
 * fronts. A 5.1 core's own rears were folded into the stereo and then
 * invented again from it, so a sound that was only ever behind the
 * listener arrived in front and was placed behind. What says it is
 * fixed is the rear channel's own tone reaching the ears at all:
 * under the guess there is nothing at that frequency anywhere in the
 * output, because the rear was a copy of the front. */
static void virtual_surround_case(void)
{
   audio_driver_state_t *st = &audio_driver_st;
   size_t frames = 4410 * 2, f;
   unsigned c;
   float *inf = (float*)malloc(frames * 6 * sizeof(float));
   double rear_energy, front_energy;

   printf("   5.1 core to headphones: the core's rears reach the ears\n");
   CHECK(up(true, AUDIO_LAYOUT_STEREO, true), "stand-up");
   /* The virtual 5.1, as audio_driver_init stands it up. */
   {
      size_t up_frames = st->output_samples_buf_length / (2 * sizeof(float));
      st->virt_buf     = (float*)malloc(up_frames * 6 * sizeof(float));
      free(st->upmix_buf);
      free(st->upmix_i16);
      st->upmix_buf    = (float*)malloc(up_frames * 2 * sizeof(float));
      st->upmix_i16    = (int16_t*)malloc(up_frames * 6 * sizeof(int16_t));
      CHECK(st->virt_buf && st->upmix_buf && st->upmix_i16, "no room for the virtual 5.1");
      CHECK(audio_upmix_init(&st->upmix, AUDIO_LAYOUT_5POINT1, 48000), "upmix");
      CHECK(audio_binaural_init(&st->binaural, AUDIO_LAYOUT_5POINT1, 48000), "binaural");
      st->virtualize   = true;
      st->upmix_frames = up_frames;
      st->out_layout   = AUDIO_LAYOUT_STEREO;
      st->out_channels = 2;
   }

   /* A sound only ever behind the listener: the rear pair carries a
    * tone and every other channel is silent. Its presence at the ears
    * proves nothing either way - the fold puts it into the stereo, so
    * it arrives under both behaviours. What differs is where it sits
    * in the 5.1 the binaural stage renders, and that is what is
    * measured: the virtual fronts. Carried, they stay silent, because
    * the core's fronts were. Invented, they hold the folded rear and
    * the rears hold a copy of it - the sound is placed in front of
    * the listener and then a copy is placed behind. */
   for (f = 0; f < frames; f++)
      for (c = 0; c < 6; c++)
         inf[f * 6 + c] = (c == 4 || c == 5)
               ? 0.4f * (float)sin(2 * M_PI * tone_hz[4] * f / 44100.0)
               : 0.0f;

   audio_driver_sample_batch_multi_float(inf, 735, 6, AUDIO_LAYOUT_5POINT1);

   /* virt_buf holds the 5.1 the binaural stage was handed for the
    * last write. */
   front_energy = tone_energy(st->virt_buf, 700, 6, 0, tone_hz[4]);
   rear_energy  = tone_energy(st->virt_buf, 700, 6, 4, tone_hz[4]);
   printf("      the virtual 5.1: front left %.4f, back left %.4f, at the rear's frequency\n",
         front_energy, rear_energy);
   CHECK(rear_energy > 0.01,
         "the rear tone is not in the virtual rear (%.5f)", rear_energy);
   CHECK(front_energy < rear_energy / 20.0,
         "the virtual front holds %.4f of a rear-only sound against %.4f behind: the rears were invented from a fold, not carried",
         front_energy, rear_energy);
   free(inf);
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

static void large_inline_batch_case(bool floating, bool fold)
{
   const size_t chunk = AUDIO_CHUNK_SIZE_NONBLOCKING >> 1;
   const size_t frames = 3 * chunk + 17;
   float *input_f = (float*)malloc(frames * 6 * sizeof(float));
   int16_t *input_i = (int16_t*)malloc(frames * 6 * sizeof(int16_t));
   float *expected;
   size_t f, made, n; unsigned c;
   for (f = 0; f < frames; f++)
      for (c = 0; c < 6; c++)
      {
         input_f[f * 6 + c] = 0.3f * (float)sin(2 * M_PI * tone_hz[c] * f / 44100.0);
         input_i[f * 6 + c] = (int16_t)(input_f[f * 6 + c] * 32767);
      }
   CHECK(up(floating, fold ? AUDIO_LAYOUT_STEREO : AUDIO_LAYOUT_5POINT1, floating), "large batch stand-up");
   n = floating ? audio_driver_sample_batch_multi_float(input_f, frames, 6, AUDIO_LAYOUT_5POINT1)
      : audio_driver_sample_batch_multi_int16(input_i, frames, 6, AUDIO_LAYOUT_5POINT1);
   CHECK(n == frames, "large batch consumption");
   CHECK(audio_driver_st.multi_fold_frames <= chunk, "front staging exceeded one chunk");
   made = cap_frames;
   expected = (float*)malloc(made * dev_channels * sizeof(float));
   memcpy(expected, cap, made * dev_channels * sizeof(float));
   CHECK(up(floating, fold ? AUDIO_LAYOUT_STEREO : AUDIO_LAYOUT_5POINT1, floating), "fragmented stand-up");
   for (f = 0; f < frames; f += n)
   {
      n = frames - f > chunk ? chunk : frames - f;
      if (floating) audio_driver_sample_batch_multi_float(input_f + f * 6, n, 6, AUDIO_LAYOUT_5POINT1);
      else audio_driver_sample_batch_multi_int16(input_i + f * 6, n, 6, AUDIO_LAYOUT_5POINT1);
   }
   CHECK(made == cap_frames, "large/fragmented frame counts differ");
   CHECK(made == cap_frames && !memcmp(expected, cap, made * dev_channels * sizeof(float)),
         "large batch lost or shifted discrete channels after the first chunk");
   free(expected); free(input_f); free(input_i);
}

static void bounded_canonical_case(bool floating)
{
   const size_t chunk = AUDIO_CHUNK_SIZE_NONBLOCKING >> 1;
   const size_t frames = 3 * chunk + 17;
   float *input_f = (float*)malloc(frames * 6 * sizeof(float));
   int16_t *input_i = (int16_t*)malloc(frames * 6 * sizeof(int16_t));
   size_t sample = floating ? sizeof(float) : sizeof(int16_t);
   uint8_t *actual = (uint8_t*)malloc(frames * AUDIO_PIPE_CANON_CHANNELS * sample);
   const uint8_t *input = floating ? (const uint8_t*)input_f : (const uint8_t*)input_i;
   audio_driver_state_t *st = &audio_driver_st;
   size_t f; unsigned c, slot;
   uint8_t zero[sizeof(float)] = {0};
   CHECK(pipe_up(floating, floating), "canonical stand-up");
   for (f = 0; f < frames * 6; f++)
   {
      input_i[f] = (int16_t)(f % 30000);
      input_f[f] = input_i[f] / 32768.0f;
   }
   AUDIO_FLAGS_SET(st, AUDIO_FLAG_SUSPENDED);
   CHECK(audio_driver_multi_pipe(st, input, frames, 6, AUDIO_LAYOUT_5POINT1, floating),
         "suspended publish accepted");
   canonical_reallocations = 0; canonical_track = true;
   AUDIO_FLAGS_CLEAR(st, AUDIO_FLAG_SUSPENDED);
   CHECK(audio_driver_multi_pipe(st, input, frames, 6, AUDIO_LAYOUT_5POINT1, floating),
         "canonical publish accepted");
   canonical_track = false;
   CHECK(!canonical_reallocations, "canonical publish allocated staging");
   CHECK(retro_spsc_read(&st->pipe_ring, actual, frames * st->pipe_frame_bytes)
         == frames * st->pipe_frame_bytes, "canonical publish lost frames");
   for (f = 0; f < frames; f++)
      for (slot = c = 0; slot < AUDIO_PIPE_CANON_CHANNELS; slot++)
      {
         const uint8_t *expected = zero;
         if (AUDIO_LAYOUT_5POINT1 & (1u << slot)) expected = input + (f * 6 + c++) * sample;
         CHECK(!memcmp(actual + (f * AUDIO_PIPE_CANON_CHANNELS + slot) * sample,
                  expected, sample), "canonical slot differs at frame %u", (unsigned)f);
      }
   free(actual); free(input_f); free(input_i);
}

static void suspended_multichannel_case(bool floating, bool discrete)
{
   static float input_f[5000 * 6];
   static int16_t input_i[5000 * 6];
   audio_driver_state_t *st = &audio_driver_st;
   const size_t frames = 5000;
   size_t accepted;
   CHECK(up(floating, discrete ? AUDIO_LAYOUT_5POINT1 : AUDIO_LAYOUT_STEREO, floating),
         "suspended stand-up");
   AUDIO_FLAGS_SET(st, AUDIO_FLAG_SUSPENDED);
   accepted = floating
      ? audio_driver_sample_batch_multi_float(input_f, frames, 6, AUDIO_LAYOUT_5POINT1)
      : audio_driver_sample_batch_multi_int16(input_i, frames, 6, AUDIO_LAYOUT_5POINT1);
   CHECK(accepted == frames, "suspended frames not accepted");
   CHECK(!st->multi_fold && !st->multi_fold_frames, "suspended batch allocated fold staging");
   CHECK(!st->extra.pending && !st->extra.channels, "suspended batch prepared extras");
   CHECK(!cap_frames, "suspended batch reached device");
   CHECK(st->core_layout == AUDIO_LAYOUT_5POINT1, "layout metadata was not retained");
   AUDIO_FLAGS_CLEAR(st, AUDIO_FLAG_SUSPENDED);
}

static void stereo_ring_format_case(bool source_float, bool ring_float)
{
   const size_t frames = 5000;
   audio_driver_state_t *st = &audio_driver_st;
   size_t sample = ring_float ? sizeof(float) : sizeof(int16_t);
   float *inf = (float*)malloc(frames * 2 * sizeof(float));
   int16_t *ini = (int16_t*)malloc(frames * 2 * sizeof(int16_t));
   uint8_t *expected = (uint8_t*)malloc(frames * 2 * sizeof(float));
   uint8_t *actual = (uint8_t*)malloc(frames * AUDIO_PIPE_CANON_CHANNELS * sizeof(float));
   size_t f, c;
   CHECK(inf && ini && expected && actual, "stereo format buffers");
   if (!inf || !ini || !expected || !actual) goto end;
   CHECK(pipe_up(source_float, ring_float), "stereo format stand-up");
   for (f = 0; f < frames * 2; f++)
   {
      ini[f] = (int16_t)((int)(f * 7919 % 65536) - 32768);
      inf[f] = ini[f] / 16384.0f;
   }
   if (source_float == ring_float)
      memcpy(expected, source_float ? (const void*)inf : (const void*)ini, frames * 2 * sample);
   else if (ring_float) convert_s16_to_float((float*)expected, ini, frames * 2, 1.0f);
   else convert_float_to_s16((int16_t*)expected, inf, frames * 2);
   audio_driver_submit(st, 1.0f, source_float ? (const void*)inf : (const void*)ini,
         frames * 2, source_float, false, false, true);
   CHECK(retro_spsc_read(&st->pipe_ring, actual, frames * st->pipe_frame_bytes)
         == frames * st->pipe_frame_bytes, "stereo ring lost frames");
   for (f = 0; f < frames; f++)
   {
      CHECK(!memcmp(actual + f * st->pipe_frame_bytes, expected + f * 2 * sample, 2 * sample),
            "stereo ring front mismatch at %u", (unsigned)f);
      for (c = 2 * sample; c < st->pipe_frame_bytes; c++)
         CHECK(actual[f * st->pipe_frame_bytes + c] == 0, "stereo ring extra slot is not silent");
   }
   CHECK(st->pipe_layout == AUDIO_LAYOUT_STEREO,
         "stereo ring layout changed");
end:
   free(actual); free(expected); free(ini); free(inf);
}

static void full_wide_ring_case(bool floating)
{
   union { float f[8 * AUDIO_PIPE_CANON_CHANNELS]; int16_t i[8 * AUDIO_PIPE_CANON_CHANNELS]; } input, output;
   audio_driver_state_t *st = &audio_driver_st;
   size_t i, n, bytes;
   unsigned round;
   CHECK(pipe_up(floating, floating), "full wide ring stand-up");
   retro_spsc_free(&st->pipe_ring);
   CHECK(retro_spsc_init(&st->pipe_ring, 128), "small wide ring");
   AUDIO_FLAGS_SET(st, AUDIO_FLAG_NONBLOCK);
   bytes = st->pipe_frame_bytes;
   for (round = 0; round < 64; round++)
   {
      for (i = 0; i < 8 * AUDIO_PIPE_CANON_CHANNELS; i++)
      {
         if (floating) input.f[i] = (float)(round * 1000 + i);
         else input.i[i] = (int16_t)(round * 100 + i);
      }
      audio_driver_submit_width(st, 1.0f, &input, 8 * AUDIO_PIPE_CANON_CHANNELS,
            floating, false, false, true, AUDIO_PIPE_CANON_CHANNELS);
      n = retro_spsc_read_avail(&st->pipe_ring);
      CHECK(n == (128 / bytes) * bytes, "wide ring published a partial frame");
      CHECK(retro_spsc_read(&st->pipe_ring, &output, n) == n, "wide ring drain");
      CHECK(!memcmp(&input, &output, n), "wide ring frame alignment changed after drop");
   }
   AUDIO_FLAGS_CLEAR(st, AUDIO_FLAG_NONBLOCK);
   AUDIO_FLAGS_SET(st, AUDIO_FLAG_STARTED);
   retro_atomic_store_release_int(&st->pipe_stalled, 0);
   audio_driver_submit_width(st, 1.0f, &input, 8 * AUDIO_PIPE_CANON_CHANNELS,
         floating, false, false, true, AUDIO_PIPE_CANON_CHANNELS);
   CHECK(retro_atomic_load_acquire_int(&st->pipe_stalled), "partial-frame room must enter the bounded wait");
   CHECK(retro_spsc_read_avail(&st->pipe_ring) == (128 / bytes) * bytes,
         "blocking publish split a frame");

}

static void record_stereo_entry_case(unsigned kind, uint32_t layout)
{
   size_t frames = kind ? 2053 : 1024, offset = kind ? 14 : 0, i;
   audio_driver_state_t *st = &audio_driver_st;
   recording_state_t *rs = recording_state_get_ptr();
   int16_t *input = (int16_t*)malloc((frames * 2 + offset) * sizeof(int16_t));
   float *input_f = (float*)malloc((frames * 2 + offset) * sizeof(float));
   int16_t *narrow = (int16_t*)malloc(frames * 2 * sizeof(int16_t));
   int16_t *expected = (int16_t*)malloc(frames * 8 * sizeof(int16_t));
   CHECK(input && input_f && narrow && expected, "record entry buffers");
   if (!input || !input_f || !narrow || !expected) goto end;
   CHECK(up(kind == 2, AUDIO_LAYOUT_STEREO, true), "record entry stand-up");
   AUDIO_FLAGS_CLEAR(st, AUDIO_FLAG_ACTIVE);
   free(rec_cap); rec_cap = NULL; rec_cap_frames = rec_frames = 0;
   rs->driver = &rec_driver; rs->data = rs; rs->layout = layout;
   rs->channels = rec_channels = audio_layout_channels(layout);
   for (i = 0; i < frames * 2 + offset; i++)
   {
      input[i] = (int16_t)((int)(i * 7919 % 65536) - 32768);
      input_f[i] = input[i] / 16384.0f;
   }
   if (kind == 2) convert_float_to_s16(narrow, input_f + offset, frames * 2);
   else memcpy(narrow, input + offset, frames * 2 * sizeof(int16_t));
   audio_layout_remap_s16(expected, layout, narrow, AUDIO_LAYOUT_STEREO, frames);
   if (!kind)
   {
      st->sample_accum = input; st->data_ptr = frames * 2;
      audio_driver_sample_accum_flush(st);
      CHECK(st->data_ptr == 0, "accumulator was not emptied");
      st->sample_accum = NULL;
   }
   else
   {
      st->rewind_buf = input; st->rewind_buf_f = kind == 2 ? input_f : NULL;
      st->rewind_ptr = offset; st->rewind_size = frames * 2 + offset;
      audio_driver_frame_is_reverse();
      st->rewind_buf = NULL; st->rewind_buf_f = NULL;
      st->rewind_ptr = st->rewind_size = 0;
   }
   CHECK(rec_frames == frames, "record entry lost frames");
   if (rec_frames == frames)
      CHECK(!memcmp(rec_cap, expected, frames * rec_channels * sizeof(int16_t)),
            "record entry did not map stereo to the recorder layout");
   CHECK(st->record_remap_frames <= 1024 * rec_channels, "record entry staging is unbounded");
   rs->driver = NULL; rs->data = NULL;
end:
   free(expected); free(narrow); free(input_f); free(input);
}

static void wide_wrap_case(bool floating)
{
   const size_t frames = 512;
   union { float f[512 * AUDIO_PIPE_CANON_CHANNELS]; int16_t i[512 * AUDIO_PIPE_CANON_CHANNELS]; } input;
   audio_driver_state_t *st = &audio_driver_st;
   float *reference = NULL;
   size_t reference_frames = 0, f, c, sample = floating ? sizeof(float) : sizeof(int16_t);
   unsigned wrap;
   for (f = 0; f < frames; f++)
      for (c = 0; c < AUDIO_PIPE_CANON_CHANNELS; c++)
      {
         int16_t v = c < 6 ? (int16_t)((int)((f * 97 + c * 7919) % 30000) - 15000) : 0;
         if (floating) input.f[f * AUDIO_PIPE_CANON_CHANNELS + c] = v / 32768.0f;
         else input.i[f * AUDIO_PIPE_CANON_CHANNELS + c] = v;
      }
   for (wrap = 0; wrap < 2; wrap++)
   {
      bool ready = pipe_up(floating, floating);
      CHECK(ready, "wide wrap stand-up");
      if (!ready) { free(reference); return; }
      if (wrap)
      {
         size_t start = st->pipe_ring.capacity - 2 * sample;
         retro_atomic_size_init(&st->pipe_ring.head, start);
         retro_atomic_size_init(&st->pipe_ring.tail, start);
         st->pipe_ring.cached_head = st->pipe_ring.cached_tail = start;
      }
      CHECK(audio_pipeline_layout_publish(&st->pipe_layouts,
               retro_atomic_load_relaxed_size(&st->pipe_ring.head), AUDIO_LAYOUT_5POINT1),
            "wide wrap layout publish");
      CHECK(retro_spsc_write_frames(&st->pipe_ring, &input, frames, st->pipe_frame_bytes) == frames,
            "wide wrap input publish");
      audio_driver_pipeline_consume(st);
      CHECK(retro_spsc_read_avail(&st->pipe_ring) == 0, "wide wrap did not release the input");
      CHECK(cap_frames > 0, "wide wrap produced no device output");
      if (!wrap)
      {
         reference_frames = cap_frames;
         reference = (float*)malloc(cap_frames * 6 * sizeof(float));
         CHECK(reference != NULL, "wide wrap reference allocation");
         if (reference) memcpy(reference, cap, cap_frames * 6 * sizeof(float));
      }
      else
      {
         CHECK(cap_frames == reference_frames, "wrapped output frame count changed");
         if (reference && cap_frames == reference_frames)
            CHECK(!memcmp(reference, cap, cap_frames * 6 * sizeof(float)),
                  "contiguous and wrapped device output differs");
      }
   }
   free(reference);
}

static void layout_epoch_case(bool floating, bool wrapped)
{
   static const unsigned layouts[] = { AUDIO_LAYOUT_5POINT1, AUDIO_LAYOUT_STEREO,
      AUDIO_LAYOUT_7POINT1, AUDIO_LAYOUT_5POINT1 };
   union { float f[128 * 8]; int16_t i[128 * 8]; } input;
   audio_driver_state_t *st = &audio_driver_st;
   float *reference = NULL;
   size_t reference_frames = 0, f;
   unsigned queued, block;
   for (queued = 0; queued < 2; queued++)
   {
      bool ready = pipe_up(floating, floating);
      CHECK(ready, "layout epoch stand-up");
      if (!ready) { free(reference); return; }
      if (wrapped)
      {
         size_t start = st->pipe_ring.capacity - st->pipe_frame_bytes * 64;
         retro_atomic_size_init(&st->pipe_ring.head, start);
         retro_atomic_size_init(&st->pipe_ring.tail, start);
         st->pipe_ring.cached_head = st->pipe_ring.cached_tail = start;
      }
      for (block = 0; block < 4; block++)
      {
         unsigned channels = audio_layout_channels(layouts[block]);
         for (f = 0; f < 128 * channels; f++)
         {
            int16_t v = (int16_t)((int)((f * 97 + block * 7919) % 30000) - 15000);
            if (floating) input.f[f] = v / 32768.0f;
            else input.i[f] = v;
         }
         if (channels == 2)
            audio_driver_submit(st, 1.0f, &input, 256, floating, false, false, true);
         else
            CHECK(audio_driver_multi_pipe(st, &input, 128, channels, layouts[block], floating),
                  "layout epoch input publish");
         if (!queued) audio_driver_pipeline_consume(st);
      }
      /* A bounded loop also catches a stranded boundary without hanging. */
      for (block = 0; block < 8 && retro_spsc_read_avail(&st->pipe_ring); block++)
         audio_driver_pipeline_consume(st);
      CHECK(!retro_spsc_read_avail(&st->pipe_ring), "layout epoch input stranded");
      CHECK(cap_frames > 0, "layout epoch has no device output");
      if (!queued)
      {
         reference_frames = cap_frames;
         reference = (float*)malloc(cap_frames * 6 * sizeof(float));
         CHECK(reference != NULL, "layout epoch reference allocation");
         if (reference) memcpy(reference, cap, cap_frames * 6 * sizeof(float));
      }
      else
      {
         CHECK(cap_frames == reference_frames, "layout epoch frame count differs");
         if (reference && cap_frames == reference_frames)
            CHECK(!memcmp(reference, cap, cap_frames * 6 * sizeof(float)),
                  "queued layout changes reinterpret old samples");
      }
   }
   free(reference);
}

static size_t epoch_underrun(void *data) { (void)data; return 1; }

static void layout_epoch_pressure_case(bool floating)
{
   audio_driver_state_t *st = &audio_driver_st;
   union { float f[11]; int16_t i[11]; } input;
   size_t held;
   unsigned i;
   bool sync = config_get_ptr()->bools.audio_sync;
   bool ready = pipe_up(floating, floating);
   CHECK(ready, "epoch pressure stand-up");
   if (!ready) return;
   memset(&input, 0, sizeof(input));
   for (i = 0; i < AUDIO_PIPELINE_LAYOUT_CAPACITY; i++)
   {
      st->pipe_layout = i & 1 ? AUDIO_LAYOUT_7POINT1 : AUDIO_LAYOUT_5POINT1;
      audio_driver_submit_width(st, 1.0f, &input, 11, floating, false, false, true, 11);
   }
   held = retro_spsc_read_avail(&st->pipe_ring);
   CHECK(held == AUDIO_PIPELINE_LAYOUT_CAPACITY * st->pipe_frame_bytes,
         "epoch pressure did not fill metadata");
   st->pipe_layout = AUDIO_LAYOUT_STEREO;
   {
      bool speedup = config_get_ptr()->bools.audio_fastforward_speedup;
      config_get_ptr()->bools.audio_fastforward_speedup = true;
      audio_driver_publish_runloop();
      audio_driver_submit_width(st, 1.0f, &input, 11, floating, false, true, true, 11);
      config_get_ptr()->bools.audio_fastforward_speedup = speedup;
      audio_driver_publish_runloop();
      CHECK(st->pipe_ff_frames == 1, "metadata pressure skipped source cadence accounting");
      audio_driver_frame_end();
      CHECK(st->last_flush_time > 0 && !st->pipe_ff_frames,
            "frame end did not measure source dropped by metadata pressure");
   }
   CHECK(retro_spsc_read_avail(&st->pipe_ring) == held,
         "unlabelled frame entered a full metadata queue");
   st->pipe_priming = true;
   audio_driver_pipeline_consume(st);
   CHECK(!st->pipe_priming && retro_spsc_read_avail(&st->pipe_ring) < held,
         "full metadata deadlocked startup priming");
   snap_pause(true);
   for (i = 0; i < AUDIO_PIPELINE_LAYOUT_CAPACITY && retro_spsc_read_avail(&st->pipe_ring); i++)
      audio_driver_pipeline_consume(st);
   CHECK(!retro_spsc_read_avail(&st->pipe_ring), "pause did not drain epoch audio");
   /* Underrun discard crosses all outstanding boundaries and releases space. */
   snap_pause(false);
   audio_driver_submit_width(st, 1.0f, &input, 11, floating, false, false, true, 11);
   st->pipe_layout = AUDIO_LAYOUT_5POINT1;
   audio_driver_submit_width(st, 1.0f, &input, 11, floating, false, false, true, 11);
   scripted_threaded.underruns = epoch_underrun;
   st->buffer_size = 0;
   config_get_ptr()->bools.audio_sync = false;
   snap_pause(false);
   cap_frames = 0;
   audio_driver_pipeline_consume(st);
   CHECK(!retro_spsc_read_avail(&st->pipe_ring) && !cap_frames,
         "underrun discard replayed stale scratch");
   CHECK(st->pipe_layouts.current_layout == AUDIO_LAYOUT_5POINT1,
         "discard did not retire layout boundaries");
   config_get_ptr()->bools.audio_sync = sync;
   snap_pause(false);
}

static unsigned short_calls;
static unsigned short_wait_calls;
static bool short_zero, short_no_room, short_fail;
static ssize_t short_device_write(void *data, const void *buffer, size_t bytes)
{
   size_t limit = 31 * dev_channels * (dev_float ? sizeof(float) : sizeof(int16_t));
   short_calls++;
   if (short_fail) return -1;
   if (short_zero) return 0;
   if (bytes > limit) bytes = limit;
   return dev_write(data, buffer, bytes);
}

static size_t short_device_wait(void *data, size_t bytes)
{
   size_t limit = 17 * dev_channels * (dev_float ? sizeof(float) : sizeof(int16_t));
   (void)data;
   short_wait_calls++;
   if (short_no_room) return 0;
   return bytes < limit ? bytes : limit;
}

static void pending_output_case(bool floating, bool stereo)
{
   union { float f[512 * 6]; int16_t i[512 * 6]; } input;
   audio_driver_state_t *st = &audio_driver_st;
   float *reference = NULL;
   size_t reference_frames = 0, f;
   unsigned partial, block, pass;
   for (f = 0; f < 512 * 6; f++)
   {
      int16_t v = (int16_t)((int)((f * 7919) % 30000) - 15000);
      if (floating) input.f[f] = v / 32768.0f;
      else input.i[f] = v;
   }
   for (partial = 0; partial < 2; partial++)
   {
      bool ready = pipe_up(floating, floating);
      CHECK(ready, "pending output stand-up");
      if (!ready) { free(reference); return; }
      if (stereo)
      {
         dev_layout = st->out_layout = AUDIO_LAYOUT_STEREO;
         dev_channels = st->out_channels = 2;
      }
      if (partial)
      {
         scripted_threaded.write = short_device_write;
         scripted_threaded.wait_writable = short_device_wait;
      }
      short_calls = 0;
      short_zero = short_no_room = short_fail = false;
      for (block = 0; block < 2; block++)
      {
         CHECK(audio_driver_multi_pipe(st, &input, 512, 6, AUDIO_LAYOUT_5POINT1, floating),
               "pending source publish");
         /* Finish old output before touching newly published source. */
         for (pass = 0; pass < 128 && st->pipe_pending_bytes; pass++)
         {
            size_t held = retro_spsc_read_avail(&st->pipe_ring);
            audio_driver_pipeline_consume(st);
            CHECK(retro_spsc_read_avail(&st->pipe_ring) == held,
                  "pending output let SRC overwrite owned scratch");
         }
         CHECK(!st->pipe_pending_bytes, "pending output did not progress");
         audio_driver_pipeline_consume(st);
         if (partial)
         {
            const uint8_t *pointer = st->pipe_pending;
            size_t bytes = st->pipe_pending_bytes;
            uint8_t *saved = (uint8_t*)malloc(bytes);
            CHECK(bytes > 0 && saved != NULL, "short write was not retained");
            if (saved && bytes) memcpy(saved, pointer, bytes);
            short_no_room = true;
            audio_driver_pipeline_consume(st);
            short_no_room = false;
            CHECK(st->pipe_pending == pointer && st->pipe_pending_bytes == bytes,
                  "unwritable device changed pending ownership");
            short_zero = true;
            audio_driver_pipeline_consume(st);
            short_zero = false;
            CHECK(st->pipe_pending == pointer && st->pipe_pending_bytes == bytes,
                  "zero write changed output ownership");
            if (saved && bytes) CHECK(!memcmp(saved, pointer, bytes), "zero write changed pending samples");
            free(saved);
         }
      }
      for (pass = 0; pass < 128 && st->pipe_pending_bytes; pass++)
         audio_driver_pipeline_consume(st);
      CHECK(!st->pipe_pending_bytes && !retro_spsc_read_avail(&st->pipe_ring),
            "pending output stranded at end of input");
      CHECK(st->sink_accepted == cap_frames, "retry accounting lost or duplicated frames");
      if (!partial)
      {
         reference_frames = cap_frames;
         reference = (float*)malloc(cap_frames * dev_channels * sizeof(float));
         CHECK(reference != NULL, "pending output reference allocation");
         if (reference) memcpy(reference, cap, cap_frames * dev_channels * sizeof(float));
      }
      else
      {
         CHECK(short_calls > 2, "short write retries were not exercised");
         CHECK(cap_frames == reference_frames, "short write output frame count differs");
         if (reference && cap_frames == reference_frames)
            CHECK(!memcmp(reference, cap, cap_frames * dev_channels * sizeof(float)),
                  "short writes changed native device samples");
      }
   }
   free(reference);
}

static void pending_lifecycle_case(bool floating)
{
   audio_driver_state_t *st = &audio_driver_st;
   unsigned scenario;
   bool sync = config_get_ptr()->bools.audio_sync;
   for (scenario = 0; scenario < 4; scenario++)
   {
      bool ready = pipe_up(floating, floating);
      CHECK(ready, "pending lifecycle stand-up");
      if (!ready) return;
      scripted_threaded.write = short_device_write;
      short_zero = true;
      short_no_room = short_fail = false;
      memset(st->output_samples_buf, 0, 64 * 6 * sizeof(float));
      {
         ssize_t written = audio_driver_write_frames(st, st->current_audio,
               st->output_samples_buf, 64, floating, false);
         CHECK(written == 0, "pending lifecycle setup write");
         audio_driver_retain_output(st, st->output_samples_buf, 64, written);
      }
      CHECK(st->pipe_pending_bytes > 0, "pending lifecycle setup ownership");
      short_zero = false;
      if (scenario == 0)
         snap_pause(true);
      else if (scenario == 1)
         short_fail = true;
      else if (scenario == 2)
      {
         config_get_ptr()->bools.audio_sync = false;
         snap_pause(false);
         scripted_threaded.underruns = epoch_underrun;
      }
      else
      {
         CHECK(audio_driver_stop(), "pending stop failed");
         CHECK(!st->pipe_pending && !st->pipe_pending_bytes, "stop retained stale output");
         continue;
      }
      audio_driver_pipeline_consume(st);
      CHECK(!st->pipe_pending && !st->pipe_pending_bytes, "pending output survived discontinuity");
      CHECK(!cap_frames, "stale pending output reached device");
   }
   short_fail = false;
   config_get_ptr()->bools.audio_sync = sync;
}

static void canonical_prefix_case(void)
{
   static uint8_t input[1024 * 8 * sizeof(float) + 2];
   static uint8_t saved[sizeof(input)];
   static uint8_t output[1024 * AUDIO_PIPE_CANON_CHANNELS * sizeof(float) + 2];
   static const size_t counts[] = { 0, 1, 32, 512, 1024 };
   static const uint32_t bits[] = { 0, 0x80000000u, 0x7fc01234u, 0x7fa12345u,
      0x7f800000u, 0xff800000u, 1, 0xffffffffu };
   unsigned floating, channels, count;
   size_t i, f, sample, frames, bytes;
   for (floating = 0; floating < 2; floating++)
      for (channels = 6; channels <= 8; channels += 2)
         for (count = 0; count < sizeof(counts) / sizeof(counts[0]); count++)
         {
            sample = floating ? sizeof(float) : sizeof(int16_t);
            frames = counts[count];
            bytes = frames * AUDIO_PIPE_CANON_CHANNELS * sample;
            for (i = 0; i < sizeof(input); i++) input[i] = (uint8_t)(i * 97 + count * 17);
            if (floating)
               for (i = 0; i < frames * channels; i++)
                  memcpy(input + 1 + i * sample, &bits[i % 8], sample);
            memcpy(saved, input, sizeof(input));
            memset(output, 0xa5, sizeof(output));
            CHECK(audio_driver_pipe_widen_prefix(output + 1, input + 1, frames, channels,
                     channels == 6 ? AUDIO_LAYOUT_5POINT1 : AUDIO_LAYOUT_7POINT1, floating),
                  "canonical prefix refused native layout");
            for (f = 0; f < frames; f++)
            {
               size_t offset = 1 + f * AUDIO_PIPE_CANON_CHANNELS * sample;
               CHECK(!memcmp(output + offset, input + 1 + f * channels * sample, 6 * sample),
                     "canonical prefix changed source bits");
               if (channels == 8)
                  CHECK(!memcmp(output + offset + 9 * sample,
                           input + 1 + (f * channels + 6) * sample, 2 * sample),
                        "canonical side channels changed source bits");
               for (i = 6 * sample; i < (channels == 8 ? 9 : AUDIO_PIPE_CANON_CHANNELS) * sample; i++)
                  CHECK(output[offset + i] == 0, "canonical absent position is not silent");
            }
            CHECK(output[0] == 0xa5, "canonical prefix underflow");
            for (i = bytes + 1; i < sizeof(output); i++)
               CHECK(output[i] == 0xa5, "canonical prefix overflow");
            CHECK(!memcmp(saved, input, sizeof(input)), "canonical prefix modified source");
         }
   memset(output, 0xa5, sizeof(output));
   CHECK(!audio_driver_pipe_widen_prefix(output, input, 32, 6, AUDIO_LAYOUT_5POINT1_SURROUND, true),
         "non-prefix layout skipped general remapping");
   for (i = 0; i < sizeof(output); i++) CHECK(output[i] == 0xa5, "rejected prefix changed output");
}

static void resampler_discontinuity_case(unsigned backend, bool floating,
      unsigned scenario, bool hq)
{
   static const char *names[] = { "sinc", "nearest", "cc" };
   union { float f[128 * 6]; int16_t i[128 * 6]; } input;
   float reference[2048 * 6];
   size_t reference_frames = 0, f;
   unsigned dirty;
   bool sync = config_get_ptr()->bools.audio_sync;
   audio_driver_state_t *st = &audio_driver_st;
   for (dirty = 0; dirty < 2; dirty++)
   {
      void *front, *native, *extras[4];
      unsigned i;
      bool ready = pipe_up(floating, floating);
      CHECK(ready, "reset stand-up");
      if (!ready) return;
      st->src_ratio_orig = st->src_ratio_curr = hq ? 2.0 : 48000.0 / 44100.0;
      st->resampler_hq = hq && backend == 0;
      strcpy(st->resampler_ident, names[backend]);
      CHECK(retro_resampler_realloc_hq(&st->resampler_data, &st->resampler,
            names[backend], st->resampler_quality, st->src_ratio_orig,
            st->resampler_hq), "reset float backend");
      if (!floating)
      {
         config_get_ptr()->bools.audio_fastpath_s16 = true;
         st->resampler_data_int16 = audio_driver_int16_resampler_new(st);
         st->resampler_int16_process = backend == 0 ? sinc_resampler_int16_process
            : backend == 1 ? nearest_resampler_int16_process : cc_resampler_int16_process;
         st->resampler_int16_free = backend == 0 ? sinc_resampler_int16_free
            : backend == 1 ? nearest_resampler_int16_free : cc_resampler_int16_free;
         st->resampler_int16_reset = backend == 0 ? sinc_resampler_int16_reset
            : backend == 1 ? nearest_resampler_int16_reset : cc_resampler_int16_reset;
         CHECK(st->resampler_data_int16 != NULL, "reset native backend");
      }
      config_get_ptr()->bools.audio_sync = true;
      snap_pause(false);
      if (dirty)
      {
         for (f = 0; f < 128 * 6; f++)
            if (floating) input.f[f] = (float)(f % 6 + 1) / 16.0f;
            else input.i[f] = (int16_t)((f % 6 + 1) * 2048);
         CHECK(audio_driver_multi_pipe(st, &input, 128, 6, AUDIO_LAYOUT_5POINT1, floating), "reset warm publish");
         audio_driver_pipeline_consume(st);
         CHECK(st->extra.nres == 2 && st->extra.res_int16 == !floating,
               "reset did not warm native extra lanes");
         front = st->resampler_data; native = st->resampler_data_int16;
         for (i = 0; i < 4; i++) extras[i] = st->extra.res[i];
         if (scenario == 0 || scenario == 1)
         {
            CHECK(audio_driver_multi_pipe(st, &input, 128, 6, AUDIO_LAYOUT_5POINT1, floating), "reset discard publish");
            if (scenario == 0)
            {
               config_get_ptr()->bools.audio_sync = false;
               snap_pause(false);
               st->buffer_size = 0;
               scripted_threaded.underruns = epoch_underrun;
            }
            else snap_pause(true);
            audio_driver_pipeline_consume(st);
            CHECK(!retro_spsc_read_avail(&st->pipe_ring), "reset did not discard source");
         }
         else if (scenario == 2)
         {
            st->pipe_pending = (const uint8_t*)st->output_samples_buf;
            st->pipe_pending_bytes = 6 * sizeof(float);
            snap_pause(true);
            audio_driver_pipeline_consume(st);
            CHECK(!st->pipe_pending_bytes, "reset retained pending output");
         }
         else CHECK(audio_driver_stop(), "reset stop failed");
         CHECK(front == st->resampler_data && native == st->resampler_data_int16,
               "reset replaced primary resamplers");
         for (i = 0; i < 4; i++) CHECK(extras[i] == st->extra.res[i], "reset replaced extra lane");
         scripted_threaded.underruns = NULL;
         config_get_ptr()->bools.audio_sync = true;
         snap_pause(false);
         AUDIO_FLAGS_SET(st, AUDIO_FLAG_STARTED);
      }
      cap_frames = 0;
      memset(&input, 0, sizeof(input));
      CHECK(audio_driver_multi_pipe(st, &input, 128, 6, AUDIO_LAYOUT_5POINT1, floating), "reset fresh publish");
      audio_driver_pipeline_consume(st);
      CHECK(cap_frames > 0 && cap_frames <= 2048, "reset output count");
      if (!dirty)
      {
         reference_frames = cap_frames;
         if (cap_frames <= 2048) memcpy(reference, cap, cap_frames * 6 * sizeof(float));
      }
      else
      {
         CHECK(cap_frames == reference_frames, "reset %s/%u/%u/%u phase differs", names[backend], floating, scenario, hq);
         if (cap_frames == reference_frames && cap_frames <= 2048)
            CHECK(!memcmp(reference, cap, cap_frames * 6 * sizeof(float)),
                  "reset %s/%u/%u/%u leaked pre-gap samples", names[backend], floating, scenario, hq);
      }
   }
   config_get_ptr()->bools.audio_fastpath_s16 = false;
   config_get_ptr()->bools.audio_sync = sync;
}

static void resampler_discontinuity_cases(void)
{
   unsigned backend, floating, scenario, hq;
   unsigned before = failures;
   for (backend = 0; backend < 3; backend++)
      for (floating = 0; floating < 2; floating++)
         for (scenario = 0; scenario < 4; scenario++)
            for (hq = 0; hq < 2; hq++)
               resampler_discontinuity_case(backend, floating, scenario, hq);
   printf("   native SRC discontinuities: 48 cases, %u failures\n", failures - before);
}


/* Real queue/WSOLA output handed directly to the shipping frontend renderer. */
static size_t native_render_case(bool floating, bool wide, bool hq, bool filter)
{
   static union { float f[2048 * 11]; int16_t i[2048 * 11]; } input;
   union { float f[257 * 11]; int16_t i[257 * 11]; } output, saved;
   static const uint32_t layouts[] = { AUDIO_LAYOUT_5POINT1, AUDIO_LAYOUT_7POINT1,
      AUDIO_LAYOUT_STEREO, AUDIO_LAYOUT_5POINT1 };
   static const uint32_t tempos[] = { 65536, 90112, 32768, 262144 };
   audio_driver_state_t *st = &audio_driver_st;
   float *reference = NULL;
   size_t reference_frames = 0;
   unsigned fragmented;
   bool old_speedup = config_get_ptr()->bools.audio_fastforward_speedup;
   float old_slowmotion = config_get_ptr()->floats.slowmotion_ratio;
   for (fragmented = 0; fragmented < 3; fragmented++)
   {
      audio_pipeline_stretch_t *stage;
      struct audio_pipeline_stretch_block block;
      unsigned channels = wide ? 11 : 2, segment, c, iterations = 0;
      size_t frame = channels * (floating ? sizeof(float) : sizeof(int16_t));
      size_t used = 0, f;
      uint32_t serial = 0;
      bool complete = false;
      CHECK(pipe_up(floating, floating), "native renderer stand-up");
      free(cap); cap = NULL; cap_cap = cap_frames = 0;
      if (!wide)
      {
         st->pipe_channels = dev_channels = st->out_channels = 2;
         st->pipe_frame_bytes = frame;
         dev_layout = st->out_layout = AUDIO_LAYOUT_STEREO;
      }
      st->src_ratio_orig = st->src_ratio_curr = hq ? 2.0 : 48000.0 / 44100.0;
      st->resampler_hq = hq;
      CHECK(retro_resampler_realloc_hq(&st->resampler_data, &st->resampler,
            "sinc", st->resampler_quality, st->src_ratio_orig, hq), "native renderer SRC");
      if (!floating)
      {
         config_get_ptr()->bools.audio_fastpath_s16 = true;
         st->resampler_data_int16 = audio_driver_int16_resampler_new(st);
         st->resampler_int16_process = sinc_resampler_int16_process;
         st->resampler_int16_free = sinc_resampler_int16_free;
         st->resampler_int16_reset = sinc_resampler_int16_reset;
         snap_pause(false);
         CHECK(st->resampler_data_int16 != NULL, "native renderer integer SRC");
      }
      if (fragmented)
      {
         CHECK(audio_driver_pipeline_transport_prepare(48000, 1), "owned transport prepare");
         stage = st->pipe_transport;
      }
      else
         stage = audio_pipeline_stretch_new(48000, channels, floating, 1,
               &st->pipe_ring, &st->pipe_layouts, &output, 257);
      CHECK(stage != NULL, "native renderer stage");
      if (!stage) { free(reference); return 0; }
      for (segment = 0; segment < 4; segment++)
      {
         uint32_t layout = wide ? layouts[segment] : AUDIO_LAYOUT_STEREO;
         if (fragmented)
         {
            CHECK(audio_pipeline_layout_publish(&st->pipe_layouts,
                     retro_atomic_load_relaxed_size(&st->pipe_ring.head), layout),
                  "owned renderer layout");
            CHECK(audio_driver_pipeline_transport_request(tempos[segment], segment != 0,
                     segment == 2, filter && segment != 3 ? 800 + segment * 900 : 0),
                  "owned renderer processing request");
         }
         else
            CHECK(audio_pipeline_layout_publish_processing(&st->pipe_layouts,
                  segment * 512 * frame, layout, tempos[segment], segment != 0,
                  segment == 2, filter && segment != 3 ? 800 + segment * 900 : 0), "native renderer control publication");
         for (f = segment * 512; f < (segment + 1) * 512; f++)
            for (c = 0; c < channels; c++)
            {
               int16_t value = layout & (1u << c)
                  ? (int16_t)((int)((f * 7919 + c * 977) % 30000) - 15000) : 0;
               if (floating) input.f[f * channels + c] = value / 32768.0f;
               else input.i[f * channels + c] = value;
            }
         CHECK(retro_spsc_write_frames(&st->pipe_ring,
                  (const uint8_t*)&input + segment * 512 * frame, 512, frame) == 512,
               "native renderer source publication");
      }
      if (fragmented)
      {
         scripted_threaded.write = short_device_write;
         scripted_threaded.wait_writable = short_device_wait;
      }
      short_calls = 0;
      short_zero = fragmented;
      short_fail = short_no_room = false;
      cap_frames = 0;
      if (fragmented == 2) st->pipe_pass_frames = 37;
      while (!complete || st->pipe_pending_bytes)
      {
         size_t accepted, tail;
         if (++iterations > 100000) abort();
         if (fragmented)
         {
            bool pending = st->pipe_pending_bytes != 0;
            tail = retro_atomic_load_relaxed_size(&st->pipe_ring.tail);
            if (iterations == 1)
            {
               size_t events = retro_atomic_load_relaxed_size(&st->pipe_layouts.tail);
               short_no_room = true;
               CHECK(audio_driver_pipeline_transport_step(stage, &st->pipe_transport_serial, 71, 37,
                        false, &complete), "transport wait rejection");
               CHECK(tail == retro_atomic_load_relaxed_size(&st->pipe_ring.tail)
                     && events == retro_atomic_load_relaxed_size(&st->pipe_layouts.tail)
                     && !st->pipe_transport_serial && !cap_frames, "failed device wait advanced transport");
               short_no_room = false;
               snap_pause(true);
               CHECK(!audio_driver_pipeline_transport_step(stage, &st->pipe_transport_serial, 71, 37,
                        false, &complete), "paused transport must defer to lifecycle owner");
               CHECK(tail == retro_atomic_load_relaxed_size(&st->pipe_ring.tail)
                     && events == retro_atomic_load_relaxed_size(&st->pipe_layouts.tail),
                     "paused transport advanced source/control");
               snap_pause(false);
               CHECK(audio_driver_pipeline_transport_step(stage, &st->pipe_transport_serial, 71, 0,
                        false, &complete), "transport zero budget");
               CHECK(tail == retro_atomic_load_relaxed_size(&st->pipe_ring.tail),
                     "zero output budget consumed input");
            }
            if (pending) short_zero = false;
            config_get_ptr()->bools.audio_fastforward_speedup = true;
            config_get_ptr()->floats.slowmotion_ratio = 3.0f;
            runloop_state_get_ptr()->flags = RUNLOOP_FLAG_SLOWMOTION
                  | RUNLOOP_FLAG_FASTMOTION;
            audio_driver_publish_runloop();
            retro_atomic_store_release_int(&st->pipe_ff_mult_q16, 3 * 65536);
            if (fragmented == 2 && used != 2048)
               CHECK(audio_driver_callback(), "scheduled native transport callback");
            else
               CHECK(audio_driver_pipeline_transport_step(stage, &st->pipe_transport_serial, 71, 37,
                        used == 2048, &complete), "paced native transport step");
            if (pending)
               CHECK(tail == retro_atomic_load_relaxed_size(&st->pipe_ring.tail),
                     "transport retry consumed source");
            used += (retro_atomic_load_relaxed_size(&st->pipe_ring.tail) - tail) / frame;
            if (cap_frames)
               CHECK(st->stat_core_is_float == floating && st->stat_frontend_is_float == floating,
                     "paced transport left native lane");
            continue;
         }
         if (st->pipe_pending_bytes)
         {
            tail = retro_atomic_load_relaxed_size(&st->pipe_ring.tail);
            short_zero = false;
            audio_driver_pipeline_retry(st);
            CHECK(retro_atomic_load_relaxed_size(&st->pipe_ring.tail) == tail,
                  "device retry consumed new native source");
            continue;
         }
         if (used == 2048)
            CHECK(audio_pipeline_stretch_finish(stage, fragmented ? 97 : 257, &block, &complete), "native renderer finish");
         else
            CHECK(audio_pipeline_stretch_next(stage, fragmented ? 71 : 512,
                     fragmented ? 97 : 257, &block), "native renderer next");
         used += block.input_used;
         if (serial != block.reset_serial)
         {
            audio_driver_state_lock();
            audio_driver_reset_resamplers(st);
            audio_driver_state_unlock();
            serial = block.reset_serial;
         }
         accepted = fragmented && block.frames > 37 ? 37 : block.frames;
         if (accepted)
         {
            memcpy(&saved, block.data, accepted * frame);
            audio_driver_pipeline_render(st, block.data, accepted, block.layout, 0);
            CHECK(!memcmp(&saved, block.data, accepted * frame), "renderer modified owned native input");
            CHECK(st->stat_core_is_float == floating, "renderer changed source format");
            CHECK(st->stat_frontend_is_float == floating, "renderer left the native frontend lane");
         }
         if (block.passthrough) used += accepted;
         CHECK(audio_pipeline_stretch_consume(stage, accepted), "native renderer acknowledge");
      }
      CHECK(used == 2048 && !retro_spsc_read_avail(&st->pipe_ring), "native renderer source count");
      CHECK(cap_frames > 0, "native renderer produced no audio");
      if (!fragmented)
      {
         reference_frames = cap_frames;
         reference = (float*)malloc(cap_frames * dev_channels * sizeof(float));
         CHECK(reference != NULL, "native renderer reference");
         if (reference) memcpy(reference, cap, cap_frames * dev_channels * sizeof(float));
      }
      else
      {
         CHECK(short_calls > 2, "native renderer did not exercise retries");
         CHECK(cap_frames == reference_frames, "native renderer duration differs");
         if (reference && cap_frames == reference_frames)
            CHECK(!memcmp(reference, cap, cap_frames * dev_channels * sizeof(float)),
                  "fragmented native render differs: float=%u wide=%u HQ=%u filter=%u", floating, wide, hq, filter);
      }
      if (fragmented)
      {
         audio_driver_pipeline_transport_release();
         CHECK(!st->pipe_transport && !st->pipe_transport_output, "owned transport release");
      }
      else audio_pipeline_stretch_free(stage);
   }
   config_get_ptr()->bools.audio_fastpath_s16 = false;
   short_zero = false;
   config_get_ptr()->bools.audio_fastforward_speedup = old_speedup;
   config_get_ptr()->floats.slowmotion_ratio = old_slowmotion;
   snap_pause(false);
   free(reference);
   return reference_frames;
}

static void transport_settings_cases(void)
{
   extern unsigned transport_control_calls;
   audio_driver_state_t *st = &audio_driver_st;
   settings_t *settings = config_get_ptr();
   unsigned floating, wide, before = failures;
   for (floating = 0; floating < 2; floating++)
      for (wide = 0; wide < 2; wide++)
      {
         audio_driver_t wrapper;
         const audio_driver_t *saved;
         unsigned calls;
         uint32_t nan_rate = UINT32_C(0x7fc00000);
         CHECK(pipe_up(floating, floating), "settings stand-up");
         if (!wide)
         {
            st->pipe_channels = 2;
            st->pipe_frame_bytes = 2 * (floating ? sizeof(float) : sizeof(int16_t));
         }
         saved = st->current_audio; wrapper = *saved; wrapper.ident = "audio-thread";
         st->current_audio = &wrapper;
         runloop_state_get_ptr()->flags = 0;
         settings->bools.audio_time_stretch = false;
         settings->bools.audio_time_stretch_lowpass = false;
         audio_driver_publish_runloop();
         calls = transport_control_calls;
         CHECK(audio_driver_transport_configure(settings) && !st->pipe_transport
               && transport_control_calls == calls, "disabled setting touched transport");
         settings->bools.audio_time_stretch_lowpass = true;
         CHECK(audio_driver_transport_configure(settings) && st->pipe_transport
               && st->transport_lpf_only, "independent lowpass startup");
         audio_driver_pipeline_transport_release();
         calls = transport_control_calls;
         settings->bools.audio_time_stretch = true;
         st->pipe_threaded = false;
         st->core_layout = AUDIO_LAYOUT_5POINT1;
         CHECK(audio_driver_transport_configure(settings) && st->inline_transport
               && st->inline_transport->channels == AUDIO_PIPE_CANON_CHANNELS, "wide inline startup failed");
         audio_driver_inline_free(st);
         st->core_layout = AUDIO_LAYOUT_STEREO;
         st->pipe_threaded = true;
         st->input = 7999;
         CHECK(!audio_driver_transport_configure(settings), "low rate accepted");
         st->input = 192001;
         CHECK(!audio_driver_transport_configure(settings), "high rate accepted");
         memcpy(&st->input, &nan_rate, sizeof(nan_rate));
         CHECK(!audio_driver_transport_configure(settings)
               && transport_control_calls == calls, "invalid rate parked worker");
         st->input = 48000;
         transport_fail_stage = true;
         CHECK(!audio_driver_transport_configure(settings) && !st->pipe_transport,
               "failed startup retained transport");
         transport_fail_stage = false;
         CHECK(audio_driver_transport_configure(settings) && st->pipe_transport
               && st->pipe_transport_rate == 48000 && st->pipe_transport_search == 3
               && (st->pipe_transport_follow & AUDIO_TRANSPORT_LOWPASS),
               "configured startup missing native automatic policy");
         audio_driver_pipeline_transport_release();
         settings->bools.audio_time_stretch_lowpass = false;
         CHECK(audio_driver_transport_configure(settings) && st->pipe_transport
               && !(st->pipe_transport_follow & AUDIO_TRANSPORT_LOWPASS),
               "disabled lowpass enabled effect");
         audio_driver_pipeline_transport_release();
         runloop_state_get_ptr()->flags = RUNLOOP_FLAG_SLOWMOTION;
         settings->floats.slowmotion_ratio = 8;
         audio_driver_publish_runloop();
         CHECK(audio_driver_transport_configure(settings) && !st->pipe_transport
               && st->pipe_transport_suspended, "unsupported configured startup not recoverable");
         runloop_state_get_ptr()->flags = 0;
         settings->floats.slowmotion_ratio = 1;
         audio_driver_publish_runloop();
         CHECK(audio_driver_transport_update_runloop(st) && st->pipe_transport
               && !st->pipe_transport_suspended, "configured startup recovery");
         audio_driver_pipeline_transport_release();
         settings->bools.audio_time_stretch = false;
         settings->bools.audio_time_stretch_lowpass = true;
         runloop_state_get_ptr()->flags = RUNLOOP_FLAG_SLOWMOTION;
         settings->floats.slowmotion_ratio = 8;
         audio_driver_publish_runloop();
         CHECK(audio_driver_transport_configure(settings) && st->pipe_transport_suspended,
               "LPF-only unsupported startup");
         settings->floats.slowmotion_ratio = 0.5f;
         audio_driver_publish_runloop();
         CHECK(audio_driver_transport_update_runloop(st) && st->pipe_transport
               && !st->pipe_transport_suspended
               && !(st->pipe_layouts.published_control & AUDIO_PIPELINE_STRETCH)
               && st->pipe_layouts.published_cutoff, "LPF-only recovery enabled stretch");
         settings->floats.slowmotion_ratio = 1;
         runloop_state_get_ptr()->flags = 0;
         audio_driver_publish_runloop();
         st->current_audio = saved;
         audio_driver_deinit_internal(true);
         CHECK(!st->pipe_transport && !st->pipe_transport_follow,
               "configured teardown retained transport");
      }
   settings->bools.audio_time_stretch = false;
   settings->bools.audio_time_stretch_lowpass = false;
   printf("native transport settings: 4 cases, %u failures\n", failures - before);
}

static void transport_owner_cases(void)
{
   extern unsigned transport_control_calls;
   audio_driver_state_t *st = &audio_driver_st;
   unsigned floating, wide, before = failures;
   for (floating = 0; floating < 2; floating++)
      for (wide = 0; wide < 2; wide++)
      {
         audio_driver_t wrapper;
         const audio_driver_t *saved;
         audio_pipeline_stretch_t *stage;
         struct audio_pipeline_stretch_block block;
         union { float f[17*11]; int16_t i[17*11]; } input;
         void *output;
         size_t bytes;
         unsigned calls, j;
         CHECK(pipe_up(floating, floating), "transport owner stand-up");
         if (!wide)
         {
            st->pipe_channels = 2;
            st->pipe_frame_bytes = 2 * (floating ? sizeof(float) : sizeof(int16_t));
         }
         CHECK(!st->pipe_transport && !st->pipe_transport_output, "default allocated transport");
         saved = st->current_audio; wrapper = *saved; wrapper.ident = "audio-thread";
         st->current_audio = &wrapper;
         calls = transport_control_calls;
         transport_fail_output = true;
         CHECK(!audio_driver_pipeline_transport_prepare(48000, 1), "output allocation failure accepted");
         transport_fail_output = false;
         CHECK(transport_control_calls == calls + 1, "prepare bypassed wrapper parking");
         CHECK(!st->pipe_transport && !st->pipe_transport_output, "failed prepare left state");
         CHECK(audio_driver_pipeline_transport_prepare(48000, 1), "owner prepare");
         stage = st->pipe_transport; output = st->pipe_transport_output;
         runloop_state_get_ptr()->flags = RUNLOOP_FLAG_SLOWMOTION;
         config_get_ptr()->floats.slowmotion_ratio = 8;
         audio_driver_publish_runloop();
         calls = transport_control_calls;
         CHECK(!audio_driver_pipeline_transport_prepare_runloop(48000, 1, true)
               && transport_control_calls == calls, "unsupported startup parked worker");
         runloop_state_get_ptr()->flags = RUNLOOP_FLAG_FASTMOTION;
         config_get_ptr()->bools.audio_fastforward_speedup = true;
         retro_atomic_store_release_int(&st->pipe_ff_mult_q16, 16384);
         audio_driver_publish_runloop();
         transport_fail_stage = true;
         CHECK(!audio_driver_pipeline_transport_prepare_runloop(48000, 1, true),
               "failed runloop startup accepted");
         transport_fail_stage = false;
         CHECK(st->pipe_transport == stage && st->pipe_transport_output == output
               && st->pipe_layouts.published_control == 65536
               && st->pipe_layouts.published_cutoff == 0,
               "failed runloop startup changed session");
         calls = transport_control_calls;
         CHECK(audio_driver_pipeline_transport_prepare_runloop(48000, 1, true)
               && transport_control_calls == calls + 1, "runloop startup parking");
         CHECK(st->pipe_layouts.current_control == (262144 | AUDIO_PIPELINE_STRETCH)
               && st->pipe_layouts.published_control == st->pipe_layouts.current_control
               && st->pipe_layouts.current_cutoff == 5400
               && st->pipe_layouts.published_cutoff == 5400
               && !retro_atomic_load_relaxed_size(&st->pipe_layouts.head),
               "startup did not seed coherent speed controls");
         CHECK(audio_pipeline_stretch_next(st->pipe_transport, 0, 1, &block)
               && !block.frames && audio_pipeline_stretch_needs_input(st->pipe_transport),
               "startup controls not ready before source");
         runloop_state_get_ptr()->flags = 0;
         config_get_ptr()->bools.audio_fastforward_speedup = false;
         config_get_ptr()->floats.slowmotion_ratio = 1;
         audio_driver_publish_runloop();
         CHECK(audio_driver_pipeline_transport_prepare_runloop(48000, 1, false)
               && st->pipe_layouts.current_control == 65536
               && st->pipe_layouts.current_cutoff == 0, "normal startup was not dry");
         stage = st->pipe_transport; output = st->pipe_transport_output;
         CHECK(stage && output && !((uintptr_t)output % 64), "owner storage/alignment");
         wrapper.wait_writable = NULL;
         CHECK(!audio_driver_pipeline_transport_prepare(48000, 1)
               && st->pipe_transport == stage, "prepared scheduler without device pacing");
         wrapper.wait_writable = saved->wait_writable;
         CHECK(!audio_driver_pipeline_transport_prepare(7999, 1), "invalid rate accepted");
         transport_allocations = transport_frees = 0; transport_track = true;
         transport_fail_stage = true;
         CHECK(!audio_driver_pipeline_transport_prepare(48000, 1), "stage allocation failure accepted");
         transport_fail_stage = false; transport_track = false;
         CHECK(transport_allocations == 1 && transport_frees == 1, "partial prepare leaked output");
         CHECK(st->pipe_transport == stage && st->pipe_transport_output == output,
               "failed replacement destroyed session");
         memset(&input, 0, sizeof(input));
         bytes = st->pipe_frame_bytes;
         CHECK(retro_spsc_write(&st->pipe_ring, &input, bytes) == bytes, "queued owner source");
         CHECK(!audio_driver_pipeline_transport_prepare(48000, 1), "replaced queued owner");
         CHECK(!audio_driver_pipeline_transport_prepare_runloop(48000, 1, true)
               && st->pipe_transport == stage, "runloop startup replaced queued owner");
         audio_driver_set_core_float(!floating);
         CHECK(st->pipe_float == (floating != 0) && st->pipe_transport == stage,
               "queued format changed session");
         CHECK(retro_spsc_skip(&st->pipe_ring, bytes) == bytes, "owner test source release");
         CHECK(audio_pipeline_layout_publish_cutoff(&st->pipe_layouts,
                  retro_atomic_load_relaxed_size(&st->pipe_ring.head), 2000), "owner pending control");
         st->pipe_pending = (const uint8_t*)output; st->pipe_pending_bytes = bytes;
         CHECK(!audio_driver_pipeline_transport_prepare(48000, 1), "replaced pending device output");
         CHECK(!audio_driver_pipeline_transport_prepare_runloop(48000, 1, true)
               && st->pipe_transport == stage, "runloop startup replaced pending output");
         calls = transport_control_calls;
         audio_driver_set_core_float(!floating);
         CHECK(transport_control_calls == calls + 1, "native rebind bypassed parking");
         CHECK(st->pipe_transport && st->pipe_transport != stage && st->pipe_float == !floating,
               "empty native session did not rebind");
         CHECK(!st->pipe_pending_bytes && !st->pipe_pending, "rebind kept stale device output");
         CHECK(st->pipe_frame_bytes == st->pipe_channels * (!floating ? sizeof(float) : sizeof(int16_t)),
               "rebind native stride");
         CHECK(!retro_atomic_load_relaxed_size(&st->pipe_ring.head)
               && !retro_atomic_load_relaxed_size(&st->pipe_ring.tail)
               && st->pipe_layouts.current_cutoff == 2000
               && st->pipe_layouts.published_cutoff == 2000, "rebind lost alignment/control");
         for (j = 0; j < 17 * st->pipe_channels; j++)
            if (!floating) input.f[j] = 0.25f; else input.i[j] = 8192;
         CHECK(audio_pipeline_layout_publish_cutoff(&st->pipe_layouts,
                  retro_atomic_load_relaxed_size(&st->pipe_ring.head), 1000), "owner cutoff");
         CHECK(retro_spsc_write_frames(&st->pipe_ring, &input, 17, st->pipe_frame_bytes) == 17,
               "owner native publish");
         CHECK(audio_pipeline_stretch_next(st->pipe_transport, 17, 17, &block), "rebound native processing");
         CHECK(block.input_used == 17 && block.frames == 17 && block.data == st->pipe_transport_output,
               "rebound filter did not use owned storage");
         CHECK(!memcmp(block.data, &input, 17 * st->pipe_frame_bytes), "rebound native samples differ");
         st->pipe_pending = (const uint8_t*)block.data;
         st->pipe_pending_bytes = 17 * st->pipe_frame_bytes;
         wrapper.stop = dev_stop_fail;
         CHECK(!audio_driver_stop(), "failed stop accepted");
         CHECK(st->pipe_pending == block.data && st->pipe_pending_bytes == 17 * st->pipe_frame_bytes,
               "failed stop cancelled device output");
         CHECK(audio_pipeline_stretch_next(st->pipe_transport, 0, 17, &block)
               && block.frames == 17, "failed stop cancelled transport output");
         for (j = 0; j < 17 * st->pipe_channels; j++)
            if (!floating) input.f[j] = -0.25f; else input.i[j] = -8192;
         CHECK(retro_spsc_write_frames(&st->pipe_ring, &input, 17, st->pipe_frame_bytes) == 17,
               "stop queued restart source");
         wrapper.stop = dev_stop;
         transport_allocations = transport_frees = 0; transport_track = true;
         CHECK(audio_driver_stop(), "owned transport stop");
         transport_track = false;
         CHECK(!transport_allocations && !transport_frees, "stop replaced transport storage");
         CHECK(!st->pipe_pending && !st->pipe_pending_bytes, "stop retained device output");
         CHECK(retro_spsc_read_avail(&st->pipe_ring) == 17 * st->pipe_frame_bytes,
               "stop discarded queued source");
         CHECK(audio_pipeline_stretch_next(st->pipe_transport, 0, 17, &block)
               && !block.frames, "stop retained filtered output");
         CHECK(audio_pipeline_stretch_next(st->pipe_transport, 17, 17, &block)
               && block.frames == 17 && block.input_used == 17,
               "stopped transport cannot resume queued source");
         CHECK(!memcmp(block.data, &input, 17 * st->pipe_frame_bytes),
               "stop retained filter history");
         st->pipe_pending = (const uint8_t*)block.data;
         st->pipe_pending_bytes = 17 * st->pipe_frame_bytes;
         transport_fail_stage = true;
         audio_driver_set_core_float(floating);
         transport_fail_stage = false;
         CHECK(!st->pipe_transport && !st->pipe_transport_output && !st->pipe_pending_bytes,
               "failed native rebuild retained stale session");
         CHECK(st->pipe_float == (floating != 0), "failed rebuild forced old sample format");
         CHECK(audio_driver_pipeline_transport_prepare(48000, 1), "owner recovery");
         calls = transport_control_calls;
         audio_driver_pipeline_transport_release();
         CHECK(transport_control_calls == calls + 1 && !st->pipe_transport
               && !st->pipe_transport_output && !st->pipe_transport_rate, "owner release");
         CHECK(audio_driver_pipeline_transport_prepare(48000, 1), "owner teardown preparation");
         st->current_audio = saved;
         audio_driver_deinit_internal(true);
         CHECK(!st->pipe_transport && !st->pipe_transport_output && !st->pipe_transport_rate,
               "driver teardown leaked transport");
      }
   printf("native transport ownership: 4 cases, %u failures\n", failures - before);
}

static void transport_scheduler_cases(void)
{
   audio_driver_state_t *st = &audio_driver_st;
   unsigned floating, wide, before = failures;
   bool sync = config_get_ptr()->bools.audio_sync;
   for (floating = 0; floating < 2; floating++)
      for (wide = 0; wide < 2; wide++)
      {
         union { float f[34*11]; int16_t i[34*11]; } input;
         unsigned i;
         size_t tail;
         CHECK(pipe_up(floating, floating), "scheduler stand-up");
         if (!wide)
         {
            st->pipe_channels = 2;
            st->pipe_frame_bytes = 2 * (floating ? sizeof(float) : sizeof(int16_t));
         }
         CHECK(audio_driver_pipeline_transport_prepare(48000, 1), "scheduler prepare");
         scripted_threaded.wait_writable = short_device_wait;
         short_wait_calls = 0;
         retro_atomic_store_release_int(&st->pipe_wake, 1);
         CHECK(audio_driver_callback() && !retro_atomic_load_acquire_int(&st->pipe_wake) && !short_wait_calls,
               "idle scheduler paced an empty pass before wrapper wake");
         st->pipe_pass_frames = 17;
         st->buffer_size = 0;
         st->pipe_priming = true;
         retro_atomic_store_release_int(&st->pipe_wake, 1);
         CHECK(audio_driver_callback() && st->pipe_priming && !retro_atomic_load_acquire_int(&st->pipe_wake),
               "scheduler ignored startup wake");
         for (i = 0; i < AUDIO_PIPELINE_LAYOUT_CAPACITY; i++)
            CHECK(audio_driver_pipeline_transport_request(65536, false, true, 1000),
                  "scheduler startup metadata");
         CHECK(audio_driver_callback() && !st->pipe_priming
               && st->pipe_layouts.reset_serial == AUDIO_PIPELINE_LAYOUT_CAPACITY,
               "scheduler metadata-only priming deadlock");
         memset(&input, 0, sizeof(input));
         CHECK(retro_spsc_write_frames(&st->pipe_ring, &input, 34, st->pipe_frame_bytes) == 34,
               "scheduler source");
         tail = retro_atomic_load_relaxed_size(&st->pipe_ring.tail);
         scripted_threaded.wait_writable = short_device_wait;
         short_no_room = true;
         CHECK(audio_driver_callback() && retro_atomic_load_relaxed_size(&st->pipe_ring.tail) == tail,
               "scheduler consumed source without device room");
         short_no_room = false;
         scripted_threaded.write = short_device_write;
         short_zero = true; short_fail = false;
         CHECK(audio_driver_callback() && st->pipe_pending_bytes,
               "scheduler did not retain short device output");
         retro_atomic_store_release_int(&st->pipe_stalled, 1);
         snap_pause(true);
         cap_frames = 0;
         CHECK(audio_driver_callback() && !st->pipe_pending_bytes
               && !retro_spsc_read_avail(&st->pipe_ring) && !cap_frames,
               "scheduler pause replayed output or retained source");
         CHECK(retro_atomic_load_acquire_int(&st->pipe_stalled),
               "paused discard incorrectly reported device progress");
         snap_pause(false);
         CHECK(retro_spsc_write_frames(&st->pipe_ring, &input, 34, st->pipe_frame_bytes) == 34,
               "scheduler late source");
         scripted_threaded.underruns = epoch_underrun;
         st->pipe_underruns_seen = 0;
         config_get_ptr()->bools.audio_sync = false;
         snap_pause(false);
         short_zero = false;
         CHECK(audio_driver_callback() && st->pipe_underruns_seen == 1
               && !retro_spsc_read_avail(&st->pipe_ring) && !cap_frames,
               "scheduler underrun replayed late source");
         CHECK(retro_spsc_write_frames(&st->pipe_ring, &input, 17, st->pipe_frame_bytes) == 17,
               "scheduler resumed source");
         CHECK(audio_driver_callback() && !retro_spsc_read_avail(&st->pipe_ring) && cap_frames,
               "scheduler did not resume after underrun");
         audio_driver_deinit_internal(true);
      }
   config_get_ptr()->bools.audio_sync = sync;
   snap_pause(false);
   printf("native transport scheduler: 4 cases, %u failures\n", failures - before);
}

static void transport_request_cases(void)
{
   audio_driver_state_t *st = &audio_driver_st;
   unsigned floating, wide, before = failures;
   for (floating = 0; floating < 2; floating++)
      for (wide = 0; wide < 2; wide++)
      {
         union { float f[11]; int16_t i[11]; } input;
         audio_pipeline_layout_t *q;
         struct audio_pipeline_stretch_block block;
         size_t position, head;
         audio_pipeline_stretch_t *retained;
         unsigned gen, i;
         CHECK(pipe_up(floating, floating), "request stand-up");
         if (!wide)
         {
            st->pipe_channels = 2;
            st->pipe_frame_bytes = 2 * (floating ? sizeof(float) : sizeof(int16_t));
         }
         q = &st->pipe_layouts;
         CHECK(!audio_driver_pipeline_transport_request(65536, true, false, 1000),
               "request without owned stage");
         CHECK(!audio_driver_pipeline_transport_request_speed(131072, true, false, true),
               "speed request without owned stage");
         CHECK(!audio_driver_pipeline_transport_request_runloop(false, true),
               "runloop request without owned stage");
         CHECK(audio_driver_pipeline_transport_prepare(48000, 1), "request prepare");
         memset(&input, 0, sizeof(input));
         CHECK(retro_spsc_write_frames(&st->pipe_ring, &input, 1, st->pipe_frame_bytes) == 1,
               "request preceding source");
         position = retro_atomic_load_relaxed_size(&st->pipe_ring.head);
         gen = retro_atomic_load_acquire_int(&st->pipe_data_gen);
         CHECK(audio_driver_pipeline_transport_request(98304, true, true, 1000), "request publish");
         CHECK(retro_atomic_load_acquire_int(&st->pipe_data_gen) == gen + 1 && retro_atomic_load_relaxed_size(&q->head) == 1,
               "request did not wake consumer");
         CHECK(q->events[0].position == position && q->events[0].layout == q->published_layout
               && q->events[0].control == (98304 | AUDIO_PIPELINE_STRETCH | AUDIO_PIPELINE_RESET)
               && q->events[0].cutoff == 1000, "request split controls or used wrong boundary");
         CHECK(audio_pipeline_stretch_next(st->pipe_transport, 1, 1, &block)
               && block.frames == 1 && block.passthrough, "request affected preceding audio");
         CHECK(audio_pipeline_stretch_consume(st->pipe_transport, 1), "request preceding release");
         CHECK(audio_pipeline_stretch_next(st->pipe_transport, 0, 1, &block)
               && q->current_control == (98304 | AUDIO_PIPELINE_STRETCH)
               && q->current_cutoff == 1000 && q->reset_serial == 1,
               "request boundary not retired coherently");
         head = retro_atomic_load_relaxed_size(&q->head);
         gen = retro_atomic_load_acquire_int(&st->pipe_data_gen);
         CHECK(audio_driver_pipeline_transport_request(98304, true, false, 1000), "unchanged request");
         CHECK(!audio_driver_pipeline_transport_request(16383, true, true, 2000)
               && !audio_driver_pipeline_transport_request(2097153, true, true, 2000),
               "invalid request tempo accepted");
         CHECK(retro_atomic_load_relaxed_size(&q->head) == head && retro_atomic_load_acquire_int(&st->pipe_data_gen) == gen
               && q->published_cutoff == 1000 && q->published_control == (98304 | AUDIO_PIPELINE_STRETCH),
               "invalid/unchanged request mutated controls or woke consumer");
         for (i = 0; i < AUDIO_PIPELINE_LAYOUT_CAPACITY; i++)
            CHECK(audio_driver_pipeline_transport_request(98304, true, true, 1000), "request pressure fill");
         gen = retro_atomic_load_acquire_int(&st->pipe_data_gen);
         head = retro_atomic_load_relaxed_size(&q->head);
         CHECK(!audio_driver_pipeline_transport_request(131072, true, true, 2000), "full request accepted");
         CHECK(!audio_driver_pipeline_transport_request_speed(131072, true, true, true),
               "full speed request accepted");
         CHECK(!audio_driver_pipeline_transport_request_runloop(true, true),
               "full runloop request accepted");
         CHECK(retro_atomic_load_relaxed_size(&q->head) == head && retro_atomic_load_acquire_int(&st->pipe_data_gen) == gen
               && q->published_cutoff == 1000 && q->published_control == (98304 | AUDIO_PIPELINE_STRETCH),
               "full request partially published");
         CHECK(audio_driver_pipeline_transport_request(98304, true, false, 1000), "full unchanged request");
         CHECK(audio_pipeline_stretch_next(st->pipe_transport, 0, 1, &block)
               && q->reset_serial == 1 + AUDIO_PIPELINE_LAYOUT_CAPACITY, "request pressure retirement");
         CHECK(audio_driver_pipeline_transport_request(0, false, false, 2000), "inactive request retry");
         CHECK(audio_pipeline_stretch_next(st->pipe_transport, 0, 1, &block)
               && q->current_control == 65536 && q->current_cutoff == 2000,
               "inactive request did not preserve filter independence");
         CHECK(audio_driver_pipeline_transport_request_speed(131072, false, false, true),
               "speed request retry");
         CHECK(audio_pipeline_stretch_next(st->pipe_transport, 0, 1, &block)
               && q->current_control == 65536 && q->current_cutoff == 10800,
               "speed cutoff depended on WSOLA activation");
         CHECK(audio_driver_pipeline_transport_request_speed(131072, true, false, false),
               "speed request disable LPF");
         CHECK(audio_pipeline_stretch_next(st->pipe_transport, 0, 1, &block)
               && q->current_control == (131072 | AUDIO_PIPELINE_STRETCH)
               && q->current_cutoff == 0, "disabled speed LPF was not dry");
         {
            static const struct {
               uint32_t flags, tempo, cutoff;
               float slow;
               int mult;
               bool speedup;
            } modes[] = {
               {0, 65536, 0, 2, 16384, true},
               {RUNLOOP_FLAG_SLOWMOTION, 32768, 0, 2, 16384, true},
               {RUNLOOP_FLAG_SLOWMOTION, 16384, 0, 4, 16384, true},
               {RUNLOOP_FLAG_FASTMOTION, 262144, 5400, 2, 16384, true},
               {RUNLOOP_FLAG_FASTMOTION, 2097152, 675, 2, 2048, true},
               {RUNLOOP_FLAG_FASTMOTION, 262144, 5400, 2, 16384, false},
               {RUNLOOP_FLAG_FASTMOTION | RUNLOOP_FLAG_SLOWMOTION,
                  131072, 10800, 2, 16384, true},
               {RUNLOOP_FLAG_PAUSED | RUNLOOP_FLAG_FASTMOTION,
                  65536, 0, 2, 0, true}
            };
            unsigned m;
            for (m = 0; m < sizeof(modes) / sizeof(modes[0]); m++)
            {
               runloop_state_get_ptr()->flags = modes[m].flags;
               config_get_ptr()->floats.slowmotion_ratio = modes[m].slow;
               config_get_ptr()->bools.audio_fastforward_speedup = modes[m].speedup;
               retro_atomic_store_release_int(&st->pipe_ff_mult_q16, modes[m].mult);
               audio_driver_publish_runloop();
               CHECK(audio_driver_pipeline_transport_request_runloop(false, true),
                     "runloop request publish");
               CHECK(audio_pipeline_stretch_next(st->pipe_transport, 0, 1, &block)
                     && q->current_control == (modes[m].tempo == 65536 ? 65536
                        : modes[m].tempo | AUDIO_PIPELINE_STRETCH)
                     && q->current_cutoff == modes[m].cutoff,
                     "runloop request speed/cutoff composition");
            }
            runloop_state_get_ptr()->flags = RUNLOOP_FLAG_SLOWMOTION;
            audio_driver_publish_runloop();
            head = retro_atomic_load_relaxed_size(&q->head);
            gen = retro_atomic_load_acquire_int(&st->pipe_data_gen);
            config_get_ptr()->floats.slowmotion_ratio = 8;
            CHECK(!audio_driver_pipeline_transport_request_runloop(true, true),
                  "unsupported slow motion accepted");
            config_get_ptr()->floats.slowmotion_ratio = NAN;
            CHECK(!audio_driver_pipeline_transport_request_runloop(true, true),
                  "nonfinite slow motion accepted");
            runloop_state_get_ptr()->flags = RUNLOOP_FLAG_FASTMOTION;
            retro_atomic_store_release_int(&st->pipe_ff_mult_q16, 0);
            audio_driver_publish_runloop();
            CHECK(!audio_driver_pipeline_transport_request_runloop(true, true),
                  "unseeded fast-forward accepted");
            CHECK(retro_atomic_load_relaxed_size(&q->head) == head
                  && retro_atomic_load_acquire_int(&st->pipe_data_gen) == gen
                  && q->published_control == 65536 && q->published_cutoff == 0,
                  "rejected runloop request changed queued state");
            runloop_state_get_ptr()->flags = 0;
            config_get_ptr()->floats.slowmotion_ratio = 1;
            config_get_ptr()->bools.audio_fastforward_speedup = false;
            audio_driver_publish_runloop();
         }
         CHECK(audio_driver_pipeline_transport_start_runloop(48000, 1, true),
               "automatic producer start");
         for (i = 0; i < AUDIO_PIPELINE_LAYOUT_CAPACITY; i++)
            CHECK(audio_driver_pipeline_transport_request(65536, false, true, 0),
                  "automatic pressure fill");
         runloop_state_get_ptr()->flags = RUNLOOP_FLAG_FASTMOTION;
         config_get_ptr()->bools.audio_fastforward_speedup = true;
         retro_atomic_store_release_int(&st->pipe_ff_mult_q16, 16384);
         audio_driver_publish_runloop();
         audio_driver_submit_width(st, 1.0f, &input, st->pipe_channels,
               floating, false, true, true, st->pipe_channels);
         CHECK(!retro_spsc_read_avail(&st->pipe_ring)
               && (st->pipe_transport_follow & AUDIO_TRANSPORT_UPDATE)
               && q->published_control == 65536 && q->published_cutoff == 0,
               "automatic pressure published source or partial controls");
         CHECK(audio_pipeline_stretch_next(st->pipe_transport, 0, 1, &block),
               "automatic pressure retirement");
         audio_driver_submit_width(st, 1.0f, &input, st->pipe_channels,
               floating, false, true, true, st->pipe_channels);
         CHECK(retro_spsc_read_avail(&st->pipe_ring) == st->pipe_frame_bytes
               && !(st->pipe_transport_follow & AUDIO_TRANSPORT_UPDATE)
               && q->published_control == (262144 | AUDIO_PIPELINE_STRETCH)
               && q->published_cutoff == 5400, "automatic producer retry");
         head = retro_atomic_load_relaxed_size(&q->head);
         config_get_ptr()->bools.audio_fastforward_speedup = false;
         audio_driver_publish_runloop();
         audio_driver_submit_width(st, 1.0f, &input, st->pipe_channels,
               floating, false, false, true, st->pipe_channels);
         CHECK(retro_atomic_load_relaxed_size(&q->head) == head,
               "automatic producer updated twice in one frame");
         audio_driver_frame_end();
         runloop_state_get_ptr()->flags = RUNLOOP_FLAG_SLOWMOTION;
         config_get_ptr()->floats.slowmotion_ratio = 8;
         audio_driver_publish_runloop();
         position = retro_spsc_read_avail(&st->pipe_ring);
         retained = st->pipe_transport;
         transport_allocations = transport_frees = 0; transport_track = true;
         audio_driver_submit_width(st, 8.0f, &input, st->pipe_channels,
               floating, true, false, true, st->pipe_channels);
         CHECK(!st->pipe_transport && st->pipe_transport_follow
               && st->pipe_transport_suspended == retained
               && retro_spsc_read_avail(&st->pipe_ring) == position + st->pipe_frame_bytes,
               "automatic fallback lost queued source or stayed active");
         runloop_state_get_ptr()->flags = 0;
         config_get_ptr()->floats.slowmotion_ratio = 1;
         audio_driver_publish_runloop();
         CHECK(!audio_driver_pipeline_transport_start_runloop(48000, 1, true),
               "automatic restart accepted queued source");
         audio_driver_frame_end();
         audio_driver_submit_width(st, 1.0f, &input, st->pipe_channels,
               floating, false, false, true, st->pipe_channels);
         CHECK(!st->pipe_transport && st->pipe_transport_suspended == retained
               && retro_spsc_read_avail(&st->pipe_ring) == position + 2 * st->pipe_frame_bytes,
               "recovery replaced queued legacy source");
         retro_spsc_skip(&st->pipe_ring, retro_spsc_read_avail(&st->pipe_ring));
         st->pipe_pending = (const uint8_t*)&input; st->pipe_pending_bytes = 1;
         audio_driver_frame_end();
         CHECK(audio_driver_transport_update_runloop(st) && !st->pipe_transport
               && st->pipe_pending_bytes == 1, "recovery cancelled pending device output");
         st->pipe_pending = NULL; st->pipe_pending_bytes = 0;
         audio_driver_frame_end();
         CHECK(audio_driver_transport_update_runloop(st) && st->pipe_transport == retained
               && !st->pipe_transport_suspended && q->current_control == 65536
               && q->current_cutoff == 0, "automatic drained recovery");
         transport_track = false;
         CHECK(!transport_allocations && !transport_frees, "fallback/recovery replaced storage");
         audio_driver_set_core_float(!floating);
         CHECK(st->pipe_transport && st->pipe_transport_follow
               && st->pipe_float == !floating, "native rebind lost automatic mode");
         CHECK(audio_driver_transport_recover(false, 65536), "suspend before native rebind");
         audio_driver_set_core_float(floating);
         CHECK(!st->pipe_transport && st->pipe_transport_suspended
               && st->pipe_float == floating, "suspended rebind lost native format");
         CHECK(audio_driver_transport_update_runloop(st) && st->pipe_transport
               && !st->pipe_transport_suspended, "recovery after native rebind");
         CHECK(audio_driver_transport_recover(false, 65536), "suspend before release");
         audio_driver_pipeline_transport_release();
         CHECK(!st->pipe_transport_suspended && !st->pipe_transport_output
               && !st->pipe_transport_follow, "release retained suspended storage");
         CHECK(audio_driver_pipeline_transport_start_runloop(48000, 1, true)
               && audio_driver_transport_recover(false, 65536), "suspended teardown setup");
         audio_driver_deinit_internal(true);
         CHECK(!st->pipe_transport_suspended && !st->pipe_transport_output,
               "teardown retained suspended storage");
      }
   printf("native transport request: 4 cases, %u failures\n", failures - before);
}

static void transport_discard_cases(void)
{
   audio_driver_state_t *st = &audio_driver_st;
   unsigned floating, wide, wet, before = failures;
   for (floating = 0; floating < 2; floating++)
      for (wide = 0; wide < 2; wide++)
         for (wet = 0; wet < 2; wet++)
         {
            union { float f[34*11]; int16_t i[34*11]; } input;
            struct audio_pipeline_stretch_block block;
            size_t queued, tail;
            uint32_t serial;
            unsigned gen, j;
            const void *pending;
            CHECK(pipe_up(floating, floating), "discard stand-up");
            if (!wide)
            {
               st->pipe_channels = 2;
               st->pipe_frame_bytes = 2 * (floating ? sizeof(float) : sizeof(int16_t));
            }
            CHECK(!audio_driver_pipeline_transport_discard(0), "discard absent transport");
            CHECK(audio_driver_pipeline_transport_prepare(48000, 1), "discard prepare");
            CHECK(audio_pipeline_stretch_needs_input(st->pipe_transport), "fresh stage has retained work");
            if (wet)
               CHECK(audio_pipeline_layout_publish_cutoff(&st->pipe_layouts, 0, 1000),
                     "discard cutoff");
            for (j = 0; j < 34 * st->pipe_channels; j++)
               if (floating) input.f[j] = 0.25f; else input.i[j] = 8192;
            CHECK(retro_spsc_write_frames(&st->pipe_ring, &input, 34, st->pipe_frame_bytes) == 34,
                  "discard source");
            CHECK(audio_pipeline_stretch_next(st->pipe_transport, 17, 17, &block)
                  && block.frames == 17, "discard retained output");
            CHECK(!audio_pipeline_stretch_needs_input(st->pipe_transport), "retained output may sleep");
            serial = block.reset_serial;
            pending = block.data;
            st->pipe_pending = (const uint8_t*)pending;
            st->pipe_pending_bytes = 17 * st->pipe_frame_bytes;
            queued = retro_spsc_read_avail(&st->pipe_ring) / st->pipe_frame_bytes;
            tail = retro_atomic_load_relaxed_size(&st->pipe_ring.tail);
            gen = retro_atomic_load_acquire_int(&st->pipe_gen);
            retro_atomic_store_release_int(&st->pipe_stalled, 1);
            CHECK(!audio_driver_pipeline_transport_discard(queued + 1), "oversized discard accepted");
            CHECK(!audio_driver_pipeline_transport_discard((size_t)-1), "overflow discard accepted");
            CHECK(retro_atomic_load_relaxed_size(&st->pipe_ring.tail) == tail
                  && retro_atomic_load_acquire_int(&st->pipe_gen) == gen
                  && retro_atomic_load_acquire_int(&st->pipe_stalled)
                  && st->pipe_pending == pending && st->pipe_pending_bytes == 17 * st->pipe_frame_bytes,
                  "failed discard changed driver state");
            CHECK(audio_pipeline_stretch_next(st->pipe_transport, 0, 17, &block)
                  && block.frames == 17 && block.reset_serial == serial,
                  "failed discard changed transport");
            transport_allocations = transport_frees = 0; transport_track = true;
            CHECK(audio_driver_pipeline_transport_discard(1), "exact discard failed");
            CHECK(audio_pipeline_stretch_needs_input(st->pipe_transport), "discard retained synthesis");
            transport_track = false;
            CHECK(!transport_allocations && !transport_frees, "discard replaced storage");
            CHECK(!st->pipe_pending && !st->pipe_pending_bytes
                  && retro_atomic_load_acquire_int(&st->pipe_gen) == gen + 1
                  && !retro_atomic_load_acquire_int(&st->pipe_stalled),
                  "discard did not cancel output/notify progress");
            CHECK(retro_spsc_read_avail(&st->pipe_ring) == (queued - 1) * st->pipe_frame_bytes,
                  "discard released wrong source count");
            CHECK(audio_pipeline_stretch_next(st->pipe_transport, 0, 17, &block)
                  && !block.frames && block.reset_serial == serial + 1,
                  "discard retained output or epoch");
            gen = retro_atomic_load_acquire_int(&st->pipe_gen);
            CHECK(audio_driver_pipeline_transport_discard(0), "zero discard failed");
            CHECK(retro_atomic_load_acquire_int(&st->pipe_gen) == gen && retro_spsc_read_avail(&st->pipe_ring)
                  == (queued - 1) * st->pipe_frame_bytes, "zero discard released source");
            CHECK(audio_driver_pipeline_transport_discard(queued - 1), "remaining discard failed");
            CHECK(!retro_spsc_read_avail(&st->pipe_ring), "discard did not empty source");
            for (j = 0; j < 17 * st->pipe_channels; j++)
               if (floating) input.f[j] = -0.25f; else input.i[j] = -8192;
            CHECK(retro_spsc_write_frames(&st->pipe_ring, &input, 17, st->pipe_frame_bytes) == 17,
                  "discard restart source");
            CHECK(audio_pipeline_stretch_next(st->pipe_transport, 17, 17, &block)
                  && block.frames == 17, "discard restart output");
            CHECK(!memcmp(block.data, &input, 17 * st->pipe_frame_bytes), "discard retained history");
            audio_driver_deinit_internal(true);
         }
   printf("native transport discard: 8 cases, %u failures\n", failures - before);
}

static void native_render_cases(void)
{
   unsigned floating, wide, hq, before = failures;
   for (floating = 0; floating < 2; floating++)
      for (wide = 0; wide < 2; wide++)
         for (hq = 0; hq < 2; hq++)
         {
            size_t dry = native_render_case(floating, wide, hq, false);
            size_t wet = native_render_case(floating, wide, hq, true);
            CHECK(dry == wet, "LPF changed transport duration");
         }
   printf("native WSOLA frontend render: 48 runs, %u failures\n", failures - before);
}

static void canonical_reserve_cases(void)
{
   static union { float f[4097 * 8]; int16_t i[4097 * 8]; } input;
   static const size_t batches[] = {1, 17, 1024, 4097};
   audio_driver_state_t *st = &audio_driver_st;
   unsigned floating, wide, n, before = failures;
   for (floating = 0; floating < 2; floating++)
      for (wide = 0; wide < 2; wide++)
      {
         size_t frames = 0;
         CHECK(pipe_up(floating, floating), "canonical direct stand-up");
         canonical_reallocations = 0; canonical_track = true;
         canonical_fail = true;
         for (n = 0; n < ARRAY_SIZE(batches); n++)
         {
            CHECK(audio_driver_multi_pipe(st, &input, batches[n], wide ? 8 : 6,
                  wide ? AUDIO_LAYOUT_7POINT1 : AUDIO_LAYOUT_5POINT1, floating),
                  "canonical reserved publish");
            frames += batches[n];
         }
         canonical_track = canonical_fail = false;
         CHECK(!canonical_reallocations, "canonical direct publish allocated staging");
         CHECK(retro_spsc_read_avail(&st->pipe_ring) == frames * st->pipe_frame_bytes,
               "reserved callback lost source frames");
         audio_driver_deinit_internal(true);
      }
   printf("canonical allocation-free publish: 4 cases, %u failures\n", failures - before);
}

#include "transport_quality.h"

static void inline_transport_cases(void)
{
   static const float durations[] = {4.0f, 2.0f, 1.0f, 0.5f, 0.03125f};
   union { float f[257 * 2]; int16_t i[257 * 2]; } input;
   unsigned floating, speed, before = failures;
   audio_driver_state_t *st = &audio_driver_st;
   settings_t *settings = config_get_ptr();
   for (floating = 0; floating < 2; floating++)
      for (speed = 0; speed < 5; speed++)
      {
         size_t submitted = 0, f, count;
         double duration = durations[speed], expected;
         size_t total = (size_t)(16384 * (duration < 1 ? 1 / duration : 1));
         struct audio_inline_transport *saved;
         CHECK(up(floating, AUDIO_LAYOUT_STEREO, floating), "inline stand-up");
         if (!floating)
         {
            settings->bools.audio_fastpath_s16 = true;
            st->resampler_data_int16 = audio_driver_int16_resampler_new(st);
            st->resampler_int16_process = sinc_resampler_int16_process;
            st->resampler_int16_free = sinc_resampler_int16_free;
            st->resampler_int16_reset = sinc_resampler_int16_reset;
         }
         snap_pause(false);
         settings->bools.audio_time_stretch = false;
         settings->bools.audio_time_stretch_lowpass = false;
         CHECK(audio_driver_transport_configure(settings) && !st->inline_transport,
               "disabled inline allocated state");
         settings->bools.audio_time_stretch = true;
         settings->bools.audio_time_stretch_lowpass = false;
         transport_fail_output = true;
         CHECK(!audio_driver_transport_configure(settings) && !st->inline_transport,
               "failed inline allocation retained state");
         transport_fail_output = false;
         CHECK(audio_driver_transport_configure(settings) && st->inline_transport,
               "inline configuration did not activate");
         saved = st->inline_transport;
         if (!saved) return;
         settings->floats.slowmotion_ratio = durations[speed];
         /* Controlled duration exercises both tempo directions independent of wall time. */
         runloop_state_get_ptr()->flags = RUNLOOP_FLAG_SLOWMOTION;
         audio_driver_publish_runloop();
         while (submitted < total)
         {
            size_t n = total - submitted;
            if (n > 257) n = 257;
            for (f = 0; f < n; f++)
            {
               int16_t v = (int16_t)(12000 * sin(2 * M_PI * 440 * (submitted + f) / 44100.0));
               if (floating)
               {
                  input.f[2 * f] = v / 32768.0f;
                  input.f[2 * f + 1] = -v / 32768.0f;
               }
               else { input.i[2 * f] = v; input.i[2 * f + 1] = -v; }
            }
            CHECK((floating ? audio_driver_sample_batch_float(input.f, n)
                     : audio_driver_sample_batch(input.i, n)) == n, "inline source accounting");
            CHECK(!audio_stretch_stream_peek(saved->stream, &count) && !count,
                  "inline retained borrowed callback storage");
            submitted += n;
         }
         expected = total * duration * st->src_ratio_orig;
         CHECK(fabs((double)cap_frames - expected) < 1024 * (1 + duration) * st->src_ratio_orig,
               "inline duration %.5f: %u expected %.1f", duration, (unsigned)cap_frames, expected);
         CHECK(cap_frames > 8192, "inline capture too short");
         if (cap_frames > 8192)
         {
            double own = tone_energy(cap + 2048, cap_frames - 2048, 2, 0, 440);
            CHECK(own > 0.01 && own > 20 * tone_energy(cap + 2048, cap_frames - 2048, 2, 0, 660),
                  "inline pitch changed at duration %.5f: %.6f", duration, own);
         }
         CHECK(st->stat_core_is_float == floating && st->stat_frontend_is_float == floating,
               "inline native lane changed");
         CHECK(audio_driver_stop() && audio_stretch_stream_quiescent(saved->stream), "inline stop retained tail");
         CHECK(audio_driver_start(false), "inline restart");
         CHECK(!audio_driver_inline_flush(st, 8.0f, &input, 34, floating, true, false)
               && saved->bypassed, "unsupported inline duration did not fall back");
         CHECK(audio_driver_inline_flush(st, 1.0f, &input, 34, floating, false, false)
               && !saved->bypassed && st->inline_transport == saved, "inline fallback did not recover");
         st->core_layout = AUDIO_LAYOUT_5POINT1;
         st->extra.pending = true;
         CHECK(!audio_driver_inline_flush(st, 1.0f, &input, 34, floating, false, false), "wide inline did not fall back");
         CHECK(st->extra.pending, "inline fallback canceled prepared extra channels");
         st->extra.pending = false;
         st->core_layout = AUDIO_LAYOUT_STEREO;
         CHECK(!audio_driver_inline_flush(st, 1.0f, &input, 34, !floating, false, false), "format mismatch did not fall back");
         settings->bools.audio_fastforward_speedup = true;
         settings->bools.audio_time_stretch_lowpass = true;
         audio_driver_publish_runloop();
         CHECK(audio_driver_inline_flush(st, 0.5f, &input, 34, floating, true, false)
               && !audio_speed_lpf_quiescent(&saved->lpf), "inline speed LPF inactive");
         st->last_flush_time = 0;
         audio_driver_inline_flush(st, 1.0f, &input, 34, floating, false, true);
         CHECK(st->last_flush_time > 0, "inline output reset source cadence");
         audio_driver_deinit_internal(true);
         CHECK(!st->inline_transport, "inline teardown retained state");
      }
   runloop_state_get_ptr()->flags = 0;
   settings->floats.slowmotion_ratio = 1;
   settings->bools.audio_time_stretch = settings->bools.audio_time_stretch_lowpass = false;
   settings->bools.audio_fastforward_speedup = false;
   audio_driver_publish_runloop();
   printf("inline native transport: 10 cases, %u failures\n", failures - before);
}

#include "inline_wide.h"
#include "producer_spans.h"

int main(void)
{
   /* One case at a time, for when a single one is being worked on:
    * DM_ONLY=ac3 runs the bitstream case alone. */
   const char *only = getenv("DM_ONLY");
#define RUN(tag, call) do { if (!only || strstr(only, tag)) { call; } } while (0)
   printf("discrete multi-channel:\n");
   RUN("rewindframes", rewind_frame_cases());
   RUN("multireverse", multi_rewind_cases());
   RUN("mixedreverse", rewind_mixed_cases());
   RUN("reverseboundary", rewind_boundary_cases());
   RUN("statereverse", rewind_state_cases());
   RUN("independentlpf", independent_lpf_cases());
   RUN("rawspeed", raw_speed_cases());
   RUN("producerspans", producer_span_cases());
   RUN("unityclamp", unity_clamp_cases());
   RUN("bufferingcallback", callback_buffering_case());
   RUN("menutiming", menu_timing_cases());
   RUN("inlinewide", inline_wide_cases());
   RUN("inlineformat", inline_format_cases());
   RUN("callbackcontinuity", inline_callback_cases());
   RUN("inline", inline_transport_cases());
   RUN("canonicalreserve", canonical_reserve_cases());
   RUN("transportowner", transport_owner_cases());
   RUN("transportdiscard", transport_discard_cases());
   RUN("transportquality", transport_quality_cases());
   RUN("transportsettings", transport_settings_cases());
   RUN("transportrequest", transport_request_cases());
   RUN("transportscheduler", transport_scheduler_cases());
   RUN("nativerender", native_render_cases());
   RUN("srcreset", resampler_discontinuity_cases());
   RUN("suspended", suspended_multichannel_case(true, true));
   RUN("suspended", suspended_multichannel_case(false, true));
   RUN("suspended", suspended_multichannel_case(true, false));
   RUN("suspended", suspended_multichannel_case(false, false));
   RUN("prefix", canonical_prefix_case());
   RUN("pending", pending_lifecycle_case(false));
   RUN("pending", pending_lifecycle_case(true));
   RUN("pending", pending_output_case(false, false));
   RUN("pending", pending_output_case(true, false));
   RUN("pending", pending_output_case(false, true));
   RUN("pending", pending_output_case(true, true));
   RUN("epoch", layout_epoch_pressure_case(false));
   RUN("epoch", layout_epoch_pressure_case(true));
   RUN("epoch", layout_epoch_case(false, false));
   RUN("epoch", layout_epoch_case(true, false));
   RUN("epoch", layout_epoch_case(false, true));
   RUN("epoch", layout_epoch_case(true, true));
   RUN("wrap", wide_wrap_case(false));
   RUN("wrap", wide_wrap_case(true));
   RUN("fullring", full_wide_ring_case(false));
   RUN("fullring", full_wide_ring_case(true));
   RUN("wideformat", stereo_ring_format_case(false, true));
   RUN("wideformat", stereo_ring_format_case(true, false));
   RUN("wideformat", stereo_ring_format_case(false, false));
   RUN("wideformat", stereo_ring_format_case(true, true));
   RUN("canonical", bounded_canonical_case(true));
   RUN("canonical", bounded_canonical_case(false));
   RUN("large", large_inline_batch_case(true, false));
   RUN("large", large_inline_batch_case(false, false));
   RUN("discrete", discrete_case(true, true));
   RUN("discrete", discrete_case(false, false));
   RUN("discrete", discrete_case(false, true));
   RUN("discrete", discrete_case(true, false));
   RUN("threaded", threaded_case(true, true));
   RUN("threaded", threaded_case(false, false));
   RUN("threaded", stereo_on_wide_ring_case());
   RUN("record",   record_case());
   RUN("recordentry", record_stereo_entry_case(0, AUDIO_LAYOUT_STEREO));
   RUN("recordentry", record_stereo_entry_case(0, AUDIO_LAYOUT_5POINT1));
   RUN("recordentry", record_stereo_entry_case(0, AUDIO_LAYOUT_7POINT1));
   RUN("recordentry", record_stereo_entry_case(1, AUDIO_LAYOUT_STEREO));
   RUN("recordentry", record_stereo_entry_case(1, AUDIO_LAYOUT_5POINT1));
   RUN("recordentry", record_stereo_entry_case(1, AUDIO_LAYOUT_7POINT1));
   RUN("recordentry", record_stereo_entry_case(2, AUDIO_LAYOUT_STEREO));
   RUN("recordentry", record_stereo_entry_case(2, AUDIO_LAYOUT_5POINT1));
   RUN("recordentry", record_stereo_entry_case(2, AUDIO_LAYOUT_7POINT1));
   RUN("ac3",      ac3_bitstream_case());
   RUN("virtual",  virtual_surround_case());
   RUN("fold",     fold_case());
   RUN("fold",     large_inline_batch_case(true, true));
   RUN("fold",     large_inline_batch_case(false, true));
   audio_driver_deinit_internal(true);
   owned_free();
   free(cap); free(rec_cap);
   if (failures) { printf("%u failure(s)\n", failures); return 1; }
   printf("discrete multi-channel: a core's channels reach their speakers as they are\n");
   return 0;
}
