/* RetroArch - A frontend for libretro.
 * Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 * Copyright (C) 2011-2013 - Daniel De Matteis
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

#ifndef _FRONTEND_CONSOLE_H
#define _FRONTEND_CONSOLE_H

//optional RetroArch forward declarations
static void rarch_console_exec(const char *path);
static void verbose_log_init(void);

#ifdef IS_SALAMANDER
//optional Salamander forward declarations
static void find_first_libretro_core(char *first_file,
   size_t size_of_first_file, const char *dir,
   const char * ext);
#endif

#ifdef HAVE_LIBRETRO_MANAGEMENT

static void get_libretro_core_name(char *name, size_t size);
static bool install_libretro_core(const char *core_exe_path, const char *tmp_path,
 const char *libretro_path, const char *config_path, const char *extension);

#endif

#endif
