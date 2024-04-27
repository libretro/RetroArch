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

#include <features/features_cpu.h>

#include "tasks_internal.h"

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#ifdef HAVE_NETPLAYDISCOVERY

#include "../network/netplay/netplay.h"

struct netplay_lan_scan_data
{
   retro_time_t timeout;
   void (*cb)(const void*);
   bool query;
   bool busy;
};

static void task_netplay_lan_scan_handler(retro_task_t *task)
{
   struct netplay_lan_scan_data *data =
      (struct netplay_lan_scan_data*)task->task_data;

   if (data->query)
   {
      netplay_discovery_driver_ctl(
         RARCH_NETPLAY_DISCOVERY_CTL_LAN_CLEAR_RESPONSES, NULL);

      if (!init_netplay_discovery())
         goto finished;

      if (!netplay_discovery_driver_ctl(
            RARCH_NETPLAY_DISCOVERY_CTL_LAN_SEND_QUERY, NULL))
         goto finished;

      data->timeout += cpu_features_get_time_usec();
      data->query    = false;
   }
   else
   {
      if (!netplay_discovery_driver_ctl(
            RARCH_NETPLAY_DISCOVERY_CTL_LAN_GET_RESPONSES, NULL))
      {
         if (cpu_features_get_time_usec() >= data->timeout)
            goto finished;
      }
   }

   return;

finished:
   deinit_netplay_discovery();

   task_set_progress(task, 100);
   task_set_finished(task, true);
}

static void task_netplay_lan_scan_callback(retro_task_t *task,
      void *task_data, void *user_data, const char *error)
{
   struct netplay_lan_scan_data *data =
      (struct netplay_lan_scan_data*)task_data;
   net_driver_state_t *net_st         = networking_state_get_ptr();

   data->cb(&net_st->discovered_hosts);

   data->busy = false;
}

bool task_push_netplay_lan_scan(void (*cb)(const void*), unsigned timeout)
{
   static struct netplay_lan_scan_data data = {0};
   retro_task_t *task;

   /* Do not run more than one LAN scan task at a time. */
   if (data.busy)
      return false;

   task = task_init();
   if (!task)
      return false;

   data.busy    = true;
   data.query   = true;
   data.timeout = (retro_time_t)timeout * 1000;
   data.cb      = cb;

   task->handler   = task_netplay_lan_scan_handler;
   task->callback  = task_netplay_lan_scan_callback;
   task->task_data = &data;

   task_queue_push(task);

   return true;
}
#else
bool task_push_netplay_lan_scan(void (*cb)(const void*), unsigned timeout)
{
   return false;
}
#endif
