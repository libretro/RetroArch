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

#ifndef __VIDEO_DISPLAY_SERVER__H
#define __VIDEO_DISPLAY_SERVER__H

#include <retro_common_api.h>
#include <boolean.h>

RETRO_BEGIN_DECLS

typedef struct video_display_server
{
   void *(*init)(void);
   void (*destroy)(void);
   bool (*set_window_opacity)(void *data, unsigned opacity);
   bool (*set_window_progress)(void *data, int progress, bool finished);
   const char *ident;
} video_display_server_t;

void* video_display_server_init(void);
void video_display_server_destroy(void);
bool video_display_server_set_window_opacity(unsigned opacity);
bool video_display_server_set_window_progress(int progress, bool finished);

extern const video_display_server_t dispserv_win32;
extern const video_display_server_t dispserv_x11;
extern const video_display_server_t dispserv_null;

RETRO_END_DECLS

#endif
