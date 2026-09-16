/* Hermetic compile-only stand-in: only the names libretro-common uses. */
#ifndef STUB_ps3_sys_sys_time_h
#define STUB_ps3_sys_sys_time_h
#include <stdint.h>
int64_t sys_time_get_system_time(void);
#endif
