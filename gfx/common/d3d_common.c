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

