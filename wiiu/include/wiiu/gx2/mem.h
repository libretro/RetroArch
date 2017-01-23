#pragma once
#include <wiiu/types.h>
#include "enum.h"

#ifdef __cplusplus
extern "C" {
#endif

void GX2Invalidate(GX2InvalidateMode mode, void *buffer, uint32_t size);

#ifdef __cplusplus
}
#endif
