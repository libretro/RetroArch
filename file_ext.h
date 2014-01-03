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

#ifndef _FILE_EXT_H
#define _FILE_EXT_H

#ifdef HAVE_DYNAMIC
#ifdef _WIN32
#define EXT_EXECUTABLES "dll"
#elif defined(__APPLE__)
#define EXT_EXECUTABLES "dylib"
#else
#define EXT_EXECUTABLES "so"
#endif
#else
#if defined(__CELLOS_LV2__)
#define EXT_EXECUTABLES "self|SELF|bin|BIN"
#define SALAMANDER_FILE "EBOOT.BIN"
#define PLATFORM_NAME   "ps3"
#define DEFAULT_EXE_EXT ".SELF"
#elif defined(_XBOX1)
#define EXT_EXECUTABLES "xbe|XBE"
#define SALAMANDER_FILE "default.xbe"
#define DEFAULT_EXE_EXT ".xbe"
#define PLATFORM_NAME   "xdk1"
#elif defined(_XBOX360)
#define EXT_EXECUTABLES "xex|XEX"
#define SALAMANDER_FILE "default.xex"
#define DEFAULT_EXE_EXT ".xex"
#define PLATFORM_NAME   "xdk360"
#elif defined(GEKKO)
#define EXT_EXECUTABLES "dol|DOL"
#define SALAMANDER_FILE "boot.dol"
#define DEFAULT_EXE_EXT ".dol"
#ifdef HW_RVL
#define PLATFORM_NAME   "wii"
#else
#define PLATFORM_NAME   "ngc"
#endif
#elif defined(ANDROID)
#define PLATFORM_NAME   "android"
#elif defined(IOS)
#define PLATFORM_NAME   "ios"
#elif defined(__QNX__)
#define PLATFORM_NAME   "qnx"
#elif defined(EMSCRIPTEN)
#define EXT_EXECUTABLES ""
#endif
#endif

#endif
