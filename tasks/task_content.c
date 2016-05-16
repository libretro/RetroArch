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

static bool task_content_load(bool persist, bool load_content)
{
   if (persist)
   {
#ifdef HAVE_DYNAMIC
      command_event(CMD_EVENT_LOAD_CORE, NULL);
#endif
      load_content = true;
   }

   if (load_content)
   {
#ifdef HAVE_MENU
      if (!menu_content_ctl(MENU_CONTENT_CTL_LOAD, NULL))
      {
         rarch_ctl(RARCH_CTL_MENU_RUNNING, NULL);
         return false;
      }
#endif
   }
   else
   {
      char *fullpath       = NULL;

      runloop_ctl(RUNLOOP_CTL_GET_CONTENT_PATH, &fullpath);
      command_event(CMD_EVENT_EXEC, (void*)fullpath);
      command_event(CMD_EVENT_QUIT, NULL);
   }

   return true;
}

bool rarch_task_push_content_load_default(
      const char *core_path, const char *fullpath,
      bool persist, enum rarch_core_type type,
      retro_task_callback_t cb, void *user_data)
{
   bool load_content            = false;

   switch (type)
   {
      case CORE_TYPE_PLAIN:
      case CORE_TYPE_DUMMY:
         load_content    = false;
         if (persist)
            load_content = true;
         break;
      case CORE_TYPE_FFMPEG:
         persist         = false;
         load_content    = true;
         break;
      case CORE_TYPE_IMAGEVIEWER:
         persist         = false;
         load_content    = true;
         break;
   }

   if (load_content)
   {
      settings_t *settings = config_get_ptr();
      core_path            = settings->path.libretro;
   }

   if (core_path)
   {
      runloop_ctl(RUNLOOP_CTL_SET_LIBRETRO_PATH, (void*)core_path);
      persist = true;
   }

   if (fullpath)
      runloop_ctl(RUNLOOP_CTL_SET_CONTENT_PATH, (void*)fullpath);

   if (!task_content_load(persist, load_content))
      return false;

#ifdef HAVE_MENU
   menu_driver_ctl(RARCH_MENU_CTL_SET_PENDING_QUIT,       NULL);
   menu_driver_ctl(RARCH_MENU_CTL_SET_PENDING_QUICK_MENU, NULL);
#endif

   return true;
}
