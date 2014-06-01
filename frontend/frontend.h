/* RetroArch - A frontend for libretro.
 * Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 * Copyright (C) 2011-2014 - Daniel De Matteis
 * Copyright (C) 2012-2014 - Michael Lelli
 *
 * RetroArch is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Found-
 * ation, either version 3 of the License, or (at your option) any later version.
 *
 * RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with RetroArch.
 * If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _RARCH_FRONTEND_H
#define _RARCH_FRONTEND_H

#include <stdint.h>
#include <stddef.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#if defined(ANDROID)
#define args_type() struct android_app*
#define signature() void* data
#else
#define args_type() void*
#define signature() int argc, char *argv[]
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

default_paths_t default_paths;

#ifdef __cplusplus
extern "C" {
#endif

int main_entry_iterate(signature(), args_type() args);
void main_exit(args_type() args);

#ifdef __cplusplus
}
#endif

#endif
