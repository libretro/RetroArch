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
#include <compat/strl.h>
#include <retro_assert.h>
#include <retro_miscellaneous.h>
#include <string/stdstring.h>

#include "tasks_internal.h"

#include "../msg_hash.h"
#include "../network/wifi_driver.h"
#include "../verbosity.h"

#define FUNC_PUSH_TASK(funcname, handlerfunc, message) \
   bool funcname(retro_task_callback_t cb) \
   { \
      retro_task_t   *task = task_init(); \
      if (!task) \
         return false; \
      task->type           = TASK_TYPE_BLOCKING; \
      task->state          = NULL; \
      task->handler        = handlerfunc; \
      task->callback       = cb; \
      task->title          = strdup(msg_hash_to_str( \
                              message)); \
      task_queue_push(task); \
      return true; \
   }

static void task_wifi_scan_handler(retro_task_t *task)
{
   if (!task)
      return;

   driver_wifi_scan();

   task_set_progress(task, 100);
   task_free_title(task);
   task_set_title(task, strdup(msg_hash_to_str(MSG_WIFI_SCAN_COMPLETE)));
   task_set_finished(task, true);
}

static void task_wifi_enable_handler(retro_task_t *task)
{
   if (!task)
      return;

   driver_wifi_enable(true);

   task_set_progress(task, 100);
   task_set_finished(task, true);
}

static void task_wifi_disable_handler(retro_task_t *task)
{
   if (!task)
      return;

   driver_wifi_enable(false);

   task_set_progress(task, 100);
   task_set_finished(task, true);
}

static void task_wifi_disconnect_handler(retro_task_t *task)
{
   wifi_network_info_t netinfo;
   if (!task)
      return;

   if (driver_wifi_connection_info(&netinfo))
     driver_wifi_disconnect_ssid(&netinfo);

   task_set_progress(task, 100);
   task_set_finished(task, true);
}

static void task_wifi_connect_handler(retro_task_t *task)
{
   if (!task)
      return;

   driver_wifi_connect_ssid((const wifi_network_info_t*)task->user_data);
   free(task->user_data);

   task_set_progress(task, 100);
   task_set_finished(task, true);
}

bool task_push_wifi_connect(retro_task_callback_t cb, void *netptr) {
   char msg[128];
   retro_task_t           *task = task_init();
   wifi_network_info_t *netinfo = (wifi_network_info_t*)netptr;
   if (!task)
      return false;
      
   snprintf(msg, sizeof(msg), msg_hash_to_str(MSG_WIFI_CONNECTING_TO), netinfo->ssid);

   task->type           = TASK_TYPE_BLOCKING;
   task->state          = NULL;
   task->handler        = task_wifi_connect_handler;
   task->callback       = cb;
   task->title          = strdup(msg);
   task->user_data      = malloc(sizeof(*netinfo));
   memcpy(task->user_data, netinfo, sizeof(*netinfo));
   task_queue_push(task);
   return true;
}

FUNC_PUSH_TASK(task_push_wifi_scan,       task_wifi_scan_handler,       MSG_SCANNING_WIRELESS_NETWORKS)
FUNC_PUSH_TASK(task_push_wifi_enable,     task_wifi_enable_handler,     MSG_ENABLING_WIRELESS)
FUNC_PUSH_TASK(task_push_wifi_disable,    task_wifi_disable_handler,    MSG_DISABLING_WIRELESS)
FUNC_PUSH_TASK(task_push_wifi_disconnect, task_wifi_disconnect_handler, MSG_DISCONNECTING_WIRELESS)


