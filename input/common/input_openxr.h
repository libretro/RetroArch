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

/* The headset's controllers through OpenXR: a joypad device from both,
 * or one per hand, their haptics, and a laser pointer on the headset's
 * screens. Everything here but can_rumble and active runs on the main
 * thread. */

#define INPUT_OPENXR_PADS 2

/* The devices' joypad driver, and their names: both hands, or each in
 * Separate. Each name has a built-in profile, the RetroPad's ids as
 * buttons, the sticks as axes 0 to 3, L2 and R2 as axes 4 and 5. */
#define INPUT_OPENXR_JOYPAD          "openxr"
#define INPUT_OPENXR_JOYPAD_COMBINED "OpenXR Controllers"
#define INPUT_OPENXR_JOYPAD_LEFT     "OpenXR Left Controller"
#define INPUT_OPENXR_JOYPAD_RIGHT    "OpenXR Right Controller"

struct rarch_joypad_driver;

/* Joins every headset session video makes from now on. */
void input_openxr_register(void);

/* Syncs the controllers, and connects or disconnects their devices;
 * once per input poll, with Max Users, the Headset Controllers, Laser
 * Pointer and Laser Beam settings and the axis threshold. */
void input_openxr_poll(unsigned max_users, unsigned controllers,
      unsigned laser, unsigned beam, float threshold);

/* @joypad, or while the devices have slots a joypad driver that
 * answers theirs and passes every other to @joypad. */
const struct rarch_joypad_driver *input_openxr_joypad(
      const struct rarch_joypad_driver *joypad);

/* Whether a device of the headset's is in @slot. */
bool input_openxr_has_slot(unsigned slot);

/* @provider's controller was connected to @slot: another driver's
 * takes it from the headset. */
void input_openxr_slot_connected(unsigned slot, const char *provider);

/* The joypad driver restarted: the devices are reported again. */
void input_openxr_reannounce(void);

/* Looks up the profile of the device in @slot again, if it is the
 * headset's. */
bool input_openxr_autoconfigure(unsigned slot);

/* RARCH_MENU_TOGGLE, RARCH_HEADSET_RECENTER, RARCH_LASER_POINTER_TOGGLE,
 * RARCH_HEADSET_VIEW_TOGGLE or RARCH_HEADSET_EDIT_SCREENS, from the
 * headset's own actions. */
bool input_openxr_hotkey(unsigned id);

/* Edit Screens, main thread, between frames: a pause draws a frame.
 * Toggle: stops edit mode, false; otherwise whether it can start, the
 * desktop told when not. Begin, once the menu was asked to close: on at
 * once, pausing running content when @pause (While Editing Screens)
 * asks, held screens snapping when @snap (Snap to Screens) does;
 * Snap to Angles' step, @snap_angles, Hinge Gap, @hinge_gap, and Edit
 * Smoothing's level, @smoothing, hold until edit mode ends; a menu that
 * stayed open (no content) is told instead. End: off at once, as a
 * video reinit wants. */
bool input_openxr_edit_toggle(void);
void input_openxr_edit_begin(bool pause, bool snap, unsigned snap_angles,
      unsigned hinge_gap, unsigned smoothing);
void input_openxr_edit_end(void);

/* Main thread: what each hand holds is let go where it is, as before a
 * recenter. Edit mode stays on. */
void input_openxr_edit_release(void);

/* The laser as RETRO_DEVICE_POINTER or RETRO_DEVICE_LIGHTGUN on port 0,
 * while it can point (the headset focused, a screen live for the
 * laser): true, with *res set. A miss reads as offscreen. */
bool input_openxr_pointer(unsigned port, unsigned device, unsigned idx,
      unsigned id, int16_t *res);

/* The laser on the menu quad: where, from 0 to 1 across and down, and
 * whether its trigger is held. */
bool input_openxr_menu_pointer(float *u, float *v, bool *pressed);

/* Marks, on the hands the host sampled with the head, which are held
 * and through which interaction profile; a hand neither held nor
 * tracked is cleared. Main thread. */
void input_openxr_controller_poses(struct retro_vr_controller_poses *out);

/* Whether a core's rumble reaches the controllers now. Any thread. */
bool input_openxr_can_rumble(void);

/* Any thread: the controllers are set up on a live session. */
bool input_openxr_active(void);

RETRO_END_DECLS

#endif
