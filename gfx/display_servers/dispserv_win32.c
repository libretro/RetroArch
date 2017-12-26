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

#include <windows.h>
#include "../video_display_server.h"
#include "../common/win32_common.h"

typedef struct
{
   unsigned opacity;
} dispserv_win32_t;

static void* win32_display_server_init(void)
{
   dispserv_win32_t *dispserv = (dispserv_win32_t*)calloc(1, sizeof(*dispserv));

   if (!dispserv)
      return NULL;

   return dispserv;
}

static void win32_display_server_destroy(void)
{

}

static bool win32_set_window_opacity(void *data, unsigned opacity)
{
   HWND              hwnd = win32_get_window();
   dispserv_win32_t *serv = (dispserv_win32_t*)data;

   serv->opacity          = opacity;

#if defined(_WIN32_WINNT) && _WIN32_WINNT >= 0x0500
   /* Set window transparency on Windows 2000 and above */
   if (SetLayeredWindowAttributes(hwnd, 0, (255 * opacity) / 100, LWA_ALPHA))
      return true;
#endif
   return false;
}

const video_display_server_t dispserv_win32 = {
   win32_display_server_init,
   win32_display_server_destroy,
   win32_set_window_opacity,
   "win32"
};

