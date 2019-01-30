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
VideoCore OS Abstraction Layer - generic thread local storage
=============================================================================*/

#ifndef VCOS_GENERIC_TLS_H
#define VCOS_GENERIC_TLS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "interface/vcos/vcos_types.h"

/**
  * \file
  *
  * Do an emulation of Thread Local Storage. The platform needs to
  * provide a way to set and get a per-thread pointer which is
  * where the TLS data itself is stored.
  *
  *
  * Each thread that wants to join in this scheme needs to call
  * vcos_tls_thread_register().
  *
  * The platform needs to support the macros/functions
  * _vcos_tls_thread_ptr_set() and _vcos_tls_thread_ptr_get().
  */

#ifndef VCOS_WANT_TLS_EMULATION
#error Should not be included unless TLS emulation is defined
#endif

/** Number of slots to reserve per thread. This results in an overhead
  * of this many words per thread.
  */
#define VCOS_TLS_MAX_SLOTS 4

/** TLS key. Allocating one of these reserves the client one of the 
  * available slots.
  */
typedef VCOS_UNSIGNED VCOS_TLS_KEY_T;

/** TLS per-thread structure. Each thread gets one of these
  * if TLS emulation (rather than native TLS support) is
  * being used.
  */
typedef struct VCOS_TLS_THREAD_T
{
   void *slots[VCOS_TLS_MAX_SLOTS];
} VCOS_TLS_THREAD_T;

/*
 * Internal APIs 
 */

/** Register this thread's TLS storage area. */
VCOSPRE_ void VCOSPOST_ vcos_tls_thread_register(VCOS_TLS_THREAD_T *);

/** Create a new TLS key */
VCOSPRE_ VCOS_STATUS_T VCOSPOST_ vcos_generic_tls_create(VCOS_TLS_KEY_T *key);

/** Delete a TLS key */
VCOSPRE_ void VCOSPOST_ vcos_generic_tls_delete(VCOS_TLS_KEY_T tls);

/** Initialise the TLS library */
VCOSPRE_ VCOS_STATUS_T VCOSPOST_ vcos_tls_init(void);

/** Deinitialise the TLS library */
VCOSPRE_ void VCOSPOST_ vcos_tls_deinit(void);

#if defined(VCOS_INLINE_BODIES)

#undef VCOS_ASSERT_LOGGING_DISABLE
#define VCOS_ASSERT_LOGGING_DISABLE 1

/*
 * Implementations of public API functions
 */

/** Set the given value. Since everything is per-thread, there is no need
  * for any locking.
  */
VCOS_INLINE_IMPL
VCOS_STATUS_T vcos_tls_set(VCOS_TLS_KEY_T tls, void *v) {
   VCOS_TLS_THREAD_T *tlsdata = _vcos_tls_thread_ptr_get();
   vcos_assert(tlsdata); /* Fires if this thread has not been registered */
   if (tls<VCOS_TLS_MAX_SLOTS)
   {
      tlsdata->slots[tls] = v;
      return VCOS_SUCCESS;
   }
   else
   {
      vcos_assert(0);
      return VCOS_EINVAL;
   }
}

/** Get the given value. No locking required.
  */
VCOS_INLINE_IMPL
void *vcos_tls_get(VCOS_TLS_KEY_T tls) {
   VCOS_TLS_THREAD_T *tlsdata = _vcos_tls_thread_ptr_get();
   vcos_assert(tlsdata); /* Fires if this thread has not been registered */
   if (tls<VCOS_TLS_MAX_SLOTS)
   {
      return tlsdata->slots[tls];
   }
   else
   {
      vcos_assert(0);
      return NULL;
   }
}

VCOS_INLINE_IMPL
VCOS_STATUS_T vcos_tls_create(VCOS_TLS_KEY_T *key) {
   return vcos_generic_tls_create(key);
}

VCOS_INLINE_IMPL
void vcos_tls_delete(VCOS_TLS_KEY_T tls) {
   vcos_generic_tls_delete(tls);
}

#undef VCOS_ASSERT_LOGGING_DISABLE
#define VCOS_ASSERT_LOGGING_DISABLE 0

#endif /* VCOS_INLINE_BODIES */

#ifdef __cplusplus
}
#endif

#endif


