/* Exercise ABI registers and independent floating-point modes on both stacks. */
#include <libco.h>
#include <stdint.h>
#include <stdio.h>
#include <limits.h>
#ifdef _WIN32
#include <windows.h>
#endif
#if defined(__aarch64__)
#define MODE_MASK 0xc00000U
#define CHILD_MODE 0x400000U
#define PARENT_MODE 0x800000U
static unsigned control_get(void)
{
   uint64_t value;
   __asm__ __volatile__("mrs %0, fpcr" : "=r"(value));
   return (unsigned)value;
}
static void control_set(unsigned control)
{
   uint64_t value = control;
   __asm__ __volatile__("msr fpcr, %0" : : "r"(value));
}
#else
#include <xmmintrin.h>
#define MODE_MASK 0x6000U
#define CHILD_MODE 0x2000U
#define PARENT_MODE 0x4000U
#define control_get() _mm_getcsr()
#define control_set(value) _mm_setcsr(value)
#endif
static cothread_t parent, child;
static unsigned errors, visits;
extern unsigned co_probe(cothread_t handle, uint64_t seed);
static void entry(void)
{
   control_set((control_get() & ~MODE_MASK) | CHILD_MODE);
   for (;;)
   {
      unsigned result;
      if (co_active() != child || (control_get() & MODE_MASK) != CHILD_MODE)
         ++errors;
      ++visits;
      result = co_probe(parent, UINT64_C(0x123456789abcdef0));
      errors += result;
   }
}
int main(void)
{
   unsigned i, original = control_get();
#ifdef _WIN32
   SetErrorMode(SEM_NOGPFAULTERRORBOX | SEM_FAILCRITICALERRORS);
#endif
   parent = co_active();
   if (!parent)
      return 2;
   control_set((original & ~MODE_MASK) | PARENT_MODE);
   child = co_create(65536, entry);
   if (!child)
      return 2;
   for (i = 0; i < 1000; ++i)
   {
      unsigned result = co_probe(child, UINT64_C(0xfedcba9876543210));
      errors += result;
      if (co_active() != parent || (control_get() & MODE_MASK) != PARENT_MODE)
         ++errors;
   }
   co_delete(child);
   control_set(original);
   if (visits != 1000)
      ++errors;
   printf("libco context state: %u errors\n", errors);
   return errors != 0;
}
