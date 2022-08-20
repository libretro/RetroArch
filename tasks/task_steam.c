/*  RetroArch - A frontend for libretro.
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

#include "tasks_internal.h"
#include "../menu/menu_entries.h"
#include "../runloop.h"
#include "../steam/steam.h"

typedef struct steam_core_dlc_install_state
{
   AppId app_id;
   char *name;
   bool has_downloaded;
} steam_core_dlc_install_state_t;

static void task_steam_core_dlc_install_handler(retro_task_t *task)
{
   int8_t progress;
   steam_core_dlc_install_state_t *state = NULL;
   MistResult result                     = MistResult_Success;
   bool downloading                      = false;
   uint64_t bytes_downloaded             = 0;
   uint64_t bytes_total                  = 0;

   if (!task)
      goto task_finished;

   if (!(state = (steam_core_dlc_install_state_t*)task->state))
      goto task_finished;

   if (task_get_cancelled(task))
      goto task_finished;

   result = mist_steam_apps_get_dlc_download_progress(state->app_id, &downloading, &bytes_downloaded, &bytes_total);
   if (MIST_IS_ERROR(result))
      goto task_finished;
   if (!downloading)
   {
      if (state->has_downloaded)
         goto task_finished;
   }
   else
      state->has_downloaded = true;

   /* Min bytes total to avoid division by zero at start */
   if (bytes_total < 1)
      bytes_total = 1;

   progress = (int8_t)((bytes_downloaded * 100) / bytes_total);
   if (progress < 0)
      progress = 0;
   else if (progress > 100)
      progress = 100;

   task_set_progress(task, progress);

   return;
task_finished:
   if (task)
      task_set_finished(task, true);

   /* If finished successfully */
   if (MIST_IS_SUCCESS(result))
   {
      char msg[PATH_MAX_LENGTH];
      strlcpy(msg, msg_hash_to_str(MSG_CORE_INSTALLED),
            sizeof(msg));
      strlcat(msg, state->name,
            sizeof(msg));

      runloop_msg_queue_push(msg, 1, 180, true, NULL,
         MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_ERROR);
   }

   /* Cleanup */
   if (state)
   {
      free(state->name);
      free(state);
   }
}

void task_push_steam_core_dlc_install(
      AppId app_id,
      const char *name)
{
   char task_title[PATH_MAX_LENGTH];

   retro_task_t                  *task   = task_init();
   steam_core_dlc_install_state_t* state = (steam_core_dlc_install_state_t*)calloc(1,
         sizeof(steam_core_dlc_install_state_t));

   state->app_id         = app_id;
   state->name           = strdup(name);
   state->has_downloaded = false;

   strlcpy(task_title, msg_hash_to_str(MSG_CORE_STEAM_INSTALLING),
         sizeof(task_title));
   strlcat(task_title, name, sizeof(task_title));

   task->handler  = task_steam_core_dlc_install_handler;
   task->state    = state;
   task->mute     = false;
   task->title    = strdup(task_title);
   task->progress = 0;
   task->callback = NULL;

   task_queue_push(task);
}
