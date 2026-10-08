/* Compile-only stand-in for PSL1GHT's <ppu-types.h>: its fixed-width
 * names, as PSL1GHT defines them. Not a runtime shim. */
#ifndef PS3STUB_PPU_TYPES_H
#define PS3STUB_PPU_TYPES_H

#include <stdint.h>

typedef uint8_t           u8;
typedef uint16_t          u16;
typedef uint32_t          u32;
typedef uint64_t          u64;
typedef int8_t            s8;
typedef int16_t           s16;
typedef int32_t           s32;
typedef int64_t           s64;
typedef volatile u8       vu8;
typedef volatile u16      vu16;
typedef volatile u32      vu32;
typedef volatile u64      vu64;
typedef float             f32;
typedef double            f64;

#endif
