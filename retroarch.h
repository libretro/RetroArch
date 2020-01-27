/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2016 - Daniel De Matteis
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

#include <queues/task_queue.h>
#include <queues/message_queue.h>
#ifdef HAVE_AUDIOMIXER
#include <audio/audio_mixer.h>
#endif

#include "audio/audio_defines.h"
#include "gfx/video_shader_parse.h"

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

enum rarch_ctl_state
{
   RARCH_CTL_NONE = 0,

   /* Deinitializes RetroArch. */
   RARCH_CTL_MAIN_DEINIT,

   RARCH_CTL_IS_INITED,

   RARCH_CTL_IS_DUMMY_CORE,

   RARCH_CTL_IS_BPS_PREF,
   RARCH_CTL_UNSET_BPS_PREF,

   RARCH_CTL_IS_PATCH_BLOCKED,

   RARCH_CTL_IS_UPS_PREF,
   RARCH_CTL_UNSET_UPS_PREF,

   RARCH_CTL_IS_IPS_PREF,
   RARCH_CTL_UNSET_IPS_PREF,

   RARCH_CTL_IS_SRAM_USED,

   RARCH_CTL_IS_SRAM_LOAD_DISABLED,
   RARCH_CTL_IS_SRAM_SAVE_DISABLED,

   /* Block config read */
   RARCH_CTL_SET_BLOCK_CONFIG_READ,
   RARCH_CTL_UNSET_BLOCK_CONFIG_READ,
   RARCH_CTL_IS_BLOCK_CONFIG_READ,

   /* Username */
   RARCH_CTL_HAS_SET_USERNAME,

   RARCH_CTL_HAS_SET_SUBSYSTEMS,

   RARCH_CTL_TASK_INIT,

   RARCH_CTL_SET_FRAME_TIME_LAST,

   RARCH_CTL_IS_IDLE,
   RARCH_CTL_SET_IDLE,

   RARCH_CTL_SET_WINDOWED_SCALE,

   RARCH_CTL_IS_OVERRIDES_ACTIVE,

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

   RARCH_CTL_IS_PAUSED,
   RARCH_CTL_SET_PAUSED,

   RARCH_CTL_SET_SHUTDOWN,
   RARCH_CTL_IS_SHUTDOWN,

   /* Runloop state */
   RARCH_CTL_STATE_FREE,

   /* Performance counters */
   RARCH_CTL_GET_PERFCNT,
   RARCH_CTL_SET_PERFCNT_ENABLE,
   RARCH_CTL_UNSET_PERFCNT_ENABLE,
   RARCH_CTL_IS_PERFCNT_ENABLE,

   /* Key event */
   RARCH_CTL_DATA_DEINIT,

   /* Core options */
   RARCH_CTL_HAS_CORE_OPTIONS,
   RARCH_CTL_GET_CORE_OPTION_SIZE,
   RARCH_CTL_CORE_OPTIONS_LIST_GET,
   RARCH_CTL_CORE_OPTION_PREV,
   RARCH_CTL_CORE_OPTION_NEXT,
   RARCH_CTL_CORE_VARIABLES_INIT,
   RARCH_CTL_CORE_OPTIONS_INIT,
   RARCH_CTL_CORE_OPTIONS_INTL_INIT,
   RARCH_CTL_CORE_OPTIONS_DEINIT,
   RARCH_CTL_CORE_OPTIONS_DISPLAY,
   RARCH_CTL_CORE_IS_RUNNING,

   /* BSV Movie */
   RARCH_CTL_BSV_MOVIE_IS_INITED
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

void retroarch_set_current_core_type(
      enum rarch_core_type type, bool explicitly_set);

bool retroarch_apply_shader(enum rarch_shader_type type, const char *preset_path,
      bool message);

const char* retroarch_get_shader_preset(void);

bool retroarch_is_switching_display_mode(void);

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
int runloop_iterate(void);

void runloop_msg_queue_push(const char *msg,
      unsigned prio, unsigned duration,
      bool flush,
      char *title,
      enum message_queue_icon icon, enum message_queue_category category);

void runloop_get_status(bool *is_paused, bool *is_idle, bool *is_slowmotion,
      bool *is_perfcnt_enable);

void retroarch_menu_running(void);

void retroarch_menu_running_finished(bool quit);

char *get_retroarch_launch_arguments(void);

rarch_system_info_t *runloop_get_system_info(void);

struct retro_system_info *runloop_get_libretro_system_info(void);

void retroarch_force_video_driver_fallback(const char *driver);

void rarch_get_cpu_architecture_string(char *cpu_arch_str, size_t len);

void rarch_log_file_init(void);

void rarch_log_file_deinit(void);

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

void audio_driver_load_menu_sounds(void);

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
extern audio_driver_t audio_switch;
extern audio_driver_t audio_switch_thread;
extern audio_driver_t audio_switch_libnx_audren;
extern audio_driver_t audio_switch_libnx_audren_thread;
extern audio_driver_t audio_rwebaudio;

/* Recording */

enum ffemu_pix_format
{
   FFEMU_PIX_RGB565 = 0,
   FFEMU_PIX_BGR24,
   FFEMU_PIX_ARGB8888
};

enum streaming_mode
{
   STREAMING_MODE_TWITCH = 0,
   STREAMING_MODE_YOUTUBE,
   STREAMING_MODE_LOCAL,
   STREAMING_MODE_CUSTOM
};

enum record_config_type
{
   RECORD_CONFIG_TYPE_RECORDING_CUSTOM = 0,
   RECORD_CONFIG_TYPE_RECORDING_LOW_QUALITY,
   RECORD_CONFIG_TYPE_RECORDING_MED_QUALITY,
   RECORD_CONFIG_TYPE_RECORDING_HIGH_QUALITY,
   RECORD_CONFIG_TYPE_RECORDING_LOSSLESS_QUALITY,
   RECORD_CONFIG_TYPE_RECORDING_WEBM_FAST,
   RECORD_CONFIG_TYPE_RECORDING_WEBM_HIGH_QUALITY,
   RECORD_CONFIG_TYPE_RECORDING_GIF,
   RECORD_CONFIG_TYPE_RECORDING_APNG,
   RECORD_CONFIG_TYPE_STREAMING_CUSTOM,
   RECORD_CONFIG_TYPE_STREAMING_LOW_QUALITY,
   RECORD_CONFIG_TYPE_STREAMING_MED_QUALITY,
   RECORD_CONFIG_TYPE_STREAMING_HIGH_QUALITY,
   RECORD_CONFIG_TYPE_STREAMING_NETPLAY

};

/* Parameters passed to ffemu_new() */
struct record_params
{
   /* Framerate per second of input video. */
   double fps;
   /* Sample rate of input audio. */
   double samplerate;

   /* Desired output resolution. */
   unsigned out_width;
   unsigned out_height;

   /* Total size of framebuffer used in input. */
   unsigned fb_width;
   unsigned fb_height;

   /* Aspect ratio of input video. Parameters are passed to the muxer,
    * the video itself is not scaled.
    */
   float aspect_ratio;

   /* Audio channels. */
   unsigned channels;

   enum record_config_type preset;

   /* Input pixel format. */
   enum ffemu_pix_format pix_fmt;

   /* Filename to dump to. */
   const char *filename;

   /* Path to config. Optional. */
   const char *config;
};

struct record_video_data
{
   const void *data;
   unsigned width;
   unsigned height;
   int pitch;
   bool is_dupe;
};

struct record_audio_data
{
   const void *data;
   size_t frames;
};

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

bool recording_is_enabled(void);

bool streaming_is_enabled(void);

void recording_driver_update_streaming_url(void);

/* Video */

#ifdef HAVE_OVERLAY
#include "input/input_overlay.h"
#endif

#ifdef HAVE_VIDEO_LAYOUT
#include "gfx/video_layout.h"
#endif

#include "gfx/video_defines.h"
#include "gfx/video_coord_array.h"
#include "gfx/video_filter.h"

#include "input/input_driver.h"
#include "input/input_types.h"

#define RARCH_SCALE_BASE 256

#define VIDEO_SHADER_STOCK_BLEND (GFX_MAX_SHADERS - 1)
#define VIDEO_SHADER_MENU        (GFX_MAX_SHADERS - 2)
#define VIDEO_SHADER_MENU_2      (GFX_MAX_SHADERS - 3)
#define VIDEO_SHADER_MENU_3      (GFX_MAX_SHADERS - 4)
#define VIDEO_SHADER_MENU_4      (GFX_MAX_SHADERS - 5)
#define VIDEO_SHADER_MENU_5      (GFX_MAX_SHADERS - 6)
#define VIDEO_SHADER_MENU_6      (GFX_MAX_SHADERS - 7)

#if defined(_XBOX360)
#define DEFAULT_SHADER_TYPE RARCH_SHADER_HLSL
#elif defined(__PSL1GHT__) || defined(HAVE_OPENGLES2) || defined(HAVE_GLSL)
#define DEFAULT_SHADER_TYPE RARCH_SHADER_GLSL
#elif defined(__CELLOS_LV2__) || defined(HAVE_CG)
#define DEFAULT_SHADER_TYPE RARCH_SHADER_CG
#else
#define DEFAULT_SHADER_TYPE RARCH_SHADER_NONE
#endif

#ifndef MAX_EGLIMAGE_TEXTURES
#define MAX_EGLIMAGE_TEXTURES 32
#endif

#define MAX_VARIABLES 64

enum
{
   TEXTURES = 8,
   TEXTURESMASK = TEXTURES - 1
};

struct LinkInfo
{
   unsigned tex_w, tex_h;
   struct video_shader_pass *pass;
};

enum gfx_ctx_api
{
   GFX_CTX_NONE = 0,
   GFX_CTX_OPENGL_API,
   GFX_CTX_OPENGL_ES_API,
   GFX_CTX_DIRECT3D8_API,
   GFX_CTX_DIRECT3D9_API,
   GFX_CTX_DIRECT3D10_API,
   GFX_CTX_DIRECT3D11_API,
   GFX_CTX_DIRECT3D12_API,
   GFX_CTX_OPENVG_API,
   GFX_CTX_VULKAN_API,
   GFX_CTX_SIXEL_API,
   GFX_CTX_NETWORK_VIDEO_API,
   GFX_CTX_METAL_API,
   GFX_CTX_GDI_API,
   GFX_CTX_FPGA_API,
   GFX_CTX_GX_API,
   GFX_CTX_GX2_API
};

enum display_metric_types
{
   DISPLAY_METRIC_NONE = 0,
   DISPLAY_METRIC_MM_WIDTH,
   DISPLAY_METRIC_MM_HEIGHT,
   DISPLAY_METRIC_DPI,
   DISPLAY_METRIC_PIXEL_WIDTH,
   DISPLAY_METRIC_PIXEL_HEIGHT
};

enum display_flags
{
   GFX_CTX_FLAGS_NONE            = 0,
   GFX_CTX_FLAGS_GL_CORE_CONTEXT,
   GFX_CTX_FLAGS_MULTISAMPLING,
   GFX_CTX_FLAGS_CUSTOMIZABLE_SWAPCHAIN_IMAGES,
   GFX_CTX_FLAGS_HARD_SYNC,
   GFX_CTX_FLAGS_BLACK_FRAME_INSERTION,
   GFX_CTX_FLAGS_MENU_FRAME_FILTERING,
   GFX_CTX_FLAGS_ADAPTIVE_VSYNC,
   GFX_CTX_FLAGS_SHADERS_GLSL,
   GFX_CTX_FLAGS_SHADERS_CG,
   GFX_CTX_FLAGS_SHADERS_HLSL,
   GFX_CTX_FLAGS_SHADERS_SLANG,
   GFX_CTX_FLAGS_SCREENSHOTS_SUPPORTED
};

enum shader_uniform_type
{
   UNIFORM_1F = 0,
   UNIFORM_2F,
   UNIFORM_3F,
   UNIFORM_4F,
   UNIFORM_1FV,
   UNIFORM_2FV,
   UNIFORM_3FV,
   UNIFORM_4FV,
   UNIFORM_1I
};

enum shader_program_type
{
   SHADER_PROGRAM_VERTEX = 0,
   SHADER_PROGRAM_FRAGMENT,
   SHADER_PROGRAM_COMBINED
};

struct shader_program_info
{
   bool is_file;
   const char *vertex;
   const char *fragment;
   const char *combined;
   unsigned idx;
   void *data;
};

struct uniform_info
{
   bool enabled;

   int32_t location;
   int32_t count;
   unsigned type; /* shader uniform type */

   struct
   {
      enum shader_program_type type;
      const char *ident;
      uint32_t idx;
      bool add_prefix;
      bool enable;
   } lookup;

   struct
   {
      struct
      {
         intptr_t v0;
         intptr_t v1;
         intptr_t v2;
         intptr_t v3;
      } integer;

      intptr_t *integerv;

      struct
      {
         uintptr_t v0;
         uintptr_t v1;
         uintptr_t v2;
         uintptr_t v3;
      } unsigned_integer;

      uintptr_t *unsigned_integerv;

      struct
      {
         float v0;
         float v1;
         float v2;
         float v3;
      } f;

      float *floatv;
   } result;
};

typedef struct shader_backend
{
   void *(*init)(void *data, const char *path);
   void (*init_menu_shaders)(void *data);
   void (*deinit)(void *data);

   /* Set shader parameters. */
   void (*set_params)(void *data, void *shader_data);

   void (*set_uniform_parameter)(void *data, struct uniform_info *param,
         void *uniform_data);

   /* Compile a shader program. */
   bool (*compile_program)(void *data, unsigned idx,
         void *program_data, struct shader_program_info *program_info);

   /* Use a shader program specified by variable 'index'. */
   void (*use)(void *data, void *shader_data, unsigned index, bool set_active);

   /* Returns the number of currently loaded shaders. */
   unsigned (*num_shaders)(void *data);

   bool (*filter_type)(void *data, unsigned index, bool *smooth);
   enum gfx_wrap_type (*wrap_type)(void *data, unsigned index);
   void (*shader_scale)(void *data,
         unsigned index, struct gfx_fbo_scale *scale);
   bool (*set_coords)(void *shader_data, const struct video_coords *coords);
   bool (*set_mvp)(void *shader_data, const void *mat_data);
   unsigned (*get_prev_textures)(void *data);
   bool (*get_feedback_pass)(void *data, unsigned *pass);
   bool (*mipmap_input)(void *data, unsigned index);

   struct video_shader *(*get_current_shader)(void *data);

   void (*get_flags)(uint32_t*);

   enum rarch_shader_type type;

   /* Human readable string. */
   const char *ident;
} shader_backend_t;

typedef struct video_shader_ctx_init
{
   enum rarch_shader_type shader_type;
   const char *path;
   const shader_backend_t *shader;
   void *data;
   void *shader_data;
   struct
   {
      bool core_context_enabled;
   } gl;
} video_shader_ctx_init_t;

typedef struct video_shader_ctx_params
{
   unsigned width;
   unsigned height;
   unsigned tex_width;
   unsigned tex_height;
   unsigned out_width;
   unsigned out_height;
   unsigned frame_counter;
   unsigned fbo_info_cnt;
   void *data;
   const void *info;
   const void *prev_info;
   const void *feedback_info;
   const void *fbo_info;
} video_shader_ctx_params_t;

typedef struct video_shader_ctx_coords
{
   void *handle_data;
   const void *data;
} video_shader_ctx_coords_t;

typedef struct video_shader_ctx_scale
{
   unsigned idx;
   struct gfx_fbo_scale *scale;
} video_shader_ctx_scale_t;

typedef struct video_shader_ctx_info
{
   bool set_active;
   unsigned num;
   unsigned idx;
   void *data;
} video_shader_ctx_info_t;

typedef struct video_shader_ctx_mvp
{
   void *data;
   const void *matrix;
} video_shader_ctx_mvp_t;

typedef struct video_shader_ctx_filter
{
   unsigned index;
   bool *smooth;
} video_shader_ctx_filter_t;

typedef struct video_shader_ctx
{
   struct video_shader *data;
} video_shader_ctx_t;

typedef struct video_shader_ctx_texture
{
   unsigned id;
} video_shader_ctx_texture_t;

typedef void (*gfx_ctx_proc_t)(void);

typedef struct video_info
{
   /* Launch in fullscreen mode instead of windowed mode. */
   bool fullscreen;

   /* Start with V-Sync enabled. */
   bool vsync;

   /* If true, the output image should have the aspect ratio
    * as set in aspect_ratio. */
   bool force_aspect;

   bool font_enable;

   /* Width of window.
    * If fullscreen mode is requested,
    * a width of 0 means the resolution of the
    * desktop should be used. */
   unsigned width;

   /* Height of window.
    * If fullscreen mode is requested,
    * a height of 0 means the resolutiof the desktop should be used.
    */
   unsigned height;

   int swap_interval;

   bool adaptive_vsync;

#ifdef GEKKO
   bool vfilter;
#endif

   /* If true, applies bilinear filtering to the image,
    * otherwise nearest filtering. */
   bool smooth;

   bool is_threaded;

   /* Use 32bit RGBA rather than native RGB565/XBGR1555.
    *
    * XRGB1555 format is 16-bit and has byte ordering: 0RRRRRGGGGGBBBBB,
    * in native endian.
    *
    * ARGB8888 is AAAAAAAARRRRRRRRGGGGGGGGBBBBBBBB, native endian.
    * Alpha channel should be disregarded.
    * */
   bool rgb32;

#ifdef GEKKO
   /* TODO - we can't really have driver system-specific
    * variables in here. There should be some
    * kind of publicly accessible driver implementation
    * video struct for specific things like this.
    */

   /* Wii-specific settings. Ignored for everything else. */
   unsigned viwidth;
#endif

   /*
    * input_scale defines the maximum size of the picture that will
    * ever be used with the frame callback.
    *
    * The maximum resolution is a multiple of 256x256 size (RARCH_SCALE_BASE),
    * so an input scale of 2 means you should allocate a texture or of 512x512.
    *
    * Maximum input size: RARCH_SCALE_BASE * input_scale
    */
   unsigned input_scale;

   uintptr_t parent;
} video_info_t;

typedef struct video_frame_info
{
   bool widgets_inited;
   bool widgets_is_paused;
   bool widgets_is_fast_forwarding;
   bool widgets_is_rewinding;
   bool input_menu_swap_ok_cancel_buttons;
   bool input_driver_nonblock_state;
   bool shared_context;
   bool black_frame_insertion;
   bool hard_sync;
   bool fps_show;
   bool memory_show;
   bool statistics_show;
   bool framecount_show;
   bool scale_integer;
   bool post_filter_record;
   bool windowed_fullscreen;
   bool fullscreen;
   bool font_enable;
   bool use_rgba;
   bool libretro_running;
   bool xmb_shadows_enable;
   bool battery_level_enable;
   bool timedate_enable;
   bool runloop_is_slowmotion;
   bool runloop_is_idle;
   bool runloop_is_paused;
   bool is_perfcnt_enable;
   bool menu_is_alive;
   bool msg_bgcolor_enable;

   int custom_vp_x;
   int custom_vp_y;
   int crt_switch_center_adjust;

   unsigned fps_update_interval;
   unsigned hard_sync_frames;
   unsigned aspect_ratio_idx;
   unsigned max_swapchain_images;
   unsigned monitor_index;
   unsigned crt_switch_resolution;
   unsigned crt_switch_resolution_super;
   unsigned width;
   unsigned height;
   unsigned xmb_theme;
   unsigned xmb_color_theme;
   unsigned menu_shader_pipeline;
   unsigned materialui_color_theme;
   unsigned ozone_color_theme;
   unsigned custom_vp_width;
   unsigned custom_vp_height;
   unsigned custom_vp_full_width;
   unsigned custom_vp_full_height;

   float menu_wallpaper_opacity;
   float menu_framebuffer_opacity;
   float menu_header_opacity;
   float menu_footer_opacity;
   float refresh_rate;
   float font_msg_pos_x;
   float font_msg_pos_y;
   float font_msg_color_r;
   float font_msg_color_g;
   float font_msg_color_b;
   float xmb_alpha_factor;

   char fps_text[128];
   char stat_text[512];
   char chat_text[256];

   uint64_t frame_count;
   float frame_time;
   float frame_rate;

   struct
   {
      float x;
      float y;
      float scale;
      /* Drop shadow color multiplier. */
      float drop_mod;
      /* Drop shadow offset.
       * If both are 0, no drop shadow will be rendered. */
      int drop_x, drop_y;
      /* Drop shadow alpha */
      float drop_alpha;
      /* ABGR. Use the macros. */
      uint32_t color;
      bool full_screen;
      enum text_alignment text_align;
   } osd_stat_params;

   void (*cb_update_window_title)(void*, void *);
   void (*cb_swap_buffers)(void*, void *);
   bool (*cb_get_metrics)(void *data, enum display_metric_types type,
      float *value);
   bool (*cb_set_resize)(void*, unsigned, unsigned);

   void *context_data;
   void *userdata;
} video_frame_info_t;

typedef void (*update_window_title_cb)(void*, void*);
typedef bool (*get_metrics_cb)(void *data, enum display_metric_types type,
      float *value);
typedef bool (*set_resize_cb)(void*, unsigned, unsigned);

typedef struct gfx_ctx_driver
{
   /* The opaque pointer is the underlying video driver data (e.g. gl_t for
    * OpenGL contexts). Although not advised, the context driver is allowed
    * to hold a pointer to it as the context never outlives the video driver.
    *
    * The context driver is responsible for it's own data.*/
   void* (*init)(video_frame_info_t *video_info, void *video_driver);
   void (*destroy)(void *data);

   enum gfx_ctx_api (*get_api)(void *data);

   /* Which API to bind to. */
   bool (*bind_api)(void *video_driver, enum gfx_ctx_api,
         unsigned major, unsigned minor);

   /* Sets the swap interval. */
   void (*swap_interval)(void *data, int);

   /* Sets video mode. Creates a window, etc. */
   bool (*set_video_mode)(void*, video_frame_info_t *video_info, unsigned, unsigned, bool);

   /* Gets current window size.
    * If not initialized yet, it returns current screen size. */
   void (*get_video_size)(void*, unsigned*, unsigned*);

   float (*get_refresh_rate)(void*);

   void (*get_video_output_size)(void*, unsigned*, unsigned*);

   void (*get_video_output_prev)(void*);

   void (*get_video_output_next)(void*);

   get_metrics_cb get_metrics;

   /* Translates a window size to an aspect ratio.
    * In most cases this will be just width / height, but
    * some contexts will better know which actual aspect ratio is used.
    * This can be NULL to assume the default behavior.
    */
   float (*translate_aspect)(void*, unsigned, unsigned);

   /* Asks driver to update window title (FPS, etc). */
   update_window_title_cb update_window_title;

   /* Queries for resize and quit events.
    * Also processes events. */
   void (*check_window)(void*, bool*, bool*,
         unsigned*, unsigned*, bool);

   /* Acknowledge a resize event. This is needed for some APIs.
    * Most backends will ignore this. */
   set_resize_cb set_resize;

   /* Checks if window has input focus. */
   bool (*has_focus)(void*);

   /* Should the screensaver be suppressed? */
   bool (*suppress_screensaver)(void *data, bool enable);

   /* Checks if context driver has windowed support. */
   bool has_windowed;

   /* Swaps buffers. VBlank sync depends on
    * earlier calls to swap_interval. */
   void (*swap_buffers)(void*, void *);

   /* Most video backends will want to use a certain input driver.
    * Checks for it here. */
   void (*input_driver)(void*, const char *, input_driver_t**, void**);

   /* Wraps whatever gl_proc_address() there is.
    * Does not take opaque, to avoid lots of ugly wrapper code. */
   gfx_ctx_proc_t (*get_proc_address)(const char*);

   /* Returns true if this context supports EGLImage buffers for
    * screen drawing and was initalized correctly. */
   bool (*image_buffer_init)(void*, const video_info_t*);

   /* Writes the frame to the EGLImage and sets image_handle to it.
    * Returns true if a new image handle is created.
    * Always returns true the first time it's called for a new index.
    * The graphics core must handle a change in the handle correctly. */
   bool (*image_buffer_write)(void*, const void *frame, unsigned width,
         unsigned height, unsigned pitch, bool rgb32,
         unsigned index, void **image_handle);

   /* Shows or hides mouse. Can be NULL if context doesn't
    * have a concept of mouse pointer. */
   void (*show_mouse)(void *data, bool state);

   /* Human readable string. */
   const char *ident;

   uint32_t (*get_flags)(void *data);

   void     (*set_flags)(void *data, uint32_t flags);

   /* Optional. Binds HW-render offscreen context. */
   void (*bind_hw_render)(void *data, bool enable);

   /* Optional. Gets base data for the context which is used by the driver.
    * This is mostly relevant for graphics APIs such as Vulkan
    * which do not have global context state. */
   void *(*get_context_data)(void *data);

   /* Optional. Makes driver context (only GLX right now)
    * active for this thread. */
   void (*make_current)(bool release);
} gfx_ctx_driver_t;

typedef struct gfx_ctx_size
{
   bool *quit;
   bool *resize;
   unsigned *width;
   unsigned *height;
} gfx_ctx_size_t;

typedef struct gfx_ctx_mode
{
   unsigned width;
   unsigned height;
   bool fullscreen;
} gfx_ctx_mode_t;

typedef struct gfx_ctx_metrics
{
   enum display_metric_types type;
   float *value;
} gfx_ctx_metrics_t;

typedef struct gfx_ctx_aspect
{
   float *aspect;
   unsigned width;
   unsigned height;
} gfx_ctx_aspect_t;

typedef struct gfx_ctx_image
{
   const void *frame;
   unsigned width;
   unsigned height;
   unsigned pitch;
   unsigned index;
   bool rgb32;
   void **handle;
} gfx_ctx_image_t;

typedef struct gfx_ctx_input
{
   input_driver_t **input;
   void **input_data;
} gfx_ctx_input_t;

typedef struct gfx_ctx_ident
{
   const char *ident;
} gfx_ctx_ident_t;

struct aspect_ratio_elem
{
   char name[64];
   float value;
};

/* Optionally implemented interface to poke more
 * deeply into video driver. */

typedef struct video_poke_interface
{
   uint32_t (*get_flags)(void *data);
   uintptr_t (*load_texture)(void *video_data, void *data,
         bool threaded, enum texture_filter_type filter_type);
   void (*unload_texture)(void *data, uintptr_t id);
   void (*set_video_mode)(void *data, unsigned width,
         unsigned height, bool fullscreen);
   float (*get_refresh_rate)(void *data);
   void (*set_filtering)(void *data, unsigned index, bool smooth);
   void (*get_video_output_size)(void *data,
         unsigned *width, unsigned *height);

   /* Move index to previous resolution */
   void (*get_video_output_prev)(void *data);

   /* Move index to next resolution */
   void (*get_video_output_next)(void *data);

   uintptr_t (*get_current_framebuffer)(void *data);
   retro_proc_address_t (*get_proc_address)(void *data, const char *sym);
   void (*set_aspect_ratio)(void *data, unsigned aspectratio_index);
   void (*apply_state_changes)(void *data);

   /* Update texture. */
   void (*set_texture_frame)(void *data, const void *frame, bool rgb32,
         unsigned width, unsigned height, float alpha);
   /* Enable or disable rendering. */
   void (*set_texture_enable)(void *data, bool enable, bool full_screen);
   void (*set_osd_msg)(void *data, video_frame_info_t *video_info,
         const char *msg,
         const void *params, void *font);

   void (*show_mouse)(void *data, bool state);
   void (*grab_mouse_toggle)(void *data);

   struct video_shader *(*get_current_shader)(void *data);
   bool (*get_current_software_framebuffer)(void *data,
         struct retro_framebuffer *framebuffer);
   bool (*get_hw_render_interface)(void *data,
         const struct retro_hw_render_interface **iface);
} video_poke_interface_t;

/* msg is for showing a message on the screen
 * along with the video frame. */
typedef bool (*video_driver_frame_t)(void *data,
      const void *frame, unsigned width,
      unsigned height, uint64_t frame_count,
      unsigned pitch, const char *msg, video_frame_info_t *video_info);

typedef struct video_driver
{
   /* Should the video driver act as an input driver as well?
    * The video initialization might preinitialize an input driver
    * to override the settings in case the video driver relies on
    * input driver for event handling. */
   void *(*init)(const video_info_t *video,
         input_driver_t **input,
         void **input_data);

   /* Updates frame on the screen.
    * Frame can be either XRGB1555, RGB565 or ARGB32 format
    * depending on rgb32 setting in video_info_t.
    * Pitch is the distance in bytes between two scanlines in memory.
    *
    * When msg is non-NULL,
    * it's a message that should be displayed to the user. */
   video_driver_frame_t frame;

   /* Should we care about syncing to vblank? Fast forwarding.
    *
    * Requests nonblocking operation.
    *
    * True = VSync is turned off.
    * False = VSync is turned on.
    * */
   void (*set_nonblock_state)(void *data, bool toggle);

   /* Is the window still active? */
   bool (*alive)(void *data);

   /* Does the window have focus? */
   bool (*focus)(void *data);

   /* Should the screensaver be suppressed? */
   bool (*suppress_screensaver)(void *data, bool enable);

   /* Does the graphics context support windowed mode? */
   bool (*has_windowed)(void *data);

   /* Sets shader. Might not be implemented. Will be moved to
    * poke_interface later. */
   bool (*set_shader)(void *data, enum rarch_shader_type type,
         const char *path);

   /* Frees driver. */
   void (*free)(void *data);

   /* Human-readable identifier. */
   const char *ident;

   void (*set_viewport)(void *data, unsigned width, unsigned height,
         bool force_full, bool allow_rotate);

   void (*set_rotation)(void *data, unsigned rotation);
   void (*viewport_info)(void *data, struct video_viewport *vp);

   /* Reads out in BGR byte order (24bpp). */
   bool (*read_viewport)(void *data, uint8_t *buffer, bool is_idle);

   /* Returns a pointer to a newly allocated buffer that can
    * (and must) be passed to free() by the caller, containing a
    * copy of the current raw frame in the active pixel format
    * and sets width, height and pitch to the correct values. */
   void* (*read_frame_raw)(void *data, unsigned *width,
   unsigned *height, size_t *pitch);

#ifdef HAVE_OVERLAY
   void (*overlay_interface)(void *data,
         const video_overlay_interface_t **iface);
#endif
#ifdef HAVE_VIDEO_LAYOUT
   const video_layout_render_interface_t *(*video_layout_render_interface)(void *data);
#endif
   void (*poke_interface)(void *data, const video_poke_interface_t **iface);
   unsigned (*wrap_type_to_enum)(enum gfx_wrap_type type);

#if defined(HAVE_MENU) && defined(HAVE_MENU_WIDGETS)
   /* if set to true, will use menu widgets when applicable
    * if set to false, will use OSD as a fallback */
   bool (*menu_widgets_enabled)(void *data);
#endif
} video_driver_t;

extern struct aspect_ratio_elem aspectratio_lut[ASPECT_RATIO_END];

bool video_driver_has_windowed(void);

bool video_driver_has_focus(void);

bool video_driver_cached_frame_has_valid_framebuffer(void);

void video_driver_set_cached_frame_ptr(const void *data);

void video_driver_set_stub_frame(void);

void video_driver_unset_stub_frame(void);

bool video_driver_supports_viewport_read(void);

bool video_driver_prefer_viewport_read(void);

bool video_driver_supports_read_frame_raw(void);

void video_driver_set_viewport_core(void);

void video_driver_reset_custom_viewport(void);

void video_driver_set_rgba(void);

void video_driver_unset_rgba(void);

bool video_driver_supports_rgba(void);

bool video_driver_get_next_video_out(void);

bool video_driver_get_prev_video_out(void);

void video_driver_monitor_reset(void);

void video_driver_set_aspect_ratio(void);

void video_driver_update_viewport(struct video_viewport* vp, bool force_full, bool keep_aspect);

void video_driver_show_mouse(void);

void video_driver_hide_mouse(void);

void video_driver_apply_state_changes(void);

bool video_driver_read_viewport(uint8_t *buffer, bool is_idle);

bool video_driver_cached_frame(void);

void video_driver_default_settings(void);

void video_driver_load_settings(config_file_t *conf);

void video_driver_save_settings(config_file_t *conf);

bool video_driver_is_hw_context(void);

struct retro_hw_render_callback *video_driver_get_hw_context(void);

const struct retro_hw_render_context_negotiation_interface
*video_driver_get_context_negotiation_interface(void);

bool video_driver_is_video_cache_context(void);

void video_driver_set_video_cache_context_ack(void);

bool video_driver_get_viewport_info(struct video_viewport *viewport);

/**
 * config_get_video_driver_options:
 *
 * Get an enumerated list of all video driver names, separated by '|'.
 *
 * Returns: string listing of all video driver names, separated by '|'.
 **/
const char* config_get_video_driver_options(void);

/**
 * video_driver_get_ptr:
 *
 * Use this if you need the real video driver
 * and driver data pointers.
 *
 * Returns: video driver's userdata.
 **/
void *video_driver_get_ptr(bool force_nonthreaded_data);

bool video_driver_set_shader(enum rarch_shader_type type,
      const char *shader);

bool video_driver_set_rotation(unsigned rotation);

bool video_driver_set_video_mode(unsigned width,
      unsigned height, bool fullscreen);

bool video_driver_get_video_output_size(
      unsigned *width, unsigned *height);

void video_driver_set_osd_msg(const char *msg,
      const void *params, void *font);

void video_driver_set_texture_enable(bool enable, bool full_screen);

void video_driver_set_texture_frame(const void *frame, bool rgb32,
      unsigned width, unsigned height, float alpha);

#ifdef HAVE_VIDEO_LAYOUT
const video_layout_render_interface_t *video_driver_layout_render_interface(void);
#endif

void * video_driver_read_frame_raw(unsigned *width,
   unsigned *height, size_t *pitch);

void video_driver_set_filtering(unsigned index, bool smooth);

const char *video_driver_get_ident(void);

bool video_driver_set_viewport(unsigned width, unsigned height,
      bool force_fullscreen, bool allow_rotate);

void video_driver_get_size(unsigned *width, unsigned *height);

void video_driver_set_size(unsigned *width, unsigned *height);

float video_driver_get_aspect_ratio(void);

void video_driver_set_aspect_ratio_value(float value);

enum retro_pixel_format video_driver_get_pixel_format(void);

void video_driver_cached_frame_set(const void *data, unsigned width,
      unsigned height, size_t pitch);

void video_driver_cached_frame_get(const void **data, unsigned *width,
      unsigned *height, size_t *pitch);

void video_driver_menu_settings(void **list_data, void *list_info_data,
      void *group_data, void *subgroup_data, const char *parent_group);

/**
 * video_viewport_get_scaled_integer:
 * @vp            : Viewport handle
 * @width         : Width.
 * @height        : Height.
 * @aspect_ratio  : Aspect ratio (in float).
 * @keep_aspect   : Preserve aspect ratio?
 *
 * Gets viewport scaling dimensions based on
 * scaled integer aspect ratio.
 **/
void video_viewport_get_scaled_integer(struct video_viewport *vp,
      unsigned width, unsigned height,
      float aspect_ratio, bool keep_aspect);

struct retro_system_av_info *video_viewport_get_system_av_info(void);

struct video_viewport *video_viewport_get_custom(void);

/**
 * video_monitor_set_refresh_rate:
 * @hz                 : New refresh rate for monitor.
 *
 * Sets monitor refresh rate to new value.
 **/
void video_monitor_set_refresh_rate(float hz);

/**
 * video_monitor_fps_statistics
 * @refresh_rate       : Monitor refresh rate.
 * @deviation          : Deviation from measured refresh rate.
 * @sample_points      : Amount of sampled points.
 *
 * Gets the monitor FPS statistics based on the current
 * runtime.
 *
 * Returns: true (1) on success.
 * false (0) if:
 * a) threaded video mode is enabled
 * b) less than 2 frame time samples.
 * c) FPS monitor enable is off.
 **/
bool video_monitor_fps_statistics(double *refresh_rate,
      double *deviation, unsigned *sample_points);

unsigned video_pixel_get_alignment(unsigned pitch);

void crt_switch_driver_reinit(void);

#define video_driver_translate_coord_viewport_wrap(vp, mouse_x, mouse_y, res_x, res_y, res_screen_x, res_screen_y) \
   (video_driver_get_viewport_info(vp) ? video_driver_translate_coord_viewport(vp, mouse_x, mouse_y, res_x, res_y, res_screen_x, res_screen_y) : false)

/**
 * video_driver_translate_coord_viewport:
 * @mouse_x                        : Pointer X coordinate.
 * @mouse_y                        : Pointer Y coordinate.
 * @res_x                          : Scaled  X coordinate.
 * @res_y                          : Scaled  Y coordinate.
 * @res_screen_x                   : Scaled screen X coordinate.
 * @res_screen_y                   : Scaled screen Y coordinate.
 *
 * Translates pointer [X,Y] coordinates into scaled screen
 * coordinates based on viewport info.
 *
 * Returns: true (1) if successful, false if video driver doesn't support
 * viewport info.
 **/
bool video_driver_translate_coord_viewport(
      struct video_viewport *vp,
      int mouse_x, int mouse_y,
      int16_t *res_x, int16_t *res_y, int16_t *res_screen_x,
      int16_t *res_screen_y);

uintptr_t video_driver_display_userdata_get(void);

uintptr_t video_driver_display_get(void);

enum rarch_display_type video_driver_display_type_get(void);

uintptr_t video_driver_window_get(void);

void video_driver_display_type_set(enum rarch_display_type type);

void video_driver_display_set(uintptr_t idx);

void video_driver_display_userdata_set(uintptr_t idx);

void video_driver_window_set(uintptr_t idx);

uintptr_t video_driver_window_get(void);

bool video_driver_texture_load(void *data,
      enum texture_filter_type  filter_type,
      uintptr_t *id);

bool video_driver_texture_unload(uintptr_t *id);

void video_driver_build_info(video_frame_info_t *video_info);

void video_driver_reinit(int flags);

void video_driver_get_window_title(char *buf, unsigned len);

bool *video_driver_get_threaded(void);

void video_driver_set_threaded(bool val);

/**
 * video_context_driver_init_first:
 * @data                    : Input data.
 * @ident                   : Identifier of graphics context driver to find.
 * @api                     : API of higher-level graphics API.
 * @major                   : Major version number of higher-level graphics API.
 * @minor                   : Minor version number of higher-level graphics API.
 * @hw_render_ctx           : Request a graphics context driver capable of
 *                            hardware rendering?
 *
 * Finds first suitable graphics context driver and initializes.
 *
 * Returns: graphics context driver if found, otherwise NULL.
 **/
const gfx_ctx_driver_t *video_context_driver_init_first(
      void *data, const char *ident,
      enum gfx_ctx_api api, unsigned major, unsigned minor,
      bool hw_render_ctx, void **ctx_data);

bool video_context_driver_find_prev_driver(void);

bool video_context_driver_find_next_driver(void);

bool video_context_driver_write_to_image_buffer(gfx_ctx_image_t *img);

bool video_context_driver_get_video_output_prev(void);

bool video_context_driver_get_video_output_next(void);

bool video_context_driver_set(const gfx_ctx_driver_t *data);

void video_context_driver_destroy(void);

bool video_context_driver_get_video_output_size(gfx_ctx_size_t *size_data);

bool video_context_driver_get_ident(gfx_ctx_ident_t *ident);

bool video_context_driver_set_video_mode(gfx_ctx_mode_t *mode_info);

bool video_context_driver_get_video_size(gfx_ctx_mode_t *mode_info);

bool video_context_driver_get_refresh_rate(float *refresh_rate);

bool video_context_driver_set_flags(gfx_ctx_flags_t *flags);

bool video_context_driver_get_metrics(gfx_ctx_metrics_t *metrics);

bool video_context_driver_translate_aspect(gfx_ctx_aspect_t *aspect);

bool video_context_driver_input_driver(gfx_ctx_input_t *inp);

enum gfx_ctx_api video_context_driver_get_api(void);

void video_context_driver_free(void);

bool video_shader_driver_get_current_shader(video_shader_ctx_t *shader);

float video_driver_get_refresh_rate(void);

bool video_driver_started_fullscreen(void);

bool video_driver_is_threaded(void);

bool video_driver_get_flags(gfx_ctx_flags_t *flags);

bool video_context_driver_get_flags(gfx_ctx_flags_t *flags);

bool video_driver_test_all_flags(enum display_flags testflag);

void video_driver_set_gpu_device_string(const char *str);

const char* video_driver_get_gpu_device_string(void);

void video_driver_set_gpu_api_version_string(const char *str);

const char* video_driver_get_gpu_api_version_string(void);

/* string list stays owned by the caller and must be available at all times after the video driver is inited */
void video_driver_set_gpu_api_devices(enum gfx_ctx_api api, struct string_list *list);

struct string_list* video_driver_get_gpu_api_devices(enum gfx_ctx_api api);

static INLINE bool gl_set_core_context(enum retro_hw_context_type ctx_type)
{
   gfx_ctx_flags_t flags;
   if (ctx_type != RETRO_HW_CONTEXT_OPENGL_CORE)
      return false;

   /**
    * Ensure that the rest of the frontend knows we have a core context
    */
   flags.flags = 0;
   BIT32_SET(flags.flags, GFX_CTX_FLAGS_GL_CORE_CONTEXT);

   video_context_driver_set_flags(&flags);

   return true;
}

extern video_driver_t video_gl_core;
extern video_driver_t video_gl2;
extern video_driver_t video_gl1;
extern video_driver_t video_vulkan;
extern video_driver_t video_metal;
extern video_driver_t video_psp1;
extern video_driver_t video_vita2d;
extern video_driver_t video_ps2;
extern video_driver_t video_ctr;
extern video_driver_t video_switch;
extern video_driver_t video_d3d8;
extern video_driver_t video_d3d9;
extern video_driver_t video_d3d10;
extern video_driver_t video_d3d11;
extern video_driver_t video_d3d12;
extern video_driver_t video_gx;
extern video_driver_t video_wiiu;
extern video_driver_t video_xenon360;
extern video_driver_t video_xvideo;
extern video_driver_t video_sdl;
extern video_driver_t video_sdl2;
extern video_driver_t video_sdl_dingux;
extern video_driver_t video_vg;
extern video_driver_t video_omap;
extern video_driver_t video_exynos;
extern video_driver_t video_dispmanx;
extern video_driver_t video_sunxi;
extern video_driver_t video_drm;
extern video_driver_t video_xshm;
extern video_driver_t video_caca;
extern video_driver_t video_gdi;
extern video_driver_t video_vga;
extern video_driver_t video_fpga;
extern video_driver_t video_sixel;
extern video_driver_t video_network;

extern const gfx_ctx_driver_t gfx_ctx_osmesa;
extern const gfx_ctx_driver_t gfx_ctx_sdl_gl;
extern const gfx_ctx_driver_t gfx_ctx_x_egl;
extern const gfx_ctx_driver_t gfx_ctx_uwp;
extern const gfx_ctx_driver_t gfx_ctx_wayland;
extern const gfx_ctx_driver_t gfx_ctx_x;
extern const gfx_ctx_driver_t gfx_ctx_drm;
extern const gfx_ctx_driver_t gfx_ctx_mali_fbdev;
extern const gfx_ctx_driver_t gfx_ctx_vivante_fbdev;
extern const gfx_ctx_driver_t gfx_ctx_android;
extern const gfx_ctx_driver_t gfx_ctx_ps3;
extern const gfx_ctx_driver_t gfx_ctx_wgl;
extern const gfx_ctx_driver_t gfx_ctx_videocore;
extern const gfx_ctx_driver_t gfx_ctx_qnx;
extern const gfx_ctx_driver_t gfx_ctx_cgl;
extern const gfx_ctx_driver_t gfx_ctx_cocoagl;
extern const gfx_ctx_driver_t gfx_ctx_emscripten;
extern const gfx_ctx_driver_t gfx_ctx_opendingux_fbdev;
extern const gfx_ctx_driver_t gfx_ctx_khr_display;
extern const gfx_ctx_driver_t gfx_ctx_gdi;
extern const gfx_ctx_driver_t gfx_ctx_fpga;
extern const gfx_ctx_driver_t gfx_ctx_sixel;
extern const gfx_ctx_driver_t gfx_ctx_network;
extern const gfx_ctx_driver_t switch_ctx;
extern const gfx_ctx_driver_t orbis_ctx;
extern const gfx_ctx_driver_t vita_ctx;
extern const gfx_ctx_driver_t gfx_ctx_null;

extern const shader_backend_t gl_glsl_backend;
extern const shader_backend_t gl_cg_backend;

/* BSV Movie */

void bsv_movie_frame_rewind(void);

void bsv_movie_set_path(const char *path);

/* Location */

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

bool menu_driver_is_alive(void);

void menu_driver_set_binding_state(bool on);

bool menu_driver_is_toggled(void);

bool menu_driver_is_toggled(void);

bool menu_widgets_ready(void);

unsigned int retroarch_get_rotation(void);

bool is_input_keyboard_display_on(void);

RETRO_END_DECLS

#endif
