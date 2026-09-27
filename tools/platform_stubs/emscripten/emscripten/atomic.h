/* Compile-only stand-in for <emscripten/atomic.h>. Only the names the
 * audio worklet driver reaches, spelled as emscripten spells them. */
#ifndef EMSTUB_EMSCRIPTEN_ATOMIC_H
#define EMSTUB_EMSCRIPTEN_ATOMIC_H

#include <stdint.h>

uint32_t emscripten_atomic_add_u32(void *addr, uint32_t val);
uint32_t emscripten_atomic_load_u32(const void *addr);
uint32_t emscripten_atomic_store_u32(void *addr, uint32_t val);

#endif
