/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2015 - Daniel De Matteis
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

#ifndef _FILE_EXT_H
#define _FILE_EXT_H

#ifdef HAVE_DYNAMIC
#ifdef _WIN32
#define EXT_EXECUTABLES "dll"
#elif defined(__APPLE__) || defined(__MACH__)
#define EXT_EXECUTABLES "dylib"
#else
#define EXT_EXECUTABLES "so"
#endif
#else
#if defined(__CELLOS_LV2__)
#define EXT_EXECUTABLES "self|bin"
#define SALAMANDER_FILE "EBOOT.BIN"
#elif defined(PSP)
#define EXT_EXECUTABLES "pbp"
#define SALAMANDER_FILE "EBOOT.PBP"
#elif defined(VITA)
#define EXT_EXECUTABLES "velf"
#define SALAMANDER_FILE "default.velf"
#elif defined(_XBOX1)
#define EXT_EXECUTABLES "xbe"
#define SALAMANDER_FILE "default.xbe"
#elif defined(_XBOX360)
#define EXT_EXECUTABLES "xex"
#define SALAMANDER_FILE "default.xex"
#elif defined(GEKKO)
#define EXT_EXECUTABLES "dol"
#define SALAMANDER_FILE "boot.dol"
#else
#define EXT_EXECUTABLES ""
#endif
#endif

#endif
