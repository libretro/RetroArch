/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2017 - Jean-André Santoni
 *  Copyright (C) 2017 - Andrés Suárez
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

#include "tasks_internal.h"
#include "../file_path_special.h"
#include "../verbosity.h"
#include "../configuration.h"
#include "../playlist.h"
#include "../command.h"
#include "../core_info.h"
#include "../../runloop.h"

typedef struct
{
   struct string_list *lpl_list;
   char content_crc[PATH_MAX_LENGTH];
   char content_path[PATH_MAX_LENGTH];
   char hostname[512];
   char core_name[PATH_MAX_LENGTH];
   char core_path[PATH_MAX_LENGTH];
   char core_extensions[PATH_MAX_LENGTH];
   bool found;
} netplay_crc_handle_t;

static void netplay_crc_scan_callback(void *task_data,
                               void *user_data, const char *error)
{
   netplay_crc_handle_t *state     = (netplay_crc_handle_t*)task_data;
   content_ctx_info_t content_info = {0};

   if (!state)
      return;

   fflush(stdout);
   if (!string_is_empty(state->core_path) && !string_is_empty(state->content_path) &&
       !string_is_equal(state->content_path, "N/A"))
   {
      command_event(CMD_EVENT_NETPLAY_INIT_DIRECT_DEFERRED, state->hostname);
      task_push_load_content_with_new_core_from_menu(
            state->core_path, state->content_path,
            &content_info,
            CORE_TYPE_PLAIN,
            NULL, NULL);
   }
   else if (!string_is_empty(state->core_path) && !string_is_empty(state->content_path) &&
      string_is_equal(state->content_path, "N/A"))
   {
      content_ctx_info_t content_info = {0};

      command_event(CMD_EVENT_NETPLAY_INIT_DIRECT_DEFERRED, state->hostname);
      task_push_load_new_core(state->core_path, NULL,
            &content_info, CORE_TYPE_PLAIN, NULL, NULL);
      task_push_start_current_core(&content_info);
   }
   else
   {
      /* TO-DO: Inform the user no compatible core or content was found */
      RARCH_LOG("Couldn't find a suitable %s\n", 
         string_is_empty(state->content_path) ? "content file" : "core");
      runloop_msg_queue_push(
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NETPLAY_LOAD_CONTENT_MANUALLY),
            1, 480, true);
   }


   free(state);
}

static void task_netplay_crc_scan_handler(retro_task_t *task)
{
   size_t i, j;
   netplay_crc_handle_t *state = (netplay_crc_handle_t*)task->state;

   task_set_progress(task, 0);
   task_set_title(task, strdup("Looking for compatible content..."));
   task_set_finished(task, false);

   if (!state->lpl_list)
   {
      task_set_progress(task, 100);
      task_set_title(task, strdup("Playlist directory not found"));
      task_set_finished(task, true);
      free(state);
      return;
   }

   if (state->lpl_list->size == 0 && 
      !string_is_equal(state->content_path, "N/A"))
      goto no_playlists;

   /* Lobby reports content CRC, try to use CRC matching
      content with no CRC uses 00000000 */
   if (!string_is_equal(state->content_crc, "00000000|crc"))
   {

      RARCH_LOG("Using CRC matching\n");

      for (i = 0; i < state->lpl_list->size; i++)
      {
         playlist_t *playlist = NULL;
         const char *lpl_path = state->lpl_list->elems[i].data;

         if (!strstr(lpl_path, file_path_str(FILE_PATH_LPL_EXTENSION)))
            continue;

         playlist = playlist_init(lpl_path, 99999);

         for (j = 0; j < playlist->size; j++)
         {
            if (string_is_equal(playlist->entries[j].crc32, state->content_crc) && 
               strstr(state->core_extensions, path_get_extension(playlist->entries[j].path)))
            {
               RARCH_LOG("CRC Match %s\n", playlist->entries[j].crc32);
               strlcpy(state->content_path, playlist->entries[j].path, sizeof(state->content_path));
               state->found = true;
               task_set_data(task, state);
               task_set_progress(task, 100);
               task_set_title(task, strdup(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NETPLAY_COMPAT_CONTENT_FOUND)));
               task_set_finished(task, true);
               string_list_free(state->lpl_list);
               free(playlist);
               return;
            }

            task_set_progress(task, (int)(j/playlist->size*100.0));
         }

         free(playlist);
      }
   }
   /* Lobby reports core doesn't need content */
   else if(string_is_equal(state->content_path, "N/A"))
   {
      state->found = true;
      task_set_data(task, state);
      task_set_progress(task, 100);
      task_set_title(task, strdup(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NETPLAY_COMPAT_CONTENT_FOUND)));
      task_set_finished(task, true);
      return;
   }
   /* Lobby reports that the core needs content but
      the CRC wasn't reported */
   else
   {
      RARCH_LOG("Using filename matching\n");
      for (i = 0; i < state->lpl_list->size; i++)
      {
         playlist_t *playlist = NULL;
         const char *lpl_path = state->lpl_list->elems[i].data;

         if (!strstr(lpl_path, file_path_str(FILE_PATH_LPL_EXTENSION)))
            continue;

         playlist = playlist_init(lpl_path, 99999);

         for (j = 0; j < playlist->size; j++)
         {
            char entry[PATH_MAX_LENGTH];
            const char* buf = path_basename(playlist->entries[j].path);

            entry[0]    = '\0';

            strlcpy(entry, buf, sizeof(entry));

            path_remove_extension(entry);

            if ( !string_is_empty(entry) && 
                  string_is_equal(entry, state->content_path) &&
                  strstr(state->core_extensions, path_get_extension(playlist->entries[j].path)))
            {
               RARCH_LOG("Filename match %s\n", playlist->entries[j].path);

               strlcpy(state->content_path, playlist->entries[j].path, sizeof(state->content_path));
               state->found = true;
               task_set_data(task, state);
               task_set_progress(task, 100);
               task_set_title(task, strdup(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NETPLAY_COMPAT_CONTENT_FOUND)));
               task_set_finished(task, true);
               string_list_free(state->lpl_list);
               free(playlist);
               return;
            }

            task_set_progress(task, (int)(j/playlist->size*100.0));
         }

         free(playlist);
      }
   }

no_playlists:
   string_list_free(state->lpl_list);
   task_set_progress(task, 100);
   task_set_title(task, strdup("Content not found, try manual load or disconnect from host"));
   task_set_finished(task, true);
   command_event(CMD_EVENT_NETPLAY_INIT_DIRECT_DEFERRED, state->hostname);
   free(state);
   return;
}

bool task_push_netplay_crc_scan(uint32_t crc, char* name,
      const char *hostname, const char *core_name)
{
   unsigned i;
   core_info_list_t *info      = NULL;
   settings_t        *settings = config_get_ptr();
   retro_task_t          *task = (retro_task_t *)calloc(1, sizeof(*task));
   netplay_crc_handle_t *state = (netplay_crc_handle_t*)calloc(1, sizeof(*state));

   core_info_get_list(&info);
   
   if (!task || !state)
      goto error;

   state->content_crc[0]  = '\0';
   state->content_path[0] = '\0';
   state->hostname[0]     = '\0';
   state->core_name[0]    = '\0';

   snprintf(state->content_crc, sizeof(state->content_crc), "%08X|crc", crc);

   strlcpy(state->content_path,  name,       sizeof(state->content_path));
   strlcpy(state->hostname,      hostname,   sizeof(state->hostname));
   strlcpy(state->core_name,     core_name,  sizeof(state->core_name));

   state->lpl_list = dir_list_new(settings->directory.playlist,
         NULL, true, true, true, false);

   state->found = false;

   for (i=0; i < info->count; i++)
   {
      /* check if the core name matches.
         TO-DO :we could try to load the core too to check 
         if the version string matches too */
#if 0
      printf("Info: %s State: %s", info->list[i].core_name, state->core_name);
#endif
      if(string_is_equal(info->list[i].core_name, state->core_name))
      {
         strlcpy(state->core_path, info->list[i].path, sizeof(state->core_path));

         if (!string_is_equal(state->content_path, "N/A") && 
            !string_is_empty(info->list[i].supported_extensions))
         {
            strlcpy(state->core_extensions,
                  info->list[i].supported_extensions, sizeof(state->core_extensions));
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

   task_queue_ctl(TASK_QUEUE_CTL_PUSH, task);

   return true;

error:
   if (state)
      free(state);
   if (task)
      free(task);

   return false;
}
