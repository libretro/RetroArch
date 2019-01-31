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
VideoCore OS Abstraction Layer - memory alloc implementation
=============================================================================*/

#include "interface/vcos/vcos.h"

#ifndef _vcos_platform_malloc
#include <stdlib.h>
#define _vcos_platform_malloc malloc
#define _vcos_platform_free   free
#endif

typedef struct malloc_header_s {
   uint32_t guardword;
   uint32_t size;
   const char *description;
   void *ptr;
} MALLOC_HEADER_T;


#define MIN_ALIGN sizeof(MALLOC_HEADER_T)

#define GUARDWORDHEAP  0xa55a5aa5

void *vcos_generic_mem_alloc_aligned(VCOS_UNSIGNED size, VCOS_UNSIGNED align, const char *desc)
{
   int local_align = align == 0 ? 1 : align;
   int required_size = size + local_align + sizeof(MALLOC_HEADER_T);
   void *ptr = _vcos_platform_malloc(required_size);
   void *ret = NULL;
   MALLOC_HEADER_T *h;

   if (ptr)
   {
      ret = (void *)VCOS_ALIGN_UP(((char *)ptr)+sizeof(MALLOC_HEADER_T), local_align);
      h = ((MALLOC_HEADER_T *)ret)-1;
      h->size = size;
      h->description = desc;
      h->guardword = GUARDWORDHEAP;
      h->ptr = ptr;
   }

   return ret;
}

void *vcos_generic_mem_alloc(VCOS_UNSIGNED size, const char *desc)
{
   return vcos_generic_mem_alloc_aligned(size,MIN_ALIGN,desc);
}

void *vcos_generic_mem_calloc(VCOS_UNSIGNED count, VCOS_UNSIGNED sz, const char *desc)
{
   uint32_t size = count*sz;
   void *ptr = vcos_generic_mem_alloc_aligned(size,MIN_ALIGN,desc);
   if (ptr)
   {
      memset(ptr, 0, size);
   }
   return ptr;
}

void vcos_generic_mem_free(void *ptr)
{
   MALLOC_HEADER_T *h;
   if (! ptr) return;

   h = ((MALLOC_HEADER_T *)ptr)-1;
   vcos_assert(h->guardword == GUARDWORDHEAP);
   _vcos_platform_free(h->ptr);
}

