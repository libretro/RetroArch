#pragma once
#include <wut.h>
#include <gx2/enum.h>
#include "resource.h"

/**
 * \defgroup gx2r_draw Draw
 * \ingroup gx2r
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct GX2RBuffer GX2RBuffer;

void
GX2RSetAttributeBuffer(GX2RBuffer *buffer,
                       uint32_t index,
                       uint32_t stride,
                       uint32_t offset);

void
GX2RDrawIndexed(GX2PrimitiveMode mode,
                GX2RBuffer *buffer,
                GX2IndexType indexType,
                uint32_t count,
                uint32_t indexOffset,
                uint32_t vertexOffset,
                uint32_t numInstances);

#ifdef __cplusplus
}
#endif

/** @} */
