#pragma once
#include <wut.h>

/**
 * \defgroup coreinit_msgq Message Queue
 * \ingroup coreinit
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OSMessage OSMessage;
typedef struct OSMessageQueue OSMessageQueue;

typedef enum OSMessageFlags
{
   OS_MESSAGE_QUEUE_BLOCKING        = 1 << 0,
   OS_MESSAGE_QUEUE_HIGH_PRIORITY   = 1 << 1,
} OSMessageFlags;

struct OSMessage
{
   void *message;
   uint32_t args[3];
};
CHECK_OFFSET(OSMessage, 0x00, message);
CHECK_OFFSET(OSMessage, 0x04, args);
CHECK_SIZE(OSMessage, 0x10);

#define OS_MESSAGE_QUEUE_TAG 0x6D536751u

struct OSMessageQueue
{
   uint32_t tag;
   const char *name;
   UNKNOWN(4);
   OSThreadQueue sendQueue;
   OSThreadQueue recvQueue;
   OSMessage *messages;
   uint32_t size;
   uint32_t first;
   uint32_t used;
};
CHECK_OFFSET(OSMessageQueue, 0x00, tag);
CHECK_OFFSET(OSMessageQueue, 0x04, name);
CHECK_OFFSET(OSMessageQueue, 0x0c, sendQueue);
CHECK_OFFSET(OSMessageQueue, 0x1c, recvQueue);
CHECK_OFFSET(OSMessageQueue, 0x2c, messages);
CHECK_OFFSET(OSMessageQueue, 0x30, size);
CHECK_OFFSET(OSMessageQueue, 0x34, first);
CHECK_OFFSET(OSMessageQueue, 0x38, used);
CHECK_SIZE(OSMessageQueue, 0x3c);

void
OSInitMessageQueue(OSMessageQueue *queue,
                   OSMessage *messages,
                   int32_t size);

void
OSInitMessageQueueEx(OSMessageQueue *queue,
                     OSMessage *messages,
                     int32_t size,
                     const char *name);

BOOL
OSSendMessage(OSMessageQueue *queue,
              OSMessage *message,
              OSMessageFlags flags);

BOOL
OSReceiveMessage(OSMessageQueue *queue,
                 OSMessage *message,
                 OSMessageFlags flags);

BOOL
OSPeekMessage(OSMessageQueue *queue,
              OSMessage *message);

OSMessageQueue *
OSGetSystemMessageQueue();

#ifdef __cplusplus
}
#endif

/** @} */
