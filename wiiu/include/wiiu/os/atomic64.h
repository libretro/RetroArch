#pragma once
#include <wiiu/types.h>

#ifdef __cplusplus
extern "C" {
#endif

uint64_t OSGetAtomic64(uint64_t *ptr);
uint64_t OSSetAtomic64(uint64_t *ptr, uint64_t value);
BOOL OSCompareAndSwapAtomic64(uint64_t *ptr, uint64_t compare, uint64_t value);
BOOL OSCompareAndSwapAtomicEx64(uint64_t *ptr, uint64_t compare, uint64_t value, uint64_t *old);
uint64_t OSSwapAtomic64(uint64_t *ptr, uint64_t value);
int64_t OSAddAtomic64(int64_t *ptr, int64_t value);
uint64_t OSAndAtomic64(uint64_t *ptr, uint64_t value);
uint64_t OSOrAtomic64(uint64_t *ptr, uint64_t value);
uint64_t OSXorAtomic64(uint64_t *ptr, uint64_t value);
BOOL OSTestAndClearAtomic64(uint64_t *ptr, uint32_t bit);
BOOL OSTestAndSetAtomic64(uint64_t *ptr, uint32_t bit);

#ifdef __cplusplus
}
#endif
