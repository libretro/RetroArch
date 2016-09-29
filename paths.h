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

#ifndef __PATHS_H
#define __PATHS_H

#include <boolean.h>
#include <retro_common_api.h>

#include <lists/string_list.h>

RETRO_BEGIN_DECLS

enum rarch_content_type
{
   RARCH_CONTENT_NONE = 0,
   RARCH_CONTENT_MOVIE,
   RARCH_CONTENT_MUSIC,
   RARCH_CONTENT_IMAGE
};

enum rarch_path_type
{
   RARCH_PATH_NONE = 0,
   RARCH_PATH_CORE,
   RARCH_PATH_CONFIG,
   RARCH_PATH_CONTENT,
   RARCH_PATH_CONFIG_APPEND,
   RARCH_PATH_CORE_OPTIONS,
   RARCH_PATH_DEFAULT_SHADER_PRESET,
   RARCH_PATH_SUBSYSTEM
};

void path_deinit_subsystem(void);

void path_deinit_savefile(void);

void path_init_savefile(void);

/* fill functions */

void path_fill_names(void);

/* set functions */


bool path_set(enum rarch_path_type type, const char *path);

void path_set_redirect(void);

void path_set_names(const char *path);

void path_set_special(char **argv, unsigned num_content);

void path_set_basename(const char *path);

/* get size functions */

size_t path_get_core_size(void);

/* get ptr functions */

char *path_get_core_ptr(void);

/* get list functions */

struct string_list *path_get_subsystem_list(void);

/* get functions */

const char *path_get(enum rarch_path_type type);

bool path_get_content(char **fullpath);

const char *path_get_current_savefile_dir(void);

const char *path_get_basename(void);

const char *path_get_core_options(void);

bool path_get_default_shader_preset(char **preset);

/* clear functions */

void path_clear_subsystem(void);

void path_clear_default_shader_preset(void);

void path_clear_basename(void);

void path_clear_content(void);

void path_clear_core(void);

void path_clear_config(void);

void path_clear_core_options(void);

void path_clear_config_append(void);

void path_clear_all(void);

/* is functions */

bool path_is_subsystem_empty(void);

bool path_is_core_empty(void);

bool path_is_config_empty(void);

bool path_is_core_options_empty(void);

bool path_is_config_append_empty(void);

enum rarch_content_type path_is_media_type(const char *path);

RETRO_END_DECLS

#endif
