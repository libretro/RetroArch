#define LIBCO_C
#include "libco.h"
#include "settings.h"

#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>

#ifdef __cplusplus
extern "C" {
#endif

static thread_local unsigned long co_active_buffer[64];
static thread_local cothread_t co_active_handle = 0;
static void (*co_swap)(cothread_t, cothread_t) = 0;

#ifdef LIBCO_MPROTECT
  alignas(4096)
#else
  section(text)
#endif
static const unsigned long co_swap_function[1024] = {
  0xe8a16ff0,  /* stmia r1!, {r4-r11,sp,lr} */
  0xe8b0aff0,  /* ldmia r0!, {r4-r11,sp,pc} */
  0xe12fff1e,  /* bx lr                     */
};

static void co_init() {
  #ifdef LIBCO_MPROTECT
  unsigned long addr = (unsigned long)co_swap_function;
  unsigned long base = addr - (addr % sysconf(_SC_PAGESIZE));
  unsigned long size = (addr - base) + sizeof co_swap_function;
  mprotect((void*)base, size, PROT_READ | PROT_EXEC);
  #endif
}

cothread_t co_active() {
  if(!co_active_handle) co_active_handle = &co_active_buffer;
  return co_active_handle;
}

cothread_t co_derive(void* memory, unsigned int size, void (*entrypoint)(void)) {
  unsigned long* handle;
  if(!co_swap) {
    co_init();
    co_swap = (void (*)(cothread_t, cothread_t))co_swap_function;
  }
  if(!co_active_handle) co_active_handle = &co_active_buffer;

  if(handle = (unsigned long*)memory) {
    unsigned int offset = (size & ~15);
    unsigned long* p = (unsigned long*)((unsigned char*)handle + offset);
    handle[8] = (unsigned long)p;
    handle[9] = (unsigned long)entrypoint;
  }

  return handle;
}

cothread_t co_create(unsigned int size, void (*entrypoint)(void)) {
  void* memory = malloc(size);
  if(!memory) return (cothread_t)0;
  return co_derive(memory, size, entrypoint);
}

void co_delete(cothread_t handle) {
  free(handle);
}

void co_switch(cothread_t handle) {
  cothread_t co_previous_handle = co_active_handle;
  co_swap(co_active_handle = handle, co_previous_handle);
}

int co_serializable() {
  return 1;
}

#ifdef __cplusplus
}
#endif
