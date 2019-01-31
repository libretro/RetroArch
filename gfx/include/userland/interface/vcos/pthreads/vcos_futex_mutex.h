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
FIXME: This code should be moved to 'linux', it is linux-specific and not generic
on 'pthreads'.
============================================================================*/

#ifndef VCOS_MUTEX_FROM_FUTEX_H
#define VCOS_MUTEX_FROM_FUTEX_H

#ifdef __cplusplus
extern "C" {
#endif

#include "interface/vcos/vcos_types.h"
#include "vcos_platform.h"

typedef struct VCOS_FUTEX_T
{
   volatile int value;
} VCOS_FUTEX_T;

typedef VCOS_FUTEX_T VCOS_MUTEX_T;

VCOSPRE_ VCOS_STATUS_T VCOSPOST_ vcos_futex_init(VCOS_FUTEX_T *futex);
VCOSPRE_ void VCOSPOST_ vcos_futex_delete(VCOS_FUTEX_T *futex);
VCOSPRE_ VCOS_STATUS_T VCOSPOST_ vcos_futex_lock(VCOS_FUTEX_T *futex);
VCOSPRE_ void VCOSPOST_ vcos_futex_unlock(VCOS_FUTEX_T *futex);
VCOSPRE_ VCOS_STATUS_T VCOSPOST_ vcos_futex_trylock(VCOS_FUTEX_T *futex);

#if defined(VCOS_INLINE_BODIES)

VCOS_INLINE_IMPL
VCOS_STATUS_T vcos_mutex_create(VCOS_MUTEX_T *latch, const char *name) {
   vcos_unused(name);
   return vcos_futex_init(latch);
}

VCOS_INLINE_IMPL
void vcos_mutex_delete(VCOS_MUTEX_T *latch) {
   vcos_futex_delete(latch);
}

VCOS_INLINE_IMPL
VCOS_STATUS_T vcos_mutex_lock(VCOS_MUTEX_T *latch) {
   return vcos_futex_lock(latch);
}

VCOS_INLINE_IMPL
void vcos_mutex_unlock(VCOS_MUTEX_T *latch) {
   vcos_futex_unlock(latch);
}

VCOS_INLINE_IMPL
int vcos_mutex_is_locked(VCOS_MUTEX_T *latch) {
   int rc = latch->value;
   if (!rc) {
      /* it wasn't locked */
      return 0;
   }
   else {
      return 1; /* it was locked */
   }
}

VCOS_INLINE_IMPL
VCOS_STATUS_T vcos_mutex_trylock(VCOS_MUTEX_T *m) {
   return vcos_futex_trylock(m);
}

#endif /* VCOS_INLINE_BODIES */

#ifdef __cplusplus
}
#endif
#endif /* VCOS_MUTEX_FROM_FUTEX_H */

