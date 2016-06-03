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

#include <retro_assert.h>
#include <streams/file_stream.h>
#include <file/file_path.h>
#include <retro_stat.h>
#include <string/stdstring.h>

#include "menu_content.h"
#include "menu_driver.h"
#include "menu_display.h"
#include "menu_hash.h"
#include "menu_shader.h"

#include "../core_info.h"
#include "../content.h"
#include "../configuration.h"
#include "../dynamic.h"
#include "../defaults.h"
#include "../frontend/frontend.h"
#include "../playlist.h"
#include "../retroarch.h"
#include "../runloop.h"
#include "../verbosity.h"

#include "../tasks/tasks_internal.h"

/**
 * menu_content_load_from_playlist:
 * @playlist             : Playlist handle.
 * @idx                  : Index in playlist.
 *
 * Initializes core and loads content based on playlist entry.
 **/
static bool menu_content_load_from_playlist(menu_content_ctx_playlist_info_t *info)
{
   unsigned idx;
   playlist_t *playlist            = NULL;
   const char *core_path           = NULL;
   const char *path                = NULL;
   content_ctx_info_t content_info = {0};
   
   if (!info)
      return false;

   playlist = (playlist_t*)info->data;
   idx      = info->idx;

   if (!playlist)
      return false;

   playlist_get_index(playlist,
         idx, &path, NULL, &core_path, NULL, NULL, NULL);

   if (!string_is_empty(path))
   {
      unsigned i;
      bool valid_path     = false;
      char *path_check    = NULL;
      char *path_tolower  = strdup(path);

      for (i = 0; i < strlen(path_tolower); ++i)
         path_tolower[i] = tolower(path_tolower[i]);

      if (strstr(path_tolower, ".zip"))
         strstr(path_tolower, ".zip")[4] = '\0';
      else if (strstr(path_tolower, ".7z"))
         strstr(path_tolower, ".7z")[3] = '\0';

      path_check = (char *)
         calloc(strlen(path_tolower) + 1, sizeof(char));

      strncpy(path_check, path, strlen(path_tolower));

      valid_path = path_is_valid(path_check);

      free(path_tolower);
      free(path_check);

      if (!valid_path)
         goto error;
   }

   if (task_push_content_load_default(
         core_path,
         path,
         &content_info,
         CORE_TYPE_PLAIN,
         CONTENT_MODE_LOAD_CONTENT_FROM_PLAYLIST_FROM_MENU,
         NULL,
         NULL))
      return true;

error:
   runloop_msg_queue_push("File could not be loaded.\n", 1, 100, true);
   return false;
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
static bool menu_content_find_first_core(menu_content_ctx_defer_info_t *def_info)
{
   char new_core_path[PATH_MAX_LENGTH]     = {0};
   const core_info_t *info                 = NULL;
   core_info_list_t *core_info             = NULL;
   const char *default_info_dir            = NULL;
   size_t supported                        = 0;
   uint32_t menu_label_hash                = 0;

   if (def_info)
   {
      menu_label_hash   = menu_hash_calculate(def_info->menu_label);
      core_info         = (core_info_list_t*)def_info->data;
      default_info_dir  = def_info->dir;
   }

   if (!string_is_empty(default_info_dir))
   {
      const char *default_info_path = def_info->path;
      size_t default_info_length    = def_info->len;

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
   if (menu_label_hash == MENU_LABEL_LOAD_CONTENT)
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
      strlcpy(new_core_path, info->path, sizeof(new_core_path));

   runloop_ctl(RUNLOOP_CTL_SET_CONTENT_PATH, def_info->s);

   if (path_file_exists(new_core_path))
      runloop_ctl(RUNLOOP_CTL_SET_LIBRETRO_PATH, new_core_path);

   return true;
}

bool menu_content_ctl(enum menu_content_ctl_state state, void *data)
{
   switch (state)
   {
      case MENU_CONTENT_CTL_FIND_FIRST_CORE:
         return menu_content_find_first_core((menu_content_ctx_defer_info_t*)data);
      case MENU_CONTENT_CTL_LOAD_PLAYLIST:
         return menu_content_load_from_playlist((menu_content_ctx_playlist_info_t*)data);
      case MENU_CONTENT_CTL_NONE:
      default:
         break;
   }

   return true;
}
