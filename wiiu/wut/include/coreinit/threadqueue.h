#pragma once
#include <wut.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OSThread OSThread;

typedef struct
{
   OSThread *prev;
   OSThread *next;
}OSThreadLink;

typedef struct
{
   OSThread *head;
   OSThread *tail;
   void *parent;
   uint32_t __unknown;
}OSThreadQueue;

typedef struct
{
   OSThread *head;
   OSThread *tail;
}OSThreadSimpleQueue;

void OSInitThreadQueue(OSThreadQueue *queue);
void OSInitThreadQueueEx(OSThreadQueue *queue, void *parent);

#ifdef __cplusplus
}
#endif
