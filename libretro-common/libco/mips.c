/*
  libco.mips (2020-04-20)
  author: phcoder
  copyright: Google
  license: MIT
*/

#define LIBCO_C
#include <libco.h>
#include <assert.h>
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

/* Use the compiler's register model, not the ISA revision: MIPS32 is
 * numbered 32, and even MIPS III/64 uses 32-bit GPR operations with o32.
 * n32 still has 32-bit pointers but uses 64-bit GPRs. */
#if defined(__mips64)
#define STORE_REG "sd"
#define LOAD_REG "ld"
#define _GPR_OFF(x) (x * 8)
typedef uint64_t gpr_t;
#else
/* 32-bit only variant.  */
#define STORE_REG "sw"
#define LOAD_REG "lw"
#define _GPR_OFF(x) (x * 4)
typedef uint32_t gpr_t;
#endif
#define _STR(x) #x
#define STR(x) _STR(x)
#define GPR_OFF(x) STR(_GPR_OFF(x))
/* Reserve twelve eight-byte FP slots even with soft float. The even slots
 * hold doubles (paired registers with FR=0); n64 also saves odd registers.
 * The following VFPU area stays sixteen-byte aligned in both GPR layouts. */
#define _FPR_OFF(x) ((x * 8) + _GPR_OFF(12))
#define FPR_OFF(x) STR(_FPR_OFF(x))
#define _VFP_OFF(x) ((x * 16) + _FPR_OFF(12))
#define VFP_OFF(x) STR(_VFP_OFF(x))
#if defined(__mips_hard_float)
#define HAVE_FP 1
#else
#define HAVE_FP 0
#endif
#ifdef __psp__
#define HAVE_VFP 1
#else
#define HAVE_VFP 0
#endif

#define CONTEXT_SIZE 0x300

static thread_local uint64_t co_active_buffer[CONTEXT_SIZE / 8] __attribute__((__aligned__((16))));
static thread_local cothread_t co_active_handle;

__asm__ (
      ".text\n"
      ".set push\n"
      ".set noreorder\n"
      ".align 4\n"
      ".globl co_switch_mips\n"
      ".globl _co_switch_mips\n"
      "co_switch_mips:\n"
      "_co_switch_mips:\n"
      STORE_REG " $s0, " GPR_OFF(0) "($a1)\n"
      STORE_REG " $s1, " GPR_OFF(1) "($a1)\n"
      STORE_REG " $s2, " GPR_OFF(2) "($a1)\n"
      STORE_REG " $s3, " GPR_OFF(3) "($a1)\n"
      STORE_REG " $s4, " GPR_OFF(4) "($a1)\n"
      STORE_REG " $s5, " GPR_OFF(5) "($a1)\n"
      STORE_REG " $s6, " GPR_OFF(6) "($a1)\n"
      STORE_REG " $s7, " GPR_OFF(7) "($a1)\n"
      STORE_REG " $gp, " GPR_OFF(8) "($a1)\n"
      STORE_REG " $sp, " GPR_OFF(9) "($a1)\n"
      STORE_REG " $fp, " GPR_OFF(10) "($a1)\n"
      STORE_REG " $ra, " GPR_OFF(11) "($a1)\n"
#if HAVE_FP
#if __mips == 1
      /* MIPS I has only word FP transfers; save both halves of each pair. */
      " swc1 $f20, " FPR_OFF(0) "($a1)\n"
      " swc1 $f21, " FPR_OFF(1) "($a1)\n"
      " swc1 $f22, " FPR_OFF(2) "($a1)\n"
      " swc1 $f23, " FPR_OFF(3) "($a1)\n"
      " swc1 $f24, " FPR_OFF(4) "($a1)\n"
      " swc1 $f25, " FPR_OFF(5) "($a1)\n"
      " swc1 $f26, " FPR_OFF(6) "($a1)\n"
      " swc1 $f27, " FPR_OFF(7) "($a1)\n"
      " swc1 $f28, " FPR_OFF(8) "($a1)\n"
      " swc1 $f29, " FPR_OFF(9) "($a1)\n"
      " swc1 $f30, " FPR_OFF(10) "($a1)\n"
      " swc1 $f31, " FPR_OFF(11) "($a1)\n"
#else
      " sdc1 $f20, " FPR_OFF(0) "($a1)\n"
      " sdc1 $f22, " FPR_OFF(2) "($a1)\n"
      " sdc1 $f24, " FPR_OFF(4) "($a1)\n"
      " sdc1 $f26, " FPR_OFF(6) "($a1)\n"
      " sdc1 $f28, " FPR_OFF(8) "($a1)\n"
      " sdc1 $f30, " FPR_OFF(10) "($a1)\n"
#if defined(_ABI64) && _MIPS_SIM == _ABI64
      /* n64 preserves all eight f24..f31 registers, not just the evens. */
      " sdc1 $f25, " FPR_OFF(5) "($a1)\n"
      " sdc1 $f27, " FPR_OFF(7) "($a1)\n"
      " sdc1 $f29, " FPR_OFF(9) "($a1)\n"
      " sdc1 $f31, " FPR_OFF(11) "($a1)\n"
#endif
#endif
#endif
#if HAVE_VFP
      " sv.q	c000, " VFP_OFF(0) "($a1), wt\n"
      " sv.q	c010, " VFP_OFF(1) "($a1), wt\n"
      " sv.q	c020, " VFP_OFF(2) "($a1), wt\n"
      " sv.q	c030, " VFP_OFF(3) "($a1), wt\n"
      " sv.q	c100, " VFP_OFF(4) "($a1), wt\n"
      " sv.q	c110, " VFP_OFF(5) "($a1), wt\n"
      " sv.q	c120, " VFP_OFF(6) "($a1), wt\n"
      " sv.q	c130, " VFP_OFF(7) "($a1), wt\n"
      " sv.q	c200, " VFP_OFF(8) "($a1), wt\n"
      " sv.q	c210, " VFP_OFF(9) "($a1), wt\n"
      " sv.q	c220, " VFP_OFF(10) "($a1), wt\n"
      " sv.q	c230, " VFP_OFF(11) "($a1), wt\n"
      " sv.q	c300, " VFP_OFF(12) "($a1), wt\n"
      " sv.q	c310, " VFP_OFF(13) "($a1), wt\n"
      " sv.q	c320, " VFP_OFF(14) "($a1), wt\n"
      " sv.q	c330, " VFP_OFF(15) "($a1), wt\n"
      " sv.q	c400, " VFP_OFF(16) "($a1), wt\n"
      " sv.q	c410, " VFP_OFF(17) "($a1), wt\n"
      " sv.q	c420, " VFP_OFF(18) "($a1), wt\n"
      " sv.q	c430, " VFP_OFF(19) "($a1), wt\n"
      " sv.q	c500, " VFP_OFF(20) "($a1), wt\n"
      " sv.q	c510, " VFP_OFF(21) "($a1), wt\n"
      " sv.q	c520, " VFP_OFF(22) "($a1), wt\n"
      " sv.q	c530, " VFP_OFF(23) "($a1), wt\n"
      " sv.q	c600, " VFP_OFF(24) "($a1), wt\n"
      " sv.q	c610, " VFP_OFF(25) "($a1), wt\n"
      " sv.q	c620, " VFP_OFF(26) "($a1), wt\n"
      " sv.q	c630, " VFP_OFF(27) "($a1), wt\n"
      " sv.q	c700, " VFP_OFF(28) "($a1), wt\n"
      " sv.q	c710, " VFP_OFF(29) "($a1), wt\n"
      " sv.q	c720, " VFP_OFF(30) "($a1), wt\n"
      " sv.q	c730, " VFP_OFF(31) "($a1), wt\n"
#endif
      LOAD_REG " $s0, " GPR_OFF(0) "($a0)\n"
      LOAD_REG " $s1, " GPR_OFF(1) "($a0)\n"
      LOAD_REG " $s2, " GPR_OFF(2) "($a0)\n"
      LOAD_REG " $s3, " GPR_OFF(3) "($a0)\n"
      LOAD_REG " $s4, " GPR_OFF(4) "($a0)\n"
      LOAD_REG " $s5, " GPR_OFF(5) "($a0)\n"
      LOAD_REG " $s6, " GPR_OFF(6) "($a0)\n"
      LOAD_REG " $s7, " GPR_OFF(7) "($a0)\n"
      LOAD_REG " $gp, " GPR_OFF(8) "($a0)\n"
      LOAD_REG " $sp, " GPR_OFF(9) "($a0)\n"
      LOAD_REG " $fp, " GPR_OFF(10) "($a0)\n"
      LOAD_REG " $ra, " GPR_OFF(11) "($a0)\n"
      " nop\n" /* MIPS I load delay before using ra, including soft float. */
#if HAVE_FP
#if __mips == 1
      /* MIPS I has only word FP transfers; save both halves of each pair. */
      " lwc1 $f20, " FPR_OFF(0) "($a0)\n"
      " lwc1 $f21, " FPR_OFF(1) "($a0)\n"
      " lwc1 $f22, " FPR_OFF(2) "($a0)\n"
      " lwc1 $f23, " FPR_OFF(3) "($a0)\n"
      " lwc1 $f24, " FPR_OFF(4) "($a0)\n"
      " lwc1 $f25, " FPR_OFF(5) "($a0)\n"
      " lwc1 $f26, " FPR_OFF(6) "($a0)\n"
      " lwc1 $f27, " FPR_OFF(7) "($a0)\n"
      " lwc1 $f28, " FPR_OFF(8) "($a0)\n"
      " lwc1 $f29, " FPR_OFF(9) "($a0)\n"
      " lwc1 $f30, " FPR_OFF(10) "($a0)\n"
      " lwc1 $f31, " FPR_OFF(11) "($a0)\n"
#else
      " ldc1 $f20, " FPR_OFF(0) "($a0)\n"
      " ldc1 $f22, " FPR_OFF(2) "($a0)\n"
      " ldc1 $f24, " FPR_OFF(4) "($a0)\n"
      " ldc1 $f26, " FPR_OFF(6) "($a0)\n"
      " ldc1 $f28, " FPR_OFF(8) "($a0)\n"
      " ldc1 $f30, " FPR_OFF(10) "($a0)\n"
#if defined(_ABI64) && _MIPS_SIM == _ABI64
      /* n64 preserves all eight f24..f31 registers, not just the evens. */
      " ldc1 $f25, " FPR_OFF(5) "($a0)\n"
      " ldc1 $f27, " FPR_OFF(7) "($a0)\n"
      " ldc1 $f29, " FPR_OFF(9) "($a0)\n"
      " ldc1 $f31, " FPR_OFF(11) "($a0)\n"
#endif
#endif
#endif
#if HAVE_VFP
      " lv.q	c000, " VFP_OFF(0) "($a0)\n"
      " lv.q	c010, " VFP_OFF(1) "($a0)\n"
      " lv.q	c020, " VFP_OFF(2) "($a0)\n"
      " lv.q	c030, " VFP_OFF(3) "($a0)\n"
      " lv.q	c100, " VFP_OFF(4) "($a0)\n"
      " lv.q	c110, " VFP_OFF(5) "($a0)\n"
      " lv.q	c120, " VFP_OFF(6) "($a0)\n"
      " lv.q	c130, " VFP_OFF(7) "($a0)\n"
      " lv.q	c200, " VFP_OFF(8) "($a0)\n"
      " lv.q	c210, " VFP_OFF(9) "($a0)\n"
      " lv.q	c220, " VFP_OFF(10) "($a0)\n"
      " lv.q	c230, " VFP_OFF(11) "($a0)\n"
      " lv.q	c300, " VFP_OFF(12) "($a0)\n"
      " lv.q	c310, " VFP_OFF(13) "($a0)\n"
      " lv.q	c320, " VFP_OFF(14) "($a0)\n"
      " lv.q	c330, " VFP_OFF(15) "($a0)\n"
      " lv.q	c400, " VFP_OFF(16) "($a0)\n"
      " lv.q	c410, " VFP_OFF(17) "($a0)\n"
      " lv.q	c420, " VFP_OFF(18) "($a0)\n"
      " lv.q	c430, " VFP_OFF(19) "($a0)\n"
      " lv.q	c500, " VFP_OFF(20) "($a0)\n"
      " lv.q	c510, " VFP_OFF(21) "($a0)\n"
      " lv.q	c520, " VFP_OFF(22) "($a0)\n"
      " lv.q	c530, " VFP_OFF(23) "($a0)\n"
      " lv.q	c600, " VFP_OFF(24) "($a0)\n"
      " lv.q	c610, " VFP_OFF(25) "($a0)\n"
      " lv.q	c620, " VFP_OFF(26) "($a0)\n"
      " lv.q	c630, " VFP_OFF(27) "($a0)\n"
      " lv.q	c700, " VFP_OFF(28) "($a0)\n"
      " lv.q	c710, " VFP_OFF(29) "($a0)\n"
      " lv.q	c720, " VFP_OFF(30) "($a0)\n"
      " lv.q	c730, " VFP_OFF(31) "($a0)\n"
#endif
      /* PIC entrypoints derive their GOT pointer from t9. On resumption
       * t9 is caller-saved, so setting it to the continuation is harmless. */
      " move $t9, $ra\n"
      " jr $ra\n"
      "  nop\n"
      ".align 4\n"
      ".globl store_gp\n"
      ".globl _store_gp\n"
      "store_gp:\n"
      "_store_gp:\n"
      STORE_REG " $gp, 0($a0)\n"
      " jr $ra\n"
      "  nop\n"
      ".set pop\n"
    );

/* ASM */
void co_switch_mips(cothread_t handle, cothread_t current);
void store_gp(gpr_t *s);

cothread_t co_create(unsigned int size, void (*entrypoint)(void))
{
   cothread_t handle = 0;
   gpr_t *ptr;

   /* Do not let context overhead or alignment wrap the allocation size. */
   if (size > UINT_MAX - CONTEXT_SIZE - 1023)
      return 0;
   size = (size + CONTEXT_SIZE + 1023) & ~1023;
#if defined(__APPLE__) || HAVE_POSIX_MEMALIGN >= 1
   if (posix_memalign(&handle, 1024, size) != 0)
      return 0;
#else
   handle = memalign(1024, size);
#endif

   if (!handle)
      return handle;

   ptr = (gpr_t*)handle;
   memset(ptr, 0, CONTEXT_SIZE);
   /* Non-volatiles.  */
   /* ptr[0],..., ptr[7] -> s0,..., s7 */
   store_gp(&ptr[8]); /* gp */
   ptr[9] = (gpr_t)(intptr_t)((uintptr_t)ptr + size - 16); /* sp  */
   /* ptr[10] is fp */
   ptr[11] = (gpr_t)(intptr_t)entrypoint; /* ra */
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
   co_switch_mips(co_active_handle = handle, co_previous_handle);
}

#ifdef __cplusplus
}
#endif
