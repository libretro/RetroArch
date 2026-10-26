/* A stand-in FidelityFX API library (amd_fidelityfx_dx12.dll and
 * amd_fidelityfx_vk.dll on Windows, their .so names on Linux): exports
 * the five ffx-api entry points gfx/amd/ffx_loader.c resolves, laid out
 * from the SDK's own descriptor layouts (declared here again, as the
 * SDK's C++ headers have them, with no dependence on the loader's),
 * and records what it was asked (fake_ffx.h). Two upscaler versions, a
 * context a handle, the render size of a mode by the SDK's ratios. */
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "fake_ffx.h"

#if defined(_WIN32)
#define EXPORT __declspec(dllexport)
#else
#define EXPORT __attribute__((visibility("default")))
#endif

/* The SDK's layouts (ffx_api.h, ffx_api_types.h, ffx_upscale.h) */
typedef struct sdk_header { uint64_t type; struct sdk_header *pNext; } sdk_header;
struct sdk_dims { uint32_t width, height; };
struct sdk_coords { float x, y; };
struct sdk_resource_desc { uint32_t type, format, width, height, depth, mipCount, flags, usage; };
struct sdk_resource { void *resource; struct sdk_resource_desc description; uint32_t state; };
struct sdk_create_upscale { sdk_header header; uint32_t flags; struct sdk_dims maxRenderSize, maxUpscaleSize; void (*fpMessage)(uint32_t, const wchar_t*); };
struct sdk_dispatch_upscale
{
   sdk_header header; void *commandList;
   struct sdk_resource color, depth, motionVectors, exposure, reactive, transparencyAndComposition, output;
   struct sdk_coords jitterOffset, motionVectorScale;
   struct sdk_dims renderSize, upscaleSize;
   unsigned char enableSharpening; float sharpness, frameTimeDelta, preExposure;
   unsigned char reset; float cameraNear, cameraFar, cameraFovAngleVertical, viewSpaceToMetersFactor;
   uint32_t flags;
};
struct sdk_backend_dx12 { sdk_header header; void *device; };
struct sdk_backend_vk { sdk_header header; void *vkDevice, *vkPhysicalDevice, *vkDeviceProcAddr; };
struct sdk_override_version { sdk_header header; uint64_t versionId; };
struct sdk_upscale_version { sdk_header header; uint32_t version; };
struct sdk_get_versions { sdk_header header; uint64_t createDescType; void *device; uint64_t *outputCount; uint64_t *versionIds; const char **versionNames; };
struct sdk_render_of_mode { sdk_header header; uint32_t displayWidth, displayHeight, qualityMode; uint32_t *pOutRenderWidth, *pOutRenderHeight; };
struct sdk_debug1 { sdk_header header; void (*fpMessage)(uint32_t, const wchar_t*); uint32_t debugLevel; };

#define T_GET_VERSIONS      4u
#define T_OVERRIDE_VERSION  5u
#define T_DEBUG1            1u
#define T_BACKEND_DX12      2u
#define T_BACKEND_VK        3u
#define T_UPSCALE_CREATE    0x00010000u
#define T_UPSCALE_DISPATCH  0x00010001u
#define T_RENDER_OF_MODE    0x00010003u
#define T_UPSCALE_VERSION   0x0001000bu

#define OK 0u
#define ERR 1u
#define ERR_PARAMETER 6u

static fake_ffx_record_t record;
static int contexts[FAKE_FFX_MAX_CALLS];
static const uint64_t version_ids[2]   = { 0x4002, 0x3104 };
static const char *version_names[2]    = { "FSR 4.0.2", "FSR 3.1.4" };

EXPORT fake_ffx_record_t *fake_ffx_record(void) { return &record; }

static void take_resource(fake_ffx_resource_t *r, const struct sdk_resource *s)
{
   r->present   = s->resource != NULL;
   r->resource  = s->resource;
   r->type      = s->description.type;
   r->format    = s->description.format;
   r->width     = s->description.width;
   r->height    = s->description.height;
   r->mip_count = s->description.mipCount;
   r->usage     = s->description.usage;
   r->state     = s->state;
}

EXPORT uint32_t ffxCreateContext(void **context, sdk_header *desc, const void *memCb)
{
   fake_ffx_create_t *c;
   const sdk_header *h;
   int i;
   (void)memCb;
   if (!context || !desc || desc->type != T_UPSCALE_CREATE)
      return ERR_PARAMETER;
   if (record.creates < FAKE_FFX_MAX_CALLS)
   {
      const struct sdk_create_upscale *u = (const struct sdk_create_upscale*)desc;
      c = &record.create[record.creates];
      memset(c, 0, sizeof(*c));
      c->flags         = u->flags;
      c->max_render_w  = u->maxRenderSize.width;
      c->max_render_h  = u->maxRenderSize.height;
      c->max_upscale_w = u->maxUpscaleSize.width;
      c->max_upscale_h = u->maxUpscaleSize.height;
      c->has_message   = u->fpMessage != NULL;
      for (h = desc->pNext; h; h = h->pNext)
      {
         if (h->type == T_BACKEND_DX12)
         {
            c->backend_type = h->type;
            c->device = ((const struct sdk_backend_dx12*)h)->device;
         }
         else if (h->type == T_BACKEND_VK)
         {
            c->backend_type = h->type;
            c->device = ((const struct sdk_backend_vk*)h)->vkDevice;
            c->physical_device = ((const struct sdk_backend_vk*)h)->vkPhysicalDevice;
            c->get_device_proc_addr = ((const struct sdk_backend_vk*)h)->vkDeviceProcAddr;
         }
         else if (h->type == T_OVERRIDE_VERSION)
            c->version_id = ((const struct sdk_override_version*)h)->versionId;
         else if (h->type == T_UPSCALE_VERSION)
            c->api_version = ((const struct sdk_upscale_version*)h)->version;
      }
   }
   record.creates++;
   if (record.create_fails)
      return ERR;
   for (i = 0; i < FAKE_FFX_MAX_CALLS; i++)
      if (!contexts[i])
      {
         contexts[i] = 1;
         *context = &contexts[i];
         return OK;
      }
   return ERR;
}

EXPORT uint32_t ffxDestroyContext(void **context, const void *memCb)
{
   (void)memCb;
   if (!context || !*context)
      return ERR_PARAMETER;
   *(int*)*context = 0;
   record.last_destroyed = *context;
   record.destroys++;
   *context = NULL;
   return OK;
}

EXPORT uint32_t ffxConfigure(void **context, const sdk_header *desc)
{
   (void)context;
   record.configures++;
   if (desc && desc->type == T_DEBUG1)
   {
      record.debug_level = ((const struct sdk_debug1*)desc)->debugLevel;
      return OK;
   }
   return ERR_PARAMETER;
}

EXPORT uint32_t ffxQuery(void **context, sdk_header *desc)
{
   record.queries++;
   if (!desc)
      return ERR_PARAMETER;
   if (desc->type == T_GET_VERSIONS)
   {
      struct sdk_get_versions *q = (struct sdk_get_versions*)desc;
      uint64_t n = record.no_versions ? 0 : 2, i;
      if (record.versions_need_device && !q->device)
         n = 0;
      if (q->createDescType != T_UPSCALE_CREATE || !q->outputCount)
         return ERR_PARAMETER;
      if (q->versionIds || q->versionNames)
      {
         if (*q->outputCount < n)
            n = *q->outputCount;
         for (i = 0; i < n; i++)
         {
            if (q->versionIds)
               q->versionIds[i] = version_ids[i];
            if (q->versionNames)
               q->versionNames[i] = version_names[i];
         }
      }
      *q->outputCount = n;
      return OK;
   }
   if (desc->type == T_RENDER_OF_MODE)
   {
      struct sdk_render_of_mode *q = (struct sdk_render_of_mode*)desc;
      static const float ratios[5] = { 1.0f, 1.5f, 1.7f, 2.0f, 3.0f };
      (void)context;
      if (q->qualityMode > 4 || !q->pOutRenderWidth || !q->pOutRenderHeight)
         return ERR_PARAMETER;
      *q->pOutRenderWidth  = (uint32_t)((float)q->displayWidth / ratios[q->qualityMode]);
      *q->pOutRenderHeight = (uint32_t)((float)q->displayHeight / ratios[q->qualityMode]);
      return OK;
   }
   return ERR;
}

EXPORT uint32_t ffxDispatch(void **context, const sdk_header *desc)
{
   if (!context || !*context || !desc || desc->type != T_UPSCALE_DISPATCH)
      return ERR_PARAMETER;
   if (record.dispatches < FAKE_FFX_MAX_CALLS)
   {
      const struct sdk_dispatch_upscale *u = (const struct sdk_dispatch_upscale*)desc;
      fake_ffx_dispatch_t *d = &record.dispatch[record.dispatches];
      memset(d, 0, sizeof(*d));
      d->command_list = u->commandList;
      take_resource(&d->color, &u->color);
      take_resource(&d->depth, &u->depth);
      take_resource(&d->motion_vectors, &u->motionVectors);
      take_resource(&d->exposure, &u->exposure);
      take_resource(&d->reactive, &u->reactive);
      take_resource(&d->transparency, &u->transparencyAndComposition);
      take_resource(&d->output, &u->output);
      d->jitter_x     = u->jitterOffset.x;
      d->jitter_y     = u->jitterOffset.y;
      d->mv_scale_x   = u->motionVectorScale.x;
      d->mv_scale_y   = u->motionVectorScale.y;
      d->render_w     = u->renderSize.width;
      d->render_h     = u->renderSize.height;
      d->upscale_w    = u->upscaleSize.width;
      d->upscale_h    = u->upscaleSize.height;
      d->sharpening   = u->enableSharpening;
      d->sharpness    = u->sharpness;
      d->reset        = u->reset;
      d->frame_time   = u->frameTimeDelta;
      d->pre_exposure = u->preExposure;
      d->camera_near  = u->cameraNear;
      d->camera_far   = u->cameraFar;
      d->fov          = u->cameraFovAngleVertical;
      d->flags        = u->flags;
   }
   record.dispatches++;
   return record.dispatch_fails ? ERR : OK;
}
