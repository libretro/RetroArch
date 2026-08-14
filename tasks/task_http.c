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
#include <streams/file_stream.h>
#include <retro_timers.h>
#include <retro_miscellaneous.h>

#include "task_file_transfer.h"
#include "tasks_internal.h"

enum http_status_enum
{
   HTTP_STATUS_CONNECTION_TRANSFER = 0,
   HTTP_STATUS_CONNECTION_TRANSFER_PARSE,
   HTTP_STATUS_TRANSFER
};

struct http_handle
{
   struct http_t *handle;
   struct
   {
      struct http_connection_t *handle;
      transfer_cb_t  cb;
   } connection;
   /* Streaming sink, used by task_push_http_download_file(): the
    * body is written straight to disk as it arrives instead of being
    * accumulated and then written in one go.  NULL for every other
    * entry point. */
   RFILE *sink_file;
   char  *sink_path;
   enum http_status_enum status;
   bool error;
   bool headers_accept_err;
   bool sink_failed;
   /* Full request URL, owned.  This was a char[NAME_MAX_LENGTH]
    * filled by strlcpy, but task_http_finder() compares the stored
    * copy against the raw candidate URL -- so truncation on the
    * stored side alone meant that past NAME_MAX_LENGTH (256, or 128
    * on the small-path platforms) the two could never compare equal
    * and the "concurrent download of the same file is not allowed"
    * guard silently stopped guarding.  Playlist thumbnail URLs
    * exceed it routinely, and the result was two tasks reaching
    * cb_http_task_download_pl_thumbnail() and racing to
    * filestream_write_file() the same path.
    *
    * NULL means "no key", which matches nothing: an allocation
    * failure therefore degrades to admitting a redundant download
    * rather than dropping one, which is the safe direction -- a
    * dropped push strands the caller's completion callback forever. */
   char *connection_url;
};

typedef struct http_handle http_handle_t;

/* Sink callback: append the run of decoded body bytes to the output
 * file.  Returning false aborts the transfer, which is how a full
 * disk surfaces as a failed download rather than a truncated core. */
static bool task_http_file_sink(void *userdata, const void *data, size_t len)
{
   http_handle_t *http = (http_handle_t*)userdata;
   if (!http->sink_file)
      return false;
   if (filestream_write(http->sink_file, data, (int64_t)len) != (int64_t)len)
   {
      http->sink_failed = true;
      return false;
   }
   return true;
}

/* Close the output file, removing it unless the transfer succeeded.
 * A partial file left behind would otherwise be indistinguishable
 * from a good one on the next run. */
static void task_http_sink_close(http_handle_t *http, bool keep)
{
   if (http->sink_file)
   {
      filestream_close(http->sink_file);
      http->sink_file = NULL;
   }
   if (http->sink_path)
   {
      if (!keep)
         filestream_delete(http->sink_path);
      free(http->sink_path);
      http->sink_path = NULL;
   }
}

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

   /* Set http->error on this path too.  It used to just return -1,
    * and the caller (task_http_conn_iterate_transfer_parse) discards
    * the return value -- so the task advanced to HTTP_STATUS_TRANSFER
    * with a NULL handle, net_http_update(NULL, ...) returned true
    * immediately, and the task finished with neither data nor an
    * error.  Callers saw a clean success with task_data == NULL. */
   if (!network_init())
   {
      http->error = true;
      return -1;
   }

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
      default:
         break;
   }

   if (http->error)
      goto task_finished;

   return;
task_finished:
   task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);

   if (http->sink_path)
   {
      /* Keep the file only on a clean, non-cancelled 2xx. */
      bool ok = http->handle
             && !http->error
             && !http->sink_failed
             && !net_http_error(http->handle)
             && ((flg & RETRO_TASK_FLG_CANCELLED) == 0);
      task_http_sink_close(http, ok);
      if (!ok && !task_get_error(task))
         task_set_error(task, strldup("Download failed.",
               sizeof("Download failed.")));
   }

   if (http->handle)
   {
      size_t _len = 0;
      char   *tmp = (char*)net_http_data(http->handle, &_len, false);

      if (!tmp)
         tmp = (char*)net_http_data(http->handle, &_len, true);

      if ((flg & RETRO_TASK_FLG_CANCELLED) > 0)
      {
         if (tmp)
            free(tmp);

         task_set_error(task,
               strldup("Task cancelled.", sizeof("Task cancelled.")));
      }
      else
      {
         data          = (http_transfer_data_t*)malloc(sizeof(*data));
         /* NULL-check: the field writes below NULL-deref on OOM.
          * Free the already-fetched 'tmp' buffer (which data was
          * about to take ownership of), set a task error so the
          * caller sees a clean failure, and skip the task_set_data
          * attachment. */
         if (!data)
         {
            if (tmp)
               free(tmp);
            task_set_error(task, strldup("Out of memory.",
                  sizeof("Out of memory.")));
         }
         else
         {
            data->data    = tmp;
            data->len     = _len;
            data->headers = net_http_headers_ex(http->handle, http->headers_accept_err);
            data->status  = net_http_status(http->handle);

            task_set_data(task, data);

            /* RETRO_TASK_FLG_MUTE must not gate this.  MUTE suppresses
             * the on-screen notification only, in
             * task_queue_push_progress(); task->error is a separate
             * channel handed to the callback unconditionally.  Gating
             * it here quietened nothing and deleted the only failure
             * signal muted transfers had -- and muted is the common
             * case for internal traffic.  task_core_updater.c's list
             * callback computes `data && (!err || !*err)`, which a
             * failed DNS lookup satisfied, so an unreachable host
             * looked like a successfully downloaded empty core list. */
            if (net_http_error(http->handle))
               task_set_error(task, strldup("Download failed.",
                  sizeof("Download failed.")));
         }
      }
      net_http_delete(http->handle);
   }
   else if (http->error)
      task_set_error(task, strldup("Internal error.",
               sizeof("Internal error.")));

   /* Deliberately NOT freeing `http` here.
    *
    * task->state stays live until the task queue retires the task,
    * because a task remains *findable* long after its handler has
    * finished: retro_task_threaded_find() scans the running, finished
    * AND retiring lists, and task_http_finder() dereferences
    * task->state on every candidate it is handed.  Freeing the handle
    * at FINISHED time left task->state dangling for the whole window
    * between the last handler tick and full retirement -- many frames
    * -- so any concurrent push racing a finishing download read freed
    * memory.
    *
    * It went unnoticed while connection_url was an inline array,
    * since reading a freed-but-still-mapped char[] usually yields
    * stale bytes and strcmp() survives.  Once connection_url became a
    * pointer, the same read picked up the freed chunk's tombstone and
    * strcmp() dereferenced it: a hard SIGSEGV inside task_http_finder.
    *
    * The teardown now happens in task_http_transfer_cleanup(), which
    * the queue calls during retirement. */
   http->handle = NULL;
}

static void task_http_transfer_cleanup(retro_task_t *task)
{
   http_transfer_data_t* data = (http_transfer_data_t*)task_get_data(task);
   http_handle_t        *http = (http_handle_t*)task->state;

   if (data)
   {
      string_list_free(data->headers);
      if (data->data)
         free(data->data);
      free(data);
   }

   /* Release the handle here rather than in the handler; see the note
    * at the end of task_http_transfer_handler().  task->state is
    * cleared first so that a finder which somehow still reaches this
    * task sees no state rather than a stale pointer. */
   if (http)
   {
      task->state = NULL;
      /* Normally a no-op: the handler already closed the sink with
       * the correct keep/discard decision and NULLed both fields.
       * `false` covers the path where the handler never ran, where
       * a partial file must not be left behind. */
      task_http_sink_close(http, false);
      free(http->connection_url);
      free(http);
   }
}

static bool task_http_finder(retro_task_t *task, void *user_data)
{
   http_handle_t *http = NULL;
   if (task && (task->handler == task_http_transfer_handler) && user_data)
      if ((http = (http_handle_t*)task->state))
         return http->connection_url
             && string_is_equal(http->connection_url, (const char*)user_data);
   return false;
}

static void *task_push_http_transfer_generic_titled(
      struct http_connection_t *conn,
      const char *url, bool mute, bool headers_accept_err,
      const char *title, const char *sink_path,
      retro_task_callback_t cb, void *user_data)
{
   retro_task_t  *t        = NULL;
   http_handle_t *http     = NULL;
   const char    *method   = NULL;

   if (!conn)
      return NULL;

   method = net_http_connection_method(conn);

   /* net_http_connection_new() permits a NULL method, so
    * net_http_connection_method() may legitimately return NULL here.
    * Treat that as a hard error: we cannot dispatch a request without
    * a method, and the GET fast-path below would deref NULL. */
   if (!method)
      goto error;

   /* POST requests usually mutate the server, so assume multiple calls are
    * intended, even if they're duplicated. Additionally, they may differ
    * only by the POST data, and task_http_finder doesn't look at that, so
    * unique requests could be misclassified as duplicates.
    */
   if (string_is_equal(method, "GET"))
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
   http->headers_accept_err = headers_accept_err;
   http->sink_file           = NULL;
   http->sink_path           = NULL;
   http->sink_failed         = false;
   http->connection_url      = strdup(url);

   /* Streaming to disk: open the output now, so a bad path fails the
    * push rather than surfacing megabytes later, and arm the sink
    * before net_http_new() consumes the connection in
    * cb_http_conn_default(). */
   if (sink_path)
   {
      if (!(http->sink_path = strdup(sink_path)))
         goto error;
      if (!(http->sink_file = filestream_open(sink_path,
                  RETRO_VFS_FILE_ACCESS_WRITE,
                  RETRO_VFS_FILE_ACCESS_HINT_NONE)))
         goto error;
      net_http_connection_set_sink(conn, task_http_file_sink, http);
   }

   if (!(t = task_init()))
      goto error;

   t->handler              = task_http_transfer_handler;
   t->state                = http;
   t->callback             = cb;
   t->progress_cb          = task_window_progress_cb;
   t->cleanup              = task_http_transfer_cleanup;
   t->user_data            = user_data;
   t->progress             = -1;
   t->flags               |=  RETRO_TASK_FLG_ALTERNATIVE_LOOK;
   if (mute)
      t->flags            |=  RETRO_TASK_FLG_MUTE;
   else
      t->flags            &= ~RETRO_TASK_FLG_MUTE;

   /* Set t->title BEFORE task_queue_push().  Once the task is on the
    * queue it can be picked up, run, finished, and freed by a worker
    * thread before this function returns; any later write to t->* is
    * a use-after-free.  This bit callers like task_push_http_transfer_file()
    * which previously assigned t->title after the push -- if a download
    * failed instantly (DNS failure, ENETUNREACH, cancelled task) the
    * worker could finalise and free t before t->title was written,
    * giving glibc a stale pointer to free or strdup later. */
   if (title)
      t->title             = strdup(title);

   task_queue_push(t);

   return t;

error:
   if (conn)
      net_http_connection_free(conn);
   if (http)
   {
      task_http_sink_close(http, false);
      free(http->connection_url);
      free(http);
   }

   return NULL;
}

static void *task_push_http_transfer_generic(
      struct http_connection_t *conn,
      const char *url, bool mute, bool headers_accept_err,
      retro_task_callback_t cb, void *user_data)
{
   return task_push_http_transfer_generic_titled(
         conn, url, mute, headers_accept_err, NULL, NULL, cb, user_data);
}

void* task_push_http_transfer(const char *url, bool mute,
      const char *type,
      retro_task_callback_t cb, void *user_data)
{
   if (url && *url)
      return task_push_http_transfer_generic(
            net_http_connection_new(url, type ? type : "GET", NULL),
            url, mute, false, cb, user_data);
   return NULL;
}

void *task_push_webdav_stat(const char *url, bool mute, const char *headers,
      retro_task_callback_t cb, void *user_data)
{
   struct http_connection_t *conn;

   if (!url || !*url)
      return NULL;

   if (!(conn = net_http_connection_new(url, "OPTIONS", NULL)))
      return NULL;

   if (headers)
      net_http_connection_set_headers(conn, headers);

   return task_push_http_transfer_generic(conn, url, mute, false, cb, user_data);
}

void* task_push_webdav_mkdir(const char *url, bool mute,
      const char *headers,
      retro_task_callback_t cb, void *user_data)
{
   struct http_connection_t *conn;

   if (!url || !*url)
      return NULL;

   if (!(conn = net_http_connection_new(url, "MKCOL", NULL)))
      return NULL;

   if (headers)
      net_http_connection_set_headers(conn, headers);

   return task_push_http_transfer_generic(conn, url, mute, false, cb, user_data);
}

void* task_push_webdav_put(const char *url,
      const void *put_data, size_t len, bool mute,
      const char *headers, retro_task_callback_t cb, void *user_data)
{
   struct http_connection_t *conn;
   char                      expect[1024]; /* TODO/FIXME - check size */
   size_t                    _len;

   if (!url || !*url)
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

   return task_push_http_transfer_generic(conn, url, mute, false, cb, user_data);
}

void* task_push_webdav_delete(const char *url, bool mute,
      const char *headers,
      retro_task_callback_t cb, void *user_data)
{
   struct http_connection_t *conn;

   if (!url || !*url)
      return NULL;

   if (!(conn = net_http_connection_new(url, "DELETE", NULL)))
      return NULL;

   if (headers)
      net_http_connection_set_headers(conn, headers);

   return task_push_http_transfer_generic(conn, url, mute, false, cb, user_data);
}

void *task_push_webdav_move(const char *url,
      const char *dest, bool mute, const char *headers,
      retro_task_callback_t cb, void *userdata)
{
   size_t _len;
   struct http_connection_t *conn;
   char dest_header[PATH_MAX_LENGTH + 512];

   if (!url || !*url)
      return NULL;

   if (!(conn = net_http_connection_new(url, "MOVE", NULL)))
      return NULL;

   _len  = strlcpy(dest_header, "Destination: ", sizeof(dest_header));
   _len += strlcpy(dest_header + _len, dest,   sizeof(dest_header) - _len);
   _len += strlcpy(dest_header + _len, "\r\n", sizeof(dest_header) - _len);

   if (headers)
      strlcpy(dest_header + _len, headers, sizeof(dest_header) - _len);

   net_http_connection_set_headers(conn, dest_header);

   return task_push_http_transfer_generic(conn, url, mute, false, cb, userdata);
}

void* task_push_http_transfer_file(const char* url, bool mute,
      const char* type,
      retro_task_callback_t cb, file_transfer_t* transfer_data)
{
   size_t _len;
   const char *s               = NULL;
   char tmp[NAME_MAX_LENGTH]   = "";

   if (!url || !*url)
      return NULL;

   /* Build the task title BEFORE pushing to the queue.  See the
    * comment in task_push_http_transfer_generic_titled() -- writing
    * t->title after the push is a use-after-free if the worker
    * picks up and finalises the task before the write lands. */
   if (transfer_data)
      s        = transfer_data->path;
   else
      s        = url;
   _len = 0;
   strlcpy_append(tmp, sizeof(tmp), &_len,
         msg_hash_to_str(MSG_DOWNLOADING));
   strlcpy_append(tmp, sizeof(tmp), &_len, ": ");

   if (string_ends_with_size(s, ".index",
            strlen(s), STRLEN_CONST(".index")))
      s       = msg_hash_to_str(MSG_INDEX_FILE);

   strlcpy_append(tmp, sizeof(tmp), &_len, s);

   /* should be using type but some callers now rely on type being ignored */
   return task_push_http_transfer_generic_titled(
         net_http_connection_new(url, "GET", NULL),
         url, mute, false, tmp, NULL, cb, transfer_data);
}

void* task_push_http_transfer_with_user_agent(const char *url, bool mute,
   const char *type, const char *user_agent,
   retro_task_callback_t cb, void *user_data)
{
   struct http_connection_t *conn;

   if (!url || !*url)
      return NULL;

   if (!(conn = net_http_connection_new(url, type ? type : "GET", NULL)))
      return NULL;

   if (user_agent)
      net_http_connection_set_user_agent(conn, user_agent);

   return task_push_http_transfer_generic(conn, url, mute, false, cb, user_data);
}

void* task_push_http_transfer_with_headers(const char *url, bool mute,
   const char *type, const char *headers,
   retro_task_callback_t cb, void *user_data)
{
   struct http_connection_t *conn;

   if (!url || !*url)
      return NULL;

   if (!(conn = net_http_connection_new(url, type ? type : "GET", NULL)))
      return NULL;

   if (headers)
      net_http_connection_set_headers(conn, headers);

   return task_push_http_transfer_generic(conn, url, mute, false, cb, user_data);
}

void* task_push_http_post_transfer(const char *url,
      const char *post_data, bool mute,
      const char *type, retro_task_callback_t cb, void *user_data)
{
   if (url && *url)
      return task_push_http_transfer_generic(
            net_http_connection_new(url, type ? type : "POST", post_data),
            url, mute, false, cb, user_data);
   return NULL;
}

void* task_push_http_post_transfer_with_user_agent(const char *url,
   const char *post_data, bool mute,
   const char *type, const char *user_agent,
   retro_task_callback_t cb, void *user_data)
{
   struct http_connection_t* conn;

   if (!url || !*url)
      return NULL;

   if (!(conn = net_http_connection_new(url, type ? type : "POST", post_data)))
      return NULL;

   if (user_agent)
      net_http_connection_set_user_agent(conn, user_agent);

   return task_push_http_transfer_generic(conn, url, mute, false, cb, user_data);
}

void* task_push_http_post_transfer_with_headers(const char *url,
   const char *post_data, bool mute,
   const char *type, const char *headers,
   retro_task_callback_t cb, void *user_data)
{
   struct http_connection_t* conn;

   if (!url || !*url)
      return NULL;

   if (!(conn = net_http_connection_new(url, type ? type : "POST", post_data)))
      return NULL;

   if (headers)
      net_http_connection_set_headers(conn, headers);

   return task_push_http_transfer_generic(conn, url, mute, false, cb, user_data);
}

void *task_push_http_transfer_with_content(const char *url,
      const char *method, const void *content, size_t content_len,
      const char *content_type, bool mute, bool headers_accept_err,
      const char *headers, retro_task_callback_t cb, void *user_data)
{
   struct http_connection_t *conn;

   if (!url || !*url)
      return NULL;

   if (!(conn = net_http_connection_new(url, method, NULL)))
      return NULL;

   if (content && content_len)
      net_http_connection_set_content(conn, content_type,
            content_len, content);

   if (headers)
      net_http_connection_set_headers(conn, headers);

   return task_push_http_transfer_generic(conn, url, mute,
         headers_accept_err, cb, user_data);
}

/**
 * task_push_http_download_file:
 *
 * Download @url straight to @path, streaming the body to disk as it
 * arrives instead of accumulating it in RAM and writing once at the
 * end.  Peak memory is the receive window rather than the payload,
 * which for core and asset downloads is the difference between tens
 * of kilobytes and up to NET_HTTP_MAX_CONTENT_LENGTH.
 *
 * The completion callback receives an http_transfer_data_t with
 * status and headers populated but data == NULL and len == 0 -- the
 * body is already on disk.  The partial file is removed unless the
 * transfer finished cleanly with a 2xx.
 *
 * The output directory must exist; this does not create it.
 **/
void *task_push_http_download_file(const char *url, const char *path,
      bool mute, const char *title,
      retro_task_callback_t cb, void *user_data)
{
   if (!url || !*url || !path || !*path)
      return NULL;
   return task_push_http_transfer_generic_titled(
         net_http_connection_new(url, "GET", NULL),
         url, mute, false, title, path, cb, user_data);
}
