#ifndef WEBDAV_BACKUP_STUBS_H
#define WEBDAV_BACKUP_STUBS_H

#include <boolean.h>

#include "../../../tasks/tasks_internal.h"

/* The request the driver has started and the test has not answered. */
typedef struct
{
   const char *method;
   char url[512];
   char dest[512];
   bool had_auth;
   bool overlapped;
   retro_task_callback_t cb;
   void *user_data;
} stub_request_t;

extern stub_request_t stub_pending;
extern bool           stub_has_pending;

#endif
