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

#include <string.h>

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

/* A television's remote, passed on over HDMI-CEC.
 *
 * Android turns the CEC commands into key events - the arrows, Center
 * and Back - with no input device behind them: the device id is -1 and
 * the source is AINPUT_SOURCE_HDMI. Asked for the name of device -1,
 * Android answers "Virtual", and an event from an unknown device that
 * is not a keyboard is taken for a new gamepad. So the remote became
 * an unconfigured pad called "Virtual" on the next free port: its keys
 * did nothing, and it held a port a controller should have had.
 *
 * It is told by the source, not by that name - the name is shared by
 * everything synthetic - and it is not a controller. Its keys go where
 * a keyboard's do, to the keyboard row, where the arrows are already
 * arrows, Center is folded into Enter and Back is the remote's back;
 * it is given no pad slot.
 *
 * The value is spelled out because it is an enumerator of the NDK's
 * input.h, which a host build of this header does not have. */
#define ANDROID_KEY_SOURCE_HDMI 0x02000001

static INLINE bool android_key_source_is_cec(int source)
{
   return (source & ANDROID_KEY_SOURCE_HDMI) == ANDROID_KEY_SOURCE_HDMI;
}

/* The NVIDIA SHIELD maps its remote and CEC input onto a pad of its
 * own ("SHIELD Virtual Controller", handle_hotplug), with an
 * autoconfig profile to match. That keeps working as it does. */
static INLINE bool android_cec_remote_is_keyboard(const char *device_model)
{
   return !device_model || !strstr(device_model, "SHIELD");
}

#endif
