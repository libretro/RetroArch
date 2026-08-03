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

#ifndef __EMSCRIPTEN__
#error "task_http_emscripten only makes sense in emscripten builds"
#endif

/* Emscripten HTTP task backend.
 *
 * This used to sit on emscripten_async_wget2_data(), whose signature
 * is the reason the file had drifted so far from tasks/task_http.c:
 * it takes the request body as a single NUL-terminated `const char*`
 * and has no provision at all for request headers, a user agent, or
 * reading response headers back.  Everything the native path offers
 * beyond "GET a URL into memory" was therefore silently dropped or
 * stubbed out with an error log.
 *
 * emscripten_fetch() covers all of it: binary request bodies with an
 * explicit length, arbitrary request headers, the real response
 * status, and the response headers.  It needs -sFETCH at link time,
 * which Makefile.emscripten now passes.
 */

#include <stdlib.h>
#include <string.h>

#include "verbosity.h"
#include <emscripten/emscripten.h>
#include <emscripten/fetch.h>

#include <string/stdstring.h>
#include <compat/strl.h>
#include <file/file_path.h>
#include <streams/file_stream.h>
#include <lists/string_list.h>
#include <retro_timers.h>
#include <retro_miscellaneous.h>

#include "task_file_transfer.h"
#include "tasks_internal.h"

struct http_handle
{
   /* In-flight fetch, NULL once the transfer has settled or been
    * aborted.  emscripten_fetch does not copy requestData or
    * requestHeaders, so this struct owns both for the fetch's
    * lifetime. */
   emscripten_fetch_t   *fetch;
   char                **req_headers;
   char                 *req_data;
   size_t                req_len;
   http_transfer_data_t *response;
   /* Destination for task_push_http_download_file().  The browser
    * buffers the whole response either way, so unlike the native
    * backend this is not a memory optimisation -- it exists so the
    * two backends honour the same API contract: body on disk,
    * data == NULL in the callback. */
   char                 *sink_path;
   bool                  settled;
   /* Full request URL, owned; see the matching comment in
    * tasks/task_http.c.  A fixed buffer truncated the stored copy but
    * not the candidate task_http_finder() compares it against, so the
    * duplicate-download guard stopped working past NAME_MAX_LENGTH.
    * NULL matches nothing, so OOM admits a redundant download rather
    * than dropping one. */
   char                 *connection_url;
};

typedef struct http_handle http_handle_t;

/* ------------------------------------------------------------------ */
/* Header marshalling                                                  */
/* ------------------------------------------------------------------ */

static void http_req_headers_free(char **h)
{
   size_t i;
   if (!h)
      return;
   for (i = 0; h[i]; i++)
      free(h[i]);
   free(h);
}

/* RetroArch passes request headers around as one raw CRLF-delimited
 * blob ("Key: Value\r\nKey2: Value2\r\n"), which the native path
 * writes straight onto the wire.  emscripten_fetch instead wants a
 * NULL-terminated array of alternating key/value C strings, so split
 * the blob.  Malformed and blank lines are skipped rather than sent
 * as-is. */
static char **http_req_headers_parse(const char *blob)
{
   const char *p   = blob;
   size_t      cap = 8;
   size_t      n   = 0;
   char      **out;

   if (!blob || !*blob)
      return NULL;

   if (!(out = (char**)calloc(cap + 1, sizeof(char*))))
      return NULL;

   while (*p)
   {
      const char *eol;
      const char *colon;
      const char *v;
      size_t      klen, vlen;
      char       *k;

      /* Tolerate a bare LF as well as CRLF; some of these blobs are
       * built by hand. */
      if (!(eol = strchr(p, '\n')))
         eol = p + strlen(p);

      if (!(colon = (const char*)memchr(p, ':', (size_t)(eol - p))))
      {
         p = (*eol) ? eol + 1 : eol;
         continue;
      }

      klen = (size_t)(colon - p);
      v    = colon + 1;
      while (v < eol && (*v == ' ' || *v == '\t'))
         v++;
      vlen = (size_t)(eol - v);
      while (vlen && (v[vlen - 1] == '\r' || v[vlen - 1] == ' '))
         vlen--;

      if (!klen)
      {
         p = (*eol) ? eol + 1 : eol;
         continue;
      }

      if (n + 2 > cap)
      {
         char **tmp;
         size_t ncap = cap * 2;
         if (!(tmp = (char**)realloc(out, (ncap + 1) * sizeof(char*))))
            goto error;
         memset(tmp + cap + 1, 0, (ncap - cap) * sizeof(char*));
         out = tmp;
         cap = ncap;
      }

      if (!(k = (char*)malloc(klen + 1)))
         goto error;
      memcpy(k, p, klen);
      k[klen] = '\0';
      out[n++] = k;

      if (!(k = (char*)malloc(vlen + 1)))
         goto error;
      memcpy(k, v, vlen);
      k[vlen] = '\0';
      out[n++] = k;

      p = (*eol) ? eol + 1 : eol;
   }

   out[n] = NULL;

   if (!n)
   {
      http_req_headers_free(out);
      return NULL;
   }

   return out;

error:
   out[n] = NULL;
   http_req_headers_free(out);
   return NULL;
}

/* Turn the fetch's response headers into the same string_list of
 * "Name: Value" lines that net_http.c produces, so consumers such as
 * network/cloud_sync/webdav.c behave identically on both backends.
 *
 * Browsers normalise response header names to lower case, so these
 * arrive as "www-authenticate: Digest ..." where the native path
 * gives "WWW-Authenticate: ...".  Header names are case-insensitive
 * per RFC 9110, so the consumers were what needed fixing. */
static struct string_list *http_response_headers(emscripten_fetch_t *fetch)
{
   union string_list_elem_attr attr;
   struct string_list *list;
   size_t  len;
   char   *raw;
   char   *p;

   if (!(len = emscripten_fetch_get_response_headers_length(fetch)))
      return NULL;

   if (!(raw = (char*)malloc(len + 1)))
      return NULL;

   emscripten_fetch_get_response_headers(fetch, raw, len + 1);
   raw[len] = '\0';

   if (!(list = string_list_new()))
   {
      free(raw);
      return NULL;
   }

   attr.i = 0;
   p      = raw;

   while (*p)
   {
      char *eol = strchr(p, '\n');
      char *end;

      if (!eol)
         eol = p + strlen(p);
      end = eol;
      while (end > p && (end[-1] == '\r' || end[-1] == ' '))
         end--;

      if (end > p)
      {
         char save = *end;
         *end      = '\0';
         string_list_append(list, p, attr);
         *end      = save;
      }

      p = (*eol) ? eol + 1 : eol;
   }

   free(raw);
   return list;
}

/* ------------------------------------------------------------------ */
/* Task plumbing                                                       */
/* ------------------------------------------------------------------ */

static void http_handle_free(http_handle_t *http)
{
   if (!http)
      return;
   /* Aborts the transfer if it is still running.  emscripten_fetch
    * guarantees no further callbacks after close, which is what makes
    * freeing the handle immediately afterwards safe.  The wget2 path
    * never aborted at all on cancel. */
   if (http->fetch)
   {
      emscripten_fetch_close(http->fetch);
      http->fetch = NULL;
   }
   http_req_headers_free(http->req_headers);
   free(http->req_data);
   free(http->sink_path);
   free(http->connection_url);
   free(http);
}

static void task_http_transfer_handler(retro_task_t *task)
{
   http_handle_t *http = (http_handle_t*)task->state;
   uint8_t        flg  = task_get_flags(task);

   if ((flg & RETRO_TASK_FLG_CANCELLED) > 0)
      goto task_finished;

   if (http->settled)
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
         bool mute      = ((task->flags & RETRO_TASK_FLG_MUTE) > 0);
         int  status    = http->response->status;
         task_set_data(task, http->response);
         http->response = NULL;
         if (!mute && status >= 400)
            task_set_error(task, strldup("Download failed.",
               sizeof("Download failed.")));
      }
   }
   else if ((flg & RETRO_TASK_FLG_CANCELLED) > 0)
      task_set_error(task,
            strldup("Task cancelled.", sizeof("Task cancelled.")));
   else
      task_set_error(task,
            strldup("Internal error.", sizeof("Internal error.")));

   /* Handle freed in task_http_transfer_cleanup(), not here: the task
    * stays findable until the queue retires it, and
    * task_http_finder() dereferences task->state.  See the matching
    * note in tasks/task_http.c. */
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

   if (http)
   {
      task->state = NULL;
      http_handle_free(http);
   }
}

/* ------------------------------------------------------------------ */
/* fetch callbacks                                                     */
/* ------------------------------------------------------------------ */

static void http_fetch_settle(emscripten_fetch_t *fetch)
{
   retro_task_t         *task = (retro_task_t*)fetch->userData;
   http_handle_t        *http = (http_handle_t*)task->state;
   http_transfer_data_t *resp;

   http->fetch = NULL;

   if ((resp = (http_transfer_data_t*)malloc(sizeof(*resp))))
   {
      resp->data    = NULL;
      resp->len     = 0;
      resp->status  = (int)fetch->status;
      resp->headers = http_response_headers(fetch);

      /* fetch->data belongs to the fetch and dies with
       * emscripten_fetch_close(), so it has to be copied out.  The
       * old wget2 path handed the callback's buffer straight to the
       * caller, which worked only because wget2 transferred
       * ownership; fetch does not. */
      if (http->sink_path)
      {
         /* Write straight through; the caller gets status and headers
          * only.  A failed or non-2xx transfer leaves no file behind,
          * matching the native backend. */
         if (     fetch->status >= 200 && fetch->status <= 299
               && fetch->numBytes > 0 && fetch->data)
         {
            if (!filestream_write_file(http->sink_path, fetch->data,
                     (int64_t)fetch->numBytes))
            {
               RARCH_ERR("[HTTP] Failed writing %s\n", http->sink_path);
               resp->status = -1;
            }
         }
         else
            filestream_delete(http->sink_path);
      }
      else if (fetch->numBytes > 0 && fetch->data)
      {
         if ((resp->data = (char*)malloc((size_t)fetch->numBytes)))
         {
            memcpy(resp->data, fetch->data, (size_t)fetch->numBytes);
            resp->len = (size_t)fetch->numBytes;
         }
         else
            /* Keep the status and headers so the caller still sees a
             * coherent, if empty, response. */
            RARCH_ERR("[HTTP] Out of memory buffering %llu byte response.\n",
                  (unsigned long long)fetch->numBytes);
      }

      http->response = resp;
   }
   else
      RARCH_ERR("[HTTP] Out of memory allocating response.\n");

   http->settled = true;

   /* Unconditional.  The old onload handler returned early on malloc
    * failure and leaked the entire response buffer. */
   emscripten_fetch_close(fetch);
}

static void http_fetch_onsuccess(emscripten_fetch_t *fetch)
{
   http_fetch_settle(fetch);
}

static void http_fetch_onerror(emscripten_fetch_t *fetch)
{
   /* Settle with the real status rather than discarding it.  The old
    * onerror path recorded nothing, so callers that branch on
    * data->status -- webdav's 401 digest challenge, the thumbnail
    * downloader's 404 check -- got a NULL task_data and could not
    * tell "not found" from "the network died".  Error bodies are kept
    * too, since 4xx/5xx responses routinely carry one. */
   http_fetch_settle(fetch);
}

static void http_fetch_onprogress(emscripten_fetch_t *fetch)
{
   retro_task_t *task = (retro_task_t*)fetch->userData;
   uint64_t      pos  = fetch->dataOffset + fetch->numBytes;
   uint64_t      tot  = fetch->totalBytes;

   if (!task)
      return;

   if (tot == 0)
      task_set_progress(task, -1);
   else if (pos < (((uint64_t)-1) / 100))
      /* prefer multiply then divide for more accurate results */
      task_set_progress(task, (signed)(pos * 100 / tot));
   else
      /* but invert the logic if it would cause an overflow */
      task_set_progress(task, MIN((signed)(pos / (tot / 100)), 100));
}

/* ------------------------------------------------------------------ */

static void *task_push_http_transfer_generic(
      const char *url, const char *method,
      const void *data, size_t data_len, const char *user_agent,
      const char *headers, bool mute, const char *title,
      retro_task_callback_t cb, void *user_data)
{
   retro_task_t            *t    = NULL;
   http_handle_t           *http = NULL;
   emscripten_fetch_attr_t  attr;

   if (!url || !*url || !method)
      return NULL;

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
         return NULL;
   }

   if (!(http = (http_handle_t*)calloc(1, sizeof(*http))))
      return NULL;

   http->connection_url = strdup(url);

   /* Own a copy of the request body.  emscripten_fetch keeps the
    * pointer rather than copying it, so a caller's stack buffer, or
    * one it frees on return, would be read after the fact.  The
    * length travels explicitly, which is the fix for
    * task_push_webdav_put(): the old path passed put_data as a
    * NUL-terminated string, truncating any binary payload at its
    * first zero byte. */
   if (data && data_len)
   {
      if (!(http->req_data = (char*)malloc(data_len)))
         goto error;
      memcpy(http->req_data, data, data_len);
      http->req_len = data_len;
   }

   http->req_headers = http_req_headers_parse(headers);

   if (user_agent)
      /* Browsers refuse to let script set User-Agent; the request
       * carries the browser's own.  Say so once rather than pretend
       * the argument took effect. */
      RARCH_DBG("[HTTP] User agent \"%s\" ignored: the browser controls "
            "this header.\n", user_agent);

   if (!(t = task_init()))
      goto error;

   t->handler     = task_http_transfer_handler;
   t->state       = http;
   t->callback    = cb;
   t->progress_cb = task_window_progress_cb;
   t->cleanup     = task_http_transfer_cleanup;
   t->user_data   = user_data;
   t->progress    = -1;
   t->flags      |= RETRO_TASK_FLG_ALTERNATIVE_LOOK;
   if (mute)
      t->flags   |=  RETRO_TASK_FLG_MUTE;
   else
      t->flags   &= ~RETRO_TASK_FLG_MUTE;

   /* Set t->title BEFORE task_queue_push(), matching task_http.c.
    * Once queued, the task can be picked up, finished and freed
    * before this function returns, so any later write to t->* is a
    * use-after-free.  task_push_http_transfer_file() used to assign
    * t->title after the push -- harmless single-threaded, a genuine
    * race under -pthread/PROXY_TO_PTHREAD. */
   if (title)
      t->title = strdup(title);

   emscripten_fetch_attr_init(&attr);
   strlcpy(attr.requestMethod, method, sizeof(attr.requestMethod));
   attr.attributes      = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY
                        | EMSCRIPTEN_FETCH_REPLACE;
   attr.userData        = t;
   attr.onsuccess       = http_fetch_onsuccess;
   attr.onerror         = http_fetch_onerror;
   attr.onprogress      = http_fetch_onprogress;
   attr.requestHeaders  = (const char* const*)http->req_headers;
   attr.requestData     = http->req_data;
   attr.requestDataSize = http->req_len;

   /* Queue before dispatching.  The callbacks reach the task through
    * fetch->userData and cannot run until this function yields to the
    * browser event loop, but pushing first keeps the task visible for
    * the whole life of the fetch. */
   task_queue_push(t);

   if (!(http->fetch = emscripten_fetch(&attr, url)))
      /* Could not even start.  Mark it settled so the handler
       * finalises the task on its next tick; the task owns `http`
       * from here, so it must not be freed on this path. */
      http->settled = true;

   return t;

error:
   http_handle_free(http);
   return NULL;
}

void* task_push_http_transfer(const char *url, bool mute,
      const char *type,
      retro_task_callback_t cb, void *user_data)
{
   return task_push_http_transfer_generic(url, type ? type : "GET",
         NULL, 0, NULL, NULL, mute, NULL, cb, user_data);
}

void *task_push_webdav_stat(const char *url, bool mute, const char *headers,
      retro_task_callback_t cb, void *user_data)
{
   return task_push_http_transfer_generic(url, "OPTIONS", NULL, 0, NULL,
         headers, mute, NULL, cb, user_data);
}

void* task_push_webdav_mkdir(const char *url, bool mute,
      const char *headers,
      retro_task_callback_t cb, void *user_data)
{
   return task_push_http_transfer_generic(url, "MKCOL", NULL, 0, NULL,
         headers, mute, NULL, cb, user_data);
}

void* task_push_webdav_put(const char *url,
      const void *put_data, size_t len, bool mute,
      const char *headers, retro_task_callback_t cb, void *user_data)
{
   /* No "Expect: 100-continue" here, unlike task_http.c: the browser
    * owns the request/response dance and rejects the header as a
    * forbidden name. */
   return task_push_http_transfer_generic(url, "PUT", put_data, len, NULL,
         headers, mute, NULL, cb, user_data);
}

void* task_push_webdav_delete(const char *url, bool mute,
      const char *headers,
      retro_task_callback_t cb, void *user_data)
{
   return task_push_http_transfer_generic(url, "DELETE", NULL, 0, NULL,
         headers, mute, NULL, cb, user_data);
}

void *task_push_webdav_move(const char *url,
      const char *dest, bool mute, const char *headers,
      retro_task_callback_t cb, void *user_data)
{
   size_t _len;
   char dest_header[PATH_MAX_LENGTH + 512];

   _len  = strlcpy(dest_header, "Destination: ", sizeof(dest_header));
   _len += strlcpy(dest_header + _len, dest,   sizeof(dest_header) - _len);
   _len += strlcpy(dest_header + _len, "\r\n", sizeof(dest_header) - _len);

   if (headers)
      strlcpy(dest_header + _len, headers, sizeof(dest_header) - _len);

   return task_push_http_transfer_generic(url, "MOVE", NULL, 0, NULL,
         dest_header, mute, NULL, cb, user_data);
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

   /* Build the title BEFORE pushing; see the comment in
    * task_push_http_transfer_generic(). */
   if (transfer_data)
      s = transfer_data->path;
   else
      s = url;

   _len = 0;
   strlcpy_append(tmp, sizeof(tmp), &_len,
         msg_hash_to_str(MSG_DOWNLOADING));
   strlcpy_append(tmp, sizeof(tmp), &_len, ": ");

   if (string_ends_with_size(s, ".index",
            strlen(s), STRLEN_CONST(".index")))
      s = msg_hash_to_str(MSG_INDEX_FILE);

   strlcpy_append(tmp, sizeof(tmp), &_len, s);

   /* should be using type but some callers now rely on type being ignored */
   return task_push_http_transfer_generic(url, "GET", NULL, 0, NULL, NULL,
         mute, tmp, cb, transfer_data);
}

void* task_push_http_transfer_with_user_agent(const char *url, bool mute,
   const char *type, const char *user_agent,
   retro_task_callback_t cb, void *user_data)
{
   return task_push_http_transfer_generic(url, type ? type : "GET", NULL, 0,
         user_agent, NULL, mute, NULL, cb, user_data);
}

void* task_push_http_transfer_with_headers(const char *url, bool mute,
   const char *type, const char *headers,
   retro_task_callback_t cb, void *user_data)
{
   return task_push_http_transfer_generic(url, type ? type : "GET", NULL, 0,
         NULL, headers, mute, NULL, cb, user_data);
}

void* task_push_http_post_transfer(const char *url,
      const char *post_data, bool mute,
      const char *type, retro_task_callback_t cb, void *user_data)
{
   return task_push_http_transfer_generic(url, type ? type : "POST",
         post_data, post_data ? strlen(post_data) : 0,
         NULL, NULL, mute, NULL, cb, user_data);
}

void* task_push_http_post_transfer_with_user_agent(const char *url,
   const char *post_data, bool mute,
   const char *type, const char *user_agent,
   retro_task_callback_t cb, void *user_data)
{
   return task_push_http_transfer_generic(url, type ? type : "POST",
         post_data, post_data ? strlen(post_data) : 0,
         user_agent, NULL, mute, NULL, cb, user_data);
}

void* task_push_http_post_transfer_with_headers(const char *url,
   const char *post_data, bool mute,
   const char *type, const char *headers,
   retro_task_callback_t cb, void *user_data)
{
   return task_push_http_transfer_generic(url, type ? type : "POST",
         post_data, post_data ? strlen(post_data) : 0,
         NULL, headers, mute, NULL, cb, user_data);
}

void *task_push_http_transfer_with_content(const char *url,
      const char *method, const void *content, size_t content_len,
      const char *content_type, bool mute, bool headers_accept_err,
      const char *headers, retro_task_callback_t cb, void *user_data)
{
   char   hdr[1024];
   size_t _len = 0;

   /* Content-Type arrives as its own argument on this entry point but
    * is just another header to fetch, so fold it in. */
   hdr[0] = '\0';
   if (content_type && *content_type)
   {
      _len += strlcpy(hdr + _len, "Content-Type: ", sizeof(hdr) - _len);
      _len += strlcpy(hdr + _len, content_type,     sizeof(hdr) - _len);
      _len += strlcpy(hdr + _len, "\r\n",           sizeof(hdr) - _len);
   }
   if (headers)
      strlcpy(hdr + _len, headers, sizeof(hdr) - _len);

   return task_push_http_transfer_generic(url, method, content, content_len,
         NULL, *hdr ? hdr : NULL, mute, NULL, cb, user_data);
}

void *task_push_http_download_file(const char *url, const char *path,
      bool mute, const char *title,
      retro_task_callback_t cb, void *user_data)
{
   retro_task_t  *t;
   http_handle_t *http;

   if (!url || !*url || !path || !*path)
      return NULL;

   if (!(t = (retro_task_t*)task_push_http_transfer_generic(url, "GET",
               NULL, 0, NULL, NULL, mute, title, cb, user_data)))
      return NULL;

   /* Safe to reach into the handle here: the fetch cannot have
    * settled yet, since its callbacks only run once this function
    * yields to the browser event loop. */
   if ((http = (http_handle_t*)t->state))
      http->sink_path = strdup(path);

   return t;
}
