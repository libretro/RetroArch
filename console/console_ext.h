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

#ifndef ROM_EXT_H__
#define ROM_EXT_H__

#include "console_ext_input.h"

/*============================================================
	ROM EXTENSIONS
============================================================ */

void ssnes_console_set_rom_ext(const char *ext);

// Get rom extensions for current library.
// Returns NULL if library doesn't have any preferences in particular.
const char *ssnes_console_get_rom_ext(void);

// Transforms a library id to a name suitable as a pathname.
void ssnes_console_name_from_id(char *name, size_t size);

#ifdef HAVE_ZLIB
int ssnes_extract_zipfile(const char *zip_path);
#endif

/*============================================================
	INPUT EXTENSIONS
============================================================ */

const char *ssnes_input_find_platform_key_label(uint64_t joykey);
uint64_t ssnes_input_find_previous_platform_key(uint64_t joykey);
uint64_t ssnes_input_find_next_platform_key(uint64_t joykey);

// Sets custom default keybind names (some systems emulated by the emulator
// will need different keybind names for buttons, etc.)
void ssnes_input_set_default_keybind_names_for_emulator(void);

#endif
