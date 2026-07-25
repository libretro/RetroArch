#ifndef STUB_3DS_OS_H
#define STUB_3DS_OS_H
#include <stdint.h>
typedef enum { MEMREGION_ALL = 0 } MemRegion;
uint32_t osGetMemRegionSize(MemRegion region);
uint32_t osGetMemRegionFree(MemRegion region);
#endif
