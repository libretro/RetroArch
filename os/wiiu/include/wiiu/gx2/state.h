#pragma once
#include <wiiu/types.h>
#include "enum.h"

#ifdef __cplusplus
extern "C" {
#endif

void GX2Init(uint32_t *attributes);
void GX2Shutdown();
void GX2Flush();

#ifdef __cplusplus
}
#endif
