#pragma once
#include <wiiu/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
   GX2R_RESOURCE_BIND_TEXTURE             = 1 << 0,
   GX2R_RESOURCE_BIND_COLOR_BUFFER        = 1 << 1,
   GX2R_RESOURCE_BIND_DEPTH_BUFFER        = 1 << 2,
   GX2R_RESOURCE_BIND_SCAN_BUFFER         = 1 << 3,
   GX2R_RESOURCE_BIND_VERTEX_BUFFER       = 1 << 4,
   GX2R_RESOURCE_BIND_INDEX_BUFFER        = 1 << 5,
   GX2R_RESOURCE_BIND_UNIFORM_BLOCK       = 1 << 6,
   GX2R_RESOURCE_BIND_SHADER_PROGRAM      = 1 << 7,
   GX2R_RESOURCE_BIND_STREAM_OUTPUT       = 1 << 8,
   GX2R_RESOURCE_BIND_DISPLAY_LIST        = 1 << 9,
   GX2R_RESOURCE_BIND_GS_RING_BUFFER      = 1 << 10,

   GX2R_RESOURCE_USAGE_CPU_READ           = 1 << 11,
   GX2R_RESOURCE_USAGE_CPU_WRITE          = 1 << 12,
   GX2R_RESOURCE_USAGE_GPU_READ           = 1 << 13,
   GX2R_RESOURCE_USAGE_GPU_WRITE          = 1 << 14,
   GX2R_RESOURCE_USAGE_DMA_READ           = 1 << 15,
   GX2R_RESOURCE_USAGE_DMA_WRITE          = 1 << 16,
   GX2R_RESOURCE_USAGE_FORCE_MEM1         = 1 << 17,
   GX2R_RESOURCE_USAGE_FORCE_MEM2         = 1 << 18,

   GX2R_RESOURCE_DISABLE_CPU_INVALIDATE   = 1 << 20,
   GX2R_RESOURCE_DISABLE_GPU_INVALIDATE   = 1 << 21,

   GX2R_RESOURCE_LOCKED_READ_ONLY         = 1 << 22,
   GX2R_RESOURCE_USER_MEMORY              = 1 << 29,
   GX2R_RESOURCE_LOCKED                   = 1 << 30,
} GX2RResourceFlags;

#ifdef __cplusplus
}
#endif

/** @} */
