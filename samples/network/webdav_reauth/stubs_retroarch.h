#ifndef WEBDAV_REAUTH_STUBS_H
#define WEBDAV_REAUTH_STUBS_H

#include <stddef.h>
#include <boolean.h>

#include "../../../tasks/tasks_internal.h"

/* The most recent request the driver started, and how many it has
 * started since stub_reset(). */
typedef struct
{
   unsigned count;
   const char *method;
   /* A download's file, which the real task streams the body into;
    * NULL for any other request. The driver's own string, valid until
    * the request is answered. */
   const char *sink_path;
   bool had_auth;
   retro_task_callback_t cb;
   void *user_data;
   char *put_data;
   size_t put_len;
} stub_request_t;

extern stub_request_t stub_req;

void stub_reset(void);

#endif
