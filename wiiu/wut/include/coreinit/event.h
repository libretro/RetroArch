#pragma once
#include <wut.h>
#include "thread.h"
#include "threadqueue.h"

/**
 * \defgroup coreinit_event Event Object
 * \ingroup coreinit
 *
 * Standard event object implementation. There are two supported event object modes, check OSEventMode.
 *
 * Similar to Windows <a href="https://msdn.microsoft.com/en-us/library/windows/desktop/ms682655(v=vs.85).aspx">Event Objects</a>.
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OSEvent OSEvent;

typedef enum OSEventMode
{
   //! A manual event will only reset when OSResetEvent is called.
   OS_EVENT_MODE_MANUAL    = 0,

   //! An auto event will reset everytime a thread is woken.
   OS_EVENT_MODE_AUTO      = 1,
} OSEventMode;

#define OS_EVENT_TAG 0x65566E54u

struct OSEvent
{
   //! Should always be set to the value OS_EVENT_TAG.
   uint32_t tag;

   //! Name set by OSInitEventEx.
   const char *name;

   UNKNOWN(4);

   //! The current value of the event object.
   BOOL value;

   //! The threads currently waiting on this event object with OSWaitEvent.
   OSThreadQueue queue;

   //! The mode of the event object, set by OSInitEvent.
   OSEventMode mode;
};
CHECK_OFFSET(OSEvent, 0x0, tag);
CHECK_OFFSET(OSEvent, 0x4, name);
CHECK_OFFSET(OSEvent, 0xc, value);
CHECK_OFFSET(OSEvent, 0x10, queue);
CHECK_OFFSET(OSEvent, 0x20, mode);
CHECK_SIZE(OSEvent, 0x24);


/**
 * Initialise an event object with value and mode.
 */
void
OSInitEvent(OSEvent *event,
            BOOL value,
            OSEventMode mode);


/**
 * Initialise an event object with value, mode and name.
 */
void
OSInitEventEx(OSEvent *event,
              BOOL value,
              OSEventMode mode,
              char *name);


/**
 * Signals the event.
 *
 * If no threads are waiting the event value is set.
 *
 * If the event mode is OS_EVENT_MODE_MANUAL this will wake all waiting threads
 * and the event will remain set until OSResetEvent is called.
 *
 * If the event mode is OS_EVENT_MODE_AUTO this will wake only one thread
 * and the event will be reset immediately.
 *
 * Similar to <a href="https://msdn.microsoft.com/en-us/library/windows/desktop/ms686211(v=vs.85).aspx">SetEvent</a>.
 */
void
OSSignalEvent(OSEvent *event);

/**
 * Signals all threads waiting on an event.
 *
 * If no threads are waiting the event value is set.
 *
 * If the event mode is OS_EVENT_MODE_MANUAL this will wake all waiting threads
 * and the event will remain set until OSResetEvent is called.
 *
 * If the event mode is OS_EVENT_MODE_AUTO this will wake all waiting threads
 * and the event will be reset.
 */
void
OSSignalEventAll(OSEvent *event);


/**
 * Wait until an event is signalled.
 *
 * If the event is already set, this returns immediately.
 *
 * If the event mode is OS_EVENT_MODE_AUTO the event will be reset before
 * returning from this method.
 *
 * Similar to <a href="https://msdn.microsoft.com/en-us/library/windows/desktop/ms687032(v=vs.85).aspx">WaitForSingleObject</a>.
 */
void
OSWaitEvent(OSEvent *event);


/**
 * Resets the event object.
 *
 * Similar to <a href="https://msdn.microsoft.com/en-us/library/windows/desktop/ms685081(v=vs.85).aspx">ResetEvent</a>.
 */
void
OSResetEvent(OSEvent *event);


/**
 * Wait until an event is signalled or a timeout has occurred.
 *
 * Similar to <a href="https://msdn.microsoft.com/en-us/library/windows/desktop/ms687032(v=vs.85).aspx">WaitForSingleObject</a>.
 */
BOOL
OSWaitEventWithTimeout(OSEvent *event,
                       OSTime timeout);

#ifdef __cplusplus
}
#endif

/** @} */
