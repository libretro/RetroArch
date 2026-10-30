/* Frontend, mixer and resampler symbols audio/audio_driver.c
 * references. The subject of this harness is the driver's own locking
 * around mixer_streams[], so the mixer stubs are just enough to make
 * load/play/stop/destroy produce distinguishable non-NULL handles and
 * voices - the state machine that add_stream and stop_stream drive
 * lives in audio_driver.c and is exercised for real.
 *
 * Signatures are copied from the tree's headers rather than guessed.
 * Anything the driver should not reach on these paths aborts instead
 * of quietly returning. */

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sched.h>

#include <boolean.h>
#include <retro_atomic.h>
#include <audio/audio_mixer.h>

#include "../../../configuration.h"
#include "../../../defaults.h"
#include "../../../record/record_driver.h"
#include "../../../runloop.h"
#include "../../../menu/menu_driver.h"
#include "../../../gfx/video_driver.h"
#include "../../../command.h"

void RARCH_LOG(const char *fmt, ...) { (void)fmt; }
void RARCH_WARN(const char *fmt, ...) { (void)fmt; }
void RARCH_DBG(const char *fmt, ...) { (void)fmt; }
void RARCH_LOG_OUTPUT(const char *fmt, ...) { (void)fmt; }

void RARCH_ERR(const char *fmt, ...)
{
   va_list ap;
   va_start(ap, fmt);
   printf("ERR: ");
   vprintf(fmt, ap);
   va_end(ap);
}

bool verbosity_is_enabled(void) { return false; }

settings_t *config_get_ptr(void)
{
   static settings_t settings;
   return &settings;
}

static runloop_state_t runloop_st;
runloop_state_t *runloop_state_get_ptr(void) { return &runloop_st; }
uint32_t runloop_get_flags(void) { return 0; }

static recording_state_t recording_st;
recording_state_t *recording_state_get_ptr(void) { return &recording_st; }

static video_driver_state_t video_st;
video_driver_state_t *video_state_get_ptr(void) { return &video_st; }

static struct menu_state menu_st;
struct menu_state *menu_state_get_ptr(void) { return &menu_st; }

/* --- mixer ----------------------------------------------------------- */

/* Distinct non-NULL cookies: the driver only ever stores and compares
 * these, and freeing them here would hide a use-after-free in the
 * driver rather than expose one, so each allocation is real. */
/* The real audio_mixer_destroy() frees the buffer the sound was loaded
 * from for the streaming types (it owns it once loaded, unless it was
 * borrowed via data_owner). The stub mirrors that ownership so leak
 * checking over these paths means something. */
struct audio_mixer_sound { void *owned_data; unsigned tag; };
/* A static pool, as the real mixer's: a play takes a free voice, and a
 * stop or a finish gives it back for the next play. Each change to a
 * voice is made holding its busy word, so the threads of the stress
 * stage can play and finish voices at once. As in the real mixer a
 * finishing play is still on its voice while its callback runs, and a
 * stopped one is off it before. */
struct audio_mixer_voice
{
   audio_mixer_sound_t  *sound;
   audio_mixer_stop_cb_t stop_cb;
   retro_atomic_int_t    serial;   /* 0 while free */
   retro_atomic_int_t    busy;
};
#define STUB_VOICES 32
static struct audio_mixer_voice stub_voices[STUB_VOICES];
static retro_atomic_int_t stub_serial;

/* Called by every load stub, which is where a sound is built: the test
 * looks at whether the state lock or the table is held while it is. */
void (*stub_load_hook)(void) = NULL;
retro_atomic_int_t stub_sound_destroys;
bool stub_play_fail = false;

static audio_mixer_sound_t *new_sound_owning(void *data)
{
   audio_mixer_sound_t *s;
   if (stub_load_hook)
      stub_load_hook();
   s = (audio_mixer_sound_t*)calloc(1, sizeof(*s));
   if (s)
      s->owned_data = data;
   return s;
}

static audio_mixer_sound_t *new_sound(void)
{
   return new_sound_owning(NULL);
}

static audio_mixer_voice_t *new_voice(audio_mixer_sound_t *sound,
      audio_mixer_stop_cb_t stop_cb)
{
   unsigned i;
   for (i = 0; i < STUB_VOICES; i++)
   {
      audio_mixer_voice_t *v = &stub_voices[i];
      if (!retro_atomic_cas_int(&v->busy, 0, 1))
         continue;
      if (!retro_atomic_load_acquire_int(&v->serial))
      {
         v->sound   = sound;
         v->stop_cb = stop_cb;
         retro_atomic_store_release_int(&v->serial,
               retro_atomic_fetch_add_int(&stub_serial, 1) + 1);
         retro_atomic_store_release_int(&v->busy, 0);
         return v;
      }
      retro_atomic_store_release_int(&v->busy, 0);
   }
   return NULL;
}

/* The mix passes over a voice someone else has; a stop waits for the
 * pass that has it, as the real mixer's owner word makes it. */
static bool stub_voice_end(audio_mixer_voice_t *voice, unsigned reason)
{
   audio_mixer_sound_t  *sound;
   audio_mixer_stop_cb_t stop_cb;
   while (!retro_atomic_cas_int(&voice->busy, 0, 1))
   {
      if (reason == AUDIO_MIXER_SOUND_FINISHED)
         return false;
      sched_yield();
   }
   if (!retro_atomic_load_acquire_int(&voice->serial))
   {
      retro_atomic_store_release_int(&voice->busy, 0);
      return false;
   }
   sound   = voice->sound;
   stop_cb = voice->stop_cb;
   if (reason == AUDIO_MIXER_SOUND_FINISHED && stop_cb)
      stop_cb(sound, reason);
   retro_atomic_store_release_int(&voice->serial, 0);
   if (reason != AUDIO_MIXER_SOUND_FINISHED && stop_cb)
      stop_cb(sound, reason);
   retro_atomic_store_release_int(&voice->busy, 0);
   return true;
}

/* What the mix does as a play runs out. */
bool stub_voice_finish(audio_mixer_voice_t *voice)
{
   return stub_voice_end(voice, AUDIO_MIXER_SOUND_FINISHED);
}

/* The mix of the stress stage: whichever voices play now finish. */
audio_mixer_voice_t *stub_voice_at(unsigned i) { return &stub_voices[i]; }
unsigned stub_voice_count(void) { return STUB_VOICES; }

void audio_mixer_sound_set_tag(audio_mixer_sound_t *sound, unsigned tag)
{
   if (sound)
      sound->tag = tag;
}

unsigned audio_mixer_sound_tag(const audio_mixer_sound_t *sound)
{
   return sound ? sound->tag : 0;
}

void audio_mixer_init(unsigned rate) { (void)rate; }
void audio_mixer_done(void) { }

/* The bytes the last WAV was built from. */
const void *stub_wav_source = NULL;

audio_mixer_sound_t *audio_mixer_load_wav(void *buffer, size_t size,
      const char *resampler_ident, enum resampler_quality quality,
      bool want_s16)
{
   stub_wav_source = buffer;
   (void)size; (void)resampler_ident; (void)quality;
   (void)want_s16;
   return new_sound();
}

audio_mixer_sound_t *audio_mixer_load_wav_stream(void *buffer,
      size_t size)
{
   (void)size;
   return new_sound_owning(buffer);
}

audio_mixer_sound_t *audio_mixer_load_ogg(void *buffer, size_t size)
{
   (void)size;
   return new_sound_owning(buffer);
}

audio_mixer_sound_t *audio_mixer_load_mod(void *buffer, size_t size)
{
   (void)size;
   return new_sound_owning(buffer);
}

void audio_mixer_destroy(audio_mixer_sound_t *sound)
{
   if (!sound)
      return;
   retro_atomic_fetch_add_int(&stub_sound_destroys, 1);
   if (sound->owned_data)
      free(sound->owned_data);
   free(sound);
}

audio_mixer_voice_t *audio_mixer_play(audio_mixer_sound_t *sound,
      bool repeat, float volume, const char *resampler_ident,
      enum resampler_quality quality, audio_mixer_stop_cb_t stop_cb)
{
   (void)repeat; (void)volume; (void)resampler_ident; (void)quality;
   return stub_play_fail ? NULL : new_voice(sound, stop_cb);
}

audio_mixer_voice_t *audio_mixer_play_s16(audio_mixer_sound_t *sound,
      bool repeat, int32_t gain, const char *resampler_ident,
      enum resampler_quality quality,
      audio_mixer_stop_cb_t stop_cb)
{
   (void)repeat; (void)gain; (void)resampler_ident; (void)quality;
   return stub_play_fail ? NULL : new_voice(sound, stop_cb);
}

void audio_mixer_stop(audio_mixer_voice_t *voice)
{
   stub_voice_end(voice, AUDIO_MIXER_SOUND_STOPPED);
}

void audio_mixer_mix(float *buffer, size_t num_frames,
      float gain_override, bool override)
{
   (void)buffer; (void)num_frames; (void)gain_override; (void)override;
}

void audio_mixer_mix_s16(int16_t *buffer, size_t num_frames,
      int32_t gain_override, bool override)
{
   (void)buffer; (void)num_frames; (void)gain_override; (void)override;
}

void audio_mixer_voice_set_volume(audio_mixer_voice_t *voice, float val)
{
   (void)voice; (void)val;
}

void audio_mixer_voice_set_avail(audio_mixer_voice_t *voice, size_t avail)
{
   (void)voice; (void)avail;
}

void audio_mixer_sound_set_avail(audio_mixer_sound_t *sound, size_t avail)
{
   (void)sound; (void)avail;
}

void audio_mixer_sound_set_data_owner(audio_mixer_sound_t *sound,
      void *owner, void (*owner_free)(void *owner))
{
   (void)sound; (void)owner; (void)owner_free;
}

size_t audio_mixer_voice_buffer_tell(audio_mixer_voice_t *voice)
{
   (void)voice;
   return 0;
}

bool audio_mixer_has_float_voices(void) { return true; }
bool audio_mixer_has_s16_voices(void) { return false; }

/* --- DSP filter ------------------------------------------------------- */

#include <audio/dsp_filter.h>

struct retro_dsp_filter { int unused; };
retro_atomic_int_t stub_dsp_frees;

retro_dsp_filter_t *retro_dsp_filter_new(const char *filter_config,
      void *string_data, float sample_rate)
{
   (void)filter_config; (void)string_data; (void)sample_rate;
   return (retro_dsp_filter_t*)calloc(1, sizeof(retro_dsp_filter_t));
}

void retro_dsp_filter_free(retro_dsp_filter_t *dsp)
{
   retro_atomic_fetch_add_int(&stub_dsp_frees, 1);
   free(dsp);
}

void retro_dsp_filter_process(retro_dsp_filter_t *dsp,
      struct retro_dsp_data *data)
{
   (void)dsp; (void)data;
}

bool retro_dsp_filter_supports_int16(retro_dsp_filter_t *dsp)
{
   (void)dsp;
   return false;
}

void retro_dsp_filter_process_int16(retro_dsp_filter_t *dsp,
      struct retro_dsp_data_int16 *data)
{
   (void)dsp; (void)data;
}

/* --- everything the locking paths must not reach --------------------- */

static void unreachable(const char *what)
{
   fprintf(stderr, "stub %s reached unexpectedly\n", what);
   abort();
}

bool command_event(enum event_command action, void *data)
{
   (void)action; (void)data;
   unreachable("command_event");
   return false;
}

void *task_push_audio_mixer_load(const char *path,
      retro_task_callback_t cb, void *user_data, bool system,
      enum audio_mixer_slot_selection_type type, int slot)
{
   (void)path; (void)cb; (void)user_data; (void)system; (void)type;
   (void)slot;
   unreachable("task_push_audio_mixer_load");
   return NULL;
}

/* --- remaining frontend references ----------------------------------- */

#include "../../../list_special.h"
#include "../../../driver.h"
#include "../../../midi_driver.h"
#include "../../../file_path_special.h"
#include "../../../msg_hash.h"
#include "../../../audio/audio_thread_wrapper.h"

const char *char_list_new_special(enum string_list_type type, void *data)
{
   (void)type; (void)data;
   return NULL;
}

int driver_find_index(const char *label, const char *drv)
{
   (void)label; (void)drv;
   return -1;
}

bool midi_driver_synth_active(void) { return false; }

bool midi_driver_render_audio(float *out, size_t frames, unsigned rate)
{
   (void)out; (void)frames; (void)rate;
   return false;
}

const char *audio_thread_wrapped_ident(void *data)
{
   (void)data;
   return NULL;
}

const audio_driver_t *audio_thread_wrapped_driver(void *data)
{
   (void)data;
   return NULL;
}

bool audio_init_thread(const audio_driver_t **out_driver, void **out_data,
      const char *device, unsigned out_rate, unsigned *new_rate,
      unsigned latency, bool raise_priority,
      bool prefer_fast_cores, const audio_driver_t *driver)
{
   (void)out_driver; (void)out_data; (void)device; (void)out_rate;
   (void)new_rate; (void)latency;
   (void)raise_priority; (void)prefer_fast_cores; (void)driver;
   unreachable("audio_init_thread");
   return false;
}

size_t fill_pathname_application_special(char *s, size_t len,
      enum application_special_type type)
{
   (void)type;
   if (len)
      s[0] = '\0';
   return 0;
}

const char *msg_hash_to_str(enum msg_hash_enums msg)
{
   (void)msg;
   return "";
}

/* retro_resampler_realloc: the real one is linked in. */

/* The platform's audio defaults: audio_driver_device_block_frames()
 * reads the device's transfer granularity from here, and no platform
 * in a harness reports one. */
struct defaults g_defaults;

/* These fixtures have no wrapper worker: it is always parked. */
bool audio_thread_parked(void *data)
{
   (void)data;
   return true;
}

/* The driver exposes transport pause; these harnesses stub mixer I/O. */
void audio_mixer_voice_set_paused(audio_mixer_voice_t *voice, bool paused)
{
   (void)voice;
   (void)paused;
}

void audio_mixer_voice_set_repeat(audio_mixer_voice_t *voice, bool repeat)
{
   (void)voice;
   (void)repeat;
}

/* No real PCM is mixed by this stand-in: a play has an identity and
 * no frames. */
bool audio_mixer_voice_clock(audio_mixer_voice_t *voice,
      size_t *frames, unsigned *rate, size_t *serial)
{
   if (frames) *frames = 0;
   if (rate) *rate = 0;
   int now = voice ? retro_atomic_load_acquire_int(&voice->serial) : 0;
   if (serial) *serial = (size_t)now;
   return now != 0;
}
