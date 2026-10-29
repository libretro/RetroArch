/* Stand-ins for the coreinit read-modify-writes retro_atomic.h's Wii U
 * backend calls, built on the host's own atomics, each counting its
 * calls so the test can see which ones an operation made. */

#include <stdint.h>

#include "coreinit_stub.h"

volatile int coreinit_stub_calls[COREINIT_STUB_COUNT];

static void counted(int which)
{
   __atomic_fetch_add(&coreinit_stub_calls[which], 1, __ATOMIC_RELAXED);
}

int32_t OSCompareAndSwapAtomic(volatile uint32_t *p, uint32_t expected,
      uint32_t desired)
{
   counted(COREINIT_STUB_CAS);
   return __atomic_compare_exchange_n(p, &expected, desired, 0,
         __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST) ? 1 : 0;
}

uint32_t OSSwapAtomic(volatile uint32_t *p, uint32_t v)
{
   counted(COREINIT_STUB_SWAP);
   return __atomic_exchange_n(p, v, __ATOMIC_SEQ_CST);
}

int32_t OSAddAtomic(volatile int32_t *p, int32_t v)
{
   counted(COREINIT_STUB_ADD);
   return (int32_t)__atomic_fetch_add((volatile uint32_t*)p, (uint32_t)v,
         __ATOMIC_SEQ_CST);
}

uint32_t OSAndAtomic(volatile uint32_t *p, uint32_t v)
{
   counted(COREINIT_STUB_AND);
   return __atomic_fetch_and(p, v, __ATOMIC_SEQ_CST);
}

uint32_t OSOrAtomic(volatile uint32_t *p, uint32_t v)
{
   counted(COREINIT_STUB_OR);
   return __atomic_fetch_or(p, v, __ATOMIC_SEQ_CST);
}
