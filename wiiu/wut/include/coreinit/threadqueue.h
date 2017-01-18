#pragma once
#include <wut.h>

/**
 * \defgroup coreinit_threadq Thread Queue
 * \ingroup coreinit
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OSThread OSThread;

typedef struct OSThreadLink OSThreadLink;
typedef struct OSThreadQueue OSThreadQueue;
typedef struct OSThreadSimpleQueue OSThreadSimpleQueue;

struct OSThreadLink
{
   OSThread *prev;
   OSThread *next;
};
CHECK_OFFSET(OSThreadLink, 0x00, prev);
CHECK_OFFSET(OSThreadLink, 0x04, next);
CHECK_SIZE(OSThreadLink, 0x8);

struct OSThreadQueue
{
   OSThread *head;
   OSThread *tail;
   void *parent;
   UNKNOWN(4);
};
CHECK_OFFSET(OSThreadQueue, 0x00, head);
CHECK_OFFSET(OSThreadQueue, 0x04, tail);
CHECK_OFFSET(OSThreadQueue, 0x08, parent);
CHECK_SIZE(OSThreadQueue, 0x10);

struct OSThreadSimpleQueue
{
   OSThread *head;
   OSThread *tail;
};
CHECK_OFFSET(OSThreadSimpleQueue, 0x00, head);
CHECK_OFFSET(OSThreadSimpleQueue, 0x04, tail);
CHECK_SIZE(OSThreadSimpleQueue, 0x08);

void
OSInitThreadQueue(OSThreadQueue *queue);

void
OSInitThreadQueueEx(OSThreadQueue *queue,
                    void *parent);

#ifdef __cplusplus
}
#endif

/** @} */
