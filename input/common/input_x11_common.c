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
#include <retro_atomic.h>

#include "input_x11_common.h"

enum x11_mouse_btn_flags
{
   X11_MOUSE_WU_BTN   = (1 << 0),
   X11_MOUSE_WD_BTN   = (1 << 1),
   X11_MOUSE_HWU_BTN  = (1 << 2),
   X11_MOUSE_HWD_BTN  = (1 << 3),
   X11_MOUSE_BTN_4    = (1 << 4),
   X11_MOUSE_BTN_5    = (1 << 5)
};

/* X11_MOUSE_* bits. Set by button events in the event pump, which runs
 * on the video thread under the threaded wrapper, and read (the wheel
 * bits also cleared) by the input driver on the runloop thread, so
 * every access is a single atomic operation on the word. */
static retro_atomic_int_t g_x11_mouse_flags;

/* Reads a wheel bit and clears it in the same operation, so a notch
 * the event pump latches between the two is not lost. */
static int16_t x_mouse_take_wheel(int bit)
{
   return (int16_t)(retro_atomic_fetch_and_int(&g_x11_mouse_flags, ~bit) & bit);
}

int16_t x_mouse_state_wheel(unsigned id)
{
   switch (id)
   {
      case RETRO_DEVICE_ID_MOUSE_WHEELUP:
         return x_mouse_take_wheel(X11_MOUSE_WU_BTN);
      case RETRO_DEVICE_ID_MOUSE_WHEELDOWN:
         return x_mouse_take_wheel(X11_MOUSE_WD_BTN);
      case RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELUP:
         return x_mouse_take_wheel(X11_MOUSE_HWU_BTN);
      case RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELDOWN:
         return x_mouse_take_wheel(X11_MOUSE_HWD_BTN);
      case RETRO_DEVICE_ID_MOUSE_BUTTON_4:
         return (int16_t)(retro_atomic_load_relaxed_int(
                  &g_x11_mouse_flags) & X11_MOUSE_BTN_4);
      case RETRO_DEVICE_ID_MOUSE_BUTTON_5:
         return (int16_t)(retro_atomic_load_relaxed_int(
                  &g_x11_mouse_flags) & X11_MOUSE_BTN_5);
   }

   return 0;
}

void x_input_poll_wheel(XButtonEvent *event, bool latch)
{
   switch (event->button)
   {
      case 4:
         retro_atomic_fetch_or_int(&g_x11_mouse_flags, X11_MOUSE_WU_BTN);
         break;
      case 5:
         retro_atomic_fetch_or_int(&g_x11_mouse_flags, X11_MOUSE_WD_BTN);
         break;
      case 6:
         /* Scroll wheel left == HORIZ_WHEELDOWN */
         retro_atomic_fetch_or_int(&g_x11_mouse_flags, X11_MOUSE_HWD_BTN);
         break;
      case 7:
         /* Scroll wheel right == HORIZ_WHEELUP */
         retro_atomic_fetch_or_int(&g_x11_mouse_flags, X11_MOUSE_HWU_BTN);
         break;
      case 8:
         /* Extra buttons are regular press-release events,
          * while scroll wheels do not stay pressed. */
         /* Mouse button 4 */
         switch (event->type)
         {
            case ButtonPress:
               retro_atomic_fetch_or_int(&g_x11_mouse_flags, X11_MOUSE_BTN_4);
               break;
            case ButtonRelease:
               retro_atomic_fetch_and_int(&g_x11_mouse_flags, ~X11_MOUSE_BTN_4);
               break;
         }
         break;
      case 9:
         /* Mouse button 5 */
         switch (event->type)
         {
            case ButtonPress:
               retro_atomic_fetch_or_int(&g_x11_mouse_flags, X11_MOUSE_BTN_5);
               break;
            case ButtonRelease:
               retro_atomic_fetch_and_int(&g_x11_mouse_flags, ~X11_MOUSE_BTN_5);
               break;
         }
         break;
   }
}
