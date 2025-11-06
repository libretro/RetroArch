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

#include <coreinit/memheap.h>
#include <coreinit/memexpheap.h>
#include <coreinit/memfrmheap.h>
#include <proc_ui/procui.h>

static MEMHeapHandle mem1_heap   = NULL;
static MEMHeapHandle bucket_heap = NULL;

// https://github.com/devkitPro/wut/blob/cac70f560e00d55a20a68e07d0f679934de28eb5/libraries/wutcrt/wut_preinit.c
void
__init_wut_sbrk_heap(MEMHeapHandle heapHandle);
void
__init_wut_malloc_lock();
void
__init_wut_defaultheap();

void __preinit_user(MEMHeapHandle *mem1,
               MEMHeapHandle *foreground,
               MEMHeapHandle *mem2)
{
   unsigned int  mem1_allocatable_size   = MEMGetAllocatableSizeForFrmHeapEx(*mem1, 4);
   void*         mem1_memory             = MEMAllocFromFrmHeapEx(*mem1, mem1_allocatable_size, 4);
   unsigned int  bucket_allocatable_size = MEMGetAllocatableSizeForFrmHeapEx(*foreground, 4);
   void*         bucket_memory           = MEMAllocFromFrmHeapEx(*foreground, bucket_allocatable_size, 4);

   __init_wut_sbrk_heap(*mem2);
   __init_wut_malloc_lock();
   __init_wut_defaultheap();

   if (mem1_memory)
      mem1_heap = MEMCreateExpHeapEx(mem1_memory, mem1_allocatable_size, 0);

   if (bucket_memory)
      bucket_heap = MEMCreateExpHeapEx(bucket_memory, bucket_allocatable_size, 0);
}

/* some wrappers */

void* MEM2_alloc(unsigned int size, unsigned int align)
{
   return memalign(align, size);
}

void MEM2_free(void* ptr)
{
   free(ptr);
}

void* MEM1_alloc(unsigned int size, unsigned int align)
{
   if (!ProcUIInForeground())
      return NULL;
   if (align < 4)
      align = 4;

   return MEMAllocFromExpHeapEx(mem1_heap, size, align);
}

void MEM1_free(void* ptr)
{
   if (!ProcUIInForeground())
      return;
   if (ptr)
      MEMFreeToExpHeap(mem1_heap, ptr);
}

void* MEMBucket_alloc(unsigned int size, unsigned int align)
{
   if (!ProcUIInForeground())
      return NULL;
   if (align < 4)
      align = 4;

   return MEMAllocFromExpHeapEx(bucket_heap, size, align);
}

void MEMBucket_free(void* ptr)
{
   if (!ProcUIInForeground())
      return;
   if (ptr)
      MEMFreeToExpHeap(bucket_heap, ptr);
}
