/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2021 - Daniel De Matteis
 *  Copyright (C) 2016-2019 - Brad Parker
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

#ifndef __RETROARCH_H
#define __RETROARCH_H

#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>
#include <stdlib.h>

#include <boolean.h>
#include <retro_inline.h>
#include <retro_common_api.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <lists/string_list.h>
#include <queues/task_queue.h>
#include <queues/message_queue.h>
#ifdef HAVE_AUDIOMIXER
#include <audio/audio_mixer.h>
#endif

#include "audio/audio_defines.h"
#include "gfx/video_driver.h"

#include "core.h"

#include "runloop.h"
#include "retroarch_types.h"

RETRO_BEGIN_DECLS

#define MEASURE_FRAME_TIME_SAMPLES_COUNT (2 * 1024)

#define RETRO_ENVIRONMENT_RETROARCH_START_BLOCK 0x800000

#define RETRO_ENVIRONMENT_SET_SAVE_STATE_IN_BACKGROUND (2 | RETRO_ENVIRONMENT_RETROARCH_START_BLOCK)
                                            /* bool * --
                                            * Boolean value that tells the front end to save states in the
                                            * background or not.
                                            */

#define RETRO_ENVIRONMENT_GET_CLEAR_ALL_THREAD_WAITS_CB (3 | RETRO_ENVIRONMENT_RETROARCH_START_BLOCK)
                                            /* retro_environment_t * --
                                            * Provides the callback to the frontend method which will cancel
                                            * all currently waiting threads.  Used when coordination is needed
                                            * between the core and the frontend to gracefully stop all threads.
                                            */

#define RETRO_ENVIRONMENT_POLL_TYPE_OVERRIDE (4 | RETRO_ENVIRONMENT_RETROARCH_START_BLOCK)
                                            /* unsigned * --
                                            * Tells the frontend to override the poll type behavior. 
                                            * Allows the frontend to influence the polling behavior of the
                                            * frontend.
                                            *
                                            * Will be unset when retro_unload_game is called.
                                            *
                                            * 0 - Don't Care, no changes, frontend still determines polling type behavior.
                                            * 1 - Early
                                            * 2 - Normal
                                            * 3 - Late
                                            */

bool retroarch_ctl(enum rarch_ctl_state state, void *data);

int retroarch_get_capabilities(enum rarch_capabilities type,
      char *s, size_t len);

void retroarch_override_setting_set(enum rarch_override_setting enum_idx, void *data);

void retroarch_override_setting_unset(enum rarch_override_setting enum_idx, void *data);

bool retroarch_override_setting_is_set(enum rarch_override_setting enum_idx, void *data);

bool retroarch_is_forced_fullscreen(void);

void retroarch_set_current_core_type(
      enum rarch_core_type type, bool explicitly_set);

const char* retroarch_get_shader_preset(void);

bool retroarch_is_switching_display_mode(void);

/**
 * retroarch_main_init:
 * @argc                 : Count of (commandline) arguments.
 * @argv                 : (Commandline) arguments.
 *
 * Initializes the program.
 *
 * Returns: 1 (true) on success, otherwise false (0) if there was an error.
 **/
bool retroarch_main_init(int argc, char *argv[]);

bool retroarch_main_quit(void);

global_t *global_get_ptr(void);

content_state_t *content_state_get_ptr(void);

unsigned content_get_subsystem_rom_id(void);

int content_get_subsystem(void);

/**
 * runloop_iterate:
 *
 * Run Libretro core in RetroArch for one frame.
 *
 * Returns: 0 on successful run,
 * Returns 1 if we have to wait until button input in order
 * to wake up the loop.
 * Returns -1 if we forcibly quit out of the
 * RetroArch iteration loop.
 **/
int runloop_iterate(void);

void runloop_msg_queue_push(const char *msg,
      unsigned prio, unsigned duration,
      bool flush,
      char *title,
      enum message_queue_icon icon, enum message_queue_category category);

void retroarch_menu_running(void);

void retroarch_menu_running_finished(bool quit);

enum retro_language rarch_get_language_from_iso(const char *lang);

void rarch_favorites_init(void);

void rarch_favorites_deinit(void);

/* Audio */

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

bool audio_driver_enable_callback(void);

bool audio_driver_disable_callback(void);

/**
 * config_get_audio_driver_options:
 *
 * Get an enumerated list of all audio driver names, separated by '|'.
 *
 * Returns: string listing of all audio driver names, separated by '|'.
 **/
const char* config_get_audio_driver_options(void);

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

/* Recording */

typedef struct record_driver
{
   void *(*init)(const struct record_params *params);
   void  (*free)(void *data);
   bool  (*push_video)(void *data, const struct record_video_data *video_data);
   bool  (*push_audio)(void *data, const struct record_audio_data *audio_data);
   bool  (*finalize)(void *data);
   const char *ident;
} record_driver_t;

extern const record_driver_t record_ffmpeg;

/**
 * config_get_record_driver_options:
 *
 * Get an enumerated list of all record driver names, separated by '|'.
 *
 * Returns: string listing of all record driver names, separated by '|'.
 **/
const char* config_get_record_driver_options(void);

bool recording_is_enabled(void);

void streaming_set_state(bool state);

bool streaming_is_enabled(void);

void recording_driver_update_streaming_url(void);

/* Video */

#ifdef HAVE_OVERLAY
#include "input/input_overlay.h"
#endif

/* BSV Movie */

void bsv_movie_frame_rewind(void);

/* Camera */

typedef struct camera_driver
{
   /* FIXME: params for initialization - queries for resolution,
    * framerate, color format which might or might not be honored. */
   void *(*init)(const char *device, uint64_t buffer_types,
         unsigned width, unsigned height);

   void (*free)(void *data);

   bool (*start)(void *data);
   void (*stop)(void *data);

   /* Polls the camera driver.
    * Will call the appropriate callback if a new frame is ready.
    * Returns true if a new frame was handled. */
   bool (*poll)(void *data,
         retro_camera_frame_raw_framebuffer_t frame_raw_cb,
         retro_camera_frame_opengl_texture_t frame_gl_cb);

   const char *ident;
} camera_driver_t;

extern camera_driver_t camera_v4l2;
extern camera_driver_t camera_android;
extern camera_driver_t camera_rwebcam;
extern camera_driver_t camera_avfoundation;

/**
 * config_get_camera_driver_options:
 *
 * Get an enumerated list of all camera driver names,
 * separated by '|'.
 *
 * Returns: string listing of all camera driver names,
 * separated by '|'.
 **/
const char* config_get_camera_driver_options(void);

bool gfx_widgets_ready(void);

unsigned int retroarch_get_rotation(void);

void retroarch_init_task_queue(void);

/* Human readable order of input binds */
static const unsigned input_config_bind_order[] = {
   RETRO_DEVICE_ID_JOYPAD_UP,
   RETRO_DEVICE_ID_JOYPAD_DOWN,
   RETRO_DEVICE_ID_JOYPAD_LEFT,
   RETRO_DEVICE_ID_JOYPAD_RIGHT,
   RETRO_DEVICE_ID_JOYPAD_A,
   RETRO_DEVICE_ID_JOYPAD_B,
   RETRO_DEVICE_ID_JOYPAD_X,
   RETRO_DEVICE_ID_JOYPAD_Y,
   RETRO_DEVICE_ID_JOYPAD_SELECT,
   RETRO_DEVICE_ID_JOYPAD_START,
   RETRO_DEVICE_ID_JOYPAD_L,
   RETRO_DEVICE_ID_JOYPAD_R,
   RETRO_DEVICE_ID_JOYPAD_L2,
   RETRO_DEVICE_ID_JOYPAD_R2,
   RETRO_DEVICE_ID_JOYPAD_L3,
   RETRO_DEVICE_ID_JOYPAD_R3,
   19, /* Left Analog Up */
   18, /* Left Analog Down */
   17, /* Left Analog Left */
   16, /* Left Analog Right */
   23, /* Right Analog Up */
   22, /* Right Analog Down */
   21, /* Right Analog Left */
   20, /* Right Analog Right */
};

/* Creates folder and core options stub file for subsequent runs */
bool core_options_create_override(bool game_specific);
bool core_options_remove_override(bool game_specific);
void core_options_reset(void);
void core_options_flush(void);

typedef enum apple_view_type
{
   APPLE_VIEW_TYPE_NONE = 0,
   APPLE_VIEW_TYPE_OPENGL_ES,
   APPLE_VIEW_TYPE_OPENGL,
   APPLE_VIEW_TYPE_VULKAN,
   APPLE_VIEW_TYPE_METAL
} apple_view_type_t;

bool retroarch_get_current_savestate_path(char *path, size_t len);

runloop_state_t *runloop_state_get_ptr(void);

RETRO_END_DECLS

#endif
