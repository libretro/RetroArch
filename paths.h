/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2019 - Daniel De Matteis
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

#ifndef __PATHS_H
#define __PATHS_H

#include <boolean.h>
#include <retro_common_api.h>

#include <lists/string_list.h>

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

enum rarch_content_type
{
   RARCH_CONTENT_NONE = 0,
   RARCH_CONTENT_MOVIE,
   RARCH_CONTENT_MUSIC,
   RARCH_CONTENT_IMAGE,
   RARCH_CONTENT_GONG
};

enum rarch_path_type
{
   RARCH_PATH_NONE = 0,
   RARCH_PATH_CORE,
   RARCH_PATH_NAMES,
   RARCH_PATH_CONFIG,
   RARCH_PATH_CONTENT,
   RARCH_PATH_CONFIG_APPEND,
   RARCH_PATH_CORE_OPTIONS,
   RARCH_PATH_DEFAULT_SHADER_PRESET,
   RARCH_PATH_BASENAME,
   RARCH_PATH_SUBSYSTEM
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

void path_deinit_subsystem(void);

void path_deinit_savefile(void);

void path_init_savefile(void);

void path_fill_names(void);

bool path_set(enum rarch_path_type type, const char *path);

void path_set_redirect(void);

void path_set_special(char **argv, unsigned num_content);

size_t path_get_realsize(enum rarch_path_type type);

struct string_list *path_get_subsystem_list(void);

char *path_get_ptr(enum rarch_path_type type);

const char *path_get(enum rarch_path_type type);

void path_clear(enum rarch_path_type type);

void path_clear_all(void);

bool path_is_empty(enum rarch_path_type type);

enum rarch_content_type path_is_media_type(const char *path);

RETRO_END_DECLS

#endif
