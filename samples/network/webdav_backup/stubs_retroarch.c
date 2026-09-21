/* The frontend and task-queue symbols network/cloud_sync/webdav.c
 * reaches for.  Every request the driver starts is appended to a
 * queue with its method, URL and destination; the test plays the
 * server, answering them one by one.  Signatures are copied from the
 * tree's own headers. */

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <compat/strl.h>

#include "../../../configuration.h"
#include "../../../tasks/tasks_internal.h"

#include "stubs_retroarch.h"

void RARCH_LOG(const char *fmt, ...)
{
   va_list ap;
   va_start(ap, fmt);
   vprintf(fmt, ap);
   va_end(ap);
}

void RARCH_WARN(const char *fmt, ...)
{
   va_list ap;
   va_start(ap, fmt);
   printf("WARN: ");
   vprintf(fmt, ap);
   va_end(ap);
}

void RARCH_ERR(const char *fmt, ...)
{
   va_list ap;
   va_start(ap, fmt);
   printf("ERR: ");
   vprintf(fmt, ap);
   va_end(ap);
}

void RARCH_DBG(const char *fmt, ...)
{
   va_list ap;
   va_start(ap, fmt);
   vprintf(fmt, ap);
   va_end(ap);
}

settings_t *config_get_ptr(void)
{
   static settings_t settings;
   return &settings;
}

stub_request_t stub_pending;
bool           stub_has_pending;

static void *stub_record(const char *method, const char *url,
      const char *dest, const char *headers,
      retro_task_callback_t cb, void *user_data)
{
   /* The driver starts one request at a time and waits for its
    * answer, so one slot is enough; a second start before the answer
    * would be a driver bug the test reports. */
   if (stub_has_pending)
      stub_pending.overlapped = true;
   stub_pending.method    = method;
   strlcpy(stub_pending.url, url ? url : "", sizeof(stub_pending.url));
   strlcpy(stub_pending.dest, dest ? dest : "", sizeof(stub_pending.dest));
   stub_pending.had_auth  = (headers != NULL);
   stub_pending.cb        = cb;
   stub_pending.user_data = user_data;
   stub_has_pending       = true;
   return &stub_pending;
}

void *task_push_webdav_stat(const char *url, bool head,
      const char *headers, retro_task_callback_t cb, void *user_data)
{
   (void)head;
   return stub_record("OPTIONS", url, NULL, headers, cb, user_data);
}

void *task_push_webdav_mkdir(const char *url, bool suppress,
      const char *headers, retro_task_callback_t cb, void *user_data)
{
   (void)suppress;
   return stub_record("MKCOL", url, NULL, headers, cb, user_data);
}

void *task_push_webdav_put(const char *url, const void *data, size_t len,
      bool suppress, const char *headers, retro_task_callback_t cb,
      void *user_data)
{
   (void)data; (void)len; (void)suppress;
   return stub_record("PUT", url, NULL, headers, cb, user_data);
}

void *task_push_webdav_delete(const char *url, bool suppress,
      const char *headers, retro_task_callback_t cb, void *user_data)
{
   (void)suppress;
   return stub_record("DELETE", url, NULL, headers, cb, user_data);
}

void *task_push_http_transfer_with_headers(const char *url, bool mute,
      const char *type, const char *headers,
      retro_task_callback_t cb, void *user_data)
{
   (void)mute; (void)type;
   return stub_record("GET", url, NULL, headers, cb, user_data);
}

void *task_push_webdav_move(const char *url, const char *dest,
      bool suppress, const char *headers, retro_task_callback_t cb,
      void *user_data)
{
   (void)suppress;
   return stub_record("MOVE", url, dest, headers, cb, user_data);
}

void *task_push_webdav_copy(const char *url, const char *dest,
      bool suppress, const char *headers, retro_task_callback_t cb,
      void *user_data)
{
   (void)suppress;
   return stub_record("COPY", url, dest, headers, cb, user_data);
}
