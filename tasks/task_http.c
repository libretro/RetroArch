/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#include <net/net_http.h>
#include <string/stdstring.h>
#include <compat/strl.h>
#include <file/file_path.h>
#include <net/net_compat.h>
#include <retro_timers.h>
#include <retro_miscellaneous.h>

#ifdef RARCH_INTERNAL
#include "../gfx/video_display_server.h"
#endif
#include "task_file_transfer.h"
#include "tasks_internal.h"

enum http_status_enum
{
   HTTP_STATUS_CONNECTION_TRANSFER = 0,
   HTTP_STATUS_CONNECTION_TRANSFER_PARSE,
   HTTP_STATUS_TRANSFER,
   HTTP_STATUS_TRANSFER_PARSE,
   HTTP_STATUS_TRANSFER_PARSE_FREE
};

struct http_handle
{
   struct http_t *handle;
   struct
   {
      struct http_connection_t *handle;
      transfer_cb_t  cb;
   } connection;
   enum http_status_enum status;
   bool error;
   char connection_url[NAME_MAX_LENGTH];
};

typedef struct http_handle http_handle_t;

static int task_http_con_iterate_transfer(http_handle_t *http)
{
   if (!net_http_connection_iterate(http->connection.handle))
      return -1;
   return 0;
}

static int task_http_conn_iterate_transfer_parse(
      http_handle_t *http)
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

   if (!(http->handle = net_http_new(http->connection.handle)))
   {
      http->error = true;
      return -1;
   }

   return 0;
}

/**
 * task_http_iterate_transfer:
 *
 * Resumes HTTP transfer update.
 *
 * Returns: 0 when finished, -1 when we should continue
 * with the transfer on the next frame.
 **/
static int task_http_iterate_transfer(retro_task_t *task)
{
   http_handle_t *http  = (http_handle_t*)task->state;
   size_t pos  = 0, tot = 0;

   /* FIXME: This wouldn't be needed if we could wait for a timeout */
   if (task_queue_is_threaded())
      retro_sleep(1);

   if (!net_http_update(http->handle, &pos, &tot))
   {
      if (tot == 0)
         task_set_progress(task, -1);
      else if (pos < (((size_t)-1) / 100))
         /* prefer multiply then divide for more accurate results */
         task_set_progress(task, (signed)(pos * 100 / tot));
      else
         /* but invert the logic if it would cause an overflow */
         task_set_progress(task, MIN((signed)pos / (tot / 100), 100));
      return -1;
   }

   return 0;
}

static void task_http_transfer_handler(retro_task_t *task)
{
   http_transfer_data_t *data = NULL;
   http_handle_t        *http = (http_handle_t*)task->state;
   uint8_t flg                = task_get_flags(task);

   if ((flg & RETRO_TASK_FLG_CANCELLED) > 0)
      goto task_finished;

   switch (http->status)
   {
      case HTTP_STATUS_CONNECTION_TRANSFER_PARSE:
         task_http_conn_iterate_transfer_parse(http);
         http->status = HTTP_STATUS_TRANSFER;
         break;
      case HTTP_STATUS_CONNECTION_TRANSFER:
         if (!task_http_con_iterate_transfer(http))
            http->status = HTTP_STATUS_CONNECTION_TRANSFER_PARSE;
         break;
      case HTTP_STATUS_TRANSFER:
         if (!task_http_iterate_transfer(task))
            goto task_finished;
         break;
      case HTTP_STATUS_TRANSFER_PARSE:
         goto task_finished;
      default:
         break;
   }

   if (http->error)
      goto task_finished;

   return;
task_finished:
   task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);

   if (http->handle)
   {
      size_t _len = 0;
      char   *tmp = (char*)net_http_data(http->handle, &_len, false);
      struct string_list *headers = net_http_headers(http->handle);

      if (!tmp)
         tmp = (char*)net_http_data(http->handle, &_len, true);

      if ((flg & RETRO_TASK_FLG_CANCELLED) > 0)
      {
         if (tmp)
            free(tmp);
         string_list_free(headers);

         task_set_error(task,
               strldup("Task cancelled.", sizeof("Task cancelled.")));
      }
      else
      {
         bool mute;
         data          = (http_transfer_data_t*)malloc(sizeof(*data));
         data->data    = tmp;
         data->headers = headers;
         data->len     = _len;
         data->status  = net_http_status(http->handle);

         task_set_data(task, data);

         mute          = ((task->flags & RETRO_TASK_FLG_MUTE) > 0);

         if (!mute && net_http_error(http->handle))
            task_set_error(task, strldup("Download failed.",
               sizeof("Download failed.")));
      }
      net_http_delete(http->handle);
   }
   else if (http->error)
      task_set_error(task, strldup("Internal error.",
               sizeof("Internal error.")));

   free(http);
}

static void task_http_transfer_cleanup(retro_task_t *task)
{
   http_transfer_data_t* data = (http_transfer_data_t*)task_get_data(task);
   if (data)
   {
      string_list_free(data->headers);
      if (data->data)
         free(data->data);
      free(data);
   }
}

static bool task_http_finder(retro_task_t *task, void *user_data)
{
   http_handle_t *http = NULL;
   if (task && (task->handler == task_http_transfer_handler) && user_data)
      if ((http = (http_handle_t*)task->state))
         return string_is_equal(http->connection_url, (const char*)user_data);
   return false;
}

static void http_transfer_progress_cb(retro_task_t *task)
{
#ifdef RARCH_INTERNAL
   if (task)
      video_display_server_set_window_progress(task->progress,
            ((task->flags & RETRO_TASK_FLG_FINISHED) > 0));
#endif
}

static void *task_push_http_transfer_generic(
      struct http_connection_t *conn,
      const char *url, bool mute,
      retro_task_callback_t cb, void *user_data)
{
   retro_task_t  *t        = NULL;
   http_handle_t *http     = NULL;
   const char    *method   = NULL;

   if (!conn)
      return NULL;

   method = net_http_connection_method(conn);
   if (!string_is_equal(method, "GET"))
   {
      /* POST requests usually mutate the server, so assume multiple calls are
       * intended, even if they're duplicated. Additionally, they may differ
       * only by the POST data, and task_http_finder doesn't look at that, so
       * unique requests could be misclassified as duplicates.
       */
   }
   else
   {
      task_finder_data_t find_data;
      find_data.func     = task_http_finder;
      find_data.userdata = (void*)url;

      /* Concurrent download of the same file is not allowed */
      if (task_queue_find(&find_data))
      {
         net_http_connection_free(conn);
         return NULL;
      }
   }

   if (!(http = (http_handle_t*)malloc(sizeof(*http))))
      goto error;

   http->handle              = NULL;
   http->connection.handle   = conn;
   http->connection.cb       = &cb_http_conn_default;
   http->status              = HTTP_STATUS_CONNECTION_TRANSFER;
   http->error               = false;
   http->connection_url[0]   = '\0';

   strlcpy(http->connection_url, url, sizeof(http->connection_url));

   if (!(t = task_init()))
      goto error;

   t->handler              = task_http_transfer_handler;
   t->state                = http;
   t->callback             = cb;
   t->progress_cb          = http_transfer_progress_cb;
   t->cleanup              = task_http_transfer_cleanup;
   t->user_data            = user_data;
   t->progress             = -1;
   if (mute)
      t->flags            |=  RETRO_TASK_FLG_MUTE;
   else
      t->flags            &= ~RETRO_TASK_FLG_MUTE;

   task_queue_push(t);

   return t;

error:
   if (conn)
      net_http_connection_free(conn);
   if (http)
      free(http);

   return NULL;
}

void* task_push_http_transfer(const char *url, bool mute,
      const char *type,
      retro_task_callback_t cb, void *user_data)
{
   if (!string_is_empty(url))
      return task_push_http_transfer_generic(
            net_http_connection_new(url, type ? type : "GET", NULL),
            url, mute, cb, user_data);
   return NULL;
}

void *task_push_webdav_stat(const char *url, bool mute, const char *headers,
      retro_task_callback_t cb, void *user_data)
{
   struct http_connection_t *conn;

   if (string_is_empty(url))
      return NULL;

   if (!(conn = net_http_connection_new(url, "OPTIONS", NULL)))
      return NULL;

   if (headers)
      net_http_connection_set_headers(conn, headers);

   return task_push_http_transfer_generic(conn, url, mute, cb, user_data);
}

void* task_push_webdav_mkdir(const char *url, bool mute,
      const char *headers,
      retro_task_callback_t cb, void *user_data)
{
   struct http_connection_t *conn;

   if (string_is_empty(url))
      return NULL;

   if (!(conn = net_http_connection_new(url, "MKCOL", NULL)))
      return NULL;

   if (headers)
      net_http_connection_set_headers(conn, headers);

   return task_push_http_transfer_generic(conn, url, mute, cb, user_data);
}

void* task_push_webdav_put(const char *url,
      const void *put_data, size_t len, bool mute,
      const char *headers, retro_task_callback_t cb, void *user_data)
{
   struct http_connection_t *conn;
   char                      expect[1024]; /* TODO/FIXME - check size */
   size_t                    _len;

   if (string_is_empty(url))
      return NULL;

   if (!(conn = net_http_connection_new(url, "PUT", NULL)))
      return NULL;

   _len = strlcpy(expect, "Expect: 100-continue\r\n", sizeof(expect));
   if (headers)
   {
      strlcpy(expect + _len, headers, sizeof(expect) - _len);
      net_http_connection_set_headers(conn, expect);
   }

   if (put_data)
      net_http_connection_set_content(conn, NULL, len, put_data);

   return task_push_http_transfer_generic(conn, url, mute, cb, user_data);
}

void* task_push_webdav_delete(const char *url, bool mute,
      const char *headers,
      retro_task_callback_t cb, void *user_data)
{
   struct http_connection_t *conn;

   if (string_is_empty(url))
      return NULL;

   if (!(conn = net_http_connection_new(url, "DELETE", NULL)))
      return NULL;

   if (headers)
      net_http_connection_set_headers(conn, headers);

   return task_push_http_transfer_generic(conn, url, mute, cb, user_data);
}

void *task_push_webdav_move(const char *url,
      const char *dest, bool mute, const char *headers,
      retro_task_callback_t cb, void *userdata)
{
   size_t _len;
   struct http_connection_t *conn;
   char dest_header[PATH_MAX_LENGTH + 512];

   if (string_is_empty(url))
      return NULL;

   if (!(conn = net_http_connection_new(url, "MOVE", NULL)))
      return NULL;

   _len  = strlcpy(dest_header, "Destination: ", sizeof(dest_header));
   _len += strlcpy(dest_header + _len, dest,   sizeof(dest_header) - _len);
   _len += strlcpy(dest_header + _len, "\r\n", sizeof(dest_header) - _len);

   if (headers)
      strlcpy(dest_header + _len, headers, sizeof(dest_header) - _len);

   net_http_connection_set_headers(conn, dest_header);

   return task_push_http_transfer_generic(conn, url, mute, cb, userdata);
}

void* task_push_http_transfer_file(const char* url, bool mute,
      const char* type,
      retro_task_callback_t cb, file_transfer_t* transfer_data)
{
   size_t _len;
   const char *s               = NULL;
   char tmp[NAME_MAX_LENGTH]   = "";
   retro_task_t *t             = NULL;

   if (string_is_empty(url))
      return NULL;

   if (!(t = (retro_task_t*)task_push_http_transfer_generic(
         /* should be using type but some callers now rely on type being ignored */
         net_http_connection_new(url, "GET", NULL),
         url, mute, cb, transfer_data)))
      return NULL;

   if (transfer_data)
      s        = transfer_data->path;
   else
      s        = url;

   _len        = strlcpy(tmp, msg_hash_to_str(MSG_DOWNLOADING), sizeof(tmp));
   tmp[  _len] = ' ';
   tmp[++_len] = '\0';

   if (string_ends_with_size(s, ".index",
            strlen(s), STRLEN_CONST(".index")))
      s       = msg_hash_to_str(MSG_INDEX_FILE);

   strlcpy(tmp + _len, s, sizeof(tmp) - _len);

   t->title = strdup(tmp);
   return t;
}

void* task_push_http_transfer_with_user_agent(const char *url, bool mute,
   const char *type, const char *user_agent,
   retro_task_callback_t cb, void *user_data)
{
   struct http_connection_t *conn;

   if (string_is_empty(url))
      return NULL;

   if (!(conn = net_http_connection_new(url, type ? type : "GET", NULL)))
      return NULL;

   if (user_agent)
      net_http_connection_set_user_agent(conn, user_agent);

   return task_push_http_transfer_generic(conn, url, mute, cb, user_data);
}

void* task_push_http_transfer_with_headers(const char *url, bool mute,
   const char *type, const char *headers,
   retro_task_callback_t cb, void *user_data)
{
   struct http_connection_t *conn;

   if (string_is_empty(url))
      return NULL;

   if (!(conn = net_http_connection_new(url, type ? type : "GET", NULL)))
      return NULL;

   if (headers)
      net_http_connection_set_headers(conn, headers);

   return task_push_http_transfer_generic(conn, url, mute, cb, user_data);
}

void* task_push_http_post_transfer(const char *url,
      const char *post_data, bool mute,
      const char *type, retro_task_callback_t cb, void *user_data)
{
   if (!string_is_empty(url))
      return task_push_http_transfer_generic(
            net_http_connection_new(url, type ? type : "POST", post_data),
            url, mute, cb, user_data);
   return NULL;
}

void* task_push_http_post_transfer_with_user_agent(const char *url,
   const char *post_data, bool mute,
   const char *type, const char *user_agent,
   retro_task_callback_t cb, void *user_data)
{
   struct http_connection_t* conn;

   if (string_is_empty(url))
      return NULL;

   if (!(conn = net_http_connection_new(url, type ? type : "POST", post_data)))
      return NULL;

   if (user_agent)
      net_http_connection_set_user_agent(conn, user_agent);

   return task_push_http_transfer_generic(conn, url, mute, cb, user_data);
}

void* task_push_http_post_transfer_with_headers(const char *url,
   const char *post_data, bool mute,
   const char *type, const char *headers,
   retro_task_callback_t cb, void *user_data)
{
   struct http_connection_t* conn;

   if (string_is_empty(url))
      return NULL;

   if (!(conn = net_http_connection_new(url, type ? type : "POST", post_data)))
      return NULL;

   if (headers)
      net_http_connection_set_headers(conn, headers);

   return task_push_http_transfer_generic(conn, url, mute, cb, user_data);
}
