/* Hermetic compile-only stand-in: only the names libretro-common uses. */
#ifndef STUB_wiiu_wiiu_os_time_h
#define STUB_wiiu_wiiu_os_time_h
#include <stdint.h>
int64_t OSGetSystemTime(void);
#define ticks_to_us(t) ((int64_t)(t) / 62)
#endif
