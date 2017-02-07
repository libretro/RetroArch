#pragma once
#include <wiiu/types.h>

#ifdef __cplusplus
extern "C" {
#endif

uint32_t OSGetCoreCount();
uint32_t OSGetCoreId();
uint32_t OSGetMainCoreId();
BOOL OSIsMainCore();

#ifdef __cplusplus
}
#endif

/** @} */
