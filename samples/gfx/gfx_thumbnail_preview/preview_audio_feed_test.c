/* Drive the real preview feeder and mixer across oversized video packets. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <audio/audio_mixer.h>
#include <formats/rmp4.h>
#include <formats/rwebm.h>
#include <formats/audio.h>
#include <formats/video_player.h>
#include <formats/data_transfer.h>
#include <memmap.h>
#ifdef PREVIEW_COUNT_READS
#include <retro_atomic.h>
#include <streams/file_stream.h>
static retro_atomic_int_t window_reads, window_bytes;
int64_t __real_filestream_read(RFILE *file, void *data, int64_t size);
int64_t __wrap_filestream_read(RFILE *file, void *data, int64_t size)
{
   retro_atomic_fetch_add_int(&window_reads, 1);
   retro_atomic_fetch_add_int(&window_bytes, (int)size);
   return __real_filestream_read(file, data, size);
}
#endif
#ifdef HAVE_THREADS
#include <rthreads/rthreads.h>
#include <retro_atomic.h>
#include <retro_timers.h>
#endif
#include "../../../gfx/gfx_anim_preview.h"
#include "../../../audio/audio_driver.h"
#include "../../../configuration.h"

#define FRAMES 800

static settings_t settings;
static audio_mixer_sound_t *sound;
static audio_mixer_voice_t *voice;
static unsigned repeats, finishes;
static bool seek_probe, seek_probe_wait, seek_probe_sparse;
static int seek_probe_error;
static unsigned seek_probe_calls, seek_probe_end, seek_probe_stride;
static int64_t seek_probe_target;
static size_t seek_probe_anchor, seek_probe_lo, seek_probe_hi;
static size_t seek_probe_frontier, seek_probe_avail, seek_probe_pin_lo, seek_probe_pin_hi;
static unsigned seek_probe_boundary;

settings_t *config_get_ptr(void) { return &settings; }

size_t image_transfer_anim_stream_media_floor(void *stream,
      enum image_type_enum type)
{ return type == IMAGE_TYPE_WEBM ? rwebm_media_floor((rwebm_t*)stream) : rmp4_media_floor((rmp4_t*)stream); }

int64_t image_transfer_anim_stream_duration_ns(void *stream,
      enum image_type_enum type)
{ return type == IMAGE_TYPE_WEBM ? rwebm_duration_ns((rwebm_t*)stream) : rmp4_duration_ns((rmp4_t*)stream); }

void image_transfer_anim_stream_get_info(void *s, enum image_type_enum t,
      unsigned *w, unsigned *h, int *n, int *loops)
{ (void)s; (void)t; *w = *h = 0; *n = 2; *loops = 0; }
bool image_transfer_anim_stream_set_argb(void *s, enum image_type_enum t, int a)
{ (void)s; (void)t; (void)a; return false; }

/* Video drawing is outside this audio test; accidental calls fail loudly. */
uint64_t mem_stats_free(void) { abort(); return 0; }
enum image_type_enum image_texture_get_type(const char *p)
{ (void)p; abort(); return IMAGE_TYPE_NONE; }
void *image_transfer_anim_stream_new_hw_avail(void *b, size_t n, size_t a,
      enum image_type_enum t, int *more, size_t *lo, size_t *hi)
{ (void)b; (void)n; (void)a; (void)t; (void)more; (void)lo; (void)hi; abort(); return NULL; }
void *image_transfer_anim_stream_new_hw_deferred_avail(void *b, size_t n,
      size_t a, enum image_type_enum t, int *more, size_t *lo, size_t *hi)
{ (void)b; (void)n; (void)a; (void)t; (void)more; (void)lo; (void)hi; abort(); return NULL; }
void *image_transfer_anim_stream_new_avail(void *b, size_t n, size_t a,
      enum image_type_enum t, int *more, size_t *lo, size_t *hi)
{ (void)b; (void)n; (void)a; (void)t; (void)more; (void)lo; (void)hi; abort(); return NULL; }
void image_transfer_anim_stream_free(void *s, enum image_type_enum t)
{ (void)s; (void)t; abort(); }
size_t image_transfer_anim_stream_consumed(void *s, enum image_type_enum t)
{ (void)s; (void)t; if (!seek_probe) abort(); return seek_probe_anchor; }
size_t image_transfer_anim_stream_feed_frontier(void *s, enum image_type_enum t)
{ (void)s; (void)t; if (!seek_probe) abort(); return seek_probe_frontier ? seek_probe_frontier : seek_probe_anchor; }
void image_transfer_anim_stream_pin_span(void *s, enum image_type_enum t,
      size_t *lo, size_t *hi)
{ (void)s; (void)t; if (!seek_probe) abort(); *lo = seek_probe_pin_lo; *hi = seek_probe_pin_hi; }
int image_transfer_anim_stream_seek_span(void *s, enum image_type_enum t,
      size_t *lo, size_t *hi)
{
   (void)s; (void)t;
   if (!seek_probe_sparse) return 0;
   *lo = seek_probe_anchor;
   *hi = seek_probe_anchor + 10;
   return 1;
}
void image_transfer_anim_stream_next_span(void *s, enum image_type_enum t,
      size_t *lo, size_t *hi)
{ (void)s; (void)t; if (!seek_probe) abort(); *lo=seek_probe_lo; *hi=seek_probe_hi; }
void image_transfer_anim_stream_set_avail(void *s, enum image_type_enum t, size_t a)
{ (void)s; (void)t; if (!seek_probe) abort(); seek_probe_avail = a; }
const uint32_t *image_transfer_anim_stream_next(void *s,
      enum image_type_enum t, int *ms)
{ (void)s; (void)t; (void)ms; abort(); return NULL; }
int64_t image_transfer_anim_stream_start_ns(void *s, enum image_type_enum t)
{ (void)s; (void)t; return 0; }
bool image_transfer_anim_stream_seekable(void *s, enum image_type_enum t)
{ return s && (t == IMAGE_TYPE_MP4 || t == IMAGE_TYPE_WEBM); }
bool image_transfer_anim_stream_seek_begin(void *s, enum image_type_enum t,
      int64_t us) { (void)s; (void)t; if (!seek_probe) abort(); seek_probe_target=us; return true; }
int image_transfer_anim_stream_seek_step(void *s, enum image_type_enum t,
      int64_t *us)
{
   (void)s; (void)t;
   if (!seek_probe) abort();
   seek_probe_calls++;
   if (seek_probe_frontier)
   {
      if (seek_probe_avail < seek_probe_frontier + 32) return 0;
      *us = seek_probe_target;
      return 1;
   }
   if (seek_probe_sparse) seek_probe_anchor += seek_probe_stride;
   if (seek_probe_boundary == 1) seek_probe_anchor = 1;
   if (seek_probe_boundary == 2) seek_probe_anchor = ((gfx_anim_preview_t*)s)->seek_feed_hi;
   if (seek_probe_boundary == 3)
   {
      seek_probe_lo = seek_probe_anchor;
      seek_probe_hi = ((gfx_anim_preview_t*)s)->seek_feed_hi + 1;
   }
   if (seek_probe_error) return -1;
   if (!seek_probe_end || seek_probe_calls < seek_probe_end) return 0;
   *us=seek_probe_target;
   return 1;
}
bool image_transfer_anim_stream_completed(const void *stream,
      enum image_type_enum type)
{
   (void)stream;
   (void)type;
   return false;
}

bool image_transfer_anim_stream_waiting(const void *s, enum image_type_enum t)
{ (void)s; (void)t; if (!seek_probe) abort(); return seek_probe_wait; }
void image_transfer_anim_stream_rewind(void *s, enum image_type_enum t)
{ (void)s; (void)t; abort(); }

static void stopped(audio_mixer_sound_t *s, unsigned reason)
{
   (void)s;
   if (reason == AUDIO_MIXER_SOUND_REPEATED) repeats++;
   if (reason == AUDIO_MIXER_SOUND_FINISHED) finishes++;
}

bool audio_driver_mixer_add_stream(audio_mixer_stream_params_t *p)
{
   sound = audio_mixer_load_m4a(p->buf, p->bufsize);
   audio_mixer_sound_set_data_owner(sound, p->buf_owner, p->buf_owner_free);
   if (!sound) return false;
   audio_mixer_sound_set_avail(sound, p->avail);
   voice = audio_mixer_play(sound, true, p->volume, "sinc",
         RESAMPLER_QUALITY_DONTCARE, stopped);
   *p->out_slot = voice ? 0 : -1;
   if (voice) return true;
   audio_mixer_destroy(sound);
   sound = NULL;
   return false;
}

int64_t audio_driver_mixer_stream_byte_tell(unsigned slot)
{ (void)slot; return voice ? (int64_t)audio_mixer_voice_buffer_tell(voice) : -1; }

void audio_driver_mixer_stream_set_avail(unsigned slot, size_t avail)
{ (void)slot; audio_mixer_voice_set_avail(voice, avail); }

void audio_driver_mixer_stream_set_repeat(unsigned i, bool repeat)
{
   (void)i;
   (void)repeat;
}

void audio_driver_mixer_stream_set_paused(unsigned slot, bool paused)
{ (void)slot; audio_mixer_voice_set_paused(voice, paused); }

bool audio_driver_mixer_add_stream_clocked(audio_mixer_stream_params_t *params,
      audio_mixer_voice_t **out_voice, size_t *serial)
{
   bool ok = audio_driver_mixer_add_stream(params);
   *out_voice = NULL;
   *serial = 0;
   if (ok && audio_mixer_voice_clock(voice, NULL, NULL, serial))
      *out_voice = voice;
   return ok;
}

static bool prepared_s16;
bool audio_driver_mixer_add_prepared_stream(audio_mixer_stream_params_t *p,
      audio_mixer_sound_t *prepared, audio_mixer_voice_t **out_voice, size_t *serial)
{
   *out_voice = NULL;
   *serial = 0;
   sound = prepared;
   voice = prepared_s16 ? audio_mixer_play_s16(sound, true,
         AUDIO_MIXER_GAIN_UNITY, "sinc", RESAMPLER_QUALITY_DONTCARE, stopped)
      : audio_mixer_play(sound, true, p->volume, "sinc",
            RESAMPLER_QUALITY_DONTCARE, stopped);
   *p->out_slot = voice ? 0 : -1;
   if (voice && audio_mixer_voice_clock(voice, NULL, NULL, serial))
   {
      *out_voice = voice;
      return true;
   }
   audio_mixer_destroy(sound);
   sound = NULL;
   return false;
}

void audio_driver_mixer_remove_stream_clocked(unsigned slot,
      audio_mixer_voice_t *expected, size_t serial)
{
   size_t current = 0;
   if (voice == expected
         && audio_mixer_voice_clock(voice, NULL, NULL, &current)
         && current == serial)
      audio_driver_mixer_remove_stream(slot);
}

const char *audio_driver_mixer_get_stream_name(unsigned slot)
{ (void)slot; return voice ? "__gfx_thumb_preview" : NULL; }

void audio_driver_mixer_remove_stream(unsigned slot)
{
   (void)slot;
   if (voice) audio_mixer_stop(voice);
   if (sound) audio_mixer_destroy(sound);
   voice = NULL;
   sound = NULL;
}

void config_userdata_free(void *u) { (void)u; }
int config_userdata_get_float(void *u, const char *k, float *v, float d)
{ (void)u; (void)k; *v = d; return 0; }
int config_userdata_get_int(void *u, const char *k, int *v, int d)
{ (void)u; (void)k; *v = d; return 0; }
int config_userdata_get_float_array(void *u, const char *k, float **v,
      unsigned *n)
{ (void)u; (void)k; (void)v; (void)n; return 0; }
int config_userdata_get_int_array(void *u, const char *k, int **v, unsigned *n)
{ (void)u; (void)k; (void)v; (void)n; return 0; }
int config_userdata_get_string(void *u, const char *k, char **v, const char *d)
{ (void)u; (void)k; (void)v; (void)d; return 0; }

static int run_one(const char *path, unsigned rate)
{
   FILE *f = fopen(path, "rb");
   uint8_t *buf;
   size_t len, i, ticks;
   int64_t ns;
   rmp4_t *demux;
   gfx_anim_preview_t *p;
   float out[FRAMES * 2];
   double energy = 0.0;
   int failed = 0;
   int64_t wall = 1000000;
   size_t previous_frames = 0;
   unsigned starved_blocks = 0;
   if (!f) return 1;
   fseek(f, 0, SEEK_END);
   len = (size_t)ftell(f);
   rewind(f);
   buf = (uint8_t*)malloc(len);
   if (!buf || fread(buf, 1, len, f) != len)
   {
      free(buf);
      fclose(f);
      return 1;
   }
   fclose(f);
   demux = rmp4_open_memory(buf, len);
   if (!demux) { free(buf); return 1; }
   ns = rmp4_duration_ns(demux);
   if (ns <= 0) { rmp4_close(demux); free(buf); return 1; }
   p = gfx_anim_preview_wrap(demux, IMAGE_TYPE_MP4, NULL, buf, len, false, path);
   if (!p) { rmp4_close(demux); free(buf); return 1; }
   ticks = (size_t)((double)ns / 1000000000.0 * 60 * 3.5);
   repeats = finishes = 0;
   audio_mixer_init(rate);
   settings.bools.menu_thumbnail_preview_audio = true;
   if (gfx_anim_preview_clock(p, wall) != wall) failed = 1;
   gfx_anim_preview_set_paused(p, true);
   gfx_anim_preview_audio_begin(p);
   if (p->audio_dt || voice) failed = 1;
   gfx_anim_preview_set_paused(p, false);
   gfx_anim_preview_audio_begin(p);
   for (i = 0; i < ticks && p->audio_dt; i++)
   {
      size_t j;
      if (i == 120 && voice)
      {
         size_t at = audio_mixer_voice_buffer_tell(voice);
         int64_t before = gfx_anim_preview_clock(p, wall);
         unsigned n;
         gfx_anim_preview_set_paused(p, true);
         for (n = 0; n < 8; n++)
         {
            memset(out, 0, sizeof(out));
            audio_mixer_mix(out, FRAMES, 1.0f, false);
            for (j = 0; j < FRAMES * 2; j++)
               if (out[j] != 0.0f) failed = 1;
            wall += 33000;
            if (gfx_anim_preview_clock(p, wall) != before) failed = 1;
         }
         if (audio_mixer_voice_buffer_tell(voice) != at) failed = 1;
         gfx_anim_preview_set_paused(p, false);
      }
      if (!gfx_anim_preview_audio_feed(p)) break;
      memset(out, 0, sizeof(out));
      audio_mixer_mix(out, FRAMES, 1.0f, false);
      wall += 33000;
      {
         size_t mixed = 0;
         unsigned hz = 0;
         int64_t actual = gfx_anim_preview_clock(p, wall);
         int64_t expected;
         if (!audio_mixer_voice_clock(voice, &mixed, &hz, NULL) || hz != rate)
            failed = 1;
         expected = 1000000 + (int64_t)((uint64_t)mixed * 1000000 / rate);
         if (mixed == previous_frames)
         {
            starved_blocks++;
            for (j = 0; j < FRAMES * 2; j++)
               if (out[j] != 0.0f) failed = 1;
         }
         previous_frames = mixed;
         if (actual != expected)
         {
            if (!failed)
               printf("clock mismatch at tick %lu: actual=%lld expected=%lld audio=%u\n",
                     (unsigned long)i, (long long)actual, (long long)expected,
                     p->clock_audio);
            failed = 1;
         }
      }
      for (j = 0; j < FRAMES * 2; j++) energy += out[j] * out[j];
   }
   failed |= i != ticks || finishes || repeats < 3 || energy <= 0;
   if (voice)
   {
      int64_t before = gfx_anim_preview_clock(p, wall);
      size_t serial = p->clock_serial;
      gfx_anim_preview_audio_stop(p);
      gfx_anim_preview_audio_begin(p);
      wall += 33000;
      if (!voice || gfx_anim_preview_clock(p, wall) != before
            || !p->clock_serial || p->clock_serial == serial) failed = 1;
      if (voice && gfx_anim_preview_audio_feed(p))
      {
         memset(out, 0, sizeof(out));
         audio_mixer_mix(out, FRAMES, 1.0f, false);
         wall += 33000;
         if (gfx_anim_preview_clock(p, wall) != before
               + (int64_t)((uint64_t)FRAMES * 1000000 / rate)) failed = 1;
      }
      else failed = 1;
   }
   if (p->audio_dt && voice)
   {
      size_t resident = 0;
      const uint8_t *base;
      audio_driver_mixer_remove_stream((unsigned)p->audio_slot);
      base = data_transfer_window_base(p->audio_dt, &resident);
      if (!base || !resident || !p->audio_owner)
         failed = 1;
      if (gfx_anim_preview_audio_feed(p) || p->audio_dt
            || p->audio_owner || p->audio_voice)
         failed = 1;
   }
   else failed = 1;
   printf("%s: rate=%u ticks=%lu repeats=%u finishes=%u energy=%.1f starved=%u %s\n",
         path, rate, (unsigned long)i, repeats, finishes, energy,
         starved_blocks,
         failed ? "FAIL" : "PASS");
   gfx_anim_preview_release(p);
   audio_mixer_done();
   rmp4_close(demux);
   free(buf);
   return failed;
}

static int prepared_session(const char *path, bool s16, bool webm,
      int64_t target_us)
{
   FILE *file = fopen(path, "rb");
   uint8_t *data = NULL;
   void *reference = NULL;
   float *expected = NULL;
   void *actual = NULL;
   gfx_anim_preview_t *p = NULL;
   gfx_anim_preview_audio_prepared_t *a = NULL;
   void *demux = NULL;
   enum audio_type_enum type = AUDIO_TYPE_AAC;
   enum image_type_enum container = webm ? IMAGE_TYPE_WEBM : IMAGE_TYPE_MP4;
   size_t size, got, frames = 123, resident = 0;
   bool reserved = false;
   unsigned rate, channels, i, steps;
   long length;
   int failed = 1, result = 0;
   double peak = 0.0;
   if (!file) return 1;
   if (fseek(file, 0, SEEK_END) || (length = ftell(file)) <= 0
         || fseek(file, 0, SEEK_SET)) goto done;
   size = (size_t)length;
   data = (uint8_t*)malloc(size);
   if (!data || fread(data, 1, size, file) != size) goto done;
   if (webm) type = audio_transfer_webm_audio_type(data, size);
   reference = audio_transfer_new(type);
   if (!reference) goto done;
   audio_transfer_set_buffer_ptr(reference, type, data, size);
   if (!audio_transfer_start(reference, type)
         || !audio_transfer_info(reference, type, &channels, &rate, NULL)
         || !channels || channels > 2) goto done;
   expected = (float*)calloc(64 * channels, sizeof(float));
   actual = calloc(64 * 2, sizeof(float));
   if (!expected || !actual) goto done;
   if (!audio_transfer_seek(reference, type,
            (uint64_t)rate * (uint64_t)target_us / 1000000)) goto done;
   for (steps = 0; steps < 10000; steps++)
   {
      result = audio_transfer_read_f32(reference, type, expected, 64, &got);
      if (result < 0 || (result == AUDIO_PROCESS_END && !got)) goto done;
      if (got) break;
   }
   if (got != 64) goto done;
   result = 0;
   demux = webm ? (void*)rwebm_open_memory(data, size) : (void*)rmp4_open_memory(data, size);
   if (!demux) goto done;
   p = gfx_anim_preview_wrap(demux, container, NULL, data, size, false, path);
   if (!p) goto done;
   audio_mixer_init(rate);
   prepared_s16 = s16;
   gfx_anim_preview_set_paused(p, true);
   if (gfx_anim_preview_clock(p, 1000) != 1000) goto mixer_done;
   a = gfx_anim_preview_audio_prepare_new(p, target_us, rate, s16);
   if (!a) goto mixer_done;
   /* Cancel an unpublished preparation, then prepare its replacement. */
   if (gfx_anim_preview_audio_prepare_step(a) < 0) goto mixer_done;
   gfx_anim_preview_audio_prepare_free(a);
   a = NULL;
   retro_atomic_store_relaxed_int(&p->seek_audio_format,
         (int)(rate | (s16 ? 0x40000000U : 0)));
   seek_probe = true;
   seek_probe_calls = 0;
   seek_probe_end = 1;
   {
      int64_t position = -1;
      if (gfx_anim_preview_seek_step(p, target_us, true, &position)
            != VIDEO_PLAYER_SEEK_PROGRESS || p->seek_audio)
         goto mixer_done;
      if (gfx_anim_preview_seek_step(p, target_us, false, &position)
            != VIDEO_PLAYER_SEEK_PROGRESS || !p->seek_audio)
         goto mixer_done;
      for (steps = 0; steps < 10000; steps++)
      {
         result = gfx_anim_preview_seek_step(p, target_us, false, &position);
         if (result != VIDEO_PLAYER_SEEK_PROGRESS && result != 0) break;
      }
      if (result != 1 || position != target_us) goto mixer_done;
   }
   a = p->seek_audio;
   p->seek_audio = NULL;
   seek_probe = false;
   result = gfx_anim_preview_audio_adopt(p, a);
   a = NULL;
   if (!result || !p->audio_voice || !p->audio_owner) goto mixer_done;
   resident = data_transfer_window_resident(p->audio_dt);
   reserved = data_transfer_window_is_reserved(p->audio_dt);
   if (size > 16 * 1024 * 1024 && data_transfer_reserve_supported()
         && (!reserved || resident >= size / 2))
      goto mixer_done;
   if (p->type == IMAGE_TYPE_WEBM && reserved && resident > 3 * 1024 * 1024)
      goto mixer_done;
   if (s16) audio_mixer_mix_s16((int16_t*)actual, 64, AUDIO_MIXER_GAIN_UNITY, false);
   else audio_mixer_mix((float*)actual, 64, 1.0f, false);
   for (i = 0; i < 64 * 2; i++)
      if (s16 ? ((int16_t*)actual)[i] != 0 : ((float*)actual)[i] != 0.0f)
         goto mixer_done;
   if (!audio_mixer_voice_clock(p->audio_voice, &frames, NULL, NULL) || frames
         || gfx_anim_preview_clock(p, 9999) != 1000) goto mixer_done;
   gfx_anim_preview_set_paused(p, false);
   if (!gfx_anim_preview_audio_feed(p)) goto mixer_done;
   if (s16) audio_mixer_mix_s16((int16_t*)actual, 64, AUDIO_MIXER_GAIN_UNITY, false);
   else audio_mixer_mix((float*)actual, 64, 1.0f, false);
   for (i = 0; i < 64 * 2; i++)
   {
      float value = s16 ? (float)((int16_t*)actual)[i] / 32768.0f : ((float*)actual)[i];
      double error = (double)value - expected[(i / 2) * channels
         + (channels == 2 ? i % 2 : 0)];
      if (error < 0) error = -error;
      if (error > peak) peak = error;
   }
   failed = peak > (webm ? 0.0001 : s16 ? 0.00004 : 0.0000001);
 mixer_done:
   seek_probe = false;
   seek_probe_calls = seek_probe_end = 0;
   gfx_anim_preview_audio_prepare_free(a); a = NULL;
   gfx_anim_preview_release(p); p = NULL;
   audio_mixer_done();
 done:
   gfx_anim_preview_audio_prepare_free(a);
   if (p) gfx_anim_preview_release(p);
   if (reference) audio_transfer_free(reference, type);
   if (demux) { if (webm) rwebm_close((rwebm_t*)demux); else rmp4_close((rmp4_t*)demux); }
   free(data); free(expected); free(actual);
   fclose(file);
   printf("prepared session: target=%ld s16=%u reserved=%u resident=%lu peak=%.9f %s\n",
         (long)target_us, s16, reserved, (unsigned long)resident, peak,
         failed ? "FAIL" : "PASS");
   return failed;
}

static int bounded_aac_seek(const char *path)
{
   FILE *file = fopen(path, "rb");
   uint8_t *data = NULL;
   void *linear = NULL, *seek = NULL, *legacy = NULL;
   float *reference = NULL, *actual = NULL, *old = NULL;
   uint64_t total = 0, target;
   unsigned channels = 0, rate, pass, steps, i;
   size_t size, got, decoded = 0;
   long length;
   int failed = 1, result;
   double peak = 0.0, old_peak = 0.0, equivalent = 0.0;
   if (!file) return 1;
   if (fseek(file, 0, SEEK_END) || (length = ftell(file)) <= 0
         || fseek(file, 0, SEEK_SET)) goto done;
   size = (size_t)length;
   data = (uint8_t*)memreserve(size);
   if (!data || !memcommit(data, size)
         || fread(data, 1, size, file) != size) goto done;
   linear = audio_transfer_new(AUDIO_TYPE_AAC);
   seek = audio_transfer_new(AUDIO_TYPE_AAC);
   legacy = audio_transfer_new(AUDIO_TYPE_AAC);
   if (!linear || !seek || !legacy) goto done;
   audio_transfer_set_buffer_ptr(linear, AUDIO_TYPE_AAC, data, size);
   audio_transfer_set_buffer_ptr(seek, AUDIO_TYPE_AAC, data, size);
   audio_transfer_set_buffer_ptr(legacy, AUDIO_TYPE_AAC, data, size);
   if (!audio_transfer_start(legacy, AUDIO_TYPE_AAC)
         || !audio_transfer_start(linear, AUDIO_TYPE_AAC)
         || !audio_transfer_start(seek, AUDIO_TYPE_AAC)
         || !audio_transfer_info(linear, AUDIO_TYPE_AAC, &channels, &rate, &total)
         || !channels || channels > 8 || total < 1024 || total > 1000000)
      goto done;
   reference = (float*)calloc((size_t)total * channels, sizeof(float));
   actual = (float*)calloc(64 * channels, sizeof(float));
   old = (float*)calloc(64 * channels, sizeof(float));
   if (!reference || !actual || !old) goto done;
   while (decoded < (size_t)total)
   {
      got = 0;
      result = audio_transfer_read_f32(linear, AUDIO_TYPE_AAC,
            reference + decoded * channels, (size_t)total - decoded, &got);
      if (result < 0 || !got) goto done;
      decoded += got;
   }
   for (pass = 0; pass < 4; pass++)
   {
      target = pass == 0 ? total / 2 + 13 : pass == 1 ? total - 100
         : pass == 2 ? 113 : total / 3;
      audio_transfer_set_avail(seek, AUDIO_TYPE_AAC, size);
      if (!memrearm(data, size)) goto done;
      if (!audio_transfer_seek_begin(seek, AUDIO_TYPE_AAC, target)) goto done;
      /* Metadata slices may select a new byte frontier. Every encoded
       * source page is inaccessible: begin/retries must use copied
       * sample tables and refuse decoder reads until explicitly fed. */
      for (i = 0; i < 10; i++)
         if (audio_transfer_seek_step(seek, AUDIO_TYPE_AAC) != 0)
            goto done;
      if (audio_transfer_seek_begin(seek, AUDIO_TYPE_AAC, (uint64_t)-1)) goto done;
      if (pass == 3)
      {
         target = 113;
         if (!audio_transfer_seek_begin(seek, AUDIO_TYPE_AAC, target)) goto done;
      }
      if (mprotect(data, size, PROT_READ | PROT_WRITE) != 0) goto done;
      audio_transfer_set_avail(seek, AUDIO_TYPE_AAC, size);
      result = 0;
      for (steps = 0; steps < 8 && result == 0; steps++)
      {
         /* A replacement request may still be mapping its sample
          * table; re-admit the selected frontier after every step. */
         audio_transfer_set_avail(seek, AUDIO_TYPE_AAC, size);
         result = audio_transfer_seek_step(seek, AUDIO_TYPE_AAC);
      }
      if (result != 1) goto done;
      got = 0;
      if (audio_transfer_read_f32(seek, AUDIO_TYPE_AAC, actual, 64, &got) < 0
            || got != 64) goto done;
      if (!audio_transfer_seek(legacy, AUDIO_TYPE_AAC, target)
            || audio_transfer_read_f32(legacy, AUDIO_TYPE_AAC, old, 64, &got) < 0
            || got != 64) goto done;
      for (i = 0; i < 64 * channels; i++)
      {
         double error = (double)actual[i] - reference[(size_t)target * channels + i];
         if (error < 0) error = -error;
         if (error > peak) peak = error;
         error = (double)old[i] - reference[(size_t)target * channels + i];
         if (error < 0) error = -error;
         if (error > old_peak) old_peak = error;
         error = (double)old[i] - actual[i];
         if (error < 0) error = -error;
         if (error > equivalent) equivalent = error;
      }
   }
   /* Lossy AAC seeks reset PNS state; the existing synchronous seek
    * has the same documented limitation. Check absolute full-scale
    * error below -60dB for both against a sequential decode. */
   failed = peak > 0.001 || old_peak > 0.001 || equivalent > 0.0000001;
 done:
   if (legacy) audio_transfer_free(legacy, AUDIO_TYPE_AAC);
   if (linear) audio_transfer_free(linear, AUDIO_TYPE_AAC);
   if (seek) audio_transfer_free(seek, AUDIO_TYPE_AAC);
   if (data) memrelease(data, size);
   free(reference); free(actual); free(old);
   fclose(file);
   printf("bounded AAC seek: peak=%.9f legacy=%.9f difference=%.9f %s\n", peak, old_peak, equivalent, failed ? "FAIL" : "PASS");
   return failed;
}

static int prepared_aac(const char *path, bool s16)
{
   FILE *file = fopen(path, "rb");
   uint8_t *data = NULL, *copy = NULL;
   void *transfer = NULL, *scratch = NULL, *reference = NULL, *actual = NULL;
   audio_mixer_sound_t *first = NULL, *prepared = NULL;
   audio_mixer_voice_t *playing = NULL;
   unsigned channels = 0, rate = 0, active = 0, j;
   size_t got = 0, size, bytes = s16 ? sizeof(int16_t) : sizeof(float);
   long length;
   int failed = 1, result;
   if (!file) return 1;
   if (fseek(file, 0, SEEK_END) || (length = ftell(file)) <= 0
         || fseek(file, 0, SEEK_SET)) goto done;
   size = (size_t)length;
   data = (uint8_t*)malloc(size);
   copy = (uint8_t*)malloc(size);
   if (!data || !copy || fread(data, 1, size, file) != size) goto done;
   memcpy(copy, data, size);
   transfer = audio_transfer_new(AUDIO_TYPE_AAC);
   if (!transfer) goto done;
   audio_transfer_set_buffer_ptr(transfer, AUDIO_TYPE_AAC, copy, size);
   if (!audio_transfer_start(transfer, AUDIO_TYPE_AAC)
         || !audio_transfer_info(transfer, AUDIO_TYPE_AAC, &channels, &rate, NULL)
         || !channels || channels > 8 || !rate) goto done;
   scratch = calloc(113 * channels, bytes);
   reference = calloc((113 + 64) * 2, bytes);
   actual = calloc(64 * 2, bytes);
   if (!scratch || !reference || !actual) goto done;
   result = s16 ? audio_transfer_read_s16(transfer, AUDIO_TYPE_AAC,
         (int16_t*)scratch, 113, &got)
      : audio_transfer_read_f32(transfer, AUDIO_TYPE_AAC,
            (float*)scratch, 113, &got);
   if (result < 0 || got != 113) goto done;
   audio_mixer_init(rate);
   first = audio_mixer_load_m4a(data, size);
   if (!first) goto mixer_done;
   data = NULL;
   prepared = audio_mixer_load_m4a(copy, size);
   if (!prepared) goto mixer_done;
   copy = NULL;
   if (!audio_mixer_sound_set_prepared_transfer(prepared, transfer, rate, s16))
      goto mixer_done;
   transfer = NULL;
   playing = s16 ? audio_mixer_play_s16(first, false, AUDIO_MIXER_GAIN_UNITY,
         "sinc", RESAMPLER_QUALITY_NORMAL, NULL)
      : audio_mixer_play(first, false, 1.0f, "sinc",
            RESAMPLER_QUALITY_NORMAL, NULL);
   if (!playing) goto mixer_done;
   if (s16) audio_mixer_mix_s16((int16_t*)reference, 113 + 64,
         AUDIO_MIXER_GAIN_UNITY, false);
   else audio_mixer_mix((float*)reference, 113 + 64, 1.0f, false);
   audio_mixer_stop(playing);
   playing = s16 ? audio_mixer_play_s16(prepared, false, AUDIO_MIXER_GAIN_UNITY,
         "sinc", RESAMPLER_QUALITY_NORMAL, NULL)
      : audio_mixer_play(prepared, false, 1.0f, "sinc",
            RESAMPLER_QUALITY_NORMAL, NULL);
   if (!playing) goto mixer_done;
   if (s16) audio_mixer_mix_s16((int16_t*)actual, 64, AUDIO_MIXER_GAIN_UNITY, false);
   else audio_mixer_mix((float*)actual, 64, 1.0f, false);
   failed = memcmp(actual, (const uint8_t*)reference + 113 * 2 * bytes,
         64 * 2 * bytes) != 0;
   for (j = 0; j < 64 * 2; j++)
      if (s16 ? ((int16_t*)actual)[j] != 0 : ((float*)actual)[j] != 0.0f)
         active++;
   failed |= !active;
mixer_done:
   if (playing) audio_mixer_stop(playing);
   /* The sound keeps the decoder's borrowed source alive. */
   if (transfer) { audio_transfer_free(transfer, AUDIO_TYPE_AAC); transfer = NULL; }
   audio_mixer_destroy(prepared);
   audio_mixer_destroy(first);
   audio_mixer_done();
done:
   if (transfer) audio_transfer_free(transfer, AUDIO_TYPE_AAC);
   fclose(file);
   free(data); free(copy); free(scratch); free(reference); free(actual);
   printf("prepared AAC: s16=%u sample=113 %s\n", s16, failed ? "FAIL" : "PASS");
   return failed;
}

static int resident_webm_pcm(const uint8_t *data, size_t size,
      unsigned rate, bool s16, float *pcm, size_t target)
{
   enum audio_type_enum type = audio_transfer_webm_audio_type(data, size);
   void *transfer = audio_transfer_new(type);
   uint8_t *copy = NULL;
   audio_mixer_sound_t *resident = NULL;
   audio_mixer_voice_t *playing = NULL;
   float block[128];
   size_t done = 0, mixed = 0, before, serial;
   unsigned i, steps;
   int result = 0, failed = 1;
   if (!transfer) return 1;
   audio_transfer_set_buffer_ptr(transfer, type, data, size);
   if (!audio_transfer_start(transfer, type)
         || !audio_transfer_seek_begin_format(transfer, type, 0, s16)) goto finish;
   for (steps = 0; steps < 10000 && !result; steps++)
      result = audio_transfer_seek_step(transfer, type);
   if (result != 1) goto finish;
   copy = (uint8_t*)malloc(size);
   if (!copy) goto finish;
   memcpy(copy, data, size);
   resident = audio_mixer_load_weba(copy, size);
   if (!resident) goto finish;
   copy = NULL;
   if (!audio_mixer_sound_set_prepared_transfer(resident, transfer, rate, s16)) goto finish;
   transfer = NULL;
   playing = s16 ? audio_mixer_play_s16(resident, getenv("AUDIO_MOVIE_LOOPS") != NULL, AUDIO_MIXER_GAIN_UNITY,
         "sinc", RESAMPLER_QUALITY_DONTCARE, NULL)
      : audio_mixer_play(resident, getenv("AUDIO_MOVIE_LOOPS") != NULL, 1.0f, "sinc", RESAMPLER_QUALITY_DONTCARE, NULL);
   if (!playing) goto finish;
   for (steps = 0; steps < 8000 && done < target; steps++)
   {
      before = done;
      memset(block, 0, sizeof(block));
      if (s16) audio_mixer_mix_s16((int16_t*)block, 64, AUDIO_MIXER_GAIN_UNITY, false);
      else audio_mixer_mix(block, 64, 1.0f, false);
      if (!audio_mixer_voice_clock(playing, &mixed, NULL, &serial)
            || mixed < before || mixed - before > 64) goto finish;
      done = mixed;
      for (i = 0; i < (done - before) * 2 && before + i / 2 < target; i++)
         pcm[before * 2 + i] = s16 ? (float)((int16_t*)block)[i] / 32768.0f : block[i];
   }
   failed = done < target;
finish:
   if (playing) audio_mixer_stop(playing);
   audio_mixer_destroy(resident);
   if (transfer) audio_transfer_free(transfer, type);
   free(copy);
   return failed;
}

#ifdef HAVE_THREADS
typedef struct audio_group_mix_state
{
   audio_mixer_voice_t *voice;
   const float *reference;
   void *out;
   size_t serial, target, done, first_bad, peak_at;
   unsigned ticks;
   double peak;
   bool s16, failed;
   retro_atomic_int_t stop, finished;
} audio_group_mix_state_t;

static void audio_group_mix_thread(void *userdata)
{
   audio_group_mix_state_t *s = (audio_group_mix_state_t*)userdata;
   size_t mixed, serial;
   s->first_bad = (size_t)-1;
   for (s->ticks = 0; s->ticks < 10000 && s->done < s->target
         && !retro_atomic_load_acquire_int(&s->stop); s->ticks++)
   {
      size_t before = s->done;
      unsigned i;
      memset(s->out, 0, 128 * sizeof(float));
      if (s->s16) audio_mixer_mix_s16((int16_t*)s->out, 64, AUDIO_MIXER_GAIN_UNITY, false);
      else audio_mixer_mix((float*)s->out, 64, 1.0f, false);
      if (!audio_mixer_voice_clock(s->voice, &mixed, NULL, &serial)
            || serial != s->serial || mixed < before || mixed - before > 64)
      { s->failed = true; break; }
      s->done = mixed;
      for (i = 0; i < (mixed - before) * 2 && before + i / 2 < s->target; i++)
      {
         double sample = s->s16 ? (double)((int16_t*)s->out)[i] / 32768.0 : ((float*)s->out)[i];
         double error = sample - s->reference[before * 2 + i];
         if (error < 0) error = -error;
         if (error > 0.0001 && s->first_bad == (size_t)-1) s->first_bad = before + i / 2;
         if (error > s->peak) { s->peak = error; s->peak_at = before + i / 2; }
      }
      if (mixed == before) retro_sleep(1);
   }
   retro_atomic_store_release_int(&s->finished, 1);
}
#endif

static int continuous_webm(const char *path, bool s16)
{
   FILE *file = fopen(path, "rb");
   uint8_t *data = NULL;
   float *reference = NULL;
   void *transfer = NULL, *out = NULL;
   rwebm_t *demux = NULL;
   gfx_anim_preview_t *p = NULL;
   gfx_anim_preview_audio_prepared_t *a = NULL;
   enum audio_type_enum type;
   uint64_t total = 0;
   unsigned rate, channels, i, ticks = 0, steps;
   size_t size, target = 0, done = 0, serial, mixed, before;
   size_t first_bad = (size_t)-1, peak_at = 0;
   size_t max_refill = 0;
   unsigned requested_polls = 0;
   long length;
   int result = 0, failed = 1;
   double peak = 0;
   bool mixer_started = false;
   if (!file) return 1;
   if (fseek(file, 0, SEEK_END) || (length = ftell(file)) <= 0
         || fseek(file, 0, SEEK_SET)) goto finish;
   size = (size_t)length;
   data = (uint8_t*)malloc(size);
   if (!data || fread(data, 1, size, file) != size) goto finish;
   type = audio_transfer_webm_audio_type(data, size);
   transfer = audio_transfer_new(type);
   if (!transfer) goto finish;
   audio_transfer_set_buffer_ptr(transfer, type, data, size);
   if (!audio_transfer_start(transfer, type)
         || !audio_transfer_info(transfer, type, &channels, &rate, &total)
         || !channels || channels > 2) goto finish;
   if (!audio_transfer_seek(transfer, type, 0)) goto finish;
   target = (size_t)rate * 2;
   if (getenv("AUDIO_MOVIE_LOOPS"))
   {
      if (!total || total > (uint64_t)rate * 5) goto finish;
      target = (size_t)total * 2;
   }
   reference = (float*)calloc(target * 2, sizeof(float));
   out = calloc(64 * 2, sizeof(float));
   if (!reference || !out) goto finish;
   demux = rwebm_open_memory(data, size);
   if (!demux) goto finish;
   p = gfx_anim_preview_wrap(demux, IMAGE_TYPE_WEBM, NULL, data, size, false, path);
   if (!p) goto finish;
   audio_mixer_init(rate); mixer_started = true; prepared_s16 = s16;
   if (resident_webm_pcm(data, size, rate, s16, reference, target)) goto finish;
   a = gfx_anim_preview_audio_prepare_new(p, 0, rate, s16);
   if (!a) goto finish;
   for (steps = 0; steps < 10000 && !result; steps++)
      result = gfx_anim_preview_audio_prepare_step(a);
   if (result != 1) goto finish;
   result = gfx_anim_preview_audio_adopt(p, a); a = NULL;
   if (!result || !p->audio_voice) goto finish;
   gfx_anim_preview_set_paused(p, false);
#ifdef HAVE_THREADS
   if (getenv("AUDIO_FEED_RACE"))
   {
      audio_group_mix_state_t state;
      sthread_t *thread;
      bool feed_ok = true;
      memset(&state, 0, sizeof(state));
      state.voice = p->audio_voice; state.serial = p->audio_serial;
      state.reference = reference; state.out = out;
      state.target = target; state.s16 = s16;
      thread = sthread_create(audio_group_mix_thread, &state);
      if (!thread) goto finish;
      for (ticks = 0; ticks < 10000
            && !retro_atomic_load_acquire_int(&state.finished); ticks++)
      {
         size_t wanted = audio_mixer_voice_buffer_read_end(p->audio_voice, p->audio_serial);
         size_t old_hi = p->audio_hi;
         if (audio_mixer_voice_buffer_read_end(p->audio_voice, p->audio_serial + 1)
               || !gfx_anim_preview_audio_feed(p))
         { feed_ok = false; break; }
         if (wanted > old_hi && p->audio_hi > old_hi)
         {
            size_t refill = p->audio_hi - old_hi;
            if (refill > max_refill) max_refill = refill;
            requested_polls++;
         }
         retro_sleep(1);
      }
      retro_atomic_store_release_int(&state.stop, 1);
      sthread_join(thread);
      done = state.done; peak = state.peak;
      first_bad = state.first_bad; peak_at = state.peak_at;
      failed = !feed_ok || state.failed || done < target || peak > 0.0001
         || requested_polls < 2 || max_refill > 256 * 1024;
      goto finish;
   }
#endif
   for (ticks = 0; ticks < 8000 && done < target; ticks++)
   {
      size_t wanted = audio_mixer_voice_buffer_read_end(p->audio_voice, p->audio_serial);
      size_t old_hi = p->audio_hi;
      before = done;
      if (audio_mixer_voice_buffer_read_end(p->audio_voice, p->audio_serial + 1))
         goto finish;
      if (!gfx_anim_preview_audio_feed(p)) goto finish;
      if (wanted > old_hi && p->audio_hi > old_hi)
      {
         size_t refill = p->audio_hi - old_hi;
         if (refill > max_refill) max_refill = refill;
         requested_polls++;
      }
      memset(out, 0, 64 * 2 * sizeof(float));
      if (s16) audio_mixer_mix_s16((int16_t*)out, 64, AUDIO_MIXER_GAIN_UNITY, false);
      else audio_mixer_mix((float*)out, 64, 1.0f, false);
      if (!audio_mixer_voice_clock(p->audio_voice, &mixed, NULL, &serial)
            || serial != p->audio_serial || mixed < before || mixed - before > 64)
         goto finish;
      done = mixed;
      for (i = 0; i < (done - before) * 2 && before + i / 2 < target; i++)
      {
         double sample = s16 ? (double)((int16_t*)out)[i] / 32768.0 : ((float*)out)[i];
         double error = sample - reference[before * 2 + i];
         if (error < 0) error = -error;
         if (error > 0.0001 && first_bad == (size_t)-1) first_bad = before + i / 2;
         if (error > peak) { peak = error; peak_at = before + i / 2; }
      }
   }
   failed = done < target || peak > 0.0001 || requested_polls < 2
      || max_refill > 256 * 1024;
finish:
   printf("continuous WebM: s16=%u frames=%lu target=%lu polls=%u peak=%.9f %s\n",
         s16, (unsigned long)done, (unsigned long)target, ticks, peak,
         failed ? "FAIL" : "PASS");
   printf("audio element admission: polls=%u largest_refill=%lu\n",
         requested_polls, (unsigned long)max_refill);
   if (peak > 0.0001) printf("PCM difference: first=%lu maximum=%lu\n",
         (unsigned long)first_bad, (unsigned long)peak_at);
   gfx_anim_preview_audio_prepare_free(a);
   if (p) gfx_anim_preview_release(p);
   if (mixer_started) audio_mixer_done();
   if (demux) rwebm_close(demux);
   if (transfer) audio_transfer_free(transfer, type);
   if (file) fclose(file);
   free(data); free(reference); free(out);
   return failed;
}

static bool seek_pending(int result)
{
   return result == 0 || result == VIDEO_PLAYER_SEEK_PROGRESS;
}

static int bounded_video_seek(void)
{
   gfx_anim_preview_t p;
   int64_t position = -1;
   int failed = 0;
   unsigned attempt;
   memset(&p, 0, sizeof(p));
   p.stream = &p;
   p.type = IMAGE_TYPE_MP4;
   p.audio_slot = -1;
   seek_probe = true;
   seek_probe_calls = 0;
   seek_probe_end = 64;
   for (attempt = 0; attempt < 8 && p.seek_phase != 2; attempt++)
      if (!seek_pending(gfx_anim_preview_seek_step(&p, 1200000, attempt == 0, &position)))
         failed = 1;
   if (p.seek_phase != 2 || seek_probe_calls != 64) failed = 1;
   if (gfx_anim_preview_seek_step(&p, 1200000, false, &position) != 1
         || position != 1200000) failed = 1;
   seek_probe_calls = seek_probe_end = 0;
   if (!seek_pending(gfx_anim_preview_seek_step(&p, 0, true, &position))
         || !seek_probe_calls || seek_probe_calls > 1024) failed = 1;
   seek_probe_calls = 0;
   seek_probe_wait = true;
   if (gfx_anim_preview_seek_step(&p, 0, true, &position) != 0
         || seek_probe_calls != 1) failed = 1;
   seek_probe_calls = 0;
   seek_probe_wait = false;
   seek_probe_error = 1;
   if (gfx_anim_preview_seek_step(&p, 0, true, &position) != -1
         || seek_probe_calls != 1) failed = 1;
   seek_probe_error = 0;
   {
      const char *path = "seek-batch-window.tmp";
      unsigned char zeros[1024];
      FILE *file = fopen(path, "wb");
      unsigned i;
      memset(zeros, 0, sizeof(zeros));
      if (!file) failed = 1;
      else
      {
         for (i = 0; i < 1024; i++)
            if (fwrite(zeros, 1, sizeof(zeros), file) != sizeof(zeros)) failed = 1;
         fclose(file);
         p.dt = data_transfer_open_window(path, 65536);
         if (!p.dt) failed = 1;
         else
         {
            p.windowed = true;
            p.len = 1024 * 1024;
            p.feed_ahead = 4096;
#ifdef PREVIEW_COUNT_READS
            if (!data_transfer_window_extend(p.dt, p.len)) failed = 1;
            p.feed_tell = seek_probe_anchor = 65536;
            p.feed_res_hi = p.len;
            seek_probe_lo = seek_probe_hi = seek_probe_boundary = 0;
            seek_probe_calls = 0;
            seek_probe_wait = true;
            retro_atomic_store_relaxed_int(&window_reads, 0);
            if (!seek_pending(gfx_anim_preview_seek_step(&p, 0, true, &position))
                  || retro_atomic_load_relaxed_int(&window_reads))
               failed = 1;
            fprintf(stderr, "warm seek window: %d reads\n",
                  retro_atomic_load_relaxed_int(&window_reads));
            seek_probe_wait = false;
#endif
            for (i = 1; i <= 3; i++)
            {
               p.seek_phase = 1;
               p.seek_feed_anchor = p.seek_feed_hi = 0;
               seek_probe_anchor = 4096;
               seek_probe_lo = seek_probe_hi = 0;
               seek_probe_boundary = i;
               seek_probe_calls = 0;
               seek_probe_wait = i != 1;
               if (!seek_pending(gfx_anim_preview_seek_step(&p, 0, false, &position))
                     || (i == 1 ? seek_probe_calls != 1
                        : seek_probe_calls <= 1 || seek_probe_calls > 8))
                  failed = 1;
               fprintf(stderr, "seek boundary %u: %u decoder steps\n",
                     i, seek_probe_calls);
               seek_probe_wait = false;
            }
            seek_probe_boundary = 0;
            seek_probe_sparse = seek_probe_wait = true;
            seek_probe_calls = seek_probe_end = 0;
            seek_probe_stride = 128;
            seek_probe_anchor = 65536;
            p.seek_phase = 1;
            data_transfer_window_rewind(p.dt);
            p.seek_feed_anchor = p.seek_feed_hi = seek_probe_anchor;
            if (gfx_anim_preview_seek_step(&p, 0, false, &position)
                  != VIDEO_PLAYER_SEEK_PROGRESS
                  || !seek_probe_calls || seek_probe_calls > 1024) failed = 1;

            seek_probe_calls = 0;
            seek_probe_end = 64;
            seek_probe_stride = 128;
            seek_probe_anchor = 65536;
            p.seek_phase = 1;
            data_transfer_window_rewind(p.dt);
            p.seek_feed_anchor = p.seek_feed_hi = seek_probe_anchor;
#ifdef PREVIEW_COUNT_READS
            retro_atomic_store_relaxed_int(&window_reads, 0);
#endif
            for (attempt = 0; attempt < 8 && p.seek_phase != 2; attempt++)
               if (!seek_pending(gfx_anim_preview_seek_step(&p, 0, false, &position)))
                  failed = 1;
            if (p.seek_phase != 2 || seek_probe_calls != 64) failed = 1;
#ifdef PREVIEW_COUNT_READS
            if (retro_atomic_load_relaxed_int(&window_reads) != 1) failed = 1;
            fprintf(stderr, "sparse seek headers: %d reads for 64 spans\n",
                  retro_atomic_load_relaxed_int(&window_reads));
#endif
            seek_probe_calls = 0;
            seek_probe_end = 4;
            seek_probe_stride = 128 * 1024;
            seek_probe_anchor = 65536;
            p.seek_phase = 1;
            data_transfer_window_rewind(p.dt);
            p.seek_feed_anchor = p.seek_feed_hi = seek_probe_anchor;
#ifdef PREVIEW_COUNT_READS
            retro_atomic_store_relaxed_int(&window_reads, 0);
            retro_atomic_store_relaxed_int(&window_bytes, 0);
#endif
            for (attempt = 0; attempt < 8 && p.seek_phase != 2; attempt++)
               if (!seek_pending(gfx_anim_preview_seek_step(&p, 0, false, &position)))
                  failed = 1;
            if (p.seek_phase != 2 || seek_probe_calls != 4) failed = 1;
#ifdef PREVIEW_COUNT_READS
            if (retro_atomic_load_relaxed_int(&window_reads) != 4
                  || retro_atomic_load_relaxed_int(&window_bytes)
                     > GFX_ANIM_PREVIEW_SEEK_AHEAD + 30) failed = 1;
            fprintf(stderr, "distant seek headers: %d bytes for 4 spans\n",
                  retro_atomic_load_relaxed_int(&window_bytes));
#endif
            seek_probe_sparse = false;
            seek_probe_wait = true;
            seek_probe_frontier = 512 * 1024;
            seek_probe_anchor = seek_probe_pin_lo = 70000;
            seek_probe_pin_hi = seek_probe_pin_lo + 32;
            seek_probe_lo = seek_probe_hi = seek_probe_avail = 0;
            if (!data_transfer_window_ensure(p.dt, seek_probe_pin_lo, seek_probe_pin_hi))
               failed = 1;
            p.seek_phase = 1;
            p.seek_feed_anchor = p.seek_feed_hi = seek_probe_anchor;
            if (gfx_anim_preview_seek_step(&p, 0, false, &position)
                  != VIDEO_PLAYER_SEEK_PROGRESS || p.seek_phase != 2)
               failed = 1;
            {
               size_t length = 0;
               const unsigned char *base = data_transfer_window_base(p.dt, &length);
               if (!base || length != p.len || base[seek_probe_pin_lo] != 0)
                  failed = 1;
            }
            seek_probe_frontier = seek_probe_pin_lo = seek_probe_pin_hi = 0;
            seek_probe_sparse = seek_probe_wait = false;
            seek_probe_end = 0;
            data_transfer_free(p.dt);
         }
         remove(path);
      }
   }
   seek_probe_boundary = 0;
   seek_probe = false;
   fprintf(stderr, "%s bounded video seek progress, waiting, error and step ceiling\n",
         failed ? "FAIL" : "PASS");
   return failed;
}

int main(int argc, char **argv)
{
   bool webm_only = argc > 2 && !strcmp(argv[1], "--prepared-webm");
   bool prepared_only = webm_only || (argc > 2 && !strcmp(argv[1], "--prepared-only"));
   int first_arg = prepared_only ? 2 : 1;
   int i, failed = argc <= first_arg;
   gfx_anim_preview_t fallback;
   if (argc == 2 && !strcmp(argv[1], "--seek-batch")) return bounded_video_seek();
   failed |= bounded_video_seek();
   if (argc == 3 && !strcmp(argv[1], "--continuous-webm"))
      return continuous_webm(argv[2], false) | continuous_webm(argv[2], true);
   memset(&fallback, 0, sizeof(fallback));
   fallback.audio_slot = -1;
   if (gfx_anim_preview_clock(&fallback, 1000) != 1000
         || gfx_anim_preview_clock(&fallback, 2000) != 2000) failed = 1;
   if (gfx_anim_preview_clock(&fallback, 1900) != 2000
         || gfx_anim_preview_clock(&fallback, 2000) != 2000) failed = 1;
   gfx_anim_preview_set_paused(&fallback, true);
   if (gfx_anim_preview_clock(&fallback, 3000) != 2000
         || gfx_anim_preview_clock(&fallback, 9000) != 2000) failed = 1;
   gfx_anim_preview_set_paused(&fallback, false);
   if (gfx_anim_preview_clock(&fallback, 10000) != 3000) failed = 1;
   for (i = first_arg; i < argc; i++)
   {
      failed |= prepared_session(argv[i], false, webm_only, 1250000);
      failed |= prepared_session(argv[i], true, webm_only, 1250000);
      if (webm_only)
      {
         failed |= prepared_session(argv[i], false, true, 3250000);
         failed |= prepared_session(argv[i], true, true, 3250000);
         continue;
      }
      failed |= bounded_aac_seek(argv[i]);
      failed |= prepared_aac(argv[i], false);
      failed |= prepared_aac(argv[i], true);
   }
   for (i = first_arg; i < argc; i++)
   {
      if (prepared_only) continue;
      failed |= run_one(argv[i], 48000);
      failed |= run_one(argv[i], 44100);
   }
   return failed;
}
