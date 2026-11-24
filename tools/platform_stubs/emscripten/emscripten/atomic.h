/* Compile-only stand-in for <emscripten/atomic.h>. Only the names the
 * audio worklet driver reaches, spelled as emscripten spells them. */
#ifndef EMSTUB_EMSCRIPTEN_ATOMIC_H
#define EMSTUB_EMSCRIPTEN_ATOMIC_H

#include <stdint.h>

uint32_t emscripten_atomic_add_u32(void *addr, uint32_t val);
uint32_t emscripten_atomic_load_u32(const void *addr);
uint32_t emscripten_atomic_store_u32(void *addr, uint32_t val);

#define ATOMICS_WAIT_RESULT_T int
#define ATOMICS_WAIT_OK        0
#define ATOMICS_WAIT_NOT_EQUAL 1
#define ATOMICS_WAIT_TIMED_OUT 2
#define EMSCRIPTEN_NOTIFY_ALL_WAITERS (-1LL)

ATOMICS_WAIT_RESULT_T emscripten_atomic_wait_u32(void *addr, uint32_t expectedValue, int64_t maxWaitNanoseconds);
int emscripten_atomic_notify(void *addr, int64_t count);

#endif
