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
VideoCore OS Abstraction Layer - event flags implemented via a semaphore
=============================================================================*/

#ifndef VCOS_GENERIC_EVENT_FLAGS_H
#define VCOS_GENERIC_EVENT_FLAGS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "interface/vcos/vcos_types.h"

/**
  * \file
  *
  * This provides event flags (as per Nucleus Event Groups) based on a
  * mutex, a semaphore (per waiting thread) and a timer (per waiting
  * thread).
  * 
  * The data structure is a 32 bit unsigned int (the current set of
  * flags) and a linked list of clients waiting to be 'satisfied'.
  *
  * The mutex merely locks access to the data structure. If a client
  * calls vcos_event_flags_get() and the requested bits are not already
  * present, it then sleeps on its per-thread semaphore after adding
  * this semaphore to the queue waiting. It also sets up a timer.
  *
  * The per-thread semaphore and timer are actually stored in the
  * thread context (joinable thread). In future it may become necessary
  * to support non-VCOS threads by using thread local storage to
  * create these objects and associate them with the thread.
  */

struct VCOS_EVENT_WAITER_T;

typedef struct VCOS_EVENT_FLAGS_T
{
   VCOS_UNSIGNED events;      /**< Events currently set */
   VCOS_MUTEX_T lock;         /**< Serialize access */
   struct
   {
      struct VCOS_EVENT_WAITER_T *head;   /**< List of threads waiting */
      struct VCOS_EVENT_WAITER_T *tail;   /**< List of threads waiting */
   } waiters;
} VCOS_EVENT_FLAGS_T;

#define VCOS_OR      1
#define VCOS_AND     2
#define VCOS_CONSUME 4
#define VCOS_OR_CONSUME (VCOS_OR | VCOS_CONSUME)
#define VCOS_AND_CONSUME (VCOS_AND | VCOS_CONSUME)
#define VCOS_EVENT_FLAG_OP_MASK (VCOS_OR|VCOS_AND)

VCOSPRE_  VCOS_STATUS_T VCOSPOST_ vcos_generic_event_flags_create(VCOS_EVENT_FLAGS_T *flags, const char *name);
VCOSPRE_  void VCOSPOST_ vcos_generic_event_flags_set(VCOS_EVENT_FLAGS_T *flags,
                                                      VCOS_UNSIGNED events,
                                                      VCOS_OPTION op);
VCOSPRE_  void VCOSPOST_ vcos_generic_event_flags_delete(VCOS_EVENT_FLAGS_T *);
VCOSPRE_  VCOS_STATUS_T VCOSPOST_ vcos_generic_event_flags_get(VCOS_EVENT_FLAGS_T *flags,
                                                               VCOS_UNSIGNED requested_events,
                                                               VCOS_OPTION op,
                                                               VCOS_UNSIGNED suspend,
                                                               VCOS_UNSIGNED *retrieved_events);

#ifdef VCOS_INLINE_BODIES

VCOS_INLINE_IMPL
VCOS_STATUS_T vcos_event_flags_create(VCOS_EVENT_FLAGS_T *flags, const char *name) {
   return vcos_generic_event_flags_create(flags, name);
}

VCOS_INLINE_IMPL
void vcos_event_flags_set(VCOS_EVENT_FLAGS_T *flags,
                          VCOS_UNSIGNED events,
                          VCOS_OPTION op) {
   vcos_generic_event_flags_set(flags, events, op);
}

VCOS_INLINE_IMPL
void vcos_event_flags_delete(VCOS_EVENT_FLAGS_T *f) {
   vcos_generic_event_flags_delete(f);
}

VCOS_INLINE_IMPL
VCOS_STATUS_T vcos_event_flags_get(VCOS_EVENT_FLAGS_T *flags,
                                   VCOS_UNSIGNED requested_events,
                                   VCOS_OPTION op,
                                   VCOS_UNSIGNED suspend,
                                   VCOS_UNSIGNED *retrieved_events) {
   return vcos_generic_event_flags_get(flags, requested_events, op, suspend, retrieved_events);
}

#endif /* VCOS_INLINE_BODIES */

#ifdef __cplusplus
}
#endif
#endif

