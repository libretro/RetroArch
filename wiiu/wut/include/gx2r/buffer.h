#pragma once
#include <wut.h>
#include "resource.h"

/**
 * \defgroup gx2r_buffer Buffer
 * \ingroup gx2r
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct GX2RBuffer GX2RBuffer;

struct GX2RBuffer
{
   GX2RResourceFlags flags;
   uint32_t elemSize;
   uint32_t elemCount;
   void *buffer;
};
CHECK_SIZE(GX2RBuffer, 0x10);
CHECK_OFFSET(GX2RBuffer, 0x00, flags);
CHECK_OFFSET(GX2RBuffer, 0x04, elemSize);
CHECK_OFFSET(GX2RBuffer, 0x08, elemCount);
CHECK_OFFSET(GX2RBuffer, 0x0C, buffer);

BOOL
GX2RBufferExists(GX2RBuffer *buffer);

BOOL
GX2RCreateBuffer(GX2RBuffer *buffer);

BOOL
GX2RCreateBufferUserMemory(GX2RBuffer *buffer,
                           void *memory,
                           uint32_t size);

void
GX2RDestroyBufferEx(GX2RBuffer *buffer,
                    GX2RResourceFlags flags);

uint32_t
GX2RGetBufferAlignment(GX2RResourceFlags flags);

uint32_t
GX2RGetBufferAllocationSize(GX2RBuffer *buffer);

void
GX2RInvalidateBuffer(GX2RBuffer *buffer,
                     GX2RResourceFlags flags);

void *
GX2RLockBufferEx(GX2RBuffer *buffer,
                 GX2RResourceFlags flags);

void
GX2RUnlockBufferEx(GX2RBuffer *buffer,
                   GX2RResourceFlags flags);

void
GX2RSetVertexUniformBlock(GX2RBuffer *buffer,
                          uint32_t location,
                          uint32_t offset);

void
GX2RSetPixelUniformBlock(GX2RBuffer *buffer,
                         uint32_t location,
                         uint32_t offset);

void
GX2RSetGeometryUniformBlock(GX2RBuffer *buffer,
                            uint32_t location,
                            uint32_t offset);

#ifdef __cplusplus
}
#endif

/** @} */
