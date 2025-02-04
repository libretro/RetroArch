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

#ifndef EMSCRIPTEN
#error "task_http_emscripten only makes sense in emscripten builds"
#endif

#include <stdlib.h>
#include "verbosity.h"
#include <emscripten/emscripten.h>

#include <string/stdstring.h>
#include <compat/strl.h>
#include <file/file_path.h>
#include <retro_timers.h>
#include <retro_miscellaneous.h>

#ifdef RARCH_INTERNAL
#include "../gfx/video_display_server.h"
#endif
#include "task_file_transfer.h"
#include "tasks_internal.h"

struct http_handle
{
   int handle;
   char connection_url[NAME_MAX_LENGTH];
   http_transfer_data_t *response;
};

typedef struct http_handle http_handle_t;

static void task_http_transfer_handler(retro_task_t *task)
{
   http_handle_t        *http = (http_handle_t*)task->state;
   uint8_t flg                = task_get_flags(task);

   if ((flg & RETRO_TASK_FLG_CANCELLED) > 0)
      goto task_finished;

   if (http->response || (http->handle == -1))
      goto task_finished;

   return;
task_finished:
   task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
   if (http->response)
   {
      if ((flg & RETRO_TASK_FLG_CANCELLED) > 0)
      {
         string_list_free(http->response->headers);
         free(http->response->data);
         free(http->response);
         http->response = NULL;
         task_set_error(task,
               strldup("Task cancelled.", sizeof("Task cancelled.")));
      }
      else
      {
         bool mute;
         task_set_data(task, http->response);
         mute          = ((task->flags & RETRO_TASK_FLG_MUTE) > 0);
         if (!mute && http->response->status >= 400)
            task_set_error(task, strldup("Download failed.",
               sizeof("Download failed.")));
      }
   }
   free(http);
}

static void http_transfer_progress_cb(retro_task_t *task)
{
   if (task)
      video_display_server_set_window_progress(task->progress,
            ((task->flags & RETRO_TASK_FLG_FINISHED) > 0));
}

static bool task_http_finder(retro_task_t *task, void *user_data)
{
   http_handle_t *http = NULL;
   if (task && (task->handler == task_http_transfer_handler) && user_data)
      if ((http = (http_handle_t*)task->state))
         return string_is_equal(http->connection_url, (const char*)user_data);
   return false;
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

void wget_onload_cb(unsigned handle, void *t_ptr, void *data, unsigned len) {
   retro_task_t *task  = (retro_task_t *)t_ptr;
   http_handle_t *http = (http_handle_t*)task->state;
   http_transfer_data_t *resp;
   if (!(resp = (http_transfer_data_t*)malloc(sizeof(*resp)))) {
      http->handle = -1;
      return;
   } else {
      resp->data = data;
      resp->len = len;
      resp->status = 200;
      resp->headers = NULL; // sorry webdav
      http->response = resp;
   }
}

void wget_onerror_cb(unsigned handle, void *t_ptr, int status, const char *err) {
   retro_task_t *task  = (retro_task_t *)t_ptr;
   http_handle_t *http = (http_handle_t*)task->state;
   bool mute           = ((task->flags & RETRO_TASK_FLG_MUTE) > 0);
   if (!mute)
      task_set_error(task, strldup("Download failed.",
                                   sizeof("Download failed.")));
   http->handle        = -1;
}

void wget_onprogress_cb(unsigned handle, void *t_ptr, int pos, int tot) {
   retro_task_t *task  = (retro_task_t *)t_ptr;
   if (tot == 0)
      task_set_progress(task, -1);
   else if (pos < (((size_t)-1) / 100))
      /* prefer multiply then divide for more accurate results */
      task_set_progress(task, (signed)(pos * 100 / tot));
   else
      /* but invert the logic if it would cause an overflow */
      task_set_progress(task, MIN((signed)pos / (tot / 100), 100));
}

static void *task_push_http_transfer_generic(
      const char *url, const char *method,
      const char *data, const char *user_agent,
      const char *headers,
      bool mute,
      retro_task_callback_t cb, void *user_data)
{
   retro_task_t  *t        = NULL;
   http_handle_t *http     = NULL;
   int wget_handle         = -1;
   if (!url)
      return NULL;

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
         return NULL;
   }

   if (!(http = (http_handle_t*)malloc(sizeof(*http))))
      goto error;

   http->handle              = -1;
   http->response            = NULL;
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

   wget_handle = emscripten_async_wget2_data(url, method, data, t, false, wget_onload_cb, wget_onerror_cb, wget_onprogress_cb);

   http->handle = wget_handle;

   task_queue_push(t);

   return t;

error:
   if (http)
      free(http);
   if (t)
      free(t);
   return NULL;
}


void* task_push_http_transfer(const char *url, bool mute,
      const char *type,
      retro_task_callback_t cb, void *user_data)
{
   return task_push_http_transfer_generic(url, type ? type : "GET", NULL, NULL, NULL, mute, cb, user_data);
}

void *task_push_webdav_stat(const char *url, bool mute, const char *headers,
      retro_task_callback_t cb, void *user_data)
{
   RARCH_ERR("[http] response headers not supported, webdav won't work\n");
   return task_push_http_transfer_generic(url, "OPTIONS", NULL, NULL, headers, mute, cb, user_data);
}

void* task_push_webdav_mkdir(const char *url, bool mute,
      const char *headers,
      retro_task_callback_t cb, void *user_data)
{
   RARCH_ERR("[http] response headers not supported, webdav won't work\n");
   return task_push_http_transfer_generic(url, "MKCOL", NULL, NULL, headers, mute, cb, user_data);
}

void* task_push_webdav_put(const char *url,
      const void *put_data, size_t len, bool mute,
      const char *headers, retro_task_callback_t cb, void *user_data)
{
   char                      expect[1024]; /* TODO/FIXME - check size */
   size_t                    _len;
   RARCH_ERR("[http] response headers not supported, webdav won't work\n");

   _len = strlcpy(expect, "Expect: 100-continue\r\n", sizeof(expect));
   if (headers)
   {
      strlcpy(expect + _len, headers, sizeof(expect) - _len);
   }

   return task_push_http_transfer_generic(url, "PUT", put_data, NULL, expect, mute, cb, user_data);
}

void* task_push_webdav_delete(const char *url, bool mute,
      const char *headers,
      retro_task_callback_t cb, void *user_data)
{
   RARCH_ERR("[http] response headers not supported, webdav won't work\n");
   return task_push_http_transfer_generic(url, "DELETE", NULL, NULL, headers, mute, cb, user_data);
}

void *task_push_webdav_move(const char *url,
      const char *dest, bool mute, const char *headers,
      retro_task_callback_t cb, void *user_data)
{
   size_t _len;
   char dest_header[PATH_MAX_LENGTH + 512];
   RARCH_ERR("[http] response headers not supported, webdav won't work\n");

   _len  = strlcpy(dest_header, "Destination: ", sizeof(dest_header));
   _len += strlcpy(dest_header + _len, dest,   sizeof(dest_header) - _len);
   _len += strlcpy(dest_header + _len, "\r\n", sizeof(dest_header) - _len);

   if (headers)
      strlcpy(dest_header + _len, headers, sizeof(dest_header) - _len);

   return task_push_http_transfer_generic(url, "MOVE", NULL, NULL, dest_header, mute, cb, user_data);
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
               url, "GET",
               NULL, NULL, NULL,
               mute, cb, transfer_data)))
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
   return task_push_http_transfer_generic(url, type ? type : "GET", NULL, user_agent, NULL, mute, cb, user_data);
}

void* task_push_http_transfer_with_headers(const char *url, bool mute,
   const char *type, const char *headers,
   retro_task_callback_t cb, void *user_data)
{
   return task_push_http_transfer_generic(url, type ? type : "GET", NULL, NULL, headers, mute, cb, user_data);
}

void* task_push_http_post_transfer(const char *url,
      const char *post_data, bool mute,
      const char *type, retro_task_callback_t cb, void *user_data)
{
   return task_push_http_transfer_generic(url, type ? type : "POST", post_data, NULL, NULL, mute, cb, user_data);
}

void* task_push_http_post_transfer_with_user_agent(const char *url,
   const char *post_data, bool mute,
   const char *type, const char *user_agent,
   retro_task_callback_t cb, void *user_data)
{
   return task_push_http_transfer_generic(url, type ? type : "POST", post_data, user_agent, NULL, mute, cb, user_data);
}

void* task_push_http_post_transfer_with_headers(const char *url,
   const char *post_data, bool mute,
   const char *type, const char *headers,
   retro_task_callback_t cb, void *user_data)
{
   return task_push_http_transfer_generic(url, type ? type : "POST", post_data, NULL, headers, mute, cb, user_data);
}
