#pragma once
#include <wiiu/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum OSMessageFlags
{
   OS_MESSAGE_QUEUE_BLOCKING        = 1 << 0,
   OS_MESSAGE_QUEUE_HIGH_PRIORITY   = 1 << 1,
} OSMessageFlags;

typedef struct OSMessage
{
   void *message;
   uint32_t args[3];
} OSMessage;

#define OS_MESSAGE_QUEUE_TAG 0x6D536751u
typedef struct OSMessageQueue
{
   uint32_t tag;
   const char *name;
   uint32_t __unknown;
   OSThreadQueue sendQueue;
   OSThreadQueue recvQueue;
   OSMessage *messages;
   uint32_t size;
   uint32_t first;
   uint32_t used;
} OSMessageQueue;

void OSInitMessageQueue(OSMessageQueue *queue, OSMessage *messages, int32_t size);
void OSInitMessageQueueEx(OSMessageQueue *queue, OSMessage *messages, int32_t size, const char *name);
BOOL OSSendMessage(OSMessageQueue *queue, OSMessage *message, OSMessageFlags flags);
BOOL OSReceiveMessage(OSMessageQueue *queue, OSMessage *message, OSMessageFlags flags);
BOOL OSPeekMessage(OSMessageQueue *queue, OSMessage *message);
OSMessageQueue *OSGetSystemMessageQueue();

#ifdef __cplusplus
}
#endif
