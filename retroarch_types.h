#ifndef _RETROARCH_TYPES_H
#define _RETROARCH_TYPES_H

#include <setjmp.h>
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
   CORE_TYPE_VIDEO_PROCESSOR
};

enum rarch_ctl_state
{
   RARCH_CTL_NONE = 0,

   /* Deinitializes RetroArch. */
   RARCH_CTL_MAIN_DEINIT,

   RARCH_CTL_IS_DUMMY_CORE,
   RARCH_CTL_IS_CORE_LOADED,

#if defined(HAVE_RUNAHEAD) && (defined(HAVE_DYNAMIC) || defined(HAVE_DYLIB))
   RARCH_CTL_IS_SECOND_CORE_AVAILABLE,
   RARCH_CTL_IS_SECOND_CORE_LOADED,
#endif

   RARCH_CTL_UNSET_BPS_PREF,
   RARCH_CTL_UNSET_UPS_PREF,
   RARCH_CTL_UNSET_IPS_PREF,

#ifdef HAVE_CONFIGFILE
   /* Block config read */
   RARCH_CTL_SET_BLOCK_CONFIG_READ,
   RARCH_CTL_UNSET_BLOCK_CONFIG_READ,
#endif

   RARCH_CTL_HAS_SET_SUBSYSTEMS,

   RARCH_CTL_SET_WINDOWED_SCALE,

#ifdef HAVE_CONFIGFILE
   RARCH_CTL_SET_REMAPS_CORE_ACTIVE,
   RARCH_CTL_SET_REMAPS_CONTENT_DIR_ACTIVE,
   RARCH_CTL_SET_REMAPS_GAME_ACTIVE,
#endif

   RARCH_CTL_IS_MISSING_BIOS,
   RARCH_CTL_SET_MISSING_BIOS,
   RARCH_CTL_UNSET_MISSING_BIOS,

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

enum rarch_main_wrap_flags
{
   RARCH_MAIN_WRAP_FLAG_VERBOSE    = (1 << 0),
   RARCH_MAIN_WRAP_FLAG_NO_CONTENT = (1 << 1),
   RARCH_MAIN_WRAP_FLAG_TOUCHED    = (1 << 2)
};

enum content_state_flags
{
   CONTENT_ST_FLAG_IS_INITED                  = (1 << 0),
   CONTENT_ST_FLAG_CORE_DOES_NOT_NEED_CONTENT = (1 << 1),
   CONTENT_ST_FLAG_PENDING_SUBSYSTEM_INIT     = (1 << 2),
   CONTENT_ST_FLAG_PENDING_ROM_CRC            = (1 << 3)
};

typedef struct rarch_memory_descriptor
{
   struct retro_memory_descriptor core;        /* uint64_t alignment */
   /* Retroarch can have additional context here */
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
   uint8_t flags;
};

/* All run-time- / command line flag-related globals go here. */

typedef struct global
{
   jmp_buf error_sjlj_context;              /* 4-byte alignment,
                                               put it right before long */

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
            struct
            {
               unsigned idx;
               unsigned id;
            } current;
            struct
            {
               unsigned idx;
               unsigned id;
            } initial;
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

   char error_string[255];
   bool launched_from_cli;
   bool cli_load_menu_on_error;
   bool error_on_init;
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
   uint8_t flags;

   char companion_ui_crc32[32];
   char pending_subsystem_ident[255];
   char pending_rom_crc_path[PATH_MAX_LENGTH];
   char companion_ui_db_name[PATH_MAX_LENGTH];
} content_state_t;

RETRO_END_DECLS

#endif
