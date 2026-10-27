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

#include <vulkan/vulkan_symbol_wrapper.h>

#include "../../verbosity.h"
#include "vulkan_upscaler.h"
#include "vulkan_ngx.h"
#include "vulkan_ffx.h"
#include "vulkan_xess.h"

struct vulkan_upscaler
{
   enum video_temporal_upscaler kind;
   vulkan_ngx_t *ngx;
   vulkan_ffx_t *ffx;
   vulkan_xess_t *xess;
};

unsigned vulkan_upscaler_instance_extensions(enum video_temporal_upscaler kind,
      const char *dir, const char **list, unsigned count, unsigned cap)
{
   switch (kind)
   {
      case VIDEO_TEMPORAL_UPSCALER_DLSS:
         return vulkan_ngx_instance_extensions(list, count, cap);
      case VIDEO_TEMPORAL_UPSCALER_XESS:
         return vulkan_xess_instance_extensions(dir, list, count, cap);
      default:
         break;
   }
   return count;
}

unsigned vulkan_upscaler_device_extensions(enum video_temporal_upscaler kind,
      const char *dir, VkPhysicalDevice gpu, PFN_vkGetInstanceProcAddr gipa,
      VkInstance instance, const char **list, unsigned count, unsigned cap)
{
   switch (kind)
   {
      case VIDEO_TEMPORAL_UPSCALER_DLSS:
         return vulkan_ngx_device_extensions(gpu, gipa, instance, list, count, cap);
      case VIDEO_TEMPORAL_UPSCALER_FSR:
         return vulkan_ffx_device_extensions(dir, gpu, gipa, instance, list, count, cap);
      case VIDEO_TEMPORAL_UPSCALER_XESS:
         return vulkan_xess_device_extensions(dir, gpu, gipa, instance, list, count, cap);
      default:
         break;
   }
   return count;
}

const void *vulkan_upscaler_device_features(enum video_temporal_upscaler kind,
      const char *dir, VkPhysicalDevice gpu, PFN_vkGetInstanceProcAddr gipa,
      VkInstance instance, const void *pnext)
{
   switch (kind)
   {
      case VIDEO_TEMPORAL_UPSCALER_FSR:
         return vulkan_ffx_device_features(dir, gpu, gipa, instance, pnext);
      case VIDEO_TEMPORAL_UPSCALER_XESS:
         return vulkan_xess_device_features(dir, gpu, gipa, instance, pnext);
      default:
         break;
   }
   return pnext;
}

vulkan_upscaler_t *vulkan_upscaler_init(enum video_temporal_upscaler kind,
      VkInstance instance, VkPhysicalDevice gpu, VkDevice device,
      PFN_vkGetInstanceProcAddr gipa, PFN_vkGetDeviceProcAddr gdpa,
      const VkPhysicalDeviceMemoryProperties *memory_properties,
      const char *dir)
{
   vulkan_upscaler_t *up = (vulkan_upscaler_t*)calloc(1, sizeof(*up));
   if (!up)
      return NULL;
   up->kind = kind;
   switch (kind)
   {
      case VIDEO_TEMPORAL_UPSCALER_DLSS:
         up->ngx = vulkan_ngx_init(instance, gpu, device, gipa, gdpa,
               memory_properties, dir);
         break;
      case VIDEO_TEMPORAL_UPSCALER_FSR:
         up->ffx = vulkan_ffx_init(gpu, device, gdpa, memory_properties, dir);
         break;
      case VIDEO_TEMPORAL_UPSCALER_XESS:
         up->xess = vulkan_xess_init(instance, gpu, device, memory_properties, dir);
         break;
      default:
         break;
   }
   if (!up->ngx && !up->ffx && !up->xess)
   {
      free(up);
      return NULL;
   }
   return up;
}

bool vulkan_upscaler_optimal(vulkan_upscaler_t *up, unsigned mode,
      unsigned out_dims, unsigned *render_dims,
      unsigned *min_dims, unsigned *max_dims)
{
   if (!up)
      return false;
   switch (up->kind)
   {
      case VIDEO_TEMPORAL_UPSCALER_DLSS:
         return vulkan_ngx_optimal(up->ngx, video_upscaler_mode_ngx(mode),
               out_dims, render_dims, min_dims, max_dims);
      case VIDEO_TEMPORAL_UPSCALER_FSR:
         return vulkan_ffx_optimal(up->ffx, video_upscaler_mode_ffx(mode),
               out_dims, render_dims, min_dims, max_dims);
      case VIDEO_TEMPORAL_UPSCALER_XESS:
         return vulkan_xess_optimal(up->xess, video_upscaler_mode_xess(mode),
               out_dims, render_dims, min_dims, max_dims);
      default:
         break;
   }
   return false;
}

bool vulkan_upscaler_upscale(vulkan_upscaler_t *up, VkCommandBuffer cmd,
      const vulkan_upscaler_image_t *color,
      const struct retro_hw_upscaler_inputs *inputs,
      unsigned mode, unsigned out_dims, vulkan_upscaler_image_t *out)
{
   if (!up)
      return false;
   switch (up->kind)
   {
      case VIDEO_TEMPORAL_UPSCALER_DLSS:
      {
         vulkan_ngx_image_t in, res;
         in.image  = color->image;
         in.view   = color->view;
         in.layout = color->layout;
         in.format = color->format;
         in.dims   = color->dims;
         if (!vulkan_ngx_upscale(up->ngx, cmd, &in, inputs,
                  video_upscaler_mode_ngx(mode), video_upscaler_nr_mode(),
                  out_dims, &res))
            return false;
         out->image  = res.image;
         out->view   = res.view;
         out->layout = res.layout;
         out->format = res.format;
         out->dims   = res.dims;
         return true;
      }
      case VIDEO_TEMPORAL_UPSCALER_FSR:
         return vulkan_ffx_upscale(up->ffx, cmd, color, inputs,
               video_upscaler_mode_ffx(mode), out_dims, out);
      case VIDEO_TEMPORAL_UPSCALER_XESS:
         return vulkan_xess_upscale(up->xess, cmd, color, inputs,
               video_upscaler_mode_xess(mode), out_dims, out);
      default:
         break;
   }
   return false;
}

void vulkan_upscaler_frame_done(vulkan_upscaler_t *up)
{
   if (!up)
      return;
   vulkan_ngx_frame_done(up->ngx);
   vulkan_ffx_frame_done(up->ffx);
   vulkan_xess_frame_done(up->xess);
}

void vulkan_upscaler_free(vulkan_upscaler_t *up)
{
   if (!up)
      return;
   vulkan_ngx_free(up->ngx);
   vulkan_ffx_free(up->ffx);
   vulkan_xess_free(up->xess);
   free(up);
}

/* ---- What the backends share ---- */

VkImageAspectFlags vulkan_upscaler_aspect(VkFormat format)
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

void vulkan_upscaler_barrier(VkCommandBuffer cmd, VkImage image,
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

bool vulkan_upscaler_input_needs_move(const struct retro_hw_upscaler_image *img)
{
   return img->image
      && img->state != (unsigned)VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
      && img->state != (unsigned)VK_IMAGE_LAYOUT_GENERAL;
}

void vulkan_upscaler_move_input(VkCommandBuffer cmd,
      const struct retro_hw_upscaler_image *img, bool to_read)
{
   VkImageLayout layout = (VkImageLayout)img->state;
   vulkan_upscaler_barrier(cmd, (VkImage)(uintptr_t)img->image,
         vulkan_upscaler_aspect((VkFormat)img->format),
         to_read ? layout : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
         to_read ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : layout,
         to_read ? VK_ACCESS_MEMORY_WRITE_BIT : VK_ACCESS_MEMORY_READ_BIT,
         to_read ? VK_ACCESS_MEMORY_READ_BIT : VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
         VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
}

void vulkan_upscaler_full_barrier(VkCommandBuffer cmd)
{
   VkMemoryBarrier barrier;
   barrier.sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
   barrier.pNext         = NULL;
   barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
   barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
   vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
         VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 1, &barrier, 0, NULL, 0, NULL);
}

bool vulkan_upscaler_output_create(VkDevice device,
      const VkPhysicalDeviceMemoryProperties *memory_properties,
      VkFormat format, unsigned dims, vulkan_upscaler_output_t *out)
{
   VkImageCreateInfo image;
   VkImageViewCreateInfo view;
   VkMemoryRequirements reqs;
   VkMemoryAllocateInfo alloc;
   uint32_t type;
   unsigned i;

   memset(out, 0, sizeof(*out));
   image.sType                 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
   image.pNext                 = NULL;
   image.flags                 = 0;
   image.imageType             = VK_IMAGE_TYPE_2D;
   image.format                = format;
   image.extent.width          = VIDEO_SCALE_W(dims);
   image.extent.height         = VIDEO_SCALE_H(dims);
   image.extent.depth          = 1;
   image.mipLevels             = 1;
   image.arrayLayers           = 1;
   image.samples               = VK_SAMPLE_COUNT_1_BIT;
   image.tiling                = VK_IMAGE_TILING_OPTIMAL;
   /* (written by the library however it likes to: a transfer too) */
   image.usage                 = VK_IMAGE_USAGE_STORAGE_BIT
                               | VK_IMAGE_USAGE_SAMPLED_BIT
                               | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
                               | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
   image.sharingMode           = VK_SHARING_MODE_EXCLUSIVE;
   image.queueFamilyIndexCount = 0;
   image.pQueueFamilyIndices   = NULL;
   image.initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED;
   if (vkCreateImage(device, &image, NULL, &out->image) != VK_SUCCESS)
   {
      out->image = VK_NULL_HANDLE;
      return false;
   }

   vkGetImageMemoryRequirements(device, out->image, &reqs);
   type = memory_properties->memoryTypeCount;
   for (i = 0; i < memory_properties->memoryTypeCount; i++)
      if (   (reqs.memoryTypeBits & (1u << i))
          && (memory_properties->memoryTypes[i].propertyFlags
             & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
      {
         type = i;
         break;
      }
   if (type == memory_properties->memoryTypeCount)
      for (i = 0; i < memory_properties->memoryTypeCount; i++)
         if (reqs.memoryTypeBits & (1u << i))
         {
            type = i;
            break;
         }
   alloc.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
   alloc.pNext           = NULL;
   alloc.allocationSize  = reqs.size;
   alloc.memoryTypeIndex = type;
   if (   type == memory_properties->memoryTypeCount
       || vkAllocateMemory(device, &alloc, NULL, &out->memory) != VK_SUCCESS
       || vkBindImageMemory(device, out->image, out->memory, 0) != VK_SUCCESS)
      goto fail;

   view.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
   view.pNext                           = NULL;
   view.flags                           = 0;
   view.image                           = out->image;
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
   if (vkCreateImageView(device, &view, NULL, &out->view) != VK_SUCCESS)
      goto fail;
   out->format = format;
   out->dims   = dims;
   return true;

fail:
   vulkan_upscaler_output_destroy(device, out);
   return false;
}

void vulkan_upscaler_output_destroy(VkDevice device, vulkan_upscaler_output_t *out)
{
   if (out->view)
      vkDestroyImageView(device, out->view, NULL);
   if (out->image)
      vkDestroyImage(device, out->image, NULL);
   if (out->memory)
      vkFreeMemory(device, out->memory, NULL);
   memset(out, 0, sizeof(*out));
}

static void vulkan_upscaler_ring_free_slot(vulkan_upscaler_ring_t *ring,
      vulkan_upscaler_retired_t *slot)
{
   if (slot->feature && ring->release)
      ring->release(ring->release_data, slot->feature);
   vulkan_upscaler_output_destroy(ring->device, &slot->output);
   memset(slot, 0, sizeof(*slot));
}

void vulkan_upscaler_ring_retire(vulkan_upscaler_ring_t *ring,
      void *feature, vulkan_upscaler_output_t *output)
{
   unsigned i;
   vulkan_upscaler_retired_t *slot = NULL;
   if (!feature && !output->image)
      return;
   for (i = 0; i < VULKAN_UPSCALER_RETIRED_MAX; i++)
      if (!ring->slot[i].frames)
      {
         slot = &ring->slot[i];
         break;
      }
   if (!slot)
   {
      slot = &ring->slot[0];
      for (i = 1; i < VULKAN_UPSCALER_RETIRED_MAX; i++)
         if (ring->slot[i].frames < slot->frames)
            slot = &ring->slot[i];
      vulkan_upscaler_ring_free_slot(ring, slot);
   }
   slot->feature = feature;
   slot->output  = *output;
   slot->frames  = VULKAN_UPSCALER_RETIRE_FRAMES;
   memset(output, 0, sizeof(*output));
}

void vulkan_upscaler_ring_frame_done(vulkan_upscaler_ring_t *ring)
{
   unsigned i;
   for (i = 0; i < VULKAN_UPSCALER_RETIRED_MAX; i++)
   {
      vulkan_upscaler_retired_t *r = &ring->slot[i];
      if (!r->frames || --r->frames)
         continue;
      vulkan_upscaler_ring_free_slot(ring, r);
   }
}

void vulkan_upscaler_ring_flush(vulkan_upscaler_ring_t *ring)
{
   unsigned i;
   for (i = 0; i < VULKAN_UPSCALER_RETIRED_MAX; i++)
      if (ring->slot[i].frames)
         vulkan_upscaler_ring_free_slot(ring, &ring->slot[i]);
}
