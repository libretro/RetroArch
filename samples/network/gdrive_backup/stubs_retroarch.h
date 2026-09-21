#ifndef GDRIVE_BACKUP_STUBS_H
#define GDRIVE_BACKUP_STUBS_H

#include <stddef.h>
#include <boolean.h>

#include "../../../tasks/tasks_internal.h"

/* The request the driver has started and the test has not answered. */
typedef struct
{
   char method[16];
   char url[1024];
   char headers[2048];
   size_t body_len;
   char body[1024];
   bool overlapped;
   retro_task_callback_t cb;
   void *user_data;
} stub_request_t;

extern stub_request_t stub_pending;
extern bool           stub_has_pending;

#endif
