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

#include <boolean.h>
#include <audio/audio_mixer.h>

#include "../../../configuration.h"
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
struct audio_mixer_sound { void *owned_data; };
struct audio_mixer_voice { int tag; };

static audio_mixer_sound_t *new_sound_owning(void *data)
{
   audio_mixer_sound_t *s = (audio_mixer_sound_t*)calloc(1, sizeof(*s));
   if (s)
      s->owned_data = data;
   return s;
}

static audio_mixer_sound_t *new_sound(void)
{
   return new_sound_owning(NULL);
}

static audio_mixer_voice_t *new_voice(void)
{
   audio_mixer_voice_t *v = (audio_mixer_voice_t*)calloc(1, sizeof(*v));
   return v;
}

void audio_mixer_init(unsigned rate) { (void)rate; }
void audio_mixer_done(void) { }

audio_mixer_sound_t *audio_mixer_load_wav(void *buffer, size_t size,
      const char *resampler_ident, enum resampler_quality quality,
      bool want_s16)
{
   (void)buffer; (void)size; (void)resampler_ident; (void)quality;
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
   if (sound->owned_data)
      free(sound->owned_data);
   free(sound);
}

audio_mixer_voice_t *audio_mixer_play(audio_mixer_sound_t *sound,
      bool repeat, float volume, const char *resampler_ident,
      enum resampler_quality quality, audio_mixer_stop_cb_t stop_cb)
{
   (void)sound; (void)repeat; (void)volume; (void)resampler_ident;
   (void)quality; (void)stop_cb;
   return new_voice();
}

audio_mixer_voice_t *audio_mixer_play_s16(audio_mixer_sound_t *sound,
      bool repeat, int32_t gain, enum resampler_quality quality,
      audio_mixer_stop_cb_t stop_cb)
{
   (void)sound; (void)repeat; (void)gain; (void)quality; (void)stop_cb;
   return new_voice();
}

void audio_mixer_stop(audio_mixer_voice_t *voice)
{
   free(voice);
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
      unsigned latency, unsigned block_frames, bool raise_priority,
      bool prefer_fast_cores, const audio_driver_t *driver)
{
   (void)out_driver; (void)out_data; (void)device; (void)out_rate;
   (void)new_rate; (void)latency; (void)block_frames;
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
