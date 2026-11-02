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

#include <stdio.h>
#include <string.h>
#include <compat/strl.h>
#include <retro_miscellaneous.h>

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
      task->type           = TASK_TYPE_NONE; \
      task->handler        = handlerfunc; \
      task->title          = strdup(msg_hash_to_str( \
                              message)); \
      if (task_wifi_push(task, cb)) \
         return true; \
      task_free_title(task); \
      free(task); \
      return false; \
   }

static void task_wifi_scan_handler(retro_task_t *task)
{
   if (!task)
      return;

   driver_wifi_scan();

   task_set_progress(task, 100);
   task_free_title(task);
   task_set_title(task, strdup(msg_hash_to_str(MSG_WIFI_SCAN_COMPLETE)));
   task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
}

static void task_wifi_enable_handler(retro_task_t *task)
{
   if (!task)
      return;

   driver_wifi_enable(true);

   task_set_progress(task, 100);
   task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
}

static void task_wifi_disable_handler(retro_task_t *task)
{
   if (!task)
      return;

   driver_wifi_enable(false);

   task_set_progress(task, 100);
   task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
}

static void task_wifi_disconnect_handler(retro_task_t *task)
{
   wifi_network_info_t netinfo;
   if (!task)
      return;

   if (driver_wifi_connection_info(&netinfo))
     driver_wifi_disconnect_ssid(&netinfo);

   task_set_progress(task, 100);
   task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
}

static void task_wifi_connect_handler(retro_task_t *task)
{
   if (!task)
      return;

   driver_wifi_connect_ssid((const wifi_network_info_t*)task->user_data);
   free(task->user_data);

   task_set_progress(task, 100);
   task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
}

/* One Wi-Fi task at a time, the driver being used by one at a time,
 * and among themselves only. The rest are held, in order, and pushed
 * as the one ahead of them finishes. Turned away, Connect, Disconnect
 * and the Wi-Fi Enabled toggle did nothing while the networks list's
 * periodic scan ran - the toggle leaving the setting and the driver
 * apart. A scan behind a scan is refused: it would find the same.
 * Frame thread only: the menu pushes them and their callbacks run
 * there. Bounded: past it a task is refused, as all were. */
#define WIFI_TASKS_HELD_MAX 4
static retro_task_t *wifi_tasks_held[WIFI_TASKS_HELD_MAX];
static unsigned      wifi_tasks_held_count = 0;
/* the handler of the Wi-Fi task in flight, or NULL */
static retro_task_handler_t wifi_task_live = NULL;

/* the caller's callback, run from task_wifi_done() */
typedef struct
{
   retro_task_callback_t cb;
} task_wifi_state_t;

static void task_wifi_done(retro_task_t *task, void *task_data,
      void *user_data, const char *error)
{
   task_wifi_state_t *st = (task_wifi_state_t*)task->state;
   task->state           = NULL;
   wifi_task_live        = NULL;
   if (st)
   {
      if (st->cb)
         st->cb(task, task_data, user_data, error);
      free(st);
   }
   if (wifi_tasks_held_count)
   {
      unsigned i;
      retro_task_t *next = wifi_tasks_held[0];
      for (i = 1; i < wifi_tasks_held_count; i++)
         wifi_tasks_held[i - 1] = wifi_tasks_held[i];
      wifi_tasks_held[--wifi_tasks_held_count] = NULL;
      wifi_task_live = next->handler;
      task_queue_push(next);
   }
}

/* Pushes @task, or holds it behind the Wi-Fi task in flight; false
 * when the hold is full, or for a scan already running or held, with
 * @task left for the caller to free. */
static bool task_wifi_push(retro_task_t *task, retro_task_callback_t cb)
{
   task_wifi_state_t *st;
   unsigned i;

   if (task->handler == task_wifi_scan_handler)
   {
      if (wifi_task_live == task_wifi_scan_handler)
         return false;
      for (i = 0; i < wifi_tasks_held_count; i++)
         if (wifi_tasks_held[i]->handler == task_wifi_scan_handler)
            return false;
   }
   if (     (wifi_task_live || wifi_tasks_held_count)
         && wifi_tasks_held_count >= WIFI_TASKS_HELD_MAX)
      return false;
   if (!(st = (task_wifi_state_t*)malloc(sizeof(*st))))
      return false;
   st->cb         = cb;
   task->state    = st;
   task->callback = task_wifi_done;

   if (wifi_task_live || wifi_tasks_held_count)
   {
      wifi_tasks_held[wifi_tasks_held_count++] = task;
      return true;
   }
   wifi_task_live = task->handler;
   task_queue_push(task);
   return true;
}

bool task_push_wifi_connect(retro_task_callback_t cb, void *netptr)
{
   char msg[128];
   retro_task_t           *task = task_init();
   wifi_network_info_t *netinfo = (wifi_network_info_t*)netptr;
   if (!task)
      return false;

   snprintf(msg, sizeof(msg), msg_hash_to_str(MSG_WIFI_CONNECTING_TO), netinfo->ssid);

   task->type           = TASK_TYPE_NONE;
   task->handler        = task_wifi_connect_handler;
   task->title          = strdup(msg);
   task->user_data      = malloc(sizeof(*netinfo));
   /* NULL-check: the memcpy on the next line NULL-derefs on OOM.
    * Free the task_init-allocated task and fail cleanly; caller
    * (menu wifi settings) already handles the false return by
    * leaving the UI in its pre-connect state. */
   if (!task->user_data)
   {
      if (task->title)
         free(task->title);
      free(task);
      return false;
   }
   memcpy(task->user_data, netinfo, sizeof(*netinfo));
   if (task_wifi_push(task, cb))
      return true;

   free(task->user_data);
   task_free_title(task);
   free(task);
   return false;
}

FUNC_PUSH_TASK(task_push_wifi_scan,       task_wifi_scan_handler,       MSG_SCANNING_WIRELESS_NETWORKS)
FUNC_PUSH_TASK(task_push_wifi_enable,     task_wifi_enable_handler,     MSG_ENABLING_WIRELESS)
FUNC_PUSH_TASK(task_push_wifi_disable,    task_wifi_disable_handler,    MSG_DISABLING_WIRELESS)
FUNC_PUSH_TASK(task_push_wifi_disconnect, task_wifi_disconnect_handler, MSG_DISCONNECTING_WIRELESS)


