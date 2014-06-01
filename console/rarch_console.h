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

#ifndef CONSOLE_EXT_H__
#define CONSOLE_EXT_H__

#include <stdint.h>
#include <stddef.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

typedef struct
{
   char config_path[PATH_MAX];
   char autoconfig_dir[PATH_MAX];
   char assets_dir[PATH_MAX];
   char core_dir[PATH_MAX];
   char core_info_dir[PATH_MAX];
   char overlay_dir[PATH_MAX];
   char port_dir[PATH_MAX];
   char shader_dir[PATH_MAX];
   char savestate_dir[PATH_MAX];
   char sram_dir[PATH_MAX];
   char screenshot_dir[PATH_MAX];
   char system_dir[PATH_MAX];
} default_paths_t;

extern default_paths_t default_paths;

#endif
