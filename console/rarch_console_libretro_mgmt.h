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

#ifndef LIBRETRO_MGMT_H__
#define LIBRETRO_MGMT_H__

#include "../boolean.h"

enum
{
   EXTERN_LAUNCHER_SALAMANDER,
#ifdef HAVE_MULTIMAN
   EXTERN_LAUNCHER_MULTIMAN,
#endif
#ifdef GEKKO
   EXTERN_LAUNCHER_CHANNEL,
#endif
};

void rarch_console_name_from_id(char *name, size_t size);

//We need to set libretro to the first entry in the cores
//directory so that it will be saved to the config file
void rarch_manage_libretro_set_first_file(char *first_file, size_t size_of_first_file, const char *libretro_path, const char * exe_ext);

#ifndef IS_SALAMANDER

//install/upgrade (and rename) libretro core if 'CORE' executable exists
bool rarch_libretro_core_install(const char *full_path, const char *tmp_path,
 const char *libretro_path, const char *config_path, const char *extension);

// Transforms a library id to a name suitable as a pathname.
bool rarch_manage_libretro_extension_supported(const char *filename);
#endif


#endif
