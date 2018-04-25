#pragma once
#include <wiiu/types.h>

#ifdef __cplusplus
extern "C" {
#endif

uint8_t SwapAtomic8(uint8_t *ptr, uint8_t value);
uint16_t SwapAtomic16(uint16_t *ptr, uint16_t value);
uint32_t SwapAtomic32(uint32_t *ptr, uint32_t value);

#ifdef __cplusplus
}
#endif
