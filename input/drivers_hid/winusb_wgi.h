/*  RetroArch - A frontend for libretro.
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

#ifndef _WINUSB_WGI_H
#define _WINUSB_WGI_H

#include <stdint.h>

#include <boolean.h>
#include <retro_inline.h>

#include "../input_defines.h"

/* An Xbox pad on Windows' HID driver has its triggers on one axis and
 * no rumble there. Windows.Gaming.Input, in Windows from 10 on, has
 * both: winusb pairs each such pad with its gamepad there, as SDL
 * does, and takes the triggers from it and sends it rumble. Before
 * Windows 10, or built without the header, there is none, and the pad
 * is read from its HID collection alone. */

typedef struct winusb_wgi_pad winusb_wgi_pad_t;

/* What one gamepad says, on the HID collection's axes: the sticks as
 * axes 0 to 3, down positive; A, B, X, Y, LB, RB, View, Menu, LS and RS
 * as buttons 0 to 9. */
typedef struct
{
   int16_t sticks[4];
   uint16_t buttons;
} winusb_wgi_look_t;

/* Whether a gamepad that looks like @look is the HID collection whose
 * sticks are @axes and buttons @buttons: the same buttons down, and
 * each stick within a little of the other's. */
static INLINE bool winusb_wgi_alike(const winusb_wgi_look_t *look,
      const int16_t *axes, uint16_t buttons)
{
   unsigned i;
   if (look->buttons != buttons)
      return false;
   for (i = 0; i < 4; i++)
   {
      int d = (int)look->sticks[i] - (int)axes[i];
      if (d < -6000 || d > 6000)
         return false;
   }
   return true;
}

#ifdef HAVE_WGI
bool winusb_wgi_start(void);
void winusb_wgi_stop(void);
/* The one gamepad not yet paired that is alike; NULL while none or
 * more than one is. Not by its ids: Windows gives every Xbox One pad's
 * HID collection 045e:02ff, and the gamepad its own. */
winusb_wgi_pad_t *winusb_wgi_pair(const int16_t *axes, uint16_t buttons);
/* Its triggers, 0 to 32767; false if it could not be read. */
bool winusb_wgi_triggers(winusb_wgi_pad_t *pad, int16_t *left,
      int16_t *right);
/* Its motors, 0 to 255: sent by a thread of the pairing's, never the
 * caller's. */
void winusb_wgi_rumble(winusb_wgi_pad_t *pad, uint8_t strong, uint8_t weak);
void winusb_wgi_unpair(winusb_wgi_pad_t *pad);
#else
static INLINE bool winusb_wgi_start(void) { return false; }
static INLINE void winusb_wgi_stop(void) { }
static INLINE winusb_wgi_pad_t *winusb_wgi_pair(const int16_t *axes,
      uint16_t buttons) { return NULL; }
static INLINE bool winusb_wgi_triggers(winusb_wgi_pad_t *pad,
      int16_t *left, int16_t *right) { return false; }
static INLINE void winusb_wgi_rumble(winusb_wgi_pad_t *pad,
      uint8_t strong, uint8_t weak) { }
static INLINE void winusb_wgi_unpair(winusb_wgi_pad_t *pad) { }
#endif

#endif
