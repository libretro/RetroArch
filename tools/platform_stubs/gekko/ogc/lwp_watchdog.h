/* Hermetic compile-only stand-in: only the names libretro-common uses. */
#ifndef STUB_gekko_ogc_lwp_watchdog_h
#define STUB_gekko_ogc_lwp_watchdog_h
#include <stdint.h>
uint64_t gettime(void);
#define ticks_to_microsecs(t) ((uint64_t)(t) / 40)
#endif
