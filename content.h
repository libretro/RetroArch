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
#include <retro_common_api.h>

#include "frontend/frontend_driver.h"

RETRO_BEGIN_DECLS

typedef struct ram_type ram_type_t;

typedef struct content_ctx_info
{
   int argc;                       /* Argument count. */
   char **argv;                    /* Argument variable list. */
   void *args;                     /* Arguments passed from callee */
   environment_get_t environ_get;  /* Function passed for environment_get function */
} content_ctx_info_t;

/* Load a RAM state from disk to memory. */
bool content_load_ram_file(unsigned slot);

/* Save a RAM state from memory to disk. */
bool content_save_ram_file(unsigned slot);

/* Load a state from disk to memory. */
bool content_load_state(const char* path, bool load_to_backup_buffer);

/* Save a state from memory to disk. */
bool content_save_state(const char *path, bool save_to_disk);

/* Copy a save state. */
bool content_rename_state(const char *origin, const char *dest);

/* Undoes the last load state operation that was done */
bool content_undo_load_state(void);

/* Restores the last savestate file which was overwritten */
bool content_undo_save_state(void);

bool content_does_not_need_content(void);

void content_set_does_not_need_content(void);

void content_unset_does_not_need_content(void);

bool content_get_crc(uint32_t **content_crc_ptr);

bool content_is_inited(void);

void content_deinit(void);

/* Initializes and loads a content file for the currently
 * selected libretro core. */
bool content_init(void);

/* Resets the state and savefile backup buffers */
bool content_reset_savestate_backups(void);

/* Checks if the buffers are empty */
bool content_undo_load_buf_is_empty(void);
bool content_undo_save_buf_is_empty(void);

RETRO_END_DECLS

#endif
