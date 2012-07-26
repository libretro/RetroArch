/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2012 - Daniel De Matteis
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

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include "../boolean.h"
#include "retroarch_console_input.h"

struct platform_bind
{
   uint64_t joykey;
   const char *label;
};

uint64_t rarch_default_keybind_lut[RARCH_FIRST_META_KEY];

char rarch_default_libretro_keybind_name_lut[RARCH_FIRST_META_KEY][256] = {
   "RetroPad Button B",          /* RETRO_DEVICE_ID_JOYPAD_B      */
   "RetroPad Button Y",          /* RETRO_DEVICE_ID_JOYPAD_Y      */
   "RetroPad Button Select",     /* RETRO_DEVICE_ID_JOYPAD_SELECT */
   "RetroPad Button Start",      /* RETRO_DEVICE_ID_JOYPAD_START  */
   "RetroPad D-Pad Up",          /* RETRO_DEVICE_ID_JOYPAD_UP     */
   "RetroPad D-Pad Down",        /* RETRO_DEVICE_ID_JOYPAD_DOWN   */
   "RetroPad D-Pad Left",        /* RETRO_DEVICE_ID_JOYPAD_LEFT   */
   "RetroPad D-Pad Right",       /* RETRO_DEVICE_ID_JOYPAD_RIGHT  */
   "RetroPad Button A",          /* RETRO_DEVICE_ID_JOYPAD_A      */
   "RetroPad Button X",          /* RETRO_DEVICE_ID_JOYPAD_X      */
   "RetroPad Button L1",         /* RETRO_DEVICE_ID_JOYPAD_L      */
   "RetroPad Button R1",         /* RETRO_DEVICE_ID_JOYPAD_R      */
   "RetroPad Button L2",         /* RETRO_DEVICE_ID_JOYPAD_L2     */
   "RetroPad Button R2",         /* RETRO_DEVICE_ID_JOYPAD_R2     */
   "RetroPad Button L3",         /* RETRO_DEVICE_ID_JOYPAD_L3     */
   "RetroPad Button R3",         /* RETRO_DEVICE_ID_JOYPAD_R3     */
};

#ifdef HAVE_DEFAULT_RETROPAD_INPUT

extern const struct platform_bind platform_keys[];
extern const unsigned int platform_keys_size;

uint64_t rarch_input_find_previous_platform_key(uint64_t joykey)
{
   size_t arr_size = platform_keys_size / sizeof(platform_keys[0]);

   if (platform_keys[0].joykey == joykey)
      return joykey;

   for (size_t i = 1; i < arr_size; i++)
   {
      if (platform_keys[i].joykey == joykey)
         return platform_keys[i - 1].joykey;
   }

   return NO_BTN;
}

uint64_t rarch_input_find_next_platform_key(uint64_t joykey)
{
   size_t arr_size = platform_keys_size / sizeof(platform_keys[0]);

   if (platform_keys[arr_size - 1].joykey == joykey)
      return joykey;

   for (size_t i = 0; i < arr_size - 1; i++)
   {
      if (platform_keys[i].joykey == joykey)
         return platform_keys[i + 1].joykey;
   }

   return NO_BTN;
}

const char *rarch_input_find_platform_key_label(uint64_t joykey)
{
   if (joykey == NO_BTN)
      return "No button";

   size_t arr_size = platform_keys_size / sizeof(platform_keys[0]);
   for (size_t i = 0; i < arr_size; i++)
   {
      if (platform_keys[i].joykey == joykey)
         return platform_keys[i].label;
   }

   return "Unknown";
}


void rarch_input_set_keybind(unsigned player, unsigned keybind_action, uint64_t default_retro_joypad_id)
{
   uint64_t *key = &g_settings.input.binds[player][default_retro_joypad_id].joykey;

   switch (keybind_action)
   {
      case KEYBIND_DECREMENT:
         *key = rarch_input_find_previous_platform_key(*key);
         break;

      case KEYBIND_INCREMENT:
         *key = rarch_input_find_next_platform_key(*key);
         break;

      case KEYBIND_DEFAULT:
         *key = rarch_default_keybind_lut[default_retro_joypad_id];
         break;

      default:
         break;
   }
}

void rarch_input_set_default_keybinds(unsigned player)
{
   for (unsigned i = 0; i < RARCH_FIRST_META_KEY; i++)
   {
      g_settings.input.binds[player][i].id = i;
      g_settings.input.binds[player][i].joykey = rarch_default_keybind_lut[i];
   }
   g_settings.input.dpad_emulation[player] = DPAD_EMULATION_LSTICK;
}

void rarch_input_set_controls_default (const input_driver_t *input)
{
   input->set_default_keybind_lut();

   for(uint32_t x = 0; x < MAX_PLAYERS; x++)
      rarch_input_set_default_keybinds(x);
}

const char *rarch_input_get_default_keybind_name(unsigned id)
{
   return rarch_default_libretro_keybind_name_lut[id];
}

#endif

/* TODO: Hackish, try to do this in a cleaner, more extensible and less hardcoded way */

void rarch_input_set_default_keybind_names_for_emulator(void)
{
   struct retro_system_info info;
#ifdef ANDROID
   pretro_get_system_info(&info);
#else
   retro_get_system_info(&info);
#endif
   const char *id = info.library_name ? info.library_name : "Unknown";

   // Genesis Plus GX/Next
   if (strstr(id, "Genesis Plus GX"))
   {
      strlcpy(rarch_default_libretro_keybind_name_lut[RETRO_DEVICE_ID_JOYPAD_B],
            "B button", sizeof(rarch_default_libretro_keybind_name_lut[RETRO_DEVICE_ID_JOYPAD_B]));
      strlcpy(rarch_default_libretro_keybind_name_lut[RETRO_DEVICE_ID_JOYPAD_A],
            "C button", sizeof(rarch_default_libretro_keybind_name_lut[RETRO_DEVICE_ID_JOYPAD_A]));
      strlcpy(rarch_default_libretro_keybind_name_lut[RETRO_DEVICE_ID_JOYPAD_X],
            "Y button", sizeof(rarch_default_libretro_keybind_name_lut[RETRO_DEVICE_ID_JOYPAD_X]));
      strlcpy(rarch_default_libretro_keybind_name_lut[RETRO_DEVICE_ID_JOYPAD_Y],
            "A button", sizeof(rarch_default_libretro_keybind_name_lut[RETRO_DEVICE_ID_JOYPAD_Y]));
      strlcpy(rarch_default_libretro_keybind_name_lut[RETRO_DEVICE_ID_JOYPAD_L],
            "X button", sizeof(rarch_default_libretro_keybind_name_lut[RETRO_DEVICE_ID_JOYPAD_L]));
      strlcpy(rarch_default_libretro_keybind_name_lut[RETRO_DEVICE_ID_JOYPAD_R],
            "Z button", sizeof(rarch_default_libretro_keybind_name_lut[RETRO_DEVICE_ID_JOYPAD_R]));
      strlcpy(rarch_default_libretro_keybind_name_lut[RETRO_DEVICE_ID_JOYPAD_SELECT],
            "Mode button", sizeof(rarch_default_libretro_keybind_name_lut[RETRO_DEVICE_ID_JOYPAD_SELECT]));
   }
}
