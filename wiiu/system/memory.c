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
#include <coreinit/memory.h>
#include <coreinit/baseheap.h>
#include <coreinit/frameheap.h>
#include <coreinit/expandedheap.h>

#define MEMORY_ARENA_1          0
#define MEMORY_ARENA_2          1
#define MEMORY_ARENA_3          2
#define MEMORY_ARENA_4          3
#define MEMORY_ARENA_5          4
#define MEMORY_ARENA_6          5
#define MEMORY_ARENA_7          6
#define MEMORY_ARENA_8          7
#define MEMORY_ARENA_FG_BUCKET  8

static MEMExpandedHeap* mem1_heap;
static MEMExpandedHeap* bucket_heap;

void memoryInitialize(void)
{
    MEMHeapHandle mem1_heap_handle = MEMGetBaseHeapHandle(MEM_BASE_HEAP_MEM1);
    unsigned int mem1_allocatable_size = MEMGetAllocatableSizeForFrmHeapEx(mem1_heap_handle, 4);
    void *mem1_memory = MEMAllocFromFrmHeapEx(mem1_heap_handle, mem1_allocatable_size, 4);
    if(mem1_memory)
        mem1_heap = MEMCreateExpHeapEx(mem1_memory, mem1_allocatable_size, 0);

    MEMHeapHandle bucket_heap_handle = MEMGetBaseHeapHandle(MEM_BASE_HEAP_FG);
    unsigned int bucket_allocatable_size = MEMGetAllocatableSizeForFrmHeapEx(bucket_heap_handle, 4);
    void *bucket_memory = MEMAllocFromFrmHeapEx(bucket_heap_handle, bucket_allocatable_size, 4);
    if(bucket_memory)
        bucket_heap = MEMCreateExpHeapEx(bucket_memory, bucket_allocatable_size, 0);
}

void memoryRelease(void)
{
    MEMDestroyExpHeap(mem1_heap);
    MEMFreeToFrmHeap(MEMGetBaseHeapHandle(MEMORY_ARENA_1), 3);
    mem1_heap = NULL;

    MEMDestroyExpHeap(bucket_heap);
    MEMFreeToFrmHeap(MEMGetBaseHeapHandle(MEMORY_ARENA_FG_BUCKET), 3);
    bucket_heap = NULL;
}

#if 0
//!-------------------------------------------------------------------------------------------
//! wraps
//!-------------------------------------------------------------------------------------------
void *__wrap_malloc(size_t size)
{
    // pointer to a function resolve
   if(!size)
      return NULL;
	return ((void * (*)(size_t))(*pMEMAllocFromDefaultHeap))(size);
}

void *__wrap_memalign(size_t align, size_t size)
{
    if (align < 4)
        align = 4;
    if(!size)
       return NULL;
    // pointer to a function resolve
    return ((void * (*)(size_t, size_t))(*pMEMAllocFromDefaultHeapEx))(size, align);
}

void __wrap_free(void *p)
{
    // pointer to a function resolve
    if(p != 0)
        ((void (*)(void *))(*pMEMFreeToDefaultHeap))(p);
}

void *__wrap_calloc(size_t n, size_t size)
{
    void *p = __wrap_malloc(n * size);
	if (p != 0) {
		memset(p, 0, n * size);
	}
	return p;
}

size_t __wrap_malloc_usable_size(void *p)
{
    //! TODO: this is totally wrong and needs to be addressed
	return 0x7FFFFFFF;
}

void *__wrap_realloc(void *p, size_t size)
{
    void *new_ptr = __wrap_malloc(size);
	if (p != 0 && new_ptr != 0)
	{
		memcpy(new_ptr, p, __wrap_malloc_usable_size(p) < size ? __wrap_malloc_usable_size(p) : size);
		__wrap_free(p);
	}
	return new_ptr;
}

//!-------------------------------------------------------------------------------------------
//! reent versions
//!-------------------------------------------------------------------------------------------
void *__wrap__malloc_r(struct _reent *r, size_t size)
{
	return __wrap_malloc(size);
}

void *__wrap__calloc_r(struct _reent *r, size_t n, size_t size)
{
    return __wrap_calloc(n, size);
}

void *__wrap__memalign_r(struct _reent *r, size_t align, size_t size)
{
    return __wrap_memalign(align, size);
}

void __wrap__free_r(struct _reent *r, void *p)
{
    __wrap_free(p);
}

size_t __wrap__malloc_usable_size_r(struct _reent *r, void *p)
{
    return __wrap_malloc_usable_size(p);
}

void *__wrap__realloc_r(struct _reent *r, void *p, size_t size)
{
    return __wrap_realloc(p, size);
}
#else

void *
__wrap_memalign(size_t alignment, size_t size) {
   return MEMAllocFromExpHeapEx(MEMGetBaseHeapHandle(MEMORY_ARENA_2), size, alignment);
}

void *
__wrap_malloc(size_t size) {
   return __wrap_memalign(4, size);
}

void
__wrap_free(void *ptr) {
   if (ptr) {
      MEMFreeToExpHeap(MEMGetBaseHeapHandle(MEMORY_ARENA_2), ptr);
   }
}

size_t
__wrap_malloc_usable_size(void *ptr) {
   return MEMGetSizeForMBlockExpHeap(ptr);
}

void *
__wrap_realloc(void *ptr, size_t size) {
   if (!ptr) {
      return __wrap_malloc(size);
   }

   if (__wrap_malloc_usable_size(ptr) >= size) {
      return ptr;
   }

   void *realloc_ptr = __wrap_malloc(size);

   if(!realloc_ptr) {
      return NULL;
   }

   memcpy(realloc_ptr, ptr, __wrap_malloc_usable_size(ptr));
   __wrap_free(ptr);

   return realloc_ptr;
}

void *
__wrap_calloc(size_t num, size_t size) {
   void *ptr = __wrap_malloc(num*size);

   if(ptr) {
      memset(ptr, 0, num*size);
   }

   return ptr;
}

void *
__wrap_valloc(size_t size) {
   return __wrap_memalign(64, size);
}



void *
__wrap__memalign_r(struct _reent *r, size_t alignment, size_t size) {
   return __wrap_memalign(alignment, size);
}

void *
__wrap__malloc_r(struct _reent *r, size_t size) {
   return __wrap_malloc(size);
}

void
__wrap__free_r(struct _reent *r, void *ptr) {
   return __wrap_free(ptr);
}

void *
__wrap__realloc_r(struct _reent *r, void *ptr, size_t size) {
   return __wrap_realloc(ptr, size);
}

void *
__wrap__calloc_r(struct _reent *r, size_t num, size_t size) {
   return __wrap_calloc(num, size);
}

size_t
__wrap__malloc_usable_size_r(struct _reent *r, void *ptr) {
   return __wrap_malloc_usable_size(ptr);
}

void *
__wrap__valloc_r(struct _reent *r, size_t size) {
   return __wrap_valloc(size);
}

#endif
//!-------------------------------------------------------------------------------------------
//! some wrappers
//!-------------------------------------------------------------------------------------------
void * MEM2_alloc(unsigned int size, unsigned int align)
{
    return __wrap_memalign(align, size);
}

void MEM2_free(void *ptr)
{
    __wrap_free(ptr);
}

void * MEM1_alloc(unsigned int size, unsigned int align)
{
    if (align < 4)
        align = 4;
    return MEMAllocFromExpHeapEx(mem1_heap, size, align);
}

void MEM1_free(void *ptr)
{
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
    MEMFreeToExpHeap(bucket_heap, ptr);
}
