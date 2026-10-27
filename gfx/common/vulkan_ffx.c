/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2026 - Libretro team
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <string.h>

#include <retro_miscellaneous.h>
#include <gfx/amd/ffx_loader.h>
#include <vulkan/vulkan_symbol_wrapper.h>

#include "../../verbosity.h"
#include "../video_upscaler.h"
#include "ffx_common.h"
#include "vulkan_ffx.h"

struct vulkan_ffx
{
   const ffx_lib_t *lib;
   ffx_context_t context;
   VkDevice device;
   VkPhysicalDevice gpu;
   PFN_vkGetDeviceProcAddr gdpa;
   VkPhysicalDeviceMemoryProperties memory_properties;
   vulkan_upscaler_ring_t ring;
   vulkan_upscaler_output_t out;
   uint64_t version;
   /* What the context was made for; sizes are VIDEO_SCALE_PACK */
   unsigned context_out_dims;
   unsigned context_flags;
   /* What failed last, create or dispatch: not tried again until the
    * output size or flags change, the frames shown as they are */
   unsigned failed_out_dims;
   unsigned failed_flags;
   bool reset_pending;
};

/* ffxApiGetSurfaceFormatVK of the SDK's ffx_api_vk.h */
static unsigned vulkan_ffx_format(VkFormat format)
{
   switch (format)
   {
      case VK_FORMAT_R32G32B32A32_SFLOAT:     return FFX_FORMAT_R32G32B32A32_FLOAT;
      case VK_FORMAT_R16G16B16A16_SFLOAT:     return FFX_FORMAT_R16G16B16A16_FLOAT;
      case VK_FORMAT_R32G32_SFLOAT:           return FFX_FORMAT_R32G32_FLOAT;
      case VK_FORMAT_R32_UINT:
      case VK_FORMAT_D24_UNORM_S8_UINT:
      case VK_FORMAT_X8_D24_UNORM_PACK32:     return FFX_FORMAT_R32_UINT;
      case VK_FORMAT_R8G8B8A8_UNORM:          return FFX_FORMAT_R8G8B8A8_UNORM;
      case VK_FORMAT_R8G8B8A8_SRGB:           return FFX_FORMAT_R8G8B8A8_SRGB;
      case VK_FORMAT_B8G8R8A8_UNORM:          return FFX_FORMAT_B8G8R8A8_UNORM;
      case VK_FORMAT_B8G8R8A8_SRGB:           return FFX_FORMAT_B8G8R8A8_SRGB;
      case VK_FORMAT_B10G11R11_UFLOAT_PACK32: return FFX_FORMAT_R11G11B10_FLOAT;
      case VK_FORMAT_A2B10G10R10_UNORM_PACK32: return FFX_FORMAT_R10G10B10A2_UNORM;
      case VK_FORMAT_R16G16_SFLOAT:           return FFX_FORMAT_R16G16_FLOAT;
      case VK_FORMAT_R16_SFLOAT:              return FFX_FORMAT_R16_FLOAT;
      case VK_FORMAT_R16_UNORM:
      case VK_FORMAT_D16_UNORM:
      case VK_FORMAT_D16_UNORM_S8_UINT:       return FFX_FORMAT_R16_UNORM;
      case VK_FORMAT_R8_UNORM:                return FFX_FORMAT_R8_UNORM;
      case VK_FORMAT_R8_UINT:
      case VK_FORMAT_S8_UINT:                 return FFX_FORMAT_R8_UINT;
      case VK_FORMAT_R32_SFLOAT:
      case VK_FORMAT_D32_SFLOAT:
      case VK_FORMAT_D32_SFLOAT_S8_UINT:      return FFX_FORMAT_R32_FLOAT;
      default:                                break;
   }
   return FFX_FORMAT_UNKNOWN;
}

/* A storage image cannot be sRGB: the output takes the UNORM twin, the
 * encoding the same */
static VkFormat vulkan_ffx_output_format(VkFormat format)
{
   switch (format)
   {
      case VK_FORMAT_R8G8B8A8_SRGB: return VK_FORMAT_R8G8B8A8_UNORM;
      case VK_FORMAT_B8G8R8A8_SRGB: return VK_FORMAT_B8G8R8A8_UNORM;
      default:                      break;
   }
   return format;
}

static bool vulkan_ffx_in_list(const char **list, unsigned count,
      const char *name)
{
   unsigned i;
   for (i = 0; i < count; i++)
      if (!strcmp(list[i], name))
         return true;
   return false;
}

static bool vulkan_ffx_gpu_has(VkPhysicalDevice gpu,
      PFN_vkGetInstanceProcAddr gipa, VkInstance instance, const char *name)
{
   PFN_vkEnumerateDeviceExtensionProperties enumerate;
   VkExtensionProperties *props = NULL;
   uint32_t nprops = 0, j;
   bool found = false;
   enumerate = (PFN_vkEnumerateDeviceExtensionProperties)
      gipa(instance, "vkEnumerateDeviceExtensionProperties");
   if (!enumerate || enumerate(gpu, NULL, &nprops, NULL) != VK_SUCCESS || !nprops)
      return false;
   if (!(props = (VkExtensionProperties*)malloc(nprops * sizeof(*props))))
      return false;
   if (enumerate(gpu, NULL, &nprops, props) == VK_SUCCESS)
      for (j = 0; j < nprops; j++)
         if (!strcmp(props[j].extensionName, name))
         {
            found = true;
            break;
         }
   free(props);
   return found;
}

/* The 16-bit arithmetic the library's shaders use where they can */
static const char *vulkan_ffx_wanted_exts[] = {
   "VK_KHR_shader_float16_int8",
   "VK_KHR_16bit_storage"
};

unsigned vulkan_ffx_device_extensions(const char *dir, VkPhysicalDevice gpu,
      PFN_vkGetInstanceProcAddr gipa, VkInstance instance,
      const char **list, unsigned count, unsigned cap)
{
   unsigned i;
   if (!ffx_runtime_in(FFX_API_VULKAN, dir))
      return count;
   for (i = 0; i < ARRAY_SIZE(vulkan_ffx_wanted_exts) && count < cap; i++)
      if (   !vulkan_ffx_in_list(list, count, vulkan_ffx_wanted_exts[i])
          && vulkan_ffx_gpu_has(gpu, gipa, instance, vulkan_ffx_wanted_exts[i]))
         list[count++] = vulkan_ffx_wanted_exts[i];
   return count;
}

static bool vulkan_ffx_chain_has(const void *pnext, VkStructureType type)
{
   const VkBaseInStructure *chain;
   for (chain = (const VkBaseInStructure*)pnext; chain; chain = chain->pNext)
      if (chain->sType == type)
         return true;
   return false;
}

const void *vulkan_ffx_device_features(const char *dir, VkPhysicalDevice gpu,
      PFN_vkGetInstanceProcAddr gipa, VkInstance instance, const void *pnext)
{
   /* (one device at a time: the chain is read at vkCreateDevice) */
   static VkPhysicalDeviceShaderFloat16Int8FeaturesKHR f16;
   static VkPhysicalDevice16BitStorageFeaturesKHR storage16;
   VkPhysicalDeviceFeatures2 f2;
   PFN_vkGetPhysicalDeviceFeatures2 get;
   if (!ffx_runtime_in(FFX_API_VULKAN, dir))
      return pnext;
   get = (PFN_vkGetPhysicalDeviceFeatures2)gipa(instance, "vkGetPhysicalDeviceFeatures2");
   if (!get)
      return pnext;
   memset(&f16, 0, sizeof(f16));
   memset(&storage16, 0, sizeof(storage16));
   f16.sType       = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES_KHR;
   storage16.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES_KHR;
   f16.pNext       = &storage16;
   memset(&f2, 0, sizeof(f2));
   f2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
   f2.pNext = &f16;
   get(gpu, &f2);
   /* (only what the GPU has, and none the chain names already) */
   f16.shaderInt8 = VK_FALSE;
   storage16.storageInputOutput16 = VK_FALSE;
   if (   f16.shaderFloat16
       && !vulkan_ffx_chain_has(pnext, f16.sType)
       && vulkan_ffx_gpu_has(gpu, gipa, instance, vulkan_ffx_wanted_exts[0]))
   {
      f16.pNext = (void*)pnext;
      pnext     = &f16;
   }
   if (   storage16.storageBuffer16BitAccess
       && !vulkan_ffx_chain_has(pnext, storage16.sType)
       && vulkan_ffx_gpu_has(gpu, gipa, instance, vulkan_ffx_wanted_exts[1]))
   {
      storage16.pNext = (void*)pnext;
      pnext           = &storage16;
   }
   return pnext;
}

vulkan_ffx_t *vulkan_ffx_init(VkPhysicalDevice gpu, VkDevice device,
      PFN_vkGetDeviceProcAddr gdpa,
      const VkPhysicalDeviceMemoryProperties *memory_properties,
      const char *dir)
{
   vulkan_ffx_t *ffx;
   const ffx_lib_t *lib = ffx_lib_get(FFX_API_VULKAN, dir ? dir : "");
   const char *name;
   if (!lib)
   {
      RARCH_LOG("[Vulkan] Temporal Upscaler: %s, upscaler off.\n",
            ffx_lib_status(FFX_API_VULKAN));
      return NULL;
   }
   if (!(ffx = (vulkan_ffx_t*)calloc(1, sizeof(*ffx))))
      return NULL;
   ffx->lib               = lib;
   ffx->device            = device;
   ffx->gpu               = gpu;
   ffx->gdpa              = gdpa;
   ffx->memory_properties = *memory_properties;
   ffx->ring.device       = device;
   ffx->version           = ffx_common_version(lib, NULL, &name);
   RARCH_LOG("[Vulkan] Temporal Upscaler: %s; FSR ready (%s).\n",
         ffx_lib_status(FFX_API_VULKAN), *name ? name : "the library's own version");
   return ffx;
}

bool vulkan_ffx_optimal(vulkan_ffx_t *ffx, unsigned mode,
      unsigned out_dims, unsigned *render_dims,
      unsigned *min_dims, unsigned *max_dims)
{
   if (!ffx)
      return false;
   return ffx_common_optimal(ffx->lib, mode, out_dims, render_dims,
         min_dims, max_dims);
}

static void vulkan_ffx_release(void *data, void *feature)
{
   vulkan_ffx_t *ffx = (vulkan_ffx_t*)data;
   ffx_context_t context = (ffx_context_t)feature;
   ffx->lib->destroy_context(&context, NULL);
}

static void vulkan_ffx_retire(vulkan_ffx_t *ffx)
{
   ffx->ring.release      = vulkan_ffx_release;
   ffx->ring.release_data = ffx;
   vulkan_upscaler_ring_retire(&ffx->ring, ffx->context, &ffx->out);
   ffx->context = NULL;
}

void vulkan_ffx_frame_done(vulkan_ffx_t *ffx)
{
   if (!ffx)
      return;
   vulkan_upscaler_ring_frame_done(&ffx->ring);
}

static bool vulkan_ffx_create_context(vulkan_ffx_t *ffx, VkFormat format,
      unsigned flags, unsigned out_dims)
{
   ffx_upscale_create_t create;
   ffx_backend_vk_t backend;
   ffx_override_version_t version;
   ffx_return_t res;

   vulkan_ffx_retire(ffx);
   if (!vulkan_upscaler_output_create(ffx->device, &ffx->memory_properties,
            vulkan_ffx_output_format(format), out_dims, &ffx->out))
   {
      RARCH_ERR("[Vulkan] Temporal Upscaler: no output image of %ux%u.\n",
            VIDEO_SCALE_W(out_dims), VIDEO_SCALE_H(out_dims));
      return false;
   }
   memset(&create, 0, sizeof(create));
   memset(&backend, 0, sizeof(backend));
   memset(&version, 0, sizeof(version));
   create.header.type            = FFX_TYPE_UPSCALE_CREATE;
   create.header.next            = &backend.header;
   create.flags                  = flags;
   create.max_render_size.width  = VIDEO_SCALE_W(out_dims);
   create.max_render_size.height = VIDEO_SCALE_H(out_dims);
   create.max_upscale_size       = create.max_render_size;
   create.message                = ffx_common_message;
   backend.header.type           = FFX_TYPE_BACKEND_VK;
   backend.device                = (void*)ffx->device;
   backend.physical_device       = (void*)ffx->gpu;
   memcpy(&backend.get_device_proc_addr, &ffx->gdpa, sizeof(backend.get_device_proc_addr));
   if (ffx->version)
   {
      backend.header.next = &version.header;
      version.header.type = FFX_TYPE_OVERRIDE_VERSION;
      version.version_id  = ffx->version;
   }
   res = ffx->lib->create_context(&ffx->context, &create.header, NULL);
   if (res != FFX_RETURN_OK)
   {
      RARCH_ERR("[Vulkan] Temporal Upscaler: FSR context -> %ux%u failed (%u).\n",
            VIDEO_SCALE_W(out_dims), VIDEO_SCALE_H(out_dims), (unsigned)res);
      ffx->context = NULL;
      vulkan_ffx_retire(ffx);
      ffx->failed_out_dims = out_dims;
      ffx->failed_flags    = flags;
      return false;
   }
   ffx->context_out_dims = out_dims;
   ffx->context_flags    = flags;
   ffx->reset_pending    = true;
   RARCH_LOG("[Vulkan] Temporal Upscaler: FSR context -> %ux%u, flags 0x%x.\n",
         VIDEO_SCALE_W(out_dims), VIDEO_SCALE_H(out_dims), flags);
   return true;
}

/* An input as the library takes it: in SHADER_READ_ONLY_OPTIMAL, or in
 * GENERAL as the core may keep it, which the library moves and moves
 * back; a depth image through its depth */
static void vulkan_ffx_resource(ffx_resource_t *r,
      const struct retro_hw_upscaler_image *img, unsigned usage,
      unsigned state)
{
   memset(r, 0, sizeof(*r));
   r->resource              = (void*)(uintptr_t)img->image;
   r->description.type      = FFX_RESOURCE_TEXTURE2D;
   r->description.format    = vulkan_ffx_format((VkFormat)img->format);
   r->description.width     = img->width;
   r->description.height    = img->height;
   r->description.depth     = 1;
   r->description.mip_count = 1;
   r->description.usage     = usage;
   if (vulkan_upscaler_aspect((VkFormat)img->format) & VK_IMAGE_ASPECT_DEPTH_BIT)
      r->description.usage |= FFX_USAGE_DEPTHTARGET;
   r->state                 = state;
}

static unsigned vulkan_ffx_input_state(const struct retro_hw_upscaler_image *img)
{
   if (img->state == (unsigned)VK_IMAGE_LAYOUT_GENERAL)
      return FFX_STATE_UNORDERED_ACCESS;
   return FFX_STATE_COMPUTE_READ;
}

bool vulkan_ffx_upscale(vulkan_ffx_t *ffx, VkCommandBuffer cmd,
      const vulkan_upscaler_image_t *color,
      const struct retro_hw_upscaler_inputs *inputs,
      unsigned mode, unsigned out_dims, vulkan_upscaler_image_t *out)
{
   ffx_upscale_dispatch_t d;
   struct retro_hw_upscaler_image img;
   unsigned render_dims, min_dims, max_dims;
   unsigned flags;
   ffx_return_t res;

   if (!ffx || !inputs || !color->image || !inputs->motion_vectors.image
         || !inputs->depth.image
         || VIDEO_SCALE_W(out_dims) < 32 || VIDEO_SCALE_H(out_dims) < 32)
      return false;
   flags = ffx_common_create_flags(inputs);
   if (ffx->failed_out_dims == out_dims && ffx->failed_flags == flags)
      return false;
   if (!vulkan_ffx_optimal(ffx, mode, out_dims, &render_dims, &min_dims,
            &max_dims))
      return false;
   if (     VIDEO_SCALE_W(color->dims) < VIDEO_SCALE_W(min_dims)
         || VIDEO_SCALE_W(color->dims) > VIDEO_SCALE_W(max_dims)
         || VIDEO_SCALE_H(color->dims) < VIDEO_SCALE_H(min_dims)
         || VIDEO_SCALE_H(color->dims) > VIDEO_SCALE_H(max_dims))
      return false;

   if (   !ffx->context
       || ffx->context_out_dims != out_dims
       || ffx->context_flags != flags
       || ffx->out.format != vulkan_ffx_output_format(color->format))
   {
      if (!vulkan_ffx_create_context(ffx, color->format, flags, out_dims))
         return false;
   }

   /* The output: the last frame's read is over before this write; its
    * first frame has nothing to wait for */
   vulkan_upscaler_barrier(cmd, ffx->out.image, VK_IMAGE_ASPECT_COLOR_BIT,
         ffx->reset_pending ? VK_IMAGE_LAYOUT_UNDEFINED
                            : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
         VK_IMAGE_LAYOUT_GENERAL,
         VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_MEMORY_WRITE_BIT,
         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
         VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
   if (vulkan_upscaler_input_needs_move(&inputs->motion_vectors))
      vulkan_upscaler_move_input(cmd, &inputs->motion_vectors, true);
   if (vulkan_upscaler_input_needs_move(&inputs->depth))
      vulkan_upscaler_move_input(cmd, &inputs->depth, true);

   memset(&d, 0, sizeof(d));
   d.command_list = (void*)cmd;
   img.image  = (uint64_t)(uintptr_t)color->image;
   img.view   = (uint64_t)(uintptr_t)color->view;
   img.format = (unsigned)color->format;
   img.state  = (unsigned)color->layout;
   img.width  = VIDEO_SCALE_W(color->dims);
   img.height = VIDEO_SCALE_H(color->dims);
   vulkan_ffx_resource(&d.color, &img, FFX_USAGE_READ_ONLY, vulkan_ffx_input_state(&img));
   vulkan_ffx_resource(&d.depth, &inputs->depth, FFX_USAGE_READ_ONLY,
         vulkan_upscaler_input_needs_move(&inputs->depth)
         ? FFX_STATE_COMPUTE_READ : vulkan_ffx_input_state(&inputs->depth));
   vulkan_ffx_resource(&d.motion_vectors, &inputs->motion_vectors, FFX_USAGE_READ_ONLY,
         vulkan_upscaler_input_needs_move(&inputs->motion_vectors)
         ? FFX_STATE_COMPUTE_READ : vulkan_ffx_input_state(&inputs->motion_vectors));
   if (inputs->exposure.image)
      vulkan_ffx_resource(&d.exposure, &inputs->exposure, FFX_USAGE_READ_ONLY,
            vulkan_ffx_input_state(&inputs->exposure));
   if (inputs->transparency_mask.image)
      vulkan_ffx_resource(&d.transparency_and_composition, &inputs->transparency_mask,
            FFX_USAGE_READ_ONLY, vulkan_ffx_input_state(&inputs->transparency_mask));
   img.image  = (uint64_t)(uintptr_t)ffx->out.image;
   img.view   = (uint64_t)(uintptr_t)ffx->out.view;
   img.format = (unsigned)ffx->out.format;
   img.state  = (unsigned)VK_IMAGE_LAYOUT_GENERAL;
   img.width  = VIDEO_SCALE_W(out_dims);
   img.height = VIDEO_SCALE_H(out_dims);
   vulkan_ffx_resource(&d.output, &img, FFX_USAGE_UAV, FFX_STATE_UNORDERED_ACCESS);
   ffx_common_dispatch(&d, inputs, color->dims, out_dims, ffx->reset_pending,
         FFX_MODE_SHARPNESS(mode));
   vulkan_upscaler_full_barrier(cmd);
   res = ffx->lib->dispatch(&ffx->context, &d.header);
   ffx->reset_pending = false;

   if (vulkan_upscaler_input_needs_move(&inputs->motion_vectors))
      vulkan_upscaler_move_input(cmd, &inputs->motion_vectors, false);
   if (vulkan_upscaler_input_needs_move(&inputs->depth))
      vulkan_upscaler_move_input(cmd, &inputs->depth, false);
   vulkan_upscaler_barrier(cmd, ffx->out.image, VK_IMAGE_ASPECT_COLOR_BIT,
         VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
         VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
         VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
   if (res != FFX_RETURN_OK)
   {
      RARCH_ERR("[Vulkan] Temporal Upscaler: FSR dispatch failed (%u).\n",
            (unsigned)res);
      ffx->failed_out_dims = out_dims;
      ffx->failed_flags    = flags;
      return false;
   }
   out->image  = ffx->out.image;
   out->view   = ffx->out.view;
   out->layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
   out->format = ffx->out.format;
   out->dims   = out_dims;
   return true;
}

void vulkan_ffx_free(vulkan_ffx_t *ffx)
{
   if (!ffx)
      return;
   vulkan_ffx_retire(ffx);
   vulkan_upscaler_ring_flush(&ffx->ring);
   RARCH_LOG("[Vulkan] Temporal Upscaler: FSR context released.\n");
   free(ffx);
}
