#pragma once
#include <wut.h>

/**
 * \defgroup gx2r_resource Resource
 * \ingroup gx2r
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef enum GX2RResourceFlags
{
   //! This resource is to be used as a texture
   GX2R_RESOURCE_BIND_TEXTURE             = 1 << 0,

   //! This resource is to be used as a colour buffer
   GX2R_RESOURCE_BIND_COLOR_BUFFER        = 1 << 1,

   //! This resource is to be used as a depth buffer
   GX2R_RESOURCE_BIND_DEPTH_BUFFER        = 1 << 2,

   //! This resource is to be used as a scan buffer
   GX2R_RESOURCE_BIND_SCAN_BUFFER         = 1 << 3,

   //! This resource is to be used as a vertex buffer
   GX2R_RESOURCE_BIND_VERTEX_BUFFER       = 1 << 4,

   //! This resource is to be used as a index buffer
   GX2R_RESOURCE_BIND_INDEX_BUFFER        = 1 << 5,

   //! This resource is to be used as a uniform block
   GX2R_RESOURCE_BIND_UNIFORM_BLOCK       = 1 << 6,

   //! This resource is to be used as a shader program
   GX2R_RESOURCE_BIND_SHADER_PROGRAM      = 1 << 7,

   //! This resource is to be used as a stream output
   GX2R_RESOURCE_BIND_STREAM_OUTPUT       = 1 << 8,

   //! This resource is to be used as a display list
   GX2R_RESOURCE_BIND_DISPLAY_LIST        = 1 << 9,

   //! This resource is to be used as a geometry shader ring buffer
   GX2R_RESOURCE_BIND_GS_RING_BUFFER      = 1 << 10,

   //! Invalidate resource for a CPU read
   GX2R_RESOURCE_USAGE_CPU_READ           = 1 << 11,

   //! Invalidate resource for a CPU write
   GX2R_RESOURCE_USAGE_CPU_WRITE          = 1 << 12,

   //! Invalidate resource for a GPU read
   GX2R_RESOURCE_USAGE_GPU_READ           = 1 << 13,

   //! Invalidate resource for a GPU write
   GX2R_RESOURCE_USAGE_GPU_WRITE          = 1 << 14,

   //! Invalidate resource for a DMA read
   GX2R_RESOURCE_USAGE_DMA_READ           = 1 << 15,

   //! Invalidate resource for a DMA write
   GX2R_RESOURCE_USAGE_DMA_WRITE          = 1 << 16,

   //! Force resource allocation to be in MEM1
   GX2R_RESOURCE_USAGE_FORCE_MEM1         = 1 << 17,

   //! Force resource allocation to be in MEM2
   GX2R_RESOURCE_USAGE_FORCE_MEM2         = 1 << 18,

   //! Disable CPU invalidation
   GX2R_RESOURCE_DISABLE_CPU_INVALIDATE   = 1 << 20,

   //! Disable GPU invalidation
   GX2R_RESOURCE_DISABLE_GPU_INVALIDATE   = 1 << 21,

   //! Resource is locked for read-only access
   GX2R_RESOURCE_LOCKED_READ_ONLY         = 1 << 22,

   //! Resource is to be allocated in user memory
   GX2R_RESOURCE_USER_MEMORY              = 1 << 29,

   //! Resource is locked for all access
   GX2R_RESOURCE_LOCKED                   = 1 << 30,
} GX2RResourceFlags;


#ifdef __cplusplus
}
#endif

/** @} */
