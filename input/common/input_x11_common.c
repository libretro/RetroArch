/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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

#include <libretro.h>

#include "input_x11_common.h"

static bool x11_mouse_wu;
static bool x11_mouse_wd;
static bool x11_mouse_hwu;
static bool x11_mouse_hwd;

int16_t x_mouse_state_wheel(unsigned id)
{
   int16_t ret = 0;

   switch (id)
   {
      case RETRO_DEVICE_ID_MOUSE_WHEELUP:
         ret = x11_mouse_wu;
         x11_mouse_wu = 0;
         return ret;
      case RETRO_DEVICE_ID_MOUSE_WHEELDOWN:
         ret = x11_mouse_wd;
         x11_mouse_wd = 0;
         return ret;
      case RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELUP:
         ret = x11_mouse_hwu;
         x11_mouse_hwu = 0;
         return ret;
      case RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELDOWN:
         ret = x11_mouse_hwd;
         x11_mouse_hwd = 0;
         return ret;
   }

   return 0;
}

void x_input_poll_wheel(XButtonEvent *event, bool latch)
{
   switch (event->button)
   {
      case 4:
         x11_mouse_wu = 1;
         break;
      case 5:
         x11_mouse_wd = 1;
         break;
      case 6:
         /* Scroll wheel left == HORIZ_WHEELDOWN */
         x11_mouse_hwd = 1;
         break;
      case 7:
         /* Scroll wheel right == HORIZ_WHEELUP */
         x11_mouse_hwu = 1;
         break;
   }
}
