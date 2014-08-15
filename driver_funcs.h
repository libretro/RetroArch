/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2014 - Daniel De Matteis
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

// Convenience macros.
#ifndef _RARCH_DRIVER_FUNCS_H
#define _RARCH_DRIVER_FUNCS_H

static inline bool input_key_pressed_func(int key)
{
   bool ret = false;

   if (!driver.block_hotkey)
      ret = ret || driver.input->key_pressed(driver.input_data, key);

#ifdef HAVE_OVERLAY
   ret = ret || (driver.overlay_state.buttons & (1ULL << key));
#endif

#ifdef HAVE_COMMAND
   if (driver.command)
      ret = ret || rarch_cmd_get(driver.command, key);
#endif

   return ret;
}

#endif /* _RARCH_DRIVER_FUNCS_H */
