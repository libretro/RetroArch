/* Compile-only stand-in for PSL1GHT's <sys/event_queue.h>. */
#ifndef PS3STUB_SYS_EVENT_QUEUE_H
#define PS3STUB_SYS_EVENT_QUEUE_H

#include <stdint.h>

typedef uint32_t sys_event_queue_t;
typedef uint32_t sys_ipc_key_t;

typedef struct
{
   uint64_t source;
   uint64_t data_1;
   uint64_t data_2;
   uint64_t data_3;
} sys_event_t;

int sysEventQueueReceive(sys_event_queue_t id, sys_event_t *event,
      uint32_t timeout);

#endif
