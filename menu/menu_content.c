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
#include <retro_file.h>
#include <file/file_path.h>
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

static void menu_content_environment_get(int *argc, char *argv[],
      void *args, void *params_data)
{
   struct rarch_main_wrap *wrap_args = (struct rarch_main_wrap*)params_data;
   char *fullpath                    = NULL;
   global_t *global                  = global_get_ptr();
   settings_t *settings              = config_get_ptr();
    
   if (!wrap_args)
      return;

   runloop_ctl(RUNLOOP_CTL_GET_CONTENT_PATH, &fullpath);

   wrap_args->no_content       = menu_driver_ctl(
         RARCH_MENU_CTL_HAS_LOAD_NO_CONTENT, NULL);

   if (!global->has_set.verbosity)
      wrap_args->verbose       = *retro_main_verbosity();

   wrap_args->touched          = true;
   wrap_args->config_path      = NULL;
   wrap_args->sram_path        = NULL;
   wrap_args->state_path       = NULL;
   wrap_args->content_path     = NULL;

   if (*global->path.config)
      wrap_args->config_path   = global->path.config;
   if (*global->dir.savefile)
      wrap_args->sram_path     = global->dir.savefile;
   if (*global->dir.savestate)
      wrap_args->state_path    = global->dir.savestate;
   if (*fullpath)
      wrap_args->content_path  = fullpath;
   if (!global->has_set.libretro)
      wrap_args->libretro_path = *settings->libretro 
         ? settings->libretro : NULL;

}

/**
 * menu_content_load:
 *
 * Loads content into currently selected core.
 * Will also optionally push the content entry to the history playlist.
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/

static bool menu_content_load(void)
{
   char name[PATH_MAX_LENGTH];
   char msg[PATH_MAX_LENGTH];
   bool msg_force       = true;
   char *fullpath       = NULL;

   runloop_ctl(RUNLOOP_CTL_GET_CONTENT_PATH, &fullpath);
   /* redraw menu frame */
   menu_display_ctl(MENU_DISPLAY_CTL_SET_MSG_FORCE, &msg_force);
   menu_driver_ctl(RARCH_MENU_CTL_RENDER, NULL);

   if (*fullpath)
      fill_pathname_base(name, fullpath, sizeof(name));

   if (!(content_load(0, NULL, NULL, menu_content_environment_get)))
      goto error;

   if (*fullpath)
   {
      snprintf(msg, sizeof(msg), "INFO - Loading %s ...", name);
      runloop_msg_queue_push(msg, 1, 1, false);
   }

   if (*fullpath || 
         menu_driver_ctl(RARCH_MENU_CTL_HAS_LOAD_NO_CONTENT, NULL))
   {
      struct retro_system_info *info = NULL;
      menu_driver_ctl(RARCH_MENU_CTL_SYSTEM_INFO_GET,
            &info);
      content_push_to_history_playlist(true, fullpath, info);
      content_playlist_write_file(g_defaults.history);
   }

   return true;

error:
   snprintf(msg, sizeof(msg), "Failed to load %s.\n", name);
   runloop_msg_queue_push(msg, 1, 90, false);
   return false;
}

/**
 * menu_content_load_from_playlist:
 * @playlist             : Playlist handle.
 * @idx                  : Index in playlist.
 *
 * Initializes core and loads content based on playlist entry.
 **/
static bool menu_content_load_from_playlist(void *data)
{
   unsigned idx;
   const char *core_path        = NULL;
   const char *path             = NULL;
   menu_content_ctx_playlist_info_t *info = 
      (menu_content_ctx_playlist_info_t *)data;
   content_playlist_t *playlist = NULL;
   
   if (!info)
      return false;

   playlist = (content_playlist_t*)info->data;
   idx      = info->idx;

   if (!playlist)
      return false;

   content_playlist_get_index(playlist,
         idx, &path, NULL, &core_path, NULL, NULL, NULL);

   if (path && !string_is_empty(path))
   {
      unsigned i;
      RFILE *fp           = NULL;
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

      free(path_tolower);

      fp = retro_fopen(path_check, RFILE_MODE_READ, -1);

      free(path_check);

      if (!fp)
         goto error;

      retro_fclose(fp);
   }

   runloop_ctl(RUNLOOP_CTL_SET_LIBRETRO_PATH, (void*)core_path);

   if (path)
      menu_driver_ctl(RARCH_MENU_CTL_UNSET_LOAD_NO_CONTENT, NULL);
   else
      menu_driver_ctl(RARCH_MENU_CTL_SET_LOAD_NO_CONTENT, NULL);

   if (!event_cmd_ctl(EVENT_CMD_EXEC, (void*)path))
      return false;

   event_cmd_ctl(EVENT_CMD_LOAD_CORE, NULL);

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
static bool menu_content_find_first_core(void *data)
{
   char new_core_path[PATH_MAX_LENGTH];
   const core_info_t *info                 = NULL;
   size_t supported                        = 0;
   menu_content_ctx_defer_info_t *def_info = 
      (menu_content_ctx_defer_info_t *)data;
   core_info_list_t *core_info             = 
      (core_info_list_t*)def_info->data;
   uint32_t menu_label_hash                = 
      menu_hash_calculate(def_info->menu_label);

   if (     !string_is_empty(def_info->dir) 
         && !string_is_empty(def_info->path))
      fill_pathname_join(def_info->s, 
            def_info->dir, def_info->path, def_info->len);

#ifdef HAVE_COMPRESSION
   if (path_is_compressed_file(def_info->dir))
   {
      /* In case of a compressed archive, we have to join with a hash */
      /* We are going to write at the position of dir: */
      retro_assert(strlen(def_info->dir) < strlen(def_info->s));
      def_info->s[strlen(def_info->dir)] = '#';
   }
#endif

   if (core_info)
      core_info_list_get_supported_cores(core_info,
            def_info->s, &info,
            &supported);

   /* We started the menu with 'Load Content', we are 
    * going to use the current core to load this. */
   if (menu_label_hash == MENU_LABEL_LOAD_CONTENT)
   {
      core_info_ctl(CORE_INFO_CTL_CURRENT_CORE_GET, (void*)&info);
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
         return menu_content_find_first_core(data);
      case MENU_CONTENT_CTL_LOAD:
         return menu_content_load();
      case MENU_CONTENT_CTL_LOAD_PLAYLIST:
         return menu_content_load_from_playlist(data);
      case MENU_CONTENT_CTL_NONE:
      default:
         break;
   }

   return true;
}
