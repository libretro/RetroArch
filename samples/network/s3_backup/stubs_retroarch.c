/* The frontend and task-queue symbols network/cloud_sync/s3.c reaches
 * for.  Each request the driver starts is recorded - method, URL,
 * headers, body length - for the test, which plays the server and
 * answers it.  Signatures are copied from the tree's own headers. */

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <compat/strl.h>
#include <lrc_hash.h>

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
   stub_pending.body_len      = body_len;
   stub_pending.body_streamed = false;
   stub_pending.body_whole    = false;
   stub_pending.body_sha[0]   = '\0';
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

/* Drains @source the way net_http does: a few bytes, then a replay
 * from the start (a pooled connection the server had closed), then the
 * whole body in odd-sized runs, and once more past its end. */
void *task_push_http_transfer_with_source(const char *url,
      const char *method, net_http_source_t source,
      net_http_source_rewind_t rewind, void *source_data, size_t len,
      const char *content_type, bool mute, bool headers_accept_err,
      const char *headers, retro_task_callback_t cb, void *user_data)
{
   static const char hex[] = "0123456789abcdef";
   static uint8_t     run[3001];
   struct sha256_state st;
   uint8_t  digest[32];
   size_t   total = 0;
   bool     whole = true;
   unsigned i;
   void    *ret;

   (void)content_type; (void)mute; (void)headers_accept_err;
   if (!source || !rewind)
      return NULL;
   if (len > 7 && source(source_data, run, 7) != 7)
      whole = false;
   if (!rewind(source_data))
      whole = false;

   sha256_stream_init(&st, 0);
   while (total < len)
   {
      size_t  want = len - total < sizeof(run) ? len - total : sizeof(run);
      int64_t got  = source(source_data, run, want);
      if (got <= 0 || (size_t)got > want)
      {
         whole = false;
         break;
      }
      sha256_stream_update(&st, run, (size_t)got);
      total += (size_t)got;
   }
   if (source(source_data, run, sizeof(run)) != 0)
      whole = false;
   sha256_stream_final(&st, digest);

   ret = stub_record(method, url, headers, len, cb, user_data);
   for (i = 0; i < 32; i++)
   {
      stub_pending.body_sha[2 * i]     = hex[digest[i] >> 4];
      stub_pending.body_sha[2 * i + 1] = hex[digest[i] & 15];
   }
   stub_pending.body_sha[64]  = '\0';
   stub_pending.body_streamed = true;
   stub_pending.body_whole    = whole && total == len;
   return ret;
}

void *task_push_http_post_transfer_with_headers(const char *url,
      const char *post_data, bool mute, const char *type,
      const char *headers, retro_task_callback_t cb, void *user_data)
{
   (void)post_data; (void)mute;
   return stub_record(type, url, headers, 0, cb, user_data);
}
