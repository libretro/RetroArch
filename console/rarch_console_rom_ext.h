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

#ifndef RARCH_ROM_EXT_H__
#define RARCH_ROM_EXT_H__

void rarch_console_load_game_wrap(const char *path, unsigned extract_zip_mode, unsigned delay);

// Get rom extensions for current library.
// Returns NULL if library doesn't have any preferences in particular.
const char *rarch_console_get_rom_ext(void);

#ifdef HAVE_ZLIB
#define WRITEBUFFERSIZE (1024 * 512)

enum
{
   ZIP_EXTRACT_TO_CURRENT_DIR = 0,
   ZIP_EXTRACT_TO_CURRENT_DIR_AND_LOAD_FIRST_FILE,
   ZIP_EXTRACT_TO_CACHE_DIR
};
#endif

#endif
