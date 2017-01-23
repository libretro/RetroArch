#pragma once
#include <wiiu/types.h>
#include "thread.h"
#include "time.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OSAlarm OSAlarm;

typedef void (*OSAlarmCallback)(OSAlarm *, OSContext *);

#define OS_ALARM_QUEUE_TAG 0x614C6D51u
typedef struct
{
   uint32_t tag;
   const char *name;
   uint32_t __unknown;
   OSThreadQueue threadQueue;
   OSAlarm *head;
   OSAlarm *tail;
} OSAlarmQueue;

typedef struct
{
   OSAlarm *prev;
   OSAlarm *next;
} OSAlarmLink;

#define OS_ALARM_TAG 0x614C724Du
typedef struct OSAlarm
{
   uint32_t tag;
   const char *name;
   uint32_t __unknown0;
   OSAlarmCallback callback;
   uint32_t group;
   uint32_t __unknown1;
   OSTime nextFire;
   OSAlarmLink link;
   OSTime period;
   OSTime start;
   void *userData;
   uint32_t state;
   OSThreadQueue threadQueue;
   OSAlarmQueue *alarmQueue;
   OSContext *context;
} OSAlarm;

void OSCreateAlarm(OSAlarm *alarm);
void OSCreateAlarmEx(OSAlarm *alarm, const char *name);
void OSSetAlarmUserData(OSAlarm *alarm, void *data);
void *OSGetAlarmUserData(OSAlarm *alarm);
void OSInitAlarmQueue(OSAlarmQueue *queue);
void OSInitAlarmQueueEx(OSAlarmQueue *queue, const char *name);
BOOL OSSetAlarm(OSAlarm *alarm, OSTime time, OSAlarmCallback callback);
BOOL OSSetPeriodicAlarm(OSAlarm *alarm, OSTime start, OSTime interval, OSAlarmCallback callback);
void OSSetAlarmTag(OSAlarm *alarm, uint32_t group);
BOOL OSCancelAlarm(OSAlarm *alarm);
void OSCancelAlarms(uint32_t group);
BOOL OSWaitAlarm(OSAlarm *alarm);

#ifdef __cplusplus
}
#endif
