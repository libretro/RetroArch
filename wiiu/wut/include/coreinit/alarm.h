#pragma once
#include <wut.h>
#include "thread.h"
#include "threadqueue.h"
#include "time.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*OSAlarmCallback)(OSAlarm *, OSContext *);

#define OS_ALARM_QUEUE_TAG 0x614C6D51u

typedef struct OSAlarmQueue
{
   //! Should always be set to the value OS_ALARM_QUEUE_TAG.
   uint32_t tag;

   //! Name set by OSInitAlarmQueueEx
   const char *name;
   uint32_t __unknown;

   OSThreadQueue threadQueue;
   OSAlarm *head;
   OSAlarm *tail;
}OSAlarmQueue;

typedef struct OSAlarmLink
{
   struct OSAlarm *prev;
   struct OSAlarm *next;
}OSAlarmLink;

#define OS_ALARM_TAG 0x614C724Du
typedef struct OSAlarm
{
   //! Should always be set to the value OS_ALARM_TAG.
   uint32_t tag;

   //! Name set from OSCreateAlarmEx.
   const char *name;

   uint32_t __unknown;

   //! The callback to execute once the alarm is triggered.
   OSAlarmCallback callback;

   //! Used with OSCancelAlarms for bulk cancellation of alarms.
   uint32_t group;

   uint32_t __unknown;

   //! The time when the alarm will next be triggered.
   OSTime nextFire;

   //! Link used for when this OSAlarm object is inside an OSAlarmQueue
   OSAlarmLink link;

   //! The period between alarm triggers, this is only set for periodic alarms.
   OSTime period;

   //! The time the alarm was started.
   OSTime start;

   //! User data set with OSSetAlarmUserData and retrieved with OSGetAlarmUserData.
   void *userData;

   //! The current state of the alarm, internal values.
   uint32_t state;

   //! Queue of threads currently waiting for the alarm to trigger with OSWaitAlarm.
   OSThreadQueue threadQueue;

   //! The queue that this alarm is currently in.
   OSAlarmQueue *alarmQueue;

   //! The context the alarm was triggered on.
   OSContext *context;
}OSAlarm;

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
