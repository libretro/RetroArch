/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2016 - Daniel De Matteis
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
#include <stdlib.h>

#include <retro_common_api.h>
#include <boolean.h>

#include <queues/task_queue.h>
#include <queues/message_queue.h>
#include <audio/audio_mixer.h>

#include "audio/audio_defines.h"

#include "core_type.h"
#include "core.h"

#ifdef HAVE_MENU
#include "menu/menu_defines.h"
#endif

RETRO_BEGIN_DECLS

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

enum rarch_ctl_state
{
   RARCH_CTL_NONE = 0,

   /* Initialize all drivers. */
   RARCH_CTL_INIT,

   /* Deinitializes RetroArch. */
   RARCH_CTL_MAIN_DEINIT,

   RARCH_CTL_IS_INITED,

   RARCH_CTL_IS_DUMMY_CORE,

   RARCH_CTL_PREINIT,

   RARCH_CTL_DESTROY,

   RARCH_CTL_IS_BPS_PREF,
   RARCH_CTL_UNSET_BPS_PREF,

   RARCH_CTL_IS_PATCH_BLOCKED,
   RARCH_CTL_SET_PATCH_BLOCKED,
   RARCH_CTL_UNSET_PATCH_BLOCKED,

   RARCH_CTL_IS_UPS_PREF,
   RARCH_CTL_UNSET_UPS_PREF,

   RARCH_CTL_IS_IPS_PREF,
   RARCH_CTL_UNSET_IPS_PREF,

   RARCH_CTL_IS_SRAM_USED,
   RARCH_CTL_SET_SRAM_ENABLE,
   RARCH_CTL_SET_SRAM_ENABLE_FORCE,
   RARCH_CTL_UNSET_SRAM_ENABLE,

   RARCH_CTL_IS_SRAM_LOAD_DISABLED,
   RARCH_CTL_IS_SRAM_SAVE_DISABLED,

   /* Block config read */
   RARCH_CTL_SET_BLOCK_CONFIG_READ,
   RARCH_CTL_UNSET_BLOCK_CONFIG_READ,
   RARCH_CTL_IS_BLOCK_CONFIG_READ,

   /* Username */
   RARCH_CTL_HAS_SET_USERNAME,
   RARCH_CTL_USERNAME_SET,
   RARCH_CTL_USERNAME_UNSET,

   RARCH_CTL_SET_FRAME_LIMIT,

   RARCH_CTL_TASK_INIT,

   RARCH_CTL_FRAME_TIME_FREE,
   RARCH_CTL_SET_FRAME_TIME_LAST,
   RARCH_CTL_SET_FRAME_TIME,

   RARCH_CTL_IS_IDLE,
   RARCH_CTL_SET_IDLE,

   RARCH_CTL_GET_WINDOWED_SCALE,
   RARCH_CTL_SET_WINDOWED_SCALE,

   RARCH_CTL_IS_OVERRIDES_ACTIVE,
   RARCH_CTL_SET_OVERRIDES_ACTIVE,
   RARCH_CTL_UNSET_OVERRIDES_ACTIVE,

   RARCH_CTL_IS_REMAPS_CORE_ACTIVE,
   RARCH_CTL_SET_REMAPS_CORE_ACTIVE,
   RARCH_CTL_UNSET_REMAPS_CORE_ACTIVE,

   RARCH_CTL_IS_REMAPS_CONTENT_DIR_ACTIVE,
   RARCH_CTL_SET_REMAPS_CONTENT_DIR_ACTIVE,
   RARCH_CTL_UNSET_REMAPS_CONTENT_DIR_ACTIVE,

   RARCH_CTL_IS_REMAPS_GAME_ACTIVE,
   RARCH_CTL_SET_REMAPS_GAME_ACTIVE,
   RARCH_CTL_UNSET_REMAPS_GAME_ACTIVE,

   RARCH_CTL_IS_MISSING_BIOS,
   RARCH_CTL_SET_MISSING_BIOS,
   RARCH_CTL_UNSET_MISSING_BIOS,

   RARCH_CTL_IS_GAME_OPTIONS_ACTIVE,

   RARCH_CTL_IS_NONBLOCK_FORCED,
   RARCH_CTL_SET_NONBLOCK_FORCED,
   RARCH_CTL_UNSET_NONBLOCK_FORCED,

   RARCH_CTL_IS_PAUSED,
   RARCH_CTL_SET_PAUSED,

   RARCH_CTL_SET_CORE_SHUTDOWN,

   RARCH_CTL_SET_SHUTDOWN,
   RARCH_CTL_UNSET_SHUTDOWN,
   RARCH_CTL_IS_SHUTDOWN,

   /* Runloop state */
   RARCH_CTL_STATE_FREE,

   /* Performance counters */
   RARCH_CTL_GET_PERFCNT,
   RARCH_CTL_SET_PERFCNT_ENABLE,
   RARCH_CTL_UNSET_PERFCNT_ENABLE,
   RARCH_CTL_IS_PERFCNT_ENABLE,

   /* Key event */
   RARCH_CTL_FRONTEND_KEY_EVENT_GET,
   RARCH_CTL_UNSET_KEY_EVENT,
   RARCH_CTL_KEY_EVENT_GET,
   RARCH_CTL_DATA_DEINIT,

   /* Core options */
   RARCH_CTL_HAS_CORE_OPTIONS,
   RARCH_CTL_GET_CORE_OPTION_SIZE,
   RARCH_CTL_IS_CORE_OPTION_UPDATED,
   RARCH_CTL_CORE_OPTIONS_LIST_GET,
   RARCH_CTL_CORE_OPTION_PREV,
   RARCH_CTL_CORE_OPTION_NEXT,
   RARCH_CTL_CORE_OPTIONS_GET,
   RARCH_CTL_CORE_OPTIONS_INIT,
   RARCH_CTL_CORE_OPTIONS_DEINIT,

   /* System info */
   RARCH_CTL_SYSTEM_INFO_INIT,
   RARCH_CTL_SYSTEM_INFO_FREE,

   /* HTTP server */
   RARCH_CTL_HTTPSERVER_INIT,
   RARCH_CTL_HTTPSERVER_DESTROY,

   RARCH_CTL_CONTENT_RUNTIME_LOG_INIT,
   RARCH_CTL_CONTENT_RUNTIME_LOG_DEINIT,

   /* Camera */
   RARCH_CTL_CAMERA_SET_ACTIVE,
   RARCH_CTL_CAMERA_UNSET_ACTIVE,
   RARCH_CTL_CAMERA_SET_CB,

   /* BSV Movie */
   RARCH_CTL_BSV_MOVIE_IS_INITED,

   /* Location */
   RARCH_CTL_LOCATION_SET_ACTIVE,
   RARCH_CTL_LOCATION_UNSET_ACTIVE
};

enum rarch_capabilities
{
   RARCH_CAPABILITIES_NONE = 0,
   RARCH_CAPABILITIES_CPU,
   RARCH_CAPABILITIES_COMPILER
};

enum rarch_override_setting
{
   RARCH_OVERRIDE_SETTING_NONE = 0,
   RARCH_OVERRIDE_SETTING_LIBRETRO,
   RARCH_OVERRIDE_SETTING_VERBOSITY,
   RARCH_OVERRIDE_SETTING_LIBRETRO_DIRECTORY,
   RARCH_OVERRIDE_SETTING_SAVE_PATH,
   RARCH_OVERRIDE_SETTING_STATE_PATH,
   RARCH_OVERRIDE_SETTING_NETPLAY_MODE,
   RARCH_OVERRIDE_SETTING_NETPLAY_IP_ADDRESS,
   RARCH_OVERRIDE_SETTING_NETPLAY_IP_PORT,
   RARCH_OVERRIDE_SETTING_NETPLAY_STATELESS_MODE,
   RARCH_OVERRIDE_SETTING_NETPLAY_CHECK_FRAMES,
   RARCH_OVERRIDE_SETTING_UPS_PREF,
   RARCH_OVERRIDE_SETTING_BPS_PREF,
   RARCH_OVERRIDE_SETTING_IPS_PREF,
   RARCH_OVERRIDE_SETTING_LIBRETRO_DEVICE,
   RARCH_OVERRIDE_SETTING_LOG_TO_FILE,
   RARCH_OVERRIDE_SETTING_LAST
};

enum runloop_action
{
   RUNLOOP_ACTION_NONE = 0,
   RUNLOOP_ACTION_AUTOSAVE
};

struct rarch_main_wrap
{
   char **argv;
   const char *content_path;
   const char *sram_path;
   const char *state_path;
   const char *config_path;
   const char *libretro_path;
   bool verbose;
   bool no_content;
   bool touched;
   int argc;
};

typedef struct rarch_resolution
{
   unsigned idx;
   unsigned id;
} rarch_resolution_t;

/* All run-time- / command line flag-related globals go here. */

typedef struct global
{
   struct
   {
      char savefile[8192];
      char savestate[8192];
      char cheatfile[8192];
      char ups[8192];
      char bps[8192];
      char ips[8192];
      char label[8192];
      char *remapfile;
   } name;

   /* Recording. */
   struct
   {
      bool use_output_dir;
      char path[8192];
      char config[8192];
      char output_dir[8192];
      char config_dir[8192];
      unsigned width;
      unsigned height;

      size_t gpu_width;
      size_t gpu_height;
   } record;

   /* Settings and/or global state that is specific to
    * a console-style implementation. */
   struct
   {
      bool flickerfilter_enable;
      bool softfilter_enable;

      struct
      {
         bool pal_enable;
         bool pal60_enable;
         unsigned char soft_filter_index;
         unsigned      gamma_correction;
         unsigned int  flicker_filter_index;

         struct
         {
            bool check;
            unsigned count;
            uint32_t *list;
            rarch_resolution_t current;
            rarch_resolution_t initial;
         } resolutions;
      } screen;
   } console;
   /* Settings and/or global states specific to menus */
#ifdef HAVE_MENU
   struct
   {
      retro_time_t prev_start_time ;
      retro_time_t noop_press_time ;
      retro_time_t noop_start_time  ;
      retro_time_t action_start_time  ;
      retro_time_t action_press_time ;
      enum menu_action prev_action ;
   } menu;
#endif
} global_t;

bool rarch_ctl(enum rarch_ctl_state state, void *data);

int retroarch_get_capabilities(enum rarch_capabilities type,
      char *s, size_t len);

void retroarch_override_setting_set(enum rarch_override_setting enum_idx, void *data);

void retroarch_override_setting_unset(enum rarch_override_setting enum_idx, void *data);

bool retroarch_override_setting_is_set(enum rarch_override_setting enum_idx, void *data);

bool retroarch_validate_game_options(char *s, size_t len, bool mkdir);

bool retroarch_is_forced_fullscreen(void);

void retroarch_unset_forced_fullscreen(void);

void retroarch_set_current_core_type(enum rarch_core_type type, bool explicitly_set);

void retroarch_set_shader_preset(const char* preset);

void retroarch_unset_shader_preset(void);

char* retroarch_get_shader_preset(void);

bool retroarch_is_switching_display_mode(void);

void retroarch_set_switching_display_mode(void);

void retroarch_unset_switching_display_mode(void);

/**
 * retroarch_fail:
 * @error_code  : Error code.
 * @error       : Error message to show.
 *
 * Sanely kills the program.
 **/
void retroarch_fail(int error_code, const char *error);

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
int runloop_iterate(unsigned *sleep_ms);

void runloop_task_msg_queue_push(retro_task_t *task,
      const char *msg,
      unsigned prio, unsigned duration,
      bool flush);

void runloop_msg_queue_push(const char *msg,
      unsigned prio, unsigned duration,
      bool flush,
      char *title,
      enum message_queue_icon icon, enum message_queue_category category);

bool runloop_msg_queue_pull(const char **ret);

void runloop_get_status(bool *is_paused, bool *is_idle, bool *is_slowmotion,
      bool *is_perfcnt_enable);

void runloop_set(enum runloop_action action);

void runloop_unset(enum runloop_action action);

void rarch_menu_running(void);

void rarch_menu_running_finished(void);

bool retroarch_is_on_main_thread(void);

char *get_retroarch_launch_arguments(void);

rarch_system_info_t *runloop_get_system_info(void);

struct retro_system_info *runloop_get_libretro_system_info(void);

#ifdef HAVE_THREADS
void runloop_msg_queue_lock(void);

void runloop_msg_queue_unlock(void);
#endif

void rarch_force_video_driver_fallback(const char *driver);

void rarch_core_runtime_tick(void);

void rarch_send_debug_info(void);

bool rarch_write_debug_info(void);

void rarch_get_cpu_architecture_string(char *cpu_arch_str, size_t len);

void rarch_log_file_init(void);

void rarch_log_file_deinit(void);

enum retro_language rarch_get_language_from_iso(const char *lang);

/* Audio */

typedef struct audio_mixer_stream
{
   audio_mixer_sound_t *handle;
   audio_mixer_voice_t *voice;
   audio_mixer_stop_cb_t stop_cb;
   enum audio_mixer_stream_type  stream_type;
   enum audio_mixer_type type;
   enum audio_mixer_state state;
   float volume;
   void *buf;
   char *name;
   size_t bufsize;
} audio_mixer_stream_t;

typedef struct audio_mixer_stream_params
{
   float volume;
   enum audio_mixer_slot_selection_type slot_selection_type;
   unsigned slot_selection_idx;
   enum audio_mixer_stream_type  stream_type;
   enum audio_mixer_type  type;
   enum audio_mixer_state state;
   void *buf;
   char *basename;
   size_t bufsize;
   audio_mixer_stop_cb_t cb;
} audio_mixer_stream_params_t;

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

void audio_driver_suspend(void);

bool audio_driver_is_suspended(void);

void audio_driver_resume(void);

void audio_driver_set_active(void);

bool audio_driver_is_active(void);

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

bool audio_driver_dsp_filter_init(const char *device);

void audio_driver_set_buffer_size(size_t bufsize);

bool audio_driver_get_devices_list(void **ptr);

void audio_driver_setup_rewind(void);

bool audio_driver_set_callback(const void *data);

bool audio_driver_callback(void);

bool audio_driver_has_callback(void);

bool audio_driver_toggle_mute(void);

bool audio_driver_start(bool is_shutdown);

bool audio_driver_stop(void);

void audio_driver_frame_is_reverse(void);

void audio_set_float(enum audio_action action, float val);

void audio_set_bool(enum audio_action action, bool val);

void audio_unset_bool(enum audio_action action, bool val);

float *audio_get_float_ptr(enum audio_action action);

bool *audio_get_bool_ptr(enum audio_action action);

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

bool compute_audio_buffer_statistics(audio_statistics_t *stats);

void audio_driver_load_menu_sounds(void);

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
extern audio_driver_t audio_switch;
extern audio_driver_t audio_switch_thread;
extern audio_driver_t audio_rwebaudio;
extern audio_driver_t audio_null;

/* BSV Movie */

enum rarch_movie_type
{
   RARCH_MOVIE_PLAYBACK = 0,
   RARCH_MOVIE_RECORD
};

void bsv_movie_deinit(void);

bool bsv_movie_init(void);

void bsv_movie_frame_rewind(void);

void bsv_movie_set_path(const char *path);

bool bsv_movie_get_input(int16_t *bsv_data);

void bsv_movie_set_input(int16_t *bsv_data);

bool bsv_movie_check(void);

/* Location */

enum rarch_location_ctl_state
{
   RARCH_LOCATION_CTL_NONE = 0,
};

typedef struct location_driver
{
   void *(*init)(void);
   void (*free)(void *data);

   bool (*start)(void *data);
   void (*stop)(void *data);

   bool (*get_position)(void *data, double *lat, double *lon,
         double *horiz_accuracy, double *vert_accuracy);
   void (*set_interval)(void *data, unsigned interval_msecs,
         unsigned interval_distance);
   const char *ident;
} location_driver_t;

extern location_driver_t location_corelocation;
extern location_driver_t location_android;
extern location_driver_t location_null;

/**
 * driver_location_start:
 *
 * Starts location driver interface..
 * Used by RETRO_ENVIRONMENT_GET_LOCATION_INTERFACE.
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
bool driver_location_start(void);

/**
 * driver_location_stop:
 *
 * Stops location driver interface..
 * Used by RETRO_ENVIRONMENT_GET_LOCATION_INTERFACE.
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
void driver_location_stop(void);

/**
 * driver_location_get_position:
 * @lat                : Latitude of current position.
 * @lon                : Longitude of current position.
 * @horiz_accuracy     : Horizontal accuracy.
 * @vert_accuracy      : Vertical accuracy.
 *
 * Gets current positioning information from
 * location driver interface.
 * Used by RETRO_ENVIRONMENT_GET_LOCATION_INTERFACE.
 *
 * Returns: bool (1) if successful, otherwise false (0).
 **/
bool driver_location_get_position(double *lat, double *lon,
      double *horiz_accuracy, double *vert_accuracy);

/**
 * driver_location_set_interval:
 * @interval_msecs     : Interval time in milliseconds.
 * @interval_distance  : Distance at which to update.
 *
 * Sets interval update time for location driver interface.
 * Used by RETRO_ENVIRONMENT_GET_LOCATION_INTERFACE.
 **/
void driver_location_set_interval(unsigned interval_msecs,
      unsigned interval_distance);

/**
 * config_get_location_driver_options:
 *
 * Get an enumerated list of all location driver names,
 * separated by '|'.
 *
 * Returns: string listing of all location driver names,
 * separated by '|'.
 **/
const char* config_get_location_driver_options(void);

/**
 * location_driver_find_handle:
 * @index              : index of driver to get handle to.
 *
 * Returns: handle to location driver at index. Can be NULL
 * if nothing found.
 **/
const void *location_driver_find_handle(int index);

/**
 * location_driver_find_ident:
 * @index              : index of driver to get handle to.
 *
 * Returns: Human-readable identifier of location driver at index. Can be NULL
 * if nothing found.
 **/
const char *location_driver_find_ident(int index);

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
extern camera_driver_t camera_null;

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

/**
 * camera_driver_find_handle:
 * @index              : index of driver to get handle to.
 *
 * Returns: handle to camera driver at index. Can be NULL
 * if nothing found.
 **/
const void *camera_driver_find_handle(int index);

/**
 * camera_driver_find_ident:
 * @index              : index of driver to get handle to.
 *
 * Returns: Human-readable identifier of camera driver at index. Can be NULL
 * if nothing found.
 **/
const char *camera_driver_find_ident(int index);

void driver_camera_stop(void);

bool driver_camera_start(void);

RETRO_END_DECLS

#endif
