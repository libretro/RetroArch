/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2016-2019 - Brad Parker
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
#include <retro_miscellaneous.h>

#include "frontend/frontend_driver.h"

RETRO_BEGIN_DECLS

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
bool content_load_state(const char* path, bool load_to_backup_buffer, bool autoload);

/* Save a state from memory to disk. */
bool content_save_state(const char *path, bool save_to_disk, bool autosave);

/* Copy a save state. */
bool content_rename_state(const char *origin, const char *dest);

/* Undoes the last load state operation that was done */
bool content_undo_load_state(void);

/* Restores the last savestate file which was overwritten */
bool content_undo_save_state(void);

void content_get_status(bool *contentless,
      bool *is_inited);

void content_set_does_not_need_content(void);

void content_unset_does_not_need_content(void);

uint32_t content_get_crc(void);

void content_deinit(void);

/* Initializes and loads a content file for the currently
 * selected libretro core. */
bool content_init(void);

/* Resets the state and savefile backup buffers */
bool content_reset_savestate_backups(void);

/* Checks if the buffers are empty */
bool content_undo_load_buf_is_empty(void);
bool content_undo_save_buf_is_empty(void);

/* Checks if launched from the commandline */
bool content_launched_from_cli(void);

/* Clears the pending subsystem rom buffer */
bool content_is_subsystem_pending_load(void);

/* Clears the pending subsystem rom buffer */
void content_clear_subsystem(void);

/* Set the current subsystem*/
void content_set_subsystem(unsigned subsystem);

/* Get the current subsystem*/
int content_get_subsystem(void);

/* Add a rom to the subsystem rom buffer */
void content_add_subsystem(const char* path);

/* Get the current subsystem rom id */
unsigned content_get_subsystem_rom_id(void);

/* Set environment variables before a subsystem load */
void content_set_subsystem_info(void);

/* Get the path to the last selected subsystem rom */
char* content_get_subsystem_rom(unsigned index);

/* Sets the subsystem by name */
bool content_set_subsystem_by_name(const char* subsystem_name);

/* Get the current subsystem "friendly name" */
void content_get_subsystem_friendly_name(const char* subsystem_name, char* subsystem_friendly_name, size_t len);

RETRO_END_DECLS

#endif
