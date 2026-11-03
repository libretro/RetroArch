/*
  libco.amd64 (2009-10-12)
  author: byuu
  license: public domain
*/

#define LIBCO_C
#include <libco.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdint.h>

#if defined(__GNUC__) && !defined(_WIN32) && !defined(__cplusplus)
#define CO_USE_INLINE_ASM
#endif

#ifdef _WIN32
#define CO_STATIC_CODE
#ifdef _MSC_VER
/* Keep initialized-data attributes separate from MSVC function sections. */
#pragma section(".libcotx", read, execute)
#define CO_CODE_ALLOC __declspec(allocate(".libcotx")) __declspec(align(16))
#else
#define CO_CODE_ALLOC __attribute__((section(".text$libco"), aligned(16)))
#endif
#endif

#if !defined(CO_USE_INLINE_ASM) && !defined(CO_STATIC_CODE)
#include <retro_atomic.h>
#endif

#if defined(__GENODE__)
#define CO_STORAGE_BYTES 512
#elif defined(_WIN32)
#define CO_STORAGE_BYTES 272
#else
#define CO_STORAGE_BYTES 64
#endif

#ifndef LIBCO_CONTEXT_ALLOC
#define LIBCO_CONTEXT_ALLOC(size) malloc(size)
#endif
#ifndef LIBCO_CONTEXT_FREE
#define LIBCO_CONTEXT_FREE(handle) free(handle)
#endif
#ifndef LIBCO_STACK_TOP
#define LIBCO_STACK_TOP(handle,size) ((char*)(handle) + (size))
#endif
#ifndef LIBCO_STACK_BOTTOM
#define LIBCO_STACK_BOTTOM(handle) ((char*)(handle) + CO_STORAGE_BYTES)
#endif
#ifndef LIBCO_STACK_ALLOCATION
#define LIBCO_STACK_ALLOCATION(handle) (handle)
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _MSC_VER
__declspec(align(16))
#else
__attribute__((aligned(16)))
#endif
static thread_local long long co_active_buffer[CO_STORAGE_BYTES / sizeof(long long)];
static thread_local cothread_t co_active_handle = 0;

#ifdef _MSC_VER
CO_CODE_ALLOC static const unsigned char co_control_code[] = {
   0x0f,0xae,0x5c,0x24,0x08, /* stmxcsr [rsp+8] */
   0xd9,0x7c,0x24,0x0c,      /* fnstcw [rsp+12] */
   0x8b,0x44,0x24,0x08,      /* mov eax,[rsp+8] */
   0x0f,0xb7,0x54,0x24,0x0c, /* movzx edx,word [rsp+12] */
   0x48,0xc1,0xe2,0x20,      /* shl rdx,32 */
   0x48,0x09,0xd0,           /* or rax,rdx */
   0xc3                     /* ret */
};
static uint64_t (*const co_read_control)(void) =
      (uint64_t (*)(void))co_control_code;
#endif

#ifdef _WIN32
/* ABI: Win64 */
  /* On windows handle is allocated by malloc and there it's guaranteed to
     have at least 16-byte alignment. Hence we don't need to align
     it in order to use movaps.  */
CO_CODE_ALLOC static const unsigned char co_swap_function[] = {
    0x48, 0x89, 0x22,              /* mov [rdx],rsp          */
    0x48, 0x8b, 0x21,              /* mov rsp,[rcx]          */
    0x58,                          /* pop rax                */
    0x48, 0x89, 0x6a, 0x08,        /* mov [rdx+ 8],rbp       */
    0x48, 0x89, 0x72, 0x10,        /* mov [rdx+16],rsi       */
    0x48, 0x89, 0x7a, 0x18,        /* mov [rdx+24],rdi       */
    0x48, 0x89, 0x5a, 0x20,        /* mov [rdx+32],rbx       */
    0x4c, 0x89, 0x62, 0x28,        /* mov [rdx+40],r12       */
    0x4c, 0x89, 0x6a, 0x30,        /* mov [rdx+48],r13       */
    0x4c, 0x89, 0x72, 0x38,        /* mov [rdx+56],r14       */
    0x4c, 0x89, 0x7a, 0x40,        /* mov [rdx+64],r15       */
    0x0f, 0xae, 0x5a, 0x48,        /* stmxcsr [rdx+72]       */
    0xd9, 0x7a, 0x4c,              /* fnstcw [rdx+76]        */
    0x65, 0x4c, 0x8b, 0x14, 0x25, 0x30, 0x00, 0x00, 0x00, /* mov r10,gs:[48] */
    0x4d, 0x8b, 0x5a, 0x20,        /* mov r11,[r10+32]       */
    0x4c, 0x89, 0x9a, 0xf0, 0x00, 0x00, 0x00, /* mov [rdx+240],r11 */
    0x4d, 0x8b, 0x9a, 0x78, 0x14, 0x00, 0x00, /* mov r11,[r10+5240] */
    0x4c, 0x89, 0x9a, 0xf8, 0x00, 0x00, 0x00, /* mov [rdx+248],r11 */
    0xf3, 0x41, 0x0f, 0x6f, 0x42, 0x08, /* movdqu xmm0,[r10+8] */
    0xf3, 0x0f, 0x7f, 0x82, 0x00, 0x01, 0x00, 0x00, /* movdqu [rdx+256],xmm0 */
  #if !defined(LIBCO_NO_SSE)
    0x0f, 0x29, 0x72, 0x50,        /* movaps [rdx+ 80],xmm6  */
    0x0f, 0x29, 0x7a, 0x60,        /* movaps [rdx+ 96],xmm7  */
    0x44, 0x0f, 0x29, 0x42, 0x70,  /* movaps [rdx+112],xmm8  */
    0x48, 0x83, 0xc2, 0x70,        /* add rdx,112            */
    0x44, 0x0f, 0x29, 0x4a, 0x10,  /* movaps [rdx+ 16],xmm9  */
    0x44, 0x0f, 0x29, 0x52, 0x20,  /* movaps [rdx+ 32],xmm10 */
    0x44, 0x0f, 0x29, 0x5a, 0x30,  /* movaps [rdx+ 48],xmm11 */
    0x44, 0x0f, 0x29, 0x62, 0x40,  /* movaps [rdx+ 64],xmm12 */
    0x44, 0x0f, 0x29, 0x6a, 0x50,  /* movaps [rdx+ 80],xmm13 */
    0x44, 0x0f, 0x29, 0x72, 0x60,  /* movaps [rdx+ 96],xmm14 */
    0x44, 0x0f, 0x29, 0x7a, 0x70,  /* movaps [rdx+112],xmm15 */
  #endif
    0x48, 0x8b, 0x69, 0x08,        /* mov rbp,[rcx+ 8]       */
    0x48, 0x8b, 0x71, 0x10,        /* mov rsi,[rcx+16]       */
    0x48, 0x8b, 0x79, 0x18,        /* mov rdi,[rcx+24]       */
    0x48, 0x8b, 0x59, 0x20,        /* mov rbx,[rcx+32]       */
    0x4c, 0x8b, 0x61, 0x28,        /* mov r12,[rcx+40]       */
    0x4c, 0x8b, 0x69, 0x30,        /* mov r13,[rcx+48]       */
    0x4c, 0x8b, 0x71, 0x38,        /* mov r14,[rcx+56]       */
    0x4c, 0x8b, 0x79, 0x40,        /* mov r15,[rcx+64]       */
    0x0f, 0xae, 0x51, 0x48,        /* ldmxcsr [rcx+72]       */
    0xd9, 0x69, 0x4c,              /* fldcw [rcx+76]         */
    0x4c, 0x8b, 0x99, 0xf0, 0x00, 0x00, 0x00, /* mov r11,[rcx+240] */
    0x4d, 0x89, 0x5a, 0x20,        /* mov [r10+32],r11       */
    0x4c, 0x8b, 0x99, 0xf8, 0x00, 0x00, 0x00, /* mov r11,[rcx+248] */
    0x4d, 0x89, 0x9a, 0x78, 0x14, 0x00, 0x00, /* mov [r10+5240],r11 */
    0xf3, 0x0f, 0x6f, 0x81, 0x00, 0x01, 0x00, 0x00, /* movdqu xmm0,[rcx+256] */
    0xf3, 0x41, 0x0f, 0x7f, 0x42, 0x08, /* movdqu [r10+8],xmm0 */
  #if !defined(LIBCO_NO_SSE)
    0x0f, 0x28, 0x71, 0x50,        /* movaps xmm6, [rcx+ 80] */
    0x0f, 0x28, 0x79, 0x60,        /* movaps xmm7, [rcx+ 96] */
    0x44, 0x0f, 0x28, 0x41, 0x70,  /* movaps xmm8, [rcx+112] */
    0x48, 0x83, 0xc1, 0x70,        /* add rcx,112            */
    0x44, 0x0f, 0x28, 0x49, 0x10,  /* movaps xmm9, [rcx+ 16] */
    0x44, 0x0f, 0x28, 0x51, 0x20,  /* movaps xmm10,[rcx+ 32] */
    0x44, 0x0f, 0x28, 0x59, 0x30,  /* movaps xmm11,[rcx+ 48] */
    0x44, 0x0f, 0x28, 0x61, 0x40,  /* movaps xmm12,[rcx+ 64] */
    0x44, 0x0f, 0x28, 0x69, 0x50,  /* movaps xmm13,[rcx+ 80] */
    0x44, 0x0f, 0x28, 0x71, 0x60,  /* movaps xmm14,[rcx+ 96] */
    0x44, 0x0f, 0x28, 0x79, 0x70,  /* movaps xmm15,[rcx+112] */
  #endif
    0xff, 0xe0,                    /* jmp rax                */
};

#else
/* ABI: SystemV */
#ifndef CO_USE_INLINE_ASM
static const unsigned char co_swap_function[] = {
  0x48, 0x89, 0x26,                                 /* mov    [rsi],rsp      */
  0x48, 0x8b, 0x27,                                 /* mov    rsp,[rdi]      */
  0x58,                                             /* pop    rax            */
  0x48, 0x89, 0x6e, 0x08,                           /* mov    [rsi+0x08],rbp */
  0x48, 0x89, 0x5e, 0x10,                           /* mov    [rsi+0x10],rbx */
  0x4c, 0x89, 0x66, 0x18,                           /* mov    [rsi+0x18],r12 */
  0x4c, 0x89, 0x6e, 0x20,                           /* mov    [rsi+0x20],r13 */
  0x4c, 0x89, 0x76, 0x28,                           /* mov    [rsi+0x28],r14 */
  0x4c, 0x89, 0x7e, 0x30,                           /* mov    [rsi+0x30],r15 */
  0x0f, 0xae, 0x5e, 0x38,                           /* stmxcsr [rsi+0x38]    */
  0xd9, 0x7e, 0x3c,                                 /* fnstcw [rsi+0x3c]     */
  0x48, 0x8b, 0x6f, 0x08,                           /* mov    rbp,[rdi+0x08] */
  0x48, 0x8b, 0x5f, 0x10,                           /* mov    rbx,[rdi+0x10] */
  0x4c, 0x8b, 0x67, 0x18,                           /* mov    r12,[rdi+0x18] */
  0x4c, 0x8b, 0x6f, 0x20,                           /* mov    r13,[rdi+0x20] */
  0x4c, 0x8b, 0x77, 0x28,                           /* mov    r14,[rdi+0x28] */
  0x4c, 0x8b, 0x7f, 0x30,                           /* mov    r15,[rdi+0x30] */
  0x0f, 0xae, 0x57, 0x38,                           /* ldmxcsr [rdi+0x38]    */
  0xd9, 0x6f, 0x3c,                                 /* fldcw [rdi+0x3c]      */
  0xff, 0xe0,                                       /* jmp    rax            */
};

#include <unistd.h>
#include <sys/mman.h>

static int co_init(void)
{
   unsigned long long addr = (unsigned long long)co_swap_function;
   unsigned long long base = addr - (addr % sysconf(_SC_PAGESIZE));
   unsigned long long size = (addr - base) + sizeof(co_swap_function);
   return mprotect((void*)base, size, PROT_READ | PROT_EXEC) == 0;
}
#endif
#endif

#ifndef CO_USE_INLINE_ASM
static void (*const co_swap)(cothread_t, cothread_t) =
      (void (*)(cothread_t, cothread_t))co_swap_function;
#ifndef CO_STATIC_CODE
static retro_atomic_int_t co_ready;
#endif
#endif

static void co_init_fp(cothread_t handle)
{
#ifdef _WIN32
#define CO_FP_OFFSET 72
#else
#define CO_FP_OFFSET 56
#endif
#ifdef _MSC_VER
   uint64_t state = co_read_control();
   memcpy((char*)handle + CO_FP_OFFSET, &state, sizeof(state));
#else
   __asm__ __volatile__(
         "stmxcsr %c1(%0)\n\tfnstcw %c2(%0)"
         : : "r"(handle), "i"(CO_FP_OFFSET), "i"(CO_FP_OFFSET + 4)
         : "memory");
#endif
#undef CO_FP_OFFSET
}

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
   unsigned int overhead = CO_STORAGE_BYTES;
#ifdef _WIN32
   overhead += 32;
#endif
   if (size > UINT_MAX - overhead - 15)
      return NULL;
#if !defined(CO_USE_INLINE_ASM) && !defined(CO_STATIC_CODE)
   if (!retro_atomic_load_acquire_int(&co_ready))
   {
      if (!co_init())
         return NULL;
      retro_atomic_store_release_int(&co_ready, 1);
   }
#endif

   if (!co_active_handle)
      co_active_handle = &co_active_buffer;
   size = (size + overhead + 15) & ~15U;

#ifdef __GENODE__
   if ((handle = (cothread_t)genode_alloc_secondary_stack(size)))
   {
      long long *p        = (long long*)((char*)handle); /* OS returns top of stack */
      *--p                = (long long)crash;            /* crash if entrypoint returns */
      *--p                = (long long)entrypoint;       /* start of function */
      *(long long*)handle = (long long)p;                /* stack pointer */
   }
#else
   if ((handle = (cothread_t)LIBCO_CONTEXT_ALLOC(size)))
   {
      long long *p = (long long*)LIBCO_STACK_TOP(handle,size);
#ifdef _WIN32
      p -= 4;                                         /* Win64 caller home space */
#endif
      *--p = (long long)crash;                           /* crash if entrypoint returns */
      *--p = (long long)entrypoint;                      /* start of function */
      *(long long*)handle = (long long)p;                /* stack pointer */
   }
#endif

   if (handle)
   {
      co_init_fp(handle);
#ifdef _WIN32
      {
         uintptr_t *state = (uintptr_t*)((char*)handle + 240);
         state[0] = 0;
         state[1] = (uintptr_t)LIBCO_STACK_ALLOCATION(handle);
         state[2] = (uintptr_t)LIBCO_STACK_TOP(handle,size);
         state[3] = (uintptr_t)LIBCO_STACK_BOTTOM(handle);
      }
#endif
   }
   return handle;
}

void co_delete(cothread_t handle)
{
#ifdef __GENODE__
   genode_free_secondary_stack(handle);
#else
   LIBCO_CONTEXT_FREE(handle);
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
/* The context swap takes both handles as arguments and never touches
 * co_active_handle itself.  The previous revision defined the whole of
 * co_switch here, including
 *    mov rsi, [rip+co_active_handle]
 * - a plain PC-relative global access baked into the asm text.  With
 * the default (empty) thread_local that is correct, which is why it
 * went unnoticed; under LIBCO_MP the C compiler places the variable in
 * .tbss and every C-side access goes through the TLS machinery, while
 * this instruction still addressed it PC-relative.  The linker accepts
 * the mismatched R_X86_64_PC32 against the TLS symbol without a word,
 * and at run time the asm reads and writes the TLS *initialisation
 * template* while each thread's C code uses its own copy: all threads
 * share the asm's notion of the active coroutine (the exact race
 * LIBCO_MP exists to prevent) and writes corrupt the template that
 * seeds TLS for threads created later.  Keeping the handle traffic in
 * C - exactly how the aarch64, ARM and Windows paths already do it -
 * lets the compiler emit the correct addressing for whichever storage
 * class thread_local expands to. */
__asm__(
".intel_syntax noprefix         \n"
".globl " ASM_PREFIX "co_swap_amd64          \n"
ASM_PREFIX "co_swap_amd64:                 \n"
"mov [rsi],rsp                  \n"
"mov [rsi+0x08],rbp             \n"
"mov [rsi+0x10],rbx             \n"
"mov [rsi+0x18],r12             \n"
"mov [rsi+0x20],r13             \n"
"mov [rsi+0x28],r14             \n"
"mov [rsi+0x30],r15             \n"
"stmxcsr [rsi+0x38]             \n"
"fnstcw [rsi+0x3c]              \n"
"mov rsp,[rdi]                  \n"
"mov rbp,[rdi+0x08]             \n"
"mov rbx,[rdi+0x10]             \n"
"mov r12,[rdi+0x18]             \n"
"mov r13,[rdi+0x20]             \n"
"mov r14,[rdi+0x28]             \n"
"mov r15,[rdi+0x30]             \n"
"ldmxcsr [rdi+0x38]             \n"
"fldcw [rdi+0x3c]               \n"
"pop rax                        \n"
"jmp rax                        \n"
".att_syntax                    \n"
);

void co_swap_amd64(cothread_t to, cothread_t from);

void co_switch(cothread_t handle)
{
   register cothread_t co_previous_handle = co_active_handle;
   co_swap_amd64(co_active_handle = handle, co_previous_handle);
}
#endif

#ifdef __cplusplus
}
#endif
