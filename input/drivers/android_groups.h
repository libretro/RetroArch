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

#ifndef _ANDROID_GROUPS_H
#define _ANDROID_GROUPS_H

#include "../input_group.h"

/* Built-in controllers that reach Android as several input devices,
 * read as one pad (input_group.h). The order is the order they are
 * tried in: the SHIELD Android TV's model holds for the SHIELD's too. */
enum
{
   ANDROID_GROUP_SHIELD_TV = 0,
   ANDROID_GROUP_SHIELD_PORTABLE,
   ANDROID_GROUP_SHIELD_GAMEPAD,
   ANDROID_GROUP_GPD_XD,
   ANDROID_GROUP_XPERIA_PLAY,
   ANDROID_GROUP_ARCHOS,
   ANDROID_GROUP_RULES
};

static const input_group_rule_t android_group_rules[ANDROID_GROUP_RULES] = {
   /* The NVIDIA button comes as a "Virtual" device of its own, as does
    * a CEC remote; it joins the controller's pad */
   { "NVIDIA SHIELD Controller", { "SHIELD Android TV" },
      { { "Virtual", INPUT_GROUP_FOLLOWS },
        { "NVIDIA Corporation NVIDIA Controller v01.0", INPUT_GROUP_LEADS } },
      false, false, false },
   { "NVIDIA SHIELD Portable", { "SHIELD" },
      { { "Virtual", INPUT_GROUP_MEMBER }, { "gpio", INPUT_GROUP_MEMBER },
        { "NVIDIA Corporation NVIDIA Controller v01.01", INPUT_GROUP_MEMBER },
        { "NVIDIA Corporation NVIDIA Controller v01.02", INPUT_GROUP_MEMBER } },
      false, false, false },
   { "NVIDIA SHIELD Gamepad", { "SHIELD" },
      { { "NVIDIA Corporation NVIDIA Controller v01.03", INPUT_GROUP_LEADS },
        { "Virtual", INPUT_GROUP_FOLLOWS }, { "gpio", INPUT_GROUP_FOLLOWS } },
      false, false, false },
   /* the back button is a device of its own */
   { "GPD XD", { "XD" },
      { { "Virtual", INPUT_GROUP_MEMBER }, { "rk29-keypad", INPUT_GROUP_MEMBER },
        { "Playstation3", INPUT_GROUP_MEMBER }, { "XBOX", INPUT_GROUP_MEMBER } },
      false, false, true },
   { "XPERIA Play", { "R800", "Play", "SO-01D" },
      { { "keypad-game-zeus", INPUT_GROUP_MEMBER },
        { "keypad-zeus", INPUT_GROUP_MEMBER },
        { "Android Gamepad", INPUT_GROUP_MEMBER } },
      true, true, true },
   { "ARCHOS GamePad", { "ARCHOS GAMEPAD" },
      { { "joy_key", INPUT_GROUP_MEMBER }, { "joystick", INPUT_GROUP_MEMBER } },
      false, false, true }
};

#endif
