/* The frontend and task-queue symbols network/cloud_sync/webdav.c
 * reaches for. The probe under test touches none of them - it reads a
 * header list and nothing else - but they have to resolve for the
 * translation unit to link. Signatures are copied from the tree's own
 * headers rather than guessed, and any that a future probe change
 * starts calling will fail loudly here rather than silently doing
 * nothing. */

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

#include <boolean.h>

#include "../../../configuration.h"
#include "../../../tasks/tasks_internal.h"

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

static void unreachable(const char *what)
{
   fprintf(stderr, "stub %s reached; the probe should not perform I/O\n",
         what);
   abort();
}

/* The OPTIONS probe is the one piece of I/O the driver is expected to
 * start, so this stub records the call rather than aborting on it.
 * webdav_stub_stat_auth says whether an Authorization header went with
 * it, which is what separates an anonymous probe from an authenticated
 * one. */
unsigned webdav_stub_stat_calls = 0;
bool     webdav_stub_stat_auth  = false;

void webdav_stub_reset(void)
{
   webdav_stub_stat_calls = 0;
   webdav_stub_stat_auth  = false;
}

void *task_push_webdav_stat(const char *url, bool head,
      const char *headers, retro_task_callback_t cb, void *user_data)
{
   (void)url; (void)head; (void)cb;
   webdav_stub_stat_calls++;
   webdav_stub_stat_auth = (headers != NULL);
   /* The caller owns this until the task completes; nothing here does,
    * so release it now and keep the harness leak-clean under ASan. */
   free(user_data);
   return NULL;
}

void *task_push_webdav_mkdir(const char *url, bool suppress,
      const char *headers, retro_task_callback_t cb, void *user_data)
{
   (void)url; (void)suppress; (void)headers; (void)cb; (void)user_data;
   unreachable("task_push_webdav_mkdir");
   return NULL;
}

void *task_push_webdav_put(const char *url, const void *data, size_t len,
      bool suppress, const char *headers, retro_task_callback_t cb,
      void *user_data)
{
   (void)url; (void)data; (void)len; (void)suppress; (void)headers;
   (void)cb; (void)user_data;
   unreachable("task_push_webdav_put");
   return NULL;
}

void *task_push_webdav_delete(const char *url, bool suppress,
      const char *headers, retro_task_callback_t cb, void *user_data)
{
   (void)url; (void)suppress; (void)headers; (void)cb; (void)user_data;
   unreachable("task_push_webdav_delete");
   return NULL;
}

void *task_push_http_transfer_with_headers(const char *url, bool mute,
      const char *type, const char *headers,
      retro_task_callback_t cb, void *user_data)
{
   (void)url; (void)mute; (void)type; (void)headers; (void)cb;
   (void)user_data;
   unreachable("task_push_http_transfer_with_headers");
   return NULL;
}

void *task_push_webdav_move(const char *url, const char *dest,
      bool suppress, const char *headers, retro_task_callback_t cb,
      void *user_data)
{
   (void)url; (void)dest; (void)suppress; (void)headers; (void)cb;
   (void)user_data;
   unreachable("task_push_webdav_move");
   return NULL;
}

void *task_push_webdav_copy(const char *url, const char *dest,
      bool suppress, const char *headers, retro_task_callback_t cb,
      void *user_data)
{
   (void)url; (void)dest; (void)suppress; (void)headers; (void)cb;
   (void)user_data;
   unreachable("task_push_webdav_copy");
   return NULL;
}
