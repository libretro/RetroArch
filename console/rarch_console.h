/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2013 - Daniel De Matteis
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

#ifndef CONSOLE_EXT_H__
#define CONSOLE_EXT_H__

#ifdef _WIN32
#define MAXIMUM_PATH 260
#else
#define MAXIMUM_PATH 512
#endif

typedef struct
{
   char menu_border_file[MAXIMUM_PATH];
   char border_dir[MAXIMUM_PATH];
   char core_dir[MAXIMUM_PATH];
   char config_path[MAXIMUM_PATH];
   char libretro_path[MAXIMUM_PATH];
   char port_dir[MAXIMUM_PATH];
   char savestate_dir[MAXIMUM_PATH];
   char sram_dir[MAXIMUM_PATH];
   char system_dir[MAXIMUM_PATH];
} default_paths_t;

extern default_paths_t default_paths;

#endif
