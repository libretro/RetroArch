#include <3ds.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>

#include "ctr_debug.h"

extern u32 __heapBase;
extern u32 __heap_size;
extern u32 __stack_bottom;
extern u32 __stack_size_extra;
extern u32 __stacksize__;

u32 ctr_get_linear_free(void);
u32 ctr_get_linear_unused(void);

u32 ctr_get_stack_free(void)
{
   extern u32 __stack_bottom;

   uint32_t* stack_bottom_current = (u32*)__stack_bottom;
   while(*stack_bottom_current++ == 0xFCFCFCFC);
   stack_bottom_current--;

   return ((u32)stack_bottom_current - __stack_bottom);
}

u32 ctr_get_stack_usage(void)
{
   extern u32 __stacksize__;
   u32 stack_free = ctr_get_stack_free();

   return __stacksize__ > stack_free? __stacksize__ - stack_free: 0;
}

void ctr_linear_free_pages(u32 pages);

void ctr_free_pages(u32 pages)
{
   if(!pages)
      return;

   u32 linear_free_pages = ctr_get_linear_free() >> 12;

   if((ctr_get_linear_unused() >> 12) > pages + 0x100)
      return ctr_linear_free_pages(pages);

#if 0
   if(linear_free_pages > pages + 0x400)
      return ctr_linear_free_pages(pages);
#endif

   u32 stack_free = ctr_get_stack_free();
   u32 stack_usage = __stacksize__ > stack_free? __stacksize__ - stack_free: 0;

   stack_free = stack_free > __stack_size_extra ? __stack_size_extra : stack_free;

   u32 stack_free_pages = stack_free >> 12;

   if(linear_free_pages + (stack_free_pages - (stack_usage >> 12)) > pages)
   {
      stack_free_pages -= (stack_usage >> 12);
      stack_free_pages  = stack_free_pages > pages ? pages : stack_free_pages;
      linear_free_pages = pages - stack_free_pages;
   }
   else if(linear_free_pages + stack_free_pages > pages)
      stack_free_pages = pages - linear_free_pages;
   else
      return;

   if(linear_free_pages)
      ctr_linear_free_pages(linear_free_pages);

   if(stack_free_pages)
   {
      u32 tmp;
      svcControlMemory(&tmp, __stack_bottom,
            0x0,
            stack_free_pages << 12,
            MEMOP_FREE, MEMPERM_READ | MEMPERM_WRITE);
      __stack_bottom     += stack_free_pages << 12;
      __stack_size_extra -= stack_free_pages << 12;
      __stacksize__      -= stack_free_pages << 12;
#if 0
      printf("s:0x%08X-->0x%08X(-0x%08X) \n", stack_free,
            stack_free - (stack_free_pages << 12), stack_free_pages << 12);
      DEBUG_HOLD();
#endif
   }
}

u32 ctr_get_free_space(void)
{
   s64 mem_used;
   u32 app_memory = *((u32*)0x1FF80040);
   svcGetSystemInfo(&mem_used, 0, 1);
   return app_memory - (u32)mem_used;
}

void ctr_request_free_pages(u32 pages)
{
   u32 free_pages = ctr_get_free_space() >> 12;
   if (pages > free_pages)
   {
      ctr_free_pages(pages - free_pages);
      free_pages = ctr_get_free_space() >> 12;
   }
}

void* _sbrk_r(struct _reent *ptr, ptrdiff_t incr)
{
   static u32 sbrk_top = 0;

   if (!sbrk_top)
      sbrk_top = __heapBase;

   u32 tmp;

   int diff = ((sbrk_top + incr + 0xFFF) & ~0xFFF)
      - (__heapBase + __heap_size);

   if (diff > 0)
   {
      ctr_request_free_pages(diff >> 12);

      if (svcControlMemory(&tmp, __heapBase + __heap_size,
               0x0, diff, MEMOP_ALLOC, MEMPERM_READ | MEMPERM_WRITE) < 0)
      {
         ptr->_errno = ENOMEM;
         return (caddr_t) -1;
      }
   }

   __heap_size += diff;

   if (diff < 0)
      svcControlMemory(&tmp, __heapBase + __heap_size,
            0x0, -diff, MEMOP_FREE, MEMPERM_READ | MEMPERM_WRITE);

   sbrk_top += incr;

   return (caddr_t)(sbrk_top - incr);
}
