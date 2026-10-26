#pragma once
#include <wiiu/types.h>
#include "resource.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void *(*GX2RAllocFunction)(GX2RResourceFlags, uint32_t, uint32_t);
typedef void (*GX2RFreeFunction)(GX2RResourceFlags, void *);

void GX2RInvalidateMemory(GX2RResourceFlags flags, void *buffer, uint32_t size);
BOOL GX2RIsUserMemory(GX2RResourceFlags flags);
void GX2RSetAllocator(GX2RAllocFunction allocFn, GX2RFreeFunction freeFn);

#ifdef __cplusplus
}
#endif

/** @} */
