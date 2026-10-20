/* Drive the real preview feeder and mixer across oversized video packets. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <audio/audio_mixer.h>
#include <formats/rmp4.h>
#include <formats/data_transfer.h>
#include "../../../gfx/gfx_anim_preview.h"
#include "../../../audio/audio_driver.h"
#include "../../../configuration.h"

#define FRAMES 800

static settings_t settings;
static audio_mixer_sound_t *sound;
static audio_mixer_voice_t *voice;
static unsigned repeats, finishes;

settings_t *config_get_ptr(void) { return &settings; }

size_t image_transfer_anim_stream_media_floor(void *stream,
      enum image_type_enum type)
{ (void)type; return rmp4_media_floor((rmp4_t*)stream); }

int64_t image_transfer_anim_stream_duration_ns(void *stream,
      enum image_type_enum type)
{ (void)type; return rmp4_duration_ns((rmp4_t*)stream); }

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
void *image_transfer_anim_stream_new_avail(void *b, size_t n, size_t a,
      enum image_type_enum t, int *more, size_t *lo, size_t *hi)
{ (void)b; (void)n; (void)a; (void)t; (void)more; (void)lo; (void)hi; abort(); return NULL; }
void image_transfer_anim_stream_free(void *s, enum image_type_enum t)
{ (void)s; (void)t; abort(); }
size_t image_transfer_anim_stream_consumed(void *s, enum image_type_enum t)
{ (void)s; (void)t; abort(); return 0; }
void image_transfer_anim_stream_next_span(void *s, enum image_type_enum t,
      size_t *lo, size_t *hi)
{ (void)s; (void)t; (void)lo; (void)hi; abort(); }
void image_transfer_anim_stream_set_avail(void *s, enum image_type_enum t, size_t a)
{ (void)s; (void)t; (void)a; abort(); }
const uint32_t *image_transfer_anim_stream_next(void *s,
      enum image_type_enum t, int *ms)
{ (void)s; (void)t; (void)ms; abort(); return NULL; }
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

int main(int argc, char **argv)
{
   int i, failed = argc < 2;
   gfx_anim_preview_t fallback;
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
   for (i = 1; i < argc; i++)
   {
      failed |= run_one(argv[i], 48000);
      failed |= run_one(argv[i], 44100);
   }
   return failed;
}
