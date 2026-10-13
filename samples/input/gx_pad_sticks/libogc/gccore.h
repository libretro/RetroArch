/* Stand-in for libogc's gccore.h: the types and system calls the gx
 * joypad driver uses; the test defines the calls. */
#ifndef GX_JOYPAD_TEST_GCCORE_H
#define GX_JOYPAD_TEST_GCCORE_H

#include <stdint.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int8_t   s8;
typedef int16_t  s16;
typedef int32_t  s32;
typedef unsigned char ubyte;

typedef void (*resetcallback)(u32 irq, void *ctx);
typedef void (*powercallback)(void);

resetcallback SYS_SetResetCallback(resetcallback cb);
powercallback SYS_SetPowerCallback(powercallback cb);

#endif
