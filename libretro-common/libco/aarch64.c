/*
  libco.aarch64 (2017-06-26)
  author: webgeek1234
  license: public domain
*/

#define LIBCO_C
#include "libco.h"
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

static thread_local uint64_t co_active_buffer[64];
static thread_local cothread_t co_active_handle;

asm (
      ".globl co_switch_aarch64\n"
      ".globl _co_switch_aarch64\n"
      "co_switch_aarch64:\n"
      "_co_switch_aarch64:\n"
      "  stp x8,  x9,  [x1]\n"
      "  stp x10, x11, [x1, #16]\n"
      "  stp x12, x13, [x1, #32]\n"
      "  stp x14, x15, [x1, #48]\n"
      "  str x19, [x1, #72]\n"
      "  stp x20, x21, [x1, #80]\n"
      "  stp x22, x23, [x1, #96]\n"
      "  stp x24, x25, [x1, #112]\n"
      "  stp x26, x27, [x1, #128]\n"
      "  stp x28, x29, [x1, #144]\n"
      "  mov x16, sp\n"
      "  stp x16, x30, [x1, #160]\n"

      "  ldp x8,  x9,  [x0]\n"
      "  ldp x10, x11, [x0, #16]\n"
      "  ldp x12, x13, [x0, #32]\n"
      "  ldp x14, x15, [x0, #48]\n"
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

static void crash(void)
{
   /* Called only if cothread_t entrypoint returns. */
   assert(0);
}

cothread_t co_create(unsigned int size, void (*entrypoint)(void))
{
	uint64_t *ptr     = NULL;
   cothread_t handle = 0;
   size              = (size + 1023) & ~1023;
#if HAVE_POSIX_MEMALIGN >= 1
   if (posix_memalign(&handle, 1024, size + 512) < 0)
      return 0;
#else
   handle            = memalign(1024, size + 512);
#endif

   if (!handle)
      return handle;

   ptr     = (uint64_t*)handle;
   /* Non-volatiles.  */
   ptr[0]  = 0; /* x8  */
   ptr[1]  = 0; /* x9  */
   ptr[2]  = 0; /* x10 */
   ptr[3]  = 0; /* x11 */
   ptr[4]  = 0; /* x12 */
   ptr[5]  = 0; /* x13 */
   ptr[6]  = 0; /* x14 */
   ptr[7]  = 0; /* x15 */
   ptr[8]  = 0; /* padding */
   ptr[9]  = 0; /* x19 */
   ptr[10] = 0; /* x20 */
   ptr[11] = 0; /* x21 */
   ptr[12] = 0; /* x22 */
   ptr[13] = 0; /* x23 */
   ptr[14] = 0; /* x24 */
   ptr[15] = 0; /* x25 */
   ptr[16] = 0; /* x26 */
   ptr[17] = 0; /* x27 */
   ptr[18] = 0; /* x28 */
   ptr[20] = (uintptr_t)ptr + size + 512 - 16; /* x30, stack pointer */
   ptr[19] = ptr[20]; /* x29, frame pointer */
   ptr[21] = (uintptr_t)entrypoint; /* PC (link register x31 gets saved here). */
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
