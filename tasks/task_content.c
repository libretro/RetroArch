/*  RetroArch - A frontend for libretro.
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

#include <stdlib.h>

#include <file/file_path.h>

#ifdef HAVE_MENU
#include "../menu/menu_driver.h"
#include "../menu/menu_display.h"
#include "../menu/menu_content.h"
#endif

#include "tasks_internal.h"

#include "../command.h"
#include "../content.h"
#include "../defaults.h"
#include "../configuration.h"
#include "../frontend/frontend.h"
#include "../retroarch.h"
#include "../verbosity.h"

/* TODO/FIXME - turn this into actual task */

#ifdef HAVE_MENU
static void menu_content_environment_get(int *argc, char *argv[],
      void *args, void *params_data)
{
   struct rarch_main_wrap *wrap_args = (struct rarch_main_wrap*)params_data;
   char *fullpath                    = NULL;
   global_t *global                  = global_get_ptr();
   settings_t *settings              = config_get_ptr();
    
   if (!wrap_args)
      return;

   runloop_ctl(RUNLOOP_CTL_GET_CONTENT_PATH, &fullpath);

   wrap_args->no_content       = menu_driver_ctl(
         RARCH_MENU_CTL_HAS_LOAD_NO_CONTENT, NULL);

   if (!global->has_set.verbosity)
      wrap_args->verbose       = *retro_main_verbosity();

   wrap_args->touched          = true;
   wrap_args->config_path      = NULL;
   wrap_args->sram_path        = NULL;
   wrap_args->state_path       = NULL;
   wrap_args->content_path     = NULL;

   if (*global->path.config)
      wrap_args->config_path   = global->path.config;
   if (*global->dir.savefile)
      wrap_args->sram_path     = global->dir.savefile;
   if (*global->dir.savestate)
      wrap_args->state_path    = global->dir.savestate;
   if (*fullpath)
      wrap_args->content_path  = fullpath;
   if (!global->has_set.libretro)
      wrap_args->libretro_path = *settings->path.libretro 
         ? settings->path.libretro : NULL;

}

/**
 * menu_content_load:
 *
 * Loads content into currently selected core.
 * Will also optionally push the content entry to the history playlist.
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/

static bool menu_content_load(void)
{
   content_ctx_info_t content_info;
   char name[PATH_MAX_LENGTH];
   char msg[PATH_MAX_LENGTH];
   char *fullpath       = NULL;

   runloop_ctl(RUNLOOP_CTL_GET_CONTENT_PATH, &fullpath);
   /* redraw menu frame */
   menu_display_set_msg_force(true);
   menu_driver_ctl(RARCH_MENU_CTL_RENDER, NULL);

   if (*fullpath)
      fill_pathname_base(name, fullpath, sizeof(name));

   content_info.argc        = 0;
   content_info.argv        = NULL;
   content_info.args        = NULL;
   content_info.environ_get = menu_content_environment_get;

   if (!content_load(&content_info))
      goto error;

   if (*fullpath)
   {
      snprintf(msg, sizeof(msg), "INFO - Loading %s ...", name);
      runloop_msg_queue_push(msg, 1, 1, false);
   }

   if (*fullpath || 
         menu_driver_ctl(RARCH_MENU_CTL_HAS_LOAD_NO_CONTENT, NULL))
   {
      struct retro_system_info *info = NULL;
      menu_driver_ctl(RARCH_MENU_CTL_SYSTEM_INFO_GET,
            &info);
      content_push_to_history_playlist(true, fullpath, info);
      playlist_write_file(g_defaults.history);
   }

   return true;

error:
   snprintf(msg, sizeof(msg), "Failed to load %s.\n", name);
   runloop_msg_queue_push(msg, 1, 90, false);
   return false;
}
#endif

static bool command_event_cmd_exec(void *data)
{
   char *fullpath = NULL;

   runloop_ctl(RUNLOOP_CTL_GET_CONTENT_PATH, &fullpath);

   if (fullpath != data)
   {
      runloop_ctl(RUNLOOP_CTL_CLEAR_CONTENT_PATH, NULL);
      if (data)
         runloop_ctl(RUNLOOP_CTL_SET_CONTENT_PATH, data);
   }

#if defined(HAVE_DYNAMIC)
#ifdef HAVE_MENU
   if (!menu_content_load())
   {
      rarch_ctl(RARCH_CTL_MENU_RUNNING, NULL);
      return false;
   }
#endif
#else
   frontend_driver_set_fork(FRONTEND_FORK_CORE_WITH_ARGS);
#endif

   return true;
}

bool rarch_task_push_content_load_default(
      const char *core_path,
      const char *fullpath,
      bool persist,
      enum rarch_core_type type,
      enum content_mode_load mode,
      retro_task_callback_t cb,
      void *user_data)
{
   settings_t *settings = config_get_ptr();

   switch (mode)
   {
      case CONTENT_MODE_LOAD_NOTHING_WITH_DUMMY_CORE:
         break;
      case CONTENT_MODE_LOAD_NOTHING_WITH_CURRENT_CORE_FROM_MENU:
         runloop_ctl(RUNLOOP_CTL_CLEAR_CONTENT_PATH, NULL);
#ifdef HAVE_MENU
         if (!menu_content_load())
            goto error;
#endif
         break;
      case CONTENT_MODE_LOAD_CONTENT_WITH_CURRENT_CORE_FROM_MENU:
         core_path            = settings->path.libretro;
         runloop_ctl(RUNLOOP_CTL_SET_CONTENT_PATH,  (void*)fullpath);
         runloop_ctl(RUNLOOP_CTL_SET_LIBRETRO_PATH, (void*)core_path);
#ifdef HAVE_DYNAMIC
         command_event(CMD_EVENT_LOAD_CORE, NULL);
#endif
#ifdef HAVE_MENU
         if (!menu_content_load())
            goto error;
#endif
         break;
      case CONTENT_MODE_LOAD_CONTENT_WITH_CURRENT_CORE_FROM_COMPANION_UI:
         core_path            = settings->path.libretro;
         runloop_ctl(RUNLOOP_CTL_SET_CONTENT_PATH,  (void*)fullpath);
         runloop_ctl(RUNLOOP_CTL_SET_LIBRETRO_PATH, (void*)core_path);
#ifdef HAVE_MENU
         if (!menu_content_load())
            goto error;
#endif
#ifdef HAVE_MENU
         if (!menu_content_load())
            goto error;
#endif
         break;
      case CONTENT_MODE_LOAD_CONTENT_WITH_FFMPEG_CORE_FROM_MENU:
         core_path            = settings->path.libretro; /* TODO/FIXME */
         runloop_ctl(RUNLOOP_CTL_SET_CONTENT_PATH,  (void*)fullpath);
         runloop_ctl(RUNLOOP_CTL_SET_LIBRETRO_PATH, (void*)core_path);
#ifdef HAVE_DYNAMIC
         command_event(CMD_EVENT_LOAD_CORE, NULL);
#endif
#ifdef HAVE_MENU
         if (!menu_content_load())
            goto error;
#endif
         break;
      case CONTENT_MODE_LOAD_CONTENT_WITH_IMAGEVIEWER_CORE_FROM_MENU:
         core_path            = settings->path.libretro; /* TODO/FIXME */
         runloop_ctl(RUNLOOP_CTL_SET_LIBRETRO_PATH, (void*)core_path);
#ifdef HAVE_DYNAMIC
         command_event(CMD_EVENT_LOAD_CORE, NULL);
#endif
         runloop_ctl(RUNLOOP_CTL_SET_CONTENT_PATH, (void*)fullpath);
#ifdef HAVE_MENU
         if (!menu_content_load())
            goto error;
#endif
         break;
      case CONTENT_MODE_LOAD_CONTENT_WITH_NEW_CORE_FROM_MENU:
         runloop_ctl(RUNLOOP_CTL_SET_CONTENT_PATH, (void*)fullpath);
         runloop_ctl(RUNLOOP_CTL_SET_LIBRETRO_PATH, (void*)core_path);
#ifdef HAVE_DYNAMIC
         command_event(CMD_EVENT_LOAD_CORE, NULL);
#ifdef HAVE_MENU
         if (!menu_content_load())
            goto error;
#endif
#else
         {
         char *fullpath       = NULL;

         runloop_ctl(RUNLOOP_CTL_GET_CONTENT_PATH, &fullpath);
         command_event_cmd_exec((void*)fullpath);
         command_event(CMD_EVENT_QUIT, NULL);
         }
#endif
         break;
      case CONTENT_MODE_LOAD_CONTENT_FROM_PLAYLIST_FROM_MENU:
         runloop_ctl(RUNLOOP_CTL_SET_LIBRETRO_PATH, (void*)core_path);

         if (fullpath)
            menu_driver_ctl(RARCH_MENU_CTL_UNSET_LOAD_NO_CONTENT, NULL);
         else
            menu_driver_ctl(RARCH_MENU_CTL_SET_LOAD_NO_CONTENT, NULL);

         if (!command_event_cmd_exec((void*)fullpath))
            return false;

         command_event(CMD_EVENT_LOAD_CORE, NULL);
         break;
      case CONTENT_MODE_LOAD_NONE:
         break;
   }

#ifdef HAVE_MENU
   if (type != CORE_TYPE_DUMMY)
   {
      menu_driver_ctl(RARCH_MENU_CTL_SET_PENDING_QUIT,       NULL);
      menu_driver_ctl(RARCH_MENU_CTL_SET_PENDING_QUICK_MENU, NULL);
   }
#endif

   return true;

error:
#ifdef HAVE_MENU
   switch (mode)
   {
      case CONTENT_MODE_LOAD_NOTHING_WITH_CURRENT_CORE_FROM_MENU:
      case CONTENT_MODE_LOAD_CONTENT_WITH_CURRENT_CORE_FROM_MENU:
      case CONTENT_MODE_LOAD_CONTENT_WITH_FFMPEG_CORE_FROM_MENU:
      case CONTENT_MODE_LOAD_CONTENT_WITH_IMAGEVIEWER_CORE_FROM_MENU:
      case CONTENT_MODE_LOAD_CONTENT_WITH_NEW_CORE_FROM_MENU:
         rarch_ctl(RARCH_CTL_MENU_RUNNING, NULL);
         break;
      default:
         break;
   }
#endif
   return false;
}
