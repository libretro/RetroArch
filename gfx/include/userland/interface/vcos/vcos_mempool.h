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
VideoCore OS Abstraction Layer - memory pool support
=============================================================================*/

#ifndef VCOS_MEMPOOL_H
#define VCOS_MEMPOOL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "interface/vcos/vcos_types.h"
#include "vcos.h"

/** \file
  *
  * Memory pools - variable sized allocator.
  *
  * A very basic memory pool API.
  *
  * This interface is deliberately not thread safe - clients should add
  * their own locking, if required.
  *
  *
  * \fixme: Add fixed-size allocator.
  *
  */

/** Initialize a memory pool. The control data is taken from the memory
  * supplied itself.
  *
  * Note: the dmalloc pool uses the memory supplied for its control
  * area. This is probably a bit broken, as it stops you creating
  * a pool in some "special" area of memory, while leaving the control
  * information in normal memory.
  *
  * @param pool  Pointer to pool object.
  *
  * @param name  Name for the pool. Used for diagnostics.
  *
  * @param start Starting address. Must be at least 8byte aligned.
  *
  * @param size  Size of pool in bytes.
  *
  * @return VCOS_SUCCESS if pool was created.
  */
VCOS_INLINE_DECL
VCOS_STATUS_T vcos_mempool_create(VCOS_MEMPOOL_T *pool, const char *name, void *start, VCOS_UNSIGNED size);

/** Allocate some memory from a pool. If no memory is available, it
  * returns NULL.
  *
  * @param pool Pool to allocate from
  * @param len  Length of memory to allocate
  *
  */
VCOS_INLINE_DECL
void *vcos_mempool_alloc(VCOS_MEMPOOL_T *pool, VCOS_UNSIGNED len);

/** Free some memory back to a pool.
  *
  * @param pool Pool to return to
  * @param mem Memory to return
  */
VCOS_INLINE_DECL
void vcos_mempool_free(VCOS_MEMPOOL_T *pool, void *mem);

/** Deinitialize a memory pool.
  *
  * @param pool Pool to return to
  */
VCOS_INLINE_DECL
void vcos_mempool_delete(VCOS_MEMPOOL_T *pool);

#ifdef __cplusplus
}
#endif
#endif

