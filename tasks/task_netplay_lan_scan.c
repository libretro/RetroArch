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

#include <string/stdstring.h>

#include "tasks_internal.h"
#include "../verbosity.h"
#include "../network/netplay/netplay_discovery.h"
#include "../menu/menu_entries.h"
#include "../menu/menu_driver.h"

static void netplay_lan_scan_callback(void *task_data,
                               void *user_data, const char *error)
{
   unsigned i;
   unsigned menu_type                      = 0;
   const char *path                        = NULL;
   const char *label                       = NULL;
   enum msg_hash_enums enum_idx            = MSG_UNKNOWN;
   file_list_t *file_list                  = NULL;
   struct netplay_host_list *netplay_hosts = NULL;

   menu_entries_get_last_stack(&path, &label, &menu_type, &enum_idx, NULL);

   /* Don't push the results if we left the LAN scan menu */
   if (!string_is_equal(label,
         msg_hash_to_str(
            MENU_ENUM_LABEL_DEFERRED_NETPLAY_LAN_SCAN_SETTINGS_LIST)))
      return;

   if (netplay_discovery_driver_ctl(
            RARCH_NETPLAY_DISCOVERY_CTL_LAN_GET_RESPONSES,
            (void *) &netplay_hosts))
   {
      if (netplay_hosts->size > 0)
      {
         file_list = menu_entries_get_selection_buf_ptr(0);
         menu_entries_ctl(MENU_ENTRIES_CTL_CLEAR, file_list);
      }

      for (i = 0; i < netplay_hosts->size; i++)
      {
         struct netplay_host *host = &netplay_hosts->hosts[i];
         menu_entries_append_enum(file_list,
               host->nick,
               msg_hash_to_str(MENU_ENUM_LABEL_NETPLAY_CONNECT_TO),
               MENU_ENUM_LABEL_NETPLAY_CONNECT_TO,
               MENU_NETPLAY_LAN_SCAN, 0, 0);
      }
   }
}

static void task_netplay_lan_scan_handler(retro_task_t *task)
{
   if (init_netplay_discovery())
   {
      netplay_discovery_driver_ctl(
            RARCH_NETPLAY_DISCOVERY_CTL_LAN_CLEAR_RESPONSES, NULL);
      netplay_discovery_driver_ctl(
            RARCH_NETPLAY_DISCOVERY_CTL_LAN_SEND_QUERY, NULL);
   }

   task_set_progress(task, 100);
   task_set_title(task, strdup(msg_hash_to_str(MSG_NETPLAY_LAN_SCANNING)));
   task_set_finished(task, true);

   return;
}

bool task_push_netplay_lan_scan(void)
{
   retro_task_t *task = (retro_task_t*)calloc(1, sizeof(*task));

   if (!task)
      return false;

   task->type     = TASK_TYPE_BLOCKING;
   task->handler  = task_netplay_lan_scan_handler;
   task->callback = netplay_lan_scan_callback;
   task->title    = strdup(msg_hash_to_str(MSG_NETPLAY_LAN_SCANNING));

   task_queue_ctl(TASK_QUEUE_CTL_PUSH, task);

   return true;
}
