/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2016-2017 - Jean-André Santoni
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

#include "tasks_internal.h"
#include "../verbosity.h"
#include "../wifi/wifi_driver.h"
#include "../menu/menu_entries.h"
#include "../menu/menu_driver.h"

typedef struct
{
   struct string_list *ssid_list;
} wifi_handle_t;

static void wifi_scan_callback(void *task_data,
                               void *user_data, const char *error)
{
   unsigned i;
   unsigned menu_type            = 0;
   const char *path              = NULL;
   const char *label             = NULL;
   enum msg_hash_enums enum_idx  = MSG_UNKNOWN;
   file_list_t *file_list        = NULL;
   struct string_list *ssid_list = NULL;

   menu_entries_get_last_stack(&path, &label, &menu_type, &enum_idx, NULL);

   /* Don't push the results if we left the wifi menu */
   if (!string_is_equal(label,
         msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_WIFI_SETTINGS_LIST)))
      return;

   file_list = menu_entries_get_selection_buf_ptr(0);
   ssid_list = string_list_new();

   menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, file_list);

   driver_wifi_get_ssids(ssid_list);

   for (i = 0; i < ssid_list->size; i++)
   {
      const char *ssid = ssid_list->elems[i].data;
      menu_entries_append_enum(file_list,
            ssid,
            msg_hash_to_str(MENU_ENUM_LABEL_CONNECT_WIFI),
            MENU_ENUM_LABEL_CONNECT_WIFI,
            MENU_WIFI, 0, 0);
   }

   string_list_free(ssid_list);
}

static void task_wifi_scan_handler(retro_task_t *task)
{
   driver_wifi_scan();

   task_set_progress(task, 100);
   task_set_title(task, strdup(msg_hash_to_str(MSG_WIFI_SCAN_COMPLETE)));
   task_set_finished(task, true);

   return;
}

bool task_push_wifi_scan(void)
{
   retro_task_t   *task = (retro_task_t *)calloc(1, sizeof(*task));
   wifi_handle_t *state = (wifi_handle_t*)calloc(1, sizeof(*state));

   if (!task || !state)
      goto error;

   state->ssid_list     = string_list_new();

   /* blocking means no other task can run while this one is running, 
    * which is the default */
   task->type           = TASK_TYPE_BLOCKING;
   task->state          = state;
   task->handler        = task_wifi_scan_handler;
   task->callback       = wifi_scan_callback;
   task->title          = strdup(msg_hash_to_str(
                           MSG_SCANNING_WIRELESS_NETWORKS));

   task_queue_ctl(TASK_QUEUE_CTL_PUSH, task);

   return true;

error:
   if (state)
      free(state);
   if (task)
      free(task);

   return false;
}
