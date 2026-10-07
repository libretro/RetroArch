/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2026 - The RetroArch team
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

#ifndef __INPUT_OPENXR_H
#define __INPUT_OPENXR_H

#include <stdint.h>

#include <boolean.h>
#include <retro_common_api.h>
#include <libretro.h>

RETRO_BEGIN_DECLS

/* The headset's controllers through OpenXR: one RetroPad from both, or
 * one per hand, their haptics, and a laser pointer on the headset's
 * screens. Everything here but the rumble runs on the main thread. */

#define INPUT_OPENXR_PADS 2

/* Joins every headset session video makes from now on. */
void input_openxr_register(void);

/* Syncs the controllers; once per input poll, with the Headset
 * Controllers and Laser Pointer settings and the axis threshold. */
void input_openxr_poll(unsigned controllers, unsigned laser,
      float threshold);

/* A RetroPad button held on user port's pad, or RARCH_MENU_TOGGLE,
 * RARCH_HEADSET_RECENTER or RARCH_LASER_POINTER_TOGGLE, which answer
 * for any port. */
bool input_openxr_button(unsigned port, unsigned id);

/* A stick axis (idx LEFT/RIGHT, id X/Y) or analog L2/R2 (idx
 * ANALOG_BUTTON) of user port's pad, or res when that is further from
 * zero. */
int16_t input_openxr_analog(unsigned port, unsigned idx, unsigned id,
      int16_t res);

/* The laser as RETRO_DEVICE_POINTER or RETRO_DEVICE_LIGHTGUN on port 0,
 * while it can point (the headset focused, a screen live for the
 * laser): true, with *res set. A miss reads as offscreen. */
bool input_openxr_pointer(unsigned port, unsigned device, unsigned idx,
      unsigned id, int16_t *res);

/* The laser on the menu quad: where, from 0 to 1 across and down, and
 * whether its trigger is held. */
bool input_openxr_menu_pointer(float *u, float *v, bool *pressed);

/* A core's rumble for user port, applied at the next poll. Any thread. */
bool input_openxr_set_rumble(unsigned port, enum retro_rumble_effect effect,
      uint16_t strength);

/* Whether a core's rumble reaches the controllers now. Any thread. */
bool input_openxr_can_rumble(void);

RETRO_END_DECLS

#endif
