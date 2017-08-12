/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2016-2017 - Hans-Kristian Arntzen
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

#include <retro_assert.h>
#include <dynamic/dylib.h>
#include <string/stdstring.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifdef HAVE_X11
#ifdef HAVE_XCB
#include <X11/Xlib-xcb.h>
#endif
#endif

#include "vulkan_common.h"
#include "../../configuration.h"

static dylib_t vulkan_library;
static VkInstance cached_instance;
static VkDevice cached_device;
static retro_vulkan_destroy_device_t cached_destroy_device;

#ifdef VULKAN_DEBUG
static VKAPI_ATTR VkBool32 VKAPI_CALL vulkan_debug_cb(
      VkDebugReportFlagsEXT flags,
      VkDebugReportObjectTypeEXT objectType,
      uint64_t object,
      size_t location,
      int32_t messageCode,
      const char *pLayerPrefix,
      const char *pMessage,
      void *pUserData)
{
   (void)objectType;
   (void)object;
   (void)location;
   (void)messageCode;
   (void)pUserData;

   if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT)
   {
      RARCH_ERR("[Vulkan]: Error: %s: %s\n",
            pLayerPrefix, pMessage);
   }
   else if (flags & VK_DEBUG_REPORT_WARNING_BIT_EXT)
   {
      RARCH_WARN("[Vulkan]: Warning: %s: %s\n",
            pLayerPrefix, pMessage);
   }
   else if (flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT)
   {
      RARCH_LOG("[Vulkan]: Performance warning: %s: %s\n",
            pLayerPrefix, pMessage);
   }
   else
   {
      RARCH_LOG("[Vulkan]: Information: %s: %s\n",
            pLayerPrefix, pMessage);
   }

   return VK_FALSE;
}
#endif

uint32_t vulkan_find_memory_type(
      const VkPhysicalDeviceMemoryProperties *mem_props,
      uint32_t device_reqs, uint32_t host_reqs)
{
   uint32_t i;
   for (i = 0; i < VK_MAX_MEMORY_TYPES; i++)
   {
      if ((device_reqs & (1u << i)) &&
            (mem_props->memoryTypes[i].propertyFlags & host_reqs) == host_reqs)
         return i;
   }

   RARCH_ERR("[Vulkan]: Failed to find valid memory type. This should never happen.");
   abort();
}

uint32_t vulkan_find_memory_type_fallback(
      const VkPhysicalDeviceMemoryProperties *mem_props,
      uint32_t device_reqs, uint32_t host_reqs_first,
      uint32_t host_reqs_second)
{
   uint32_t i;
   for (i = 0; i < VK_MAX_MEMORY_TYPES; i++)
   {
      if ((device_reqs & (1u << i)) &&
            (mem_props->memoryTypes[i].propertyFlags & host_reqs_first) == host_reqs_first)
         return i;
   }

   if (host_reqs_first == 0)
   {
      RARCH_ERR("[Vulkan]: Failed to find valid memory type. This should never happen.");
      abort();
   }

   return vulkan_find_memory_type_fallback(mem_props,
         device_reqs, host_reqs_second, 0);
}

void vulkan_transfer_image_ownership(VkCommandBuffer cmd,
      VkImage image, VkImageLayout layout,
      VkPipelineStageFlags src_stages,
      VkPipelineStageFlags dst_stages,
      uint32_t src_queue_family,
      uint32_t dst_queue_family)
{
   VkImageMemoryBarrier barrier =
   { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };

   barrier.srcAccessMask               = 0;
   barrier.dstAccessMask               = 0;
   barrier.oldLayout                   = layout;
   barrier.newLayout                   = layout;
   barrier.srcQueueFamilyIndex         = src_queue_family;
   barrier.dstQueueFamilyIndex         = dst_queue_family;
   barrier.image                       = image;
   barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
   barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
   barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

   vkCmdPipelineBarrier(cmd, src_stages, dst_stages,
         false, 0, NULL, 0, NULL, 1, &barrier);
}

void vulkan_map_persistent_texture(
      VkDevice device,
      struct vk_texture *texture)
{
   vkMapMemory(device, texture->memory, texture->offset,
         texture->size, 0, &texture->mapped);
}

void vulkan_copy_staging_to_dynamic(vk_t *vk, VkCommandBuffer cmd,
      struct vk_texture *dynamic,
      struct vk_texture *staging)
{
   VkImageCopy region;

   retro_assert(dynamic->type == VULKAN_TEXTURE_DYNAMIC);
   retro_assert(staging->type == VULKAN_TEXTURE_STAGING);

   vulkan_sync_texture_to_gpu(vk, staging);
   vulkan_transition_texture(vk, cmd, staging);

   /* We don't have to sync against previous TRANSFER, 
    * since we observed the completion by fences. 
    *
    * If we have a single texture_optimal, we would need to sync against
    * previous transfers to avoid races.
    *
    * We would also need to optionally maintain extra textures due to 
    * changes in resolution, so this seems like the sanest and 
    * simplest solution. */
   vulkan_image_layout_transition(vk, cmd, dynamic->image,
         VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
         0, VK_ACCESS_TRANSFER_WRITE_BIT,
         VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
         VK_PIPELINE_STAGE_TRANSFER_BIT);

   memset(&region, 0, sizeof(region));
   region.extent.width = dynamic->width;
   region.extent.height = dynamic->height;
   region.extent.depth = 1;
   region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
   region.srcSubresource.layerCount = 1;
   region.dstSubresource = region.srcSubresource;

   vkCmdCopyImage(cmd,
         staging->image, VK_IMAGE_LAYOUT_GENERAL,
         dynamic->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
         1, &region);

   vulkan_image_layout_transition(vk, cmd,
         dynamic->image,
         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
         VK_ACCESS_TRANSFER_WRITE_BIT,
         VK_ACCESS_SHADER_READ_BIT,
         VK_PIPELINE_STAGE_TRANSFER_BIT,
         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

   dynamic->layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}

#ifdef VULKAN_DEBUG_TEXTURE_ALLOC
static VkImage vk_images[4 * 1024];
static unsigned vk_count;

void vulkan_log_textures(void)
{
   unsigned i;
   for (i = 0; i < vk_count; i++)
   {
      RARCH_WARN("[Vulkan]: Found leaked texture %llu.\n",
            (unsigned long long)vk_images[i]);
   }
   vk_count = 0;
}

static unsigned track_seq;
static void vulkan_track_alloc(VkImage image)
{
   vk_images[vk_count++] = image;
   RARCH_LOG("[Vulkan]: Alloc %llu (%u).\n",
         (unsigned long long)image, track_seq);
   track_seq++;
}

static void vulkan_track_dealloc(VkImage image)
{
   unsigned i;
   for (i = 0; i < vk_count; i++)
   {
      if (image == vk_images[i])
      {
         vk_count--;
         memmove(vk_images + i, vk_images + 1 + i,
               sizeof(VkImage) * (vk_count - i));
         return;
      }
   }
   retro_assert(0 && "Couldn't find VkImage in dealloc!");
}
#endif

void vulkan_sync_texture_to_gpu(vk_t *vk, const struct vk_texture *tex)
{
   VkMappedMemoryRange range = { VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE };
   if (!tex || !tex->need_manual_cache_management || tex->memory == VK_NULL_HANDLE)
      return;

   range.memory = tex->memory;
   range.offset = 0;
   range.size = VK_WHOLE_SIZE;
   vkFlushMappedMemoryRanges(vk->context->device, 1, &range);
}

void vulkan_sync_texture_to_cpu(vk_t *vk, const struct vk_texture *tex)
{
   VkMappedMemoryRange range = { VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE };
   if (!tex || !tex->need_manual_cache_management || tex->memory == VK_NULL_HANDLE)
      return;

   range.memory = tex->memory;
   range.offset = 0;
   range.size = VK_WHOLE_SIZE;
   vkInvalidateMappedMemoryRanges(vk->context->device, 1, &range);
}

static unsigned vulkan_num_miplevels(unsigned width, unsigned height)
{
   unsigned size = MAX(width, height);
   unsigned levels = 0;
   while (size)
   {
      levels++;
      size >>= 1;
   }
   return levels;
}

struct vk_texture vulkan_create_texture(vk_t *vk,
      struct vk_texture *old,
      unsigned width, unsigned height,
      VkFormat format,
      const void *initial,
      const VkComponentMapping *swizzle,
      enum vk_texture_type type)
{
   unsigned i;
   struct vk_texture tex;
   VkMemoryRequirements mem_reqs;
   VkSubresourceLayout layout;
   VkDevice device                      = vk->context->device;
   VkImageCreateInfo info               = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
   VkImageViewCreateInfo view           = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
   VkMemoryAllocateInfo alloc           = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
   VkImageSubresource subresource       = { VK_IMAGE_ASPECT_COLOR_BIT };
   VkCommandBufferAllocateInfo cmd_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
   VkSubmitInfo submit_info             = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
   VkCommandBufferBeginInfo begin_info  = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };

   memset(&tex, 0, sizeof(tex));

   info.imageType     = VK_IMAGE_TYPE_2D;
   info.format        = format;
   info.extent.width  = width;
   info.extent.height = height;
   info.extent.depth  = 1;
   info.arrayLayers   = 1;

   /* For simplicity, always build mipmaps for
    * static textures, samplers can be used to enable it dynamically.
    */
   if (type == VULKAN_TEXTURE_STATIC)
   {
      info.mipLevels  = vulkan_num_miplevels(width, height);
      tex.mipmap      = true;
   }
   else
      info.mipLevels  = 1;

   info.samples       = VK_SAMPLE_COUNT_1_BIT;

   if (type == VULKAN_TEXTURE_STREAMED)
   {
      VkFormatProperties format_properties;
      VkFormatFeatureFlags required = VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT |
         VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;

      vkGetPhysicalDeviceFormatProperties(
            vk->context->gpu, format, &format_properties);

      if ((format_properties.linearTilingFeatures & required) != required)
      {
         RARCH_LOG("[Vulkan]: GPU does not support using linear images as textures. Falling back to copy path.\n");
         type = VULKAN_TEXTURE_STAGING;
      }
   }

   switch (type)
   {
      case VULKAN_TEXTURE_STATIC:
         retro_assert(initial && "Static textures must have initial data.\n");
         info.tiling        = VK_IMAGE_TILING_OPTIMAL;
         info.usage         = VK_IMAGE_USAGE_SAMPLED_BIT |
                              VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                              VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
         info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
         break;

      case VULKAN_TEXTURE_DYNAMIC:
         retro_assert(!initial && "Dynamic textures must not have initial data.\n");
         info.tiling        = VK_IMAGE_TILING_OPTIMAL;
         info.usage         = VK_IMAGE_USAGE_SAMPLED_BIT |
                              VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                              VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
         info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
         break;

      case VULKAN_TEXTURE_STREAMED:
         info.usage         = VK_IMAGE_USAGE_SAMPLED_BIT |
                              VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
         info.tiling        = VK_IMAGE_TILING_LINEAR;
         info.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
         break;

      case VULKAN_TEXTURE_STAGING:
         info.usage         = VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
         info.tiling        = VK_IMAGE_TILING_LINEAR;
         info.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
         break;

      case VULKAN_TEXTURE_READBACK:
         info.usage         = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
         info.tiling        = VK_IMAGE_TILING_LINEAR;
         info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
         break;
   }

   vkCreateImage(device, &info, NULL, &tex.image);
#if 0
   vulkan_track_alloc(tex.image);
#endif
   vkGetImageMemoryRequirements(device, tex.image, &mem_reqs);
   alloc.allocationSize = mem_reqs.size;

   switch (type)
   {
      case VULKAN_TEXTURE_STATIC:
      case VULKAN_TEXTURE_DYNAMIC:
         alloc.memoryTypeIndex = vulkan_find_memory_type_fallback(
               &vk->context->memory_properties,
               mem_reqs.memoryTypeBits,
               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0);
         break;

      default:
         /* Try to find a memory type which is cached, even if it means manual cache management. */
         alloc.memoryTypeIndex = vulkan_find_memory_type_fallback(
               &vk->context->memory_properties,
               mem_reqs.memoryTypeBits,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
               VK_MEMORY_PROPERTY_HOST_CACHED_BIT,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
               VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

         tex.need_manual_cache_management =
            (vk->context->memory_properties.memoryTypes[alloc.memoryTypeIndex].propertyFlags &
             VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0;
         break;
   }

   /* If the texture is STREAMED and it's not DEVICE_LOCAL, we expect to hit a slower path,
    * so fallback to copy path. */
   if (type == VULKAN_TEXTURE_STREAMED && 
         (vk->context->memory_properties.memoryTypes[alloc.memoryTypeIndex].propertyFlags &
          VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) == 0)
   {
      /* Recreate texture but for STAGING this time ... */
      RARCH_LOG("[Vulkan]: GPU supports linear images as textures, but not DEVICE_LOCAL. Falling back to copy path.\n");
      type                  = VULKAN_TEXTURE_STAGING;
      vkDestroyImage(device, tex.image, NULL);

      info.usage            = VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
      vkCreateImage(device, &info, NULL, &tex.image);

      vkGetImageMemoryRequirements(device, tex.image, &mem_reqs);

      alloc.allocationSize  = mem_reqs.size;
      alloc.memoryTypeIndex = vulkan_find_memory_type_fallback(
            &vk->context->memory_properties,
            mem_reqs.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
            VK_MEMORY_PROPERTY_HOST_CACHED_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
   }

   /* We're not reusing the objects themselves. */
   if (old && old->view != VK_NULL_HANDLE)
      vkDestroyImageView(vk->context->device, old->view, NULL);
   if (old && old->image != VK_NULL_HANDLE)
   {
      vkDestroyImage(vk->context->device, old->image, NULL);
#ifdef VULKAN_DEBUG_TEXTURE_ALLOC
      vulkan_track_dealloc(old->image);
#endif
   }

   /* We can pilfer the old memory and move it over to the new texture. */
   if (old &&
         old->memory_size >= mem_reqs.size &&
         old->memory_type == alloc.memoryTypeIndex)
   {
      tex.memory      = old->memory;
      tex.memory_size = old->memory_size;
      tex.memory_type = old->memory_type;

      if (old->mapped)
         vkUnmapMemory(device, old->memory);

      old->memory     = VK_NULL_HANDLE;
   }
   else
   {
      vkAllocateMemory(device, &alloc, NULL, &tex.memory);
      tex.memory_size = alloc.allocationSize;
      tex.memory_type = alloc.memoryTypeIndex;
   }

   if (old)
   {
      if (old->memory != VK_NULL_HANDLE)
         vkFreeMemory(device, old->memory, NULL);
      memset(old, 0, sizeof(*old));
   }

   vkBindImageMemory(device, tex.image, tex.memory, 0);

   if (type != VULKAN_TEXTURE_STAGING && type != VULKAN_TEXTURE_READBACK)
   {
      view.image                       = tex.image;
      view.viewType                    = VK_IMAGE_VIEW_TYPE_2D;
      view.format                      = format;
      if (swizzle)
         view.components               = *swizzle;
      else
      {
         view.components.r             = VK_COMPONENT_SWIZZLE_R;
         view.components.g             = VK_COMPONENT_SWIZZLE_G;
         view.components.b             = VK_COMPONENT_SWIZZLE_B;
         view.components.a             = VK_COMPONENT_SWIZZLE_A;
      }
      view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      view.subresourceRange.levelCount = info.mipLevels;
      view.subresourceRange.layerCount = 1;

      vkCreateImageView(device, &view, NULL, &tex.view);
   }
   else
      tex.view = VK_NULL_HANDLE;

   if (info.tiling == VK_IMAGE_TILING_LINEAR)
      vkGetImageSubresourceLayout(device, tex.image, &subresource, &layout);
   else
      memset(&layout, 0, sizeof(layout));

   tex.stride = layout.rowPitch;
   tex.offset = layout.offset;
   tex.size   = layout.size;
   tex.layout = info.initialLayout;

   tex.width  = width;
   tex.height = height;
   tex.format = format;
   tex.type   = type;

   if (initial && (type == VULKAN_TEXTURE_STREAMED || type == VULKAN_TEXTURE_STAGING))
   {
      unsigned y;
      uint8_t *dst       = NULL;
      const uint8_t *src = NULL;
      void *ptr          = NULL;
      unsigned bpp       = vulkan_format_to_bpp(tex.format);
      unsigned stride    = tex.width * bpp;

      vkMapMemory(device, tex.memory, tex.offset, tex.size, 0, &ptr);

      dst                = (uint8_t*)ptr;
      src                = (const uint8_t*)initial;
      for (y = 0; y < tex.height; y++, dst += tex.stride, src += stride)
         memcpy(dst, src, width * bpp);

      vulkan_sync_texture_to_gpu(vk, &tex);
      vkUnmapMemory(device, tex.memory);
   }
   else if (initial && type == VULKAN_TEXTURE_STATIC)
   {
      VkImageCopy region;
      VkCommandBuffer staging;
      struct vk_texture tmp       = vulkan_create_texture(vk, NULL,
            width, height, format, initial, NULL, VULKAN_TEXTURE_STAGING);

      cmd_info.commandPool        = vk->staging_pool;
      cmd_info.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
      cmd_info.commandBufferCount = 1;

      vkAllocateCommandBuffers(vk->context->device, &cmd_info, &staging);

      begin_info.flags            = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

      vkBeginCommandBuffer(staging, &begin_info);

      vulkan_image_layout_transition(vk, staging, tmp.image,
            VK_IMAGE_LAYOUT_PREINITIALIZED, VK_IMAGE_LAYOUT_GENERAL,
            VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
            VK_PIPELINE_STAGE_HOST_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT);

      /* If doing mipmapping on upload, keep in general so we can easily do transfers to
       * and transfers from the images without having to
       * mess around with lots of extra transitions at per-level granularity.
       */
      vulkan_image_layout_transition(vk,
            staging,
            tex.image,
            VK_IMAGE_LAYOUT_UNDEFINED,
            tex.mipmap ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            0, VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT);

      memset(&region, 0, sizeof(region));
      region.extent.width              = width;
      region.extent.height             = height;
      region.extent.depth              = 1;
      region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      region.srcSubresource.layerCount = 1;
      region.dstSubresource            = region.srcSubresource;

      vkCmdCopyImage(staging,
            tmp.image,
            VK_IMAGE_LAYOUT_GENERAL,
            tex.image,
            tex.mipmap ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, &region);

      if (tex.mipmap)
      {
         for (i = 1; i < info.mipLevels; i++)
         {
            VkImageBlit blit_region;
            unsigned src_width                        = MAX(width >> (i - 1), 1);
            unsigned src_height                       = MAX(height >> (i - 1), 1);
            unsigned target_width                     = MAX(width >> i, 1);
            unsigned target_height                    = MAX(height >> i, 1);
            memset(&blit_region, 0, sizeof(blit_region));

            blit_region.srcSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            blit_region.srcSubresource.mipLevel       = i - 1;
            blit_region.srcSubresource.baseArrayLayer = 0;
            blit_region.srcSubresource.layerCount     = 1;
            blit_region.dstSubresource                = blit_region.srcSubresource;
            blit_region.dstSubresource.mipLevel       = i;
            blit_region.srcOffsets[1].x               = src_width;
            blit_region.srcOffsets[1].y               = src_height;
            blit_region.srcOffsets[1].z               = 1;
            blit_region.dstOffsets[1].x               = target_width;
            blit_region.dstOffsets[1].y               = target_height;
            blit_region.dstOffsets[1].z               = 1;

            /* Only injects execution and memory barriers,
             * not actual transition. */
            vulkan_image_layout_transition(vk, staging, tex.image,
                  VK_IMAGE_LAYOUT_GENERAL,
                  VK_IMAGE_LAYOUT_GENERAL,
                  VK_ACCESS_TRANSFER_WRITE_BIT,
                  VK_ACCESS_TRANSFER_READ_BIT,
                  VK_PIPELINE_STAGE_TRANSFER_BIT,
                  VK_PIPELINE_STAGE_TRANSFER_BIT);

            vkCmdBlitImage(staging,
                  tex.image, VK_IMAGE_LAYOUT_GENERAL,
                  tex.image, VK_IMAGE_LAYOUT_GENERAL,
                  1, &blit_region, VK_FILTER_LINEAR);
         }

         /* Complete our texture. */
         vulkan_image_layout_transition(vk, staging, tex.image,
               VK_IMAGE_LAYOUT_GENERAL,
               VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
               VK_ACCESS_TRANSFER_WRITE_BIT,
               VK_ACCESS_SHADER_READ_BIT,
               VK_PIPELINE_STAGE_TRANSFER_BIT,
               VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
      }
      else
      {
         vulkan_image_layout_transition(vk, staging, tex.image,
               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
               VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
               VK_ACCESS_TRANSFER_WRITE_BIT,
               VK_ACCESS_SHADER_READ_BIT,
               VK_PIPELINE_STAGE_TRANSFER_BIT,
               VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
      }

      vkEndCommandBuffer(staging);
      submit_info.commandBufferCount = 1;
      submit_info.pCommandBuffers    = &staging;

#ifdef HAVE_THREADS
      slock_lock(vk->context->queue_lock);
#endif
      vkQueueSubmit(vk->context->queue,
            1, &submit_info, VK_NULL_HANDLE);

      /* TODO: Very crude, but texture uploads only happen 
       * during init, so waiting for GPU to complete transfer 
       * and blocking isn't a big deal. */
      vkQueueWaitIdle(vk->context->queue);
#ifdef HAVE_THREADS
      slock_unlock(vk->context->queue_lock);
#endif

      vkFreeCommandBuffers(vk->context->device,
            vk->staging_pool, 1, &staging);
      vulkan_destroy_texture(
            vk->context->device, &tmp);
      tex.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
   }
   return tex;
}

void vulkan_destroy_texture(
      VkDevice device,
      struct vk_texture *tex)
{
   if (tex->mapped)
      vkUnmapMemory(device, tex->memory);
   vkFreeMemory(device, tex->memory, NULL);
   if (tex->view)
      vkDestroyImageView(device, tex->view, NULL);
   vkDestroyImage(device, tex->image, NULL);
#ifdef VULKAN_DEBUG_TEXTURE_ALLOC
   vulkan_track_dealloc(tex->image);
#endif
   memset(tex, 0, sizeof(*tex));
}

static void vulkan_write_quad_descriptors(
      VkDevice device,
      VkDescriptorSet set,
      VkBuffer buffer,
      VkDeviceSize offset,
      VkDeviceSize range,
      const struct vk_texture *texture,
      VkSampler sampler)
{
   VkDescriptorBufferInfo buffer_info;
   VkWriteDescriptorSet write      = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };

   buffer_info.buffer              = buffer;
   buffer_info.offset              = offset;
   buffer_info.range               = range;

   write.dstSet                    = set;
   write.dstBinding                = 0;
   write.descriptorCount           = 1;
   write.descriptorType            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
   write.pBufferInfo               = &buffer_info;
   vkUpdateDescriptorSets(device, 1, &write, 0, NULL);

   if (texture)
   {
      VkDescriptorImageInfo image_info;

      image_info.sampler              = sampler;
      image_info.imageView            = texture->view;
      image_info.imageLayout          = texture->layout;

      write.dstSet                    = set;
      write.dstBinding                = 1;
      write.descriptorCount           = 1;
      write.descriptorType            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      write.pImageInfo                = &image_info;
      vkUpdateDescriptorSets(device, 1, &write, 0, NULL);
   }
}

void vulkan_transition_texture(vk_t *vk, VkCommandBuffer cmd, struct vk_texture *texture)
{
   /* Transition to GENERAL layout for linear streamed textures.
    * We're using linear textures here, so only 
    * GENERAL layout is supported.
    * If we're already in GENERAL, add a host -> shader read memory barrier
    * to invalidate texture caches.
    */
   if (texture->layout != VK_IMAGE_LAYOUT_PREINITIALIZED &&
       texture->layout != VK_IMAGE_LAYOUT_GENERAL)
      return;

   switch (texture->type)
   {
      case VULKAN_TEXTURE_STREAMED:
         vulkan_image_layout_transition(vk, cmd, texture->image,
               texture->layout, VK_IMAGE_LAYOUT_GENERAL,
               VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
               VK_PIPELINE_STAGE_HOST_BIT,
               VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
         break;

      case VULKAN_TEXTURE_STAGING:
         vulkan_image_layout_transition(vk, cmd, texture->image,
               texture->layout, VK_IMAGE_LAYOUT_GENERAL,
               VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
               VK_PIPELINE_STAGE_HOST_BIT,
               VK_PIPELINE_STAGE_TRANSFER_BIT);
         break;

      default:
         retro_assert(0 && "Attempting to transition invalid texture type.\n");
         break;
   }
   texture->layout = VK_IMAGE_LAYOUT_GENERAL;
}

static void vulkan_check_dynamic_state(
      vk_t *vk)
{

   if (vk->tracker.dirty & VULKAN_DIRTY_DYNAMIC_BIT)
   {
      VkRect2D sci;
      
      sci.offset.x      = vk->vp.x;
      sci.offset.y      = vk->vp.y;
      sci.extent.width  = vk->vp.width;
      sci.extent.height = vk->vp.height;

      vkCmdSetViewport(vk->cmd, 0, 1, &vk->vk_vp);
      vkCmdSetScissor (vk->cmd, 0, 1, &sci);

      vk->tracker.dirty &= ~VULKAN_DIRTY_DYNAMIC_BIT;
   }
}

void vulkan_draw_triangles(vk_t *vk, const struct vk_draw_triangles *call)
{
   if (call->texture)
      vulkan_transition_texture(vk, vk->cmd, call->texture);

   if (call->pipeline != vk->tracker.pipeline)
   {
      vkCmdBindPipeline(vk->cmd,
            VK_PIPELINE_BIND_POINT_GRAPHICS, call->pipeline);
      vk->tracker.pipeline = call->pipeline;

      /* Changing pipeline invalidates dynamic state. */
      vk->tracker.dirty |= VULKAN_DIRTY_DYNAMIC_BIT;
   }

   vulkan_check_dynamic_state(vk);

   /* Upload descriptors */
   {
      VkDescriptorSet set;

      /* Upload UBO */
      struct vk_buffer_range range;
      if (!vulkan_buffer_chain_alloc(vk->context, &vk->chain->ubo,
               call->uniform_size, &range))
         return;

      memcpy(range.data, call->uniform, call->uniform_size);

      set = vulkan_descriptor_manager_alloc(
            vk->context->device,
            &vk->chain->descriptor_manager);

      vulkan_write_quad_descriptors(
            vk->context->device,
            set,
            range.buffer,
            range.offset,
            call->uniform_size,
            call->texture,
            call->sampler);

      vkCmdBindDescriptorSets(vk->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
            vk->pipelines.layout, 0,
            1, &set, 0, NULL);

      vk->tracker.view = VK_NULL_HANDLE;
      vk->tracker.sampler = VK_NULL_HANDLE;
      memset(&vk->tracker.mvp, 0, sizeof(vk->tracker.mvp));
   }

   /* VBO is already uploaded. */
   vkCmdBindVertexBuffers(vk->cmd, 0, 1,
         &call->vbo->buffer, &call->vbo->offset);

   /* Draw the quad */
   vkCmdDraw(vk->cmd, call->vertices, 1, 0, 0);
}

void vulkan_draw_quad(vk_t *vk, const struct vk_draw_quad *quad)
{
   vulkan_transition_texture(vk, vk->cmd, quad->texture);

   if (quad->pipeline != vk->tracker.pipeline)
   {
      vkCmdBindPipeline(vk->cmd,
            VK_PIPELINE_BIND_POINT_GRAPHICS, quad->pipeline);

      vk->tracker.pipeline = quad->pipeline;
      /* Changing pipeline invalidates dynamic state. */
      vk->tracker.dirty   |= VULKAN_DIRTY_DYNAMIC_BIT;
   }

   vulkan_check_dynamic_state(vk);

   /* Upload descriptors */
   {
      VkDescriptorSet set;
      struct vk_buffer_range range;

      if (!vulkan_buffer_chain_alloc(vk->context, &vk->chain->ubo,
               sizeof(*quad->mvp), &range))
         return;

      if (
               string_is_equal_fast(quad->mvp,
                  &vk->tracker.mvp, sizeof(*quad->mvp)) 
            || quad->texture->view != vk->tracker.view
            || quad->sampler != vk->tracker.sampler)
      {
         /* Upload UBO */
         struct vk_buffer_range range;

         if (!vulkan_buffer_chain_alloc(vk->context, &vk->chain->ubo,
                  sizeof(*quad->mvp), &range))
            return;

         memcpy(range.data, quad->mvp, sizeof(*quad->mvp));

         set = vulkan_descriptor_manager_alloc(
               vk->context->device,
               &vk->chain->descriptor_manager);

         vulkan_write_quad_descriptors(
               vk->context->device,
               set,
               range.buffer,
               range.offset,
               sizeof(*quad->mvp),
               quad->texture,
               quad->sampler);

         vkCmdBindDescriptorSets(vk->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
               vk->pipelines.layout, 0,
               1, &set, 0, NULL);

         vk->tracker.view    = quad->texture->view;
         vk->tracker.sampler = quad->sampler;
         vk->tracker.mvp     = *quad->mvp;
      }
   }

   /* Upload VBO */
   {
      struct vk_buffer_range range;
      if (!vulkan_buffer_chain_alloc(vk->context, &vk->chain->vbo,
               6 * sizeof(struct vk_vertex), &range))
         return;

      vulkan_write_quad_vbo((struct vk_vertex*)range.data,
            0.0f, 0.0f, 1.0f, 1.0f,
            0.0f, 0.0f, 1.0f, 1.0f,
            &quad->color);

      vkCmdBindVertexBuffers(vk->cmd, 0, 1,
            &range.buffer, &range.offset);
   }

   /* Draw the quad */
   vkCmdDraw(vk->cmd, 6, 1, 0, 0);
}

void vulkan_image_layout_transition(
      vk_t *vk,
      VkCommandBuffer cmd, VkImage image,
      VkImageLayout old_layout,
      VkImageLayout new_layout,
      VkAccessFlags srcAccess,
      VkAccessFlags dstAccess,
      VkPipelineStageFlags srcStages,
      VkPipelineStageFlags dstStages)
{
   VkImageMemoryBarrier barrier        = 
   { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };

   barrier.srcAccessMask               = srcAccess;
   barrier.dstAccessMask               = dstAccess;
   barrier.oldLayout                   = old_layout;
   barrier.newLayout                   = new_layout;
   barrier.srcQueueFamilyIndex         = VK_QUEUE_FAMILY_IGNORED;
   barrier.dstQueueFamilyIndex         = VK_QUEUE_FAMILY_IGNORED;
   barrier.image                       = image;
   barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
   barrier.subresourceRange.levelCount = 1;
   barrier.subresourceRange.layerCount = 1;

   vkCmdPipelineBarrier(cmd,
         srcStages,
         dstStages,
         0,
         0, NULL,
         0, NULL,
         1, &barrier);
}

struct vk_buffer vulkan_create_buffer(
      const struct vulkan_context *context,
      size_t size, VkBufferUsageFlags usage)
{
   struct vk_buffer buffer;
   VkMemoryRequirements mem_reqs;
   VkMemoryAllocateInfo alloc = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
   VkBufferCreateInfo info    = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };

   info.size                  = size;
   info.usage                 = usage;
   info.sharingMode           = VK_SHARING_MODE_EXCLUSIVE;
   vkCreateBuffer(context->device, &info, NULL, &buffer.buffer);

   vkGetBufferMemoryRequirements(context->device, buffer.buffer, &mem_reqs);

   alloc.allocationSize       = mem_reqs.size;
   alloc.memoryTypeIndex      = vulkan_find_memory_type(
         &context->memory_properties,
         mem_reqs.memoryTypeBits,
         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | 
         VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
   vkAllocateMemory(context->device, &alloc, NULL, &buffer.memory);
   vkBindBufferMemory(context->device, buffer.buffer, buffer.memory, 0);

   buffer.size                = alloc.allocationSize;

   vkMapMemory(context->device,
         buffer.memory, 0, buffer.size, 0, &buffer.mapped);
   return buffer;
}

void vulkan_destroy_buffer(
      VkDevice device,
      struct vk_buffer *buffer)
{
   vkUnmapMemory(device, buffer->memory);
   vkFreeMemory(device, buffer->memory, NULL);

   vkDestroyBuffer(device, buffer->buffer, NULL);

   memset(buffer, 0, sizeof(*buffer));
}

static struct vk_descriptor_pool *vulkan_alloc_descriptor_pool(
      VkDevice device,
      const struct vk_descriptor_manager *manager)
{
   unsigned i;
   VkDescriptorPoolCreateInfo pool_info   = { 
      VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
   VkDescriptorSetAllocateInfo alloc_info = { 
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };

   struct vk_descriptor_pool *pool        = 
      (struct vk_descriptor_pool*)calloc(1, sizeof(*pool));
   if (!pool)
      return NULL;

   pool_info.maxSets       = VULKAN_DESCRIPTOR_MANAGER_BLOCK_SETS;
   pool_info.poolSizeCount = manager->num_sizes;
   pool_info.pPoolSizes    = manager->sizes;
   pool_info.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

   vkCreateDescriptorPool(device, &pool_info, NULL, &pool->pool);

   /* Just allocate all descriptor sets up front. */
   alloc_info.descriptorPool     = pool->pool;
   alloc_info.descriptorSetCount = 1;
   alloc_info.pSetLayouts        = &manager->set_layout;

   for (i = 0; i < VULKAN_DESCRIPTOR_MANAGER_BLOCK_SETS; i++)
      vkAllocateDescriptorSets(device, &alloc_info, &pool->sets[i]);

   return pool;
}

VkDescriptorSet vulkan_descriptor_manager_alloc(
      VkDevice device, struct vk_descriptor_manager *manager)
{
   if (manager->count < VULKAN_DESCRIPTOR_MANAGER_BLOCK_SETS)
      return manager->current->sets[manager->count++];

   while (manager->current->next)
   {
      manager->current = manager->current->next;
      manager->count   = 0;
      return manager->current->sets[manager->count++];
   }

   manager->current->next = vulkan_alloc_descriptor_pool(device, manager);
   retro_assert(manager->current->next);

   manager->current = manager->current->next;
   manager->count   = 0;
   return manager->current->sets[manager->count++];
}

void vulkan_descriptor_manager_restart(struct vk_descriptor_manager *manager)
{
   manager->current = manager->head;
   manager->count = 0;
}

struct vk_descriptor_manager vulkan_create_descriptor_manager(
      VkDevice device,
      const VkDescriptorPoolSize *sizes,
      unsigned num_sizes,
      VkDescriptorSetLayout set_layout)
{
   struct vk_descriptor_manager manager;
   memset(&manager, 0, sizeof(manager));
   retro_assert(num_sizes <= VULKAN_MAX_DESCRIPTOR_POOL_SIZES);
   memcpy(manager.sizes, sizes, num_sizes * sizeof(*sizes));
   manager.num_sizes  = num_sizes;
   manager.set_layout = set_layout;

   manager.head       = vulkan_alloc_descriptor_pool(device, &manager);
   retro_assert(manager.head);
   return manager;
}

void vulkan_destroy_descriptor_manager(
      VkDevice device,
      struct vk_descriptor_manager *manager)
{
   struct vk_descriptor_pool *node = manager->head;

   while (node)
   {
      struct vk_descriptor_pool *next = node->next;

      vkFreeDescriptorSets(device, node->pool,
            VULKAN_DESCRIPTOR_MANAGER_BLOCK_SETS, node->sets);
      vkDestroyDescriptorPool(device, node->pool, NULL);

      free(node);
      node = next;
   }

   memset(manager, 0, sizeof(*manager));
}

static void vulkan_buffer_chain_step(struct vk_buffer_chain *chain)
{
   chain->current = chain->current->next;
   chain->offset  = 0;
}

static bool vulkan_buffer_chain_suballoc(struct vk_buffer_chain *chain,
      size_t size, struct vk_buffer_range *range)
{
   VkDeviceSize next_offset = chain->offset + size;
   if (next_offset <= chain->current->buffer.size)
   {
      range->data   = (uint8_t*)chain->current->buffer.mapped + chain->offset;
      range->buffer = chain->current->buffer.buffer;
      range->offset = chain->offset;
      chain->offset = (next_offset + chain->alignment - 1) 
         & ~(chain->alignment - 1);

      return true;
   }
   
   return false;
}

static struct vk_buffer_node *vulkan_buffer_chain_alloc_node(
      const struct vulkan_context *context,
      size_t size, VkBufferUsageFlags usage)
{
   struct vk_buffer_node *node = (struct vk_buffer_node*)
      calloc(1, sizeof(*node));
   if (!node)
      return NULL;

   node->buffer = vulkan_create_buffer(
         context, size, usage);
   return node;
}

struct vk_buffer_chain vulkan_buffer_chain_init(
      VkDeviceSize block_size,
      VkDeviceSize alignment,
      VkBufferUsageFlags usage)
{
   struct vk_buffer_chain chain;
   
   chain.block_size = block_size;
   chain.alignment  = alignment;
   chain.offset     = 0;
   chain.usage      = usage;
   chain.head       = NULL;
   chain.current    = NULL;

   return chain;
}

void vulkan_buffer_chain_discard(struct vk_buffer_chain *chain)
{
   chain->current = chain->head;
   chain->offset = 0;
}

bool vulkan_buffer_chain_alloc(const struct vulkan_context *context,
      struct vk_buffer_chain *chain,
      size_t size, struct vk_buffer_range *range)
{
   if (!chain->head)
   {
      chain->head = vulkan_buffer_chain_alloc_node(context,
            chain->block_size, chain->usage);
      if (!chain->head)
         return false;

      chain->current = chain->head;
      chain->offset = 0;
   }

   if (vulkan_buffer_chain_suballoc(chain, size, range))
      return true;

   /* We've exhausted the current chain, traverse list until we
    * can find a block we can use. Usually, we just step once. */
   while (chain->current->next)
   {
      vulkan_buffer_chain_step(chain);
      if (vulkan_buffer_chain_suballoc(chain, size, range))
         return true;
   }

   /* We have to allocate a new node, might allocate larger
    * buffer here than block_size in case we have 
    * a very large allocation. */
   if (size < chain->block_size)
      size = chain->block_size;

   chain->current->next = vulkan_buffer_chain_alloc_node(
         context, size, chain->usage);
   if (!chain->current->next)
      return false;

   vulkan_buffer_chain_step(chain);
   /* This cannot possibly fail. */
   retro_assert(vulkan_buffer_chain_suballoc(chain, size, range));
   return true;
}

void vulkan_buffer_chain_free(
      VkDevice device,
      struct vk_buffer_chain *chain)
{
   struct vk_buffer_node *node = chain->head;
   while (node)
   {
      struct vk_buffer_node *next = node->next;
      vulkan_destroy_buffer(device, &node->buffer);

      free(node);
      node = next;
   }
   memset(chain, 0, sizeof(*chain));
}

static bool vulkan_load_instance_symbols(gfx_ctx_vulkan_data_t *vk)
{
   if (!vulkan_symbol_wrapper_load_core_instance_symbols(vk->context.instance))
      return false;

   VULKAN_SYMBOL_WRAPPER_LOAD_INSTANCE_EXTENSION_SYMBOL(vk->context.instance, vkDestroySurfaceKHR);
   VULKAN_SYMBOL_WRAPPER_LOAD_INSTANCE_EXTENSION_SYMBOL(vk->context.instance, vkGetPhysicalDeviceSurfaceSupportKHR);
   VULKAN_SYMBOL_WRAPPER_LOAD_INSTANCE_EXTENSION_SYMBOL(vk->context.instance, vkGetPhysicalDeviceSurfaceCapabilitiesKHR);
   VULKAN_SYMBOL_WRAPPER_LOAD_INSTANCE_EXTENSION_SYMBOL(vk->context.instance, vkGetPhysicalDeviceSurfaceFormatsKHR);
   VULKAN_SYMBOL_WRAPPER_LOAD_INSTANCE_EXTENSION_SYMBOL(vk->context.instance, vkGetPhysicalDeviceSurfacePresentModesKHR);
   return true;
}

static bool vulkan_load_device_symbols(gfx_ctx_vulkan_data_t *vk)
{
   if (!vulkan_symbol_wrapper_load_core_device_symbols(vk->context.device))
      return false;

   VULKAN_SYMBOL_WRAPPER_LOAD_DEVICE_EXTENSION_SYMBOL(vk->context.device, vkCreateSwapchainKHR);
   VULKAN_SYMBOL_WRAPPER_LOAD_DEVICE_EXTENSION_SYMBOL(vk->context.device, vkDestroySwapchainKHR);
   VULKAN_SYMBOL_WRAPPER_LOAD_DEVICE_EXTENSION_SYMBOL(vk->context.device, vkGetSwapchainImagesKHR);
   VULKAN_SYMBOL_WRAPPER_LOAD_DEVICE_EXTENSION_SYMBOL(vk->context.device, vkAcquireNextImageKHR);
   VULKAN_SYMBOL_WRAPPER_LOAD_DEVICE_EXTENSION_SYMBOL(vk->context.device, vkQueuePresentKHR);
   return true;
}

static bool vulkan_find_extensions(const char **exts, unsigned num_exts,
      const VkExtensionProperties *properties, unsigned property_count)
{
   unsigned i, ext;
   bool found;
   for (ext = 0; ext < num_exts; ext++)
   {
      found = false;
      for (i = 0; i < property_count; i++)
      {
         if (string_is_equal(exts[ext], properties[i].extensionName))
         {
            found = true;
            break;
         }
      }

      if (!found)
         return false;
   }
   return true;
}

static bool vulkan_find_instance_extensions(const char **exts, unsigned num_exts)
{
   uint32_t property_count;
   bool ret                          = true;
   VkExtensionProperties *properties = NULL;

   if (vkEnumerateInstanceExtensionProperties(NULL, &property_count, NULL) != VK_SUCCESS)
      return false;

   properties = (VkExtensionProperties*)malloc(property_count * sizeof(*properties));
   if (!properties)
   {
      ret = false;
      goto end;
   }

   if (vkEnumerateInstanceExtensionProperties(NULL, &property_count, properties) != VK_SUCCESS)
   {
      ret = false;
      goto end;
   }

   if (!vulkan_find_extensions(exts, num_exts, properties, property_count))
   {
      RARCH_ERR("[Vulkan]: Could not find instance extensions. Will attempt without them.\n");
      ret = false;
      goto end;
   }

end:
   free(properties);
   return ret;
}

static bool vulkan_find_device_extensions(VkPhysicalDevice gpu, const char **exts, unsigned num_exts)
{
   bool ret = true;
   VkExtensionProperties *properties = NULL;
   uint32_t property_count;

   if (vkEnumerateDeviceExtensionProperties(gpu, NULL, &property_count, NULL) != VK_SUCCESS)
      return false;

   properties = (VkExtensionProperties*)malloc(property_count * sizeof(*properties));
   if (!properties)
   {
      ret = false;
      goto end;
   }

   if (vkEnumerateDeviceExtensionProperties(gpu, NULL, &property_count, properties) != VK_SUCCESS)
   {
      ret = false;
      goto end;
   }

   if (!vulkan_find_extensions(exts, num_exts, properties, property_count))
   {
      RARCH_ERR("[Vulkan]: Could not find device extensions. Will attempt without them.\n");
      ret = false;
      goto end;
   }

end:
   free(properties);
   return ret;
}

static bool vulkan_context_init_gpu(gfx_ctx_vulkan_data_t *vk)
{
   uint32_t gpu_count     = 0;
   VkPhysicalDevice *gpus = NULL;

   if (vk->context.gpu != VK_NULL_HANDLE)
      return true;

   if (vkEnumeratePhysicalDevices(vk->context.instance,
            &gpu_count, NULL) != VK_SUCCESS)
   {
      RARCH_ERR("[Vulkan]: Failed to enumerate physical devices.\n");
      return false;
   }

   gpus = (VkPhysicalDevice*)calloc(gpu_count, sizeof(*gpus));
   if (!gpus)
   {
      RARCH_ERR("[Vulkan]: Failed to enumerate physical devices.\n");
      return false;
   }

   if (vkEnumeratePhysicalDevices(vk->context.instance,
            &gpu_count, gpus) != VK_SUCCESS)
   {
      RARCH_ERR("[Vulkan]: Failed to enumerate physical devices.\n");
      return false;
   }

   if (gpu_count < 1)
   {
      RARCH_ERR("[Vulkan]: Failed to enumerate Vulkan physical device.\n");
      free(gpus);
      return false;
   }

   vk->context.gpu = gpus[0];
   free(gpus);
   return true;
}

static bool vulkan_context_init_device(gfx_ctx_vulkan_data_t *vk)
{
   bool use_device_ext;
   uint32_t queue_count;
   VkResult res;
   unsigned i;
   static const float one             = 1.0f;
   bool found_queue                   = false;

   VkPhysicalDeviceFeatures features  = { false };
   VkDeviceQueueCreateInfo queue_info = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
   VkDeviceCreateInfo device_info     = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };

   static const char *device_extensions[] = {
      "VK_KHR_swapchain",
   };

#ifdef VULKAN_DEBUG
   static const char *device_layers[] = { "VK_LAYER_LUNARG_standard_validation" };
#endif

   struct retro_hw_render_context_negotiation_interface_vulkan *iface =
      (struct retro_hw_render_context_negotiation_interface_vulkan*)video_driver_get_context_negotiation_interface();

   if (iface && iface->interface_type != RETRO_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE_VULKAN)
   {
      RARCH_WARN("[Vulkan]: Got HW context negotiation interface, but it's the wrong API.\n");
      iface = NULL;
   }

   if (iface && iface->interface_version != RETRO_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE_VULKAN_VERSION)
   {
      RARCH_WARN("[Vulkan]: Got HW context negotiation interface, but it's the wrong interface version.\n");
      iface = NULL;
   }

   if (!cached_device && iface && iface->create_device)
   {
      struct retro_vulkan_context context = { 0 };
      const VkPhysicalDeviceFeatures features = { 0 };

      bool ret = iface->create_device(&context, vk->context.instance,
            vk->context.gpu,
            vk->vk_surface,
            vulkan_symbol_wrapper_instance_proc_addr(),
            device_extensions,
            ARRAY_SIZE(device_extensions),
#ifdef VULKAN_DEBUG
            device_layers,
            ARRAY_SIZE(device_layers),
#else
            NULL,
            0,
#endif
            &features);

      if (!ret)
      {
         RARCH_WARN("[Vulkan]: Failed to create device with negotiation interface. Falling back to default path.\n");
      }
      else
      {
         vk->context.destroy_device = iface->destroy_device;

         vk->context.device = context.device;
         vk->context.queue = context.queue;
         vk->context.gpu = context.gpu;
         vk->context.graphics_queue_index = context.queue_family_index;

         if (context.presentation_queue != context.queue)
         {
            RARCH_ERR("[Vulkan]: Present queue != graphics queue. This is currently not supported.\n");
            return false;
         }
      }
   }

   if (cached_device && cached_destroy_device)
   {
      vk->context.destroy_device = cached_destroy_device;
      cached_destroy_device = NULL;
   }

   if (!vulkan_context_init_gpu(vk))
      return false;

   vkGetPhysicalDeviceProperties(vk->context.gpu,
         &vk->context.gpu_properties);
   vkGetPhysicalDeviceMemoryProperties(vk->context.gpu,
         &vk->context.memory_properties);

   RARCH_LOG("[Vulkan]: Using GPU: %s\n", vk->context.gpu_properties.deviceName);

   if (vk->context.device == VK_NULL_HANDLE)
   {
      VkQueueFamilyProperties *queue_properties = NULL;
      vkGetPhysicalDeviceQueueFamilyProperties(vk->context.gpu,
            &queue_count, NULL);

      if (queue_count < 1)
      {
         RARCH_ERR("[Vulkan]: Invalid number of queues detected.\n");
         return false;
      }

      queue_properties = (VkQueueFamilyProperties*)malloc(queue_count * sizeof(*queue_properties));
      if (!queue_properties)
         return false;

      vkGetPhysicalDeviceQueueFamilyProperties(vk->context.gpu,
            &queue_count, queue_properties);

      for (i = 0; i < queue_count; i++)
      {
         VkQueueFlags required;
         VkBool32 supported = VK_FALSE;
         vkGetPhysicalDeviceSurfaceSupportKHR(
               vk->context.gpu, i,
               vk->vk_surface, &supported);

         required = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;
         if (supported && ((queue_properties[i].queueFlags & required) == required))
         {
            vk->context.graphics_queue_index = i;
            RARCH_LOG("[Vulkan]: Queue family %u supports %u sub-queues.\n",
                  i, queue_properties[i].queueCount);
            found_queue = true;
            break;
         }
      }

      free(queue_properties);

      if (!found_queue)
      {
         RARCH_ERR("[Vulkan]: Did not find suitable graphics queue.\n");
         return false;
      }

      use_device_ext = vulkan_find_device_extensions(vk->context.gpu,
            device_extensions, ARRAY_SIZE(device_extensions));

      queue_info.queueFamilyIndex         = vk->context.graphics_queue_index;
      queue_info.queueCount               = 1;
      queue_info.pQueuePriorities         = &one;

      device_info.queueCreateInfoCount    = 1;
      device_info.pQueueCreateInfos       = &queue_info;
      device_info.enabledExtensionCount   = use_device_ext ? ARRAY_SIZE(device_extensions) : 0;
      device_info.ppEnabledExtensionNames = use_device_ext ? device_extensions : NULL;
      device_info.pEnabledFeatures        = &features;
#ifdef VULKAN_DEBUG
      device_info.enabledLayerCount       = ARRAY_SIZE(device_layers);
      device_info.ppEnabledLayerNames     = device_layers;
#endif

      if (cached_device)
      {
         vk->context.device = cached_device;
         cached_device      = NULL;

         video_driver_set_video_cache_context_ack();
         RARCH_LOG("[Vulkan]: Using cached Vulkan context.\n");
      }
      else if (vkCreateDevice(vk->context.gpu, &device_info,
               NULL, &vk->context.device) != VK_SUCCESS)
      {
         RARCH_ERR("[Vulkan]: Failed to create device.\n");
         return false;
      }
   }

   if (!vulkan_load_device_symbols(vk))
   {
      RARCH_ERR("[Vulkan]: Failed to load device symbols.\n");
      return false;
   }

   if (vk->context.queue == VK_NULL_HANDLE)
   {
      vkGetDeviceQueue(vk->context.device,
            vk->context.graphics_queue_index, 0, &vk->context.queue);
   }

#ifdef HAVE_THREADS
   vk->context.queue_lock = slock_new();
   if (!vk->context.queue_lock)
   {
      RARCH_ERR("[Vulkan]: Failed to create queue lock.\n");
      return false;
   }
#endif

   return true;
}

bool vulkan_context_init(gfx_ctx_vulkan_data_t *vk,
      enum vulkan_wsi_type type)
{
   unsigned i;
   VkResult res;
   PFN_vkGetInstanceProcAddr GetInstanceProcAddr;
   VkInstanceCreateInfo info          = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
   VkApplicationInfo app              = { VK_STRUCTURE_TYPE_APPLICATION_INFO };

   const char *instance_extensions[4];
   unsigned ext_count = 0;

#ifdef VULKAN_DEBUG
   instance_extensions[ext_count++] = "VK_EXT_debug_report";
   static const char *instance_layers[] = { "VK_LAYER_LUNARG_standard_validation" };
#endif

   bool use_instance_ext;
   struct retro_hw_render_context_negotiation_interface_vulkan *iface =
      (struct retro_hw_render_context_negotiation_interface_vulkan*)video_driver_get_context_negotiation_interface();

   if (iface && iface->interface_type != RETRO_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE_VULKAN)
   {
      RARCH_WARN("[Vulkan]: Got HW context negotiation interface, but it's the wrong API.\n");
      iface = NULL;
   }

   if (iface && iface->interface_version != RETRO_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE_VULKAN_VERSION)
   {
      RARCH_WARN("[Vulkan]: Got HW context negotiation interface, but it's the wrong interface version.\n");
      iface = NULL;
   }

   instance_extensions[ext_count++] = "VK_KHR_surface";

   switch (type)
   {
      case VULKAN_WSI_WAYLAND:
         instance_extensions[ext_count++] = "VK_KHR_wayland_surface";
         break;
      case VULKAN_WSI_ANDROID:
         instance_extensions[ext_count++] = "VK_KHR_android_surface";
         break;
      case VULKAN_WSI_WIN32:
         instance_extensions[ext_count++] = "VK_KHR_win32_surface";
         break;
      case VULKAN_WSI_XLIB:
         instance_extensions[ext_count++] = "VK_KHR_xlib_surface";
         break;
      case VULKAN_WSI_XCB:
         instance_extensions[ext_count++] = "VK_KHR_xcb_surface";
         break;
      case VULKAN_WSI_MIR:
         instance_extensions[ext_count++] = "VK_KHR_mir_surface";
         break;
      case VULKAN_WSI_DISPLAY:
         instance_extensions[ext_count++] = "VK_KHR_display";
         break;
      case VULKAN_WSI_NONE:
      default:
         break;
   }

   if (!vulkan_library)
   {
#ifdef _WIN32
      vulkan_library = dylib_load("vulkan-1.dll");
#else
      vulkan_library = dylib_load("libvulkan.so");
#endif
   }

   if (!vulkan_library)
   {
      RARCH_ERR("[Vulkan]: Failed to open Vulkan loader.\n");
      return false;
   }

   RARCH_LOG("Vulkan dynamic library loaded.\n");
   
   GetInstanceProcAddr =
      (PFN_vkGetInstanceProcAddr)dylib_proc(vulkan_library, "vkGetInstanceProcAddr");

   if (!GetInstanceProcAddr)
   {
      RARCH_ERR("[Vulkan]: Failed to load vkGetInstanceProcAddr symbol, broken loader?\n");
      return false;
   }

   vulkan_symbol_wrapper_init(GetInstanceProcAddr);

   if (!vulkan_symbol_wrapper_load_global_symbols())
   {
      RARCH_ERR("[Vulkan]: Failed to load global Vulkan symbols, broken loader?\n");
      return false;
   }

   use_instance_ext = vulkan_find_instance_extensions(instance_extensions, ext_count);

   app.pApplicationName              = "RetroArch";
   app.applicationVersion            = 0;
   app.pEngineName                   = "RetroArch";
   app.engineVersion                 = 0;
   app.apiVersion                    = VK_MAKE_VERSION(1, 0, 18);

   info.pApplicationInfo             = &app;
   info.enabledExtensionCount        = use_instance_ext ? ext_count : 0;
   info.ppEnabledExtensionNames      = use_instance_ext ? instance_extensions : NULL;
#ifdef VULKAN_DEBUG
   info.enabledLayerCount            = ARRAY_SIZE(instance_layers);
   info.ppEnabledLayerNames          = instance_layers;
#endif

   if (iface && iface->get_application_info)
   {
      info.pApplicationInfo = iface->get_application_info();
      if (info.pApplicationInfo->pApplicationName)
      {
         RARCH_LOG("[Vulkan]: App: %s (version %u)\n",
               info.pApplicationInfo->pApplicationName,
               info.pApplicationInfo->applicationVersion);
      }

      if (info.pApplicationInfo->pEngineName)
      {
         RARCH_LOG("[Vulkan]: Engine: %s (version %u)\n",
               info.pApplicationInfo->pEngineName,
               info.pApplicationInfo->engineVersion);
      }
   }

   if (cached_instance)
   {
      vk->context.instance           = cached_instance;
      cached_instance                = NULL;
      res                            = VK_SUCCESS;
   }
   else
      res = vkCreateInstance(&info, NULL, &vk->context.instance);

#ifdef VULKAN_DEBUG
   VULKAN_SYMBOL_WRAPPER_LOAD_INSTANCE_EXTENSION_SYMBOL(vk->context.instance,
         vkCreateDebugReportCallbackEXT);
   VULKAN_SYMBOL_WRAPPER_LOAD_INSTANCE_EXTENSION_SYMBOL(vk->context.instance,
         vkDebugReportMessageEXT);
   VULKAN_SYMBOL_WRAPPER_LOAD_INSTANCE_EXTENSION_SYMBOL(vk->context.instance,
         vkDestroyDebugReportCallbackEXT);

   {
      VkDebugReportCallbackCreateInfoEXT info =
      { VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT };
      info.flags =
         VK_DEBUG_REPORT_ERROR_BIT_EXT |
         VK_DEBUG_REPORT_WARNING_BIT_EXT |
         VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
      info.pfnCallback = vulkan_debug_cb;
      vkCreateDebugReportCallbackEXT(vk->context.instance, &info, NULL, &vk->context.debug_callback);
   }
   RARCH_LOG("[Vulkan]: Enabling Vulkan debug layers.\n");
#endif

   /* Try different API versions if driver has compatible
    * but slightly different VK_API_VERSION. */
   for (i = 1; i < 4 && res == VK_ERROR_INCOMPATIBLE_DRIVER; i++)
   {
      info.pApplicationInfo = &app;
      app.apiVersion = VK_MAKE_VERSION(1, 0, i);
      res = vkCreateInstance(&info, NULL, &vk->context.instance);
   }

   if (res == VK_ERROR_INCOMPATIBLE_DRIVER)
   {
      RARCH_ERR("Failed to create Vulkan instance.\n");
      return false;
   }

   if (!vulkan_load_instance_symbols(vk))
   {
      RARCH_ERR("[Vulkan]: Failed to load instance symbols.\n");
      return false;
   }

   return true;
}

static bool vulkan_update_display_mode(
      unsigned *width,
      unsigned *height,
      const VkDisplayModePropertiesKHR *mode,
      const struct vulkan_display_surface_info *info)
{
   unsigned visible_width = mode->parameters.visibleRegion.width;
   unsigned visible_height = mode->parameters.visibleRegion.height;

   if (!info->width || !info->height)
   {
      /* Strategy here is to pick something which is largest resolution. */
      unsigned area = visible_width * visible_height;
      if (area > (*width) * (*height))
      {
         *width = visible_width;
         *height = visible_height;
         return true;
      }
      else
         return false;
   }
   else
   {
      /* For particular resolutions, find the closest. */
      int delta_x = (int)info->width - (int)visible_width;
      int delta_y = (int)info->height - (int)visible_height;
      int old_delta_x = (int)info->width - (int)*width;
      int old_delta_y = (int)info->height - (int)*height;

      int dist = delta_x * delta_x + delta_y * delta_y;
      int old_dist = old_delta_x * old_delta_x + old_delta_y * old_delta_y;
      if (dist < old_dist)
      {
         *width = visible_width;
         *height = visible_height;
         return true;
      }
      else
         return false;
   }
}

static bool vulkan_create_display_surface(gfx_ctx_vulkan_data_t *vk,
      unsigned *width, unsigned *height,
      const struct vulkan_display_surface_info *info)
{
   bool ret = true;
   uint32_t display_count = 0;
   uint32_t plane_count = 0;
   VkDisplayPropertiesKHR *displays = NULL;
   VkDisplayPlanePropertiesKHR *planes = NULL;
   uint32_t mode_count = 0;
   VkDisplayModePropertiesKHR *modes = NULL;
   unsigned dpy, i, j;
   uint32_t best_plane = UINT32_MAX;
   VkDisplayPlaneAlphaFlagBitsKHR alpha_mode = VK_DISPLAY_PLANE_ALPHA_OPAQUE_BIT_KHR;
   VkDisplaySurfaceCreateInfoKHR create_info = { VK_STRUCTURE_TYPE_DISPLAY_SURFACE_CREATE_INFO_KHR };
   VkDisplayModeKHR best_mode = VK_NULL_HANDLE;

   /* We need to decide on GPU here to be able to query support. */
   if (!vulkan_context_init_gpu(vk))
      return false;

   VULKAN_SYMBOL_WRAPPER_LOAD_INSTANCE_EXTENSION_SYMBOL(vk->context.instance,
         vkGetPhysicalDeviceDisplayPropertiesKHR);
   VULKAN_SYMBOL_WRAPPER_LOAD_INSTANCE_EXTENSION_SYMBOL(vk->context.instance,
         vkGetPhysicalDeviceDisplayPlanePropertiesKHR);
   VULKAN_SYMBOL_WRAPPER_LOAD_INSTANCE_EXTENSION_SYMBOL(vk->context.instance,
         vkGetDisplayPlaneSupportedDisplaysKHR);
   VULKAN_SYMBOL_WRAPPER_LOAD_INSTANCE_EXTENSION_SYMBOL(vk->context.instance,
         vkGetDisplayModePropertiesKHR);
   VULKAN_SYMBOL_WRAPPER_LOAD_INSTANCE_EXTENSION_SYMBOL(vk->context.instance,
         vkCreateDisplayModeKHR);
   VULKAN_SYMBOL_WRAPPER_LOAD_INSTANCE_EXTENSION_SYMBOL(vk->context.instance,
         vkGetDisplayPlaneCapabilitiesKHR);
   VULKAN_SYMBOL_WRAPPER_LOAD_INSTANCE_EXTENSION_SYMBOL(vk->context.instance,
         vkCreateDisplayPlaneSurfaceKHR);

#define GOTO_FAIL() do { \
   ret = false; \
   goto end; \
} while(0)

   if (vkGetPhysicalDeviceDisplayPropertiesKHR(vk->context.gpu, &display_count, NULL) != VK_SUCCESS)
      GOTO_FAIL();
   displays = (VkDisplayPropertiesKHR*)calloc(display_count, sizeof(*displays));
   if (!displays)
      GOTO_FAIL();
   if (vkGetPhysicalDeviceDisplayPropertiesKHR(vk->context.gpu, &display_count, displays) != VK_SUCCESS)
      GOTO_FAIL();

   if (vkGetPhysicalDeviceDisplayPlanePropertiesKHR(vk->context.gpu, &plane_count, NULL) != VK_SUCCESS)
      GOTO_FAIL();
   planes = (VkDisplayPlanePropertiesKHR*)calloc(plane_count, sizeof(*planes));
   if (!planes)
      GOTO_FAIL();
   if (vkGetPhysicalDeviceDisplayPlanePropertiesKHR(vk->context.gpu, &plane_count, planes) != VK_SUCCESS)
      GOTO_FAIL();

   for (dpy = 0; dpy < display_count; dpy++)
   {
      VkDisplayKHR display = displays[dpy].display;
      best_mode = VK_NULL_HANDLE;
      best_plane = UINT32_MAX;

      if (vkGetDisplayModePropertiesKHR(vk->context.gpu,
            display, &mode_count, NULL) != VK_SUCCESS)
         GOTO_FAIL();

      modes = (VkDisplayModePropertiesKHR*)calloc(mode_count, sizeof(*modes));
      if (!modes)
         GOTO_FAIL();

      if (vkGetDisplayModePropertiesKHR(vk->context.gpu,
            display, &mode_count, modes) != VK_SUCCESS)
         GOTO_FAIL();

      for (i = 0; i < mode_count; i++)
      {
         const VkDisplayModePropertiesKHR *mode = &modes[i];
         if (vulkan_update_display_mode(width, height, mode, info))
            best_mode = modes[i].displayMode;
      }

      free(modes);
      modes = NULL;
      mode_count = 0;

      if (best_mode == VK_NULL_HANDLE)
         continue;

      for (i = 0; i < plane_count; i++)
      {
         uint32_t supported_count = 0;
         VkDisplayKHR *supported = NULL;
         VkDisplayPlaneCapabilitiesKHR plane_caps;
         vkGetDisplayPlaneSupportedDisplaysKHR(vk->context.gpu, i, &supported_count, NULL);
         if (!supported_count)
            continue;

         supported = (VkDisplayKHR*)calloc(supported_count, sizeof(*supported));
         if (!supported)
            GOTO_FAIL();

         vkGetDisplayPlaneSupportedDisplaysKHR(vk->context.gpu, i, &supported_count,
               supported);

         for (j = 0; j < supported_count; j++)
         {
            if (supported[j] == display)
            {
               if (best_plane == UINT32_MAX)
                  best_plane = j;
               break;
            }
         }

         free(supported);
         supported = NULL;

         if (j == supported_count)
            continue;

         if (planes[i].currentDisplay == VK_NULL_HANDLE ||
             planes[i].currentDisplay == display)
            best_plane = j;
         else
            continue;

         vkGetDisplayPlaneCapabilitiesKHR(vk->context.gpu,
               best_mode, i, &plane_caps);

         if (plane_caps.supportedAlpha & VK_DISPLAY_PLANE_ALPHA_OPAQUE_BIT_KHR)
         {
            best_plane = j;
            alpha_mode = VK_DISPLAY_PLANE_ALPHA_OPAQUE_BIT_KHR;
            goto out;
         }
      }
   }
out:

   if (best_mode == VK_NULL_HANDLE)
      GOTO_FAIL();
   if (best_plane == UINT32_MAX)
      GOTO_FAIL();

   create_info.displayMode = best_mode;
   create_info.planeIndex = best_plane;
   create_info.planeStackIndex = planes[best_plane].currentStackIndex;
   create_info.transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
   create_info.globalAlpha = 1.0f;
   create_info.alphaMode = alpha_mode;
   create_info.imageExtent.width = *width;
   create_info.imageExtent.height = *height;

   if (vkCreateDisplayPlaneSurfaceKHR(vk->context.instance,
            &create_info, NULL, &vk->vk_surface) != VK_SUCCESS)
      GOTO_FAIL();

end:
   free(displays);
   free(planes);
   free(modes);
   return ret;
}

bool vulkan_surface_create(gfx_ctx_vulkan_data_t *vk,
      enum vulkan_wsi_type type,
      void *display, void *surface,
      unsigned width, unsigned height,
      unsigned swap_interval)
{
   switch (type)
   {
      case VULKAN_WSI_WAYLAND:
#ifdef HAVE_WAYLAND
         {
            PFN_vkCreateWaylandSurfaceKHR create;
            if (!VULKAN_SYMBOL_WRAPPER_LOAD_INSTANCE_SYMBOL(vk->context.instance, "vkCreateWaylandSurfaceKHR", create))
               return false;
            VkWaylandSurfaceCreateInfoKHR surf_info; 

            memset(&surf_info, 0, sizeof(surf_info));

            surf_info.sType   = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
            surf_info.pNext   = NULL;
            surf_info.flags   = 0;
            surf_info.display = (struct wl_display*)display;
            surf_info.surface = (struct wl_surface*)surface;

            if (create(vk->context.instance,
                     &surf_info, NULL, &vk->vk_surface) != VK_SUCCESS)
               return false;
         }
#endif
         break;
      case VULKAN_WSI_ANDROID:
#ifdef ANDROID
         {
            PFN_vkCreateAndroidSurfaceKHR create;
            if (!VULKAN_SYMBOL_WRAPPER_LOAD_INSTANCE_SYMBOL(vk->context.instance, "vkCreateAndroidSurfaceKHR", create))
               return false;
            VkAndroidSurfaceCreateInfoKHR surf_info;

            memset(&surf_info, 0, sizeof(surf_info));

            surf_info.sType  = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
            surf_info.flags  = 0;
            surf_info.window = (ANativeWindow*)surface;

            if (create(vk->context.instance,
                     &surf_info, NULL, &vk->vk_surface) != VK_SUCCESS)
            {
               RARCH_ERR("[Vulkan]: Failed to create Android surface.\n");
               return false;
            }
            RARCH_LOG("[Vulkan]: Created Android surface: %llu\n",
                  (unsigned long long)vk->vk_surface);
         }
#endif
         break;
      case VULKAN_WSI_WIN32:
#ifdef _WIN32
         {
            VkWin32SurfaceCreateInfoKHR surf_info;
            PFN_vkCreateWin32SurfaceKHR create;

            if (!VULKAN_SYMBOL_WRAPPER_LOAD_INSTANCE_SYMBOL(vk->context.instance, "vkCreateWin32SurfaceKHR", create))
               return false;

            memset(&surf_info, 0, sizeof(surf_info));

            surf_info.sType     = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
            surf_info.flags     = 0;
            surf_info.hinstance = *(const HINSTANCE*)display;
            surf_info.hwnd      = *(const HWND*)surface;

            if (create(vk->context.instance,
                     &surf_info, NULL, &vk->vk_surface) != VK_SUCCESS)
               return false;
         }
#endif
         break;
      case VULKAN_WSI_XLIB:
#ifdef HAVE_XLIB
         {
            PFN_vkCreateXlibSurfaceKHR create;
            if (!VULKAN_SYMBOL_WRAPPER_LOAD_INSTANCE_SYMBOL(vk->context.instance, "vkCreateXlibSurfaceKHR", create))
               return false;
            VkXlibSurfaceCreateInfoKHR surf_info;

            memset(&surf_info, 0, sizeof(surf_info));

            surf_info.sType  = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
            surf_info.flags  = 0;
            surf_info.dpy    = (Display*)display;
            surf_info.window = *(const Window*)surface;

            if (create(vk->context.instance,
                     &surf_info, NULL, &vk->vk_surface) 
                  != VK_SUCCESS)
               return false;
         }
#endif
         break;
      case VULKAN_WSI_XCB:
#ifdef HAVE_X11
#ifdef HAVE_XCB
         {
            PFN_vkCreateXcbSurfaceKHR create;
            if (!VULKAN_SYMBOL_WRAPPER_LOAD_INSTANCE_SYMBOL(vk->context.instance, "vkCreateXcbSurfaceKHR", create))
               return false;
            VkXcbSurfaceCreateInfoKHR surf_info;

            memset(&surf_info, 0, sizeof(surf_info));

            surf_info.sType      = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
            surf_info.flags      = 0;
            surf_info.connection = XGetXCBConnection((Display*)display);
            surf_info.window     = *(const xcb_window_t*)surface;

            if (create(vk->context.instance,
                     &surf_info, NULL, &vk->vk_surface) 
                  != VK_SUCCESS)
               return false;
         }
#endif
#endif
         break;
      case VULKAN_WSI_MIR:
#ifdef HAVE_MIR
         {
            PFN_vkCreateMirSurfaceKHR create;
            if (!VULKAN_SYMBOL_WRAPPER_LOAD_INSTANCE_SYMBOL(vk->context.instance, "vkCreateMirSurfaceKHR", create))
               return false;
            VkMirSurfaceCreateInfoKHR surf_info;

            memset(&surf_info, 0, sizeof(surf_info));

            surf_info.sType      = VK_STRUCTURE_TYPE_MIR_SURFACE_CREATE_INFO_KHR;
            surf_info.connection = display;
            surf_info.mirSurface = surface;

            if (create(vk->context.instance,
                     &surf_info, NULL, &vk->vk_surface) 
                  != VK_SUCCESS)
               return false;
         }
#endif
         break;
      case VULKAN_WSI_DISPLAY:
         {
            if (!vulkan_create_display_surface(vk,
                     &width, &height,
                     (const struct vulkan_display_surface_info*)display))
               return false;
         }
         break;
      case VULKAN_WSI_NONE:
      default:
         return false;
   }

   /* Must create device after surface since we need to be able to query queues to use for presentation. */
   if (!vulkan_context_init_device(vk))
      return false;

   if (!vulkan_create_swapchain(
            vk, width, height, swap_interval))
      return false;

   return true;
}

void vulkan_present(gfx_ctx_vulkan_data_t *vk, unsigned index)
{
   VkPresentInfoKHR present        = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
   VkResult result                 = VK_SUCCESS;
   VkResult err                    = VK_SUCCESS;

   present.swapchainCount          = 1;
   present.pSwapchains             = &vk->swapchain;
   present.pImageIndices           = &index;
   present.pResults                = &result;
   present.waitSemaphoreCount      = 1;
   present.pWaitSemaphores         = &vk->context.swapchain_semaphores[index];

   /* Better hope QueuePresent doesn't block D: */
#ifdef HAVE_THREADS
   slock_lock(vk->context.queue_lock);
#endif
   err = vkQueuePresentKHR(vk->context.queue, &present);

   if (err != VK_SUCCESS || result != VK_SUCCESS)
   {
      RARCH_LOG("[Vulkan]: QueuePresent failed, invalidating swapchain.\n");
      vk->context.invalid_swapchain = true;
   }

#ifdef HAVE_THREADS
   slock_unlock(vk->context.queue_lock);
#endif
}

void vulkan_context_destroy(gfx_ctx_vulkan_data_t *vk,
      bool destroy_surface)
{
   unsigned i;

   if (!vk->context.instance)
      return;

   if (vk->context.device)
      vkDeviceWaitIdle(vk->context.device);
   if (vk->swapchain)
      vkDestroySwapchainKHR(vk->context.device,
            vk->swapchain, NULL);

   if (destroy_surface && vk->vk_surface != VK_NULL_HANDLE)
      vkDestroySurfaceKHR(vk->context.instance,
            vk->vk_surface, NULL);

   for (i = 0; i < VULKAN_MAX_SWAPCHAIN_IMAGES; i++)
   {
      if (vk->context.swapchain_semaphores[i] != VK_NULL_HANDLE)
         vkDestroySemaphore(vk->context.device,
               vk->context.swapchain_semaphores[i], NULL);
      if (vk->context.swapchain_fences[i] != VK_NULL_HANDLE)
         vkDestroyFence(vk->context.device,
               vk->context.swapchain_fences[i], NULL);
   }

#ifdef VULKAN_DEBUG
   if (vk->context.debug_callback)
      vkDestroyDebugReportCallbackEXT(vk->context.instance, vk->context.debug_callback, NULL);
#endif

   if (video_driver_is_video_cache_context())
   {
      cached_device         = vk->context.device;
      cached_instance       = vk->context.instance;
      cached_destroy_device = vk->context.destroy_device;
   }
   else
   {
      if (vk->context.device)
         vkDestroyDevice(vk->context.device, NULL);
      if (vk->context.instance)
      {
         if (vk->context.destroy_device)
            vk->context.destroy_device();

         vkDestroyInstance(vk->context.instance, NULL);
         if (vulkan_library)
         {
            dylib_close(vulkan_library);
            vulkan_library = NULL;
         }
      }
   }
}

void vulkan_acquire_next_image(gfx_ctx_vulkan_data_t *vk)
{
   unsigned index;
   VkResult err;
   VkFence fence;
   VkSemaphoreCreateInfo sem_info =
   { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
   VkFenceCreateInfo fence_info =
   { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
   VkFence *next_fence             = NULL;

   vkCreateFence(vk->context.device, &fence_info, NULL, &fence);

   err = vkAcquireNextImageKHR(vk->context.device,
         vk->swapchain, UINT64_MAX,
         VK_NULL_HANDLE, fence, &vk->context.current_swapchain_index);

   index = vk->context.current_swapchain_index;
   if (vk->context.swapchain_semaphores[index] == VK_NULL_HANDLE)
      vkCreateSemaphore(vk->context.device, &sem_info,
            NULL, &vk->context.swapchain_semaphores[index]);

   vkWaitForFences(vk->context.device, 1, &fence, true, UINT64_MAX);
   vkDestroyFence(vk->context.device, fence, NULL);

   next_fence = &vk->context.swapchain_fences[index];

   if (*next_fence != VK_NULL_HANDLE)
   {
      vkWaitForFences(vk->context.device, 1, next_fence, true, UINT64_MAX);

      vkResetFences(vk->context.device, 1, next_fence);
   }
   else
      vkCreateFence(vk->context.device, &fence_info, NULL, next_fence);

   if (err != VK_SUCCESS)
   {
      RARCH_LOG("[Vulkan]: AcquireNextImage failed, invalidating swapchain.\n");
      vk->context.invalid_swapchain = true;
   }
}

bool vulkan_create_swapchain(gfx_ctx_vulkan_data_t *vk,
      unsigned width, unsigned height,
      unsigned swap_interval)
{
   unsigned i;
   uint32_t format_count;
   uint32_t present_mode_count;
   uint32_t desired_swapchain_images;
   VkSurfaceCapabilitiesKHR surface_properties;
   VkSurfaceFormatKHR formats[256];
   VkPresentModeKHR present_modes[16];
   VkSurfaceFormatKHR format;
   VkExtent2D swapchain_size;
   VkSwapchainKHR old_swapchain;
   VkSurfaceTransformFlagBitsKHR pre_transform;
   VkSwapchainCreateInfoKHR info           = { 
      VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
   VkPresentModeKHR swapchain_present_mode = VK_PRESENT_MODE_FIFO_KHR;
   settings_t                    *settings = config_get_ptr();
   VkCompositeAlphaFlagBitsKHR composite   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

   vkDeviceWaitIdle(vk->context.device);

   present_mode_count = 0;
   vkGetPhysicalDeviceSurfacePresentModesKHR(
         vk->context.gpu, vk->vk_surface,
         &present_mode_count, NULL);
   if (present_mode_count < 1 || present_mode_count > 16)
   {
      RARCH_ERR("[Vulkan]: Bogus present modes found.\n");
      return false;
   }
   vkGetPhysicalDeviceSurfacePresentModesKHR(
         vk->context.gpu, vk->vk_surface,
         &present_mode_count, present_modes);

   for (i = 0; i < present_mode_count; i++)
   {
      RARCH_LOG("[Vulkan]: Swapchain supports present mode: %u.\n",
            present_modes[i]);
   }

   vk->context.swap_interval = swap_interval;
   for (i = 0; i < present_mode_count; i++)
   {
      if (!swap_interval && present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
      {
         swapchain_present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
         break;
      }
      else if (!swap_interval && present_modes[i] 
            == VK_PRESENT_MODE_IMMEDIATE_KHR)
      {
         swapchain_present_mode = VK_PRESENT_MODE_IMMEDIATE_KHR;
         break;
      }
      else if (swap_interval && present_modes[i] == VK_PRESENT_MODE_FIFO_KHR)
      {
         /* Kind of tautological since FIFO must always be present. */
         swapchain_present_mode = VK_PRESENT_MODE_FIFO_KHR;
         break;
      }
   }

   RARCH_LOG("[Vulkan]: Creating swapchain with present mode: %u\n",
         (unsigned)swapchain_present_mode);

   vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vk->context.gpu,
         vk->vk_surface, &surface_properties);
   vkGetPhysicalDeviceSurfaceFormatsKHR(vk->context.gpu,
         vk->vk_surface, &format_count, NULL);
   vkGetPhysicalDeviceSurfaceFormatsKHR(vk->context.gpu,
         vk->vk_surface, &format_count, formats);

   format.format = VK_FORMAT_UNDEFINED;
   if (format_count == 1 && formats[0].format == VK_FORMAT_UNDEFINED)
   {
      format        = formats[0];
      format.format = VK_FORMAT_B8G8R8A8_UNORM;
   }
   else
   {
      if (format_count == 0)
      {
         RARCH_ERR("[Vulkan]: Surface has no formats.\n");
         return false;
      }

      for (i = 0; i < format_count; i++)
      {
         if (
               formats[i].format == VK_FORMAT_R8G8B8A8_UNORM ||
               formats[i].format == VK_FORMAT_B8G8R8A8_UNORM ||
               formats[i].format == VK_FORMAT_A8B8G8R8_UNORM_PACK32)
         {
            format = formats[i];
         }
      }

      if (format.format == VK_FORMAT_UNDEFINED)
         format = formats[0];
   }

   if (surface_properties.currentExtent.width == -1)
   {
      swapchain_size.width     = width;
      swapchain_size.height    = height;
   }
   else
      swapchain_size           = surface_properties.currentExtent;

   RARCH_LOG("[Vulkan]: Using swapchain size %u x %u.\n",
         swapchain_size.width, swapchain_size.height);

   desired_swapchain_images = surface_properties.minImageCount + 1;

   /* Limit latency. */
   if (desired_swapchain_images > settings->uints.video_max_swapchain_images)
      desired_swapchain_images = settings->uints.video_max_swapchain_images;

   if (desired_swapchain_images < surface_properties.minImageCount)
      desired_swapchain_images = surface_properties.minImageCount;

   if ((surface_properties.maxImageCount > 0) 
         && (desired_swapchain_images > surface_properties.maxImageCount))
      desired_swapchain_images = surface_properties.maxImageCount;

   if (surface_properties.supportedTransforms 
         & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
      pre_transform            = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
   else
      pre_transform            = surface_properties.currentTransform;

   if (surface_properties.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR)
      composite = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
   else if (surface_properties.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR)
      composite = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
   else if (surface_properties.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR)
      composite = VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR;
   else if (surface_properties.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR)
      composite = VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR;

   old_swapchain               = vk->swapchain;

   info.surface                = vk->vk_surface;
   info.minImageCount          = desired_swapchain_images;
   info.imageFormat            = format.format;
   info.imageColorSpace        = format.colorSpace;
   info.imageExtent.width      = swapchain_size.width;
   info.imageExtent.height     = swapchain_size.height;
   info.imageArrayLayers       = 1;
   info.imageSharingMode       = VK_SHARING_MODE_EXCLUSIVE;
   info.preTransform           = pre_transform;
   info.compositeAlpha         = composite;
   info.presentMode            = swapchain_present_mode;
   info.clipped                = true;
   info.oldSwapchain           = old_swapchain;
   info.imageUsage             = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT 
      | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

   if (vkCreateSwapchainKHR(vk->context.device,
            &info, NULL, &vk->swapchain) != VK_SUCCESS)
   {
      RARCH_ERR("[Vulkan]: Failed to create swapchain.\n");
      return false;
   }

   if (old_swapchain != VK_NULL_HANDLE)
   {
      RARCH_LOG("[Vulkan]: Recycled old swapchain.\n");
      vkDestroySwapchainKHR(vk->context.device, old_swapchain, NULL);
   }

   vk->context.swapchain_width  = swapchain_size.width;
   vk->context.swapchain_height = swapchain_size.height;

   /* Make sure we create a backbuffer format that is as we expect. */
   switch (format.format)
   {
      case VK_FORMAT_B8G8R8A8_SRGB:
         vk->context.swapchain_format  = VK_FORMAT_B8G8R8A8_UNORM;
         vk->context.swapchain_is_srgb = true;
         break;

      case VK_FORMAT_R8G8B8A8_SRGB:
         vk->context.swapchain_format  = VK_FORMAT_R8G8B8A8_UNORM;
         vk->context.swapchain_is_srgb = true;
         break;

      case VK_FORMAT_R8G8B8_SRGB:
         vk->context.swapchain_format  = VK_FORMAT_R8G8B8_UNORM;
         vk->context.swapchain_is_srgb = true;
         break;

      case VK_FORMAT_B8G8R8_SRGB:
         vk->context.swapchain_format  = VK_FORMAT_B8G8R8_UNORM;
         vk->context.swapchain_is_srgb = true;
         break;

      default:
         vk->context.swapchain_format  = format.format;
         break;
   }

   vkGetSwapchainImagesKHR(vk->context.device, vk->swapchain,
         &vk->context.num_swapchain_images, NULL);
   vkGetSwapchainImagesKHR(vk->context.device, vk->swapchain,
         &vk->context.num_swapchain_images, vk->context.swapchain_images);

   RARCH_LOG("[Vulkan]: Got %u swapchain images.\n",
         vk->context.num_swapchain_images);

   for (i = 0; i < vk->context.num_swapchain_images; i++)
   {
      if (vk->context.swapchain_fences[i])
      {
         vkDestroyFence(vk->context.device,
               vk->context.swapchain_fences[i], NULL);
         vk->context.swapchain_fences[i] = VK_NULL_HANDLE;
      }
   }

   vulkan_acquire_next_image(vk);

   return true;
}
