/* A stand-in XeSS library (libxess.dll on Windows, libxess.so on
 * Linux): exports the entry points gfx/intel/xess_loader.c resolves,
 * with the SDK's structures declared here again as xess.h, xess_d3d12.h
 * and xess_vk.h have them (packed to 8, their natural layout), and
 * records what it was asked (fake_xess.h). A context is a handle; the
 * input resolution of a mode is by the SDK's ratios (XeSS 1.3 and up),
 * the range a third of the output to the output. */
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "fake_xess.h"

#if defined(_WIN32)
#define EXPORT __declspec(dllexport)
#else
#define EXPORT __attribute__((visibility("default")))
#endif

typedef struct { uint16_t major, minor, patch, reserved; } sdk_version;
typedef struct { uint32_t x, y; } sdk_2d;
typedef struct
{
   sdk_2d outputResolution; int qualitySetting; uint32_t initFlags, creationNodeMask, visibleNodeMask;
   void *pTempBufferHeap; uint64_t bufferHeapOffset; void *pTempTextureHeap; uint64_t textureHeapOffset; void *pPipelineLibrary;
} sdk_d3d12_init;
typedef struct
{
   void *pColorTexture, *pVelocityTexture, *pDepthTexture, *pExposureScaleTexture, *pResponsivePixelMaskTexture, *pOutputTexture;
   float jitterOffsetX, jitterOffsetY, exposureScale; uint32_t resetHistory, inputWidth, inputHeight;
   sdk_2d inputColorBase, inputMotionVectorBase, inputDepthBase, inputResponsiveMaskBase, reserved0, outputColorBase;
   void *pDescriptorHeap; uint32_t descriptorHeapOffset;
} sdk_d3d12_execute;
typedef struct
{
   uint64_t imageView, image; struct { uint32_t aspectMask, baseMipLevel, levelCount, baseArrayLayer, layerCount; } subresourceRange;
   int format; unsigned int width, height;
} sdk_vk_image_view_info;
typedef struct
{
   sdk_vk_image_view_info colorTexture, velocityTexture, depthTexture, exposureScaleTexture, responsivePixelMaskTexture, outputTexture;
   float jitterOffsetX, jitterOffsetY, exposureScale; uint32_t resetHistory, inputWidth, inputHeight;
   sdk_2d inputColorBase, inputMotionVectorBase, inputDepthBase, inputResponsiveMaskBase, reserved0, outputColorBase;
} sdk_vk_execute;
typedef struct
{
   sdk_2d outputResolution; int qualitySetting; uint32_t initFlags, creationNodeMask, visibleNodeMask;
   uint64_t tempBufferHeap, bufferHeapOffset, tempTextureHeap, textureHeapOffset, pipelineCache;
} sdk_vk_init;

#define SUCCESS 0
#define ERROR_UNSUPPORTED_DEVICE -1
#define ERROR_INVALID_ARGUMENT -4
#define ERROR_DEVICE -6
#define ERROR_INVALID_CONTEXT -8

static fake_xess_record_t record;
static struct { int used; int vk; int inited; } contexts[FAKE_XESS_MAX_CALLS];
static const char *vk_instance_exts[] = { "VK_KHR_get_physical_device_properties2" };
static const char *vk_device_exts[]   = { "VK_KHR_shader_float16_int8", "VK_KHR_16bit_storage" };
/* (a chain the device creation can take as it is: a features structure
 * of the SDK's own, VkPhysicalDeviceShaderFloat16Int8Features, here
 * asking for nothing so that any device takes it) */
static struct { uint32_t sType; void *pNext; uint32_t shaderFloat16, shaderInt8; } vk_features = { 1000082000u, NULL, 0, 0 };

EXPORT fake_xess_record_t *fake_xess_record(void) { return &record; }

static int valid(void *ctx)
{
   return ctx >= (void*)&contexts[0] && ctx < (void*)&contexts[FAKE_XESS_MAX_CALLS]
      && ((int*)ctx)[0];
}

static int make_context(void **out, int vk)
{
   int i;
   record.creates++;
   if (record.create_fails)
      return ERROR_UNSUPPORTED_DEVICE;
   for (i = 0; i < FAKE_XESS_MAX_CALLS; i++)
      if (!contexts[i].used)
      {
         contexts[i].used = 1;
         contexts[i].vk = vk;
         contexts[i].inited = 0;
         *out = &contexts[i];
         return SUCCESS;
      }
   return ERROR_DEVICE;
}

EXPORT int xessGetVersion(sdk_version *v)
{
   if (!v)
      return ERROR_INVALID_ARGUMENT;
   v->major = 2; v->minor = 0; v->patch = 1; v->reserved = 0;
   return SUCCESS;
}

EXPORT int xessGetOptimalInputResolution(void *ctx, const sdk_2d *out, int quality, sdk_2d *optimal, sdk_2d *min, sdk_2d *max)
{
   static const float ratios[7] = { 3.0f, 2.3f, 2.0f, 1.7f, 1.5f, 1.3f, 1.0f };
   if (!valid(ctx) || !out || !optimal || !min || !max || quality < 100 || quality > 106)
      return ERROR_INVALID_ARGUMENT;
   optimal->x = (uint32_t)((float)out->x / ratios[quality - 100]);
   optimal->y = (uint32_t)((float)out->y / ratios[quality - 100]);
   min->x = out->x / 3; min->y = out->y / 3;
   *max = *out;
   return SUCCESS;
}

EXPORT int xessDestroyContext(void *ctx)
{
   if (!valid(ctx))
      return ERROR_INVALID_CONTEXT;
   memset(ctx, 0, sizeof(contexts[0]));
   record.destroys++;
   return SUCCESS;
}

EXPORT int xessSetJitterScale(void *ctx, float x, float y)
{
   if (!valid(ctx)) return ERROR_INVALID_CONTEXT;
   record.jitter_scale_x = x; record.jitter_scale_y = y;
   return SUCCESS;
}

EXPORT int xessSetVelocityScale(void *ctx, float x, float y)
{
   if (!valid(ctx)) return ERROR_INVALID_CONTEXT;
   record.velocity_scale_x = x; record.velocity_scale_y = y;
   return SUCCESS;
}

EXPORT int xessSetExposureMultiplier(void *ctx, float scale)
{
   if (!valid(ctx)) return ERROR_INVALID_CONTEXT;
   record.exposure_multiplier = scale;
   return SUCCESS;
}

EXPORT int xessGetPipelineBuildStatus(void *ctx)
{
   if (!valid(ctx)) return ERROR_INVALID_CONTEXT;
   if (record.build_pending)
   {
      record.build_pending--;
      return -9;
   }
   return SUCCESS;
}

EXPORT int xessSetLoggingCallback(void *ctx, int level, void (*cb)(const char*, int))
{
   if (!valid(ctx)) return ERROR_INVALID_CONTEXT;
   record.log_level = level;
   if (cb)
      cb("fake xess: hello", 1);
   return SUCCESS;
}

static void take_init(int vk, void *ctx, const sdk_2d *out, int quality, uint32_t flags, uint32_t mask)
{
   if (record.inits < FAKE_XESS_MAX_CALLS)
   {
      fake_xess_init_t *i = &record.init[record.inits];
      i->vk = vk; i->context = ctx; i->out_w = out->x; i->out_h = out->y;
      i->quality = quality; i->flags = flags; i->node_mask = mask;
   }
   record.inits++;
}

/* ---- Direct3D 12 ---- */

EXPORT int xessD3D12CreateContext(void *device, void **ctx)
{
   if (!device || !ctx)
      return ERROR_INVALID_ARGUMENT;
   record.create_device = device;
   return make_context(ctx, 0);
}

EXPORT int xessD3D12BuildPipelines(void *ctx, void *library, unsigned char blocking, uint32_t flags)
{
   (void)library; (void)flags;
   if (!valid(ctx)) return ERROR_INVALID_CONTEXT;
   record.pipelines++;
   record.build_blocking = blocking;
   return SUCCESS;
}

EXPORT int xessD3D12Init(void *ctx, const sdk_d3d12_init *p)
{
   if (!valid(ctx) || !p)
      return ERROR_INVALID_ARGUMENT;
   take_init(0, ctx, &p->outputResolution, p->qualitySetting, p->initFlags, p->creationNodeMask);
   if (record.init_fails)
      return ERROR_DEVICE;
   ((int*)ctx)[2] = 1;
   return SUCCESS;
}

static void take_d3d12_image(fake_xess_image_t *i, void *res)
{
   memset(i, 0, sizeof(*i));
   i->present = res != NULL;
   i->image = (uint64_t)(uintptr_t)res;
}

EXPORT int xessD3D12Execute(void *ctx, void *list, const sdk_d3d12_execute *p)
{
   if (!valid(ctx) || !p || !list)
      return ERROR_INVALID_ARGUMENT;
   if (!((int*)ctx)[2])
      return -3;
   if (record.executes < FAKE_XESS_MAX_CALLS)
   {
      fake_xess_execute_t *e = &record.execute[record.executes];
      memset(e, 0, sizeof(*e));
      e->vk = 0; e->context = ctx; e->command_list = list;
      take_d3d12_image(&e->color, p->pColorTexture);
      take_d3d12_image(&e->velocity, p->pVelocityTexture);
      take_d3d12_image(&e->depth, p->pDepthTexture);
      take_d3d12_image(&e->exposure, p->pExposureScaleTexture);
      take_d3d12_image(&e->mask, p->pResponsivePixelMaskTexture);
      take_d3d12_image(&e->output, p->pOutputTexture);
      e->jitter_x = p->jitterOffsetX; e->jitter_y = p->jitterOffsetY; e->exposure_scale = p->exposureScale;
      e->reset = p->resetHistory; e->input_w = p->inputWidth; e->input_h = p->inputHeight;
   }
   record.executes++;
   return record.execute_fails ? ERROR_DEVICE : SUCCESS;
}

/* ---- Vulkan ---- */

EXPORT int xessVKGetRequiredInstanceExtensions(uint32_t *count, const char *const **exts, uint32_t *min_version)
{
   if (!count || !exts || !min_version)
      return ERROR_INVALID_ARGUMENT;
   *count = 1; *exts = vk_instance_exts; *min_version = (1u << 22) | (1u << 12);
   return SUCCESS;
}

EXPORT int xessVKGetRequiredDeviceExtensions(void *instance, void *gpu, uint32_t *count, const char *const **exts)
{
   if (!instance || !gpu || !count || !exts)
      return ERROR_INVALID_ARGUMENT;
   *count = 2; *exts = vk_device_exts;
   return SUCCESS;
}

EXPORT int xessVKGetRequiredDeviceFeatures(void *instance, void *gpu, void **features)
{
   if (!instance || !gpu || !features)
      return ERROR_INVALID_ARGUMENT;
   *features = &vk_features;
   return SUCCESS;
}

EXPORT int xessVKCreateContext(void *instance, void *gpu, void *device, void **ctx)
{
   if (!instance || !gpu || !device || !ctx)
      return ERROR_INVALID_ARGUMENT;
   record.create_instance = instance; record.create_physical_device = gpu; record.create_device = device;
   return make_context(ctx, 1);
}

EXPORT int xessVKBuildPipelines(void *ctx, uint64_t cache, unsigned char blocking, uint32_t flags)
{
   (void)cache; (void)flags;
   if (!valid(ctx)) return ERROR_INVALID_CONTEXT;
   record.pipelines++;
   record.build_blocking = blocking;
   return SUCCESS;
}

EXPORT int xessVKInit(void *ctx, const sdk_vk_init *p)
{
   if (!valid(ctx) || !p)
      return ERROR_INVALID_ARGUMENT;
   take_init(1, ctx, &p->outputResolution, p->qualitySetting, p->initFlags, p->creationNodeMask);
   if (record.init_fails)
      return ERROR_DEVICE;
   ((int*)ctx)[2] = 1;
   return SUCCESS;
}

static void take_vk_image(fake_xess_image_t *i, const sdk_vk_image_view_info *v)
{
   i->present = v->image != 0;
   i->image = v->image; i->view = v->imageView;
   i->aspect = v->subresourceRange.aspectMask; i->format = v->format;
   i->width = v->width; i->height = v->height;
}

EXPORT int xessVKExecute(void *ctx, void *cmd, const sdk_vk_execute *p)
{
   if (!valid(ctx) || !p || !cmd)
      return ERROR_INVALID_ARGUMENT;
   if (!((int*)ctx)[2])
      return -3;
   if (record.executes < FAKE_XESS_MAX_CALLS)
   {
      fake_xess_execute_t *e = &record.execute[record.executes];
      memset(e, 0, sizeof(*e));
      e->vk = 1; e->context = ctx; e->command_list = cmd;
      take_vk_image(&e->color, &p->colorTexture);
      take_vk_image(&e->velocity, &p->velocityTexture);
      take_vk_image(&e->depth, &p->depthTexture);
      take_vk_image(&e->exposure, &p->exposureScaleTexture);
      take_vk_image(&e->mask, &p->responsivePixelMaskTexture);
      take_vk_image(&e->output, &p->outputTexture);
      e->jitter_x = p->jitterOffsetX; e->jitter_y = p->jitterOffsetY; e->exposure_scale = p->exposureScale;
      e->reset = p->resetHistory; e->input_w = p->inputWidth; e->input_h = p->inputHeight;
   }
   record.executes++;
   return record.execute_fails ? ERROR_DEVICE : SUCCESS;
}
