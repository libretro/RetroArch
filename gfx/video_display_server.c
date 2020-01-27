/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#include <stdio.h>
#include "video_display_server.h"
#include "../retroarch.h"
#include "../verbosity.h"

static const video_display_server_t dispserv_null = {
   NULL, /* init */
   NULL, /* destroy */
   NULL, /* set_window_opacity */
   NULL, /* set_window_progress */
   NULL, /* set_window_decorations */
   NULL, /* set_resolution */
   NULL, /* get_resolution_list */
   NULL, /* get_output_options */
   NULL, /* set_screen_orientation */
   NULL, /* get_screen_orientation */
   NULL, /* get_flags */
   "null"
};

static const video_display_server_t *current_display_server = &dispserv_null;
static void                    *current_display_server_data = NULL;
static enum rotation initial_screen_orientation          = ORIENTATION_NORMAL;
static enum rotation current_screen_orientation          = ORIENTATION_NORMAL;

const char *video_display_server_get_ident(void)
{
   if (!current_display_server)
      return "null";
   return current_display_server->ident;
}

void* video_display_server_init(void)
{
   enum rarch_display_type type = video_driver_display_type_get();

   video_display_server_destroy();

   switch (type)
   {
      case RARCH_DISPLAY_WIN32:
#if defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__)
         current_display_server = &dispserv_win32;
#endif
         break;
      case RARCH_DISPLAY_X11:
#if defined(HAVE_X11)
         current_display_server = &dispserv_x11;
#endif
         break;
      default:
#if defined(ANDROID)
         current_display_server = &dispserv_android;
#else
         current_display_server = &dispserv_null;
#endif
         break;
   }

   if (current_display_server && current_display_server->init)
       current_display_server_data = current_display_server->init();

   RARCH_LOG("[Video]: Found display server: %s\n",
		   current_display_server->ident);

   initial_screen_orientation = video_display_server_get_screen_orientation();
   current_screen_orientation = initial_screen_orientation;

   return current_display_server_data;
}

void video_display_server_destroy(void)
{
   if (initial_screen_orientation != current_screen_orientation)
      video_display_server_set_screen_orientation(initial_screen_orientation);

   if (current_display_server)
      if (current_display_server_data)
         current_display_server->destroy(current_display_server_data);
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

bool video_display_server_set_window_decorations(bool on)
{
   if (current_display_server && current_display_server->set_window_decorations)
      return current_display_server->set_window_decorations(current_display_server_data, on);
   return false;
}

bool video_display_server_set_resolution(unsigned width, unsigned height,
      int int_hz, float hz, int center, int monitor_index, int xoffset)
{
   if (current_display_server && current_display_server->set_resolution)
      return current_display_server->set_resolution(current_display_server_data, width, height, int_hz, hz, center, monitor_index, xoffset);
   return false;
}

bool video_display_server_has_resolution_list(void)
{
   return (current_display_server 
         && current_display_server->get_resolution_list);
}

void *video_display_server_get_resolution_list(unsigned *size)
{
   if (video_display_server_has_resolution_list())
      return current_display_server->get_resolution_list(current_display_server_data, size);
   return NULL;
}

const char *video_display_server_get_output_options(void)
{
   if (current_display_server && current_display_server->get_output_options)
      return current_display_server->get_output_options(current_display_server_data);
   return NULL;
}

void video_display_server_set_screen_orientation(enum rotation rotation)
{
   if (current_display_server && current_display_server->set_screen_orientation)
   {
      RARCH_LOG("[Video]: Setting screen orientation to %d.\n", rotation);
      current_screen_orientation = rotation;
      current_display_server->set_screen_orientation(rotation);
   }
}

bool video_display_server_can_set_screen_orientation(void)
{
   if (current_display_server && current_display_server->set_screen_orientation)
      return true;
   return false;
}

enum rotation video_display_server_get_screen_orientation(void)
{
   if (current_display_server && current_display_server->get_screen_orientation)
      return current_display_server->get_screen_orientation();
   return ORIENTATION_NORMAL;
}

bool video_display_server_get_flags(gfx_ctx_flags_t *flags)
{
   if (!current_display_server || !current_display_server->get_flags)
      return false;
   if (!flags)
      return false;

   flags->flags = current_display_server->get_flags(
         current_display_server_data);
   return true;
}
