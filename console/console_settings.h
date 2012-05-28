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

#ifndef CONSOLE_SETTINGS_H
#define CONSOLE_SETTINGS_H

enum
{
   S_OVERSCAN_DECREMENT = 0,
   S_OVERSCAN_INCREMENT,
   S_ROTATION_DECREMENT,
   S_ROTATION_INCREMENT,
   S_SAVESTATE_DECREMENT,
   S_SAVESTATE_INCREMENT,
   S_SCALE_ENABLED,
   S_SCALE_FACTOR_DECREMENT,
   S_SCALE_FACTOR_INCREMENT,
   S_THROTTLE,
   S_TRIPLE_BUFFERING,
} changed_settings;

enum
{
   S_DEF_OVERSCAN = 0,
   S_DEF_THROTTLE,
   S_DEF_TRIPLE_BUFFERING,
   S_DEF_SAVE_STATE,
   S_DEF_SCALE_ENABLED,
   S_DEF_SCALE_FACTOR,
} default_settings;

void rarch_settings_change (unsigned setting);
void rarch_settings_default (unsigned setting);

#endif
