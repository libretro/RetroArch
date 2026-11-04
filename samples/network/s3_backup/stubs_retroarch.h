#ifndef S3_BACKUP_STUBS_H
#define S3_BACKUP_STUBS_H

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
   /* A streamed body, drained by the stub as net_http would: its
    * SHA-256 in hex, and whether it gave exactly body_len bytes, a
    * replay from the start included, and then its end */
   char body_sha[65];
   bool body_streamed;
   bool body_whole;
   bool overlapped;
   retro_task_callback_t cb;
   void *user_data;
} stub_request_t;

extern stub_request_t stub_pending;
extern bool           stub_has_pending;

#endif
