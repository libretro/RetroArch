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

#include <compat/strl.h>
#include <retro_assert.h>
#include <streams/file_stream.h>
#include <file/file_path.h>
#include <retro_stat.h>
#include <string/stdstring.h>

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include "menu_content.h"
#include "menu_driver.h"
#include "menu_display.h"
#include "menu_shader.h"

#include "../core_info.h"
#include "../configuration.h"
#include "../defaults.h"
#include "../playlist.h"
#include "../verbosity.h"

/**
 * menu_content_load_from_playlist:
 * @playlist             : Playlist handle.
 * @idx                  : Index in playlist.
 *
 * Initializes core and loads content based on playlist entry.
 **/
bool menu_content_playlist_load(menu_content_ctx_playlist_info_t *info)
{
   const char *path     = NULL;
   playlist_t *playlist = (playlist_t*)info->data;

   if (!playlist)
      return false;

   playlist_get_index(playlist,
         info->idx, &path, NULL, NULL, NULL, NULL, NULL);

   if (string_is_empty(path))
      return false;

   {
      unsigned i;
      bool valid_path     = false;
      char *path_check    = NULL;
      char *path_tolower  = strdup(path);

      for (i = 0; i < strlen(path_tolower); ++i)
         path_tolower[i] = tolower(path_tolower[i]);

      if (strstr(path_tolower, file_path_str(FILE_PATH_ZIP_EXTENSION)))
         strstr(path_tolower, file_path_str(FILE_PATH_ZIP_EXTENSION))[4] = '\0';
      else if (strstr(path_tolower, file_path_str(FILE_PATH_7Z_EXTENSION)))
         strstr(path_tolower, file_path_str(FILE_PATH_7Z_EXTENSION))[3] = '\0';

      path_check = (char *)
         calloc(strlen(path_tolower) + 1, sizeof(char));

      strncpy(path_check, path, strlen(path_tolower));

      valid_path = path_is_valid(path_check);

      free(path_tolower);
      free(path_check);

      if (!valid_path)
         return false;
   }

   return true;
}

bool menu_content_playlist_find_associated_core(const char *path, char *s, size_t len)
{
   unsigned j;
   bool                                ret = false;
   settings_t *settings                    = config_get_ptr();
   struct string_list *existing_core_names = 
      string_split(settings->playlist_names, ";");
   struct string_list *existing_core_paths = 
      string_split(settings->playlist_cores, ";");

   for (j = 0; j < existing_core_names->size; j++)
   {
      if (!string_is_equal(path, existing_core_names->elems[j].data))
         continue;

      if (existing_core_paths)
      {
         const char *existing_core = existing_core_paths->elems[j].data;

         if (existing_core)
         {
            strlcpy(s, existing_core, len);
            ret = true;
         }
      }

      break;
   }

   string_list_free(existing_core_names);
   string_list_free(existing_core_paths);
   return ret;
}

/**
 * menu_content_find_first_core:
 * @core_info            : Core info list handle.
 * @dir                  : Directory. Gets joined with @path.
 * @path                 : Path. Gets joined with @dir.
 * @menu_label           : Label identifier of menu setting.
 * @s                    : Deferred core path. Will be filled in
 *                         by function.
 * @len                  : Size of @s.
 *
 * Gets deferred core.
 *
 * Returns: false if there are multiple deferred cores and a
 * selection needs to be made from a list, otherwise
 * returns true and fills in @s with path to core.
 **/
bool menu_content_find_first_core(menu_content_ctx_defer_info_t *def_info,
      bool load_content_with_current_core,
      char *new_core_path, size_t len)
{
   const core_info_t *info                 = NULL;
   size_t supported                        = 0;
   core_info_list_t *core_info             = (core_info_list_t*)def_info->data;
   const char *default_info_dir            = def_info->dir;

   if (!string_is_empty(default_info_dir))
   {
      const char *default_info_path = NULL;
      size_t default_info_length    = 0;

      if (def_info)
      {
         default_info_path = def_info->path;
         default_info_length    = def_info->len;
      }

      if (!string_is_empty(default_info_path))
         fill_pathname_join(def_info->s, 
               default_info_dir, default_info_path,
               default_info_length);

#ifdef HAVE_COMPRESSION
      if (path_is_compressed_file(default_info_dir))
      {
         size_t len = strlen(default_info_dir);
         /* In case of a compressed archive, we have to join with a hash */
         /* We are going to write at the position of dir: */
         retro_assert(len < strlen(def_info->s));
         def_info->s[len] = '#';
      }
#endif
   }

   if (core_info)
      core_info_list_get_supported_cores(core_info,
            def_info->s, &info,
            &supported);

   /* We started the menu with 'Load Content', we are 
    * going to use the current core to load this. */
   if (load_content_with_current_core)
   {
      core_info_get_current_core((core_info_t**)&info);
      if (info)
      {
         RARCH_LOG("Use the current core (%s) to load this content...\n",
               info->path);
         supported = 1;
      }
   }

   /* There are multiple deferred cores and a
    * selection needs to be made from a list, return 0. */
   if (supported != 1)
      return false;

    if (info)
      strlcpy(new_core_path, info->path, len);

   return true;
}
