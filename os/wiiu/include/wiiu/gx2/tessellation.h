#pragma once
#include <wiiu/types.h>
#include "enum.h"

#ifdef __cplusplus
extern "C" {
#endif

void GX2SetTessellation(GX2TessellationMode tessellationMode, GX2PrimitiveMode primitiveMode, GX2IndexType indexType);
void GX2SetMinTessellationLevel(float min);
void GX2SetMaxTessellationLevel(float max);

#ifdef __cplusplus
}
#endif
