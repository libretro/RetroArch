/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2016 - Daniel De Matteis
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

#ifndef __RARCH_FILE_H
#define __RARCH_FILE_H

#include <boolean.h>
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

enum content_ctl_state
{
   CONTENT_CTL_NONE = 0,

   CONTENT_CTL_IS_INITED,

   /* Initializes and loads a content file for the currently
    * selected libretro core. */
   CONTENT_CTL_INIT,

   CONTENT_CTL_DEINIT,

   /* Load a state from disk to memory. */
   CONTENT_CTL_LOAD_STATE,

   /* Save a state from memory to disk. */
   CONTENT_CTL_SAVE_STATE,

   /* Frees temporary content handle. */
   CONTENT_CTL_TEMPORARY_FREE
};


/**
 * load_ram_file:
 * @path             : path of RAM state that will be loaded from.
 * @type             : type of memory
 *
 * Load a RAM state from disk to memory.
 */
void load_ram_file(const char *path, int type);

/**
 * save_ram_file:
 * @path             : path of RAM state that shall be written to.
 * @type             : type of memory
 *
 * Save a RAM state from memory to disk.
 *
 * In case the file could not be written to, a fallback function
 * 'dump_to_file_desperate' will be called.
 */
void save_ram_file(const char *path, int type);

bool content_ctl(enum content_ctl_state state, void *data);

#ifdef __cplusplus
}
#endif

#endif
