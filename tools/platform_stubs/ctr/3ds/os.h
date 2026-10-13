/* Hermetic compile-only stand-in for libctru's <3ds/os.h>: the clock
 * libretro-common reads and the memory-region queries mem_stats.c
 * makes. */
#ifndef STUB_ctr_3ds_os_h
#define STUB_ctr_3ds_os_h
#include <stdint.h>
typedef enum { MEMREGION_ALL = 0 } MemRegion;
uint64_t osGetTime(void);
uint32_t osGetMemRegionSize(MemRegion region);
uint32_t osGetMemRegionFree(MemRegion region);
#endif
