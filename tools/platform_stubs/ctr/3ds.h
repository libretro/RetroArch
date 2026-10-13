/* Hermetic stand-in for libctru's <3ds.h>: only what retro_timers.h,
 * features_cpu.h, mem_stats.c and ctr_joypad.c name, so a matrix lane
 * can compile the 3DS branches without devkitARM present. */
#ifndef STUB_3DS_H
#define STUB_3DS_H
#include <stdint.h>
#include <3ds/services/hid.h>
#include <3ds/services/irrst.h>
typedef int64_t s64;
typedef uint64_t u64;
void svcSleepThread(s64 ns);
u64 osGetTime(void);
#endif
