/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2017-2017 - Gregor Richards
 *  Copyright (C) 2021-2022 - Roberto V. Rampim
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

#include <stdlib.h>

#include <string/stdstring.h>

#include "tasks_internal.h"

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#ifdef HAVE_NETWORKING
#include <net/net_natt.h>
#include "../network/netplay/netplay.h"

/* Find the most suitable address within the device's network. */
static bool find_local_address(struct net_ifinfo *interfaces,
   struct natt_device *device, struct natt_request *request)
{
   size_t i, j;
   struct addrinfo **addrs = NULL;
   uint32_t *scores        = NULL;
   uint32_t highest_score  = 0;
   struct addrinfo hints   = {0};
   uint8_t *dev_addr8      = (uint8_t *) &device->addr.sin_addr;
   bool ret                = false;

   addrs = calloc(interfaces->size, sizeof(*addrs));
   if (!addrs)
      goto done;
   scores = calloc(interfaces->size, sizeof(*scores));
   if (!scores)
      goto done;
   hints.ai_family = AF_INET;

   /* Score interfaces based on how close their address
      is from the device's address. */
   for (i = 0; i < interfaces->size; i++)
   {
      struct net_ifinfo_entry *entry = &interfaces->entries[i];
      struct addrinfo         **addr = &addrs[i];
      uint32_t                *score = &scores[i];       

      if (getaddrinfo_retro(entry->host, NULL, &hints, addr))
         continue;

      if (*addr)
      {
         uint8_t *addr8 = (uint8_t *)
            &((struct sockaddr_in *) (*addr)->ai_addr)->sin_addr;

         for (j = 0; j < sizeof(device->addr.sin_addr); j++)
         {
            if (addr8[j] != dev_addr8[j])
               break;
            (*score)++;
         }
      }
   }

   /* Get the highest scored interface. */
   for (j = 0; j < interfaces->size; j++)
   {
      uint32_t score = scores[j];

      if (score > highest_score)
      {
         highest_score = score;
         i = j;
      }
   }
   /* Skip a highest score of zero. */
   if (highest_score)
   {
      /* Copy the interface's address to our request. */
      memcpy(&request->addr.sin_addr,
         &((struct sockaddr_in *) addrs[i]->ai_addr)->sin_addr,
         sizeof(request->addr.sin_addr));
      ret = true;
   }

   for (i = 0; i < interfaces->size; i++)
      freeaddrinfo_retro(addrs[i]);

done:
   free(addrs);
   free(scores);

   return ret;
}

static void task_netplay_nat_traversal_handler(retro_task_t *task)
{
   struct nat_traversal_data *data = task->task_data;
   natt_state_t *natt_st           = natt_state_get_ptr();

   /* Try again on the next call. */
   if (natt_st->device.busy)
      return;

   switch (data->status)
   {
      case NAT_TRAVERSAL_STATUS_DISCOVERY:
         {
            if (!natt_init())
               goto finished;

            data->status = NAT_TRAVERSAL_STATUS_SELECT_DEVICE;
         }
         break;

      case NAT_TRAVERSAL_STATUS_SELECT_DEVICE:
         {
            if (!natt_device_next(&natt_st->device))
            {
               natt_deinit();
               goto finished;
            }
            if (!string_is_empty(natt_st->device.desc))
               data->status = NAT_TRAVERSAL_STATUS_QUERY_DEVICE;
         }
         break;

      case NAT_TRAVERSAL_STATUS_QUERY_DEVICE:
         {
            if (natt_query_device(&natt_st->device, false))
               data->status = NAT_TRAVERSAL_STATUS_EXTERNAL_ADDRESS;
            else
               data->status = NAT_TRAVERSAL_STATUS_SELECT_DEVICE;
         }
         break;

      case NAT_TRAVERSAL_STATUS_EXTERNAL_ADDRESS:
         {
            if (string_is_empty(natt_st->device.service_type))
            {
               data->status = NAT_TRAVERSAL_STATUS_SELECT_DEVICE;
               break;
            }
            if (natt_external_address(&natt_st->device, false))
            {
               data->forward_type = NATT_FORWARD_TYPE_ANY;
               data->status       = NAT_TRAVERSAL_STATUS_OPEN;
            }
            else
               data->status       = NAT_TRAVERSAL_STATUS_SELECT_DEVICE;
         }
         break;

      case NAT_TRAVERSAL_STATUS_OPEN:
         {
            if (natt_st->device.ext_addr.sin_family != AF_INET)
            {
               data->status = NAT_TRAVERSAL_STATUS_SELECT_DEVICE;
               break;
            }
            if (!find_local_address(&natt_st->interfaces,
               &natt_st->device, &data->request))
            {
               data->status = NAT_TRAVERSAL_STATUS_SELECT_DEVICE;
               break;
            }

            if (natt_open_port(&natt_st->device,
               &data->request, data->forward_type, false))
            {
               data->status = NAT_TRAVERSAL_STATUS_OPENING;
               break;
            }
            if (data->forward_type == NATT_FORWARD_TYPE_ANY)
               data->forward_type = NATT_FORWARD_TYPE_NONE;
            else
               data->status = NAT_TRAVERSAL_STATUS_SELECT_DEVICE;
         }
         break;

      case NAT_TRAVERSAL_STATUS_OPENING:
         {
            if (data->request.success)
            {
               /* We no longer need these */
               natt_device_end();
               natt_interfaces_destroy();

               data->status = NAT_TRAVERSAL_STATUS_OPENED;

               goto finished;
            }
            if (data->forward_type == NATT_FORWARD_TYPE_ANY)
            {
               data->forward_type = NATT_FORWARD_TYPE_NONE;
               data->status       = NAT_TRAVERSAL_STATUS_OPEN;
            }
            else
               data->status       = NAT_TRAVERSAL_STATUS_SELECT_DEVICE;
         }
         break;

      default:
         break;
   }

   return;

finished:
   task_set_progress(task, 100);
   task_set_finished(task, true);
}

static void task_netplay_nat_close_handler(retro_task_t *task)
{
   struct nat_traversal_data *data = task->task_data;
   natt_state_t *natt_st           = natt_state_get_ptr();

   /* Try again on the next call. */
   if (natt_st->device.busy)
      return;

   switch (data->status)
   {
      case NAT_TRAVERSAL_STATUS_CLOSE:
         {
            natt_close_port(&natt_st->device, &data->request, false);

            /* We will deinit NAT whether it succeeds or fails. */
            data->status = NAT_TRAVERSAL_STATUS_CLOSING;
         }
         break;

      case NAT_TRAVERSAL_STATUS_CLOSING:
         {
            natt_deinit();

            data->status = NAT_TRAVERSAL_STATUS_CLOSED;

            goto finished;
         }
         break;

      default:
         break;
   }

   return;

finished:
   task_set_progress(task, 100);
   task_set_finished(task, true);
}

static void netplay_nat_traversal_callback(retro_task_t *task,
   void *task_data, void *user_data, const char *error)
{
   netplay_driver_ctl(RARCH_NETPLAY_CTL_FINISHED_NAT_TRAVERSAL, NULL);
}

static bool nat_task_finder(retro_task_t *task, void *userdata)
{
   if (!task)
      return false;

   return task->handler == task_netplay_nat_traversal_handler ||
      task->handler == task_netplay_nat_close_handler;
}

static bool nat_task_queued(void *data)
{
   task_finder_data_t find_data = {nat_task_finder, NULL};

   return task_queue_find(&find_data);
}

bool task_push_netplay_nat_traversal(void *data, uint16_t port)
{
   retro_task_t *task;
   struct nat_traversal_data *natt_data = data;

   /* Do not run more than one NAT task at a time. */
   task_queue_wait(nat_task_queued, NULL);

   task = task_init();
   if (!task)
      return false;

   natt_deinit();

   natt_data->request.addr.sin_family = AF_INET;
   natt_data->request.addr.sin_port   = htons(port);
   natt_data->request.proto           = SOCKET_PROTOCOL_TCP;
   natt_data->status                  = NAT_TRAVERSAL_STATUS_DISCOVERY;

   task->handler   = task_netplay_nat_traversal_handler;
   task->callback  = netplay_nat_traversal_callback;
   task->task_data = data;

   task_queue_push(task);

   return true;
}

bool task_push_netplay_nat_close(void *data)
{
   retro_task_t *task;
   struct nat_traversal_data *natt_data = data;

   /* Do not run more than one NAT task at a time. */
   task_queue_wait(nat_task_queued, NULL);

   if (!natt_data->request.addr.sin_port)
      return false;

   task = task_init();
   if (!task)
      return false;

   natt_data->request.addr.sin_family = AF_INET;
   natt_data->request.proto           = SOCKET_PROTOCOL_TCP;
   natt_data->status                  = NAT_TRAVERSAL_STATUS_CLOSE;

   task->handler   = task_netplay_nat_close_handler;
   task->task_data = data;

   task_queue_push(task);

   return true;
}
#else
bool task_push_netplay_nat_traversal(void *data, uint16_t port) { return false; }
bool task_push_netplay_nat_close(void *data) { return false; }
#endif
