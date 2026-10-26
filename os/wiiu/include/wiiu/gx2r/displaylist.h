#pragma once
#include <wiiu/types.h>
#include "resource.h"
#include "buffer.h"

#ifdef __cplusplus
extern "C" {
#endif

void GX2RBeginDisplayListEx(GX2RBuffer *displayList, uint32_t unknown, GX2RResourceFlags flags);
uint32_t GX2REndDisplayList(GX2RBuffer *displayList);
void GX2RCallDisplayList(GX2RBuffer *displayList, uint32_t size);
void GX2RDirectCallDisplayList(GX2RBuffer *displayList, uint32_t size);

#ifdef __cplusplus
}
#endif
