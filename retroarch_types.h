#ifndef _RETROARCH_TYPES_H
#define _RETROARCH_TYPES_H

#include <boolean.h>
#include <retro_inline.h>
#include <retro_common_api.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_MENU
#include "menu/menu_defines.h"
#endif

#include "input/input_defines.h"
#include "disk_control_interface.h"

RETRO_BEGIN_DECLS

enum
{
   /* Polling is performed before
    * call to retro_run. */
   POLL_TYPE_EARLY = 0,

   /* Polling is performed when requested. */
   POLL_TYPE_NORMAL,

   /* Polling is performed on first call to
    * retro_input_state per frame. */
   POLL_TYPE_LATE
};

enum rarch_core_type
{
   CORE_TYPE_PLAIN = 0,
   CORE_TYPE_DUMMY,
   CORE_TYPE_FFMPEG,
   CORE_TYPE_MPV,
   CORE_TYPE_IMAGEVIEWER,
   CORE_TYPE_NETRETROPAD,
   CORE_TYPE_VIDEO_PROCESSOR,
   CORE_TYPE_GONG
};

enum rarch_ctl_state
{
   RARCH_CTL_NONE = 0,

   /* Deinitializes RetroArch. */
   RARCH_CTL_MAIN_DEINIT,

   RARCH_CTL_IS_INITED,

   RARCH_CTL_IS_DUMMY_CORE,
   RARCH_CTL_IS_CORE_LOADED,

#if defined(HAVE_RUNAHEAD) && (defined(HAVE_DYNAMIC) || defined(HAVE_DYLIB))
   RARCH_CTL_IS_SECOND_CORE_AVAILABLE,
   RARCH_CTL_IS_SECOND_CORE_LOADED,
#endif

   RARCH_CTL_IS_BPS_PREF,
   RARCH_CTL_UNSET_BPS_PREF,

   RARCH_CTL_IS_UPS_PREF,
   RARCH_CTL_UNSET_UPS_PREF,

   RARCH_CTL_IS_IPS_PREF,
   RARCH_CTL_UNSET_IPS_PREF,

#ifdef HAVE_CONFIGFILE
   /* Block config read */
   RARCH_CTL_SET_BLOCK_CONFIG_READ,
   RARCH_CTL_UNSET_BLOCK_CONFIG_READ,
#endif

   /* Username */
   RARCH_CTL_HAS_SET_USERNAME,

   RARCH_CTL_HAS_SET_SUBSYSTEMS,

   RARCH_CTL_IS_IDLE,
   RARCH_CTL_SET_IDLE,

   RARCH_CTL_SET_WINDOWED_SCALE,

#ifdef HAVE_CONFIGFILE
   RARCH_CTL_IS_OVERRIDES_ACTIVE,

   RARCH_CTL_IS_REMAPS_CORE_ACTIVE,
   RARCH_CTL_SET_REMAPS_CORE_ACTIVE,

   RARCH_CTL_IS_REMAPS_CONTENT_DIR_ACTIVE,
   RARCH_CTL_SET_REMAPS_CONTENT_DIR_ACTIVE,

   RARCH_CTL_IS_REMAPS_GAME_ACTIVE,
   RARCH_CTL_SET_REMAPS_GAME_ACTIVE,
#endif

   RARCH_CTL_IS_MISSING_BIOS,
   RARCH_CTL_SET_MISSING_BIOS,
   RARCH_CTL_UNSET_MISSING_BIOS,

   RARCH_CTL_IS_GAME_OPTIONS_ACTIVE,
   RARCH_CTL_IS_FOLDER_OPTIONS_ACTIVE,

   RARCH_CTL_IS_PAUSED,
   RARCH_CTL_SET_PAUSED,

   RARCH_CTL_SET_SHUTDOWN,

   /* Runloop state */
   RARCH_CTL_STATE_FREE,

   /* Performance counters */
   RARCH_CTL_GET_PERFCNT,
   RARCH_CTL_SET_PERFCNT_ENABLE,
   RARCH_CTL_UNSET_PERFCNT_ENABLE,
   RARCH_CTL_IS_PERFCNT_ENABLE,

   /* Core options */
   RARCH_CTL_HAS_CORE_OPTIONS,
   RARCH_CTL_GET_CORE_OPTION_SIZE,
   RARCH_CTL_CORE_OPTIONS_LIST_GET,
   RARCH_CTL_CORE_OPTION_PREV,
   RARCH_CTL_CORE_OPTION_NEXT,
   RARCH_CTL_CORE_OPTION_UPDATE_DISPLAY,
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
#ifdef HAVE_NETWORKING
   RARCH_OVERRIDE_SETTING_NETPLAY_MODE,
   RARCH_OVERRIDE_SETTING_NETPLAY_IP_ADDRESS,
   RARCH_OVERRIDE_SETTING_NETPLAY_IP_PORT,
   RARCH_OVERRIDE_SETTING_NETPLAY_STATELESS_MODE,
   RARCH_OVERRIDE_SETTING_NETPLAY_CHECK_FRAMES,
#endif
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
   STREAMING_MODE_FACEBOOK,
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

typedef struct rarch_memory_descriptor
{
   struct retro_memory_descriptor core;        /* uint64_t alignment */
   size_t disconnect_mask;
} rarch_memory_descriptor_t;

typedef struct rarch_memory_map
{
   rarch_memory_descriptor_t *descriptors;
   unsigned num_descriptors;
} rarch_memory_map_t;

typedef struct rarch_system_info
{
   struct retro_location_callback location_cb; /* ptr alignment */
   disk_control_interface_t disk_control;      /* ptr alignment */
   struct retro_system_info info;              /* ptr alignment */
   rarch_memory_map_t mmaps;                   /* ptr alignment */
   const char *input_desc_btn[MAX_USERS][RARCH_FIRST_META_KEY];
   struct
   {
      struct retro_subsystem_info *data;
      unsigned size;
   } subsystem;
   struct
   {
      struct retro_controller_info *data;
      unsigned size;
   } ports;
   unsigned rotation;
   unsigned performance_level;
   char valid_extensions[255];
   bool load_no_content;
   bool supports_vfs;
} rarch_system_info_t;

typedef struct retro_ctx_input_state_info
{
   retro_input_state_t cb;
} retro_ctx_input_state_info_t;

typedef struct retro_ctx_cheat_info
{
   const char *code;
   unsigned index;
   bool enabled;
} retro_ctx_cheat_info_t;

typedef struct retro_ctx_api_info
{
   unsigned version;
} retro_ctx_api_info_t;

typedef struct retro_ctx_region_info
{
  unsigned region;
} retro_ctx_region_info_t;

typedef struct retro_ctx_controller_info
{
   unsigned port;
   unsigned device;
} retro_ctx_controller_info_t;

typedef struct retro_ctx_memory_info
{
   void *data;
   size_t size;
   unsigned id;
} retro_ctx_memory_info_t;

typedef struct retro_ctx_load_content_info
{
   struct retro_game_info *info;
   const struct string_list *content;
   const struct retro_subsystem_info *special;
} retro_ctx_load_content_info_t;

typedef struct retro_ctx_serialize_info
{
   const void *data_const;
   void *data;
   size_t size;
} retro_ctx_serialize_info_t;

typedef struct retro_ctx_size_info
{
   size_t size;
} retro_ctx_size_info_t;

typedef struct retro_ctx_environ_info
{
   retro_environment_t env;
} retro_ctx_environ_info_t;

typedef struct retro_callbacks
{
   retro_video_refresh_t frame_cb;
   retro_audio_sample_t sample_cb;
   retro_audio_sample_batch_t sample_batch_cb;
   retro_input_state_t state_cb;
   retro_input_poll_t poll_cb;
} retro_callbacks_t;

struct rarch_main_wrap
{
   char **argv;
   const char *content_path;
   const char *sram_path;
   const char *state_path;
   const char *config_path;
   const char *libretro_path;
   int argc;
   bool verbose;
   bool no_content;
   bool touched;
};

typedef struct rarch_resolution
{
   unsigned idx;
   unsigned id;
} rarch_resolution_t;

/* All run-time- / command line flag-related globals go here. */

typedef struct global
{
#ifdef HAVE_MENU
   struct
   {
      retro_time_t prev_start_time;
      retro_time_t noop_press_time;
      retro_time_t noop_start_time;
      retro_time_t action_start_time;
      retro_time_t action_press_time;
   } menu;
#endif
   struct
   {
      char *remapfile;
      char savefile[8192];
      char savestate[8192];
      char cheatfile[8192];
      char ups[8192];
      char bps[8192];
      char ips[8192];
      char label[8192];
   } name;

   /* Recording. */
   struct
   {
      size_t gpu_width;
      size_t gpu_height;
      unsigned width;
      unsigned height;
      char path[8192];
      char config[8192];
      char output_dir[8192];
      char config_dir[8192];
      bool use_output_dir;
   } record;

   /* Settings and/or global state that is specific to
    * a console-style implementation. */
   struct
   {
      struct
      {
         struct
         {
            uint32_t *list;
            unsigned count;
            rarch_resolution_t current;
            rarch_resolution_t initial;
            bool check;
         } resolutions;
         unsigned      gamma_correction;
         unsigned int  flicker_filter_index;
         unsigned char soft_filter_index;
         bool pal_enable;
         bool pal60_enable;
      } screen;

      bool flickerfilter_enable;
      bool softfilter_enable;

   } console;
   unsigned old_analog_dpad_mode[MAX_USERS];
   unsigned old_libretro_device[MAX_USERS];
   bool old_analog_dpad_mode_set;
   bool old_libretro_device_set;
   bool remapping_cache_active;
   /* Settings and/or global states specific to menus */
#ifdef HAVE_MENU
   enum menu_action menu_prev_action;
#endif
   bool launched_from_cli;
   bool cli_load_menu_on_error;
} global_t;

typedef struct content_file_override
{
   char *ext;
   bool need_fullpath;
   bool persistent_data;
} content_file_override_t;

typedef struct content_file_info
{
   char *full_path;
   char *archive_path;
   char *archive_file;
   char *dir;
   char *name;
   char *ext;
   char *meta; /* Unused at present */
   void *data;
   size_t data_size;
   bool file_in_archive;
   bool persistent_data;
} content_file_info_t;

typedef struct content_file_list
{
   content_file_info_t *entries;
   struct string_list *temporary_files;
   struct retro_game_info *game_info;
   struct retro_game_info_ext *game_info_ext;
   size_t size;
} content_file_list_t;

typedef struct content_state
{
   char *pending_subsystem_roms[RARCH_MAX_SUBSYSTEM_ROMS];

   content_file_override_t *content_override_list;
   content_file_list_t *content_list;

   int pending_subsystem_rom_num;
   int pending_subsystem_id;
   unsigned pending_subsystem_rom_id;
   uint32_t rom_crc;

   char companion_ui_crc32[32];
   char pending_subsystem_ident[255];
   char pending_rom_crc_path[PATH_MAX_LENGTH];
   char companion_ui_db_name[PATH_MAX_LENGTH];

   bool is_inited;
   bool core_does_not_need_content;
   bool pending_subsystem_init;
   bool pending_rom_crc;
} content_state_t;

/* Parameters passed to ffemu_new() */
struct record_params
{
   /* Framerate per second of input video. */
   double fps;
   /* Sample rate of input audio. */
   double samplerate;

   /* Filename to dump to. */
   const char *filename;

   /* Path to config. Optional. */
   const char *config;

   const char *audio_resampler;

   /* Desired output resolution. */
   unsigned out_width;
   unsigned out_height;

   /* Total size of framebuffer used in input. */
   unsigned fb_width;
   unsigned fb_height;

   /* Audio channels. */
   unsigned channels;

   unsigned video_record_scale_factor;
   unsigned video_stream_scale_factor;
   unsigned video_record_threads;
   unsigned streaming_mode;

   /* Aspect ratio of input video. Parameters are passed to the muxer,
    * the video itself is not scaled.
    */
   float aspect_ratio;

   enum record_config_type preset;

   /* Input pixel format. */
   enum ffemu_pix_format pix_fmt;

   bool video_gpu_record;
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


RETRO_END_DECLS

#endif
