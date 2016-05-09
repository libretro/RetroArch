/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2016 - Daniel De Matteis
 *  Copyright (C) 2013-2015 - Jason Fetters
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

#include <stddef.h>

#include <lists/string_list.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
   char *path;
   char *desc;
   /* Set missing once to avoid opening
    * the same file several times. */
   bool missing;
   bool optional;
} core_info_firmware_t;

typedef struct
{
   char *path;
   void *config_data;
   char *display_name;
   char *core_name;
   char *system_manufacturer;
   char *systemname;
   char *supported_extensions;
   char *authors;
   char *permissions;
   char *licenses;
   char *categories;
   char *databases;
   char *notes;
   struct string_list *categories_list;
   struct string_list *databases_list;
   struct string_list *note_list;   
   struct string_list *supported_extensions_list;
   struct string_list *authors_list;
   struct string_list *permissions_list;
   struct string_list *licenses_list;

   core_info_firmware_t *firmware;
   size_t firmware_count;
   bool supports_no_game;
   void *userdata;
} core_info_t;

typedef struct
{
   core_info_t *list;
   size_t count;
   char *all_ext;
} core_info_list_t;

typedef struct core_info_ctx_firmware
{
   const char *path;
   struct
   {
      const char *system;
   } directory;
} core_info_ctx_firmware_t;

typedef struct core_info_ctx_find
{
   core_info_t *inf;
   const char *path;
} core_info_ctx_find_t;

size_t core_info_list_num_info_files(core_info_list_t *list);

/* Non-reentrant, does not allocate. Returns pointer to internal state. */
void core_info_list_get_supported_cores(core_info_list_t *list,
      const char *path, const core_info_t **infos, size_t *num_infos);

bool core_info_list_get_display_name(core_info_list_t *list,
      const char *path, char *s, size_t len);

bool core_info_get_display_name(const char *path, char *s, size_t len);

void core_info_get_name(const char *path, char *s, size_t len);

core_info_t *core_info_get(core_info_list_t *list, size_t i);

void core_info_free_current_core(void);

bool core_info_init_current_core(void);

bool core_info_get_current_core(core_info_t **core);

void core_info_deinit_list(void);

bool core_info_init_list(void);

bool core_info_get_list(core_info_list_t **core);

bool core_info_list_update_missing_firmware(core_info_ctx_firmware_t *info);

bool core_info_find(core_info_ctx_find_t *info);

bool core_info_load(core_info_ctx_find_t *info);

#ifdef __cplusplus
}
#endif

#endif /* CORE_INFO_H_ */
