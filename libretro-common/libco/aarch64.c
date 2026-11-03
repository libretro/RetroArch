/*
  libco.aarch64 (2017-06-26)
  author: webgeek1234
  license: public domain
*/

#define LIBCO_C
#include "libco.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>

#ifndef __APPLE__
#include <malloc.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

static thread_local uint64_t co_active_buffer[64];
static thread_local cothread_t co_active_handle;

__asm__ (
      ".globl co_switch_aarch64\n"
      ".globl _co_switch_aarch64\n"
      "co_switch_aarch64:\n"
      "_co_switch_aarch64:\n"
      "  stp d8,  d9,  [x1]\n"
      "  stp d10, d11, [x1, #16]\n"
      "  stp d12, d13, [x1, #32]\n"
      "  stp d14, d15, [x1, #48]\n"
      "  mrs x16, fpcr\n"
      "  mrs x17, fpsr\n"
      "  stp w16, w17, [x1, #64]\n"
      "  str x19, [x1, #72]\n"
      "  stp x20, x21, [x1, #80]\n"
      "  stp x22, x23, [x1, #96]\n"
      "  stp x24, x25, [x1, #112]\n"
      "  stp x26, x27, [x1, #128]\n"
      "  stp x28, x29, [x1, #144]\n"
      "  mov x16, sp\n"
      "  stp x16, x30, [x1, #160]\n"

      "  ldp d8,  d9,  [x0]\n"
      "  ldp d10, d11, [x0, #16]\n"
      "  ldp d12, d13, [x0, #32]\n"
      "  ldp d14, d15, [x0, #48]\n"
      "  ldp w16, w17, [x0, #64]\n"
      "  msr fpcr, x16\n"
      "  msr fpsr, x17\n"
      "  ldr x19, [x0, #72]\n"
      "  ldp x20, x21, [x0, #80]\n"
      "  ldp x22, x23, [x0, #96]\n"
      "  ldp x24, x25, [x0, #112]\n"
      "  ldp x26, x27, [x0, #128]\n"
      "  ldp x28, x29, [x0, #144]\n"
      "  ldp x16, x17, [x0, #160]\n"
      "  mov sp, x16\n"
      "  br x17\n"
    );

/* ASM */
void co_switch_aarch64(cothread_t handle, cothread_t current);

static void co_entry(void)
{
   void (*entrypoint)(void) = (void (*)(void))
         ((uint64_t*)co_active_handle)[22];
   entrypoint();
   abort();
}

cothread_t co_create(unsigned int size, void (*entrypoint)(void))
{
   uint64_t *ptr;
   uint64_t fpcr, fpsr;
   cothread_t handle = NULL;
   if (size > UINT_MAX - 1023U - 512U)
      return NULL;
   size = (size + 1023U) & ~1023U;
#if HAVE_POSIX_MEMALIGN >= 1
   if (posix_memalign(&handle, 1024, (size_t)size + 512) != 0)
      return NULL;
#else
   handle = memalign(1024, (size_t)size + 512);
#endif
   if (!handle)
      return NULL;
   ptr = (uint64_t*)handle;
   memset(ptr, 0, 23 * sizeof(*ptr));
   __asm__ __volatile__("mrs %0, fpcr\n\tmrs %1, fpsr"
         : "=r"(fpcr), "=r"(fpsr));
   ((uint32_t*)ptr)[16] = (uint32_t)fpcr;
   ((uint32_t*)ptr)[17] = (uint32_t)fpsr;
   ptr[20] = (uintptr_t)ptr + size + 512 - 16;
   ptr[21] = (uintptr_t)co_entry;
   ptr[22] = (uintptr_t)entrypoint;
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
   co_switch_aarch64(co_active_handle = handle, co_previous_handle);
}

#ifdef __cplusplus
}
#endif
