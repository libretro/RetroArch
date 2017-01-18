#pragma once
#include <wut.h>
#include "thread.h"
#include "threadqueue.h"
#include "time.h"

/**
 * \defgroup coreinit_alarms Alarms
 * \ingroup coreinit
 *
 * The alarm family of functions are used for creating alarms which call
 * a callback or wake up waiting threads after a period of time.
 *
 * Alarms can be one shot alarms which trigger once after a period of time,
 * or periodic which trigger at regular intervals until they are cancelled.
 *
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OSAlarm OSAlarm;
typedef struct OSAlarmLink OSAlarmLink;
typedef struct OSAlarmQueue OSAlarmQueue;

typedef void (*OSAlarmCallback)(OSAlarm *, OSContext *);

#define OS_ALARM_QUEUE_TAG 0x614C6D51u

struct OSAlarmQueue
{
   //! Should always be set to the value OS_ALARM_QUEUE_TAG.
   uint32_t tag;

   //! Name set by OSInitAlarmQueueEx
   const char *name;
   UNKNOWN(4);

   OSThreadQueue threadQueue;
   OSAlarm *head;
   OSAlarm *tail;
};
CHECK_OFFSET(OSAlarmQueue, 0x00, tag);
CHECK_OFFSET(OSAlarmQueue, 0x04, name);
CHECK_OFFSET(OSAlarmQueue, 0x0c, threadQueue);
CHECK_OFFSET(OSAlarmQueue, 0x1c, head);
CHECK_OFFSET(OSAlarmQueue, 0x20, tail);
CHECK_SIZE(OSAlarmQueue, 0x24);

struct OSAlarmLink
{
   OSAlarm *prev;
   OSAlarm *next;
};
CHECK_OFFSET(OSAlarmLink, 0x00, prev);
CHECK_OFFSET(OSAlarmLink, 0x04, next);
CHECK_SIZE(OSAlarmLink, 0x08);

#define OS_ALARM_TAG 0x614C724Du
struct OSAlarm
{
   //! Should always be set to the value OS_ALARM_TAG.
   uint32_t tag;

   //! Name set from OSCreateAlarmEx.
   const char *name;

   UNKNOWN(4);

   //! The callback to execute once the alarm is triggered.
   OSAlarmCallback callback;

   //! Used with OSCancelAlarms for bulk cancellation of alarms.
   uint32_t group;

   UNKNOWN(4);

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
};
CHECK_OFFSET(OSAlarm, 0x00, tag);
CHECK_OFFSET(OSAlarm, 0x04, name);
CHECK_OFFSET(OSAlarm, 0x0c, callback);
CHECK_OFFSET(OSAlarm, 0x10, group);
CHECK_OFFSET(OSAlarm, 0x18, nextFire);
CHECK_OFFSET(OSAlarm, 0x20, link);
CHECK_OFFSET(OSAlarm, 0x28, period);
CHECK_OFFSET(OSAlarm, 0x30, start);
CHECK_OFFSET(OSAlarm, 0x38, userData);
CHECK_OFFSET(OSAlarm, 0x3c, state);
CHECK_OFFSET(OSAlarm, 0x40, threadQueue);
CHECK_OFFSET(OSAlarm, 0x50, alarmQueue);
CHECK_OFFSET(OSAlarm, 0x54, context);
CHECK_SIZE(OSAlarm, 0x58);


/**
 * Cancel an alarm.
 */
BOOL
OSCancelAlarm(OSAlarm *alarm);


/**
 * Cancel all alarms which have a matching tag set by OSSetAlarmTag.
 *
 * \param group The alarm tag to cancel.
 */
void
OSCancelAlarms(uint32_t group);


/**
 * Initialise an alarm structure.
 */
void
OSCreateAlarm(OSAlarm *alarm);


/**
 * Initialise an alarm structure with a name.
 */
void
OSCreateAlarmEx(OSAlarm *alarm,
                const char *name);


/**
 * Return user data set by OSSetAlarmUserData.
 */
void *
OSGetAlarmUserData(OSAlarm *alarm);


/**
 * Initialise an alarm queue structure.
 */
void
OSInitAlarmQueue(OSAlarmQueue *queue);


/**
 * Initialise an alarm queue structure with a name.
 */
void
OSInitAlarmQueueEx(OSAlarmQueue *queue,
                   const char *name);


/**
 * Set a one shot alarm to perform a callback after a set amount of time.
 *
 * \param alarm The alarm to set.
 * \param time The duration until the alarm should be triggered.
 * \param callback The alarm callback to call when the alarm is triggered.
 */
BOOL
OSSetAlarm(OSAlarm *alarm,
           OSTime time,
           OSAlarmCallback callback);


/**
 * Set a repeated alarm to execute a callback every interval from start.
 *
 * \param alarm The alarm to set.
 * \param start The duration until the alarm should first be triggered.
 * \param interval The interval between triggers after the first trigger.
 * \param callback The alarm callback to call when the alarm is triggered.
 */
BOOL
OSSetPeriodicAlarm(OSAlarm *alarm,
                   OSTime start,
                   OSTime interval,
                   OSAlarmCallback callback);


/**
 * Set an alarm tag which is used in OSCancelAlarms for bulk cancellation.
 */
void
OSSetAlarmTag(OSAlarm *alarm,
              uint32_t group);


/**
 * Set alarm user data which is returned by OSGetAlarmUserData.
 */
void
OSSetAlarmUserData(OSAlarm *alarm,
                   void *data);


/**
 * Sleep the current thread until the alarm has been triggered or cancelled.
 */
BOOL
OSWaitAlarm(OSAlarm *alarm);

#ifdef __cplusplus
}
#endif

/** @} */
