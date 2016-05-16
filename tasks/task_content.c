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

#ifdef HAVE_MENU
#include "../menu/menu_driver.h"
#include "../menu/menu_content.h"
#endif

#include "tasks_internal.h"

#include "../command.h"
#include "../configuration.h"
#include "../retroarch.h"

/* TODO/FIXME - turn this into actual task */

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
   if (!menu_content_ctl(MENU_CONTENT_CTL_LOAD, NULL))
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
         if (!menu_content_ctl(MENU_CONTENT_CTL_LOAD, NULL))
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
         if (!menu_content_ctl(MENU_CONTENT_CTL_LOAD, NULL))
            goto error;
#endif
         break;
      case CONTENT_MODE_LOAD_CONTENT_WITH_CURRENT_CORE_FROM_COMPANION_UI:
         core_path            = settings->path.libretro;
         runloop_ctl(RUNLOOP_CTL_SET_CONTENT_PATH,  (void*)fullpath);
         runloop_ctl(RUNLOOP_CTL_SET_LIBRETRO_PATH, (void*)core_path);
#ifdef HAVE_MENU
         if (!menu_content_ctl(MENU_CONTENT_CTL_LOAD, NULL))
            goto error;
#endif
#ifdef HAVE_MENU
         if (!menu_content_ctl(MENU_CONTENT_CTL_LOAD, NULL))
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
         if (!menu_content_ctl(MENU_CONTENT_CTL_LOAD, NULL))
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
         if (!menu_content_ctl(MENU_CONTENT_CTL_LOAD, NULL))
            goto error;
#endif
         break;
      case CONTENT_MODE_LOAD_CONTENT_WITH_NEW_CORE_FROM_MENU:
         runloop_ctl(RUNLOOP_CTL_SET_CONTENT_PATH, (void*)fullpath);
         runloop_ctl(RUNLOOP_CTL_SET_LIBRETRO_PATH, (void*)core_path);
#ifdef HAVE_DYNAMIC
         command_event(CMD_EVENT_LOAD_CORE, NULL);
#ifdef HAVE_MENU
         if (!menu_content_ctl(MENU_CONTENT_CTL_LOAD, NULL))
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
