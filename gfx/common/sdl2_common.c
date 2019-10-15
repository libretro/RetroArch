/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2019 - Daniel De Matteis
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
#include <string.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include <retro_miscellaneous.h>

#include "sdl2_common.h"
#include "../../retroarch.h"

#ifdef HAVE_SDL2
#include "SDL.h"
#include "SDL_syswm.h"

void sdl2_set_handles(void *data, enum rarch_display_type display_type)
{
   /* SysWMinfo headers are broken on OSX. */
   SDL_SysWMinfo info;
   SDL_Window *window = (SDL_Window*)data;
   SDL_VERSION(&info.version);

   if (SDL_GetWindowWMInfo(window, &info) != 1)
      return;

   video_driver_display_userdata_set((uintptr_t)window);

   switch (display_type)
   {
      case RARCH_DISPLAY_WIN32:
#if defined(_WIN32)
         video_driver_display_type_set(RARCH_DISPLAY_WIN32);
         video_driver_display_set(0);
         video_driver_window_set((uintptr_t)info.info.win.window);
#endif
         break;
      case RARCH_DISPLAY_X11:
#if defined(HAVE_X11)
         video_driver_display_type_set(RARCH_DISPLAY_X11);
         video_driver_display_set((uintptr_t)info.info.x11.display);
         video_driver_window_set((uintptr_t)info.info.x11.window);
#endif
         break;
      case RARCH_DISPLAY_OSX:
#ifdef HAVE_COCOA
         video_driver_display_type_set(RARCH_DISPLAY_OSX);
         video_driver_display_set(0);
         video_driver_window_set((uintptr_t)info.info.cocoa.window);
#endif
         break;
      case RARCH_DISPLAY_WAYLAND:
/* SDL_VIDEO_DRIVER_WAYLAND is defined by SDL2 */
#if defined(HAVE_WAYLAND) && defined(SDL_VIDEO_DRIVER_WAYLAND)
         video_driver_display_type_set(RARCH_DISPLAY_WAYLAND);
         video_driver_display_set((uintptr_t)info.info.wl.display);
         video_driver_window_set((uintptr_t)info.info.wl.surface);
#endif
         break;
      default:
      case RARCH_DISPLAY_NONE:
         break;
   }
}

#endif
