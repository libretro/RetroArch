/*  RetroArch - A frontend for libretro.
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

#ifndef __DIRS_H
#define __DIRS_H

#include <boolean.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

enum rarch_dir_type
{
   RARCH_DIR_NONE = 0,
   RARCH_DIR_SAVEFILE,
   RARCH_DIR_SAVESTATE,
   RARCH_DIR_SYSTEM,
   RARCH_DIR_OSK_OVERLAY
};

/* init functions */

bool dir_init_shader(void);

/* free functions */

bool dir_free_shader(void);

/* check functions */

void dir_check_shader(bool pressed_next, bool pressed_prev);

/* empty functions */

bool dir_is_empty(enum rarch_dir_type type);

/* clear functions */

void dir_clear(enum rarch_dir_type type);

void dir_clear_all(void);

/* get size functions */

size_t dir_get_osk_overlay_size(void);

size_t dir_get_system_size(void);

size_t dir_get_savestate_size(void);

size_t dir_get_savefile_size(void);

/* get ptr functions */

char *dir_get_osk_overlay_ptr(void);

char *dir_get_savefile_ptr(void);

char *dir_get_savestate_ptr(void);

char *dir_get_system_ptr(void);

char *dir_get_osk_overlay_ptr(void);

/* get functions */

const char *dir_get_osk_overlay(void);

const char *dir_get_savefile(void);

const char *dir_get_savestate(void);

const char *dir_get_system(void);

const char *dir_get_current_savefile(void);

const char *dir_get_current_savestate(void);

/* set functions */

void dir_set_current_savefile(const char *path);

void dir_set_current_savestate(const char *path);

void dir_set_osk_overlay(const char *path);

void dir_set_savefile(const char *path);

void dir_set_savestate(const char *path);

void dir_set_system(const char *path);

void dir_check_defaults(void);

RETRO_END_DECLS

#endif
