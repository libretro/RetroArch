#pragma once
#include <wiiu/types.h>

#ifdef __cplusplus
extern "C" {
#endif

void *OSBlockMove(void *dst, const void *src, uint32_t size, BOOL flush);
void *OSBlockSet(void *dst, uint8_t val, uint32_t size);
uint32_t OSEffectiveToPhysical(void *vaddr);
void *OSAllocFromSystem(uint32_t size, int align);
void OSFreeToSystem(void *ptr);

#ifdef __cplusplus
}
#endif

/** @} */
