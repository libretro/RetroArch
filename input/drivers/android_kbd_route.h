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

#ifndef _ANDROID_KBD_ROUTE_H
#define _ANDROID_KBD_ROUTE_H

#include <boolean.h>
#include <retro_inline.h>

/* Who gets a key event while the Android system keyboard owns the
 * menu text line (input/drivers/android_input.c).
 *
 * The text lives in the Java-side field and reaches the line only as
 * whole snapshots of it. A soft keyboard commits most text straight
 * into that field, but it sends some keys as key events instead -
 * Backspace when nothing is being composed, digits on the AOSP
 * keyboard. RetroActivity is a NativeActivity, so those events come
 * out of the native input queue first, and the field only sees one
 * that is finished unhandled. Consumed here, a Backspace never
 * reaches the field: its text keeps the characters, and the next
 * snapshot puts them back on the line.
 *
 * So a key event the soft keyboard sent is finished unhandled, and
 * left out of the native key state and line editor, for as long as a
 * session is open. BACK stays native: the field takes it ahead of
 * the IME to cancel, and one left unhandled falls through to the
 * activity's own back handling.
 *
 * Kept free of NDK types so samples/input/android_kbd_keys can build
 * it on a host. */
static INLINE bool android_kbd_key_goes_to_ime(
      bool session_open, bool from_ime, bool is_back)
{
   return session_open && from_ime && !is_back;
}

#endif
