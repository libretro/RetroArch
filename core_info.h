/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
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

#ifndef CORE_INFO_H_
#define CORE_INFO_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "conf/config_file.h"

typedef struct {
   char * path;
   config_file_t* data;
   char * display_name;
   struct string_list * supported_extensions;
} core_info_t;

typedef struct {
   core_info_t *list;
   int count;
} core_info_list_t;

core_info_list_t *get_core_info_list(const char *modules_path);
void free_core_info_list(core_info_list_t * core_info_list);

bool does_core_support_file(core_info_t* core, const char *path);

#ifdef __cplusplus
}
#endif

#endif /* CORE_INFO_H_ */
