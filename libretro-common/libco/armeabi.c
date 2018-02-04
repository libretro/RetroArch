/*
  libco.armeabi (2013-04-05)
  author: Themaister
  license: public domain
*/

#define LIBCO_C
#include <libco.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#ifndef __APPLE__
#include <malloc.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

static thread_local uint32_t co_active_buffer[64];
static thread_local cothread_t co_active_handle;

__asm__ (
#if defined(__thumb2__)
      ".thumb\n"
      ".align 2\n"
      ".globl co_switch_arm\n"
      ".globl _co_switch_arm\n"
      "co_switch_arm:\n"
      "_co_switch_arm:\n"
      " mov r3, sp\n"
      " stmia r1!, {r4, r5, r6, r7, r8, r9, r10, r11}\n"
      " stmia r1!, {r3, lr}\n"
      " ldmia r0!, {r4, r5, r6, r7, r8, r9, r10, r11}\n"
      " ldmfd r0!, { r3 }\n"
      " mov sp, r3\n"
      " ldmfd r0!, { r3 }\n"
      " mov pc, r3\n"
#else
      ".arm\n"
      ".align 4\n"
      ".globl co_switch_arm\n"
      ".globl _co_switch_arm\n"
      "co_switch_arm:\n"
      "_co_switch_arm:\n"
      "  stmia r1!, {r4, r5, r6, r7, r8, r9, r10, r11, sp, lr}\n"
      "  ldmia r0!, {r4, r5, r6, r7, r8, r9, r10, r11, sp, pc}\n"
#endif
    );

/* ASM */
void co_switch_arm(cothread_t handle, cothread_t current);

cothread_t co_create(unsigned int size, void (*entrypoint)(void))
{
   size = (size + 1023) & ~1023;
   cothread_t handle = 0;
#if defined(__APPLE__) || HAVE_POSIX_MEMALIGN >= 1
   if (posix_memalign(&handle, 1024, size + 256) < 0)
      return 0;
#else
   handle = memalign(1024, size + 256);
#endif

   if (!handle)
      return handle;

   uint32_t *ptr = (uint32_t*)handle;
   /* Non-volatiles.  */
   ptr[0] = 0; /* r4  */
   ptr[1] = 0; /* r5  */
   ptr[2] = 0; /* r6  */
   ptr[3] = 0; /* r7  */
   ptr[4] = 0; /* r8  */
   ptr[5] = 0; /* r9  */
   ptr[6] = 0; /* r10 */
   ptr[7] = 0; /* r11 */
   /* Align stack to 64-bit */
   ptr[8] = (uintptr_t)ptr + size + 256 - 8; /* r13, stack pointer */
   ptr[9] = (uintptr_t)entrypoint; /* r15, PC (link register r14 gets saved here). */
   return handle;
}

cothread_t co_active(void)
{
   if (!co_active_handle)
      co_active_handle = co_active_buffer;
   return co_active_handle;
}

void co_delete(cothread_t handle)
{
   free(handle);
}

void co_switch(cothread_t handle)
{
   cothread_t co_previous_handle = co_active();
   co_switch_arm(co_active_handle = handle, co_previous_handle);
}

#ifdef __cplusplus
}
#endif

