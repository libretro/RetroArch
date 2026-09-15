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

/* Shared declarations for the DirectInput pad array.
 *
 * g_pads[] is defined in dinput_joypad.c and read by
 * xinput_hybrid_joypad.c, which cross-references DirectInput to
 * recover the VID/PID that XInput does not expose. Both files used to
 * carry their own copy of struct dinput_joypad_data under the same
 * __DINPUT_JOYPAD_H guard, and the copies had drifted apart: the
 * definer's was 432 bytes, the reader's 400, because four rumble
 * storage fields were added to one and not the other.
 *
 * That was latent rather than live. Each file also had its own static
 * copies of the accessors, and only one joypad driver is active at a
 * time, so whichever of the two was running used its own view of the
 * array consistently and the other never touched it. The array was
 * over-allocated, not overrun. It would have become live the moment
 * the two drivers cooperated on the same entries, or the moment
 * someone added a field to one copy and not the other again - which
 * is how it got here.
 *
 * One declaration, here.
 */

#ifndef __DINPUT_JOYPAD_H
#define __DINPUT_JOYPAD_H

#include <stdint.h>
#include <boolean.h>
#include <retro_common_api.h>

#define WIN32_LEAN_AND_MEAN
#include <dinput.h>

#include "../input_driver.h"

/* For DIJOYSTATE2 struct, rgbButtons will always have 128 elements */
#define ARRAY_SIZE_RGB_BUTTONS 128

/* DirectInput POV value indicating the hat is centred (no direction pressed).
 * rgdwPOV[] returns this sentinel when the hat is released. */
#define DINPUT_POV_CENTERED 0xFFFFFFFFu

RETRO_BEGIN_DECLS

struct dinput_joypad_data
{
   LPDIRECTINPUTDEVICE8 joypad;
   DIJOYSTATE2          joy_state;
   char                *joy_name;
   char                *joy_friendly_name;
   int32_t              vid;
   int32_t              pid;
   LPDIRECTINPUTEFFECT  rumble_iface[2];
   DIEFFECT             rumble_props;
   /* Persistent storage for fields referenced by rumble_props pointers.
    * Previously these lived on the stack in dinput_create_rumble_effects(),
    * causing rumble_props to hold dangling pointers after that call returned. */
   DWORD                rumble_axis;
   LONG                 rumble_direction;
   DIENVELOPE           rumble_envelope;
   DICONSTANTFORCE      rumble_force;
};

/* TODO/FIXME - globals referenced outside; candidate for context-struct refactor */
extern struct dinput_joypad_data g_pads[MAX_USERS];
extern unsigned g_joypad_cnt;

/* Joypad-owned DirectInput context, separate from the shared
 * keyboard/mouse context (g_dinput_ctx) in dinput.c. */
extern LPDIRECTINPUT8 g_dinput_joypad_ctx;

/* True while a pad enumeration task is in flight. */
extern volatile bool g_dinput_enum_inflight;

RETRO_END_DECLS

#endif
