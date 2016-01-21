/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2016 - Daniel De Matteis
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
#include <string/stdstring.h>
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
   bool error;
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
      RARCH_ERR("[http] Could not create new HTTP session handle.\n");
      http->error = true;
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
static int rarch_main_data_http_iterate_transfer(rarch_task_t *task)
{
   http_handle_t *http  = (http_handle_t*)task->state;
   size_t pos  = 0, tot = 0;

   if (!net_http_update(http->handle, &pos, &tot))
   {
      task->progress = (tot == 0) ? -1 : (signed)(pos * 100 / tot);
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
      case HTTP_STATUS_TRANSFER:
         if (!rarch_main_data_http_iterate_transfer(task))
            goto task_finished;
         break;
      case HTTP_STATUS_TRANSFER_PARSE:
      case HTTP_STATUS_POLL:
         goto task_finished;
      default:
         break;
   }

   if (task->cancelled || http->error)
      goto task_finished;

   return;
task_finished:
   task->finished = true;

   if (http->handle)
   {
      size_t len = 0;
      char  *tmp = (char*)net_http_data(http->handle, &len, false);

      if (tmp && http->cb)
         http->cb(tmp, len);

      if (net_http_error(http->handle) || task->cancelled)
      {
         tmp = (char*)net_http_data(http->handle, &len, true);

         if (tmp)
            free(tmp);

         if (task->cancelled)
            task->error = strdup("Task cancelled.");
         else
            task->error = strdup("Download failed.");
      }
      else
      {
         data = (http_transfer_data_t*)calloc(1, sizeof(*data));
         data->data = tmp;
         data->len  = len;

         task->task_data = data;
      }

      net_http_delete(http->handle);
   } else if (http->error)
      task->error = strdup("Internal error.");

   free(http);
}

static bool rarch_task_http_finder(rarch_task_t *task, void *user_data)
{
   http_handle_t *http = (http_handle_t*)task->state;
   const char *handle_url;
   if (task->handler != rarch_task_http_transfer_handler)
      return false;

   handle_url = net_http_connection_url(http->connection.handle);

   return string_is_equal(handle_url, (const char*)user_data);
}

bool rarch_task_push_http_transfer(const char *url, const char *type, rarch_task_callback_t cb, void *user_data)
{
   struct http_connection_t *conn;
   char tmp[PATH_MAX_LENGTH];
   rarch_task_t  *t               = NULL;
   http_handle_t *http            = NULL;

   if (string_is_empty(url))
      return false;

   /* Concurrent download of the same file is not allowed */
   if (rarch_task_find(rarch_task_http_finder, (void*)url))
   {
      RARCH_LOG("[http] '%s'' is already being downloaded.\n", url);
      return false;
   }

   conn = net_http_connection_new(url);

   if (!conn)
      return false;

   http                    = (http_handle_t*)calloc(1, sizeof(*http));

   if (!http)
      goto error;

   http->connection.handle = conn;
   http->connection.cb     = &cb_http_conn_default;

   if (type)
      strlcpy(http->connection.elem1, type, sizeof(http->connection.elem1));

   http->status            = HTTP_STATUS_CONNECTION_TRANSFER;
   t                       = (rarch_task_t*)calloc(1, sizeof(*t));

   if (!t)
      goto error;

   t->handler              = rarch_task_http_transfer_handler;
   t->state                = http;
   t->callback             = cb;
   t->user_data            = user_data;
   t->progress             = -1;

   snprintf(tmp, sizeof(tmp), "%s '%s'", msg_hash_to_str(MSG_DOWNLOADING), path_basename(url));
   t->title                = strdup(tmp);

   rarch_task_push(t);

   return true;

error:
   if (conn)
      net_http_connection_free(conn);
   if (t)
      free(t);
   if (http)
      free(http);

   return false;
}
