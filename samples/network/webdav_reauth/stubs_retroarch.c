/* The frontend and task-queue symbols network/cloud_sync/webdav.c
 * reaches for. Unlike the webdav_options harness, every request the
 * driver starts is recorded rather than refused: the test plays the
 * server, feeding each recorded request's callback the response it
 * chooses. Signatures are copied from the tree's own headers. */

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>

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

stub_request_t stub_req;

void stub_reset(void)
{
   free(stub_req.put_data);
   memset(&stub_req, 0, sizeof(stub_req));
}

/* Any non-NULL handle: callers only test it for NULL. */
static void *stub_record(const char *method, const char *headers,
      retro_task_callback_t cb, void *user_data)
{
   stub_req.count++;
   stub_req.method    = method;
   stub_req.had_auth  = (headers != NULL);
   stub_req.cb        = cb;
   stub_req.user_data = user_data;
   return &stub_req;
}

void *task_push_webdav_stat(const char *url, bool head,
      const char *headers, retro_task_callback_t cb, void *user_data)
{
   (void)url; (void)head;
   return stub_record("OPTIONS", headers, cb, user_data);
}

void *task_push_webdav_mkdir(const char *url, bool suppress,
      const char *headers, retro_task_callback_t cb, void *user_data)
{
   (void)url; (void)suppress;
   return stub_record("MKCOL", headers, cb, user_data);
}

void *task_push_webdav_put(const char *url, const void *data, size_t len,
      bool suppress, const char *headers, retro_task_callback_t cb,
      void *user_data)
{
   (void)url; (void)suppress;
   /* The driver frees its buffer as soon as this returns, so keep a
    * copy of what would have gone on the wire. */
   free(stub_req.put_data);
   stub_req.put_data = (char*)malloc(len ? len : 1);
   if (stub_req.put_data && len)
      memcpy(stub_req.put_data, data, len);
   stub_req.put_len = len;
   return stub_record("PUT", headers, cb, user_data);
}

void *task_push_webdav_delete(const char *url, bool suppress,
      const char *headers, retro_task_callback_t cb, void *user_data)
{
   (void)url; (void)suppress;
   return stub_record("DELETE", headers, cb, user_data);
}

void *task_push_http_transfer_with_headers(const char *url, bool mute,
      const char *type, const char *headers,
      retro_task_callback_t cb, void *user_data)
{
   (void)url; (void)mute; (void)type;
   return stub_record("GET", headers, cb, user_data);
}

void *task_push_webdav_move(const char *url, const char *dest,
      bool suppress, const char *headers, retro_task_callback_t cb,
      void *user_data)
{
   (void)url; (void)dest; (void)suppress;
   return stub_record("MOVE", headers, cb, user_data);
}

void *task_push_webdav_copy(const char *url, const char *dest,
      bool suppress, const char *headers, retro_task_callback_t cb,
      void *user_data)
{
   (void)url; (void)dest; (void)suppress;
   return stub_record("COPY", headers, cb, user_data);
}
