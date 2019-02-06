/*
Copyright (c) 2012, Broadcom Europe Ltd
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/*=============================================================================
VideoCore OS Abstraction Layer - public header file for events
=============================================================================*/

#ifndef VCOS_EVENT_H
#define VCOS_EVENT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "interface/vcos/vcos_types.h"
#include "vcos.h"

/**
  * \file
  *
  * An event is akin to the Win32 auto-reset event.
  *
  *
  * Signalling an event will wake up one waiting thread only. Once one
  * thread has been woken the event atomically returns to the unsignalled
  * state.
  *
  * If no threads are waiting on the event when it is signalled it remains
  * signalled.
  *
  * This is almost, but not quite, completely unlike the "event flags"
  * object based on Nucleus event groups and ThreadX event flags.
  *
  * In particular, it should be similar in speed to a semaphore, unlike
  * the event flags.
  */

/**
  * Create an event instance.
  *
  * @param event  Filled in with constructed event.
  * @param name   Name of the event (for debugging)
  *
  * @return VCOS_SUCCESS on success, or error code.
  */
VCOS_INLINE_DECL
VCOS_STATUS_T vcos_event_create(VCOS_EVENT_T *event, const char *name);

#ifndef vcos_event_signal

/**
  * Signal the event. The event will return to being unsignalled
  * after exactly one waiting thread has been woken up. If no
  * threads are waiting it remains signalled.
  *
  * @param event The event to signal
  */
VCOS_INLINE_DECL
void vcos_event_signal(VCOS_EVENT_T *event);

/**
  * Wait for the event.
  *
  * @param event The event to wait for
  * @return VCOS_SUCCESS on success, VCOS_EAGAIN if the wait was interrupted.
  */
VCOS_INLINE_DECL
VCOS_STATUS_T vcos_event_wait(VCOS_EVENT_T *event);

/**
  * Try event, but don't block.
  *
  * @param event The event to try
  * @return VCOS_SUCCESS on success, VCOS_EAGAIN if the event is not currently signalled
  */
VCOS_INLINE_DECL
VCOS_STATUS_T vcos_event_try(VCOS_EVENT_T *event);

#endif

/*
 * Destroy an event.
 */
VCOS_INLINE_DECL
void vcos_event_delete(VCOS_EVENT_T *event);

#ifdef __cplusplus
}
#endif

#endif

