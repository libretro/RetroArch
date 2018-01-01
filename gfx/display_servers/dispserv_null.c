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

#include <stddef.h>
#include "../video_display_server.h"

static void* null_display_server_init(void)
{
   return NULL;
}

static void null_display_server_destroy(void)
{

}

static bool null_set_window_opacity(void *data, unsigned opacity)
{
   (void)data;
   (void)opacity;
   return true;
}

static bool null_set_window_progress(void *data, int progress, bool finished)
{
   (void)data;
   (void)progress;
   (void)finished;
   return true;
}

const video_display_server_t dispserv_null = {
   null_display_server_init,
   null_display_server_destroy,
   null_set_window_opacity,
   null_set_window_progress,
   "null"
};

