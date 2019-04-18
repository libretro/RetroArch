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
VideoCore OS Abstraction Layer - named semaphores
=============================================================================*/

#ifndef VCOS_GENERIC_NAMED_SEM_H
#define VCOS_GENERIC_NAMED_SEM_H

#ifdef __cplusplus
extern "C" {
#endif

#include "interface/vcos/vcos_types.h"

/**
 * \file
 *
 * Generic support for named semaphores, using regular ones. This is only
 * suitable for emulating them on an embedded MMUless system, since there is
 * no support for opening semaphores across process boundaries.
 *
 */

#define VCOS_NAMED_SEMAPHORE_NAMELEN   64

/* In theory we could use the name facility provided within Nucleus. However, this
 * is hard to do as semaphores are constantly being created and destroyed; we
 * would need to stop everything while allocating the memory for the semaphore
 * list and then walking it. So keep our own list.
 */
typedef struct VCOS_NAMED_SEMAPHORE_T
{
   struct VCOS_NAMED_SEMAPHORE_IMPL_T *actual; /**< There are 'n' named semaphores per 1 actual semaphore  */
   VCOS_SEMAPHORE_T *sem;                      /**< Pointer to actual underlying semaphore */
} VCOS_NAMED_SEMAPHORE_T;

VCOSPRE_ VCOS_STATUS_T VCOSPOST_
vcos_generic_named_semaphore_create(VCOS_NAMED_SEMAPHORE_T *sem, const char *name, VCOS_UNSIGNED count);

VCOSPRE_ void VCOSPOST_ vcos_named_semaphore_delete(VCOS_NAMED_SEMAPHORE_T *sem);

VCOSPRE_ VCOS_STATUS_T VCOSPOST_ _vcos_named_semaphore_init(void);
VCOSPRE_ void VCOSPOST_ _vcos_named_semaphore_deinit(void);

#if defined(VCOS_INLINE_BODIES)

VCOS_INLINE_IMPL
VCOS_STATUS_T vcos_named_semaphore_create(VCOS_NAMED_SEMAPHORE_T *sem, const char *name, VCOS_UNSIGNED count) {
   return vcos_generic_named_semaphore_create(sem, name, count);
}

VCOS_INLINE_IMPL
void vcos_named_semaphore_wait(VCOS_NAMED_SEMAPHORE_T *sem) {
   vcos_semaphore_wait(sem->sem);
}

VCOS_INLINE_IMPL
VCOS_STATUS_T vcos_named_semaphore_trywait(VCOS_NAMED_SEMAPHORE_T *sem) {
   return vcos_semaphore_trywait(sem->sem);
}

VCOS_INLINE_IMPL
void vcos_named_semaphore_post(VCOS_NAMED_SEMAPHORE_T *sem) {
   vcos_semaphore_post(sem->sem);
}

#endif

#ifdef __cplusplus
}
#endif
#endif

