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
VideoCore OS Abstraction Layer - memory support
=============================================================================*/

#ifndef VCOS_MEM_H
#define VCOS_MEM_H

#ifdef __cplusplus
extern "C" {
#endif

#include "interface/vcos/vcos_types.h"
#include "vcos.h"

/** \file
  *
  * Memory allocation api (malloc/free equivalents) is for benefit of host
  * applications. VideoCore code should use rtos_XXX functions.
  *
  */


/** Allocate memory
  *
  * @param size Size of memory to allocate
  * @param description Description, to aid in debugging. May be ignored internally on some platforms.
  */
VCOS_INLINE_DECL
void *vcos_malloc(VCOS_UNSIGNED size, const char *description);

void *vcos_kmalloc(VCOS_UNSIGNED size, const char *description);
void *vcos_kcalloc(VCOS_UNSIGNED num, VCOS_UNSIGNED size, const char *description);

/** Allocate cleared memory
  *
  * @param num Number of items to allocate.
  * @param size Size of each item in bytes.
  * @param description Description, to aid in debugging. May be ignored internally on some platforms.
  */
VCOS_INLINE_DECL
void *vcos_calloc(VCOS_UNSIGNED num, VCOS_UNSIGNED size, const char *description);

/** Free memory
  *
  * Free memory that has been allocated.
  */
VCOS_INLINE_DECL
void vcos_free(void *ptr);

void vcos_kfree(void *ptr);

/** Allocate aligned memory
  *
  * Allocate memory aligned on the specified boundary.
  *
  * @param size Size of memory to allocate
  * @param description Description, to aid in debugging. May be ignored internally on some platforms.
  */
VCOS_INLINE_DECL
void *vcos_malloc_aligned(VCOS_UNSIGNED size, VCOS_UNSIGNED align, const char *description);

/** Return the amount of free heap memory
  *
  */
VCOS_INLINE_DECL
unsigned long vcos_get_free_mem(void);

#ifdef __cplusplus
}
#endif

#endif


