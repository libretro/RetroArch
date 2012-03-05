/*  SSNES - A Super Nintendo Entertainment System (SNES) Emulator frontend for libsnes.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2012 - Daniel De Matteis
 *
 *  Some code herein may be based on code found in BSNES.
 * 
 *  SSNES is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  SSNES is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with SSNES.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stddef.h>
#include "input_luts.h"

uint64_t default_keybind_lut[SSNES_FIRST_META_KEY];

#if defined(__CELLOS_LV2__)
uint64_t platform_keybind_lut[SSNES_LAST_PLATFORM_KEY] = {
   CTRL_CIRCLE_MASK,
   CTRL_CROSS_MASK,
   CTRL_TRIANGLE_MASK,
   CTRL_SQUARE_MASK,
   CTRL_UP_MASK,
   CTRL_DOWN_MASK,
   CTRL_LEFT_MASK,
   CTRL_RIGHT_MASK,
   CTRL_SELECT_MASK,
   CTRL_START_MASK,
   CTRL_L1_MASK,
   CTRL_L2_MASK,
   CTRL_L3_MASK,
   CTRL_R1_MASK,
   CTRL_R2_MASK,
   CTRL_R3_MASK,
   CTRL_LSTICK_LEFT_MASK,
   CTRL_LSTICK_RIGHT_MASK,
   CTRL_LSTICK_UP_MASK,
   CTRL_LSTICK_DOWN_MASK,
   CTRL_LEFT_MASK | CTRL_LSTICK_LEFT_MASK,
   CTRL_RIGHT_MASK | CTRL_LSTICK_RIGHT_MASK,
   CTRL_UP_MASK | CTRL_LSTICK_UP_MASK,
   CTRL_DOWN_MASK | CTRL_LSTICK_DOWN_MASK,
   CTRL_RSTICK_LEFT_MASK,
   CTRL_RSTICK_RIGHT_MASK,
   CTRL_RSTICK_UP_MASK,
   CTRL_RSTICK_DOWN_MASK,
   CTRL_LEFT_MASK | CTRL_RSTICK_LEFT_MASK,
   CTRL_RIGHT_MASK | CTRL_RSTICK_RIGHT_MASK,
   CTRL_UP_MASK | CTRL_RSTICK_UP_MASK,
   CTRL_DOWN_MASK | CTRL_RSTICK_DOWN_MASK,
};
#endif

struct platform_bind
{
   uint64_t joykey;
   const char *label;
};

#if defined(__CELLOS_LV2__)
static const struct platform_bind platform_keys[] = {
   { CTRL_CIRCLE_MASK, "Circle button" },
   { CTRL_CROSS_MASK, "Cross button" },
   { CTRL_TRIANGLE_MASK, "Triangle button" },
   { CTRL_SQUARE_MASK, "Square button" },
   { CTRL_UP_MASK, "D-Pad Up" },
   { CTRL_DOWN_MASK, "D-Pad Down" },
   { CTRL_LEFT_MASK, "D-Pad Left" },
   { CTRL_RIGHT_MASK, "D-Pad Right" },
   { CTRL_SELECT_MASK, "Select button" },
   { CTRL_START_MASK, "Start button" },
   { CTRL_L1_MASK, "L1 button" },
   { CTRL_L2_MASK, "L2 button" },
   { CTRL_L3_MASK, "L3 button" },
   { CTRL_R1_MASK, "R1 button" },
   { CTRL_R2_MASK, "R2 button" },
   { CTRL_R3_MASK, "R3 button" },
   { CTRL_LSTICK_LEFT_MASK, "LStick Left " },
   { CTRL_LSTICK_RIGHT_MASK, "LStick Right" },
   { CTRL_LSTICK_UP_MASK, "LStick Up" },
   { CTRL_LSTICK_DOWN_MASK, "LStick Down" },
   { CTRL_LEFT_MASK | CTRL_LSTICK_LEFT_MASK, "LStick D-Pad Left" },
   { CTRL_RIGHT_MASK | CTRL_LSTICK_RIGHT_MASK, "LStick D-Pad Right" },
   { CTRL_UP_MASK | CTRL_LSTICK_UP_MASK, "LStick D-Pad Up" },
   { CTRL_DOWN_MASK | CTRL_LSTICK_DOWN_MASK, "LStick D-Pad Down" },
   { CTRL_RSTICK_LEFT_MASK, "RStick Left" },
   { CTRL_RSTICK_RIGHT_MASK, "RStick Right" },
   { CTRL_RSTICK_UP_MASK, "RStick Up" },
   { CTRL_RSTICK_DOWN_MASK, "RStick Down" },
   { CTRL_LEFT_MASK | CTRL_RSTICK_LEFT_MASK, "RStick D-Pad Left" },
   { CTRL_RIGHT_MASK | CTRL_RSTICK_RIGHT_MASK, "RStick D-Pad Right" },
   { CTRL_UP_MASK | CTRL_RSTICK_UP_MASK, "RStick D-Pad Up" },
   { CTRL_DOWN_MASK | CTRL_RSTICK_DOWN_MASK, "RStick D-Pad Down" },
};
#elif defined(_XBOX)
#endif

uint64_t ssnes_input_find_previous_platform_key(uint64_t joykey)
{
   size_t arr_size = sizeof(platform_keys) / sizeof(platform_keys[0]);

   if (platform_keys[0].joykey == joykey)
      return joykey;

   for (size_t i = 1; i < arr_size; i++)
   {
      if (platform_keys[i].joykey == joykey)
         return platform_keys[i - 1].joykey;
   }

   return NO_BTN;
}

uint64_t ssnes_input_find_next_platform_key(uint64_t joykey)
{
   size_t arr_size = sizeof(platform_keys) / sizeof(platform_keys[0]);
   if (platform_keys[arr_size - 1].joykey == joykey)
      return joykey;

   for (size_t i = 0; i < arr_size - 1; i++)
   {
      if (platform_keys[i].joykey == joykey)
         return platform_keys[i + 1].joykey;
   }

   return NO_BTN;
}

const char *ssnes_input_find_platform_key_label(uint64_t joykey)
{
   if (joykey == NO_BTN)
      return "No button";

   size_t arr_size = sizeof(platform_keys) / sizeof(platform_keys[0]);
   for (size_t i = 0; i < arr_size; i++)
   {
      if (platform_keys[i].joykey == joykey)
         return platform_keys[i].label;
   }

   return "Unknown";
}

