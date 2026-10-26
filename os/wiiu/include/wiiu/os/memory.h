#pragma once
#include <wiiu/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum shared_data_type_t
{
    SHARED_FONT_CHINESE,
    SHARED_FONT_KOREAN,
    SHARED_FONT_DEFAULT,
    SHARED_FONT_TAIWAN
} shared_data_type_t;

BOOL OSGetSharedData(shared_data_type_t type, uint32_t flags, void **dst, uint32_t *size);

void *OSBlockMove(void *dst, const void *src, uint32_t size, BOOL flush);
void *OSBlockSet(void *dst, uint8_t val, uint32_t size);
uint32_t OSEffectiveToPhysical(void *vaddr);
void *OSAllocFromSystem(uint32_t size, int align);
void OSFreeToSystem(void *ptr);

#ifdef __cplusplus
}
#endif

/** @} */
