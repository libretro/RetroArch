/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2019 - Daniel De Matteis
 *  Copyright (C)      2026 - Rob Loach
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
#include <string/stdstring.h>

#include <SDL3/SDL.h>

#include "sdl3_common.h"
#include "../../retroarch.h"

/* SDL3 dropped SDL_GetWindowWMInfo / SDL_SysWMinfo in favour of the
 * window properties API, so the native display/window handles are read
 * from SDL_GetWindowProperties() instead.
 *
 * The display type is derived from SDL's active video driver name, so
 * no platform #ifdefs are needed at the call site; each case below
 * compiles away when the corresponding backend isn't built in, making
 * unsupported types safe no-ops. */
void sdl3_set_handles(SDL_Window *window)
{
   SDL_PropertiesID  props      = SDL_GetWindowProperties(window);
   const char       *sdl_driver = SDL_GetCurrentVideoDriver();
   enum rarch_display_type display_type = RARCH_DISPLAY_NONE;

   if (!props)
      return;

   if (string_is_equal(sdl_driver, "windows"))
      display_type = RARCH_DISPLAY_WIN32;
   else if (string_is_equal(sdl_driver, "cocoa"))
      display_type = RARCH_DISPLAY_OSX;
   else if (string_is_equal(sdl_driver, "x11"))
      display_type = RARCH_DISPLAY_X11;
   else if (string_is_equal(sdl_driver, "wayland"))
      display_type = RARCH_DISPLAY_WAYLAND;
   else if (string_is_equal(sdl_driver, "kmsdrm"))
      display_type = RARCH_DISPLAY_KMS;

   video_driver_display_userdata_set((uintptr_t)window);

   switch (display_type)
   {
      case RARCH_DISPLAY_WIN32:
#if defined(_WIN32)
         video_driver_display_type_set(RARCH_DISPLAY_WIN32);
         video_driver_display_set(0);
         video_driver_window_set((uintptr_t)SDL_GetPointerProperty(props,
               SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL));
#endif
         break;
      case RARCH_DISPLAY_X11:
#if defined(HAVE_X11)
         video_driver_display_type_set(RARCH_DISPLAY_X11);
         video_driver_display_set((uintptr_t)SDL_GetPointerProperty(props,
               SDL_PROP_WINDOW_X11_DISPLAY_POINTER, NULL));
         video_driver_window_set((uintptr_t)SDL_GetNumberProperty(props,
               SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0));
#endif
         break;
      case RARCH_DISPLAY_OSX:
#ifdef HAVE_COCOA
         video_driver_display_type_set(RARCH_DISPLAY_OSX);
         video_driver_display_set(0);
         video_driver_window_set((uintptr_t)SDL_GetPointerProperty(props,
               SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, NULL));
#endif
         break;
      case RARCH_DISPLAY_WAYLAND:
#if defined(HAVE_WAYLAND)
         video_driver_display_type_set(RARCH_DISPLAY_WAYLAND);
         video_driver_display_set((uintptr_t)SDL_GetPointerProperty(props,
               SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, NULL));
         video_driver_window_set((uintptr_t)SDL_GetPointerProperty(props,
               SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, NULL));
#endif
         break;
      case RARCH_DISPLAY_KMS:
#if defined(HAVE_KMS)
         /* SDL3's kmsdrm backend owns the DRM device; expose the fd so
          * the KMS display server can query it, mirroring drm_ctx. */
         video_driver_display_type_set(RARCH_DISPLAY_KMS);
         video_driver_display_set((uintptr_t)SDL_GetNumberProperty(props,
               SDL_PROP_WINDOW_KMSDRM_DRM_FD_NUMBER, 0));
         video_driver_window_set(0);
#endif
         break;
      default:
      case RARCH_DISPLAY_NONE:
         break;
   }
}
