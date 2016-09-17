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

RETRO_BEGIN_DECLS

void path_init_savefile(void);

void path_fill_names(void);

void path_set_redirect(void);

void path_set_names(const char *path);

void path_set_special(char **argv, unsigned num_content);

void path_set_basename(const char *path);

void path_set_core(const char *path);

void path_set_core_options(const char *path);

void path_set_config(const char *path);

char *path_get_core_ptr(void);

const char *path_get_current_savefile_dir(void);

const char *path_get_core(void);

const char *path_get_core_options(void);

const char *path_get_config(void);

size_t path_get_core_size(void);

bool path_is_core_empty(void);

bool path_is_config_empty(void);

bool path_is_core_options_empty(void);

void path_clear_core(void);

void path_clear_config(void);

void path_clear_core_options(void);

void path_clear_all(void);

enum rarch_content_type path_is_media_type(const char *path);

RETRO_END_DECLS

#endif
