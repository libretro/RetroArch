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
Create the vcos_malloc API from the regular system malloc/free
=============================================================================*/

/**
  * \file
  *
  * Create the vcos malloc API from a regular system malloc/free library.
  *
  * The API lets callers specify an alignment.
  *
  * Under VideoCore this is not needed, as we can simply use the rtos_malloc routines.
  * But on host platforms that won't be the case.
  *
  */

VCOSPRE_ void * VCOSPOST_  vcos_generic_mem_alloc(VCOS_UNSIGNED sz, const char *desc);
VCOSPRE_  void * VCOSPOST_ vcos_generic_mem_calloc(VCOS_UNSIGNED count, VCOS_UNSIGNED sz, const char *descr);
VCOSPRE_  void VCOSPOST_   vcos_generic_mem_free(void *ptr);
VCOSPRE_  void * VCOSPOST_ vcos_generic_mem_alloc_aligned(VCOS_UNSIGNED sz, VCOS_UNSIGNED align, const char *desc);

#ifdef VCOS_INLINE_BODIES

VCOS_INLINE_IMPL
void *vcos_malloc(VCOS_UNSIGNED size, const char *description) {
   return vcos_generic_mem_alloc(size, description);
}

VCOS_INLINE_IMPL
void *vcos_calloc(VCOS_UNSIGNED num, VCOS_UNSIGNED size, const char *description) {
   return vcos_generic_mem_calloc(num, size, description);
}

VCOS_INLINE_IMPL
void vcos_free(void *ptr) {
   vcos_generic_mem_free(ptr);
}

VCOS_INLINE_IMPL
void * vcos_malloc_aligned(VCOS_UNSIGNED size, VCOS_UNSIGNED align, const char *description) {
   return vcos_generic_mem_alloc_aligned(size, align, description);
}

/* Returns invalid result, do not use */

VCOS_INLINE_IMPL
unsigned long VCOS_DEPRECATED("returns invalid result") vcos_get_free_mem(void) {
   return 0;
}

#endif /* VCOS_INLINE_BODIES */


