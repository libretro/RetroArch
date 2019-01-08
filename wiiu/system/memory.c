/****************************************************************************
 * Copyright (C) 2015 Dimok
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 ****************************************************************************/
#include <malloc.h>
#include <string.h>
#include "memory.h"
#include <wiiu/mem.h>

static MEMExpandedHeap* mem1_heap;
static MEMExpandedHeap* bucket_heap;

void memoryInitialize(void)
{
   unsigned int bucket_allocatable_size;
   MEMHeapHandle bucket_heap_handle;
   void *bucket_memory                = NULL;
   MEMHeapHandle mem1_heap_handle     = MEMGetBaseHeapHandle(MEM_BASE_HEAP_MEM1);
   unsigned int mem1_allocatable_size = MEMGetAllocatableSizeForFrmHeapEx(mem1_heap_handle, 4);
   void *mem1_memory                  = MEMAllocFromFrmHeapEx(mem1_heap_handle, mem1_allocatable_size, 4);

   if(mem1_memory)
      mem1_heap = MEMCreateExpHeapEx(mem1_memory,
            mem1_allocatable_size, 0);

   bucket_heap_handle      = MEMGetBaseHeapHandle(MEM_BASE_HEAP_FG);
   bucket_allocatable_size = MEMGetAllocatableSizeForFrmHeapEx(bucket_heap_handle, 4);
   bucket_memory           = MEMAllocFromFrmHeapEx(bucket_heap_handle, bucket_allocatable_size, 4);

   if(bucket_memory)
      bucket_heap = MEMCreateExpHeapEx(bucket_memory,
            bucket_allocatable_size, 0);
}

void memoryRelease(void)
{
    MEMDestroyExpHeap(mem1_heap);
    MEMFreeToFrmHeap(MEMGetBaseHeapHandle(MEM_BASE_HEAP_MEM1), MEM_FRAME_HEAP_FREE_ALL);
    mem1_heap = NULL;

    MEMDestroyExpHeap(bucket_heap);
    MEMFreeToFrmHeap(MEMGetBaseHeapHandle(MEM_BASE_HEAP_FG), MEM_FRAME_HEAP_FREE_ALL);
    bucket_heap = NULL;
}

void* _memalign_r(struct _reent *r, size_t alignment, size_t size)
{
   return MEMAllocFromExpHeapEx(MEMGetBaseHeapHandle(MEM_BASE_HEAP_MEM2), size, alignment);
}

void* _malloc_r(struct _reent *r, size_t size)
{
   return _memalign_r(r, 4, size);
}

void _free_r(struct _reent *r, void *ptr)
{
   if (ptr)
      MEMFreeToExpHeap(MEMGetBaseHeapHandle(MEM_BASE_HEAP_MEM2), ptr);
}

size_t _malloc_usable_size_r(struct _reent *r, void *ptr)
{
   return MEMGetSizeForMBlockExpHeap(ptr);
}

void * _realloc_r(struct _reent *r, void *ptr, size_t size)
{
   void *realloc_ptr = NULL;
   if (!ptr)
      return _malloc_r(r, size);

   if (_malloc_usable_size_r(r, ptr) >= size)
      return ptr;

   realloc_ptr = _malloc_r(r, size);

   if(!realloc_ptr)
      return NULL;

   memcpy(realloc_ptr, ptr, _malloc_usable_size_r(r, ptr));
   _free_r(r, ptr);

   return realloc_ptr;
}

void* _calloc_r(struct _reent *r, size_t num, size_t size)
{
   void *ptr = _malloc_r(r, num*size);

   if(ptr)
      memset(ptr, 0, num*size);

   return ptr;
}

void * _valloc_r(struct _reent *r, size_t size)
{
   return _memalign_r(r, 64, size);
}

/* some wrappers */

void * MEM2_alloc(unsigned int size, unsigned int align)
{
   return MEMAllocFromExpHeapEx(MEMGetBaseHeapHandle(MEM_BASE_HEAP_MEM2), size, align);
}

void MEM2_free(void *ptr)
{
   if (ptr)
      MEMFreeToExpHeap(MEMGetBaseHeapHandle(MEM_BASE_HEAP_MEM2), ptr);
}

void * MEM1_alloc(unsigned int size, unsigned int align)
{
   if (align < 4)
      align = 4;
   return MEMAllocFromExpHeapEx(mem1_heap, size, align);
}

void MEM1_free(void *ptr)
{
   if (ptr)
      MEMFreeToExpHeap(mem1_heap, ptr);
}

void * MEMBucket_alloc(unsigned int size, unsigned int align)
{
   if (align < 4)
      align = 4;
   return MEMAllocFromExpHeapEx(bucket_heap, size, align);
}

void MEMBucket_free(void *ptr)
{
   if (ptr)
      MEMFreeToExpHeap(bucket_heap, ptr);
}
