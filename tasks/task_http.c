/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2015 - Daniel De Matteis
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

#include <retro_miscellaneous.h>
#include <net/net_http.h>
#include <queues/message_queue.h>
#include <string/string_list.h>
#include <compat/strl.h>
#include <file/file_path.h>
#include <file/file_extract.h>
#include <net/net_compat.h>
#include <retro_file.h>
#include <retro_stat.h>

#include "../file_ops.h"
#include "../general.h"
#include "../msg_hash.h"
#include "../verbosity.h"
#include "tasks.h"

extern char core_updater_path[PATH_MAX_LENGTH];

enum http_status_enum
{
   HTTP_STATUS_POLL = 0,
   HTTP_STATUS_CONNECTION_TRANSFER,
   HTTP_STATUS_CONNECTION_TRANSFER_PARSE,
   HTTP_STATUS_TRANSFER,
   HTTP_STATUS_TRANSFER_PARSE,
   HTTP_STATUS_TRANSFER_PARSE_FREE
};

typedef struct http_handle
{
   struct
   {
      struct http_connection_t *handle;
      transfer_cb_t  cb;
      char elem1[PATH_MAX_LENGTH];
   } connection;
   struct http_t *handle;
   transfer_cb_t  cb;
   unsigned status;
} http_handle_t;

static int rarch_main_data_http_con_iterate_transfer(http_handle_t *http)
{
   if (!net_http_connection_iterate(http->connection.handle))
      return -1;
   return 0;
}

static int rarch_main_data_http_conn_iterate_transfer_parse(http_handle_t *http)
{
   if (net_http_connection_done(http->connection.handle))
   {
      if (http->connection.handle && http->connection.cb)
         http->connection.cb(http, 0);
   }
   
   net_http_connection_free(http->connection.handle);

   http->connection.handle = NULL;

   return 0;
}

static int cb_http_conn_default(void *data_, size_t len)
{
   http_handle_t *http = (http_handle_t*)data_;

   if (!http)
      return -1;

   if (!network_init())
      return -1;

   http->handle = net_http_new(http->connection.handle);

   if (!http->handle)
   {
      RARCH_ERR("Could not create new HTTP session handle.\n");
      return -1;
   }

   http->cb     = NULL;

   return 0;
}

/**
 * rarch_main_data_http_iterate_transfer:
 *
 * Resumes HTTP transfer update.
 *
 * Returns: 0 when finished, -1 when we should continue
 * with the transfer on the next frame.
 **/
static int rarch_main_data_http_iterate_transfer(void *data)
{
   http_handle_t *http  = (http_handle_t*)data;
   size_t pos  = 0, tot = 0;

   if (!net_http_update(http->handle, &pos, &tot))
   {
      int percent = 0;

      if(tot != 0)
         percent = (unsigned long long)pos * 100
            / (unsigned long long)tot;

      if (percent > 0)
      {
         char tmp[PATH_MAX_LENGTH];
         snprintf(tmp, sizeof(tmp), "%s: %d%%",
               msg_hash_to_str(MSG_DOWNLOAD_PROGRESS),
               percent);
         data_runloop_osd_msg(tmp, sizeof(tmp));
      }

      return -1;
   }

   return 0;
}

static void rarch_task_http_transfer_handler(rarch_task_t *task)
{
   http_handle_t *http = (http_handle_t*)task->state;
   http_transfer_data_t *data;

   switch (http->status)
   {
      case HTTP_STATUS_CONNECTION_TRANSFER_PARSE:
         rarch_main_data_http_conn_iterate_transfer_parse(http);
         http->status = HTTP_STATUS_TRANSFER;
         break;
      case HTTP_STATUS_CONNECTION_TRANSFER:
         if (!rarch_main_data_http_con_iterate_transfer(http))
            http->status = HTTP_STATUS_CONNECTION_TRANSFER_PARSE;
         break;
      case HTTP_STATUS_TRANSFER_PARSE:
         goto task_finished;
         break;
      case HTTP_STATUS_TRANSFER:
         if (!rarch_main_data_http_iterate_transfer(http))
            http->status = HTTP_STATUS_TRANSFER_PARSE;
         break;
      case HTTP_STATUS_POLL:
         goto task_finished;
      default:
         break;
   }

   return;
task_finished:
   task->finished = true;

   data = (http_transfer_data_t*)calloc(1, sizeof(*data));
   task->task_data = data;

   if (http->handle)
   {
      data->data = (char*)net_http_data(http->handle, &data->len, false);

      if (data->data && http->cb)
         http->cb(data->data, data->len);

      /* we can't let net_http_delete free our data */
      net_http_delete(http->handle);
   }

   free(http);
}

bool rarch_task_push_http_transfer(const char *url, const char *type, rarch_task_callback_t cb, void *user_data)
{
   rarch_task_t  *t;
   http_handle_t *http;
   struct http_connection_t *conn;

   if (!url || !*url)
      return false;

   conn = net_http_connection_new(url);

   if (!conn)
      return false;

   http = (http_handle_t*)calloc(1, sizeof(*http));
   http->connection.handle = conn;
   http->connection.cb     = &cb_http_conn_default;

   if (type)
      strlcpy(http->connection.elem1, type, sizeof(http->connection.elem1));

   http->status = HTTP_STATUS_CONNECTION_TRANSFER;

   t = (rarch_task_t*)calloc(1, sizeof(*t));
   t->handler = rarch_task_http_transfer_handler;
   t->state = http;
   t->callback = cb;
   t->user_data = user_data;

   rarch_task_push(t);

   return true;
}
