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
   RARCH_CTL_CONTENT_RUNTIME_LOG_DEINIT
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

enum rarch_movie_type
{
   RARCH_MOVIE_PLAYBACK = 0,
   RARCH_MOVIE_RECORD
};

enum bsv_ctl_state
{
   BSV_MOVIE_CTL_NONE = 0,
   BSV_MOVIE_CTL_IS_INITED
};


void bsv_movie_deinit(void);

bool bsv_movie_init(void);

void bsv_movie_frame_rewind(void);

void bsv_movie_set_path(const char *path);

bool bsv_movie_get_input(int16_t *bsv_data);

void bsv_movie_set_input(int16_t *bsv_data);

bool bsv_movie_ctl(enum bsv_ctl_state state, void *data);

bool bsv_movie_check(void);

RETRO_END_DECLS

#endif
