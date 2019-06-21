/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2016-2017 - Jean-André Santoni
 *  Copyright (C) 2016-2019 - Andrés Suárez
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

#include <lists/file_list.h>
#include <string/stdstring.h>
#include "../paths.h"

#include "task_file_transfer.h"
#include "tasks_internal.h"

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include "../verbosity.h"
#include "../network/netplay/netplay_discovery.h"

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
   task_set_finished(task, true);

   return;
}

bool task_push_netplay_lan_scan(retro_task_callback_t cb)
{
   retro_task_t *task = task_init();

   if (!task)
      return false;

   task->type     = TASK_TYPE_BLOCKING;
   task->handler  = task_netplay_lan_scan_handler;
   task->callback = cb;
   task->title    = strdup(msg_hash_to_str(MSG_NETPLAY_LAN_SCANNING));

   task_queue_push(task);

   return true;
}

bool task_push_netplay_lan_scan_rooms(retro_task_callback_t cb)
{
   retro_task_t *task = task_init();

   if (!task)
      return false;

   task->type     = TASK_TYPE_BLOCKING;
   task->handler  = task_netplay_lan_scan_handler;
   task->callback = cb;
   task->title    = strdup(msg_hash_to_str(MSG_NETPLAY_LAN_SCANNING));

   task_queue_push(task);

   return true;
}
