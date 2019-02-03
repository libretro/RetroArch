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
VideoCore OS Abstraction Layer - legacy (Nucleus) IRQ support
=============================================================================*/

#ifndef VCOS_LEGACY_ISR_H
#define VCOS_LEGACY_ISR_H

#ifdef __cplusplus
extern "C" {
#endif

#include "interface/vcos/vcos_types.h"
#include "vcos.h"

/** \file vcos_legacy_isr.h
  *
  * API for dispatching interrupts the Nucleus way, via a LISR and HISR.
  * New code should use the single-dispatch scheme - the LISR/HISR
  * distinction is not necessary.
  *
  * Under ThreadX, a HISR is implemented as a high-priority thread which
  * waits on a counting semaphore to call the HISR function. Although this
  * provides a good approximation to the Nucleus semantics, it is potentially
  * slow if all you are trying to do is to wake a thread from LISR context.
  */

/** Register a LISR. This is identical to the NU_Register_LISR API.
  */
VCOS_INLINE_DECL
VCOS_STATUS_T vcos_register_legacy_lisr(VCOS_UNSIGNED vecnum,
                                        void (*lisr)(VCOS_INT),
                                        void (**old_lisr)(VCOS_INT));

VCOS_INLINE_DECL
VCOS_STATUS_T vcos_legacy_hisr_create(VCOS_HISR_T *hisr, const char *name,
                                      void (*entry)(void),
                                      VCOS_UNSIGNED pri,
                                      void *stack, VCOS_UNSIGNED stack_size);

/** Activate a HISR. On an OS which has no distinction between a HISR and LISR,
  * this may use some kind of emulation, which could well be less efficient than
  * a normal ISR.`
  *
  * @param hisr HISR to activate.
  */
VCOS_INLINE_DECL
void vcos_legacy_hisr_activate(VCOS_HISR_T *hisr);

/** Delete a HISR.
  *
  * @param hisr HISR to delete.
  */
VCOS_INLINE_DECL
void vcos_legacy_hisr_delete(VCOS_HISR_T *hisr);

/** Are we in a legacy LISR?
  *
  * @return On Nucleus, non-zero if in a LISR. On other platforms, non-zero if
  * in an interrupt.
  */
VCOS_INLINE_DECL
int vcos_in_legacy_lisr(void);

/** Is the current thread actually a fake HISR thread? Only implemented
  * on platforms that fake up HISRs.
  */

#ifndef VCOS_LISRS_NEED_HISRS
VCOSPRE_ int VCOSPOST_ vcos_current_thread_is_fake_hisr_thread(VCOS_HISR_T *);
#endif

#ifdef __cplusplus
}
#endif
#endif
