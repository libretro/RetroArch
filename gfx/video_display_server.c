/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2016-2017 - Brad Parker
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

#include <stdio.h>
#include "video_display_server.h"
#include "video_driver.h"
#include "../verbosity.h"

static const video_display_server_t *current_display_server = NULL;
static void *current_display_server_data = NULL;

void* video_display_server_init(void)
{
   enum rarch_display_type type = video_driver_display_type_get();

   switch (type)
   {
      case RARCH_DISPLAY_WIN32:
#if defined(_WIN32) && !defined(_XBOX)
         current_display_server = &dispserv_win32;
#endif
         break;
      case RARCH_DISPLAY_X11:
#if defined(HAVE_X11)
         current_display_server = &dispserv_x11;
#endif
         break;
      default:
         current_display_server = &dispserv_null;
         break;
   }

   current_display_server_data = current_display_server->init();

   RARCH_LOG("[Video]: Found display server: %s\n",
		   current_display_server->ident);

   return current_display_server_data;
}

void video_display_server_destroy(void)
{

}

bool video_display_server_set_window_opacity(unsigned opacity)
{
   if (current_display_server && current_display_server->set_window_opacity)
      return current_display_server->set_window_opacity(current_display_server_data, opacity);
   return false;
}

bool video_display_server_set_window_progress(int progress, bool finished)
{
   if (current_display_server && current_display_server->set_window_progress)
      return current_display_server->set_window_progress(current_display_server_data, progress, finished);
   return false;
}
