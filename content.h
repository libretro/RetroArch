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

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>

#include <boolean.h>

#include "frontend/frontend_driver.h"

#ifdef __cplusplus
extern "C" {
#endif

enum content_ctl_state
{
   CONTENT_CTL_NONE = 0,

   CONTENT_CTL_IS_INITED,

   CONTENT_CTL_DOES_NOT_NEED_CONTENT,

   CONTENT_CTL_SET_DOES_NOT_NEED_CONTENT,

   /* Initializes and loads a content file for the currently
    * selected libretro core. */
   CONTENT_CTL_INIT,

   CONTENT_CTL_DEINIT,

   CONTENT_CTL_GET_CRC,

   /* Load a RAM state from disk to memory. */
   CONTENT_CTL_LOAD_RAM_FILE,

   /* Save a RAM state from memory to disk. */
   CONTENT_CTL_SAVE_RAM_FILE,

   /* Load a state from disk to memory. */
   CONTENT_CTL_LOAD_STATE,

   /* Save a state from memory to disk. */
   CONTENT_CTL_SAVE_STATE,

   /* Frees temporary content handle. */
   CONTENT_CTL_TEMPORARY_FREE
};

typedef struct ram_type
{
   const char *path;
   int type;
} ram_type_t;

/**
 * main_load_content:
 * @argc             : Argument count.
 * @argv             : Argument variable list.
 * @args             : Arguments passed from callee.
 * @environ_get      : Function passed for environment_get function.
 *
 * Loads content file and starts up RetroArch.
 * If no content file can be loaded, will start up RetroArch
 * as-is.
 *
 * Returns: false (0) if rarch_main_init failed, otherwise true (1).
 **/
bool content_load(int argc, char **argv,
      void *args, environment_get_t environ_get);

void content_push_to_history_playlist(bool do_push,
      const char *path, void *data);

bool content_ctl(enum content_ctl_state state, void *data);

#ifdef HAVE_COMPRESSION
/* Generic compressed file loader.
 * Extracts to buf, unless optional_filename != 0
 * Then extracts to optional_filename and leaves buf alone.
 */
int content_file_compressed_read(const char * path, void **buf,
      const char* optional_filename, ssize_t *length);
#endif

/**
 * content_file_read:
 * @path             : path to file.
 * @buf              : buffer to allocate and read the contents of the
 *                     file into. Needs to be freed manually.
 * @length           : Number of items read, -1 on error.
 *
 * Read the contents of a file into @buf. Will call read_compressed_file
 * if path contains a compressed file, otherwise will call read_generic_file.
 *
 * Returns: true (1) if file read, false (0) on error.
 */
int content_file_read(const char *path, void **buf, ssize_t *length);

#ifdef __cplusplus
}
#endif

#endif
