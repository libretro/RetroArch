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

#include "tasks.h"
#include "tasks_internal.h"

#include "../command_event.h"

/* TODO/FIXME - turn this into actual task */

bool rarch_task_push_content_load_default(
      const char *core_path, const char *fullpath,
      bool persist, enum rarch_core_type type,
      retro_task_callback_t cb, void *user_data)
{
   enum event_command cmd       = EVENT_CMD_NONE;

   if (core_path)
   {
      runloop_ctl(RUNLOOP_CTL_SET_LIBRETRO_PATH, (void*)core_path);
      event_cmd_ctl(EVENT_CMD_LOAD_CORE, NULL);
   }

   if (fullpath)
      runloop_ctl(RUNLOOP_CTL_SET_CONTENT_PATH, (void*)fullpath);

   switch (type)
   {
      case CORE_TYPE_PLAIN:
      case CORE_TYPE_DUMMY:
         cmd = persist ? EVENT_CMD_LOAD_CONTENT_PERSIST : EVENT_CMD_LOAD_CONTENT;
         break;
      case CORE_TYPE_FFMPEG:
#ifdef HAVE_FFMPEG
         cmd = EVENT_CMD_LOAD_CONTENT_FFMPEG;
#endif
         break;
      case CORE_TYPE_IMAGEVIEWER:
#ifdef HAVE_IMAGEVIEWER
         cmd = EVENT_CMD_LOAD_CONTENT_IMAGEVIEWER;
#endif
         break;
   }

   if (cmd != EVENT_CMD_NONE)
      event_cmd_ctl(cmd, NULL);

   return true;
}
