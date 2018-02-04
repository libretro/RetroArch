/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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
   RARCH_DIR_CURRENT_SAVEFILE,
   RARCH_DIR_CURRENT_SAVESTATE,
   RARCH_DIR_SYSTEM
};

bool dir_init_shader(void);

bool dir_free_shader(void);

void dir_check_shader(bool pressed_next, bool pressed_prev);

bool dir_is_empty(enum rarch_dir_type type);

void dir_clear(enum rarch_dir_type type);

void dir_clear_all(void);

size_t dir_get_size(enum rarch_dir_type type);

char *dir_get_ptr(enum rarch_dir_type type);

const char *dir_get(enum rarch_dir_type type);

void dir_set(enum rarch_dir_type type, const char *path);

void dir_check_defaults(void);

RETRO_END_DECLS

#endif
