#define LIBCO_C
#include "libco.h"
#include "settings.h"

#include <assert.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__clang__) || defined(__GNUC__)
  #define fastcall __attribute__((fastcall))
#elif defined(_MSC_VER)
  #define fastcall __fastcall
#else
  #error "libco: please define fastcall macro"
#endif

static thread_local long co_active_buffer[64];
static thread_local cothread_t co_active_handle = 0;
static void (fastcall *co_swap)(cothread_t, cothread_t) = 0;

#ifdef LIBCO_MPROTECT
  alignas(4096)
#else
  section(text)
#endif
/* ABI: fastcall */
static const unsigned char co_swap_function[4096] = {
  0x89, 0x22,        /* mov [edx],esp    */
  0x8b, 0x21,        /* mov esp,[ecx]    */
  0x58,              /* pop eax          */
  0x89, 0x6a, 0x04,  /* mov [edx+ 4],ebp */
  0x89, 0x72, 0x08,  /* mov [edx+ 8],esi */
  0x89, 0x7a, 0x0c,  /* mov [edx+12],edi */
  0x89, 0x5a, 0x10,  /* mov [edx+16],ebx */
  0x8b, 0x69, 0x04,  /* mov ebp,[ecx+ 4] */
  0x8b, 0x71, 0x08,  /* mov esi,[ecx+ 8] */
  0x8b, 0x79, 0x0c,  /* mov edi,[ecx+12] */
  0x8b, 0x59, 0x10,  /* mov ebx,[ecx+16] */
  0xff, 0xe0,        /* jmp eax          */
};

#ifdef _WIN32
  #include <windows.h>

  static void co_init() {
    #ifdef LIBCO_MPROTECT
    DWORD old_privileges;
    VirtualProtect((void*)co_swap_function, sizeof co_swap_function, PAGE_EXECUTE_READ, &old_privileges);
    #endif
  }
#else
  #include <unistd.h>
  #include <sys/mman.h>

  static void co_init() {
    #ifdef LIBCO_MPROTECT
    unsigned long addr = (unsigned long)co_swap_function;
    unsigned long base = addr - (addr % sysconf(_SC_PAGESIZE));
    unsigned long size = (addr - base) + sizeof co_swap_function;
    mprotect((void*)base, size, PROT_READ | PROT_EXEC);
    #endif
  }
#endif

static void crash() {
  assert(0);  /* called only if cothread_t entrypoint returns */
}

cothread_t co_active() {
  if(!co_active_handle) co_active_handle = &co_active_buffer;
  return co_active_handle;
}

cothread_t co_derive(void* memory, unsigned int size, void (*entrypoint)(void)) {
  cothread_t handle;
  if(!co_swap) {
    co_init();
    co_swap = (void (fastcall*)(cothread_t, cothread_t))co_swap_function;
  }
  if(!co_active_handle) co_active_handle = &co_active_buffer;

  if(handle = (cothread_t)memory) {
    unsigned int offset = (size & ~15) - 32;
    long *p = (long*)((char*)handle + offset);  /* seek to top of stack */
    *--p = (long)crash;                         /* crash if entrypoint returns */
    *--p = (long)entrypoint;                    /* start of function */
    *(long*)handle = (long)p;                   /* stack pointer */
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
  register cothread_t co_previous_handle = co_active_handle;
  co_swap(co_active_handle = handle, co_previous_handle);
}

int co_serializable() {
  return 1;
}

#ifdef __cplusplus
}
#endif
