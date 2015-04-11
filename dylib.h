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

#ifndef __DYLIB_H
#define __DYLIB_H

#include <boolean.h>
#include "libretro.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#if defined(HAVE_DYNAMIC) || defined(HAVE_DYLIB)
#define NEED_DYNAMIC
#else
#undef NEED_DYNAMIC
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef void *dylib_t;
typedef void (*function_t)(void);

/**
 * init_libretro_sym:
 * @dummy                        : Load dummy symbols if true
 *
 * Initializes libretro symbols and
 * setups environment callback functions.
 **/
void init_libretro_sym(bool dummy);

/**
 * uninit_libretro_sym:
 *
 * Frees libretro core.
 *
 * Frees all core options,
 * associated state, and
 * unbind all libretro callback symbols.
 **/
void uninit_libretro_sym(void);

#ifdef NEED_DYNAMIC
/**
 * dylib_load:
 * @path                         : Path to libretro core library.
 *
 * Platform independent dylib loading.
 *
 * Returns: library handle on success, otherwise NULL.
 **/
dylib_t dylib_load(const char *path);

/**
 * dylib_close:
 * @lib                          : Library handle.
 *
 * Frees library handle.
 **/
void dylib_close(dylib_t lib);

function_t dylib_proc(dylib_t lib, const char *proc);
#endif


#endif
