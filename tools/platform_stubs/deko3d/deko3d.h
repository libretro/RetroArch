/* Stand-in for devkitPro's <deko3d.h>.
 *
 * Only what gfx/drivers/deko3d.c and libretro_deko3d.h name, with the
 * names and call shapes deko3d gives them; the structs' layouts are
 * this header's own, since everything that reads them is the test's
 * model of the device. As with the libnx stand-in, this proves the
 * driver is well formed against the API and lets a host test run it -
 * not that the real SDK matches. */
#ifndef DEKO3D_STUB_H
#define DEKO3D_STUB_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct DkDevice_T   *DkDevice;
typedef struct DkMemBlock_T *DkMemBlock;
typedef struct DkCmdBuf_T   *DkCmdBuf;
typedef struct DkQueue_T    *DkQueue;
typedef struct DkSwapchain_T *DkSwapchain;
typedef uint64_t  DkGpuAddr;
typedef uintptr_t DkCmdList;
typedef uint32_t  DkResHandle;

typedef enum
{
   DkResult_Success = 0,
   DkResult_Fail,
   DkResult_Timeout
} DkResult;

#define DK_MEMBLOCK_ALIGNMENT            0x1000
#define DK_SHADER_CODE_ALIGNMENT         0x100
#define DK_SHADER_CODE_UNUSABLE_SIZE     0x400
#define DK_UNIFORM_BUF_ALIGNMENT         0x100
#define DK_IMAGE_LINEAR_STRIDE_ALIGNMENT 32

typedef void (*DkDebugFunc)(void *userData, const char *context,
      DkResult result, const char *message);

enum
{
   DkDeviceFlags_DepthZeroToOne  = 0U << 8,
   DkDeviceFlags_OriginLowerLeft = 1U << 9
};

typedef struct DkDeviceMaker
{
   void       *userData;
   DkDebugFunc cbDebug;
   uint32_t    flags;
} DkDeviceMaker;

static inline void dkDeviceMakerDefaults(DkDeviceMaker *maker)
{
   maker->userData = NULL;
   maker->cbDebug  = NULL;
   maker->flags    = DkDeviceFlags_DepthZeroToOne;
}

enum
{
   DkMemBlockFlags_CpuUncached = 1U << 0,
   DkMemBlockFlags_CpuCached   = 2U << 0,
   DkMemBlockFlags_GpuCached   = 1U << 2,
   DkMemBlockFlags_Code        = 1U << 4,
   DkMemBlockFlags_Image       = 1U << 5
};

typedef struct DkMemBlockMaker
{
   DkDevice device;
   uint32_t size;
   uint32_t flags;
   void    *storage;
} DkMemBlockMaker;

static inline void dkMemBlockMakerDefaults(DkMemBlockMaker *maker,
      DkDevice device, uint32_t size)
{
   maker->device  = device;
   maker->size    = size;
   maker->flags   = DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached;
   maker->storage = NULL;
}

typedef struct DkCmdBufMaker
{
   DkDevice device;
} DkCmdBufMaker;

static inline void dkCmdBufMakerDefaults(DkCmdBufMaker *maker, DkDevice device)
{
   maker->device = device;
}

enum { DkQueueFlags_Graphics = 1U << 0 };

typedef struct DkQueueMaker
{
   DkDevice device;
   uint32_t flags;
} DkQueueMaker;

static inline void dkQueueMakerDefaults(DkQueueMaker *maker, DkDevice device)
{
   maker->device = device;
   maker->flags  = DkQueueFlags_Graphics;
}

/* The fence's state lives in the test's model */
typedef struct DkFence
{
   uint64_t serial;
   uint32_t valid;
} DkFence;

typedef struct DkShader
{
   const void *control;
   DkMemBlock  code;
   uint32_t    offset;
} DkShader;

typedef struct DkShaderMaker
{
   DkMemBlock  codeMem;
   const void *control;
   uint32_t    codeOffset;
} DkShaderMaker;

static inline void dkShaderMakerDefaults(DkShaderMaker *maker,
      DkMemBlock codeMem, uint32_t codeOffset)
{
   maker->codeMem    = codeMem;
   maker->control    = NULL;
   maker->codeOffset = codeOffset;
}

typedef enum
{
   DkImageFormat_None = 0,
   DkImageFormat_R8_Unorm,
   DkImageFormat_RGBA8_Unorm,
   DkImageFormat_RGBA4_Unorm,
   DkImageFormat_BGR565_Unorm,
   DkImageFormat_BGRX8_Unorm,
   DkImageFormat_BGRA8_Unorm
} DkImageFormat;

enum
{
   DkImageFlags_PitchLinear   = 1U << 2,
   DkImageFlags_UsageRender   = 1U << 8,
   DkImageFlags_UsagePresent  = 1U << 10,
   DkImageFlags_Usage2DEngine = 1U << 11,
   DkImageFlags_HwCompression = 1U << 4
};

typedef struct DkImageLayoutMaker
{
   DkDevice      device;
   uint32_t      flags;
   DkImageFormat format;
   uint32_t      dimensions[3];
   uint32_t      pitchStride;
} DkImageLayoutMaker;

static inline void dkImageLayoutMakerDefaults(DkImageLayoutMaker *maker,
      DkDevice device)
{
   maker->device        = device;
   maker->flags         = 0;
   maker->format        = DkImageFormat_None;
   maker->dimensions[0] = 0;
   maker->dimensions[1] = 0;
   maker->dimensions[2] = 0;
   maker->pitchStride   = 0;
}

typedef struct DkImageLayout
{
   DkImageLayoutMaker maker;
} DkImageLayout;

typedef struct DkImage
{
   DkImageLayout layout;
   DkMemBlock    mem;
   uint32_t      offset;
} DkImage;

typedef enum
{
   DkImageSwizzle_Zero = 0,
   DkImageSwizzle_One,
   DkImageSwizzle_Red,
   DkImageSwizzle_Green,
   DkImageSwizzle_Blue,
   DkImageSwizzle_Alpha
} DkImageSwizzle;

typedef struct DkImageView
{
   const DkImage *pImage;
   DkImageSwizzle swizzle[4];
} DkImageView;

static inline void dkImageViewDefaults(DkImageView *obj, const DkImage *pImage)
{
   obj->pImage     = pImage;
   obj->swizzle[0] = DkImageSwizzle_Red;
   obj->swizzle[1] = DkImageSwizzle_Green;
   obj->swizzle[2] = DkImageSwizzle_Blue;
   obj->swizzle[3] = DkImageSwizzle_Alpha;
}

/* What a descriptor names, as the model reads it back */
typedef struct DkImageDescriptor
{
   DkImage        image;
   DkImageSwizzle swizzle[4];
} DkImageDescriptor;

typedef enum { DkFilter_Nearest = 1, DkFilter_Linear = 2 } DkFilter;
typedef enum { DkWrapMode_Repeat = 0, DkWrapMode_ClampToEdge = 2 } DkWrapMode;

typedef struct DkSampler
{
   DkFilter   minFilter;
   DkFilter   magFilter;
   DkWrapMode wrapMode[3];
} DkSampler;

static inline void dkSamplerDefaults(DkSampler *obj)
{
   obj->minFilter   = DkFilter_Nearest;
   obj->magFilter   = DkFilter_Nearest;
   obj->wrapMode[0] = DkWrapMode_Repeat;
   obj->wrapMode[1] = DkWrapMode_Repeat;
   obj->wrapMode[2] = DkWrapMode_Repeat;
}

typedef struct DkSamplerDescriptor
{
   DkSampler sampler;
} DkSamplerDescriptor;

static inline DkResHandle dkMakeTextureHandle(uint32_t imageId, uint32_t samplerId)
{
   return (imageId & 0xFFFFF) | (samplerId << 20);
}

typedef struct DkImageRect
{
   uint32_t x, y, z;
   uint32_t width, height, depth;
} DkImageRect;

typedef struct DkViewport
{
   float x, y, width, height, near, far;
} DkViewport;

typedef struct DkScissor
{
   uint32_t x, y, width, height;
} DkScissor;

typedef enum
{
   DkStage_Vertex   = 0,
   DkStage_Fragment = 4
} DkStage;
#define DkStageFlag_GraphicsMask 0x1FU

typedef enum
{
   DkBarrier_None      = 0,
   DkBarrier_Fragments = 3,
   DkBarrier_Full      = 5
} DkBarrier;

enum
{
   DkInvalidateFlags_Image       = 1U << 0,
   DkInvalidateFlags_Descriptors = 1U << 3
};

typedef enum
{
   DkPrimitive_Triangles     = 4,
   DkPrimitive_TriangleStrip = 5
} DkPrimitive;

typedef enum { DkVtxAttribSize_2x32 = 0x04, DkVtxAttribSize_4x32 = 0x01 } DkVtxAttribSize;
typedef enum { DkVtxAttribType_Float = 7 } DkVtxAttribType;

typedef struct DkVtxAttribState
{
   uint32_t bufferId : 5;
   uint32_t          : 1;
   uint32_t isFixed  : 1;
   uint32_t offset   : 14;
   uint32_t size     : 6;
   uint32_t type     : 3;
   uint32_t          : 1;
   uint32_t isBgra   : 1;
} DkVtxAttribState;

typedef struct DkVtxBufferState
{
   uint32_t stride;
   uint32_t divisor;
} DkVtxBufferState;

typedef struct DkBufExtents
{
   DkGpuAddr addr;
   uint32_t  size;
} DkBufExtents;

typedef enum { DkFace_None = 0, DkFace_Front = 1, DkFace_Back = 2 } DkFace;

typedef struct DkRasterizerState
{
   DkFace cullMode;
} DkRasterizerState;

static inline void dkRasterizerStateDefaults(DkRasterizerState *state)
{
   state->cullMode = DkFace_Back;
}

typedef struct DkDepthStencilState
{
   bool depthTestEnable;
   bool depthWriteEnable;
} DkDepthStencilState;

static inline void dkDepthStencilStateDefaults(DkDepthStencilState *state)
{
   state->depthTestEnable  = true;
   state->depthWriteEnable = true;
}

typedef struct DkColorState
{
   uint32_t blendEnableMask;
} DkColorState;

static inline void dkColorStateDefaults(DkColorState *state)
{
   state->blendEnableMask = 0;
}

static inline void dkColorStateSetBlendEnable(DkColorState *state,
      uint32_t id, bool enable)
{
   if (enable)
      state->blendEnableMask |= 1U << id;
   else
      state->blendEnableMask &= ~(1U << id);
}

enum { DkColorMask_RGBA = 0xF };

typedef struct DkColorWriteState
{
   uint32_t masks;
} DkColorWriteState;

static inline void dkColorWriteStateDefaults(DkColorWriteState *state)
{
   state->masks = 0xFFFFFFFFU;
}

static inline void dkColorWriteStateSetMask(DkColorWriteState *state,
      uint32_t id, uint32_t colorWriteMask)
{
   state->masks = (state->masks & ~(0xFU << (id * 4)))
      | ((colorWriteMask & 0xFU) << (id * 4));
}

typedef enum
{
   DkBlendFactor_Zero        = 1,
   DkBlendFactor_One         = 2,
   DkBlendFactor_SrcAlpha    = 5,
   DkBlendFactor_InvSrcAlpha = 6,
   DkBlendFactor_DstColor    = 9
} DkBlendFactor;

typedef struct DkBlendState
{
   DkBlendFactor srcColorBlendFactor;
   DkBlendFactor dstColorBlendFactor;
   DkBlendFactor srcAlphaBlendFactor;
   DkBlendFactor dstAlphaBlendFactor;
} DkBlendState;

static inline void dkBlendStateDefaults(DkBlendState *state)
{
   state->srcColorBlendFactor = DkBlendFactor_SrcAlpha;
   state->dstColorBlendFactor = DkBlendFactor_InvSrcAlpha;
   state->srcAlphaBlendFactor = DkBlendFactor_SrcAlpha;
   state->dstAlphaBlendFactor = DkBlendFactor_InvSrcAlpha;
}

enum
{
   DkBlitFlag_FilterNearest = 0U << 0,
   DkBlitFlag_FilterLinear  = 1U << 0,
   DkBlitFlag_FlipY         = 1U << 2,
   DkBlitFlag_ModeBlit      = 0U << 4
};

typedef struct DkSwapchainMaker
{
   DkDevice              device;
   void                 *nativeWindow;
   DkImage const *const *pImages;
   uint32_t              numImages;
} DkSwapchainMaker;

static inline void dkSwapchainMakerDefaults(DkSwapchainMaker *maker,
      DkDevice device, void *nativeWindow, DkImage const *const pImages[],
      uint32_t numImages)
{
   maker->device       = device;
   maker->nativeWindow = nativeWindow;
   maker->pImages      = pImages;
   maker->numImages    = numImages;
}

DkDevice   dkDeviceCreate(DkDeviceMaker const *maker);
void       dkDeviceDestroy(DkDevice obj);

DkMemBlock dkMemBlockCreate(DkMemBlockMaker const *maker);
void       dkMemBlockDestroy(DkMemBlock obj);
void      *dkMemBlockGetCpuAddr(DkMemBlock obj);
DkGpuAddr  dkMemBlockGetGpuAddr(DkMemBlock obj);
DkResult   dkMemBlockFlushCpuCache(DkMemBlock obj, uint32_t offset, uint32_t size);

DkResult   dkFenceWait(DkFence *obj, int64_t timeout_ns);

DkCmdBuf   dkCmdBufCreate(DkCmdBufMaker const *maker);
void       dkCmdBufDestroy(DkCmdBuf obj);
void       dkCmdBufAddMemory(DkCmdBuf obj, DkMemBlock mem, uint32_t offset, uint32_t size);
DkCmdList  dkCmdBufFinishList(DkCmdBuf obj);
void       dkCmdBufClear(DkCmdBuf obj);
void       dkCmdBufSignalFence(DkCmdBuf obj, DkFence *fence, bool flush);
void       dkCmdBufBarrier(DkCmdBuf obj, DkBarrier mode, uint32_t invalidateFlags);
void       dkCmdBufBindShaders(DkCmdBuf obj, uint32_t stageMask,
      DkShader const *const shaders[], uint32_t numShaders);
void       dkCmdBufBindUniformBuffers(DkCmdBuf obj, DkStage stage,
      uint32_t firstId, DkBufExtents const buffers[], uint32_t numBuffers);
void       dkCmdBufBindTextures(DkCmdBuf obj, DkStage stage, uint32_t firstId,
      DkResHandle const handles[], uint32_t numHandles);
void       dkCmdBufBindImageDescriptorSet(DkCmdBuf obj, DkGpuAddr setAddr, uint32_t numDescriptors);
void       dkCmdBufBindSamplerDescriptorSet(DkCmdBuf obj, DkGpuAddr setAddr, uint32_t numDescriptors);
void       dkCmdBufBindRenderTargets(DkCmdBuf obj, DkImageView const *const colorTargets[],
      uint32_t numColorTargets, DkImageView const *depthTarget);
void       dkCmdBufBindRasterizerState(DkCmdBuf obj, DkRasterizerState const *state);
void       dkCmdBufBindColorState(DkCmdBuf obj, DkColorState const *state);
void       dkCmdBufBindColorWriteState(DkCmdBuf obj, DkColorWriteState const *state);
void       dkCmdBufBindBlendStates(DkCmdBuf obj, uint32_t firstId,
      DkBlendState const states[], uint32_t numStates);
void       dkCmdBufBindDepthStencilState(DkCmdBuf obj, DkDepthStencilState const *state);
void       dkCmdBufBindVtxAttribState(DkCmdBuf obj, DkVtxAttribState const attribs[], uint32_t numAttribs);
void       dkCmdBufBindVtxBufferState(DkCmdBuf obj, DkVtxBufferState const buffers[], uint32_t numBuffers);
void       dkCmdBufBindVtxBuffers(DkCmdBuf obj, uint32_t firstId,
      DkBufExtents const buffers[], uint32_t numBuffers);
void       dkCmdBufSetViewports(DkCmdBuf obj, uint32_t firstId,
      DkViewport const viewports[], uint32_t numViewports);
void       dkCmdBufSetScissors(DkCmdBuf obj, uint32_t firstId,
      DkScissor const scissors[], uint32_t numScissors);
void       dkCmdBufClearColor(DkCmdBuf obj, uint32_t targetId, uint32_t clearMask, const void *clearData);
void       dkCmdBufDraw(DkCmdBuf obj, DkPrimitive prim, uint32_t vertexCount,
      uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance);
void       dkCmdBufPushConstants(DkCmdBuf obj, DkGpuAddr uboAddr, uint32_t uboSize,
      uint32_t offset, uint32_t size, const void *data);
void       dkCmdBufBlitImage(DkCmdBuf obj, DkImageView const *srcView,
      DkImageRect const *srcRect, DkImageView const *dstView,
      DkImageRect const *dstRect, uint32_t flags, uint32_t factor);

static inline void dkCmdBufBindUniformBuffer(DkCmdBuf obj, DkStage stage,
      uint32_t id, DkGpuAddr bufAddr, uint32_t bufSize)
{
   DkBufExtents e;
   e.addr = bufAddr;
   e.size = bufSize;
   dkCmdBufBindUniformBuffers(obj, stage, id, &e, 1);
}

static inline void dkCmdBufBindTexture(DkCmdBuf obj, DkStage stage,
      uint32_t id, DkResHandle handle)
{
   dkCmdBufBindTextures(obj, stage, id, &handle, 1);
}

static inline void dkCmdBufBindVtxBuffer(DkCmdBuf obj, uint32_t id,
      DkGpuAddr bufAddr, uint32_t bufSize)
{
   DkBufExtents e;
   e.addr = bufAddr;
   e.size = bufSize;
   dkCmdBufBindVtxBuffers(obj, id, &e, 1);
}

static inline void dkCmdBufClearColorFloat(DkCmdBuf obj, uint32_t targetId,
      uint32_t clearMask, float red, float green, float blue, float alpha)
{
   float data[4];
   data[0] = red;
   data[1] = green;
   data[2] = blue;
   data[3] = alpha;
   dkCmdBufClearColor(obj, targetId, clearMask, data);
}

DkQueue    dkQueueCreate(DkQueueMaker const *maker);
void       dkQueueDestroy(DkQueue obj);
void       dkQueueWaitFence(DkQueue obj, DkFence *fence);
void       dkQueueSubmitCommands(DkQueue obj, DkCmdList cmds);
void       dkQueueWaitIdle(DkQueue obj);
int        dkQueueAcquireImage(DkQueue obj, DkSwapchain swapchain);
void       dkQueuePresentImage(DkQueue obj, DkSwapchain swapchain, int imageSlot);

void       dkShaderInitialize(DkShader *obj, DkShaderMaker const *maker);

void       dkImageLayoutInitialize(DkImageLayout *obj, DkImageLayoutMaker const *maker);
uint64_t   dkImageLayoutGetSize(DkImageLayout const *obj);
void       dkImageInitialize(DkImage *obj, DkImageLayout const *layout,
      DkMemBlock memBlock, uint32_t offset);
void       dkImageDescriptorInitialize(DkImageDescriptor *obj,
      DkImageView const *view, bool usesLoadOrStore, bool decayMS);
void       dkSamplerDescriptorInitialize(DkSamplerDescriptor *obj, DkSampler const *sampler);

DkSwapchain dkSwapchainCreate(DkSwapchainMaker const *maker);
void        dkSwapchainDestroy(DkSwapchain obj);
void        dkSwapchainSetSwapInterval(DkSwapchain obj, uint32_t interval);

#endif
