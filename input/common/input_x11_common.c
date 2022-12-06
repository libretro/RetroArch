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

enum x11_mouse_btn_flags
{
   X11_MOUSE_WU_BTN   = (1 << 0),
   X11_MOUSE_WD_BTN   = (1 << 1),
   X11_MOUSE_HWU_BTN  = (1 << 2),
   X11_MOUSE_HWD_BTN  = (1 << 3)
};

/* TODO/FIXME - static globals */
static uint8_t g_x11_mouse_flags = 0;

int16_t x_mouse_state_wheel(unsigned id)
{
   int16_t ret = 0;

   switch (id)
   {
      case RETRO_DEVICE_ID_MOUSE_WHEELUP:
         ret                = (g_x11_mouse_flags & X11_MOUSE_WU_BTN);
         g_x11_mouse_flags &= ~X11_MOUSE_WU_BTN;
         break;
      case RETRO_DEVICE_ID_MOUSE_WHEELDOWN:
         ret                = (g_x11_mouse_flags & X11_MOUSE_WD_BTN);
         g_x11_mouse_flags &= ~X11_MOUSE_WD_BTN;
         break;
      case RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELUP:
         ret                = (g_x11_mouse_flags & X11_MOUSE_HWU_BTN);
         g_x11_mouse_flags &= ~X11_MOUSE_HWU_BTN;
         break;
      case RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELDOWN:
         ret                = (g_x11_mouse_flags & X11_MOUSE_HWD_BTN);
         g_x11_mouse_flags &= ~X11_MOUSE_HWD_BTN;
         break;
   }

   return ret;
}

void x_input_poll_wheel(XButtonEvent *event, bool latch)
{
   switch (event->button)
   {
      case 4:
         g_x11_mouse_flags |= X11_MOUSE_WU_BTN;
         break;
      case 5:
         g_x11_mouse_flags |= X11_MOUSE_WD_BTN;
         break;
      case 6:
         /* Scroll wheel left == HORIZ_WHEELDOWN */
         g_x11_mouse_flags |= X11_MOUSE_HWD_BTN;
         break;
      case 7:
         /* Scroll wheel right == HORIZ_WHEELUP */
         g_x11_mouse_flags |= X11_MOUSE_HWU_BTN;
         break;
   }
}
