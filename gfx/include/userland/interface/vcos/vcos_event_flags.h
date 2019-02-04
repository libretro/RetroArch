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
VideoCore OS Abstraction Layer - public header file
=============================================================================*/

#ifndef VCOS_EVENT_FLAGS_H
#define VCOS_EVENT_FLAGS_H


#ifdef __cplusplus
extern "C" {
#endif

#include "interface/vcos/vcos_types.h"
#include "vcos.h"

#define VCOS_EVENT_FLAGS_SUSPEND    VCOS_SUSPEND
#define VCOS_EVENT_FLAGS_NO_SUSPEND VCOS_NO_SUSPEND
typedef VCOS_OPTION VCOS_EVENTGROUP_OPERATION_T;

/**
 * \file vcos_event_flags.h
 *
 * Defines event flags API.
 *
 * Similar to Nucleus event groups.
 *
 * These have the same semantics as Nucleus event groups and ThreadX event
 * flags. As such, they are quite complex internally; if speed is important
 * they might not be your best choice.
 *
 */

/**
 * Create an event flags instance.
 *
 * @param flags   Pointer to event flags instance, filled in on return.
 * @param name    Name for the event flags, used for debug.
 *
 * @return VCOS_SUCCESS if succeeded.
 */

VCOS_INLINE_DECL
VCOS_STATUS_T vcos_event_flags_create(VCOS_EVENT_FLAGS_T *flags, const char *name);

/**
  * Set some events.
  *
  * @param flags   Instance to set flags on
  * @param events  Bitmask of the flags to actually set
  * @param op      How the flags should be set. VCOS_OR will OR in the flags; VCOS_AND
  *                will AND them in, possibly clearing existing flags.
  */
VCOS_INLINE_DECL
void vcos_event_flags_set(VCOS_EVENT_FLAGS_T *flags,
                          VCOS_UNSIGNED events,
                          VCOS_OPTION op);

/**
 * Retrieve some events.
 *
 * Waits until the specified events have been set.
 *
 * @param flags            Instance to wait on
 * @param requested_events The bitmask to wait for
 * @param op               VCOS_OR - get any; VCOS_AND - get all.
 * @param ms_suspend       How long to wait, in milliseconds
 * @param retrieved_events the events actually retrieved.
 *
 * @return VCOS_SUCCESS if events were retrieved. VCOS_EAGAIN if the
 * timeout expired.
 */
VCOS_INLINE_DECL
VCOS_STATUS_T vcos_event_flags_get(VCOS_EVENT_FLAGS_T *flags,
                                                     VCOS_UNSIGNED requested_events,
                                                     VCOS_OPTION op,
                                                     VCOS_UNSIGNED ms_suspend,
                                                     VCOS_UNSIGNED *retrieved_events);


/**
 * Delete an event flags instance.
 */
VCOS_INLINE_DECL
void vcos_event_flags_delete(VCOS_EVENT_FLAGS_T *);

#ifdef __cplusplus
}
#endif

#endif

