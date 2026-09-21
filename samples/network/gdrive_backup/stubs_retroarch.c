/* The frontend and task-queue symbols network/cloud_sync/google_drive.c reaches
 * for.  Each request the driver starts is recorded - method, URL,
 * headers, body length - for the test, which plays the server and
 * answers it.  Signatures are copied from the tree's own headers. */

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
      const char *headers, size_t body_len,
      retro_task_callback_t cb, void *user_data)
{
   stub_pending.body[0] = '\0';
   /* The driver starts one request at a time per file and waits for
    * its answer, so one slot is enough; a second start before the
    * answer is a driver bug the test reports. */
   if (stub_has_pending)
      stub_pending.overlapped = true;
   strlcpy(stub_pending.method, method ? method : "GET",
         sizeof(stub_pending.method));
   strlcpy(stub_pending.url, url ? url : "", sizeof(stub_pending.url));
   strlcpy(stub_pending.headers, headers ? headers : "",
         sizeof(stub_pending.headers));
   stub_pending.body_len  = body_len;
   stub_pending.cb        = cb;
   stub_pending.user_data = user_data;
   stub_has_pending       = true;
   return &stub_pending;
}

void *task_push_http_transfer_with_headers(const char *url, bool mute,
      const char *type, const char *headers,
      retro_task_callback_t cb, void *user_data)
{
   (void)mute;
   return stub_record(type, url, headers, 0, cb, user_data);
}

void *task_push_http_transfer_with_content(const char *url,
      const char *method, const void *content, size_t content_len,
      const char *content_type, bool mute, bool headers_accept_err,
      const char *headers, retro_task_callback_t cb, void *user_data)
{
   (void)content; (void)content_type; (void)mute; (void)headers_accept_err;
   return stub_record(method, url, headers, content_len, cb, user_data);
}

void *task_push_http_post_transfer_with_headers(const char *url,
      const char *post_data, bool mute, const char *type,
      const char *headers, retro_task_callback_t cb, void *user_data)
{
   void *ret;
   (void)mute;
   ret = stub_record(type ? type : "POST", url, headers,
         post_data ? strlen(post_data) : 0, cb, user_data);
   strlcpy(stub_pending.body, post_data ? post_data : "",
         sizeof(stub_pending.body));
   return ret;
}

/* The OAuth device flow drives the task queue directly; the upload
 * paths under test never reach it. */
static void unreachable(const char *what)
{
   fprintf(stderr, "stub %s reached; the upload paths should not start the OAuth flow\n", what);
   abort();
}

retro_task_t *task_init(void)
{
   unreachable("task_init");
   return NULL;
}

void task_set_flags(retro_task_t *task, uint8_t flags, bool set)
{
   (void)task; (void)flags; (void)set;
   unreachable("task_set_flags");
}

bool task_queue_push(retro_task_t *task)
{
   (void)task;
   unreachable("task_queue_push");
   return false;
}
