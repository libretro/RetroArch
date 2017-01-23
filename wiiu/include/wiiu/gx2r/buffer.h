#pragma once
#include <wiiu/types.h>
#include "resource.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
   GX2RResourceFlags flags;
   uint32_t elemSize;
   uint32_t elemCount;
   void *buffer;
} GX2RBuffer;

BOOL X2RBufferExists(GX2RBuffer *buffer);
BOOL X2RCreateBuffer(GX2RBuffer *buffer);
BOOL GX2RCreateBufferUserMemory(GX2RBuffer *buffer, void *memory, uint32_t size);
void GX2RDestroyBufferEx(GX2RBuffer *buffer, GX2RResourceFlags flags);
uint32_t GX2RGetBufferAlignment(GX2RResourceFlags flags);
uint32_t GX2RGetBufferAllocationSize(GX2RBuffer *buffer);
void GX2RInvalidateBuffer(GX2RBuffer *buffer, GX2RResourceFlags flags);
void *GX2RLockBufferEx(GX2RBuffer *buffer, GX2RResourceFlags flags);
void GX2RUnlockBufferEx(GX2RBuffer *buffer, GX2RResourceFlags flags);
void GX2RSetVertexUniformBlock(GX2RBuffer *buffer, uint32_t location, uint32_t offset);
void GX2RSetPixelUniformBlock(GX2RBuffer *buffer, uint32_t location, uint32_t offset);
void GX2RSetGeometryUniformBlock(GX2RBuffer *buffer, uint32_t location, uint32_t offset);

#ifdef __cplusplus
}
#endif
