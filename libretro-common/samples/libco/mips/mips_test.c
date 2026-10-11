/* MIPS libco regression: return zero only after both contexts retain their
 * register state through repeated switches. See README.md for build commands. */
#include <libco.h>
#include <stdint.h>
#include <limits.h>

static cothread_t primary;
static volatile unsigned int progress;
static volatile int failed;

#if defined(__mips_hard_float)
#if defined(_ABI64) && _MIPS_SIM == _ABI64
#define FP_EVEN(OP) \
   OP " $f24, 0(%0)\n" OP " $f26, 8(%0)\n" \
   OP " $f28, 16(%0)\n" OP " $f30, 24(%0)\n"
#define FP_ODD(OP) \
   OP " $f25, 32(%0)\n" OP " $f27, 40(%0)\n" \
   OP " $f29, 48(%0)\n" OP " $f31, 56(%0)\n"
#define FP_CLOBBERS "f24", "f26", "f28", "f30", "f25", "f27", "f29", "f31"
#define FP_COUNT 8
#else
#if __mips == 1
#define FP_EVEN(OP) \
   OP " $f20, 0(%0)\n" \
   OP " $f21, 4(%0)\n" \
   OP " $f22, 8(%0)\n" \
   OP " $f23, 12(%0)\n" \
   OP " $f24, 16(%0)\n" \
   OP " $f25, 20(%0)\n" \
   OP " $f26, 24(%0)\n" \
   OP " $f27, 28(%0)\n" \
   OP " $f28, 32(%0)\n" \
   OP " $f29, 36(%0)\n" \
   OP " $f30, 40(%0)\n" \
   OP " $f31, 44(%0)\n"
#else
#define FP_EVEN(OP) \
   OP " $f20, 0(%0)\n" OP " $f22, 8(%0)\n" \
   OP " $f24, 16(%0)\n" OP " $f26, 24(%0)\n" \
   OP " $f28, 32(%0)\n" OP " $f30, 40(%0)\n"
#endif
#define FP_ODD(OP) ""
#define FP_CLOBBERS "f20", "f22", "f24", "f26", "f28", "f30"
#define FP_COUNT 6
#endif
#if __mips == 1
#define SET_FP(P) __asm__ volatile(FP_EVEN("lwc1") "nop\n" \
   : : "r"(P) : FP_CLOBBERS, "memory")
#define GET_FP(P) __asm__ volatile(FP_EVEN("swc1") \
   : : "r"(P) : "memory")
#else
#define SET_FP(P) __asm__ volatile(FP_EVEN("ldc1") FP_ODD("ldc1") \
   : : "r"(P) : FP_CLOBBERS, "memory")
#define GET_FP(P) __asm__ volatile(FP_EVEN("sdc1") FP_ODD("sdc1") \
   : : "r"(P) : "memory")
#endif
#else
#define FP_COUNT 1
#define SET_FP(P) ((void)(P))
#define GET_FP(P) ((void)(P))
#endif

static void seed(uint64_t *values, unsigned int salt)
{
   unsigned int i;
   for (i = 0; i < FP_COUNT; ++i)
      values[i] = UINT64_C(0x3ff123456789abcd) + ((uint64_t)salt << 32) + salt + i;
}

static int same(const uint64_t *a, const uint64_t *b)
{
#if defined(__mips_hard_float)
   unsigned int i;
   for (i = 0; i < FP_COUNT; ++i)
      if (a[i] != b[i])
         return 0;
#else
   (void)a;
   (void)b;
#endif
   return 1;
}

static void worker(void)
{
   uint64_t expected[FP_COUNT], actual[FP_COUNT];
   unsigned int i;
   seed(expected, 0x100);
   SET_FP(expected);
   for (i = 0; i < 16; ++i)
   {
      /* Accessing globals here also exercises a freshly entered PIC function. */
      progress = i + 1;
      co_switch(primary);
      GET_FP(actual);
      if (!same(actual, expected))
         failed = 1;
   }
   progress = 17;
   co_switch(primary);
   /* Entry functions must never return. */
   for (;;)
      co_switch(primary);
}

int main(void)
{
   uint64_t expected[FP_COUNT], actual[FP_COUNT];
   unsigned int i;
   cothread_t child;
   primary = co_active();
   if (!primary || co_active() != primary)
      return 1;
   if (co_create(UINT_MAX, worker))
      return 2;
   child = co_create(16384, worker);
   if (!child)
      return 3;
   seed(expected, 0);
   SET_FP(expected);
   for (i = 0; i < 17; ++i)
   {
      co_switch(child);
      GET_FP(actual);
      if (!same(actual, expected) || progress != i + 1 || failed ||
          co_active() != primary)
         return 4;
   }
   co_delete(child);
   return 0;
}
