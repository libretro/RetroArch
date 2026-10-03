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

#ifndef _ANDROID_KEY_STATE_H
#define _ANDROID_KEY_STATE_H

#include <stdint.h>
#include <string.h>

#include <boolean.h>
#include <retro_inline.h>
#include <retro_miscellaneous.h>

/* Writing key presses and releases into the Android input driver's
 * key state (input/drivers/android_input.c).
 *
 * The state is one row of bits per pad slot, indexed by Android
 * keycode, plus one row for the keyboard. Two keys are not written as
 * themselves:
 *
 * - DPAD_CENTER is written as ENTER, on the keyboard row only. The
 *   keyboard row is read through a keymap that can give RETROK_RETURN
 *   only one keycode. Pad rows are read by raw keycode against joypad
 *   binds, and a remote configures its OK button as "23": rewriting it
 *   there left the Amazon Fire TV Remote without an OK button (issue
 *   #18819).
 *
 * - BACK also presses X, on its own row and on the keyboard row,
 *   unless a gamepad sent it. That is what lets a television remote's
 *   Back button act in the menu.
 *
 * Everything a press sets, the matching release clears. That is the
 * whole point of having this in one place: for nine days a press of
 * BACK set X on the keyboard row and nothing cleared it. X is bound to
 * RetroPad A by default, and the frontend drops all controller input
 * after a menu toggle until every button is up, so one press of Back
 * left the menu deaf to the controller until the app was restarted
 * (issue #19436).
 *
 * Kept free of NDK types so samples/input/android_key_state can build
 * it on a host. The keycodes and sources are enumerators of the NDK's
 * headers, which a host build does not have, so they are spelled out;
 * the driver checks them against the NDK's own at compile time. */

#define ANDROID_KEY_BACK           4
#define ANDROID_KEY_DPAD_CENTER    23
#define ANDROID_KEY_X              52
#define ANDROID_KEY_ENTER          66

#define ANDROID_KEY_SOURCE_GAMEPAD  0x00000401
#define ANDROID_KEY_SOURCE_JOYSTICK 0x01000010

/* Whether a BACK from this source also presses X. */
static INLINE bool android_key_back_is_aliased(int source)
{
   return    (source & ANDROID_KEY_SOURCE_GAMEPAD)
                != ANDROID_KEY_SOURCE_GAMEPAD
          && (source & ANDROID_KEY_SOURCE_JOYSTICK)
                != ANDROID_KEY_SOURCE_JOYSTICK;
}

/* One key event.
 *
 * @row is the row of the device that sent it and @kbd_row the
 * keyboard's; they are the same row when the device is a keyboard.
 * Rows are (@last_keycode + 7) / 8 bytes wide and their readers stop
 * at @last_keycode. Keycodes arrive straight from the platform and are
 * not confined to that range - public ones run well past it and vendor
 * ones are unbounded - so one outside it is dropped, not written past
 * the row. */
static INLINE void android_key_state_write(
      uint8_t *row, uint8_t *kbd_row, int last_keycode,
      int keycode, int source, bool down)
{
   int keysym = keycode;

   if (row == kbd_row && keycode == ANDROID_KEY_DPAD_CENTER)
      keysym = ANDROID_KEY_ENTER;

   if (keysym < 0 || keysym >= last_keycode)
      return;

   if (down)
   {
      BIT_SET(row, keysym);
      if (     keysym == ANDROID_KEY_BACK
            && android_key_back_is_aliased(source))
      {
         BIT_SET(row,     ANDROID_KEY_X);
         BIT_SET(kbd_row, ANDROID_KEY_X);
      }
   }
   else
   {
      BIT_CLEAR(row, keysym);
      if (     keysym == ANDROID_KEY_BACK
            && android_key_back_is_aliased(source))
      {
         BIT_CLEAR(row,     ANDROID_KEY_X);
         BIT_CLEAR(kbd_row, ANDROID_KEY_X);
      }
   }
}

/* A pad has gone and no release will arrive from it: let go of
 * everything its row holds, the X a held BACK put on the keyboard row
 * included. */
static INLINE void android_key_state_release_row(
      uint8_t *row, uint8_t *kbd_row, int last_keycode)
{
   if (     row != kbd_row
         && BIT_GET(row, ANDROID_KEY_BACK)
         && BIT_GET(row, ANDROID_KEY_X))
      BIT_CLEAR(kbd_row, ANDROID_KEY_X);

   memset(row, 0, (size_t)((last_keycode + 7) / 8));
}

#endif
