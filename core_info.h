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

#include "conf/config_file.h"
#include "file.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
   char *path;
   char *desc;
   bool missing; // Set once to avoid opening the same file several times.
} core_info_firmware_t;

typedef struct {
   char *path;
   config_file_t *data;
   char *display_name;
   char *supported_extensions;
   char *authors;
   struct string_list *supported_extensions_list;
   struct string_list *authors_list;

   core_info_firmware_t *firmware;
   size_t firmware_count;
} core_info_t;

typedef struct {
   core_info_t *list;
   size_t count;
   char *all_ext;
} core_info_list_t;

core_info_list_t *core_info_list_new(const char *modules_path);
void core_info_list_free(core_info_list_t *core_info_list);

size_t core_info_list_num_info_files(core_info_list_t *core_info_list);

bool core_info_does_support_file(const core_info_t *core, const char *path);
bool core_info_does_support_any_file(const core_info_t *core, const struct string_list *list);

// Non-reentrant, does not allocate. Returns pointer to internal state.
void core_info_list_get_supported_cores(core_info_list_t *core_info_list, const char *path,
      const core_info_t **infos, size_t *num_infos);

// Non-reentrant, does not allocate. Returns pointer to internal state.
void core_info_list_get_missing_firmware(core_info_list_t *core_info_list,
      const char *core, const char *systemdir,
      const core_info_firmware_t **firmware, size_t *num_firmware);

const char *core_info_list_get_all_extensions(core_info_list_t *core_info_list);

bool core_info_list_get_display_name(core_info_list_t *core_info_list, const char *path, char *buf, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* CORE_INFO_H_ */
