/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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
#include <string/stdstring.h>

#include "../../configuration.h"
#include "../../input/input_driver.h"
#include "../../verbosity.h"

#include "d3d_common.h"

#define D3D_TEXTURE_FILTER_LINEAR 2
#define D3D_TEXTURE_FILTER_POINT  1

int32_t d3d_translate_filter(unsigned type)
{
   switch (type)
   {
      case RARCH_FILTER_UNSPEC:
         {
            settings_t *settings = config_get_ptr();
            bool video_smooth    = settings->bools.video_smooth;
            if (!video_smooth)
               break;
         }
         /* fall-through */
      case RARCH_FILTER_LINEAR:
         return (int32_t)D3D_TEXTURE_FILTER_LINEAR;
      case RARCH_FILTER_NEAREST:
         break;
   }

   return (int32_t)D3D_TEXTURE_FILTER_POINT;
}

void d3d_input_driver(const char* input_name, const char* joypad_name,
      input_driver_t** input, void** input_data)
{
#if defined(__WINRT__)
   /* Plain xinput is supported on UWP, but it
    * supports joypad only (uwp driver was added later) */
   if (string_is_equal(input_name, "xinput"))
   {
      void *xinput = input_driver_init_wrap(&input_xinput, joypad_name);
      *input       = xinput ? (input_driver_t*)&input_xinput : NULL;
      *input_data  = xinput;
   }
   else
   {
      void *uwp    = input_driver_init_wrap(&input_uwp, joypad_name);
      *input       = uwp ? (input_driver_t*)&input_uwp : NULL;
      *input_data  = uwp;
   }
#elif defined(_XBOX)
   void *xinput    = input_driver_init_wrap(&input_xinput, joypad_name);
   *input          = xinput ? (input_driver_t*)&input_xinput : NULL;
   *input_data     = xinput;
#else
#if _WIN32_WINNT >= 0x0501
#ifdef HAVE_WINRAWINPUT
   /* winraw only available since XP */
   if (string_is_equal(input_name, "raw"))
   {
      *input_data = input_driver_init_wrap(&input_winraw, joypad_name);
      if (*input_data)
      {
         *input = &input_winraw;
         return;
      }
   }
#endif
#endif

#ifdef HAVE_DINPUT
   *input_data = input_driver_init_wrap(&input_dinput, joypad_name);
   *input      = *input_data ? &input_dinput : NULL;
#endif
#endif
}
