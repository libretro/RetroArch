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
#include <stddef.h>
#include <sys/types.h>

#include <boolean.h>
#include <audio/audio_mixer.h>
#include <audio/audio_resampler.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

#define AUDIO_CHUNK_SIZE_BLOCKING      512

/* So we don't get complete line-noise when fast-forwarding audio. */
#define AUDIO_CHUNK_SIZE_NONBLOCKING   2048

#define AUDIO_MAX_RATIO                16

#define AUDIO_MIXER_MAX_STREAMS        16

enum audio_action
{
   AUDIO_ACTION_NONE = 0,
   AUDIO_ACTION_RATE_CONTROL_DELTA,
   AUDIO_ACTION_MIXER_MUTE_ENABLE,
   AUDIO_ACTION_MUTE_ENABLE,
   AUDIO_ACTION_VOLUME_GAIN,
   AUDIO_ACTION_MIXER_VOLUME_GAIN,
   AUDIO_ACTION_MIXER
};

enum audio_mixer_state
{
   AUDIO_STREAM_STATE_NONE = 0,
   AUDIO_STREAM_STATE_STOPPED,
   AUDIO_STREAM_STATE_PLAYING,
   AUDIO_STREAM_STATE_PLAYING_LOOPED,
   AUDIO_STREAM_STATE_PLAYING_SEQUENTIAL
};

typedef struct audio_mixer_stream
{
   audio_mixer_sound_t *handle;
   audio_mixer_voice_t *voice;
   audio_mixer_stop_cb_t stop_cb;
   enum audio_mixer_state state;
   float volume;
   void *buf;
   char *name;
   size_t bufsize;
} audio_mixer_stream_t;

typedef struct audio_statistics
{
   float average_buffer_saturation;
   float std_deviation_percentage;
   float close_to_underrun;
   float close_to_blocking;
   unsigned samples;
} audio_statistics_t;

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

typedef struct audio_mixer_stream_params
{
   float volume;
   enum audio_mixer_type  type;
   enum audio_mixer_state state;
   void *buf;
   char *basename;
   size_t bufsize;
   audio_mixer_stop_cb_t cb;
} audio_mixer_stream_params_t;

void audio_driver_destroy_data(void);

void audio_driver_set_own_driver(void);

void audio_driver_unset_own_driver(void);

void audio_driver_suspend(void);

bool audio_driver_is_suspended(void);

void audio_driver_resume(void);

void audio_driver_set_active(void);

bool audio_driver_is_active(void);

void audio_driver_destroy(void);

void audio_driver_deinit_resampler(void);

bool audio_driver_free_devices_list(void);

bool audio_driver_new_devices_list(void);

bool audio_driver_enable_callback(void);

bool audio_driver_disable_callback(void);

/**
 * audio_driver_find_handle:
 * @index              : index of driver to get handle to.
 *
 * Returns: handle to audio driver at index. Can be NULL
 * if nothing found.
 **/
const void *audio_driver_find_handle(int index);

/**
 * audio_driver_find_ident:
 * @index              : index of driver to get handle to.
 *
 * Returns: Human-readable identifier of audio driver at index. Can be NULL
 * if nothing found.
 **/
const char *audio_driver_find_ident(int index);

void audio_driver_set_nonblocking_state(bool enable);

/**
 * config_get_audio_driver_options:
 *
 * Get an enumerated list of all audio driver names, separated by '|'.
 *
 * Returns: string listing of all audio driver names, separated by '|'.
 **/
const char* config_get_audio_driver_options(void);

void audio_driver_sample(int16_t left, int16_t right);

size_t audio_driver_sample_batch(const int16_t *data, size_t frames);

void audio_driver_sample_rewind(int16_t left, int16_t right);

size_t audio_driver_sample_batch_rewind(const int16_t *data, size_t frames);

bool audio_driver_mixer_extension_supported(const char *ext);

void audio_driver_dsp_filter_free(void);

void audio_driver_dsp_filter_init(const char *device);

void audio_driver_set_buffer_size(size_t bufsize);

bool audio_driver_get_devices_list(void **ptr);

void audio_driver_setup_rewind(void);

void audio_driver_monitor_adjust_system_rates(void);

bool audio_driver_set_callback(const void *data);

bool audio_driver_callback(void);

bool audio_driver_has_callback(void);

/* Sets audio monitor rate to new value. */
void audio_driver_monitor_set_rate(void);

bool audio_driver_find_driver(void);

bool audio_driver_toggle_mute(void);

bool audio_driver_start(bool is_shutdown);

bool audio_driver_stop(void);

bool audio_driver_owns_driver(void);

void audio_driver_unset_callback(void);

void audio_driver_frame_is_reverse(void);

void audio_set_float(enum audio_action action, float val);

void audio_set_bool(enum audio_action action, bool val);

void audio_unset_bool(enum audio_action action, bool val);

float *audio_get_float_ptr(enum audio_action action);

bool *audio_get_bool_ptr(enum audio_action action);

bool audio_driver_deinit(void);

bool audio_driver_init(void);

void audio_driver_menu_sample(void);

audio_mixer_stream_t *audio_driver_mixer_get_stream(unsigned i);

bool audio_driver_mixer_add_stream(audio_mixer_stream_params_t *params);

void audio_driver_mixer_play_stream(unsigned i);

void audio_driver_mixer_play_stream_sequential(unsigned i);

void audio_driver_mixer_play_stream_looped(unsigned i);

void audio_driver_mixer_stop_stream(unsigned i);

float audio_driver_mixer_get_stream_volume(unsigned i);

void audio_driver_mixer_set_stream_volume(unsigned i, float vol);

void audio_driver_mixer_remove_stream(unsigned i);

enum resampler_quality audio_driver_get_resampler_quality(void);

enum audio_mixer_state audio_driver_mixer_get_stream_state(unsigned i);

const char *audio_driver_mixer_get_stream_name(unsigned i);

bool compute_audio_buffer_statistics(audio_statistics_t *stats);

extern audio_driver_t audio_rsound;
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
extern audio_driver_t audio_xenon360;
extern audio_driver_t audio_ps3;
extern audio_driver_t audio_gx;
extern audio_driver_t audio_ax;
extern audio_driver_t audio_psp;
extern audio_driver_t audio_ctr_csnd;
extern audio_driver_t audio_ctr_dsp;
extern audio_driver_t audio_switch;
extern audio_driver_t audio_switch_thread;
extern audio_driver_t audio_rwebaudio;
extern audio_driver_t audio_null;

RETRO_END_DECLS

#endif
