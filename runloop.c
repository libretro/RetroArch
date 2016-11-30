/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2016 - Daniel De Matteis
 *  Copyright (C) 2012-2015 - Michael Lelli
 *  Copyright (C) 2014-2015 - Jay McCarthy
 *  Copyright (C) 2016 - Brad Parker
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

#include <stdarg.h>
#include <math.h>

#include <compat/strl.h>
#include <retro_inline.h>
#include <retro_assert.h>
#include <file/file_path.h>
#include <queues/message_queue.h>
#include <queues/task_queue.h>
#include <string/stdstring.h>
#include <features/features_cpu.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_THREADS
#include <rthreads/rthreads.h>
#endif

#ifdef HAVE_CHEEVOS
#include "cheevos.h"
#endif

#ifdef HAVE_MENU
#include "menu/menu_display.h"
#include "menu/menu_driver.h"
#include "menu/menu_event.h"
#include "menu/widgets/menu_dialog.h"
#endif

#ifdef HAVE_NETWORKING
#include "network/netplay/netplay.h"
#endif

#if defined(HAVE_HTTPSERVER) && defined(HAVE_ZLIB)
#include "network/httpserver/httpserver.h"
#endif

#include "autosave.h"
#include "core_info.h"
#include "configuration.h"
#include "driver.h"
#include "movie.h"
#include "dirs.h"
#include "paths.h"
#include "retroarch.h"
#include "runloop.h"
#include "file_path_special.h"
#include "managers/core_option_manager.h"
#include "managers/cheat_manager.h"
#include "managers/state_manager.h"
#include "list_special.h"
#include "audio/audio_driver.h"
#include "camera/camera_driver.h"
#include "record/record_driver.h"
#include "input/input_driver.h"
#include "ui/ui_companion_driver.h"
#include "core.h"

#include "msg_hash.h"

#include "input/input_keyboard.h"
#include "tasks/tasks_internal.h"
#include "verbosity.h"

#ifdef HAVE_ZLIB
#define DEFAULT_EXT "zip"
#else
#define DEFAULT_EXT ""
#endif

#ifdef HAVE_MENU
#define runloop_cmd_menu_press(current_input, old_input, trigger_input)   (BIT64_GET(trigger_input, RARCH_MENU_TOGGLE) || runloop_cmd_get_state_menu_toggle_button_combo(settings, current_input, old_input, trigger_input))
#endif

enum  runloop_state
{
   RUNLOOP_STATE_NONE = 0,
   RUNLOOP_STATE_ITERATE,
   RUNLOOP_STATE_SLEEP,
   RUNLOOP_STATE_MENU_ITERATE,
   RUNLOOP_STATE_END,
   RUNLOOP_STATE_QUIT
};

static rarch_system_info_t runloop_system;
static struct retro_frame_time_callback runloop_frame_time;
static retro_keyboard_event_t runloop_key_event            = NULL;
static retro_keyboard_event_t runloop_frontend_key_event   = NULL;
static core_option_manager_t *runloop_core_options         = NULL;
#ifdef HAVE_THREADS
static slock_t *_runloop_msg_queue_lock                    = NULL;
#endif
static msg_queue_t *runloop_msg_queue                      = NULL;
static unsigned runloop_pending_windowed_scale             = 0;
static retro_usec_t runloop_frame_time_last                = 0;
static unsigned runloop_max_frames                         = false;
static bool runloop_force_nonblock                         = false;
static bool runloop_frame_time_last_enable                 = false;
static bool runloop_set_frame_limit                        = false;
static bool runloop_paused                                 = false;
static bool runloop_idle                                   = false;
static bool runloop_exec                                   = false;
static bool runloop_slowmotion                             = false;
static bool runloop_shutdown_initiated                     = false;
static bool runloop_core_shutdown_initiated                = false;
static bool runloop_perfcnt_enable                         = false;
static bool runloop_overrides_active                       = false;
static bool runloop_game_options_active                    = false;
static bool runloop_missing_bios                           = false;

global_t *global_get_ptr(void)
{
   static struct global g_extern;
   return &g_extern;
}

void update_firmware_status()
{
   core_info_t *core_info    = NULL;
   settings_t *settings       = config_get_ptr();
   core_info_ctx_firmware_t firmware_info;
   core_info_get_current_core(&core_info);

   if (core_info && settings)
   {
      firmware_info.path         = core_info->path;
      if (!string_is_empty(settings->directory.system))
         firmware_info.directory.system = settings->directory.system;
      else
      {
         char s[PATH_MAX_LENGTH];
         strlcpy(s, path_get(RARCH_PATH_CONTENT) ,sizeof(s));
         path_basedir(s);
         firmware_info.directory.system = s;
      }
      RARCH_LOG("Updating firmware status for: %s on %s\n", core_info->path, 
         firmware_info.directory.system);
      core_info_list_update_missing_firmware(&firmware_info);
   }
}

void runloop_msg_queue_push(const char *msg,
      unsigned prio, unsigned duration,
      bool flush)
{
   runloop_ctx_msg_info_t msg_info;
   settings_t *settings = config_get_ptr();

   if (!settings || !settings->video.font_enable)
      return;

#ifdef HAVE_THREADS
   slock_lock(_runloop_msg_queue_lock);
#endif

   if (flush)
      msg_queue_clear(runloop_msg_queue);

   msg_info.msg      = msg;
   msg_info.prio     = prio;
   msg_info.duration = duration;
   msg_info.flush    = flush;

   if (runloop_msg_queue)
   {
      msg_queue_push(runloop_msg_queue, msg_info.msg,
            msg_info.prio, msg_info.duration);

      if (ui_companion_is_on_foreground())
      {
         const ui_companion_driver_t *ui = ui_companion_get_ptr();
         if (ui->msg_queue_push)
            ui->msg_queue_push(msg_info.msg,
                  msg_info.prio, msg_info.duration, msg_info.flush);
      }
   }

#ifdef HAVE_THREADS
   slock_unlock(_runloop_msg_queue_lock);
#endif
}

char* runloop_msg_queue_pull(void)
{
   runloop_ctx_msg_info_t msg_info;
   runloop_ctl(RUNLOOP_CTL_MSG_QUEUE_PULL, &msg_info);

   return strdup(msg_info.msg);
}

#ifdef HAVE_MENU
static bool runloop_cmd_get_state_menu_toggle_button_combo(
      settings_t *settings,
      uint64_t current_input, uint64_t old_input,
      uint64_t trigger_input)
{
   switch (settings->input.menu_toggle_gamepad_combo)
   {
      case INPUT_TOGGLE_NONE:
         return false;
      case INPUT_TOGGLE_DOWN_Y_L_R:
         if (!BIT64_GET(current_input, RETRO_DEVICE_ID_JOYPAD_DOWN))
            return false;
         if (!BIT64_GET(current_input, RETRO_DEVICE_ID_JOYPAD_Y))
            return false;
         if (!BIT64_GET(current_input, RETRO_DEVICE_ID_JOYPAD_L))
            return false;
         if (!BIT64_GET(current_input, RETRO_DEVICE_ID_JOYPAD_R))
            return false;
         break;
      case INPUT_TOGGLE_L3_R3:
         if (!BIT64_GET(current_input, RETRO_DEVICE_ID_JOYPAD_L3))
            return false;
         if (!BIT64_GET(current_input, RETRO_DEVICE_ID_JOYPAD_R3))
            return false;
         break;
      case INPUT_TOGGLE_L1_R1_START_SELECT:
         if (!BIT64_GET(current_input, RETRO_DEVICE_ID_JOYPAD_START))
            return false;
         if (!BIT64_GET(current_input, RETRO_DEVICE_ID_JOYPAD_SELECT))
            return false;
         if (!BIT64_GET(current_input, RETRO_DEVICE_ID_JOYPAD_L))
            return false;
         if (!BIT64_GET(current_input, RETRO_DEVICE_ID_JOYPAD_R))
            return false;
         break;
      case INPUT_TOGGLE_START_SELECT:
         if (!BIT64_GET(current_input, RETRO_DEVICE_ID_JOYPAD_START))
            return false;
         if (!BIT64_GET(current_input, RETRO_DEVICE_ID_JOYPAD_SELECT))
            return false;
         break;
   }

   input_driver_set_flushing_input();
   return true;
}
#endif

/**
 * rarch_game_specific_options:
 *
 * Returns: true (1) if a game specific core
 * options path has been found,
 * otherwise false (0).
 **/
static bool rarch_game_specific_options(char **output)
{
   char game_path[255];

   game_path[0] ='\0';

   if (!retroarch_validate_game_options(game_path,
            sizeof(game_path), false))
         return false;

   if (!config_file_exists(game_path))
      return false;

   RARCH_LOG("%s %s\n",
         msg_hash_to_str(MSG_GAME_SPECIFIC_CORE_OPTIONS_FOUND_AT),
         game_path);
   *output = strdup(game_path);
   return true;
}


bool runloop_ctl(enum runloop_ctl_state state, void *data)
{

   switch (state)
   {
      case RUNLOOP_CTL_SYSTEM_INFO_INIT:
         core_get_system_info(&runloop_system.info);

         if (!runloop_system.info.library_name)
            runloop_system.info.library_name = msg_hash_to_str(MSG_UNKNOWN);
         if (!runloop_system.info.library_version)
            runloop_system.info.library_version = "v0";

         video_driver_set_title_buf();

         strlcpy(runloop_system.valid_extensions,
               runloop_system.info.valid_extensions ?
               runloop_system.info.valid_extensions : DEFAULT_EXT,
               sizeof(runloop_system.valid_extensions));
         break;
      case RUNLOOP_CTL_GET_CORE_OPTION_SIZE:
         {
            unsigned *idx = (unsigned*)data;
            if (!idx)
               return false;
            *idx = core_option_manager_size(runloop_core_options);
         }
         break;
      case RUNLOOP_CTL_HAS_CORE_OPTIONS:
         return runloop_core_options;
      case RUNLOOP_CTL_CORE_OPTIONS_LIST_GET:
         {
            core_option_manager_t **coreopts = (core_option_manager_t**)data;
            if (!coreopts)
               return false;
            *coreopts = runloop_core_options;
         }
         break;
      case RUNLOOP_CTL_SYSTEM_INFO_GET:
         {
            rarch_system_info_t **system = (rarch_system_info_t**)data;
            if (!system)
               return false;
            *system = &runloop_system;
         }
         break;
      case RUNLOOP_CTL_SYSTEM_INFO_FREE:

         /* No longer valid. */
         if (runloop_system.subsystem.data)
            free(runloop_system.subsystem.data);
         runloop_system.subsystem.data = NULL;
         runloop_system.subsystem.size = 0;

         if (runloop_system.ports.data)
            free(runloop_system.ports.data);
         runloop_system.ports.data = NULL;
         runloop_system.ports.size = 0;

         if (runloop_system.mmaps.descriptors)
            free((void *)runloop_system.mmaps.descriptors);
         runloop_system.mmaps.descriptors     = NULL;
         runloop_system.mmaps.num_descriptors = 0;

         runloop_key_event          = NULL;
         runloop_frontend_key_event = NULL;

         audio_driver_unset_callback();
         memset(&runloop_system, 0, sizeof(rarch_system_info_t));
         break;
      case RUNLOOP_CTL_SET_FRAME_TIME_LAST:
         runloop_frame_time_last_enable = true;
         break;
      case RUNLOOP_CTL_SET_OVERRIDES_ACTIVE:
         runloop_overrides_active = true;
         break;
      case RUNLOOP_CTL_UNSET_OVERRIDES_ACTIVE:
         runloop_overrides_active = false;
         break;
      case RUNLOOP_CTL_IS_OVERRIDES_ACTIVE:
         return runloop_overrides_active;
      case RUNLOOP_CTL_SET_MISSING_BIOS:
         runloop_missing_bios = true;
         break;
      case RUNLOOP_CTL_UNSET_MISSING_BIOS:
         runloop_missing_bios = false;
         break;
      case RUNLOOP_CTL_IS_MISSING_BIOS:
         return runloop_missing_bios;
      case RUNLOOP_CTL_SET_GAME_OPTIONS_ACTIVE:
         runloop_game_options_active = true;
         break;
      case RUNLOOP_CTL_UNSET_GAME_OPTIONS_ACTIVE:
         runloop_game_options_active = false;
         break;
      case RUNLOOP_CTL_IS_GAME_OPTIONS_ACTIVE:
         return runloop_game_options_active;
      case RUNLOOP_CTL_SET_FRAME_LIMIT:
         runloop_set_frame_limit = true;
         break;
      case RUNLOOP_CTL_GET_PERFCNT:
         {
            bool **perfcnt = (bool**)data;
            if (!perfcnt)
               return false;
            *perfcnt = &runloop_perfcnt_enable;
         }
         break;
      case RUNLOOP_CTL_SET_PERFCNT_ENABLE:
         runloop_perfcnt_enable = true;
         break;
      case RUNLOOP_CTL_UNSET_PERFCNT_ENABLE:
         runloop_perfcnt_enable = false;
         break;
      case RUNLOOP_CTL_IS_PERFCNT_ENABLE:
         return runloop_perfcnt_enable;
      case RUNLOOP_CTL_SET_NONBLOCK_FORCED:
         runloop_force_nonblock = true;
         break;
      case RUNLOOP_CTL_UNSET_NONBLOCK_FORCED:
         runloop_force_nonblock = false;
         break;
      case RUNLOOP_CTL_IS_NONBLOCK_FORCED:
         return runloop_force_nonblock;
      case RUNLOOP_CTL_SET_FRAME_TIME:
         {
            const struct retro_frame_time_callback *info =
               (const struct retro_frame_time_callback*)data;
#ifdef HAVE_NETWORKING
            /* retro_run() will be called in very strange and
             * mysterious ways, have to disable it. */
            if (netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_ENABLED, NULL))
               return false;
#endif
            runloop_frame_time = *info;
         }
         break;
      case RUNLOOP_CTL_GET_WINDOWED_SCALE:
         {
            unsigned **scale = (unsigned**)data;
            if (!scale)
               return false;
            *scale       = (unsigned*)&runloop_pending_windowed_scale;
         }
         break;
      case RUNLOOP_CTL_SET_WINDOWED_SCALE:
         {
            unsigned *idx = (unsigned*)data;
            if (!idx)
               return false;
            runloop_pending_windowed_scale = *idx;
         }
         break;
      case RUNLOOP_CTL_SET_LIBRETRO_PATH:
         return path_set(RARCH_PATH_CORE, (const char*)data);
      case RUNLOOP_CTL_FRAME_TIME_FREE:
         memset(&runloop_frame_time, 0,
               sizeof(struct retro_frame_time_callback));
         runloop_frame_time_last           = 0;
         runloop_max_frames                = 0;
         break;
      case RUNLOOP_CTL_STATE_FREE:
         runloop_perfcnt_enable            = false;
         runloop_idle                      = false;
         runloop_paused                    = false;
         runloop_slowmotion                = false;
         runloop_frame_time_last_enable    = false;
         runloop_set_frame_limit           = false;
         runloop_overrides_active          = false;
         runloop_ctl(RUNLOOP_CTL_FRAME_TIME_FREE, NULL);
         break;
      case RUNLOOP_CTL_GLOBAL_FREE:
         {
            global_t *global = NULL;
            command_event(CMD_EVENT_TEMPORARY_CONTENT_DEINIT, NULL);

            path_deinit_subsystem();
            command_event(CMD_EVENT_RECORD_DEINIT, NULL);
            command_event(CMD_EVENT_LOG_FILE_DEINIT, NULL);

            rarch_ctl(RARCH_CTL_UNSET_BLOCK_CONFIG_READ, NULL);
            rarch_ctl(RARCH_CTL_UNSET_SRAM_LOAD_DISABLED, NULL);
            rarch_ctl(RARCH_CTL_UNSET_SRAM_SAVE_DISABLED, NULL);
            rarch_ctl(RARCH_CTL_UNSET_SRAM_ENABLE, NULL);
            rarch_ctl(RARCH_CTL_UNSET_BPS_PREF, NULL);
            rarch_ctl(RARCH_CTL_UNSET_IPS_PREF, NULL);
            rarch_ctl(RARCH_CTL_UNSET_UPS_PREF, NULL);
            rarch_ctl(RARCH_CTL_UNSET_PATCH_BLOCKED, NULL);
            runloop_overrides_active   = false;

            core_unset_input_descriptors();

            global = global_get_ptr();
            path_clear_all();
            dir_clear_all();
            memset(global, 0, sizeof(struct global));
            retroarch_override_setting_free_state();
         }
         break;
      case RUNLOOP_CTL_CLEAR_STATE:
         driver_ctl(RARCH_DRIVER_CTL_DEINIT,  NULL);
         runloop_ctl(RUNLOOP_CTL_STATE_FREE,  NULL);
         runloop_ctl(RUNLOOP_CTL_GLOBAL_FREE, NULL);
         break;
      case RUNLOOP_CTL_SET_MAX_FRAMES:
         {
            unsigned *ptr = (unsigned*)data;
            if (!ptr)
               return false;
            runloop_max_frames = *ptr;
         }
         break;
      case RUNLOOP_CTL_IS_IDLE:
         return runloop_idle;
      case RUNLOOP_CTL_SET_IDLE:
         {
            bool *ptr = (bool*)data;
            if (!ptr)
               return false;
            runloop_idle = *ptr;
         }
         break;
      case RUNLOOP_CTL_IS_SLOWMOTION:
         return runloop_slowmotion;
      case RUNLOOP_CTL_SET_SLOWMOTION:
         {
            bool *ptr = (bool*)data;
            if (!ptr)
               return false;
            runloop_slowmotion = *ptr;
         }
         break;
      case RUNLOOP_CTL_SET_PAUSED:
         {
            bool *ptr = (bool*)data;
            if (!ptr)
               return false;
            runloop_paused = *ptr;
         }
         break;
      case RUNLOOP_CTL_IS_PAUSED:
         return runloop_paused;
      case RUNLOOP_CTL_MSG_QUEUE_PULL:
#ifdef HAVE_THREADS
         slock_lock(_runloop_msg_queue_lock);
#endif
         {
            const char **ret = (const char**)data;
            if (!ret)
            {
#ifdef HAVE_THREADS
               slock_unlock(_runloop_msg_queue_lock);
#endif
               return false;
            }
            *ret = msg_queue_pull(runloop_msg_queue);
         }
#ifdef HAVE_THREADS
         slock_unlock(_runloop_msg_queue_lock);
#endif
         break;
      case RUNLOOP_CTL_MSG_QUEUE_DEINIT:
         if (!runloop_msg_queue)
            return true;

#ifdef HAVE_THREADS
         slock_lock(_runloop_msg_queue_lock);
#endif

         msg_queue_free(runloop_msg_queue);

#ifdef HAVE_THREADS
         slock_unlock(_runloop_msg_queue_lock);
#endif

#ifdef HAVE_THREADS
         slock_free(_runloop_msg_queue_lock);
         _runloop_msg_queue_lock = NULL;
#endif

         runloop_msg_queue = NULL;
         break;
      case RUNLOOP_CTL_MSG_QUEUE_INIT:
         runloop_ctl(RUNLOOP_CTL_MSG_QUEUE_DEINIT, NULL);
         runloop_msg_queue = msg_queue_new(8);
         retro_assert(runloop_msg_queue);

#ifdef HAVE_THREADS
         _runloop_msg_queue_lock = slock_new();
         retro_assert(_runloop_msg_queue_lock);
#endif
         break;
      case RUNLOOP_CTL_TASK_INIT:
         {
#ifdef HAVE_THREADS
            settings_t *settings = config_get_ptr();
            bool threaded_enable = settings->threaded_data_runloop_enable;
#else
            bool threaded_enable = false;
#endif
            task_queue_ctl(TASK_QUEUE_CTL_DEINIT, NULL);
            task_queue_ctl(TASK_QUEUE_CTL_INIT, &threaded_enable);
         }
         break;
      case RUNLOOP_CTL_SET_CORE_SHUTDOWN:
         runloop_core_shutdown_initiated = true;
         break;
      case RUNLOOP_CTL_SET_SHUTDOWN:
         runloop_shutdown_initiated = true;
         break;
      case RUNLOOP_CTL_IS_SHUTDOWN:
         return runloop_shutdown_initiated;
      case RUNLOOP_CTL_SET_EXEC:
         runloop_exec = true;
         break;
      case RUNLOOP_CTL_DATA_DEINIT:
         task_queue_ctl(TASK_QUEUE_CTL_DEINIT, NULL);
         break;
      case RUNLOOP_CTL_IS_CORE_OPTION_UPDATED:
         if (!runloop_core_options)
            return false;
         return  core_option_manager_updated(runloop_core_options);
      case RUNLOOP_CTL_CORE_OPTION_PREV:
         {
            unsigned *idx = (unsigned*)data;
            if (!idx)
               return false;
            core_option_manager_prev(runloop_core_options, *idx);
            if (ui_companion_is_on_foreground())
               ui_companion_driver_notify_refresh();
         }
         break;
      case RUNLOOP_CTL_CORE_OPTION_NEXT:
         {
            unsigned *idx = (unsigned*)data;
            if (!idx)
               return false;
            core_option_manager_next(runloop_core_options, *idx);
            if (ui_companion_is_on_foreground())
               ui_companion_driver_notify_refresh();
         }
         break;
      case RUNLOOP_CTL_CORE_OPTIONS_GET:
         {
            struct retro_variable *var = (struct retro_variable*)data;

            if (!runloop_core_options || !var)
               return false;

            RARCH_LOG("Environ GET_VARIABLE %s:\n", var->key);
            core_option_manager_get(runloop_core_options, var);
            RARCH_LOG("\t%s\n", var->value ? var->value :
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE));
         }
         break;
      case RUNLOOP_CTL_CORE_OPTIONS_INIT:
         {
            settings_t *settings              = config_get_ptr();
            char *game_options_path           = NULL;
            bool ret                          = false;
            const struct retro_variable *vars =
               (const struct retro_variable*)data;

            if (settings && settings->game_specific_options)
               ret = rarch_game_specific_options(&game_options_path);

            if(ret)
            {
               runloop_ctl(RUNLOOP_CTL_SET_GAME_OPTIONS_ACTIVE, NULL);
               runloop_core_options =
                  core_option_manager_new(game_options_path, vars);
               free(game_options_path);
            }
            else
            {
               char buf[PATH_MAX_LENGTH];
               const char *options_path          = NULL;

               buf[0] = '\0';

               if (settings)
                  options_path = settings->path.core_options;

               if (string_is_empty(options_path) && !path_is_empty(RARCH_PATH_CONFIG))
               {
                  fill_pathname_resolve_relative(buf, path_get(RARCH_PATH_CONFIG),
                        file_path_str(FILE_PATH_CORE_OPTIONS_CONFIG), sizeof(buf));
                  options_path = buf;
               }

               runloop_ctl(RUNLOOP_CTL_UNSET_GAME_OPTIONS_ACTIVE, NULL);

               if (!string_is_empty(options_path))
                  runloop_core_options =
                     core_option_manager_new(options_path, vars);
            }

         }
         break;
      case RUNLOOP_CTL_CORE_OPTIONS_FREE:
         if (runloop_core_options)
            core_option_manager_free(runloop_core_options);
         runloop_core_options          = NULL;
         break;
      case RUNLOOP_CTL_CORE_OPTIONS_DEINIT:
         {
            if (!runloop_core_options)
               return false;

            /* check if game options file was just created and flush
               to that file instead */
            if(!path_is_empty(RARCH_PATH_CORE_OPTIONS))
            {
               core_option_manager_flush_game_specific(runloop_core_options,
                     path_get(RARCH_PATH_CORE_OPTIONS));
               path_clear(RARCH_PATH_CORE_OPTIONS);
            }
            else
               core_option_manager_flush(runloop_core_options);

            if (runloop_ctl(RUNLOOP_CTL_IS_GAME_OPTIONS_ACTIVE, NULL))
               runloop_ctl(RUNLOOP_CTL_UNSET_GAME_OPTIONS_ACTIVE, NULL);

            runloop_ctl(RUNLOOP_CTL_CORE_OPTIONS_FREE, NULL);
         }
         break;
      case RUNLOOP_CTL_KEY_EVENT_GET:
         {
            retro_keyboard_event_t **key_event =
               (retro_keyboard_event_t**)data;
            if (!key_event)
               return false;
            *key_event = &runloop_key_event;
         }
         break;
      case RUNLOOP_CTL_FRONTEND_KEY_EVENT_GET:
         {
            retro_keyboard_event_t **key_event =
               (retro_keyboard_event_t**)data;
            if (!key_event)
               return false;
            *key_event = &runloop_frontend_key_event;
         }
         break;
      case RUNLOOP_CTL_HTTPSERVER_INIT:
#if defined(HAVE_HTTPSERVER) && defined(HAVE_ZLIB)
         httpserver_init(8888);
#endif
         break;
      case RUNLOOP_CTL_HTTPSERVER_DESTROY:
#if defined(HAVE_HTTPSERVER) && defined(HAVE_ZLIB)
         httpserver_destroy();
#endif
         break;
      case RUNLOOP_CTL_NONE:
      default:
         break;
   }

   return true;
}

/* Time to exit out of the main loop?
 * Reasons for exiting:
 * a) Shutdown environment callback was invoked.
 * b) Quit key was pressed.
 * c) Frame count exceeds or equals maximum amount of frames to run.
 * d) Video driver no longer alive.
 * e) End of BSV movie and BSV EOF exit is true. (TODO/FIXME - explain better)
 */
static INLINE int runloop_iterate_time_to_exit(bool quit_key_pressed)
{
   settings_t *settings          = config_get_ptr();
   bool time_to_exit             = runloop_shutdown_initiated;
   time_to_exit                  = time_to_exit || quit_key_pressed;
   time_to_exit                  = time_to_exit || !video_driver_is_alive();
   time_to_exit                  = time_to_exit || bsv_movie_ctl(BSV_MOVIE_CTL_END_EOF, NULL);
   time_to_exit                  = time_to_exit || (runloop_max_frames && 
                                   (*(video_driver_get_frame_count_ptr()) 
                                    >= runloop_max_frames));
   time_to_exit                  = time_to_exit || runloop_exec;

   if (!time_to_exit)
      return 1;

   if (runloop_exec)
      runloop_exec = false;

   if (runloop_core_shutdown_initiated &&
         settings && settings->load_dummy_on_core_shutdown)
   {
      content_ctx_info_t content_info = {0};
      if (!task_push_content_load_default(
               NULL, NULL,
               &content_info,
               CORE_TYPE_DUMMY,
               CONTENT_MODE_LOAD_NOTHING_WITH_DUMMY_CORE,
               NULL, NULL))
         return -1;

      /* Loads dummy core instead of exiting RetroArch completely.
       * Aborts core shutdown if invoked. */
      runloop_shutdown_initiated      = false;
      runloop_core_shutdown_initiated = false;

      return 1;
   }

   /* Quits out of RetroArch main loop. */
   return -1;
}

static enum runloop_state runloop_check_state(
      settings_t *settings,
      uint64_t current_input,
      uint64_t old_input,
      unsigned *sleep_ms)
{
   static bool old_focus            = true;
#ifdef HAVE_OVERLAY
   static char prev_overlay_restore = false;
#endif
#ifdef HAVE_NETWORKING
   bool tmp                         = false;
#endif
   bool focused                     = true;
   uint64_t trigger_input           = current_input & ~old_input;
   bool pause_pressed               = runloop_cmd_triggered(trigger_input, RARCH_PAUSE_TOGGLE);

   if (input_driver_is_flushing_input())
   {
      input_driver_unset_flushing_input();
      if (current_input)
      {
         current_input = 0;

         /* If core was paused before entering menu, evoke
          * pause toggle to wake it up. */
         if (runloop_paused)
            BIT64_SET(current_input, RARCH_PAUSE_TOGGLE);
         input_driver_set_flushing_input();
      }
   }

   if (menu_driver_is_binding_state())
      trigger_input = 0;

   if (runloop_cmd_triggered(trigger_input, RARCH_OVERLAY_NEXT))
      command_event(CMD_EVENT_OVERLAY_NEXT, NULL);

   if (runloop_cmd_triggered(trigger_input, RARCH_FULLSCREEN_TOGGLE_KEY))
   {
      bool fullscreen_toggled = !runloop_paused;
#ifdef HAVE_MENU
      fullscreen_toggled = fullscreen_toggled ||
         menu_driver_ctl(RARCH_MENU_CTL_IS_ALIVE, NULL);
#endif

      if (fullscreen_toggled)
         command_event(CMD_EVENT_FULLSCREEN_TOGGLE, NULL);
   }

   if (runloop_cmd_triggered(trigger_input, RARCH_GRAB_MOUSE_TOGGLE))
      command_event(CMD_EVENT_GRAB_MOUSE_TOGGLE, NULL);


#ifdef HAVE_OVERLAY
   if (input_keyboard_ctl(
            RARCH_INPUT_KEYBOARD_CTL_IS_LINEFEED_ENABLED, NULL))
   {
      prev_overlay_restore  = false;
      command_event(CMD_EVENT_OVERLAY_INIT, NULL);
   }
   else if (prev_overlay_restore)
   {
      if (!settings->input.overlay_hide_in_menu)
         command_event(CMD_EVENT_OVERLAY_INIT, NULL);
      prev_overlay_restore = false;
   }
#endif

   if (runloop_iterate_time_to_exit(
            runloop_cmd_press(trigger_input, RARCH_QUIT_KEY)) != 1)
      return RUNLOOP_STATE_QUIT;

      #ifdef HAVE_MENU
         if (menu_driver_ctl(RARCH_MENU_CTL_IS_ALIVE, NULL))
         {
            menu_ctx_iterate_t iter;
            core_poll();

            {
               enum menu_action action = (enum menu_action)menu_event(current_input, trigger_input);
               bool focused            = settings->pause_nonactive ? video_driver_is_focused() : true;

               focused                 = focused && !ui_companion_is_on_foreground();

               iter.action             = action;

               if (!menu_driver_ctl(RARCH_MENU_CTL_ITERATE, &iter))
                  rarch_ctl(RARCH_CTL_MENU_RUNNING_FINISHED, NULL);

               if (focused || !runloop_idle)
                  menu_driver_ctl(RARCH_MENU_CTL_RENDER, NULL);

               if (!focused)
                  return RUNLOOP_STATE_SLEEP;

               if (action == MENU_ACTION_QUIT && !menu_driver_is_binding_state())
                  return RUNLOOP_STATE_QUIT;
            }
         }
      #endif

   if (runloop_idle)
      return RUNLOOP_STATE_SLEEP;

#ifdef HAVE_MENU
   if (menu_event_keyboard_is_set(RETROK_F1) == 1)
   {
      if (menu_driver_ctl(RARCH_MENU_CTL_IS_ALIVE, NULL))
      {
         if (rarch_ctl(RARCH_CTL_IS_INITED, NULL) &&
               !rarch_ctl(RARCH_CTL_IS_DUMMY_CORE, NULL))
         {
            rarch_ctl(RARCH_CTL_MENU_RUNNING_FINISHED, NULL);
            menu_event_keyboard_set(false, RETROK_F1);
         }
      }
   }
   else if ((!menu_event_keyboard_is_set(RETROK_F1) && runloop_cmd_menu_press(current_input, old_input, trigger_input)) ||
         rarch_ctl(RARCH_CTL_IS_DUMMY_CORE, NULL))
   {
      if (menu_driver_ctl(RARCH_MENU_CTL_IS_ALIVE, NULL))
      {
         if (rarch_ctl(RARCH_CTL_IS_INITED, NULL) &&
               !rarch_ctl(RARCH_CTL_IS_DUMMY_CORE, NULL))
            rarch_ctl(RARCH_CTL_MENU_RUNNING_FINISHED, NULL);
      }
      else
      {
        menu_display_toggle_set_reason(MENU_TOGGLE_REASON_USER);
        rarch_ctl(RARCH_CTL_MENU_RUNNING, NULL);
      }
   }
   else
      menu_event_keyboard_set(false, RETROK_F1);

   if (menu_driver_ctl(RARCH_MENU_CTL_IS_ALIVE, NULL))
   {
      if (!settings->menu.throttle_framerate && !settings->fastforward_ratio)
         return RUNLOOP_STATE_MENU_ITERATE;

      return RUNLOOP_STATE_END;
   }
#endif

   if (settings->pause_nonactive)
      focused                = video_driver_is_focused();

   if (runloop_cmd_triggered(trigger_input, RARCH_SCREENSHOT))
      command_event(CMD_EVENT_TAKE_SCREENSHOT, NULL);

   if (runloop_cmd_triggered(trigger_input, RARCH_MUTE))
      command_event(CMD_EVENT_AUDIO_MUTE_TOGGLE, NULL);

   if (runloop_cmd_triggered(trigger_input, RARCH_OSK))
   {
      if (input_keyboard_ctl(
               RARCH_INPUT_KEYBOARD_CTL_IS_LINEFEED_ENABLED, NULL))
         input_keyboard_ctl(
               RARCH_INPUT_KEYBOARD_CTL_UNSET_LINEFEED_ENABLED, NULL);
      else
         input_keyboard_ctl(
               RARCH_INPUT_KEYBOARD_CTL_SET_LINEFEED_ENABLED, NULL);
   }

   if (runloop_cmd_press(current_input, RARCH_VOLUME_UP))
      command_event(CMD_EVENT_VOLUME_UP, NULL);
   else if (runloop_cmd_press(current_input, RARCH_VOLUME_DOWN))
      command_event(CMD_EVENT_VOLUME_DOWN, NULL);

#ifdef HAVE_NETWORKING
   tmp = runloop_cmd_triggered(trigger_input, RARCH_NETPLAY_FLIP);
   netplay_driver_ctl(RARCH_NETPLAY_CTL_FLIP_PLAYERS, &tmp);
   tmp = runloop_cmd_triggered(trigger_input, RARCH_FULLSCREEN_TOGGLE_KEY);
#endif

   /* Check if libretro pause key was pressed. If so, pause or
    * unpause the libretro core. */

   /* FRAMEADVANCE will set us into pause mode. */
   pause_pressed |= !runloop_paused && runloop_cmd_triggered(trigger_input, RARCH_FRAMEADVANCE);

   if (focused && pause_pressed)
      command_event(CMD_EVENT_PAUSE_TOGGLE, NULL);
   else if (focused && !old_focus)
      command_event(CMD_EVENT_UNPAUSE, NULL);
   else if (!focused && old_focus)
      command_event(CMD_EVENT_PAUSE, NULL);

   old_focus = focused;

   if (!focused)
      return RUNLOOP_STATE_SLEEP;

   if (runloop_paused)
   {
      /* check pause state */

      bool check_is_oneshot = runloop_cmd_triggered(trigger_input,
            RARCH_FRAMEADVANCE)
         || runloop_cmd_press(current_input, RARCH_REWIND);
      if (runloop_cmd_triggered(trigger_input, RARCH_FULLSCREEN_TOGGLE_KEY))
      {
         command_event(CMD_EVENT_FULLSCREEN_TOGGLE, NULL);
         if (!runloop_ctl(RUNLOOP_CTL_IS_IDLE, NULL))
            video_driver_cached_frame();
      }

      if (!check_is_oneshot)
         return RUNLOOP_STATE_SLEEP;
   }

   /* To avoid continous switching if we hold the button down, we require
    * that the button must go from pressed to unpressed back to pressed
    * to be able to toggle between then.
    */
   if (runloop_cmd_triggered(trigger_input, RARCH_FAST_FORWARD_KEY))
   {
      if (input_driver_is_nonblock_state())
         input_driver_unset_nonblock_state();
      else
         input_driver_set_nonblock_state();
      driver_ctl(RARCH_DRIVER_CTL_SET_NONBLOCK_STATE, NULL);
   }
   else if ((runloop_cmd_pressed(old_input, RARCH_FAST_FORWARD_HOLD_KEY) 
         != runloop_cmd_press(current_input, RARCH_FAST_FORWARD_HOLD_KEY)))
   {
      if (runloop_cmd_press(current_input, RARCH_FAST_FORWARD_HOLD_KEY))
         input_driver_set_nonblock_state();
      else
         input_driver_unset_nonblock_state();
      driver_ctl(RARCH_DRIVER_CTL_SET_NONBLOCK_STATE, NULL);
   }

   /* Checks if the state increase/decrease keys have been pressed 
    * for this frame. */
   if (runloop_cmd_triggered(trigger_input, RARCH_STATE_SLOT_PLUS))
   {
      char msg[128];

      msg[0] = '\0';

      settings->state_slot++;

      snprintf(msg, sizeof(msg), "%s: %d",
            msg_hash_to_str(MSG_STATE_SLOT),
            settings->state_slot);

      runloop_msg_queue_push(msg, 2, 180, true);

      RARCH_LOG("%s\n", msg);
   }
   else if (runloop_cmd_triggered(trigger_input, RARCH_STATE_SLOT_MINUS))
   {
      char msg[128];

      msg[0] = '\0';

      if (settings->state_slot > 0)
         settings->state_slot--;

      snprintf(msg, sizeof(msg), "%s: %d",
            msg_hash_to_str(MSG_STATE_SLOT),
            settings->state_slot);

      runloop_msg_queue_push(msg, 2, 180, true);

      RARCH_LOG("%s\n", msg);
   }

   if (runloop_cmd_triggered(trigger_input, RARCH_SAVE_STATE_KEY))
      command_event(CMD_EVENT_SAVE_STATE, NULL);
   else if (runloop_cmd_triggered(trigger_input, RARCH_LOAD_STATE_KEY))
      command_event(CMD_EVENT_LOAD_STATE, NULL);

#ifdef HAVE_CHEEVOS
   if (!settings->cheevos.hardcore_mode_enable)
#endif
      state_manager_check_rewind(runloop_cmd_press(current_input, RARCH_REWIND));

   runloop_slowmotion = runloop_cmd_press(current_input, RARCH_SLOWMOTION);

   if (runloop_slowmotion)
   {
      /* Checks if slowmotion toggle/hold was being pressed and/or held. */
      if (settings->video.black_frame_insertion)
      {
         if (!runloop_ctl(RUNLOOP_CTL_IS_IDLE, NULL))
            video_driver_cached_frame();
      }

      if (state_manager_frame_is_reversed())
         runloop_msg_queue_push(msg_hash_to_str(MSG_SLOW_MOTION_REWIND), 2, 30, true);
      else
         runloop_msg_queue_push(msg_hash_to_str(MSG_SLOW_MOTION), 2, 30, true);
   }

   if (runloop_cmd_triggered(trigger_input, RARCH_MOVIE_RECORD_TOGGLE))
      bsv_movie_check();

   if (runloop_cmd_triggered(trigger_input, RARCH_SHADER_NEXT) ||
      runloop_cmd_triggered(trigger_input, RARCH_SHADER_PREV))
      dir_check_shader(
            runloop_cmd_triggered(trigger_input, RARCH_SHADER_NEXT),
            runloop_cmd_triggered(trigger_input, RARCH_SHADER_PREV));

   if (runloop_cmd_triggered(trigger_input, RARCH_DISK_EJECT_TOGGLE))
      command_event(CMD_EVENT_DISK_EJECT_TOGGLE, NULL);
   else if (runloop_cmd_triggered(trigger_input, RARCH_DISK_NEXT))
      command_event(CMD_EVENT_DISK_NEXT, NULL);
   else if (runloop_cmd_triggered(trigger_input, RARCH_DISK_PREV))
      command_event(CMD_EVENT_DISK_PREV, NULL);

   if (runloop_cmd_triggered(trigger_input, RARCH_RESET))
      command_event(CMD_EVENT_RESET, NULL);

   cheat_manager_state_checks(
         runloop_cmd_triggered(trigger_input, RARCH_CHEAT_INDEX_PLUS),
         runloop_cmd_triggered(trigger_input, RARCH_CHEAT_INDEX_MINUS),
         runloop_cmd_triggered(trigger_input, RARCH_CHEAT_TOGGLE));

   return RUNLOOP_STATE_ITERATE;
}


/**
 * runloop_iterate:
 *
 * Run Libretro core in RetroArch for one frame.
 *
 * Returns: 0 on success, 1 if we have to wait until
 * button input in order to wake up the loop,
 * -1 if we forcibly quit out of the RetroArch iteration loop.
 **/
int runloop_iterate(unsigned *sleep_ms)
{
   unsigned i;
   retro_time_t current, target, to_sleep_ms;
   static uint64_t last_input                   = 0;
   enum runloop_state runloop_status            = RUNLOOP_STATE_NONE;
   static retro_time_t frame_limit_minimum_time = 0.0;
   static retro_time_t frame_limit_last_time    = 0.0;
   settings_t *settings                         = config_get_ptr();
   uint64_t current_input                       = menu_driver_ctl(RARCH_MENU_CTL_IS_ALIVE, NULL) ? input_menu_keys_pressed() : input_keys_pressed();
   uint64_t old_input                           = last_input;
   static char old_core[PATH_MAX_LENGTH]        = "";

   if (!string_is_equal(old_core, path_get(RARCH_PATH_CORE)))
   {
      update_firmware_status();
      strlcpy (old_core, path_get(RARCH_PATH_CORE), sizeof(old_core));
   }

   last_input                                   = current_input;

   if (runloop_frame_time_last_enable)
   {
      runloop_frame_time_last        = 0;
      runloop_frame_time_last_enable = false;
   }

   if (runloop_set_frame_limit)
   {
      struct retro_system_av_info *av_info =
         video_viewport_get_system_av_info();
      float fastforward_ratio              =
         (settings->fastforward_ratio == 0.0f)
         ? 1.0f : settings->fastforward_ratio;

      frame_limit_last_time    = cpu_features_get_time_usec();
      frame_limit_minimum_time = (retro_time_t)roundf(1000000.0f
            / (av_info->timing.fps * fastforward_ratio));

      runloop_set_frame_limit = false;
   }

   if (runloop_frame_time.callback)
   {
      /* Updates frame timing if frame timing callback is in use by the core.
       * Limits frame time if fast forward ratio throttle is enabled. */

      retro_time_t current     = cpu_features_get_time_usec();
      retro_time_t delta       = current - runloop_frame_time_last;
      bool is_locked_fps       = (runloop_paused ||
                                  input_driver_is_nonblock_state()) |
                                  !!recording_driver_get_data_ptr();


      if (!runloop_frame_time_last || is_locked_fps)
         delta = runloop_frame_time.reference;

      if (!is_locked_fps && runloop_slowmotion)
         delta /= settings->slowmotion_ratio;

      runloop_frame_time_last = current;

      if (is_locked_fps)
         runloop_frame_time_last = 0;

      runloop_frame_time.callback(delta);
   }

   runloop_status = runloop_check_state(settings, current_input,
         old_input, sleep_ms);

   switch (runloop_status)
   {
      case RUNLOOP_STATE_QUIT:
         frame_limit_last_time = 0.0;
         command_event(CMD_EVENT_QUIT, NULL);
         return -1;
      case RUNLOOP_STATE_SLEEP:
      case RUNLOOP_STATE_END:
      case RUNLOOP_STATE_MENU_ITERATE:
         core_poll();
#ifdef HAVE_NETWORKING
         /* FIXME: This is an ugly way to tell Netplay this... */
         netplay_driver_ctl(RARCH_NETPLAY_CTL_PAUSE, NULL);
#endif
         if (runloop_status == RUNLOOP_STATE_SLEEP)
            *sleep_ms = 10;
         if (runloop_status == RUNLOOP_STATE_END)
            goto end;
         if (runloop_status == RUNLOOP_STATE_MENU_ITERATE)
            return 0;
         return 1;
      case RUNLOOP_STATE_ITERATE:
      case RUNLOOP_STATE_NONE:
      default:
         break;
   }

   autosave_lock();

   if (bsv_movie_ctl(BSV_MOVIE_CTL_IS_INITED, NULL))
      bsv_movie_ctl(BSV_MOVIE_CTL_SET_FRAME_START, NULL);

   camera_driver_ctl(RARCH_CAMERA_CTL_POLL, NULL);

   /* Update binds for analog dpad modes. */
   for (i = 0; i < settings->input.max_users; i++)
   {
      if (!settings->input.analog_dpad_mode[i])
         continue;

      input_push_analog_dpad(settings->input.binds[i],
            settings->input.analog_dpad_mode[i]);
      input_push_analog_dpad(settings->input.autoconf_binds[i],
            settings->input.analog_dpad_mode[i]);
   }

   if ((settings->video.frame_delay > 0) &&
         !input_driver_is_nonblock_state())
      retro_sleep(settings->video.frame_delay);

   core_run();

#ifdef HAVE_CHEEVOS
   cheevos_test();
#endif

   for (i = 0; i < settings->input.max_users; i++)
   {
      if (!settings->input.analog_dpad_mode[i])
         continue;

      input_pop_analog_dpad(settings->input.binds[i]);
      input_pop_analog_dpad(settings->input.autoconf_binds[i]);
   }

   if (bsv_movie_ctl(BSV_MOVIE_CTL_IS_INITED, NULL))
      bsv_movie_ctl(BSV_MOVIE_CTL_SET_FRAME_END, NULL);

   autosave_unlock();

   if (!settings->fastforward_ratio)
      return 0;

end:

   current                        = cpu_features_get_time_usec();
   target                         = frame_limit_last_time +
      frame_limit_minimum_time;
   to_sleep_ms                    = (target - current) / 1000;

   if (to_sleep_ms > 0)
   {
      *sleep_ms = (unsigned)to_sleep_ms;
      /* Combat jitter a bit. */
      frame_limit_last_time += frame_limit_minimum_time;
      return 1;
   }

   frame_limit_last_time  = cpu_features_get_time_usec();

   return 0;
}
