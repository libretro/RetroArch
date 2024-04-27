/*
libco.arm (2015-06-18)
license: public domain
*/

#define LIBCO_C
#include "libco.h"

#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <psp2/kernel/sysmem.h>
#include <stdio.h>
#include <string.h>

#define FOUR_KB_ALIGN(x) align(x, 12)
#define MB_ALIGN(x)      align(x, 20)

#ifdef __cplusplus
extern "C" {
#endif

   static inline int align(int x, int n)
   {
      return (((x >> n) + 1) << n);
   }

   static thread_local unsigned long co_active_buffer[64];
   static thread_local cothread_t co_active_handle = 0;
   static void(*co_swap)(cothread_t, cothread_t) = 0;
   static int block;
   static uint32_t co_swap_function[] = {
      0xe8a16ff0,  /* stmia r1!, {r4-r11,sp,lr} */
      0xe8b0aff0,  /* ldmia r0!, {r4-r11,sp,pc} */
      0xe12fff1e,  /* bx lr                     */
   };

   static void co_init(void)
   {
      int ret;
      void *base;

      block = sceKernelAllocMemBlockForVM("libco",
            MB_ALIGN(FOUR_KB_ALIGN(sizeof co_swap_function)));
      if (block < 0)
         return;

      /* Get base address */
      if ((ret = sceKernelGetMemBlockBase(block, &base)) < 0)
         return;

      /* Set domain to be writable by user */
      if ((ret = sceKernelOpenVMDomain()) < 0)
         return;

      memcpy(base, co_swap_function, sizeof co_swap_function);

      /* Set domain back to read-only */
      if ((ret = sceKernelCloseVMDomain()) < 0)
         return;

      /* Flush icache */
      ret = sceKernelSyncVMDomain(block, base,
            MB_ALIGN(FOUR_KB_ALIGN(sizeof co_swap_function)));
      if (ret < 0)
         return;

      co_swap = (void(*)(cothread_t, cothread_t))base;
   }

   cothread_t co_active(void)
   {
      if (!co_active_handle)
         co_active_handle = &co_active_buffer;
      return co_active_handle;
   }

   cothread_t co_create(unsigned int size, void(*entrypoint)(void))
   {
      unsigned long* handle = 0;
      if (!co_swap)
         co_init();
      if (!co_active_handle)
         co_active_handle   = &co_active_buffer;
      size                 += 256;
      size                 &= ~15;

      if ((handle = (unsigned long*)malloc(size)))
      {
         unsigned long *p   = (unsigned long*)((unsigned char*)handle + size);
         handle[8]          = (unsigned long)p;
         handle[9]          = (unsigned long)entrypoint;
      }

      return handle;
   }

   void co_delete(cothread_t handle)
   {
      free(handle);
      sceKernelFreeMemBlock(block);
   }

   void co_switch(cothread_t handle)
   {
      cothread_t co_previous_handle = co_active_handle;
      co_swap(co_active_handle = handle, co_previous_handle);
   }

#ifdef __cplusplus
}
#endif
