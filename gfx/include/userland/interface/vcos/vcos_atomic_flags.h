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

#ifndef VCOS_ATOMIC_FLAGS_H
#define VCOS_ATOMIC_FLAGS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "interface/vcos/vcos_types.h"
#include "vcos.h"

/**
 * \file vcos_atomic_flags.h
 *
 * Defines atomic flags API.
 *
 * 32 flags. Atomic "or" and "get and clear" operations
 */

/**
 * Create an atomic flags instance.
 *
 * @param atomic_flags Pointer to atomic flags instance, filled in on return
 *
 * @return VCOS_SUCCESS if succeeded.
 */
VCOS_INLINE_DECL
VCOS_STATUS_T vcos_atomic_flags_create(VCOS_ATOMIC_FLAGS_T *atomic_flags);

/**
 * Atomically set the specified flags.
 *
 * @param atomic_flags Instance to set flags on
 * @param flags        Mask of flags to set
 */
VCOS_INLINE_DECL
void vcos_atomic_flags_or(VCOS_ATOMIC_FLAGS_T *atomic_flags, uint32_t flags);

/**
 * Retrieve the current flags and then clear them. The entire operation is
 * atomic.
 *
 * @param atomic_flags Instance to get/clear flags from/on
 *
 * @return Mask of flags which were set (and we cleared)
 */
VCOS_INLINE_DECL
uint32_t vcos_atomic_flags_get_and_clear(VCOS_ATOMIC_FLAGS_T *atomic_flags);

/**
 * Delete an atomic flags instance.
 *
 * @param atomic_flags Instance to delete
 */
VCOS_INLINE_DECL
void vcos_atomic_flags_delete(VCOS_ATOMIC_FLAGS_T *atomic_flags);

#ifdef __cplusplus
}
#endif

#endif
