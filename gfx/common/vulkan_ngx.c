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
#include <gfx/nvidia/ngx_loader.h>
#include <vulkan/vulkan_symbol_wrapper.h>

#include "../../verbosity.h"
#include "vulkan_ngx.h"

/* RetroArch's NGX project: an id of its own, as NGX asks of applications
 * without an NVIDIA application id */
#define VULKAN_NGX_PROJECT_ID     "3fe3a1ab-9a3c-4b57-a9d7-26c5a2a3b0d1"
#define VULKAN_NGX_ENGINE_VERSION "1.0"

/* Frames an image or feature a submitted frame may read stays alive
 * for, past the swapchain's depth */
#define VULKAN_NGX_RETIRE_FRAMES 20
#define VULKAN_NGX_RETIRED_MAX   8

typedef struct vulkan_ngx_retired
{
   ngx_handle_t *feature;
   VkImage image;
   VkImageView view;
   VkDeviceMemory memory;
   unsigned frames;
} vulkan_ngx_retired_t;

struct vulkan_ngx
{
   const ngx_lib_t *lib;
   ngx_parameter_t *caps;
   ngx_parameter_t *params;
   ngx_handle_t *feature;
   VkDevice device;
   VkPhysicalDeviceMemoryProperties memory_properties;
   vulkan_ngx_retired_t retired[VULKAN_NGX_RETIRED_MAX];
   /* What the feature was made for */
   unsigned feature_out_w;
   unsigned feature_out_h;
   unsigned feature_max_w;
   unsigned feature_max_h;
   unsigned feature_flags;
   unsigned feature_quality;
   /* The output image, the feature's size */
   VkImage out_image;
   VkImageView out_view;
   VkDeviceMemory out_memory;
   VkFormat out_format;
   /* The last optimal query, kept for the request each frame */
   unsigned opt_quality;
   unsigned opt_out_w;
   unsigned opt_out_h;
   unsigned opt_render_w;
   unsigned opt_render_h;
   unsigned opt_min_w;
   unsigned opt_min_h;
   unsigned opt_max_w;
   unsigned opt_max_h;
   /* What failed last, create or evaluate: not tried again until the
    * output size or mode changes, the frames shown as they are */
   unsigned failed_out_w;
   unsigned failed_out_h;
   unsigned failed_quality;
   bool opt_valid;
   bool reset_pending;
};

static VkImageAspectFlags vulkan_ngx_aspect(VkFormat format)
{
   switch (format)
   {
      case VK_FORMAT_D16_UNORM:
      case VK_FORMAT_X8_D24_UNORM_PACK32:
      case VK_FORMAT_D32_SFLOAT:
         return VK_IMAGE_ASPECT_DEPTH_BIT;
      case VK_FORMAT_D16_UNORM_S8_UINT:
      case VK_FORMAT_D24_UNORM_S8_UINT:
      case VK_FORMAT_D32_SFLOAT_S8_UINT:
         return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
      case VK_FORMAT_S8_UINT:
         return VK_IMAGE_ASPECT_STENCIL_BIT;
      default:
         return VK_IMAGE_ASPECT_COLOR_BIT;
   }
}

static void vulkan_ngx_barrier(VkCommandBuffer cmd, VkImage image,
      VkImageAspectFlags aspect, VkImageLayout old_layout,
      VkImageLayout new_layout, VkAccessFlags src_access,
      VkAccessFlags dst_access, VkPipelineStageFlags src_stages,
      VkPipelineStageFlags dst_stages)
{
   VkImageMemoryBarrier barrier;
   barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
   barrier.pNext                           = NULL;
   barrier.srcAccessMask                   = src_access;
   barrier.dstAccessMask                   = dst_access;
   barrier.oldLayout                       = old_layout;
   barrier.newLayout                       = new_layout;
   barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
   barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
   barrier.image                           = image;
   barrier.subresourceRange.aspectMask     = aspect;
   barrier.subresourceRange.baseMipLevel   = 0;
   barrier.subresourceRange.levelCount     = 1;
   barrier.subresourceRange.baseArrayLayer = 0;
   barrier.subresourceRange.layerCount     = 1;
   vkCmdPipelineBarrier(cmd, src_stages, dst_stages, 0, 0, NULL, 0, NULL,
         1, &barrier);
}

static bool vulkan_ngx_in_list(const char **list, unsigned count,
      const char *name)
{
   unsigned i;
   for (i = 0; i < count; i++)
      if (!strcmp(list[i], name))
         return true;
   return false;
}

unsigned vulkan_ngx_instance_extensions(const char **list,
      unsigned count, unsigned cap)
{
   const ngx_lib_t *lib = ngx_lib_get();
   unsigned ic = 0, dc = 0, i;
   const char **ie = NULL, **de = NULL;
   if (!lib || !lib->vulkan.required_extensions)
      return count;
   if (NGX_FAILED(lib->vulkan.required_extensions(&ic, &ie, &dc, &de)))
      return count;
   for (i = 0; i < ic && count < cap; i++)
      if (!vulkan_ngx_in_list(list, count, ie[i]))
         list[count++] = ie[i];
   return count;
}

unsigned vulkan_ngx_device_extensions(VkPhysicalDevice gpu,
      PFN_vkGetInstanceProcAddr gipa, VkInstance instance,
      const char **list, unsigned count, unsigned cap)
{
   const ngx_lib_t *lib = ngx_lib_get();
   PFN_vkEnumerateDeviceExtensionProperties enumerate;
   VkExtensionProperties *props = NULL;
   uint32_t nprops = 0;
   unsigned ic = 0, dc = 0, i, j;
   const char **ie = NULL, **de = NULL;
   if (!lib || !lib->vulkan.required_extensions)
      return count;
   if (NGX_FAILED(lib->vulkan.required_extensions(&ic, &ie, &dc, &de)) || !dc)
      return count;
   enumerate = (PFN_vkEnumerateDeviceExtensionProperties)
      gipa(instance, "vkEnumerateDeviceExtensionProperties");
   if (!enumerate || enumerate(gpu, NULL, &nprops, NULL) != VK_SUCCESS || !nprops)
      return count;
   if (!(props = (VkExtensionProperties*)malloc(nprops * sizeof(*props))))
      return count;
   if (enumerate(gpu, NULL, &nprops, props) != VK_SUCCESS)
   {
      free(props);
      return count;
   }
   for (i = 0; i < dc && count < cap; i++)
   {
      if (vulkan_ngx_in_list(list, count, de[i]))
         continue;
      for (j = 0; j < nprops; j++)
         if (!strcmp(props[j].extensionName, de[i]))
         {
            list[count++] = de[i];
            break;
         }
   }
   free(props);
   return count;
}

vulkan_ngx_t *vulkan_ngx_init(VkInstance instance, VkPhysicalDevice gpu,
      VkDevice device, PFN_vkGetInstanceProcAddr gipa,
      PFN_vkGetDeviceProcAddr gdpa,
      const VkPhysicalDeviceMemoryProperties *memory_properties,
      const char *dlss_dir)
{
   /* The path list and the data path, wide as NGX takes them */
   wchar_t *wdir;
   const wchar_t *paths[1];
   ngx_feature_common_info_t info;
   vulkan_ngx_t *ngx;
   ngx_result_t res;
   int available = 0;
   const ngx_lib_t *lib = ngx_lib_get();

   if (!lib || !lib->vulkan.init_with_project_id)
   {
      RARCH_LOG("[Vulkan] Temporal Upscaler: no NGX core library.\n");
      return NULL;
   }
   if (!(ngx = (vulkan_ngx_t*)calloc(1, sizeof(*ngx))))
      return NULL;
   if (!(wdir = (wchar_t*)calloc(DIR_MAX_LENGTH, sizeof(wchar_t))))
   {
      free(ngx);
      return NULL;
   }
   ngx_utf8_to_wide(wdir, DIR_MAX_LENGTH, dlss_dir ? dlss_dir : "");
   paths[0]              = wdir;
   memset(&info, 0, sizeof(info));
   info.path_list.path   = paths;
   info.path_list.length = 1;
   res = lib->vulkan.init_with_project_id(VULKAN_NGX_PROJECT_ID,
         NGX_ENGINE_TYPE_CUSTOM, VULKAN_NGX_ENGINE_VERSION, wdir,
         (void*)instance, (void*)gpu, (void*)device, (ngx_vk_proc_t)gipa,
         (ngx_vk_proc_t)gdpa, &info, NGX_VERSION_API);
   free(wdir);
   if (NGX_FAILED(res))
   {
      RARCH_WARN("[Vulkan] Temporal Upscaler: NGX init failed (0x%08x).\n",
            (unsigned)res);
      free(ngx);
      return NULL;
   }
   ngx->lib    = lib;
   ngx->device = device;
   ngx->memory_properties = *memory_properties;
   if (   NGX_FAILED(lib->vulkan.get_capability_parameters(&ngx->caps))
       || NGX_FAILED(lib->param.get_i(ngx->caps,
             NGX_PARAM_SUPER_SAMPLING_AVAILABLE, &available))
       || !available)
   {
      int needs_driver = 0, major = 0, minor = 0, init_result = 0;
      if (ngx->caps)
      {
         lib->param.get_i(ngx->caps, NGX_PARAM_SUPER_SAMPLING_NEEDS_UPDATED_DRIVER, &needs_driver);
         lib->param.get_i(ngx->caps, NGX_PARAM_SUPER_SAMPLING_MIN_DRIVER_MAJOR, &major);
         lib->param.get_i(ngx->caps, NGX_PARAM_SUPER_SAMPLING_MIN_DRIVER_MINOR, &minor);
         lib->param.get_i(ngx->caps, NGX_PARAM_SUPER_SAMPLING_FEATURE_INIT_RESULT, &init_result);
      }
      if (needs_driver)
         RARCH_WARN("[Vulkan] Temporal Upscaler: DLSS needs driver %d.%d or newer.\n",
               major, minor);
      else
         RARCH_WARN("[Vulkan] Temporal Upscaler: DLSS Super Resolution not available on this device (0x%08x).\n",
               (unsigned)init_result);
      lib->vulkan.shutdown1((void*)device);
      free(ngx);
      return NULL;
   }
   if (NGX_FAILED(lib->vulkan.allocate_parameters(&ngx->params)))
   {
      lib->vulkan.shutdown1((void*)device);
      free(ngx);
      return NULL;
   }
   RARCH_LOG("[Vulkan] Temporal Upscaler: DLSS Super Resolution ready.\n");
   return ngx;
}

bool vulkan_ngx_optimal(vulkan_ngx_t *ngx, unsigned quality,
      unsigned out_w, unsigned out_h,
      unsigned *render_w, unsigned *render_h,
      unsigned *min_w, unsigned *min_h,
      unsigned *max_w, unsigned *max_h)
{
   const ngx_lib_t *lib;
   void *callback = NULL;
   ngx_result_t (NGX_CONV *optimal)(ngx_parameter_t *p);
   if (!ngx || !out_w || !out_h)
      return false;
   lib = ngx->lib;
   if (!ngx->opt_valid || ngx->opt_quality != quality
         || ngx->opt_out_w != out_w || ngx->opt_out_h != out_h)
   {
      ngx_parameter_t *p = ngx->caps;
      ngx->opt_valid = false;
      lib->param.get_void_pointer(p, NGX_PARAM_DLSS_OPTIMAL_SETTINGS_CALLBACK,
            &callback);
      if (!callback)
         return false;
      memcpy(&optimal, &callback, sizeof(optimal));
      lib->param.set_ui(p, NGX_PARAM_WIDTH, out_w);
      lib->param.set_ui(p, NGX_PARAM_HEIGHT, out_h);
      lib->param.set_i(p, NGX_PARAM_PERF_QUALITY, (int)quality);
      lib->param.set_i(p, NGX_PARAM_RTX_VALUE, 0);
      if (NGX_FAILED(optimal(p)))
         return false;
      ngx->opt_render_w = ngx->opt_render_h = 0;
      lib->param.get_ui(p, NGX_PARAM_OUT_WIDTH, &ngx->opt_render_w);
      lib->param.get_ui(p, NGX_PARAM_OUT_HEIGHT, &ngx->opt_render_h);
      if (!ngx->opt_render_w || !ngx->opt_render_h)
         return false;
      ngx->opt_max_w = ngx->opt_min_w = ngx->opt_render_w;
      ngx->opt_max_h = ngx->opt_min_h = ngx->opt_render_h;
      lib->param.get_ui(p, NGX_PARAM_DLSS_DYNAMIC_MAX_RENDER_WIDTH, &ngx->opt_max_w);
      lib->param.get_ui(p, NGX_PARAM_DLSS_DYNAMIC_MAX_RENDER_HEIGHT, &ngx->opt_max_h);
      lib->param.get_ui(p, NGX_PARAM_DLSS_DYNAMIC_MIN_RENDER_WIDTH, &ngx->opt_min_w);
      lib->param.get_ui(p, NGX_PARAM_DLSS_DYNAMIC_MIN_RENDER_HEIGHT, &ngx->opt_min_h);
      ngx->opt_quality = quality;
      ngx->opt_out_w   = out_w;
      ngx->opt_out_h   = out_h;
      ngx->opt_valid   = true;
   }
   *render_w = ngx->opt_render_w;
   *render_h = ngx->opt_render_h;
   *min_w    = ngx->opt_min_w;
   *min_h    = ngx->opt_min_h;
   *max_w    = ngx->opt_max_w;
   *max_h    = ngx->opt_max_h;
   return true;
}

/* The feature and output image go to the retired ring, destroyed once
 * every frame that could use them has gone by */
static void vulkan_ngx_retire(vulkan_ngx_t *ngx)
{
   unsigned i;
   vulkan_ngx_retired_t *slot = NULL;
   if (!ngx->feature && !ngx->out_image)
      return;
   for (i = 0; i < VULKAN_NGX_RETIRED_MAX; i++)
      if (!ngx->retired[i].frames)
      {
         slot = &ngx->retired[i];
         break;
      }
   if (!slot)
   {
      /* The ring is full of retirees from a run of resizes: the oldest
       * has had every frame of a swapchain's depth to finish */
      slot = &ngx->retired[0];
      for (i = 1; i < VULKAN_NGX_RETIRED_MAX; i++)
         if (ngx->retired[i].frames < slot->frames)
            slot = &ngx->retired[i];
      if (slot->feature)
         ngx->lib->vulkan.release_feature(slot->feature);
      if (slot->view)
         vkDestroyImageView(ngx->device, slot->view, NULL);
      if (slot->image)
         vkDestroyImage(ngx->device, slot->image, NULL);
      if (slot->memory)
         vkFreeMemory(ngx->device, slot->memory, NULL);
   }
   slot->feature   = ngx->feature;
   slot->image     = ngx->out_image;
   slot->view      = ngx->out_view;
   slot->memory    = ngx->out_memory;
   slot->frames    = VULKAN_NGX_RETIRE_FRAMES;
   ngx->feature    = NULL;
   ngx->out_image  = VK_NULL_HANDLE;
   ngx->out_view   = VK_NULL_HANDLE;
   ngx->out_memory = VK_NULL_HANDLE;
}

void vulkan_ngx_frame_done(vulkan_ngx_t *ngx)
{
   unsigned i;
   if (!ngx)
      return;
   for (i = 0; i < VULKAN_NGX_RETIRED_MAX; i++)
   {
      vulkan_ngx_retired_t *r = &ngx->retired[i];
      if (!r->frames || --r->frames)
         continue;
      if (r->feature)
         ngx->lib->vulkan.release_feature(r->feature);
      if (r->view)
         vkDestroyImageView(ngx->device, r->view, NULL);
      if (r->image)
         vkDestroyImage(ngx->device, r->image, NULL);
      if (r->memory)
         vkFreeMemory(ngx->device, r->memory, NULL);
      memset(r, 0, sizeof(*r));
   }
}

static bool vulkan_ngx_create_output(vulkan_ngx_t *ngx, VkFormat format,
      unsigned w, unsigned h)
{
   VkImageCreateInfo image;
   VkImageViewCreateInfo view;
   VkMemoryRequirements reqs;
   VkMemoryAllocateInfo alloc;
   uint32_t type;
   unsigned i;

   image.sType                 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
   image.pNext                 = NULL;
   image.flags                 = 0;
   image.imageType             = VK_IMAGE_TYPE_2D;
   image.format                = format;
   image.extent.width          = w;
   image.extent.height         = h;
   image.extent.depth          = 1;
   image.mipLevels             = 1;
   image.arrayLayers           = 1;
   image.samples               = VK_SAMPLE_COUNT_1_BIT;
   image.tiling                = VK_IMAGE_TILING_OPTIMAL;
   image.usage                 = VK_IMAGE_USAGE_STORAGE_BIT
                               | VK_IMAGE_USAGE_SAMPLED_BIT
                               | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
   image.sharingMode           = VK_SHARING_MODE_EXCLUSIVE;
   image.queueFamilyIndexCount = 0;
   image.pQueueFamilyIndices   = NULL;
   image.initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED;
   if (vkCreateImage(ngx->device, &image, NULL, &ngx->out_image) != VK_SUCCESS)
      return false;

   vkGetImageMemoryRequirements(ngx->device, ngx->out_image, &reqs);
   type = ngx->memory_properties.memoryTypeCount;
   for (i = 0; i < ngx->memory_properties.memoryTypeCount; i++)
      if (   (reqs.memoryTypeBits & (1u << i))
          && (ngx->memory_properties.memoryTypes[i].propertyFlags
             & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
      {
         type = i;
         break;
      }
   if (type == ngx->memory_properties.memoryTypeCount)
      for (i = 0; i < ngx->memory_properties.memoryTypeCount; i++)
         if (reqs.memoryTypeBits & (1u << i))
         {
            type = i;
            break;
         }
   alloc.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
   alloc.pNext           = NULL;
   alloc.allocationSize  = reqs.size;
   alloc.memoryTypeIndex = type;
   if (   type == ngx->memory_properties.memoryTypeCount
       || vkAllocateMemory(ngx->device, &alloc, NULL, &ngx->out_memory) != VK_SUCCESS
       || vkBindImageMemory(ngx->device, ngx->out_image, ngx->out_memory, 0) != VK_SUCCESS)
      goto fail;

   view.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
   view.pNext                           = NULL;
   view.flags                           = 0;
   view.image                           = ngx->out_image;
   view.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
   view.format                          = format;
   view.components.r                    = VK_COMPONENT_SWIZZLE_IDENTITY;
   view.components.g                    = VK_COMPONENT_SWIZZLE_IDENTITY;
   view.components.b                    = VK_COMPONENT_SWIZZLE_IDENTITY;
   view.components.a                    = VK_COMPONENT_SWIZZLE_IDENTITY;
   view.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
   view.subresourceRange.baseMipLevel   = 0;
   view.subresourceRange.levelCount     = 1;
   view.subresourceRange.baseArrayLayer = 0;
   view.subresourceRange.layerCount     = 1;
   if (vkCreateImageView(ngx->device, &view, NULL, &ngx->out_view) != VK_SUCCESS)
      goto fail;
   ngx->out_format = format;
   return true;

fail:
   if (ngx->out_memory)
      vkFreeMemory(ngx->device, ngx->out_memory, NULL);
   vkDestroyImage(ngx->device, ngx->out_image, NULL);
   ngx->out_image  = VK_NULL_HANDLE;
   ngx->out_memory = VK_NULL_HANDLE;
   return false;
}

static bool vulkan_ngx_create_feature(vulkan_ngx_t *ngx,
      VkCommandBuffer cmd, VkFormat format, unsigned quality,
      unsigned flags, unsigned out_w, unsigned out_h,
      unsigned max_w, unsigned max_h)
{
   const ngx_lib_t *lib = ngx->lib;
   ngx_parameter_t *p   = ngx->params;
   ngx_result_t res;

   vulkan_ngx_retire(ngx);
   if (!vulkan_ngx_create_output(ngx, format, out_w, out_h))
   {
      RARCH_ERR("[Vulkan] Temporal Upscaler: no output image of %ux%u.\n",
            out_w, out_h);
      return false;
   }
   lib->param.set_ui(p, NGX_PARAM_CREATION_NODE_MASK, 1);
   lib->param.set_ui(p, NGX_PARAM_VISIBILITY_NODE_MASK, 1);
   lib->param.set_ui(p, NGX_PARAM_WIDTH, max_w);
   lib->param.set_ui(p, NGX_PARAM_HEIGHT, max_h);
   lib->param.set_ui(p, NGX_PARAM_OUT_WIDTH, out_w);
   lib->param.set_ui(p, NGX_PARAM_OUT_HEIGHT, out_h);
   lib->param.set_i(p, NGX_PARAM_PERF_QUALITY, (int)quality);
   lib->param.set_i(p, NGX_PARAM_DLSS_CREATE_FLAGS, (int)flags);
   lib->param.set_i(p, NGX_PARAM_DLSS_ENABLE_OUTPUT_SUBRECTS, 0);
   res = lib->vulkan.create_feature1((void*)ngx->device, (void*)cmd,
         NGX_FEATURE_SUPER_SAMPLING, p, &ngx->feature);
   if (NGX_FAILED(res))
   {
      RARCH_ERR("[Vulkan] Temporal Upscaler: DLSS feature %ux%u -> %ux%u failed (0x%08x).\n",
            max_w, max_h, out_w, out_h, (unsigned)res);
      ngx->feature = NULL;
      vulkan_ngx_retire(ngx);
      ngx->failed_out_w   = out_w;
      ngx->failed_out_h   = out_h;
      ngx->failed_quality = quality;
      return false;
   }
   ngx->feature_out_w   = out_w;
   ngx->feature_out_h   = out_h;
   ngx->feature_max_w   = max_w;
   ngx->feature_max_h   = max_h;
   ngx->feature_flags   = flags;
   ngx->feature_quality = quality;
   ngx->reset_pending   = true;
   RARCH_LOG("[Vulkan] Temporal Upscaler: DLSS feature up to %ux%u -> %ux%u, flags 0x%x.\n",
         max_w, max_h, out_w, out_h, flags);
   return true;
}

static void vulkan_ngx_vk_resource(ngx_vk_resource_t *r,
      const struct retro_hw_upscaler_image *img, bool read_write)
{
   memset(r, 0, sizeof(*r));
   r->resource.image_view.image_view = (void*)(uintptr_t)img->view;
   r->resource.image_view.image      = (void*)(uintptr_t)img->image;
   /* A depth-stencil image is sampled through its depth alone */
   r->resource.image_view.subresource_range.aspect_mask =
      vulkan_ngx_aspect((VkFormat)img->format) & ~VK_IMAGE_ASPECT_STENCIL_BIT;
   r->resource.image_view.subresource_range.level_count = 1;
   r->resource.image_view.subresource_range.layer_count = 1;
   r->resource.image_view.format = (int)img->format;
   r->resource.image_view.width  = img->width;
   r->resource.image_view.height = img->height;
   r->type       = 0;
   r->read_write = read_write ? 1 : 0;
}

/* An input in a layout a compute shader cannot sample from is moved
 * for the evaluate, and moved back after */
static bool vulkan_ngx_input_needs_move(const struct retro_hw_upscaler_image *img)
{
   return img->image
      && img->state != (unsigned)VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
      && img->state != (unsigned)VK_IMAGE_LAYOUT_GENERAL;
}

static void vulkan_ngx_move_input(VkCommandBuffer cmd,
      const struct retro_hw_upscaler_image *img, bool to_read)
{
   VkImageLayout layout = (VkImageLayout)img->state;
   vulkan_ngx_barrier(cmd, (VkImage)(uintptr_t)img->image,
         vulkan_ngx_aspect((VkFormat)img->format),
         to_read ? layout : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
         to_read ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : layout,
         to_read ? VK_ACCESS_MEMORY_WRITE_BIT : VK_ACCESS_SHADER_READ_BIT,
         to_read ? VK_ACCESS_SHADER_READ_BIT : VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
         to_read ? VK_PIPELINE_STAGE_ALL_COMMANDS_BIT : VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
         to_read ? VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT : VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
}

bool vulkan_ngx_upscale(vulkan_ngx_t *ngx, VkCommandBuffer cmd,
      const vulkan_ngx_image_t *color,
      const struct retro_hw_upscaler_inputs *inputs,
      unsigned quality, unsigned out_w, unsigned out_h,
      vulkan_ngx_image_t *out)
{
   const ngx_lib_t *lib;
   ngx_parameter_t *p;
   ngx_vk_resource_t r_color, r_output, r_depth, r_mv, r_exposure, r_mask;
   struct retro_hw_upscaler_image color_img;
   unsigned render_w, render_h, min_w, min_h, max_w, max_h;
   unsigned flags;
   ngx_result_t res;

   if (!ngx || !inputs || !color->image || !inputs->motion_vectors.image
         || !inputs->depth.image || out_w < 32 || out_h < 32)
      return false;
   if (ngx->failed_out_w == out_w && ngx->failed_out_h == out_h
         && ngx->failed_quality == quality)
      return false;
   lib = ngx->lib;
   p   = ngx->params;
   if (!vulkan_ngx_optimal(ngx, quality, out_w, out_h, &render_w, &render_h,
            &min_w, &min_h, &max_w, &max_h))
      return false;
   if (     color->width  < min_w || color->width  > max_w
         || color->height < min_h || color->height > max_h)
      return false;

   flags = 0;
   if (!(inputs->flags & RETRO_HW_UPSCALER_MV_OUTPUT_RES))
      flags |= NGX_DLSS_FLAG_MV_LOW_RES;
   if (inputs->flags & RETRO_HW_UPSCALER_MV_JITTERED)
      flags |= NGX_DLSS_FLAG_MV_JITTERED;
   if (inputs->flags & RETRO_HW_UPSCALER_DEPTH_INVERTED)
      flags |= NGX_DLSS_FLAG_DEPTH_INVERTED;
   if (inputs->flags & RETRO_HW_UPSCALER_HDR)
      flags |= NGX_DLSS_FLAG_IS_HDR;
   if (!inputs->exposure.image)
      flags |= NGX_DLSS_FLAG_AUTO_EXPOSURE;

   if (   !ngx->feature
       || ngx->feature_out_w != out_w || ngx->feature_out_h != out_h
       || ngx->feature_max_w != max_w || ngx->feature_max_h != max_h
       || ngx->feature_flags != flags || ngx->feature_quality != quality
       || ngx->out_format != color->format)
   {
      if (!vulkan_ngx_create_feature(ngx, cmd, color->format, quality,
               flags, out_w, out_h, max_w, max_h))
         return false;
   }

   /* The output: the last frame's read is over before this write; its
    * first frame has nothing to wait for */
   vulkan_ngx_barrier(cmd, ngx->out_image, VK_IMAGE_ASPECT_COLOR_BIT,
         ngx->reset_pending ? VK_IMAGE_LAYOUT_UNDEFINED
                            : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
         VK_IMAGE_LAYOUT_GENERAL,
         VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT,
         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
   if (vulkan_ngx_input_needs_move(&inputs->motion_vectors))
      vulkan_ngx_move_input(cmd, &inputs->motion_vectors, true);
   if (vulkan_ngx_input_needs_move(&inputs->depth))
      vulkan_ngx_move_input(cmd, &inputs->depth, true);

   color_img.image  = (uint64_t)(uintptr_t)color->image;
   color_img.view   = (uint64_t)(uintptr_t)color->view;
   color_img.format = (unsigned)color->format;
   color_img.state  = (unsigned)color->layout;
   color_img.width  = color->width;
   color_img.height = color->height;
   vulkan_ngx_vk_resource(&r_color, &color_img, false);
   vulkan_ngx_vk_resource(&r_depth, &inputs->depth, false);
   vulkan_ngx_vk_resource(&r_mv, &inputs->motion_vectors, false);
   color_img.image  = (uint64_t)(uintptr_t)ngx->out_image;
   color_img.view   = (uint64_t)(uintptr_t)ngx->out_view;
   color_img.width  = out_w;
   color_img.height = out_h;
   vulkan_ngx_vk_resource(&r_output, &color_img, true);
   if (inputs->exposure.image)
      vulkan_ngx_vk_resource(&r_exposure, &inputs->exposure, false);
   if (inputs->transparency_mask.image)
      vulkan_ngx_vk_resource(&r_mask, &inputs->transparency_mask, false);

   lib->param.set_void_pointer(p, NGX_PARAM_COLOR, &r_color);
   lib->param.set_void_pointer(p, NGX_PARAM_OUTPUT, &r_output);
   lib->param.set_void_pointer(p, NGX_PARAM_DEPTH, &r_depth);
   lib->param.set_void_pointer(p, NGX_PARAM_MOTION_VECTORS, &r_mv);
   lib->param.set_void_pointer(p, NGX_PARAM_EXPOSURE_TEXTURE,
         inputs->exposure.image ? (void*)&r_exposure : NULL);
   lib->param.set_void_pointer(p, NGX_PARAM_TRANSPARENCY_MASK,
         inputs->transparency_mask.image ? (void*)&r_mask : NULL);
   lib->param.set_f(p, NGX_PARAM_JITTER_OFFSET_X, inputs->jitter_x);
   lib->param.set_f(p, NGX_PARAM_JITTER_OFFSET_Y, inputs->jitter_y);
   lib->param.set_f(p, NGX_PARAM_SHARPNESS, 0.0f);
   lib->param.set_i(p, NGX_PARAM_RESET,
         (ngx->reset_pending || (inputs->flags & RETRO_HW_UPSCALER_RESET)) ? 1 : 0);
   lib->param.set_f(p, NGX_PARAM_MV_SCALE_X,
         inputs->mv_scale_x == 0.0f ? 1.0f : inputs->mv_scale_x);
   lib->param.set_f(p, NGX_PARAM_MV_SCALE_Y,
         inputs->mv_scale_y == 0.0f ? 1.0f : inputs->mv_scale_y);
   lib->param.set_f(p, NGX_PARAM_DLSS_PRE_EXPOSURE,
         inputs->pre_exposure == 0.0f ? 1.0f : inputs->pre_exposure);
   lib->param.set_f(p, NGX_PARAM_DLSS_EXPOSURE_SCALE,
         inputs->exposure_scale == 0.0f ? 1.0f : inputs->exposure_scale);
   lib->param.set_f(p, NGX_PARAM_FRAME_TIME_DELTA_MS, inputs->frame_time_ms);
   lib->param.set_ui(p, NGX_PARAM_DLSS_RENDER_SUBRECT_WIDTH, color->width);
   lib->param.set_ui(p, NGX_PARAM_DLSS_RENDER_SUBRECT_HEIGHT, color->height);
   lib->param.set_ui(p, NGX_PARAM_DLSS_INPUT_COLOR_SUBRECT_X, 0);
   lib->param.set_ui(p, NGX_PARAM_DLSS_INPUT_COLOR_SUBRECT_Y, 0);
   lib->param.set_ui(p, NGX_PARAM_DLSS_INPUT_DEPTH_SUBRECT_X, 0);
   lib->param.set_ui(p, NGX_PARAM_DLSS_INPUT_DEPTH_SUBRECT_Y, 0);
   lib->param.set_ui(p, NGX_PARAM_DLSS_INPUT_MV_SUBRECT_X, 0);
   lib->param.set_ui(p, NGX_PARAM_DLSS_INPUT_MV_SUBRECT_Y, 0);
   lib->param.set_ui(p, NGX_PARAM_DLSS_OUTPUT_SUBRECT_X, 0);
   lib->param.set_ui(p, NGX_PARAM_DLSS_OUTPUT_SUBRECT_Y, 0);
   lib->param.set_i(p, NGX_PARAM_DLSS_INDICATOR_INVERT_X, 0);
   lib->param.set_i(p, NGX_PARAM_DLSS_INDICATOR_INVERT_Y, 0);
   res = lib->vulkan.evaluate_feature((void*)cmd, ngx->feature, p, NULL);
   ngx->reset_pending = false;

   if (vulkan_ngx_input_needs_move(&inputs->motion_vectors))
      vulkan_ngx_move_input(cmd, &inputs->motion_vectors, false);
   if (vulkan_ngx_input_needs_move(&inputs->depth))
      vulkan_ngx_move_input(cmd, &inputs->depth, false);
   vulkan_ngx_barrier(cmd, ngx->out_image, VK_IMAGE_ASPECT_COLOR_BIT,
         VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
         VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
   if (NGX_FAILED(res))
   {
      RARCH_ERR("[Vulkan] Temporal Upscaler: evaluate failed (0x%08x).\n",
            (unsigned)res);
      ngx->failed_out_w   = out_w;
      ngx->failed_out_h   = out_h;
      ngx->failed_quality = quality;
      return false;
   }
   out->image  = ngx->out_image;
   out->view   = ngx->out_view;
   out->layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
   out->format = ngx->out_format;
   out->width  = out_w;
   out->height = out_h;
   return true;
}

void vulkan_ngx_free(vulkan_ngx_t *ngx)
{
   unsigned i;
   if (!ngx)
      return;
   vulkan_ngx_retire(ngx);
   for (i = 0; i < VULKAN_NGX_RETIRED_MAX; i++)
      ngx->retired[i].frames = ngx->retired[i].frames ? 1 : 0;
   vulkan_ngx_frame_done(ngx);
   if (ngx->params)
      ngx->lib->vulkan.destroy_parameters(ngx->params);
   ngx->lib->vulkan.shutdown1((void*)ngx->device);
   free(ngx);
}
