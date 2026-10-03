/* Hermetic stand-in for libctru's <3ds.h>: only what retro_timers.h
 * and features_cpu.h name, so a matrix lane can compile the 3DS
 * branches of libretro-common without devkitARM present. */
#ifndef STUB_3DS_H
#define STUB_3DS_H
#include <stdint.h>
typedef int64_t s64;
typedef uint64_t u64;
void svcSleepThread(s64 ns);
u64 osGetTime(void);
#endif
