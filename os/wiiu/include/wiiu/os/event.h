#pragma once
#include <wiiu/types.h>
#include "thread.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum OSEventMode
{
   OS_EVENT_MODE_MANUAL    = 0,
   OS_EVENT_MODE_AUTO      = 1,
} OSEventMode;

#define OS_EVENT_TAG 0x65566E54u
typedef struct
{
   uint32_t tag;
   const char *name;
   uint32_t __unknown;
   BOOL value;
   OSThreadQueue queue;
   OSEventMode mode;
} OSEvent;

void OSInitEvent(OSEvent *event, BOOL value, OSEventMode mode);
void OSInitEventEx(OSEvent *event, BOOL value, OSEventMode mode, char *name);
void OSSignalEvent(OSEvent *event);
void OSSignalEventAll(OSEvent *event);
void OSWaitEvent(OSEvent *event);
void OSResetEvent(OSEvent *event);
BOOL OSWaitEventWithTimeout(OSEvent *event, OSTime timeout);

#ifdef __cplusplus
}
#endif
