/* Compile-only stand-in for <emscripten/wasm_worker.h> and the
 * threading primitives it pulls in. The lock and the condition
 * variable are macros over a volatile word in emscripten too. */
#ifndef EMSTUB_EMSCRIPTEN_WASM_WORKER_H
#define EMSTUB_EMSCRIPTEN_WASM_WORKER_H

#include <stdint.h>
#include <emscripten/atomic.h>

#ifndef __cplusplus
#include <stdbool.h>
#endif

#define EMSCRIPTEN_WASM_WORKER_ID_PARENT 0

#define emscripten_lock_t    volatile uint32_t
#define emscripten_condvar_t volatile uint32_t

#define EMSCRIPTEN_LOCK_T_STATIC_INITIALIZER    0
#define EMSCRIPTEN_CONDVAR_T_STATIC_INITIALIZER ((int)(0))

void emscripten_lock_init(emscripten_lock_t *lock);
bool emscripten_lock_wait_acquire(emscripten_lock_t *lock, int64_t maxWaitNanoseconds);
bool emscripten_lock_busyspin_wait_acquire(emscripten_lock_t *lock, double maxWaitMilliseconds);
void emscripten_lock_release(emscripten_lock_t *lock);

void emscripten_condvar_init(emscripten_condvar_t *condvar);
bool emscripten_condvar_wait(emscripten_condvar_t *condvar, emscripten_lock_t *lock, int64_t maxWaitNanoseconds);
void emscripten_condvar_signal(emscripten_condvar_t *condvar, uint32_t numWaitersToSignal);

#endif
