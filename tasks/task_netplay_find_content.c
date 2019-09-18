/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2017 - Jean-André Santoni
 *  Copyright (C) 2017-2019 - Andrés Suárez
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

#include <string.h>
#include <errno.h>
#include <file/nbio.h>
#include <formats/image.h>
#include <compat/strl.h>
#include <retro_assert.h>
#include <retro_miscellaneous.h>
#include <lists/string_list.h>
#include <rhash.h>
#include <string/stdstring.h>
#include <file/file_path.h>
#include <lists/dir_list.h>
#include <queues/task_queue.h>

#include "task_content.h"
#include "tasks_internal.h"
#include "../file_path_special.h"
#include "../verbosity.h"
#include "../configuration.h"
#include "../playlist.h"
#include "../command.h"
#include "../core_info.h"
#include "../../retroarch.h"
#include "../../menu/menu_driver.h"

typedef struct
{
   char content_crc[PATH_MAX_LENGTH];
   char content_path[PATH_MAX_LENGTH];
   char hostname[512];
   char subsystem_name[512];
   char core_name[PATH_MAX_LENGTH];
   char core_path[PATH_MAX_LENGTH];
   char core_extensions[PATH_MAX_LENGTH];
   bool found;
   bool current;
   bool contentless;
   struct string_list *lpl_list;
} netplay_crc_handle_t;

static void netplay_crc_scan_callback(retro_task_t *task,
      void *task_data,
      void *user_data, const char *error)
{
   netplay_crc_handle_t *state     = (netplay_crc_handle_t*)task_data;
   content_ctx_info_t content_info = {0};

   if (!state)
      return;

   fflush(stdout);

   if (!string_is_empty(state->subsystem_name) && !string_is_equal(state->subsystem_name, "N/A"))
   {
      content_ctx_info_t content_info  = {0};
      struct string_list *game_list = string_split(state->content_path, "|");
      unsigned i = 0;

      task_push_load_new_core(state->core_path, NULL,
            &content_info, CORE_TYPE_PLAIN, NULL, NULL);
      content_clear_subsystem();
      if (!content_set_subsystem_by_name(state->subsystem_name))
         RARCH_LOG("[Lobby] Subsystem not found in implementation\n");

      for (i = 0; i < game_list->size; i++)
         content_add_subsystem(game_list->elems[i].data);
      task_push_load_subsystem_with_core_from_menu(
         NULL, &content_info,
         CORE_TYPE_PLAIN, NULL, NULL);
      string_list_free(game_list);
      return;
   }

   /* regular core with content file */
   if (!string_is_empty(state->core_path) && !string_is_empty(state->content_path)
      && !state->contentless && !state->current)
   {
      struct retro_system_info *system = runloop_get_libretro_system_info();

      RARCH_LOG("[Lobby] Loading core %s with content file %s\n",
         state->core_path, state->content_path);

      command_event(CMD_EVENT_NETPLAY_INIT_DIRECT_DEFERRED, state->hostname);

      if (system && string_is_equal(system->library_name, state->core_name))
         task_push_load_content_with_core_from_menu(
               state->content_path, &content_info,
               CORE_TYPE_PLAIN, NULL, NULL);
      else
      {
         task_push_load_new_core(state->core_path, NULL,
               &content_info, CORE_TYPE_PLAIN, NULL, NULL);
         task_push_load_content_with_core_from_menu(
               state->content_path, &content_info,
               CORE_TYPE_PLAIN, NULL, NULL);
      }

   }
   else

   /* contentless core */
   if (!string_is_empty(state->core_path) && !string_is_empty(state->content_path)
      && state->contentless)
   {
      content_ctx_info_t content_info  = {0};
      struct retro_system_info *system = runloop_get_libretro_system_info();

      RARCH_LOG("[Lobby] Loading contentless core %s\n", state->core_path);

      command_event(CMD_EVENT_NETPLAY_INIT_DIRECT_DEFERRED, state->hostname);

      if (!string_is_equal(system->library_name, state->core_name))
         task_push_load_new_core(state->core_path, NULL,
               &content_info, CORE_TYPE_PLAIN, NULL, NULL);

      task_push_start_current_core(&content_info);
   }
   /* regular core with current content */
   else if (!string_is_empty(state->core_path) && !string_is_empty(state->content_path)
      && state->current)
   {
      RARCH_LOG("[Lobby] Loading core %s with current content\n", state->core_path);
      command_event(CMD_EVENT_NETPLAY_INIT_DIRECT, state->hostname);
      command_event(CMD_EVENT_RESUME, NULL);
   }
   /* no match found */
   else
   {
      RARCH_LOG("[Lobby] Couldn't find a suitable %s\n",
         string_is_empty(state->content_path) ? "content file" : "core");
      runloop_msg_queue_push(
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NETPLAY_LOAD_CONTENT_MANUALLY),
            1, 480, true,
            NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
   }

   free(state);
}

static void begin_task(retro_task_t *task, const char *title)
{
   task_set_progress(task, 0);
   task_free_title(task);
   task_set_title(task, strdup(title));
   task_set_finished(task, false);
}

static void finish_task(retro_task_t *task, const char *title)
{
   task_set_progress(task, 100);
   task_free_title(task);
   task_set_title(task, strdup(title));
   task_set_finished(task, true);
}

#define core_requires_content(state) string_is_not_equal(state->content_path, "N/A")

/**
 * Given a path to a content file, return the base name without the
 * path or the file extension.
 *
 * e.g. /home/user/foo.rom => foo
 */
static void get_entry(char *entry, int len, const char *path)
{
   const char *buf = path_basename(path);
   entry[0]        = '\0';

   strlcpy(entry, buf, len);
   path_remove_extension(entry);
}

/**
 * Execute a search for compatible content for netplay.
 * We prioritize a CRC match, if we have a CRC to match against.
 * If we don't have a CRC, or if there's no CRC match found, fall
 * back to a filename match and hope for the best.
 */
static void task_netplay_crc_scan_handler(retro_task_t *task)
{
   size_t i, j, k;
   char entry[PATH_MAX_LENGTH];
   bool have_crc               = false;
   netplay_crc_handle_t *state = (netplay_crc_handle_t*)task->state;

   begin_task(task, "Looking for compatible content...");

   /* start by checking cases that don't require a search */

   /* the core doesn't have any content to match, so fast-succeed */
   if (!core_requires_content(state))
   {
      state->found = true;
      state->contentless = true;
      task_set_data(task, state);
      finish_task(task, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NETPLAY_COMPAT_CONTENT_FOUND));
      return;
   }

   /* if this list is null, it means that RA failed to open the playlist directory */
   if (!state->lpl_list)
   {
      finish_task(task, "Playlist directory not found");
      free(state);
      return;
   }

   /* We opened the playlist directory, but there's nothing there. Nothing to do. */
   if (state->lpl_list->size == 0 && core_requires_content(state))
   {
      string_list_free(state->lpl_list);
      finish_task(task, "There are no playlists available; cannot execute search");
      command_event(CMD_EVENT_NETPLAY_INIT_DIRECT_DEFERRED, state->hostname);
      free(state);
      return;
   }

   have_crc = !string_is_equal(state->content_crc, "00000000|crc");

   /* if content is already loaded and the lobby gave us a CRC, check the loaded content first */
   if (have_crc && content_get_crc() > 0)
   {
      char current[PATH_MAX_LENGTH];

      RARCH_LOG("[Lobby] Testing CRC matching for: %s\n", state->content_crc);

      snprintf(current, sizeof(current), "%X|crc", content_get_crc());
      RARCH_LOG("[Lobby] Current content crc: %s\n", current);

      if (string_is_equal(current, state->content_crc))
      {
         RARCH_LOG("[Lobby] CRC match %s with currently loaded content\n", current);
         strlcpy(state->content_path, "N/A", sizeof(state->content_path));
         state->found = true;
         state->current = true;
         task_set_data(task, state);
         finish_task(task, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NETPLAY_COMPAT_CONTENT_FOUND));
         string_list_free(state->lpl_list);
         return;
      }
   }

   /* now let's do the search */
   if (string_is_empty(state->subsystem_name) || string_is_equal(state->subsystem_name, "N/A"))
   {
      for (i = 0; i < state->lpl_list->size; i++)
      {
         playlist_t *playlist   = NULL;
         unsigned playlist_size = 0;
         const char *lpl_path   = state->lpl_list->elems[i].data;

         /* skip files without .lpl file extension */
         if (!strstr(lpl_path, ".lpl"))
            continue;

         RARCH_LOG("[Lobby] Searching playlist: %s\n", lpl_path);
         playlist      = playlist_init(lpl_path, COLLECTION_SIZE);
         playlist_size = playlist_get_size(playlist);

         for (j = 0; j < playlist_size; j++)
         {
            const char *playlist_path     = NULL;
            const char *playlist_crc32    = NULL;
            const struct playlist_entry *playlist_entry = NULL;

            playlist_get_index(playlist, j, &playlist_entry);

            playlist_path = playlist_entry->path;
            playlist_crc32 = playlist_entry->crc32;

            if (have_crc && string_is_equal(playlist_crc32, state->content_crc))
            {
               RARCH_LOG("[Lobby] CRC match %s\n", playlist_crc32);
               strlcpy(state->content_path, playlist_path, sizeof(state->content_path));
               state->found = true;
               task_set_data(task, state);
               finish_task(task, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NETPLAY_COMPAT_CONTENT_FOUND));
               string_list_free(state->lpl_list);
               playlist_free(playlist);
               return;
            }

            get_entry(entry, sizeof(entry), playlist_path);

            /* See if the filename is a match. The response depends on whether or not we are doing a CRC
            * search.
            * Otherwise, on match we complete the task and mark it as successful immediately.
            */

            if (!string_is_empty(entry) &&
               string_is_equal(entry, state->content_path) &&
               strstr(state->core_extensions, path_get_extension(playlist_path)))
            {
               RARCH_LOG("[Lobby] Filename match %s\n", playlist_path);

               strlcpy(state->content_path, playlist_path, sizeof(state->content_path));
               state->found = true;
               task_set_data(task, state);
               finish_task(task, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NETPLAY_COMPAT_CONTENT_FOUND));
               string_list_free(state->lpl_list);
               playlist_free(playlist);
               return;
            }

            task_set_progress(task, (int)(j / playlist_size * 100.0));
         }

         playlist_free(playlist);
      }
   }
   else
   {
      bool found[100];
      struct string_list *game_list = string_split(state->content_path, "|");

      for (i = 0; i < game_list->size; i++)
      {
         found[i] = false;

         for (j = 0; j < state->lpl_list->size && !found[i]; j++)
         {
            playlist_t *playlist   = NULL;
            unsigned playlist_size = 0;
            const char *lpl_path   = state->lpl_list->elems[j].data;

            /* skip files without .lpl file extension */
            if (!strstr(lpl_path, ".lpl"))
               continue;

            RARCH_LOG("[Lobby] Searching content %d/%d (%s) in playlist: %s\n", i + 1, game_list->size, game_list->elems[i].data, lpl_path);
            playlist      = playlist_init(lpl_path, COLLECTION_SIZE);
            playlist_size = playlist_get_size(playlist);

            for (k = 0; k < playlist_size && !found[i]; k++)
            {
               const struct playlist_entry *playlist_entry = NULL;

               playlist_get_index(playlist, k, &playlist_entry);

               get_entry(entry, sizeof(entry), playlist_entry->path);

               if (!string_is_empty(entry) &&
                  strstr(game_list->elems[i].data, entry) &&
                  strstr(state->core_extensions, path_get_extension(playlist_entry->path)))
               {
                  RARCH_LOG("[Lobby] filename match %s\n", playlist_entry->path);

                  if (i == 0)
                  {
                     state->content_path[0] = '\0';
                     strlcpy(state->content_path, playlist_entry->path, sizeof(state->content_path));
                  }
                  else
                  {
                     strlcat(state->content_path, "|", sizeof(state->content_path));
                     strlcat(state->content_path, playlist_entry->path, sizeof(state->content_path));
                  }

                  found[i] = true;
               }

               task_set_progress(task, (int)(j / playlist_size * 100.0));
            }

            playlist_free(playlist);
         }
      }

      for (i = 0; i < game_list->size; i++)
      {
         state->found = true;
         if (!found[i])
         {
            state->found = false;
            break;
         }
      }

      if (state->found)
      {
         RARCH_LOG("[Lobby] Subsystem matching set found %s\n", state->content_path);
         task_set_data(task, state);
         finish_task(task, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NETPLAY_COMPAT_CONTENT_FOUND));
      }

      string_list_free(state->lpl_list);
      string_list_free(game_list);
      return;
   }

   /* end of the line. no matches at all. */
   string_list_free(state->lpl_list);
   finish_task(task, "Failed to locate matching content by either CRC or filename.");
   command_event(CMD_EVENT_NETPLAY_INIT_DIRECT_DEFERRED, state->hostname);
   free(state);
}

bool task_push_netplay_crc_scan(uint32_t crc, char* name,
      const char *hostname, const char *core_name, const char *subsystem)
{
   unsigned i;
   union string_list_elem_attr attr;
   struct string_list *lpl_list = NULL;
   core_info_list_t *info       = NULL;
   settings_t        *settings  = config_get_ptr();
   retro_task_t          *task  = task_init();
   netplay_crc_handle_t *state  = (netplay_crc_handle_t*)
      calloc(1, sizeof(*state));

   if (!task || !state)
      goto error;

   state->content_crc[0]    = '\0';
   state->content_path[0]   = '\0';
   state->hostname[0]       = '\0';
   state->core_name[0]      = '\0';
   state->subsystem_name[0] = '\0';
   attr.i = 0;

   snprintf(state->content_crc,
         sizeof(state->content_crc),
         "%08X|crc", crc);

   strlcpy(state->content_path,
         name, sizeof(state->content_path));
   strlcpy(state->hostname,
         hostname, sizeof(state->hostname));
   strlcpy(state->subsystem_name,
         subsystem, sizeof(state->subsystem_name));
   strlcpy(state->core_name,
         core_name, sizeof(state->core_name));

   lpl_list = dir_list_new(settings->paths.directory_playlist,
         NULL, true, true, true, false);

   if (!lpl_list)
      goto error;

   state->lpl_list = lpl_list;

   string_list_append(state->lpl_list,
         settings->paths.path_content_history, attr);
   state->found = false;

   core_info_get_list(&info);

   for (i = 0; i < info->count; i++)
   {
      /* check if the core name matches.
         TO-DO :we could try to load the core too to check
         if the version string matches too */
#if 0
      printf("Info: %s State: %s", info->list[i].core_name, state->core_name);
#endif
      if (string_is_equal(info->list[i].core_name, state->core_name))
      {
         strlcpy(state->core_path,
               info->list[i].path, sizeof(state->core_path));

         if (string_is_not_equal(state->content_path, "N/A") &&
            !string_is_empty(info->list[i].supported_extensions))
         {
            strlcpy(state->core_extensions,
                  info->list[i].supported_extensions,
                  sizeof(state->core_extensions));
         }
         break;
      }
   }

   /* blocking means no other task can run while this one is running,
    * which is the default */
   task->type           = TASK_TYPE_BLOCKING;
   task->state          = state;
   task->handler        = task_netplay_crc_scan_handler;
   task->callback       = netplay_crc_scan_callback;
   task->title          = strdup("Looking for matching content...");

   task_queue_push(task);

   return true;

error:
   if (state)
      free(state);
   if (task)
      free(task);

   return false;
}
