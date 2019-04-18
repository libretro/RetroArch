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
VideoCore OS Abstraction Layer - thread local storage
=============================================================================*/

#ifndef VCOS_TLS_H
#define VCOS_TLS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "interface/vcos/vcos_types.h"
#include "vcos.h"

/** Create a new thread local storage data key visible to all threads in
  * the current process.
  *
  * @param key    The key to create
  */
VCOS_INLINE_DECL
VCOS_STATUS_T vcos_tls_create(VCOS_TLS_KEY_T *key);

/** Delete an existing TLS data key.
  */
VCOS_INLINE_DECL
void vcos_tls_delete(VCOS_TLS_KEY_T tls);

/** Set the value seen by the current thread.
  *
  * @param key    The key to update
  * @param v      The value to set for the current thread.
  *
  * @return VCOS_SUCCESS, or VCOS_ENOMEM if memory for this slot
  * could not be allocated.
  *
  * If TLS is being emulated via VCOS then the memory required
  * can be preallocated at thread creation time
  */
VCOS_INLINE_DECL
VCOS_STATUS_T vcos_tls_set(VCOS_TLS_KEY_T tls, void *v);

/** Get the value for the current thread.
  *
  * @param key    The key to update
  *
  * @return The current value for this thread.
  */
VCOS_INLINE_DECL
void *vcos_tls_get(VCOS_TLS_KEY_T tls);

#ifdef __cplusplus
}
#endif

#endif
