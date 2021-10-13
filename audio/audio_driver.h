/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __AUDIO_DRIVER__H
#define __AUDIO_DRIVER__H

#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
#include <sys/types.h>

#include <boolean.h>
#include <retro_common_api.h>
#include <retro_inline.h>
#include <libretro.h>
#include <retro_miscellaneous.h>

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif /* HAVE_CONFIG_H */

#ifdef HAVE_DSP_FILTER
#include <audio/dsp_filter.h>
#endif
#ifdef HAVE_AUDIOMIXER
#include <audio/audio_mixer.h>
#endif
#include <audio/audio_resampler.h>

#include "audio_defines.h"

#define AUDIO_BUFFER_FREE_SAMPLES_COUNT (8 * 1024)

RETRO_BEGIN_DECLS

#ifdef HAVE_AUDIOMIXER
typedef struct audio_mixer_stream
{
   audio_mixer_sound_t *handle;
   audio_mixer_voice_t *voice;
   audio_mixer_stop_cb_t stop_cb;
   void *buf;
   char *name;
   size_t bufsize;
   float volume;
   enum audio_mixer_stream_type  stream_type;
   enum audio_mixer_type type;
   enum audio_mixer_state state;
} audio_mixer_stream_t;

typedef struct audio_mixer_stream_params
{
   void *buf;
   char *basename;
   audio_mixer_stop_cb_t cb;
   size_t bufsize;
   unsigned slot_selection_idx;
   float volume;
   enum audio_mixer_slot_selection_type slot_selection_type;
   enum audio_mixer_stream_type  stream_type;
   enum audio_mixer_type  type;
   enum audio_mixer_state state;
} audio_mixer_stream_params_t;
#endif

typedef struct audio_driver
{
   /* Creates and initializes handle to audio driver.
    *
    * Returns: audio driver handle on success, otherwise NULL.
    **/
   void *(*init)(const char *device, unsigned rate,
         unsigned latency, unsigned block_frames, unsigned *new_rate);

   /*
    * @data         : Pointer to audio data handle.
    * @buf          : Audio buffer data.
    * @size         : Size of audio buffer.
    *
    * Write samples to audio driver.
    *
    * Write data in buffer to audio driver.
    * A frame here is defined as one combined sample of left and right
    * channels. (I.e. 44.1kHz, 16-bit stereo has 88.2k samples/s, and
    * 44.1k frames/s.)
    *
    * Samples are interleaved in format LRLRLRLRLR ...
    * If the driver returns true in use_float(), a floating point
    * format will be used, with range [-1.0, 1.0].
    * If not, signed 16-bit samples in native byte ordering will be used.
    *
    * This function returns the number of frames successfully written.
    * If an error occurs, -1 should be returned.
    * Note that non-blocking behavior that cannot write at this time
    * should return 0 as returning -1 will terminate the driver.
    *
    * Unless said otherwise with set_nonblock_state(), all writes
    * are blocking, and it should block till it has written all frames.
    */
   ssize_t (*write)(void *data, const void *buf, size_t size);

   /* Temporarily pauses the audio driver. */
   bool (*stop)(void *data);

   /* Resumes audio driver from the paused state. */
   bool (*start)(void *data, bool is_shutdown);

   /* Is the audio driver currently running? */
   bool (*alive)(void *data);

   /* Should we care about blocking in audio thread? Fast forwarding.
    *
    * If state is true, nonblocking operation is assumed.
    * This is typically used for fast-forwarding. If driver cannot
    * implement nonblocking writes, this can be disregarded, but should
    * log a message to stderr.
    * */
   void (*set_nonblock_state)(void *data, bool toggle);

   /* Stops and frees driver data. */
   void (*free)(void *data);

   /* Defines if driver will take standard floating point samples,
    * or int16_t samples.
    *
    * If true is returned, the audio driver is capable of using
    * floating point data. This will likely increase performance as the
    * resampler unit uses floating point. The sample range is
    * [-1.0, 1.0].
    * */
   bool (*use_float)(void *data);

   /* Human-readable identifier. */
   const char *ident;

   /* Optional. Get audio device list (allocates, caller has to free this) */
   void *(*device_list_new)(void *data);

   /* Optional. Frees audio device list */
   void (*device_list_free)(void *data, void *data2);

   /* Optional. */
   size_t (*write_avail)(void *data);

   size_t (*buffer_size)(void *data);
} audio_driver_t;

typedef struct
{
   double source_ratio_original;
   double source_ratio_current;

   uint64_t free_samples_count;

   struct string_list *devices_list;
   float  *output_samples_buf;
#ifdef HAVE_REWIND
   int16_t *rewind_buf;
#endif
   int16_t *output_samples_conv_buf;
#ifdef HAVE_DSP_FILTER
   retro_dsp_filter_t *dsp;
#endif
   const retro_resampler_t *resampler;

   void *resampler_data;
   const audio_driver_t *current_audio;
   void *context_audio_data;
   float *input_data;
#ifdef HAVE_AUDIOMIXER
   struct audio_mixer_stream
      mixer_streams[AUDIO_MIXER_MAX_SYSTEM_STREAMS];
#endif
   struct retro_audio_callback callback;                 /* ptr alignment */
                                                         /* ptr alignment */
   size_t chunk_size;
   size_t chunk_nonblock_size;
   size_t chunk_block_size;

#ifdef HAVE_REWIND
   size_t rewind_ptr;
   size_t rewind_size;
#endif
   size_t buffer_size;
   size_t data_ptr;

   unsigned free_samples_buf[
      AUDIO_BUFFER_FREE_SAMPLES_COUNT];

#ifdef HAVE_AUDIOMIXER
   float mixer_volume_gain;
#endif

   float rate_control_delta;
   float input;
   float volume_gain;

   enum resampler_quality resampler_quality;

   char resampler_ident[64];

   bool active;
   bool control;
   bool mute_enable;
   bool use_float;
   bool suspended;
#ifdef HAVE_AUDIOMIXER
   bool mixer_mute_enable;
   bool mixer_active;
#endif
#ifdef HAVE_RUNAHEAD
   bool hard_disable;
#endif
} audio_driver_state_t;

bool audio_driver_enable_callback(void);

bool audio_driver_disable_callback(void);

bool audio_driver_mixer_extension_supported(const char *ext);

void audio_driver_dsp_filter_free(void);

bool audio_driver_dsp_filter_init(const char *device);

void audio_driver_set_buffer_size(size_t bufsize);

bool audio_driver_get_devices_list(void **ptr);

void audio_driver_setup_rewind(void);

bool audio_driver_callback(void);

bool audio_driver_has_callback(void);

void audio_driver_frame_is_reverse(void);

void audio_set_float(enum audio_action action, float val);

float *audio_get_float_ptr(enum audio_action action);

bool *audio_get_bool_ptr(enum audio_action action);

#ifdef HAVE_AUDIOMIXER
audio_mixer_stream_t *audio_driver_mixer_get_stream(unsigned i);

bool audio_driver_mixer_add_stream(audio_mixer_stream_params_t *params);

void audio_driver_mixer_play_stream(unsigned i);

void audio_driver_mixer_play_menu_sound(unsigned i);

void audio_driver_mixer_play_menu_sound_looped(unsigned i);

void audio_driver_mixer_play_stream_sequential(unsigned i);

void audio_driver_mixer_play_stream_looped(unsigned i);

void audio_driver_mixer_stop_stream(unsigned i);

float audio_driver_mixer_get_stream_volume(unsigned i);

void audio_driver_mixer_set_stream_volume(unsigned i, float vol);

void audio_driver_mixer_remove_stream(unsigned i);

enum audio_mixer_state audio_driver_mixer_get_stream_state(unsigned i);

const char *audio_driver_mixer_get_stream_name(unsigned i);

void audio_driver_load_system_sounds(void);

#endif

bool audio_driver_start(bool is_shutdown);

bool audio_driver_stop(void);

#ifdef HAVE_TRANSLATE
/* TODO/FIXME - Doesn't currently work.  Fix this. */
bool audio_driver_is_ai_service_speech_running(void);
#endif

extern audio_driver_t audio_rsound;
extern audio_driver_t audio_audioio;
extern audio_driver_t audio_oss;
extern audio_driver_t audio_alsa;
extern audio_driver_t audio_alsathread;
extern audio_driver_t audio_tinyalsa;
extern audio_driver_t audio_roar;
extern audio_driver_t audio_openal;
extern audio_driver_t audio_opensl;
extern audio_driver_t audio_jack;
extern audio_driver_t audio_sdl;
extern audio_driver_t audio_xa;
extern audio_driver_t audio_pulse;
extern audio_driver_t audio_dsound;
extern audio_driver_t audio_wasapi;
extern audio_driver_t audio_coreaudio;
extern audio_driver_t audio_coreaudio3;
extern audio_driver_t audio_xenon360;
extern audio_driver_t audio_ps3;
extern audio_driver_t audio_gx;
extern audio_driver_t audio_ax;
extern audio_driver_t audio_psp;
extern audio_driver_t audio_ps2;
extern audio_driver_t audio_ctr_csnd;
extern audio_driver_t audio_ctr_dsp;
#ifdef HAVE_THREADS
extern audio_driver_t audio_ctr_dsp_thread;
#endif
extern audio_driver_t audio_switch;
extern audio_driver_t audio_switch_thread;
extern audio_driver_t audio_switch_libnx_audren;
extern audio_driver_t audio_switch_libnx_audren_thread;
extern audio_driver_t audio_rwebaudio;

audio_driver_state_t *audio_state_get_ptr(void);

extern audio_driver_t *audio_drivers[];

/**
 * audio_compute_buffer_statistics:
 *
 * Computes audio buffer statistics.
 *
 **/
bool audio_compute_buffer_statistics(audio_statistics_t *stats);

float audio_driver_monitor_adjust_system_rates(
      double input_sample_rate,
      double input_fps,
      float video_refresh_rate,
      unsigned video_swap_interval,
      float audio_max_timing_skew);

bool audio_driver_init_internal(
      void *settings_data,
      bool audio_cb_inited);

bool audio_driver_deinit(void *settings_data);

bool audio_driver_find_driver(
      void *settings_data,
      const char *prefix,
      bool verbosity_enabled);

/**
 * audio_driver_sample:
 * @left                 : value of the left audio channel.
 * @right                : value of the right audio channel.
 *
 * Audio sample render callback function.
 **/
void audio_driver_sample(int16_t left, int16_t right);

/**
 * audio_driver_sample_batch:
 * @data                 : pointer to audio buffer.
 * @frames               : amount of audio frames to push.
 *
 * Batched audio sample render callback function.
 *
 * Returns: amount of frames sampled. Will be equal to @frames
 * unless @frames exceeds (AUDIO_CHUNK_SIZE_NONBLOCKING / 2).
 **/
size_t audio_driver_sample_batch(const int16_t *data, size_t frames);

#ifdef HAVE_REWIND
/**
 * audio_driver_sample_rewind:
 * @left                 : value of the left audio channel.
 * @right                : value of the right audio channel.
 *
 * Audio sample render callback function (rewind version).
 * This callback function will be used instead of
 * audio_driver_sample when rewinding is activated.
 **/
void audio_driver_sample_rewind(int16_t left, int16_t right);

/**
 * audio_driver_sample_batch_rewind:
 * @data                 : pointer to audio buffer.
 * @frames               : amount of audio frames to push.
 *
 * Batched audio sample render callback function (rewind version).
 *
 * This callback function will be used instead of
 * audio_driver_sample_batch when rewinding is activated.
 *
 * Returns: amount of frames sampled. Will be equal to @frames
 * unless @frames exceeds (AUDIO_CHUNK_SIZE_NONBLOCKING / 2).
 **/
size_t audio_driver_sample_batch_rewind(
      const int16_t *data, size_t frames);
#endif

#ifdef HAVE_MENU
void audio_driver_menu_sample(void);
#endif

RETRO_END_DECLS

#endif /* __AUDIO_DRIVER__H */
