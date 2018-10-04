/*
  libco.amd64 (2009-10-12)
  author: byuu
  license: public domain
*/

#define LIBCO_C
#include <libco.h>
#include <assert.h>
#include <stdlib.h>

#if defined(__GNUC__) && !defined(_WIN32) && !defined(__cplusplus)
#define CO_USE_INLINE_ASM
#endif

#ifdef __cplusplus
extern "C" {
#endif

static thread_local long long co_active_buffer[64];
static thread_local cothread_t co_active_handle = 0;
#ifndef CO_USE_INLINE_ASM
static void (*co_swap)(cothread_t, cothread_t) = 0;
#endif

#ifdef _WIN32
/* ABI: Win64 */
static unsigned char co_swap_function[] = {
  0x48, 0x89, 0x22,                                 /* mov    [rdx],rsp        */
  0x48, 0x8b, 0x21,                                 /* mov    rsp,[rcx]        */
  0x58,                                             /* pop    rax              */
  0x48, 0x89, 0x6a, 0x08,                           /* mov    [rdx+0x8],rbp    */
  0x48, 0x89, 0x72, 0x10,                           /* mov    [rdx+0x10],rsi   */
  0x48, 0x89, 0x7a, 0x18,                           /* mov    [rdx+0x18],rdi   */
  0x48, 0x89, 0x5a, 0x20,                           /* mov    [rdx+0x20],rbx   */
  0x4c, 0x89, 0x62, 0x28,                           /* mov    [rdx+0x28],r12   */
  0x4c, 0x89, 0x6a, 0x30,                           /* mov    [rdx+0x30],r13   */
  0x4c, 0x89, 0x72, 0x38,                           /* mov    [rdx+0x38],r14   */
  0x4c, 0x89, 0x7a, 0x40,                           /* mov    [rdx+0x40],r15   */
  0x48, 0x81, 0xc2, 0x80, 0x00, 0x00, 0x00,         /* add    rdx,0x80         */
  0x48, 0x83, 0xe2, 0xf0,                           /* and    rdx,-0x10        */
  0x0f, 0x29, 0x32,                                 /* movaps [rdx],xmm6       */
  0x0f, 0x29, 0x7a, 0x10,                           /* movaps [rdx+0x10],xmm7  */
  0x44, 0x0f, 0x29, 0x42, 0x20,                     /* movaps [rdx+0x20],xmm8  */
  0x44, 0x0f, 0x29, 0x4a, 0x30,                     /* movaps [rdx+0x30],xmm9  */
  0x44, 0x0f, 0x29, 0x52, 0x40,                     /* movaps [rdx+0x40],xmm10 */
  0x44, 0x0f, 0x29, 0x5a, 0x50,                     /* movaps [rdx+0x50],xmm11 */
  0x44, 0x0f, 0x29, 0x62, 0x60,                     /* movaps [rdx+0x60],xmm12 */
  0x44, 0x0f, 0x29, 0x6a, 0x70,                     /* movaps [rdx+0x70],xmm13 */
  0x44, 0x0f, 0x29, 0xb2, 0x80, 0x00, 0x00, 0x00,   /* movaps [rdx+0x80],xmm14 */
  0x44, 0x0f, 0x29, 0xba, 0x90, 0x00, 0x00, 0x00,   /* movaps [rdx+0x90],xmm15 */
  0x48, 0x8b, 0x69, 0x08,                           /* mov    rbp,[rcx+0x8]    */
  0x48, 0x8b, 0x71, 0x10,                           /* mov    rsi,[rcx+0x10]   */
  0x48, 0x8b, 0x79, 0x18,                           /* mov    rdi,[rcx+0x18]   */
  0x48, 0x8b, 0x59, 0x20,                           /* mov    rbx,[rcx+0x20]   */
  0x4c, 0x8b, 0x61, 0x28,                           /* mov    r12,[rcx+0x28]   */
  0x4c, 0x8b, 0x69, 0x30,                           /* mov    r13,[rcx+0x30]   */
  0x4c, 0x8b, 0x71, 0x38,                           /* mov    r14,[rcx+0x38]   */
  0x4c, 0x8b, 0x79, 0x40,                           /* mov    r15,[rcx+0x40]   */
  0x48, 0x81, 0xc1, 0x80, 0x00, 0x00, 0x00,         /* add    rcx,0x80         */
  0x48, 0x83, 0xe1, 0xf0,                           /* and    rcx,-0x10        */
  0x0f, 0x29, 0x31,                                 /* movaps [rcx],xmm6       */
  0x0f, 0x29, 0x79, 0x10,                           /* movaps [rcx+0x10],xmm7  */
  0x44, 0x0f, 0x29, 0x41, 0x20,                     /* movaps [rcx+0x20],xmm8  */
  0x44, 0x0f, 0x29, 0x49, 0x30,                     /* movaps [rcx+0x30],xmm9  */
  0x44, 0x0f, 0x29, 0x51, 0x40,                     /* movaps [rcx+0x40],xmm10 */
  0x44, 0x0f, 0x29, 0x59, 0x50,                     /* movaps [rcx+0x50],xmm11 */
  0x44, 0x0f, 0x29, 0x61, 0x60,                     /* movaps [rcx+0x60],xmm12 */
  0x44, 0x0f, 0x29, 0x69, 0x70,                     /* movaps [rcx+0x70],xmm13 */
  0x44, 0x0f, 0x29, 0xb1, 0x80, 0x00, 0x00, 0x00,   /* movaps [rcx+0x80],xmm14 */
  0x44, 0x0f, 0x29, 0xb9, 0x90, 0x00, 0x00, 0x00,   /* movaps [rcx+0x90],xmm15 */
  0xff, 0xe0,                                       /* jmp    rax              */
};

#include <windows.h>

static void co_init(void)
{
   DWORD old_privileges;
   VirtualProtect(co_swap_function,
         sizeof(co_swap_function), PAGE_EXECUTE_READWRITE, &old_privileges);
}
#else
/* ABI: SystemV */
#ifndef CO_USE_INLINE_ASM
static unsigned char co_swap_function[] = {
  0x48, 0x89, 0x26,                                 /* mov    [rsi],rsp      */
  0x48, 0x8b, 0x27,                                 /* mov    rsp,[rdi]      */
  0x58,                                             /* pop    rax            */
  0x48, 0x89, 0x6e, 0x08,                           /* mov    [rsi+0x08],rbp */
  0x48, 0x89, 0x5e, 0x10,                           /* mov    [rsi+0x10],rbx */
  0x4c, 0x89, 0x66, 0x18,                           /* mov    [rsi+0x18],r12 */
  0x4c, 0x89, 0x6e, 0x20,                           /* mov    [rsi+0x20],r13 */
  0x4c, 0x89, 0x76, 0x28,                           /* mov    [rsi+0x28],r14 */
  0x4c, 0x89, 0x7e, 0x30,                           /* mov    [rsi+0x30],r15 */
  0x48, 0x8b, 0x6f, 0x08,                           /* mov    rbp,[rdi+0x08] */
  0x48, 0x8b, 0x5f, 0x10,                           /* mov    rbx,[rdi+0x10] */
  0x4c, 0x8b, 0x67, 0x18,                           /* mov    r12,[rdi+0x18] */
  0x4c, 0x8b, 0x6f, 0x20,                           /* mov    r13,[rdi+0x20] */
  0x4c, 0x8b, 0x77, 0x28,                           /* mov    r14,[rdi+0x28] */
  0x4c, 0x8b, 0x7f, 0x30,                           /* mov    r15,[rdi+0x30] */
  0xff, 0xe0,                                       /* jmp    rax            */
};

#include <unistd.h>
#include <sys/mman.h>

static void co_init(void)
{
   unsigned long long addr = (unsigned long long)co_swap_function;
   unsigned long long base = addr - (addr % sysconf(_SC_PAGESIZE));
   unsigned long long size = (addr - base) + sizeof(co_swap_function);
   mprotect((void*)base, size, PROT_READ | PROT_WRITE | PROT_EXEC);
}
#else
static void co_init(void) {}
#endif
#endif

static void crash(void)
{
  assert(0); /* called only if cothread_t entrypoint returns */
}

cothread_t co_active(void)
{
  if (!co_active_handle)
     co_active_handle = &co_active_buffer;
  return co_active_handle;
}

cothread_t co_create(unsigned int size, void (*entrypoint)(void))
{
   cothread_t handle;

#ifndef CO_USE_INLINE_ASM
   if(!co_swap)
   {
      co_init();
      co_swap = (void (*)(cothread_t, cothread_t))co_swap_function;
   }
#endif

   if (!co_active_handle)
      co_active_handle = &co_active_buffer;
   size += 512; /* allocate additional space for storage */
   size &= ~15; /* align stack to 16-byte boundary */

#ifdef __GENODE__
   if((handle = (cothread_t)genode_alloc_secondary_stack(size)))
   {
      long long *p = (long long*)((char*)handle); /* OS returns top of stack */
      *--p = (long long)crash;                    /* crash if entrypoint returns */
      *--p = (long long)entrypoint;               /* start of function */
      *(long long*)handle = (long long)p;         /* stack pointer */
   }
#else
   if((handle = (cothread_t)malloc(size)))
   {
      long long *p = (long long*)((char*)handle + size); /* seek to top of stack */
      *--p = (long long)crash;                           /* crash if entrypoint returns */
      *--p = (long long)entrypoint;                      /* start of function */
      *(long long*)handle = (long long)p;                /* stack pointer */
   }
#endif

   return handle;
}

void co_delete(cothread_t handle)
{
#ifdef __GENODE__
   genode_free_secondary_stack(handle);
#else
   free(handle);
#endif
}

#ifndef CO_USE_INLINE_ASM
void co_switch(cothread_t handle)
{
  register cothread_t co_previous_handle = co_active_handle;
  co_swap(co_active_handle = handle, co_previous_handle);
}
#else
#ifdef __APPLE__
#define ASM_PREFIX "_"
#else
#define ASM_PREFIX ""
#endif
__asm__(
".intel_syntax noprefix         \n"
".globl " ASM_PREFIX "co_switch              \n"
ASM_PREFIX "co_switch:                     \n"
"mov rsi, [rip+" ASM_PREFIX "co_active_handle]\n"
"mov [rsi],rsp                  \n"
"mov [rsi+0x08],rbp             \n"
"mov [rsi+0x10],rbx             \n"
"mov [rsi+0x18],r12             \n"
"mov [rsi+0x20],r13             \n"
"mov [rsi+0x28],r14             \n"
"mov [rsi+0x30],r15             \n"
"mov [rip+" ASM_PREFIX "co_active_handle], rdi\n"
"mov rsp,[rdi]                  \n"
"mov rbp,[rdi+0x08]             \n"
"mov rbx,[rdi+0x10]             \n"
"mov r12,[rdi+0x18]             \n"
"mov r13,[rdi+0x20]             \n"
"mov r14,[rdi+0x28]             \n"
"mov r15,[rdi+0x30]             \n"
"ret                            \n"
".att_syntax                    \n"
);
#endif

#ifdef __cplusplus
}
#endif
