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
VideoCore OS Abstraction Layer - IRQ support
=============================================================================*/

#ifndef VCOS_ISR_H
#define VCOS_ISR_H

#ifdef __cplusplus
extern "C" {
#endif

#include "interface/vcos/vcos_types.h"
#include "vcos.h"

/**
  * \file vcos_isr.h
  *
  * \section isr ISR support
  *
  * API for dispatching interrupts.
  */

/**
  *
  * Register an interrupt handler. The old handler (if any) is returned in
  * old_handler. The old handler should be called if the interrupt was not
  * for you.
  *
  * The handler function will be called in a context with interrupts disabled,
  * so should be written to be as short as possible. If significant processing
  * is needed, the handler should delegate to a thread.
  *
  * The handler function can call any OS primitive that does not block (e.g.
  * post a semaphore or set an event flag). Blocking operations (including memory
  * allocation from the system heap) are not permitted.
  *
  * To deregister an ISR, pass in NULL.
  *
  * @param vec  Vector to register for
  * @param handler Handler to be called
  * @param old_handler Updated with the old handler, or NULL.
  */

VCOS_INLINE_DECL
void vcos_register_isr(VCOS_UNSIGNED vec,
                       VCOS_ISR_HANDLER_T handler,
                       VCOS_ISR_HANDLER_T *old_handler);

/** Disable interrupts, returning the old value (enabled/disabled) to the caller.
  */
VCOS_INLINE_DECL
VCOS_UNSIGNED vcos_int_disable(void);

/** Restore the previous interrupt enable/disable state.
  */
VCOS_INLINE_DECL
void vcos_int_restore(VCOS_UNSIGNED previous);

#ifdef __cplusplus
}
#endif
#endif

