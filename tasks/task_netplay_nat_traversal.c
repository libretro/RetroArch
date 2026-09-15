/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2017-2017 - Gregor Richards
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

#ifdef HAVE_IFINFO
#include <net/net_ifinfo.h>
#endif

#include "../network/natt.h"
#include "../network/netplay/netplay.h"

/* Find the most suitable address within the device's network. */
static bool find_local_address(struct natt_device *device,
      struct natt_request *request)
{
   bool ret = false;

#ifdef HAVE_IFINFO
   struct net_ifinfo interfaces = {0};
   struct addrinfo **addrs      = NULL;
   uint32_t *scores             = NULL;

   if (net_ifinfo_new(&interfaces) && interfaces.size > 0)
   {
      size_t i, j, k;
      uint32_t highest_score = 0;
      struct addrinfo hints  = {0};
      uint8_t *dev_addr8     = (uint8_t*)&device->addr.sin_addr;

      if (!(addrs  = (struct addrinfo**)calloc(interfaces.size, sizeof(*addrs))))
         goto done;
      if (!(scores = (uint32_t*)calloc(interfaces.size, sizeof(*scores))))
         goto done;

      hints.ai_family = AF_INET;
      hints.ai_flags  = AI_NUMERICHOST;

      /* Score interfaces based on how "close" their address
         is from the device's address. */
      for (i = 0; i < interfaces.size; i++)
      {
         struct net_ifinfo_entry *entry = &interfaces.entries[i];
         struct addrinfo         **addr = &addrs[i];
         uint32_t                *score = &scores[i];

         if (getaddrinfo_retro(entry->host, NULL, &hints, addr))
            continue;

         /* Sanity check */
         if (*addr && (*addr)->ai_family == AF_INET)
         {
            uint8_t *addr8 =
               (uint8_t*)&((struct sockaddr_in*)(*addr)->ai_addr)->sin_addr;
            bool stop_score = false;

            for (j = 0; j < sizeof(device->addr.sin_addr) && !stop_score; j++)
            {
               uint8_t bits_dev  = dev_addr8[j];
               uint8_t bits_addr = addr8[j];

               for (k = 0; k < 8; k++)
               {
                  /* Each matched bit (from high to low bits)
                     means +1 to score.
                     Stop scoring when a bit mismatch. */
                  uint8_t bit_mask = 0x80 >> k;
                  uint8_t bit_dev  = bits_dev & bit_mask;
                  uint8_t bit_addr = bits_addr & bit_mask;

                  if (bit_addr != bit_dev)
                  {
                     stop_score = true;
                     break;
                  }

                  (*score)++;
               }
            }
         }
      }

      /* Get the highest scored interface. */
      for (j = 0; j < interfaces.size; j++)
      {
         uint32_t score = scores[j];

         if (score > highest_score)
         {
            highest_score = score;
            i = j;
         }
      }
      /* Skip a highest score of less than 8. */
      if (highest_score >= 8)
      {
         /* Copy the interface's address to our request. */
         memcpy(&request->addr.sin_addr,
            &((struct sockaddr_in*)addrs[i]->ai_addr)->sin_addr,
            sizeof(request->addr.sin_addr));
         ret = true;
      }

      for (i = 0; i < interfaces.size; i++)
         freeaddrinfo_retro(addrs[i]);
   }

done:
   free(scores);
   free(addrs);
   net_ifinfo_free(&interfaces);
#else
   int dummy_fd = socket_create("dummy",
      SOCKET_DOMAIN_INET, SOCKET_TYPE_DATAGRAM, SOCKET_PROTOCOL_UDP);

   if (dummy_fd >= 0)
   {
      struct sockaddr_in addr    = {0};
      socklen_t          addrlen = sizeof(addr);

      if (     !connect(dummy_fd, (struct sockaddr*)&device->addr,
               sizeof(device->addr))
            && !getsockname(dummy_fd, (struct sockaddr*)&addr, &addrlen))
      {
         /* Make sure this is not "0.0.0.0". */
         if (addr.sin_addr.s_addr)
         {
            /* Copy the address to our request. */
            memcpy(&request->addr.sin_addr, &addr.sin_addr,
               sizeof(request->addr.sin_addr));
            ret = true;
         }
      }

      socket_close(dummy_fd);
   }
#endif

   return ret;
}

static void task_netplay_nat_traversal_handler(retro_task_t *task)
{
   static struct natt_discovery discovery = {-1, -1};
   static struct natt_device    device    = {0};
   struct nat_traversal_data   *data      = (struct nat_traversal_data*)task->task_data;

   /* Try again on the next call. */
   if (device.busy)
      return;

   switch (data->status)
   {
      case NAT_TRAVERSAL_STATUS_DISCOVERY:
         {
            if (!natt_init(&discovery))
               goto finished;

            data->status = NAT_TRAVERSAL_STATUS_SELECT_DEVICE;
         }
         break;

      case NAT_TRAVERSAL_STATUS_SELECT_DEVICE:
         {
            if (!natt_device_next(&discovery, &device))
            {
               natt_device_end(&discovery);
               goto finished;
            }

            if (!*device.desc)
               break;
            if (!find_local_address(&device, &data->request))
               break;

            data->status = NAT_TRAVERSAL_STATUS_QUERY_DEVICE;
         }
         break;

      case NAT_TRAVERSAL_STATUS_QUERY_DEVICE:
         {
            if (natt_query_device(&device, false))
               data->status = NAT_TRAVERSAL_STATUS_EXTERNAL_ADDRESS;
            else
               data->status = NAT_TRAVERSAL_STATUS_SELECT_DEVICE;
         }
         break;

      case NAT_TRAVERSAL_STATUS_EXTERNAL_ADDRESS:
         {
            if (!*device.service_type)
            {
               data->status = NAT_TRAVERSAL_STATUS_SELECT_DEVICE;
               break;
            }

            if (natt_external_address(&device, false))
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
            if (device.ext_addr.sin_family != AF_INET)
            {
               data->status = NAT_TRAVERSAL_STATUS_SELECT_DEVICE;
               break;
            }

            if (natt_open_port(&device, &data->request,
                  data->forward_type, false))
               data->status = NAT_TRAVERSAL_STATUS_OPENING;
            else
               data->status = NAT_TRAVERSAL_STATUS_SELECT_DEVICE;
         }
         break;

      case NAT_TRAVERSAL_STATUS_OPENING:
         {
            if (data->request.success)
            {
               natt_device_end(&discovery);

               /* Copy the external address into the request. */
               memcpy(&data->request.addr.sin_addr,
                  &device.ext_addr.sin_addr,
                  sizeof(data->request.addr.sin_addr));

               data->status = NAT_TRAVERSAL_STATUS_OPENED;

               goto finished;
            }
            else if (data->forward_type == NATT_FORWARD_TYPE_ANY)
            {
               data->forward_type = NATT_FORWARD_TYPE_NONE;
               data->status       = NAT_TRAVERSAL_STATUS_OPEN;
            }
            else
               data->status       = NAT_TRAVERSAL_STATUS_SELECT_DEVICE;
         }
         break;

      case NAT_TRAVERSAL_STATUS_CLOSE:
         {
            natt_close_port(&device, &data->request, false);

            data->status = NAT_TRAVERSAL_STATUS_CLOSING;
         }
         break;

      case NAT_TRAVERSAL_STATUS_CLOSING:
         {
            memset(&data->request, 0, sizeof(data->request));

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
   task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
}

/* Deferred NAT requests.
 *
 * Only one NAT task may run at a time: both requests operate on the
 * single shared nat_traversal_request object, so a second task would
 * race the first over the same struct.  That exclusion used to be
 * enforced by blocking the calling thread until the in-flight task
 * finished - and the calling thread here is the one driving the menu,
 * while the work being waited on is UPnP discovery: SSDP with
 * timeouts, seconds of frozen UI for anyone who starts hosting and
 * stops again before discovery completes.
 *
 * The requests are queued instead, and flushed from the task
 * callbacks (main thread, after the task has been popped off the
 * queue).  Order is preserved rather than coalesced: a close
 * followed by an open must run as both, in that order, or the port
 * mapping the close was meant to release stays open.  Two is the
 * depth the callers can actually produce - netplay stopping and
 * starting again while a task is in flight - and the extra slots are
 * slack; a request that would overflow is refused, which is the same
 * answer the caller already handles from any other push failure. */
enum nat_pending_kind
{
   NAT_PENDING_OPEN = 0,
   NAT_PENDING_CLOSE
};

struct nat_pending_request
{
   void *data;
   uint16_t port;
   enum nat_pending_kind kind;
};

#define NAT_PENDING_MAX 4

static struct nat_pending_request nat_pending[NAT_PENDING_MAX];
static unsigned nat_pending_count;

/* Defined below the public push entry points it calls, and used by
 * the task callbacks above them. */
static void nat_pending_flush(void);

static void task_netplay_nat_traversal_callback(retro_task_t *task,
      void *task_data, void *user_data, const char *error)
{
   struct nat_traversal_data *data = (struct nat_traversal_data*)task_data;
   uintptr_t ext_port              = ntohs(data->request.addr.sin_port);

   netplay_driver_ctl(RARCH_NETPLAY_CTL_FINISHED_NAT_TRAVERSAL,
      (void*)ext_port);

   nat_pending_flush();
}

/* The close task carries no result: its callback exists purely to
 * start whatever was queued behind it. */
static void task_netplay_nat_close_callback(retro_task_t *task,
      void *task_data, void *user_data, const char *error)
{
   nat_pending_flush();
}

static bool nat_task_finder(retro_task_t *task, void *userdata)
{
   if (!task)
      return false;

   return task->handler == task_netplay_nat_traversal_handler;
}

static bool nat_task_queued(void *data)
{
   task_finder_data_t find_data = {nat_task_finder, NULL};

   return task_queue_find(&find_data);
}

/* Queue a request to run once the in-flight task retires.  Returns
 * false when the queue is full, so the caller sees a push failure
 * rather than silently losing the request. */
static bool nat_pending_push(void *data, uint16_t port,
      enum nat_pending_kind kind)
{
   if (nat_pending_count >= NAT_PENDING_MAX)
      return false;

   nat_pending[nat_pending_count].data = data;
   nat_pending[nat_pending_count].port = port;
   nat_pending[nat_pending_count].kind = kind;
   nat_pending_count++;

   return true;
}

/* Main thread, from a task callback: start the next queued request.
 * The entry is removed before it is re-pushed, so a push that queues
 * again cannot recurse.  Re-pushing through the public entry points
 * means every validation runs at the moment the request actually
 * starts, not when it was queued - which matters for the close path,
 * whose checks require the mapping to be open, something that is
 * only true once the open task ahead of it has finished. */
static void nat_pending_flush(void)
{
   /* Drain until something actually starts.  A re-pushed request can
    * still be refused - a close whose mapping never opened because
    * the open ahead of it found no device fails its validation - and
    * a refusal starts no task, so no further callback would arrive to
    * flush the rest.  Stopping at the first refusal would strand
    * every request behind it forever. */
   while (nat_pending_count)
   {
      struct nat_pending_request req = nat_pending[0];
      unsigned i;
      bool started;

      for (i = 1; i < nat_pending_count; i++)
         nat_pending[i - 1] = nat_pending[i];
      nat_pending_count--;

      if (req.kind == NAT_PENDING_OPEN)
         started = task_push_netplay_nat_traversal(req.data, req.port);
      else
         started = task_push_netplay_nat_close(req.data);

      /* A task is running again; its callback flushes the rest. */
      if (started)
         break;
   }
}

bool task_push_netplay_nat_traversal(void *data, uint16_t port)
{
   retro_task_t *task                   = NULL;
   struct nat_traversal_data *natt_data = (struct nat_traversal_data*)data;

   /* Do not run more than one NAT task at a time: queue behind the
    * one in flight rather than blocking here. */
   if (nat_task_queued(NULL))
      return nat_pending_push(data, port, NAT_PENDING_OPEN);

   task                                 = task_init();
   if (!task)
      return false;

   natt_data->request.addr.sin_family   = AF_INET;
   natt_data->request.addr.sin_port     = htons(port);
   natt_data->request.proto             = SOCKET_PROTOCOL_TCP;
   natt_data->request.device            = NULL;
   natt_data->status                    = NAT_TRAVERSAL_STATUS_DISCOVERY;

   task->handler                        = task_netplay_nat_traversal_handler;
   task->callback                       = task_netplay_nat_traversal_callback;
   task->task_data                      = data;

   task_queue_push(task);

   return true;
}

bool task_push_netplay_nat_close(void *data)
{
   retro_task_t *task                   = NULL;
   struct nat_traversal_data *natt_data = (struct nat_traversal_data*)data;

   /* Do not run more than one NAT task at a time: queue behind the
    * one in flight rather than blocking here.  The validation below
    * runs when this request actually starts - an open task ahead of
    * it has to finish before the mapping it checks for exists. */
   if (nat_task_queued(NULL))
      return nat_pending_push(data, 0, NAT_PENDING_CLOSE);

   if (natt_data->status != NAT_TRAVERSAL_STATUS_OPENED)
      return false;
   if (natt_data->request.addr.sin_family != AF_INET)
      return false;
   if (!natt_data->request.addr.sin_port)
      return false;
   if (natt_data->request.proto != SOCKET_PROTOCOL_TCP)
      return false;
   if (!natt_data->request.device)
      return false;

   task = task_init();
   if (!task)
      return false;

   natt_data->status = NAT_TRAVERSAL_STATUS_CLOSE;

   task->handler     = task_netplay_nat_traversal_handler;
   task->callback    = task_netplay_nat_close_callback;
   task->task_data   = data;

   task_queue_push(task);

   return true;
}
#else
bool task_push_netplay_nat_traversal(void *data, uint16_t port) { return false; }
bool task_push_netplay_nat_close(void *data) { return false; }
#endif
