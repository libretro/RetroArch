/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2016-2017 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#include <stdint.h>
#include <math.h>
#include <string.h>

#include <retro_assert.h>
#include <encodings/utf.h>
#include <compat/strl.h>
#include <gfx/scaler/scaler.h>
#include <gfx/video_frame.h>
#include <formats/image.h>
#include <retro_inline.h>
#include <retro_miscellaneous.h>
#include <retro_math.h>
#include <string/stdstring.h>
#include <libretro.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif
#ifdef HAVE_GFX_WIDGETS
#include "../gfx_widgets.h"
#endif

#include "../font_driver.h"
#include "../video_driver.h"

#include "../common/vulkan_common.h"

#include "../../configuration.h"
#ifdef HAVE_REWIND
#include "../../state_manager.h"
#endif

#include "../../record/record_driver.h"
#include "../../retroarch.h"
#include "../../verbosity.h"

#define VK_REMAP_TO_TEXFMT(fmt) ((fmt == VK_FORMAT_R5G6B5_UNORM_PACK16) ? VK_FORMAT_R8G8B8A8_UNORM : fmt)

typedef struct
{
   vk_t *vk;
   void *font_data;
   struct font_atlas *atlas;
   const font_renderer_driver_t *font_driver;
   struct vk_vertex *pv;
   struct vk_texture texture;
   struct vk_texture texture_optimal;
   struct vk_buffer_range range;
   unsigned vertices;

   bool needs_update;
} vulkan_raster_t;

#ifdef VULKAN_DEBUG_TEXTURE_ALLOC
static VkImage vk_images[4 * 1024];
static unsigned vk_count;
static unsigned track_seq;
#endif

/*
 * VULKAN COMMON
 */


#ifdef VULKAN_DEBUG_TEXTURE_ALLOC
#if 0
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
#endif

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

static INLINE unsigned vulkan_format_to_bpp(VkFormat format)
{
   switch (format)
   {
      case VK_FORMAT_B8G8R8A8_UNORM:
         return 4;
      case VK_FORMAT_R4G4B4A4_UNORM_PACK16:
      case VK_FORMAT_B4G4R4A4_UNORM_PACK16:
      case VK_FORMAT_R5G6B5_UNORM_PACK16:
         return 2;
      case VK_FORMAT_R8_UNORM:
         return 1;
      default: /* Unknown format */
         break;
   }

   return 0;
}

static unsigned vulkan_num_miplevels(unsigned width, unsigned height)
{
   unsigned size   = MAX(width, height);
   unsigned levels = 0;
   while (size)
   {
      levels++;
      size >>= 1;
   }
   return levels;
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
   VkWriteDescriptorSet write;
   VkDescriptorBufferInfo buffer_info;

   buffer_info.buffer              = buffer;
   buffer_info.offset              = offset;
   buffer_info.range               = range;

   write.sType                     = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
   write.pNext                     = NULL;
   write.dstSet                    = set;
   write.dstBinding                = 0;
   write.dstArrayElement           = 0;
   write.descriptorCount           = 1;
   write.descriptorType            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
   write.pImageInfo                = NULL;
   write.pBufferInfo               = &buffer_info;
   write.pTexelBufferView          = NULL;
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


static void vulkan_transition_texture(vk_t *vk, VkCommandBuffer cmd, struct vk_texture *texture)
{
   /* Transition to GENERAL layout for linear streamed textures.
    * We're using linear textures here, so only
    * GENERAL layout is supported.
    * If we're already in GENERAL, add a host -> shader read memory barrier
    * to invalidate texture caches.
    */
   if (   (texture->layout != VK_IMAGE_LAYOUT_PREINITIALIZED)
       && (texture->layout != VK_IMAGE_LAYOUT_GENERAL))
      return;

   switch (texture->type)
   {
      case VULKAN_TEXTURE_STREAMED:
         VULKAN_IMAGE_LAYOUT_TRANSITION(cmd, texture->image,
               texture->layout, VK_IMAGE_LAYOUT_GENERAL,
               VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
               VK_PIPELINE_STAGE_HOST_BIT,
               VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
         break;

      default:
         retro_assert(0 && "Attempting to transition invalid texture type.\n");
         break;
   }
   texture->layout = VK_IMAGE_LAYOUT_GENERAL;
}

/* The VBO needs to be written to before calling this.
 * Use vulkan_buffer_chain_alloc. */
static void vulkan_draw_triangles(vk_t *vk, const struct vk_draw_triangles *call)
{
   if (call->texture && call->texture->image)
      vulkan_transition_texture(vk, vk->cmd, call->texture);

   if (call->pipeline != vk->tracker.pipeline)
   {
      VkRect2D sci;
      vkCmdBindPipeline(vk->cmd,
            VK_PIPELINE_BIND_POINT_GRAPHICS, call->pipeline);
      vk->tracker.pipeline = call->pipeline;

      /* Changing pipeline invalidates dynamic state. */
      vk->tracker.dirty |= VULKAN_DIRTY_DYNAMIC_BIT;

      if (vk->flags & VK_FLAG_TRACKER_USE_SCISSOR)
         sci               = vk->tracker.scissor;
      else
      {
         /* No scissor -> viewport */
         sci.offset.x      = vk->vp.x;
         sci.offset.y      = vk->vp.y;
         sci.extent.width  = vk->vp.width;
         sci.extent.height = vk->vp.height;
      }

      vkCmdSetViewport(vk->cmd, 0, 1, &vk->vk_vp);
      vkCmdSetScissor (vk->cmd, 0, 1, &sci);

      vk->tracker.dirty &= ~VULKAN_DIRTY_DYNAMIC_BIT;
   }
   else if (vk->tracker.dirty & VULKAN_DIRTY_DYNAMIC_BIT)
   {
      VkRect2D sci;
      if (vk->flags & VK_FLAG_TRACKER_USE_SCISSOR)
         sci               = vk->tracker.scissor;
      else
      {
         /* No scissor -> viewport */
         sci.offset.x      = vk->vp.x;
         sci.offset.y      = vk->vp.y;
         sci.extent.width  = vk->vp.width;
         sci.extent.height = vk->vp.height;
      }

      vkCmdSetViewport(vk->cmd, 0, 1, &vk->vk_vp);
      vkCmdSetScissor (vk->cmd, 0, 1, &sci);

      vk->tracker.dirty &= ~VULKAN_DIRTY_DYNAMIC_BIT;
   }

   /* Upload descriptors */
   {
      VkDescriptorSet set;
      /* Upload UBO */
      struct vk_buffer_range range;
      float *mvp_data_ptr          = NULL;

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

      vkCmdBindDescriptorSets(vk->cmd,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            vk->pipelines.layout, 0,
            1, &set, 0, NULL);

      vk->tracker.view    = VK_NULL_HANDLE;
      vk->tracker.sampler = VK_NULL_HANDLE;
      for (
              mvp_data_ptr = &vk->tracker.mvp.data[0]
            ; mvp_data_ptr < vk->tracker.mvp.data + 16
            ; mvp_data_ptr++)
         *mvp_data_ptr = 0.0f;
   }

   /* VBO is already uploaded. */
   vkCmdBindVertexBuffers(vk->cmd, 0, 1,
         &call->vbo->buffer, &call->vbo->offset);

   /* Draw the quad */
   vkCmdDraw(vk->cmd, call->vertices, 1, 0, 0);
}


static void vulkan_destroy_texture(
      VkDevice device,
      struct vk_texture *tex)
{
   if (tex->mapped)
      vkUnmapMemory(device, tex->memory);
   if (tex->view)
      vkDestroyImageView(device, tex->view, NULL);
   if (tex->image)
      vkDestroyImage(device, tex->image, NULL);
   if (tex->buffer)
      vkDestroyBuffer(device, tex->buffer, NULL);
   if (tex->memory)
      vkFreeMemory(device, tex->memory, NULL);

#ifdef VULKAN_DEBUG_TEXTURE_ALLOC
   if (tex->image)
      vulkan_track_dealloc(tex->image);
#endif
   tex->type                          = VULKAN_TEXTURE_STREAMED;
   tex->flags                         = 0;
   tex->memory_type                   = 0;
   tex->width                         = 0;
   tex->height                        = 0;
   tex->offset                        = 0;
   tex->stride                        = 0;
   tex->size                          = 0;
   tex->mapped                        = NULL;
   tex->image                         = VK_NULL_HANDLE;
   tex->view                          = VK_NULL_HANDLE;
   tex->memory                        = VK_NULL_HANDLE;
   tex->buffer                        = VK_NULL_HANDLE;
   tex->format                        = VK_FORMAT_UNDEFINED;
   tex->memory_size                   = 0;
   tex->layout                        = VK_IMAGE_LAYOUT_UNDEFINED;
}

static struct vk_texture vulkan_create_texture(vk_t *vk,
      struct vk_texture *old,
      unsigned width, unsigned height,
      VkFormat format,
      const void *initial,
      const VkComponentMapping *swizzle,
      enum vk_texture_type type)
{
   unsigned i;
   uint32_t buffer_width;
   struct vk_texture tex;
   VkImageCreateInfo info;
   VkFormat remap_tex_fmt;
   VkMemoryRequirements mem_reqs;
   VkSubresourceLayout layout;
   VkMemoryAllocateInfo alloc;
   VkBufferCreateInfo buffer_info;
   VkDevice device                      = vk->context->device;
   VkImageSubresource subresource       = { VK_IMAGE_ASPECT_COLOR_BIT };

   memset(&tex, 0, sizeof(tex));

   info.sType                 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
   info.pNext                 = NULL;
   info.flags                 = 0;
   info.imageType             = VK_IMAGE_TYPE_2D;
   info.format                = format;
   info.extent.width          = width;
   info.extent.height         = height;
   info.extent.depth          = 1;
   info.mipLevels             = 1;
   info.arrayLayers           = 1;
   info.samples               = VK_SAMPLE_COUNT_1_BIT;
   info.tiling                = VK_IMAGE_TILING_OPTIMAL;
   info.usage                 = 0;
   info.sharingMode           = VK_SHARING_MODE_EXCLUSIVE;
   info.queueFamilyIndexCount = 0;
   info.pQueueFamilyIndices   = NULL;
   info.initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED;

   /* Align stride to 4 bytes to make sure we can use compute shader uploads without too many problems. */
   buffer_width                      = width * vulkan_format_to_bpp(format);
   buffer_width                      = (buffer_width + 3u) & ~3u;

   buffer_info.sType                 = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
   buffer_info.pNext                 = NULL;
   buffer_info.flags                 = 0;
   buffer_info.size                  = buffer_width * height;
   buffer_info.usage                 = 0;
   buffer_info.sharingMode           = VK_SHARING_MODE_EXCLUSIVE;
   buffer_info.queueFamilyIndexCount = 0;
   buffer_info.pQueueFamilyIndices   = NULL;

   remap_tex_fmt                     = VK_REMAP_TO_TEXFMT(format);

   /* Compatibility concern. Some Apple hardware does not support rgb565.
    * Use compute shader uploads instead.
    * If we attempt to use streamed texture, force staging path.
    * If we're creating fallback dynamic texture, force RGBA8888. */
   if (remap_tex_fmt != format)
   {
      if (type == VULKAN_TEXTURE_STREAMED)
         type        = VULKAN_TEXTURE_STAGING;
      else if (type == VULKAN_TEXTURE_DYNAMIC)
      {
         format      = remap_tex_fmt;
         info.format = format;
         info.usage |= VK_IMAGE_USAGE_STORAGE_BIT;
      }
   }

   if (type == VULKAN_TEXTURE_STREAMED)
   {
      VkFormatProperties format_properties;
      const VkFormatFeatureFlags required = VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT
                                          | VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;

      vkGetPhysicalDeviceFormatProperties(
            vk->context->gpu, format, &format_properties);

      if ((format_properties.linearTilingFeatures & required) != required)
      {
#ifdef VULKAN_DEBUG
         RARCH_DBG("[Vulkan]: GPU does not support using linear images as textures. Falling back to copy path.\n");
#endif
         type = VULKAN_TEXTURE_STAGING;
      }
   }

   switch (type)
   {
      case VULKAN_TEXTURE_STATIC:
         /* For simplicity, always build mipmaps for
          * static textures, samplers can be used to enable it dynamically.
          */
         info.mipLevels     = vulkan_num_miplevels(width, height);
         tex.flags         |= VK_TEX_FLAG_MIPMAP;
         retro_assert(initial && "Static textures must have initial data.\n");
         info.tiling        = VK_IMAGE_TILING_OPTIMAL;
         info.usage         = VK_IMAGE_USAGE_SAMPLED_BIT
                            | VK_IMAGE_USAGE_TRANSFER_DST_BIT
                            | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
         info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
         break;

      case VULKAN_TEXTURE_DYNAMIC:
         retro_assert(!initial && "Dynamic textures must not have initial data.\n");
         info.tiling        = VK_IMAGE_TILING_OPTIMAL;
         info.usage        |= VK_IMAGE_USAGE_SAMPLED_BIT
                            | VK_IMAGE_USAGE_TRANSFER_DST_BIT
                            | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
         info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
         break;

      case VULKAN_TEXTURE_STREAMED:
         info.usage         = VK_IMAGE_USAGE_SAMPLED_BIT
                            | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
         info.tiling        = VK_IMAGE_TILING_LINEAR;
         info.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
         break;

      case VULKAN_TEXTURE_STAGING:
         buffer_info.usage  = VK_BUFFER_USAGE_TRANSFER_SRC_BIT
                            | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
         info.initialLayout = VK_IMAGE_LAYOUT_GENERAL;
         info.tiling        = VK_IMAGE_TILING_LINEAR;
         break;

      case VULKAN_TEXTURE_READBACK:
         buffer_info.usage  = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
         info.initialLayout = VK_IMAGE_LAYOUT_GENERAL;
         info.tiling        = VK_IMAGE_TILING_LINEAR;
         break;
   }

   if (     (type != VULKAN_TEXTURE_STAGING)
         && (type != VULKAN_TEXTURE_READBACK))
   {
      vkCreateImage(device, &info, NULL, &tex.image);
      vulkan_debug_mark_image(device, tex.image);
#if 0
      vulkan_track_alloc(tex.image);
#endif
      vkGetImageMemoryRequirements(device, tex.image, &mem_reqs);
   }
   else
   {
      /* Linear staging textures are not guaranteed to be supported,
       * use buffers instead. */
      vkCreateBuffer(device, &buffer_info, NULL, &tex.buffer);
      vulkan_debug_mark_buffer(device, tex.buffer);
      vkGetBufferMemoryRequirements(device, tex.buffer, &mem_reqs);
   }

   alloc.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
   alloc.pNext           = NULL;
   alloc.allocationSize  = mem_reqs.size;
   alloc.memoryTypeIndex = 0;

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
         /* Try to find a memory type which is cached,
          * even if it means manual cache management. */
         alloc.memoryTypeIndex = vulkan_find_memory_type_fallback(
               &vk->context->memory_properties,
               mem_reqs.memoryTypeBits,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
               | VK_MEMORY_PROPERTY_HOST_CACHED_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
               | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

         if ((vk->context->memory_properties.memoryTypes
                  [ alloc.memoryTypeIndex].propertyFlags
                  & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0)
            tex.flags |= VK_TEX_FLAG_NEED_MANUAL_CACHE_MANAGEMENT;

         /* If the texture is STREAMED and it's not DEVICE_LOCAL, we expect to hit a slower path,
          * so fallback to copy path. */
         if (      type == VULKAN_TEXTURE_STREAMED
               && (vk->context->memory_properties.memoryTypes[
                     alloc.memoryTypeIndex].propertyFlags
                   & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) == 0)
         {
            /* Recreate texture but for STAGING this time ... */
#ifdef VULKAN_DEBUG
            RARCH_DBG("[Vulkan]: GPU supports linear images as textures, but not DEVICE_LOCAL. Falling back to copy path.\n");
#endif
            type                  = VULKAN_TEXTURE_STAGING;
            vkDestroyImage(device, tex.image, NULL);
            tex.image             = VK_NULL_HANDLE;
            info.initialLayout    = VK_IMAGE_LAYOUT_GENERAL;

            buffer_info.usage     = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            vkCreateBuffer(device, &buffer_info, NULL, &tex.buffer);
            vulkan_debug_mark_buffer(device, tex.buffer);
            vkGetBufferMemoryRequirements(device, tex.buffer, &mem_reqs);

            alloc.allocationSize  = mem_reqs.size;
            alloc.memoryTypeIndex = vulkan_find_memory_type_fallback(
                    &vk->context->memory_properties,
                    mem_reqs.memoryTypeBits,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                  | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
                  | VK_MEMORY_PROPERTY_HOST_CACHED_BIT,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                  | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
         }
         break;
   }

   /* We're not reusing the objects themselves. */
   if (old)
   {
      if (old->view != VK_NULL_HANDLE)
         vkDestroyImageView(vk->context->device, old->view, NULL);
      if (old->image != VK_NULL_HANDLE)
      {
         vkDestroyImage(vk->context->device, old->image, NULL);
#ifdef VULKAN_DEBUG_TEXTURE_ALLOC
         vulkan_track_dealloc(old->image);
#endif
      }
      if (old->buffer != VK_NULL_HANDLE)
         vkDestroyBuffer(vk->context->device, old->buffer, NULL);
   }

   /* We can pilfer the old memory and move it over to the new texture. */
   if (     old
         && old->memory_size >= mem_reqs.size
         && old->memory_type == alloc.memoryTypeIndex)
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
      vulkan_debug_mark_memory(device, tex.memory);
      tex.memory_size = alloc.allocationSize;
      tex.memory_type = alloc.memoryTypeIndex;
   }

   if (old)
   {
      if (old->memory != VK_NULL_HANDLE)
         vkFreeMemory(device, old->memory, NULL);
      memset(old, 0, sizeof(*old));
   }

   if (tex.image)
      vkBindImageMemory(device, tex.image, tex.memory, 0);
   if (tex.buffer)
      vkBindBufferMemory(device, tex.buffer, tex.memory, 0);

   if (     type != VULKAN_TEXTURE_STAGING
         && type != VULKAN_TEXTURE_READBACK)
   {
      VkImageViewCreateInfo view;
      view.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
      view.pNext                           = NULL;
      view.flags                           = 0;
      view.image                           = tex.image;
      view.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
      view.format                          = format;
      if (swizzle)
         view.components                   = *swizzle;
      else
      {
         view.components.r                 = VK_COMPONENT_SWIZZLE_R;
         view.components.g                 = VK_COMPONENT_SWIZZLE_G;
         view.components.b                 = VK_COMPONENT_SWIZZLE_B;
         view.components.a                 = VK_COMPONENT_SWIZZLE_A;
      }
      view.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
      view.subresourceRange.baseMipLevel   = 0;
      view.subresourceRange.levelCount     = info.mipLevels;
      view.subresourceRange.baseArrayLayer = 0;
      view.subresourceRange.layerCount     = 1;

      vkCreateImageView(device, &view, NULL, &tex.view);
   }
   else
      tex.view        = VK_NULL_HANDLE;

   if (     tex.image
         && info.tiling == VK_IMAGE_TILING_LINEAR)
      vkGetImageSubresourceLayout(device, tex.image, &subresource, &layout);
   else if (tex.buffer)
   {
      layout.offset   = 0;
      layout.size     = buffer_info.size;
      layout.rowPitch = buffer_width;
   }
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

   if (initial)
   {
      switch (type)
      {
         case VULKAN_TEXTURE_STREAMED:
         case VULKAN_TEXTURE_STAGING:
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

               if (     (tex.flags & VK_TEX_FLAG_NEED_MANUAL_CACHE_MANAGEMENT)
                     && (tex.memory != VK_NULL_HANDLE))
                  VULKAN_SYNC_TEXTURE_TO_GPU(vk->context->device, tex.memory);
               vkUnmapMemory(device, tex.memory);
            }
            break;
         case VULKAN_TEXTURE_STATIC:
            {
               VkBufferImageCopy region;
               VkCommandBuffer staging;
               VkSubmitInfo submit_info;
               VkCommandBufferBeginInfo begin_info;
               VkCommandBufferAllocateInfo cmd_info;
               enum VkImageLayout layout_fmt =
                  (tex.flags & VK_TEX_FLAG_MIPMAP)
                  ? VK_IMAGE_LAYOUT_GENERAL
                  : VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
               struct vk_texture tmp                = vulkan_create_texture(vk, NULL,
                     width, height, format, initial, NULL, VULKAN_TEXTURE_STAGING);

               cmd_info.sType                       = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
               cmd_info.pNext                       = NULL;
               cmd_info.commandPool                 = vk->staging_pool;
               cmd_info.level                       = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
               cmd_info.commandBufferCount          = 1;

               vkAllocateCommandBuffers(vk->context->device,
                     &cmd_info, &staging);

               begin_info.sType                     = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
               begin_info.pNext                     = NULL;
               begin_info.flags                     = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
               begin_info.pInheritanceInfo          = NULL;

               vkBeginCommandBuffer(staging, &begin_info);

               /* If doing mipmapping on upload, keep in general
                * so we can easily do transfers to
                * and transfers from the images without having to
                * mess around with lots of extra transitions at
                * per-level granularity.
                */
               VULKAN_IMAGE_LAYOUT_TRANSITION(
                     staging,
                     tex.image,
                     VK_IMAGE_LAYOUT_UNDEFINED,
                     layout_fmt,
                     0, VK_ACCESS_TRANSFER_WRITE_BIT,
                     VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                     VK_PIPELINE_STAGE_TRANSFER_BIT);

               memset(&region, 0, sizeof(region));
               region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
               region.imageSubresource.layerCount = 1;
               region.imageExtent.width           = width;
               region.imageExtent.height          = height;
               region.imageExtent.depth           = 1;

               vkCmdCopyBufferToImage(staging, tmp.buffer,
                     tex.image, layout_fmt, 1, &region);

               if (tex.flags & VK_TEX_FLAG_MIPMAP)
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
                     VULKAN_IMAGE_LAYOUT_TRANSITION(
                           staging,
                           tex.image,
                           VK_IMAGE_LAYOUT_GENERAL,
                           VK_IMAGE_LAYOUT_GENERAL,
                           VK_ACCESS_TRANSFER_WRITE_BIT,
                           VK_ACCESS_TRANSFER_READ_BIT,
                           VK_PIPELINE_STAGE_TRANSFER_BIT,
                           VK_PIPELINE_STAGE_TRANSFER_BIT);

                     vkCmdBlitImage(
                           staging,
                           tex.image,
                           VK_IMAGE_LAYOUT_GENERAL,
                           tex.image,
                           VK_IMAGE_LAYOUT_GENERAL,
                           1,
                           &blit_region,
                           VK_FILTER_LINEAR);
                  }
               }

               /* Complete our texture. */
               VULKAN_IMAGE_LAYOUT_TRANSITION(
                     staging,
                     tex.image,
                     layout_fmt,
                     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                     VK_ACCESS_TRANSFER_WRITE_BIT,
                     VK_ACCESS_SHADER_READ_BIT,
                     VK_PIPELINE_STAGE_TRANSFER_BIT,
                     VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

               vkEndCommandBuffer(staging);
               submit_info.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
               submit_info.pNext                = NULL;
               submit_info.waitSemaphoreCount   = 0;
               submit_info.pWaitSemaphores      = NULL;
               submit_info.pWaitDstStageMask    = NULL;
               submit_info.commandBufferCount   = 1;
               submit_info.pCommandBuffers      = &staging;
               submit_info.signalSemaphoreCount = 0;
               submit_info.pSignalSemaphores    = NULL;

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
            break;
         case VULKAN_TEXTURE_DYNAMIC:
         case VULKAN_TEXTURE_READBACK:
            /* TODO/FIXME - stubs */
            break;
      }
   }

   return tex;
}

/* Dynamic texture type should be set to : VULKAN_TEXTURE_DYNAMIC
 * Staging texture type should be set to : VULKAN_TEXTURE_STAGING
 */
static void vulkan_copy_staging_to_dynamic(vk_t *vk, VkCommandBuffer cmd,
      struct vk_texture *dynamic, struct vk_texture *staging)
{
   bool compute_upload = dynamic->format != staging->format;

   if (compute_upload)
   {
      const uint32_t ubo[3] = { dynamic->width, dynamic->height, (uint32_t)(staging->stride / 4) /* in terms of u32 words */ };
      VkWriteDescriptorSet write;
      VkDescriptorBufferInfo buffer_info;
      VkDescriptorImageInfo image_info;
      struct vk_buffer_range range;
      VkDescriptorSet set;

      VULKAN_IMAGE_LAYOUT_TRANSITION(
            cmd,
            dynamic->image,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_GENERAL,
            0,
            VK_ACCESS_SHADER_WRITE_BIT,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

      /* staging->format is always RGB565 here.
       * Can be expanded as needed if more cases are added to VK_REMAP_TO_TEXFMT. */
      retro_assert(staging->format == VK_FORMAT_R5G6B5_UNORM_PACK16);

      set = vulkan_descriptor_manager_alloc(
            vk->context->device,
            &vk->chain->descriptor_manager);

      if (!vulkan_buffer_chain_alloc(vk->context, &vk->chain->ubo,
            sizeof(ubo), &range))
         return;

      memcpy(range.data, ubo, sizeof(ubo));
      VULKAN_SET_UNIFORM_BUFFER(vk->context->device,
            set,
            0,
            range.buffer,
            range.offset,
            sizeof(ubo));

      image_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
      image_info.imageView   = dynamic->view;
      image_info.sampler     = VK_NULL_HANDLE;

      buffer_info.buffer     = staging->buffer;
      buffer_info.offset     = 0;
      buffer_info.range      = VK_WHOLE_SIZE;

      write.sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      write.pNext            = NULL;
      write.dstSet           = set;
      write.dstBinding       = 3;
      write.dstArrayElement  = 0;
      write.descriptorCount  = 1;
      write.descriptorType   = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
      write.pImageInfo       = &image_info;
      write.pBufferInfo      = NULL;
      write.pTexelBufferView = NULL;

      vkUpdateDescriptorSets(vk->context->device, 1, &write, 0, NULL);

      write.descriptorType   = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
      write.dstBinding       = 4;
      write.pImageInfo       = NULL;
      write.pBufferInfo      = &buffer_info;

      vkUpdateDescriptorSets(vk->context->device, 1, &write, 0, NULL);

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, vk->pipelines.rgb565_to_rgba8888);
      vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, vk->pipelines.layout, 0, 1, &set, 0, NULL);
      vkCmdDispatch(cmd, (dynamic->width + 15) / 16, (dynamic->height + 7) / 8, 1);

      VULKAN_IMAGE_LAYOUT_TRANSITION(
            cmd,
            dynamic->image,
            VK_IMAGE_LAYOUT_GENERAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_ACCESS_SHADER_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
   }
   else
   {
      VkBufferImageCopy region;

      VULKAN_IMAGE_LAYOUT_TRANSITION(
            cmd,
            dynamic->image,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            0,
            VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT);
      region.bufferOffset                    = 0;
      region.bufferRowLength                 = 0;
      region.bufferImageHeight               = 0;
      region.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
      region.imageSubresource.mipLevel       = 0;
      region.imageSubresource.baseArrayLayer = 0;
      region.imageSubresource.layerCount     = 1;
      region.imageOffset.x                   = 0;
      region.imageOffset.y                   = 0;
      region.imageOffset.z                   = 0;
      region.imageExtent.width               = dynamic->width;
      region.imageExtent.height              = dynamic->height;
      region.imageExtent.depth               = 1;
      vkCmdCopyBufferToImage(
            cmd,
            staging->buffer,
            dynamic->image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &region);
      VULKAN_IMAGE_LAYOUT_TRANSITION(
            cmd,
            dynamic->image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
   }
   dynamic->layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}

/**
 * FORWARD DECLARATIONS
 */
static void vulkan_set_viewport(void *data, unsigned viewport_width,
      unsigned viewport_height, bool force_full, bool allow_rotate);
static bool vulkan_is_mapped_swapchain_texture_ptr(const vk_t* vk,
      const void* ptr);

#ifdef HAVE_OVERLAY
static void vulkan_overlay_free(vk_t *vk);
static void vulkan_render_overlay(vk_t *vk, unsigned width, unsigned height);
#endif
static void vulkan_viewport_info(void *data, struct video_viewport *vp);

/**
 * DISPLAY DRIVER
 */

/* Will do Y-flip later, but try to make it similar to GL. */
static const float vk_vertexes[8] = {
   0, 0,
   1, 0,
   0, 1,
   1, 1
};

static const float vk_tex_coords[8] = {
   0, 1,
   1, 1,
   0, 0,
   1, 0
};

static const float vk_colors[16] = {
   1.0f, 1.0f, 1.0f, 1.0f,
   1.0f, 1.0f, 1.0f, 1.0f,
   1.0f, 1.0f, 1.0f, 1.0f,
   1.0f, 1.0f, 1.0f, 1.0f,
};

static void *gfx_display_vk_get_default_mvp(void *data)
{
   vk_t *vk = (vk_t*)data;
   if (!vk)
      return NULL;
   return &vk->mvp_no_rot;
}

static const float *gfx_display_vk_get_default_vertices(void)
{
   return &vk_vertexes[0];
}

static const float *gfx_display_vk_get_default_tex_coords(void)
{
   return &vk_tex_coords[0];
}

#ifdef HAVE_SHADERPIPELINE
static unsigned to_menu_pipeline(
      enum gfx_display_prim_type type, unsigned pipeline)
{
   switch (pipeline)
   {
      case VIDEO_SHADER_MENU:
         return 6 + (type == GFX_DISPLAY_PRIM_TRIANGLESTRIP);
      case VIDEO_SHADER_MENU_2:
         return 8 + (type == GFX_DISPLAY_PRIM_TRIANGLESTRIP);
      case VIDEO_SHADER_MENU_3:
         return 10 + (type == GFX_DISPLAY_PRIM_TRIANGLESTRIP);
      case VIDEO_SHADER_MENU_4:
         return 12 + (type == GFX_DISPLAY_PRIM_TRIANGLESTRIP);
      case VIDEO_SHADER_MENU_5:
         return 14 + (type == GFX_DISPLAY_PRIM_TRIANGLESTRIP);
      default:
         break;
   }
   return 0;
}

static void gfx_display_vk_draw_pipeline(
      gfx_display_ctx_draw_t *draw,
      gfx_display_t *p_disp,
      void *data, unsigned video_width, unsigned video_height)
{
   static uint8_t ubo_scratch_data[768];
   static struct video_coords blank_coords;
   static float t                   = 0.0f;
   float output_size[2];
   float yflip                      = 1.0f;
   video_coord_array_t *ca          = NULL;
   vk_t *vk                         = (vk_t*)data;

   if (!vk || !draw)
      return;

   draw->x                          = 0;
   draw->y                          = 0;
   draw->matrix_data                = NULL;

   output_size[0]                   = (float)vk->context->swapchain_width;
   output_size[1]                   = (float)vk->context->swapchain_height;

   switch (draw->pipeline_id)
   {
      /* Ribbon */
      default:
      case VIDEO_SHADER_MENU:
      case VIDEO_SHADER_MENU_2:
         ca                               = &p_disp->dispca;
         draw->coords                     = (struct video_coords*)&ca->coords;
         draw->backend_data               = ubo_scratch_data;
         draw->backend_data_size          = 2 * sizeof(float);

         /* Match UBO layout in shader. */
         memcpy(ubo_scratch_data, &t, sizeof(t));
         memcpy(ubo_scratch_data + sizeof(float), &yflip, sizeof(yflip));
         break;

      /* Snow simple */
      case VIDEO_SHADER_MENU_3:
      case VIDEO_SHADER_MENU_4:
      case VIDEO_SHADER_MENU_5:
         draw->backend_data               = ubo_scratch_data;
         draw->backend_data_size          = sizeof(math_matrix_4x4)
            + 4 * sizeof(float);

         /* Match UBO layout in shader. */
         memcpy(ubo_scratch_data,
               &vk->mvp_no_rot,
               sizeof(math_matrix_4x4));
         memcpy(ubo_scratch_data + sizeof(math_matrix_4x4),
               output_size,
               sizeof(output_size));

         /* Shader uses FragCoord, need to fix up. */
         if (draw->pipeline_id == VIDEO_SHADER_MENU_5)
            yflip = -1.0f;

         memcpy(ubo_scratch_data + sizeof(math_matrix_4x4)
               + 2 * sizeof(float), &t, sizeof(t));
         memcpy(ubo_scratch_data + sizeof(math_matrix_4x4)
               + 3 * sizeof(float), &yflip, sizeof(yflip));
         draw->coords          = &blank_coords;
         blank_coords.vertices = 4;
         draw->prim_type       = GFX_DISPLAY_PRIM_TRIANGLESTRIP;
         break;
   }

   t += 0.01;
}
#endif

static void gfx_display_vk_draw(gfx_display_ctx_draw_t *draw,
      void *data, unsigned video_width, unsigned video_height)
{
   unsigned i;
   struct vk_buffer_range range;
   struct vk_texture *texture    = NULL;
   const float *vertex           = NULL;
   const float *tex_coord        = NULL;
   const float *color            = NULL;
   struct vk_vertex *pv          = NULL;
   vk_t *vk                      = (vk_t*)data;

   if (!vk || !draw)
      return;

   texture                        = (struct vk_texture*)draw->texture;
   vertex                         = draw->coords->vertex;
   tex_coord                      = draw->coords->tex_coord;
   color                          = draw->coords->color;

   if (!vertex)
      vertex                      = &vk_vertexes[0];
   if (!tex_coord)
      tex_coord                   = &vk_tex_coords[0];
   if (!draw->coords->lut_tex_coord)
      draw->coords->lut_tex_coord = &vk_tex_coords[0];
   if (!texture)
      texture                     = &vk->display.blank_texture;
   if (!color)
      color                       = &vk_colors[0];

   vk->vk_vp.x                    = draw->x;
   vk->vk_vp.y                    = vk->context->swapchain_height - draw->y - draw->height;
   vk->vk_vp.width                = draw->width;
   vk->vk_vp.height               = draw->height;
   vk->vk_vp.minDepth             = 0.0f;
   vk->vk_vp.maxDepth             = 1.0f;

   vk->tracker.dirty             |= VULKAN_DIRTY_DYNAMIC_BIT;

   /* Bake interleaved VBO. Kinda ugly, we should probably try to move to
    * an interleaved model to begin with ... */
   if (!vulkan_buffer_chain_alloc(vk->context, &vk->chain->vbo,
            draw->coords->vertices * sizeof(struct vk_vertex), &range))
      return;

   pv = (struct vk_vertex*)range.data;
   for (i = 0; i < draw->coords->vertices; i++, pv++)
   {
      pv->x       = *vertex++;
      /* Y-flip. Vulkan is top-left clip space */
      pv->y       = 1.0f - (*vertex++);
      pv->tex_x   = *tex_coord++;
      pv->tex_y   = *tex_coord++;
      pv->color.r = *color++;
      pv->color.g = *color++;
      pv->color.b = *color++;
      pv->color.a = *color++;
   }

   switch (draw->pipeline_id)
   {
#ifdef HAVE_SHADERPIPELINE
      case VIDEO_SHADER_MENU:
      case VIDEO_SHADER_MENU_2:
      case VIDEO_SHADER_MENU_3:
      case VIDEO_SHADER_MENU_4:
      case VIDEO_SHADER_MENU_5:
         {
            struct vk_draw_triangles call;

            call.pipeline     = vk->display.pipelines[
               to_menu_pipeline(draw->prim_type, draw->pipeline_id)];
            call.texture      = NULL;
            call.sampler      = VK_NULL_HANDLE;
            call.uniform      = draw->backend_data;
            call.uniform_size = draw->backend_data_size;
            call.vbo          = &range;
            call.vertices     = draw->coords->vertices;

            vulkan_draw_triangles(vk, &call);
         }
         break;
#endif

      default:
         {
            struct vk_draw_triangles call;
            unsigned
               disp_pipeline  =
                 ((draw->prim_type == GFX_DISPLAY_PRIM_TRIANGLESTRIP) << 1)
               | (((vk->flags & VK_FLAG_DISPLAY_BLEND) > 0) << 0);
            call.pipeline     = vk->display.pipelines[disp_pipeline];
            call.texture      = texture;
            call.sampler      = (texture->flags & VK_TEX_FLAG_MIPMAP)
               ? vk->samplers.mipmap_linear
               : ((texture->flags & VK_TEX_FLAG_DEFAULT_SMOOTH)
               ? vk->samplers.linear
               : vk->samplers.nearest);
            call.uniform      = draw->matrix_data
               ? draw->matrix_data : &vk->mvp_no_rot;
            call.uniform_size = sizeof(math_matrix_4x4);
            call.vbo          = &range;
            call.vertices     = draw->coords->vertices;

            vulkan_draw_triangles(vk, &call);
         }
         break;
   }
}

static void gfx_display_vk_blend_begin(void *data)
{
   vk_t *vk = (vk_t*)data;

   if (vk)
      vk->flags |=  VK_FLAG_DISPLAY_BLEND;
}

static void gfx_display_vk_blend_end(void *data)
{
   vk_t *vk = (vk_t*)data;

   if (vk)
      vk->flags &= ~VK_FLAG_DISPLAY_BLEND;
}

static void gfx_display_vk_scissor_begin(
      void *data,
      unsigned video_width,
      unsigned video_height,
      int x, int y, unsigned width, unsigned height)
{
   vk_t *vk                          = (vk_t*)data;

   vk->tracker.scissor.offset.x      = x;
   vk->tracker.scissor.offset.y      = y;
   vk->tracker.scissor.extent.width  = width;
   vk->tracker.scissor.extent.height = height;
   vk->flags                        |= VK_FLAG_TRACKER_USE_SCISSOR;
   vk->tracker.dirty                |= VULKAN_DIRTY_DYNAMIC_BIT;
}

static void gfx_display_vk_scissor_end(void *data,
      unsigned video_width,
      unsigned video_height)
{
   vk_t *vk                 = (vk_t*)data;

   vk->flags               &= ~VK_FLAG_TRACKER_USE_SCISSOR;
   vk->tracker.dirty       |=  VULKAN_DIRTY_DYNAMIC_BIT;
}

gfx_display_ctx_driver_t gfx_display_ctx_vulkan = {
   gfx_display_vk_draw,
#ifdef HAVE_SHADERPIPELINE
   gfx_display_vk_draw_pipeline,
#else
   NULL,                                  /* draw_pipeline */
#endif
   gfx_display_vk_blend_begin,
   gfx_display_vk_blend_end,
   gfx_display_vk_get_default_mvp,
   gfx_display_vk_get_default_vertices,
   gfx_display_vk_get_default_tex_coords,
   FONT_DRIVER_RENDER_VULKAN_API,
   GFX_VIDEO_DRIVER_VULKAN,
   "vulkan",
   false,
   gfx_display_vk_scissor_begin,
   gfx_display_vk_scissor_end
};

/**
 * FONT DRIVER
 */

static INLINE void vulkan_font_update_glyph(
      vulkan_raster_t *font, const struct font_glyph *glyph)
{
   unsigned row;
   for (row = glyph->atlas_offset_y; row < (glyph->atlas_offset_y + glyph->height); row++)
   {
      uint8_t *src = font->atlas->buffer + row * font->atlas->width + glyph->atlas_offset_x;
      uint8_t *dst = (uint8_t*)font->texture.mapped + row * font->texture.stride + glyph->atlas_offset_x;
      memcpy(dst, src, glyph->width);
   }
}

static void vulkan_font_free(void *data, bool is_threaded)
{
   vulkan_raster_t *font = (vulkan_raster_t*)data;
   if (!font)
      return;

   if (font->font_driver && font->font_data)
      font->font_driver->free(font->font_data);

   vkQueueWaitIdle(font->vk->context->queue);
   vulkan_destroy_texture(
         font->vk->context->device, &font->texture);
   vulkan_destroy_texture(
         font->vk->context->device, &font->texture_optimal);

   free(font);
}

static void *vulkan_font_init(void *data,
      const char *font_path, float font_size,
      bool is_threaded)
{
   vulkan_raster_t *font          =
      (vulkan_raster_t*)calloc(1, sizeof(*font));

   if (!font)
      return NULL;

   font->vk = (vk_t*)data;

   if (!font_renderer_create_default(
            &font->font_driver,
            &font->font_data, font_path, font_size))
   {
      free(font);
      return NULL;
   }

   font->atlas   = font->font_driver->get_atlas(font->font_data);
   font->texture = vulkan_create_texture(font->vk, NULL,
         font->atlas->width, font->atlas->height, VK_FORMAT_R8_UNORM, font->atlas->buffer,
         NULL, VULKAN_TEXTURE_STAGING);

   {
      struct vk_texture *texture = &font->texture;
      VK_MAP_PERSISTENT_TEXTURE(font->vk->context->device, texture);
   }

   font->texture_optimal = vulkan_create_texture(font->vk, NULL,
         font->atlas->width, font->atlas->height, VK_FORMAT_R8_UNORM, NULL,
         NULL, VULKAN_TEXTURE_DYNAMIC);

   font->needs_update = true;

   return font;
}

static int vulkan_get_message_width(void *data, const char *msg,
      size_t msg_len, float scale)
{
   const struct font_glyph* glyph_q = NULL;
   vulkan_raster_t *font = (vulkan_raster_t*)data;
   const char* msg_end   = msg + msg_len;
   int delta_x           = 0;

   if (     !font
         || !font->font_driver
         || !font->font_data )
      return 0;

   glyph_q = font->font_driver->get_glyph(font->font_data, '?');

   while (msg < msg_end)
   {
      const struct font_glyph *glyph;
      uint32_t code                  = utf8_walk(&msg);

      /* Do something smarter here ... */
      if (!(glyph = font->font_driver->get_glyph(
                  font->font_data, code)))
         if (!(glyph = glyph_q))
            continue;

      if (font->atlas->dirty)
      {
         vulkan_font_update_glyph(font, glyph);
         font->atlas->dirty = false;
         font->needs_update = true;
      }
      delta_x += glyph->advance_x;
   }

   return delta_x * scale;
}

static void vulkan_font_render_line(vk_t *vk,
      vulkan_raster_t *font,
      const struct font_glyph* glyph_q,
      const char *msg, size_t msg_len,
      float scale,
      const float color[4],
      float pos_x,
      float pos_y,
      int pre_x,
      float inv_tex_size_x,
      float inv_tex_size_y,
      float inv_win_width,
      float inv_win_height,
      unsigned text_align)
{
   struct vk_color vk_color;
   const char* msg_end              = msg + msg_len;
   int x                            = pre_x;
   int y                            = roundf((1.0f - pos_y) * vk->vp.height);
   int delta_x                      = 0;
   int delta_y                      = 0;

   vk_color.r                       = color[0];
   vk_color.g                       = color[1];
   vk_color.b                       = color[2];
   vk_color.a                       = color[3];

   switch (text_align)
   {
      case TEXT_ALIGN_RIGHT:
         x -= vulkan_get_message_width(font, msg, msg_len, scale);
         break;
      case TEXT_ALIGN_CENTER:
         x -= vulkan_get_message_width(font, msg, msg_len, scale) / 2;
         break;
   }

   while (msg < msg_end)
   {
      const struct font_glyph *glyph;
      int off_x, off_y, tex_x, tex_y, width, height;
      unsigned code = utf8_walk(&msg);

      /* Do something smarter here ... */
      if (!(glyph =
               font->font_driver->get_glyph(font->font_data, code)))
         if (!(glyph = glyph_q))
            continue;

      if (font->atlas->dirty)
      {
         vulkan_font_update_glyph(font, glyph);
         font->atlas->dirty = false;
         font->needs_update = true;
      }

      off_x  = glyph->draw_offset_x;
      off_y  = glyph->draw_offset_y;
      tex_x  = glyph->atlas_offset_x;
      tex_y  = glyph->atlas_offset_y;
      width  = glyph->width;
      height = glyph->height;

      {
         struct vk_vertex *pv          = font->pv + font->vertices;
         float _x                      = (x + (off_x + delta_x) * scale)
            * inv_win_width;
         float _y                      = (y + (off_y + delta_y) * scale)
            * inv_win_height;
         float _width                  = width  * scale * inv_win_width;
         float _height                 = height * scale * inv_win_height;
         float _tex_x                  = tex_x * inv_tex_size_x;
         float _tex_y                  = tex_y * inv_tex_size_y;
         float _tex_width              = width * inv_tex_size_x;
         float _tex_height             = height * inv_tex_size_y;
         const struct vk_color *_color = &vk_color;

         VULKAN_WRITE_QUAD_VBO(pv, _x, _y, _width, _height,
               _tex_x, _tex_y, _tex_width, _tex_height, _color);
      }

      font->vertices += 6;

      delta_x        += glyph->advance_x;
      delta_y        += glyph->advance_y;
   }
}

static void vulkan_font_render_message(vk_t *vk,
      vulkan_raster_t *font, const char *msg, float scale,
      const float color[4], float pos_x, float pos_y,
      unsigned text_align)
{
   float line_height;
   struct font_line_metrics *line_metrics = NULL;
   const struct font_glyph* glyph_q       = font->font_driver->get_glyph(font->font_data, '?');
   int x                                  = roundf(pos_x * vk->vp.width);
   int lines                              = 0;
   float inv_tex_size_x                   = 1.0f / font->texture.width;
   float inv_tex_size_y                   = 1.0f / font->texture.height;
   float inv_win_width                    = 1.0f / vk->vp.width;
   float inv_win_height                   = 1.0f / vk->vp.height;
   font->font_driver->get_line_metrics(font->font_data, &line_metrics);
   line_height = line_metrics->height * scale / vk->vp.height;

   for (;;)
   {
      const char *delim = strchr(msg, '\n');
      size_t msg_len    = delim ? (size_t)(delim - msg) : strlen(msg);

      /* Draw the line */
      vulkan_font_render_line(vk, font, glyph_q, msg, msg_len,
            scale, color,
            pos_x,
            pos_y - (float)lines * line_height,
            x,
            inv_tex_size_x,
            inv_tex_size_y,
            inv_win_width,
            inv_win_height,
            text_align);

      if (!delim)
         break;

      msg += msg_len + 1;
      lines++;
   }
}

static void vulkan_font_flush(vk_t *vk, vulkan_raster_t *font)
{
   struct vk_draw_triangles call;

   call.pipeline     = vk->pipelines.font;
   call.texture      = &font->texture_optimal;
   call.sampler      = vk->samplers.mipmap_linear;
   call.uniform      = &vk->mvp;
   call.uniform_size = sizeof(vk->mvp);
   call.vbo          = &font->range;
   call.vertices     = font->vertices;

   if (font->needs_update)
   {
      VkCommandBuffer staging;
      VkSubmitInfo submit_info;
      VkCommandBufferAllocateInfo cmd_info;
      VkCommandBufferBeginInfo begin_info;
      struct vk_texture *dynamic_tex  = NULL;
      struct vk_texture *staging_tex  = NULL;

      cmd_info.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
      cmd_info.pNext              = NULL;
      cmd_info.commandPool        = vk->staging_pool;
      cmd_info.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
      cmd_info.commandBufferCount = 1;
      vkAllocateCommandBuffers(vk->context->device, &cmd_info, &staging);

      begin_info.sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
      begin_info.pNext            = NULL;
      begin_info.flags            = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
      begin_info.pInheritanceInfo = NULL;
      vkBeginCommandBuffer(staging, &begin_info);

      VULKAN_SYNC_TEXTURE_TO_GPU_COND_OBJ(vk, font->texture);

      dynamic_tex                 = &font->texture_optimal;
      staging_tex                 = &font->texture;

      vulkan_copy_staging_to_dynamic(vk, staging,
            dynamic_tex, staging_tex);

      vkEndCommandBuffer(staging);

#ifdef HAVE_THREADS
      slock_lock(vk->context->queue_lock);
#endif

      submit_info.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
      submit_info.pNext                = NULL;
      submit_info.waitSemaphoreCount   = 0;
      submit_info.pWaitSemaphores      = NULL;
      submit_info.pWaitDstStageMask    = NULL;
      submit_info.commandBufferCount   = 1;
      submit_info.pCommandBuffers      = &staging;
      submit_info.signalSemaphoreCount = 0;
      submit_info.pSignalSemaphores    = NULL;
      vkQueueSubmit(vk->context->queue,
            1, &submit_info, VK_NULL_HANDLE);

      vkQueueWaitIdle(vk->context->queue);

#ifdef HAVE_THREADS
      slock_unlock(vk->context->queue_lock);
#endif

      vkFreeCommandBuffers(vk->context->device,
            vk->staging_pool, 1, &staging);

      font->needs_update = false;
   }

   vulkan_draw_triangles(vk, &call);
}

static void vulkan_font_render_msg(
      void *userdata,
      void *data,
      const char *msg,
      const struct font_params *params)
{
   float color[4];
   int drop_x, drop_y;
   bool full_screen;
   size_t max_glyphs;
   unsigned width, height;
   enum text_alignment text_align;
   float x, y, scale, drop_mod, drop_alpha;
   vulkan_raster_t *font            = (vulkan_raster_t*)data;
   settings_t *settings             = config_get_ptr();
   float video_msg_pos_x            = settings->floats.video_msg_pos_x;
   float video_msg_pos_y            = settings->floats.video_msg_pos_y;
   float video_msg_color_r          = settings->floats.video_msg_color_r;
   float video_msg_color_g          = settings->floats.video_msg_color_g;
   float video_msg_color_b          = settings->floats.video_msg_color_b;
   vk_t *vk                         = (vk_t*)userdata;

   if (!font || !msg || !*msg || !vk)
      return;

   width          = vk->video_width;
   height         = vk->video_height;

   if (params)
   {
      x           = params->x;
      y           = params->y;
      scale       = params->scale;
      full_screen = params->full_screen;
      text_align  = params->text_align;
      drop_x      = params->drop_x;
      drop_y      = params->drop_y;
      drop_mod    = params->drop_mod;
      drop_alpha  = params->drop_alpha;

      color[0]    = FONT_COLOR_GET_RED(params->color)   / 255.0f;
      color[1]    = FONT_COLOR_GET_GREEN(params->color) / 255.0f;
      color[2]    = FONT_COLOR_GET_BLUE(params->color)  / 255.0f;
      color[3]    = FONT_COLOR_GET_ALPHA(params->color) / 255.0f;

      /* If alpha is 0.0f, turn it into default 1.0f */
      if (color[3] <= 0.0f)
         color[3] = 1.0f;
   }
   else
   {
      x           = video_msg_pos_x;
      y           = video_msg_pos_y;
      scale       = 1.0f;
      full_screen = true;
      text_align  = TEXT_ALIGN_LEFT;
      drop_x      = -2;
      drop_y      = -2;
      drop_mod    = 0.3f;
      drop_alpha  = 1.0f;

      color[0]    = video_msg_color_r;
      color[1]    = video_msg_color_g;
      color[2]    = video_msg_color_b;
      color[3]    = 1.0f;
   }

   vulkan_set_viewport(vk, width, height, full_screen, false);

   max_glyphs = strlen(msg);
   if (drop_x || drop_y)
      max_glyphs *= 2;

   if (!vulkan_buffer_chain_alloc(vk->context, &vk->chain->vbo,
         6 * sizeof(struct vk_vertex) * max_glyphs, &font->range))
      return;

   font->vertices   = 0;
   font->pv         = (struct vk_vertex*)font->range.data;

   if (drop_x || drop_y)
   {
      float color_dark[4];
      color_dark[0] = color[0] * drop_mod;
      color_dark[1] = color[1] * drop_mod;
      color_dark[2] = color[2] * drop_mod;
      color_dark[3] = color[3] * drop_alpha;

      vulkan_font_render_message(vk, font, msg, scale, color_dark,
            x + scale * drop_x / vk->vp.width, y +
            scale * drop_y / vk->vp.height, text_align);
   }

   vulkan_font_render_message(vk, font, msg, scale,
         color, x, y, text_align);
   vulkan_font_flush(vk, font);
}

static const struct font_glyph *vulkan_font_get_glyph(
      void *data, uint32_t code)
{
   const struct font_glyph* glyph;
   vulkan_raster_t *font = (vulkan_raster_t*)data;

   if (!font || !font->font_driver)
      return NULL;

   glyph = font->font_driver->get_glyph((void*)font->font_driver, code);

   if (glyph && font->atlas->dirty)
   {
      vulkan_font_update_glyph(font, glyph);
      font->atlas->dirty = false;
      font->needs_update = true;
   }
   return glyph;
}

static bool vulkan_get_line_metrics(void* data,
      struct font_line_metrics **metrics)
{
   vulkan_raster_t *font = (vulkan_raster_t*)data;
   if (font && font->font_driver && font->font_data)
   {
      font->font_driver->get_line_metrics(font->font_data, metrics);
      return true;
   }
   return false;
}

font_renderer_t vulkan_raster_font = {
   vulkan_font_init,
   vulkan_font_free,
   vulkan_font_render_msg,
   "vulkan",
   vulkan_font_get_glyph,
   NULL,                            /* bind_block */
   NULL,                            /* flush_block */
   vulkan_get_message_width,
   vulkan_get_line_metrics
};

/*
 * VIDEO DRIVER
 */

static struct vk_descriptor_manager vulkan_create_descriptor_manager(
      VkDevice device,
      const VkDescriptorPoolSize *sizes,
      unsigned num_sizes,
      VkDescriptorSetLayout set_layout)
{
   int i;
   struct vk_descriptor_manager manager;

   manager.current    = NULL;
   manager.count      = 0;

   for (i = 0; i < VULKAN_MAX_DESCRIPTOR_POOL_SIZES; i++)
   {
      manager.sizes[i].type            = VK_DESCRIPTOR_TYPE_SAMPLER;
      manager.sizes[i].descriptorCount = 0;
   }
   memcpy(manager.sizes, sizes, num_sizes * sizeof(*sizes));
   manager.set_layout = set_layout;
   manager.num_sizes  = num_sizes;

   manager.head       = vulkan_alloc_descriptor_pool(device, &manager);
   return manager;
}

static void vulkan_destroy_descriptor_manager(
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

static struct vk_buffer_chain vulkan_buffer_chain_init(
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

static const gfx_ctx_driver_t *gfx_ctx_vk_drivers[] = {
#if defined(__APPLE__)
   &gfx_ctx_cocoavk,
#endif
#if defined(_WIN32) && !defined(__WINRT__)
   &gfx_ctx_w_vk,
#endif
#if defined(ANDROID)
   &gfx_ctx_vk_android,
#endif
#if defined(HAVE_WAYLAND)
   &gfx_ctx_vk_wayland,
#endif
#if defined(HAVE_X11)
   &gfx_ctx_vk_x,
#endif
#if defined(HAVE_VULKAN_DISPLAY)
   &gfx_ctx_khr_display,
#endif
   &gfx_ctx_null,
   NULL
};

static const gfx_ctx_driver_t *vk_context_driver_init_first(
      uint32_t runloop_flags,
      settings_t *settings,
      void *data,
      const char *ident, enum gfx_ctx_api api, unsigned major,
      unsigned minor, bool hw_render_ctx, void **ctx_data)
{
   unsigned j;
   int i = -1;
   video_driver_state_t *video_st = video_state_get_ptr();

   for (j = 0; gfx_ctx_vk_drivers[j]; j++)
   {
      if (string_is_equal_noncase(ident, gfx_ctx_vk_drivers[j]->ident))
      {
         i = j;
         break;
      }
   }

   if (i >= 0)
   {
      const gfx_ctx_driver_t *ctx = video_context_driver_init(
            (runloop_flags & RUNLOOP_FLAG_CORE_SET_SHARED_CONTEXT) ? true : false,
            settings,
            data,
            gfx_ctx_vk_drivers[i], ident,
            api, major, minor, hw_render_ctx, ctx_data);
      if (ctx)
      {
         video_st->context_data = *ctx_data;
         return ctx;
      }
   }

   for (i = 0; gfx_ctx_vk_drivers[i]; i++)
   {
      const gfx_ctx_driver_t *ctx =
         video_context_driver_init(
               (runloop_flags & RUNLOOP_FLAG_CORE_SET_SHARED_CONTEXT) ? true : false,
               settings,
               data,
               gfx_ctx_vk_drivers[i], ident,
               api, major, minor, hw_render_ctx, ctx_data);

      if (ctx)
      {
         video_st->context_data = *ctx_data;
         return ctx;
      }
   }

   return NULL;
}

static const gfx_ctx_driver_t *vulkan_get_context(vk_t *vk, settings_t *settings)
{
   void                 *ctx_data  = NULL;
   unsigned major                  = 1;
   unsigned minor                  = 0;
   enum gfx_ctx_api api            = GFX_CTX_VULKAN_API;
   uint32_t runloop_flags          = runloop_get_flags();
   const gfx_ctx_driver_t *gfx_ctx = vk_context_driver_init_first(
         runloop_flags, settings,
         vk, settings->arrays.video_context_driver, api, major, minor, false, &ctx_data);

   if (ctx_data)
      vk->ctx_data                 = ctx_data;
   return gfx_ctx;
}

static void vulkan_init_render_pass(
      vk_t *vk)
{
   VkRenderPassCreateInfo rp_info;
   VkAttachmentReference color_ref;
   VkAttachmentDescription attachment;
   VkSubpassDescription subpass;

   attachment.flags             = 0;
   /* Backbuffer format. */
   attachment.format            = vk->context->swapchain_format;
   /* Not multisampled. */
   attachment.samples           = VK_SAMPLE_COUNT_1_BIT;
   /* When starting the frame, we want tiles to be cleared. */
   attachment.loadOp            = VK_ATTACHMENT_LOAD_OP_CLEAR;
   /* When end the frame, we want tiles to be written out. */
   attachment.storeOp           = VK_ATTACHMENT_STORE_OP_STORE;
   /* Don't care about stencil since we're not using it. */
   attachment.stencilLoadOp     = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
   attachment.stencilStoreOp    = VK_ATTACHMENT_STORE_OP_DONT_CARE;

   /* We don't care about the initial layout as we'll overwrite contents anyway */
   attachment.initialLayout     = VK_IMAGE_LAYOUT_UNDEFINED;
   /* After we're done rendering, automatically transition the image to attachment_optimal */
   attachment.finalLayout       = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

   /* Color attachment reference */
   color_ref.attachment         = 0;
   color_ref.layout             = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

   /* We have one subpass.
    * This subpass has 1 color attachment. */
   subpass.flags                    = 0;
   subpass.pipelineBindPoint        = VK_PIPELINE_BIND_POINT_GRAPHICS;
   subpass.inputAttachmentCount     = 0;
   subpass.pInputAttachments        = NULL;
   subpass.colorAttachmentCount     = 1;
   subpass.pColorAttachments        = &color_ref;
   subpass.pResolveAttachments      = NULL;
   subpass.pDepthStencilAttachment  = NULL;
   subpass.preserveAttachmentCount  = 0;
   subpass.pPreserveAttachments     = NULL;

   /* Finally, create the renderpass. */
   rp_info.sType                =
      VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
   rp_info.pNext                = NULL;
   rp_info.flags                = 0;
   rp_info.attachmentCount      = 1;
   rp_info.pAttachments         = &attachment;
   rp_info.subpassCount         = 1;
   rp_info.pSubpasses           = &subpass;
   rp_info.dependencyCount      = 0;
   rp_info.pDependencies        = NULL;

   vkCreateRenderPass(vk->context->device,
         &rp_info, NULL, &vk->render_pass);
}


static void vulkan_init_hdr_readback_render_pass(vk_t *vk)
{
   VkRenderPassCreateInfo rp_info;
   VkAttachmentReference color_ref;
   VkAttachmentDescription attachment;
   VkSubpassDescription subpass;

   attachment.flags             = 0;
   /* Use BGRA as backbuffer format so CPU can just memcpy transfer results */
   attachment.format            = VK_FORMAT_B8G8R8A8_UNORM;
   /* Not multisampled. */
   attachment.samples           = VK_SAMPLE_COUNT_1_BIT;
   /* When starting the frame, we want tiles to be cleared. */
   attachment.loadOp            = VK_ATTACHMENT_LOAD_OP_CLEAR;
   /* When end the frame, we want tiles to be written out. */
   attachment.storeOp           = VK_ATTACHMENT_STORE_OP_STORE;
   /* Don't care about stencil since we're not using it. */
   attachment.stencilLoadOp     = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
   attachment.stencilStoreOp    = VK_ATTACHMENT_STORE_OP_DONT_CARE;

   /* We don't care about the initial layout as we'll overwrite contents anyway */
   attachment.initialLayout     = VK_IMAGE_LAYOUT_UNDEFINED;
   /* After we're done rendering, automatically transition the image as a source for transfers */
   attachment.finalLayout       = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

   /* Color attachment reference */
   color_ref.attachment         = 0;
   color_ref.layout             = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

   /* We have one subpass.
    * This subpass has 1 color attachment. */
   subpass.flags                    = 0;
   subpass.pipelineBindPoint        = VK_PIPELINE_BIND_POINT_GRAPHICS;
   subpass.inputAttachmentCount     = 0;
   subpass.pInputAttachments        = NULL;
   subpass.colorAttachmentCount     = 1;
   subpass.pColorAttachments        = &color_ref;
   subpass.pResolveAttachments      = NULL;
   subpass.pDepthStencilAttachment  = NULL;
   subpass.preserveAttachmentCount  = 0;
   subpass.pPreserveAttachments     = NULL;

   /* Finally, create the renderpass. */
   rp_info.sType                =
      VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
   rp_info.pNext                = NULL;
   rp_info.flags                = 0;
   rp_info.attachmentCount      = 1;
   rp_info.pAttachments         = &attachment;
   rp_info.subpassCount         = 1;
   rp_info.pSubpasses           = &subpass;
   rp_info.dependencyCount      = 0;
   rp_info.pDependencies        = NULL;

   vkCreateRenderPass(vk->context->device,
         &rp_info, NULL, &vk->readback_render_pass);
}

static void vulkan_init_framebuffers(
      vk_t *vk)
{
   int i;

   for (i = 0; i < (int) vk->num_swapchain_images; i++)
   {
      VkImageViewCreateInfo view =
      { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
      VkFramebufferCreateInfo info =
      { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };

      vk->backbuffers[i].image = vk->context->swapchain_images[i];

      if (vk->context->swapchain_images[i] == VK_NULL_HANDLE)
      {
         vk->backbuffers[i].view        = VK_NULL_HANDLE;
         vk->backbuffers[i].framebuffer = VK_NULL_HANDLE;
         continue;
      }

      /* Create an image view which we can render into. */
      view.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
      view.format                          = vk->context->swapchain_format;
      view.image                           = vk->backbuffers[i].image;
      view.subresourceRange.baseMipLevel   = 0;
      view.subresourceRange.baseArrayLayer = 0;
      view.subresourceRange.levelCount     = 1;
      view.subresourceRange.layerCount     = 1;
      view.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
      view.components.r                    = VK_COMPONENT_SWIZZLE_R;
      view.components.g                    = VK_COMPONENT_SWIZZLE_G;
      view.components.b                    = VK_COMPONENT_SWIZZLE_B;
      view.components.a                    = VK_COMPONENT_SWIZZLE_A;

      vkCreateImageView(vk->context->device,
            &view, NULL, &vk->backbuffers[i].view);

      /* Create the framebuffer */
      info.renderPass      = vk->render_pass;
      info.attachmentCount = 1;
      info.pAttachments    = &vk->backbuffers[i].view;
      info.width           = vk->context->swapchain_width;
      info.height          = vk->context->swapchain_height;
      info.layers          = 1;

      vkCreateFramebuffer(vk->context->device,
            &info, NULL, &vk->backbuffers[i].framebuffer);
   }
}

static void vulkan_init_pipeline_layout(
      vk_t *vk)
{
   VkPipelineLayoutCreateInfo layout_info;
   VkDescriptorSetLayoutCreateInfo set_layout_info;
   VkDescriptorSetLayoutBinding bindings[5];

   bindings[0].binding            = 0;
   bindings[0].descriptorType     = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
   bindings[0].descriptorCount    = 1;
   bindings[0].stageFlags         = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT |
                                    VK_SHADER_STAGE_COMPUTE_BIT;
   bindings[0].pImmutableSamplers = NULL;

   bindings[1].binding            = 1;
   bindings[1].descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
   bindings[1].descriptorCount    = 1;
   bindings[1].stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;
   bindings[1].pImmutableSamplers = NULL;

   bindings[2].binding            = 2;
   bindings[2].descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
   bindings[2].descriptorCount    = 1;
   bindings[2].stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;
   bindings[2].pImmutableSamplers = NULL;

   bindings[3].binding            = 3;
   bindings[3].descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
   bindings[3].descriptorCount    = 1;
   bindings[3].stageFlags         = VK_SHADER_STAGE_COMPUTE_BIT;
   bindings[3].pImmutableSamplers = NULL;

   bindings[4].binding            = 4;
   bindings[4].descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
   bindings[4].descriptorCount    = 1;
   bindings[4].stageFlags         = VK_SHADER_STAGE_COMPUTE_BIT;
   bindings[4].pImmutableSamplers = NULL;

   set_layout_info.sType          =
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
   set_layout_info.pNext          = NULL;
   set_layout_info.flags          = 0;
   set_layout_info.bindingCount   = 5;
   set_layout_info.pBindings      = bindings;

   vkCreateDescriptorSetLayout(vk->context->device,
         &set_layout_info, NULL, &vk->pipelines.set_layout);

   layout_info.sType                  =
      VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
   layout_info.pNext                  = NULL;
   layout_info.flags                  = 0;
   layout_info.setLayoutCount         = 1;
   layout_info.pSetLayouts            = &vk->pipelines.set_layout;
   layout_info.pushConstantRangeCount = 0;
   layout_info.pPushConstantRanges    = NULL;

   vkCreatePipelineLayout(vk->context->device,
         &layout_info, NULL, &vk->pipelines.layout);
}

static void vulkan_init_pipelines(vk_t *vk)
{
#ifdef VULKAN_HDR_SWAPCHAIN
   static const uint32_t hdr_frag[] =
#include "vulkan_shaders/hdr.frag.inc"
      ;
   static const uint32_t hdr_tonemap_frag[] =
#include "vulkan_shaders/hdr_tonemap.frag.inc"
      ;
#endif /* VULKAN_HDR_SWAPCHAIN */

   static const uint32_t alpha_blend_vert[] =
#include "vulkan_shaders/alpha_blend.vert.inc"
      ;

   static const uint32_t alpha_blend_frag[] =
#include "vulkan_shaders/alpha_blend.frag.inc"
      ;

   static const uint32_t font_frag[] =
#include "vulkan_shaders/font.frag.inc"
      ;

   static const uint32_t rgb565_to_rgba8888_comp[] =
#include "vulkan_shaders/rgb565_to_rgba8888.comp.inc"
   ;

   static const uint32_t pipeline_ribbon_vert[] =
#include "vulkan_shaders/pipeline_ribbon.vert.inc"
      ;

   static const uint32_t pipeline_ribbon_frag[] =
#include "vulkan_shaders/pipeline_ribbon.frag.inc"
      ;

   static const uint32_t pipeline_ribbon_simple_vert[] =
#include "vulkan_shaders/pipeline_ribbon_simple.vert.inc"
      ;

   static const uint32_t pipeline_ribbon_simple_frag[] =
#include "vulkan_shaders/pipeline_ribbon_simple.frag.inc"
      ;

   static const uint32_t pipeline_snow_simple_frag[] =
#include "vulkan_shaders/pipeline_snow_simple.frag.inc"
      ;

   static const uint32_t pipeline_snow_frag[] =
#include "vulkan_shaders/pipeline_snow.frag.inc"
      ;

   static const uint32_t pipeline_bokeh_frag[] =
#include "vulkan_shaders/pipeline_bokeh.frag.inc"
      ;

   int i;
   VkPipelineMultisampleStateCreateInfo multisample;
   VkPipelineInputAssemblyStateCreateInfo input_assembly = {
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
   VkPipelineVertexInputStateCreateInfo vertex_input     = {
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
   VkPipelineRasterizationStateCreateInfo raster         = {
      VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
   VkPipelineColorBlendAttachmentState blend_attachment  = {0};
   VkPipelineColorBlendStateCreateInfo blend             = {
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
   VkPipelineViewportStateCreateInfo viewport            = {
      VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
   VkPipelineDepthStencilStateCreateInfo depth_stencil   = {
      VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
   VkPipelineDynamicStateCreateInfo dynamic              = {
      VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };

   VkPipelineShaderStageCreateInfo shader_stages[2]      = {
      { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO },
      { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO },
   };

   VkGraphicsPipelineCreateInfo pipe                     = {
      VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
   VkComputePipelineCreateInfo cpipe                     = {
      VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
   VkShaderModuleCreateInfo module_info                  = {
      VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
   VkVertexInputAttributeDescription attributes[3]       = {{0}};
   VkVertexInputBindingDescription binding               = {0};

   static const VkDynamicState dynamics[]                = {
      VK_DYNAMIC_STATE_VIEWPORT,
      VK_DYNAMIC_STATE_SCISSOR,
   };

   vulkan_init_pipeline_layout(vk);

   /* Input assembly */
   input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

   /* VAO state */
   attributes[0].location  = 0;
   attributes[0].binding   = 0;
   attributes[0].format    = VK_FORMAT_R32G32_SFLOAT;
   attributes[0].offset    = 0;
   attributes[1].location  = 1;
   attributes[1].binding   = 0;
   attributes[1].format    = VK_FORMAT_R32G32_SFLOAT;
   attributes[1].offset    = 2 * sizeof(float);
   attributes[2].location  = 2;
   attributes[2].binding   = 0;
   attributes[2].format    = VK_FORMAT_R32G32B32A32_SFLOAT;
   attributes[2].offset    = 4 * sizeof(float);

   binding.binding         = 0;
   binding.stride          = sizeof(struct vk_vertex);
   binding.inputRate       = VK_VERTEX_INPUT_RATE_VERTEX;

   vertex_input.vertexBindingDescriptionCount   = 1;
   vertex_input.pVertexBindingDescriptions      = &binding;
   vertex_input.vertexAttributeDescriptionCount = 3;
   vertex_input.pVertexAttributeDescriptions    = attributes;

   /* Raster state */
   raster.polygonMode                   = VK_POLYGON_MODE_FILL;
   raster.cullMode                      = VK_CULL_MODE_NONE;
   raster.frontFace                     = VK_FRONT_FACE_COUNTER_CLOCKWISE;
   raster.depthClampEnable              = VK_FALSE;
   raster.rasterizerDiscardEnable       = VK_FALSE;
   raster.depthBiasEnable               = VK_FALSE;
   raster.lineWidth                     = 1.0f;

   /* Blend state */
   blend_attachment.blendEnable         = VK_FALSE;
   blend_attachment.colorWriteMask      = 0xf;
   blend.attachmentCount                = 1;
   blend.pAttachments                   = &blend_attachment;

   /* Viewport state */
   viewport.viewportCount               = 1;
   viewport.scissorCount                = 1;

   /* Depth-stencil state */
   depth_stencil.depthTestEnable        = VK_FALSE;
   depth_stencil.depthWriteEnable       = VK_FALSE;
   depth_stencil.depthBoundsTestEnable  = VK_FALSE;
   depth_stencil.stencilTestEnable      = VK_FALSE;
   depth_stencil.minDepthBounds         = 0.0f;
   depth_stencil.maxDepthBounds         = 1.0f;

   /* Multisample state */
   multisample.sType                    = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
   multisample.pNext                    = NULL;
   multisample.flags                    = 0;
   multisample.rasterizationSamples     = VK_SAMPLE_COUNT_1_BIT;
   multisample.sampleShadingEnable      = VK_FALSE;
   multisample.minSampleShading         = 0.0f;
   multisample.pSampleMask              = NULL;
   multisample.alphaToCoverageEnable    = VK_FALSE;
   multisample.alphaToOneEnable         = VK_FALSE;

   /* Dynamic state */
   dynamic.pDynamicStates               = dynamics;
   dynamic.dynamicStateCount            = ARRAY_SIZE(dynamics);

   pipe.stageCount                      = 2;
   pipe.pStages                         = shader_stages;
   pipe.pVertexInputState               = &vertex_input;
   pipe.pInputAssemblyState             = &input_assembly;
   pipe.pRasterizationState             = &raster;
   pipe.pColorBlendState                = &blend;
   pipe.pMultisampleState               = &multisample;
   pipe.pViewportState                  = &viewport;
   pipe.pDepthStencilState              = &depth_stencil;
   pipe.pDynamicState                   = &dynamic;
   pipe.renderPass                      = vk->render_pass;
   pipe.layout                          = vk->pipelines.layout;

   module_info.codeSize                 = sizeof(alpha_blend_vert);
   module_info.pCode                    = alpha_blend_vert;
   shader_stages[0].stage               = VK_SHADER_STAGE_VERTEX_BIT;
   shader_stages[0].pName               = "main";
   vkCreateShaderModule(vk->context->device,
         &module_info, NULL, &shader_stages[0].module);

   blend_attachment.blendEnable         = VK_TRUE;
   blend_attachment.colorWriteMask      = 0xf;
   blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
   blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
   blend_attachment.colorBlendOp        = VK_BLEND_OP_ADD;
   blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
   blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
   blend_attachment.alphaBlendOp        = VK_BLEND_OP_ADD;

   /* Glyph pipeline */
   module_info.codeSize                 = sizeof(font_frag);
   module_info.pCode                    = font_frag;
   shader_stages[1].stage               = VK_SHADER_STAGE_FRAGMENT_BIT;
   shader_stages[1].pName               = "main";
   vkCreateShaderModule(vk->context->device,
         &module_info, NULL, &shader_stages[1].module);

   vkCreateGraphicsPipelines(vk->context->device, vk->pipelines.cache,
         1, &pipe, NULL, &vk->pipelines.font);
   vkDestroyShaderModule(vk->context->device, shader_stages[1].module, NULL);

   /* Alpha-blended pipeline. */
   module_info.codeSize   = sizeof(alpha_blend_frag);
   module_info.pCode      = alpha_blend_frag;
   shader_stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
   shader_stages[1].pName = "main";
   vkCreateShaderModule(vk->context->device,
         &module_info, NULL, &shader_stages[1].module);

   vkCreateGraphicsPipelines(vk->context->device, vk->pipelines.cache,
         1, &pipe, NULL, &vk->pipelines.alpha_blend);

   /* Build display pipelines. */
   for (i = 0; i < 4; i++)
   {
      input_assembly.topology = i & 2 ?
         VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP :
         VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
      blend_attachment.blendEnable = i & 1;
      vkCreateGraphicsPipelines(vk->context->device, vk->pipelines.cache,
            1, &pipe, NULL, &vk->display.pipelines[i]);
   }

   vkDestroyShaderModule(vk->context->device, shader_stages[1].module, NULL);

#ifdef VULKAN_HDR_SWAPCHAIN
if (vk->context->flags & VK_CTX_FLAG_HDR_SUPPORT)
{
   blend_attachment.blendEnable = VK_FALSE;

   /* HDR pipeline. */
   module_info.codeSize         = sizeof(hdr_frag);
   module_info.pCode            = hdr_frag;
   shader_stages[1].stage       = VK_SHADER_STAGE_FRAGMENT_BIT;
   shader_stages[1].pName       = "main";
   vkCreateShaderModule(vk->context->device,
         &module_info, NULL, &shader_stages[1].module);

   vkCreateGraphicsPipelines(vk->context->device, vk->pipelines.cache,
         1, &pipe, NULL, &vk->pipelines.hdr);

   /* Build display hdr pipelines. */
   for (i = 4; i < 6; i++)
   {
      input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
      vkCreateGraphicsPipelines(vk->context->device, vk->pipelines.cache,
            1, &pipe, NULL, &vk->display.pipelines[i]);
   }

   vkDestroyShaderModule(vk->context->device, shader_stages[1].module, NULL);

   /* HDR->SDR tonemapping readback pipeline. */
   module_info.codeSize         = sizeof(hdr_tonemap_frag);
   module_info.pCode            = hdr_tonemap_frag;
   shader_stages[1].stage       = VK_SHADER_STAGE_FRAGMENT_BIT;
   shader_stages[1].pName       = "main";
   vkCreateShaderModule(vk->context->device,
         &module_info, NULL, &shader_stages[1].module);

   pipe.renderPass = vk->readback_render_pass;
   vkCreateGraphicsPipelines(vk->context->device, vk->pipelines.cache,
         1, &pipe, NULL, &vk->pipelines.hdr_to_sdr);

   vkDestroyShaderModule(vk->context->device, shader_stages[1].module, NULL);

   pipe.renderPass = vk->render_pass;
   blend_attachment.blendEnable = VK_TRUE;
}
#endif /* VULKAN_HDR_SWAPCHAIN */

   vkDestroyShaderModule(vk->context->device, shader_stages[0].module, NULL);

   /* Other menu pipelines. */
   for (i = 0; i < (int)ARRAY_SIZE(vk->display.pipelines) - 6; i++)
   {
      switch (i >> 1)
      {
         case 0:
            module_info.codeSize   = sizeof(pipeline_ribbon_vert);
            module_info.pCode      = pipeline_ribbon_vert;
            break;

         case 1:
            module_info.codeSize   = sizeof(pipeline_ribbon_simple_vert);
            module_info.pCode      = pipeline_ribbon_simple_vert;
            break;

         case 2:
            module_info.codeSize   = sizeof(alpha_blend_vert);
            module_info.pCode      = alpha_blend_vert;
            break;

         case 3:
            module_info.codeSize   = sizeof(alpha_blend_vert);
            module_info.pCode      = alpha_blend_vert;
            break;

         case 4:
            module_info.codeSize   = sizeof(alpha_blend_vert);
            module_info.pCode      = alpha_blend_vert;
            break;

         default:
            break;
      }

      shader_stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
      shader_stages[0].pName = "main";
      vkCreateShaderModule(vk->context->device,
            &module_info, NULL, &shader_stages[0].module);

      switch (i >> 1)
      {
         case 0:
            module_info.codeSize   = sizeof(pipeline_ribbon_frag);
            module_info.pCode      = pipeline_ribbon_frag;
            break;

         case 1:
            module_info.codeSize   = sizeof(pipeline_ribbon_simple_frag);
            module_info.pCode      = pipeline_ribbon_simple_frag;
            break;

         case 2:
            module_info.codeSize   = sizeof(pipeline_snow_simple_frag);
            module_info.pCode      = pipeline_snow_simple_frag;
            break;

         case 3:
            module_info.codeSize   = sizeof(pipeline_snow_frag);
            module_info.pCode      = pipeline_snow_frag;
            break;

         case 4:
            module_info.codeSize   = sizeof(pipeline_bokeh_frag);
            module_info.pCode      = pipeline_bokeh_frag;
            break;

         default:
            break;
      }

      shader_stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
      shader_stages[1].pName = "main";
      vkCreateShaderModule(vk->context->device,
            &module_info, NULL, &shader_stages[1].module);

      switch (i >> 1)
      {
         case 0:
         case 1:
            blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_DST_COLOR;
            blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
            break;
         default:
            blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
            blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            break;
      }

      input_assembly.topology = i & 1 ?
         VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP :
         VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

      vkCreateGraphicsPipelines(vk->context->device, vk->pipelines.cache,
            1, &pipe, NULL, &vk->display.pipelines[6 + i]);

      vkDestroyShaderModule(vk->context->device, shader_stages[0].module, NULL);
      vkDestroyShaderModule(vk->context->device, shader_stages[1].module, NULL);
   }

   cpipe.layout           = vk->pipelines.layout;
   cpipe.stage.sType      = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
   cpipe.stage.pName      = "main";
   cpipe.stage.stage      = VK_SHADER_STAGE_COMPUTE_BIT;

   module_info.codeSize   = sizeof(rgb565_to_rgba8888_comp);
   module_info.pCode      = rgb565_to_rgba8888_comp;
   vkCreateShaderModule(vk->context->device,
         &module_info, NULL, &cpipe.stage.module);
   vkCreateComputePipelines(vk->context->device, vk->pipelines.cache,
         1, &cpipe, NULL, &vk->pipelines.rgb565_to_rgba8888);
   vkDestroyShaderModule(vk->context->device, cpipe.stage.module, NULL);
}

static void vulkan_init_samplers(vk_t *vk)
{
   VkSamplerCreateInfo info;

   info.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
   info.pNext                   = NULL;
   info.flags                   = 0;
   info.magFilter               = VK_FILTER_NEAREST;
   info.minFilter               = VK_FILTER_NEAREST;
   info.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_NEAREST;
   info.addressModeU            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
   info.addressModeV            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
   info.addressModeW            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
   info.mipLodBias              = 0.0f;
   info.anisotropyEnable        = VK_FALSE;
   info.maxAnisotropy           = 1.0f;
   info.compareEnable           = VK_FALSE;
   info.minLod                  = 0.0f;
   info.maxLod                  = 0.0f;
   info.borderColor             = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
   info.unnormalizedCoordinates = VK_FALSE;
   vkCreateSampler(vk->context->device,
         &info, NULL, &vk->samplers.nearest);

   info.magFilter               = VK_FILTER_LINEAR;
   info.minFilter               = VK_FILTER_LINEAR;
   vkCreateSampler(vk->context->device,
         &info, NULL, &vk->samplers.linear);

   info.maxLod                  = VK_LOD_CLAMP_NONE;
   info.magFilter               = VK_FILTER_NEAREST;
   info.minFilter               = VK_FILTER_NEAREST;
   info.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_NEAREST;
   vkCreateSampler(vk->context->device,
         &info, NULL, &vk->samplers.mipmap_nearest);

   info.magFilter               = VK_FILTER_LINEAR;
   info.minFilter               = VK_FILTER_LINEAR;
   info.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR;
   vkCreateSampler(vk->context->device,
         &info, NULL, &vk->samplers.mipmap_linear);
}

static void vulkan_buffer_chain_free(
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


static void vulkan_deinit_buffers(vk_t *vk)
{
   int i;
   for (i = 0; i < (int) vk->num_swapchain_images; i++)
   {
      vulkan_buffer_chain_free(
            vk->context->device, &vk->swapchain[i].vbo);
      vulkan_buffer_chain_free(
            vk->context->device, &vk->swapchain[i].ubo);
   }
}

static void vulkan_deinit_descriptor_pool(vk_t *vk)
{
   int i;
   for (i = 0; i < (int) vk->num_swapchain_images; i++)
      vulkan_destroy_descriptor_manager(
            vk->context->device,
            &vk->swapchain[i].descriptor_manager);
}

static void vulkan_init_textures(vk_t *vk)
{
   const uint32_t zero = 0;

   if (!(vk->flags & VK_FLAG_HW_ENABLE))
   {
      int i;
      for (i = 0; i < (int) vk->num_swapchain_images; i++)
      {
         vk->swapchain[i].texture = vulkan_create_texture(
               vk, NULL, vk->tex_w, vk->tex_h, vk->tex_fmt,
               NULL, NULL, VULKAN_TEXTURE_STREAMED);

         {
            struct vk_texture *texture = &vk->swapchain[i].texture;
            VK_MAP_PERSISTENT_TEXTURE(vk->context->device, texture);
         }

         if (vk->swapchain[i].texture.type == VULKAN_TEXTURE_STAGING)
            vk->swapchain[i].texture_optimal = vulkan_create_texture(
                  vk, NULL, vk->tex_w, vk->tex_h, vk->tex_fmt,
                  NULL, NULL, VULKAN_TEXTURE_DYNAMIC);
      }
   }

   vk->default_texture = vulkan_create_texture(vk, NULL,
         1, 1, VK_FORMAT_B8G8R8A8_UNORM,
         &zero, NULL, VULKAN_TEXTURE_STATIC);
}

static void vulkan_deinit_textures(vk_t *vk)
{
   int i;
   video_driver_state_t *video_st = video_state_get_ptr();
   /* Avoid memcpying from a destroyed/unmapped texture later on. */
   const void *cached_frame       = video_st->frame_cache_data;
   if (vulkan_is_mapped_swapchain_texture_ptr(vk, cached_frame))
      video_st->frame_cache_data  = NULL;

   vkDestroySampler(vk->context->device, vk->samplers.nearest,        NULL);
   vkDestroySampler(vk->context->device, vk->samplers.linear,         NULL);
   vkDestroySampler(vk->context->device, vk->samplers.mipmap_nearest, NULL);
   vkDestroySampler(vk->context->device, vk->samplers.mipmap_linear,  NULL);

   for (i = 0; i < (int) vk->num_swapchain_images; i++)
   {
      if (vk->swapchain[i].texture.memory != VK_NULL_HANDLE)
         vulkan_destroy_texture(
               vk->context->device, &vk->swapchain[i].texture);

      if (vk->swapchain[i].texture_optimal.memory != VK_NULL_HANDLE)
         vulkan_destroy_texture(
               vk->context->device, &vk->swapchain[i].texture_optimal);
   }

   if (vk->default_texture.memory != VK_NULL_HANDLE)
      vulkan_destroy_texture(vk->context->device, &vk->default_texture);
}

static void vulkan_deinit_command_buffers(vk_t *vk)
{
   int i;
   for (i = 0; i < (int) vk->num_swapchain_images; i++)
   {
      if (vk->swapchain[i].cmd)
         vkFreeCommandBuffers(vk->context->device,
               vk->swapchain[i].cmd_pool, 1, &vk->swapchain[i].cmd);

      vkDestroyCommandPool(vk->context->device,
            vk->swapchain[i].cmd_pool, NULL);
   }
}

static void vulkan_deinit_pipelines(vk_t *vk)
{
   int i;

   vkDestroyPipelineLayout(vk->context->device,
         vk->pipelines.layout, NULL);
   vkDestroyDescriptorSetLayout(vk->context->device,
         vk->pipelines.set_layout, NULL);
   vkDestroyPipeline(vk->context->device,
         vk->pipelines.alpha_blend, NULL);
   vkDestroyPipeline(vk->context->device,
         vk->pipelines.font, NULL);
   vkDestroyPipeline(vk->context->device,
         vk->pipelines.rgb565_to_rgba8888, NULL);
#ifdef VULKAN_HDR_SWAPCHAIN
if (vk->context->flags & VK_CTX_FLAG_HDR_SUPPORT)
{
   vkDestroyPipeline(vk->context->device,
         vk->pipelines.hdr, NULL);
   vkDestroyPipeline(vk->context->device,
         vk->pipelines.hdr_to_sdr, NULL);
}
#endif /* VULKAN_HDR_SWAPCHAIN */

   for (i = 0; i < (int)ARRAY_SIZE(vk->display.pipelines); i++)
      vkDestroyPipeline(vk->context->device,
            vk->display.pipelines[i], NULL);
}

static void vulkan_deinit_framebuffers(vk_t *vk)
{
   int i;
   for (i = 0; i < (int) vk->num_swapchain_images; i++)
   {
      if (vk->backbuffers[i].framebuffer)
         vkDestroyFramebuffer(vk->context->device,
               vk->backbuffers[i].framebuffer, NULL);

      if (vk->backbuffers[i].view)
         vkDestroyImageView(vk->context->device,
               vk->backbuffers[i].view, NULL);
   }

   vkDestroyRenderPass(vk->context->device, vk->render_pass, NULL);
}

#ifdef VULKAN_HDR_SWAPCHAIN
static void vulkan_deinit_hdr_readback_render_pass(vk_t *vk)
{
   vkDestroyRenderPass(vk->context->device, vk->readback_render_pass, NULL);
}

static void vulkan_set_hdr_max_nits(void* data, float max_nits)
{
   vk_t *vk                            = (vk_t*)data;
   vulkan_hdr_uniform_t* mapped_ubo    = (vulkan_hdr_uniform_t*)vk->hdr.ubo.mapped;

   vk->hdr.max_output_nits             = max_nits;
   mapped_ubo->max_nits                = max_nits;
}

static void vulkan_set_hdr_paper_white_nits(void* data, float paper_white_nits)
{
   vk_t *vk                            = (vk_t*)data;
   vulkan_hdr_uniform_t* mapped_ubo    = (vulkan_hdr_uniform_t*)vk->hdr.ubo.mapped;

   mapped_ubo->paper_white_nits = paper_white_nits;
}

static void vulkan_set_hdr_contrast(void* data, float contrast)
{
   vk_t *vk                            = (vk_t*)data;
   vulkan_hdr_uniform_t* mapped_ubo    = (vulkan_hdr_uniform_t*)vk->hdr.ubo.mapped;

   mapped_ubo->contrast                = contrast;
}

static void vulkan_set_hdr_expand_gamut(void* data, bool expand_gamut)
{
   vk_t *vk                            = (vk_t*)data;
   vulkan_hdr_uniform_t* mapped_ubo    = (vulkan_hdr_uniform_t*)vk->hdr.ubo.mapped;

   mapped_ubo->expand_gamut     = expand_gamut ? 1.0f : 0.0f;
}

static void vulkan_set_hdr_inverse_tonemap(vk_t* vk, bool inverse_tonemap)
{
   vulkan_hdr_uniform_t* mapped_ubo = (vulkan_hdr_uniform_t*)vk->hdr.ubo.mapped;

   mapped_ubo->inverse_tonemap      = inverse_tonemap ? 1.0f : 0.0f;
}

static void vulkan_set_hdr10(vk_t* vk, bool hdr10)
{
   vulkan_hdr_uniform_t* mapped_ubo = (vulkan_hdr_uniform_t*)vk->hdr.ubo.mapped;

   mapped_ubo->hdr10                = hdr10 ? 1.0f : 0.0f;
}
#endif /* VULKAN_HDR_SWAPCHAIN */

static bool vulkan_init_default_filter_chain(vk_t *vk)
{
   struct vulkan_filter_chain_create_info info;

   if (!vk->context)
      return false;

   info.device                = vk->context->device;
   info.gpu                   = vk->context->gpu;
   info.memory_properties     = &vk->context->memory_properties;
   info.pipeline_cache        = vk->pipelines.cache;
   info.queue                 = vk->context->queue;
   info.command_pool          = vk->swapchain[vk->context->current_frame_index].cmd_pool;
   info.num_passes            = 0;
   info.original_format       = VK_REMAP_TO_TEXFMT(vk->tex_fmt);
   info.max_input_size.width  = vk->tex_w;
   info.max_input_size.height = vk->tex_h;
   info.swapchain.viewport    = vk->vk_vp;
   info.swapchain.format      = vk->context->swapchain_format;
   info.swapchain.render_pass = vk->render_pass;
   info.swapchain.num_indices = vk->context->num_swapchain_images;

   vk->filter_chain           = vulkan_filter_chain_create_default(
         &info,
         vk->video.smooth
         ? GLSLANG_FILTER_CHAIN_LINEAR
         : GLSLANG_FILTER_CHAIN_NEAREST);

   if (!vk->filter_chain)
   {
      RARCH_ERR("Failed to create filter chain.\n");
      return false;
   }

#ifdef VULKAN_HDR_SWAPCHAIN
   if (vk->context->flags & VK_CTX_FLAG_HDR_ENABLE)
   {
      struct video_shader* shader_preset = vulkan_filter_chain_get_preset(
      vk->filter_chain);
      VkFormat rt_format = (shader_preset && shader_preset->passes)
         ? vulkan_filter_chain_get_pass_rt_format(vk->filter_chain, shader_preset->passes - 1)
         : VK_FORMAT_UNDEFINED;
      bool emits_hdr10 = shader_preset && shader_preset->passes && vulkan_filter_chain_emits_hdr10(vk->filter_chain);

      if (vulkan_is_hdr10_format(rt_format))
      {
         /* If the last shader pass uses a RGB10A2 back buffer
          * and HDR has been enabled, assume we want to skip
          * the inverse tonemapper and HDR10 conversion.
          * If we just inherited HDR10 format based on backbuffer,
          * we would have used RGBA8, and thus we should do inverse tonemap as expected. */
         vulkan_set_hdr_inverse_tonemap(vk, !emits_hdr10);
         vulkan_set_hdr10(vk, !emits_hdr10);
         vk->flags |= VK_FLAG_SHOULD_RESIZE;
      }
      else if (rt_format == VK_FORMAT_R16G16B16A16_SFLOAT)
      {
         /* If the last shader pass uses a RGBA16 backbuffer
          * and HDR has been enabled, assume we want to
          * skip the inverse tonemapper */
         vulkan_set_hdr_inverse_tonemap(vk, false);
         vulkan_set_hdr10(vk, true);
         vk->flags |= VK_FLAG_SHOULD_RESIZE;
      }
      else
      {
         vulkan_set_hdr_inverse_tonemap(vk, true);
         vulkan_set_hdr10(vk, true);
      }
   }
#endif /* VULKAN_HDR_SWAPCHAIN */

   return true;
}

static bool vulkan_init_filter_chain_preset(vk_t *vk, const char *shader_path)
{
   struct vulkan_filter_chain_create_info info;

   info.device                = vk->context->device;
   info.gpu                   = vk->context->gpu;
   info.memory_properties     = &vk->context->memory_properties;
   info.pipeline_cache        = vk->pipelines.cache;
   info.queue                 = vk->context->queue;
   info.command_pool          = vk->swapchain[vk->context->current_frame_index].cmd_pool;
   info.num_passes            = 0;
   info.original_format       = VK_REMAP_TO_TEXFMT(vk->tex_fmt);
   info.max_input_size.width  = vk->tex_w;
   info.max_input_size.height = vk->tex_h;
   info.swapchain.viewport    = vk->vk_vp;
   info.swapchain.format      = vk->context->swapchain_format;
   info.swapchain.render_pass = vk->render_pass;
   info.swapchain.num_indices = vk->context->num_swapchain_images;

   vk->filter_chain           = vulkan_filter_chain_create_from_preset(
         &info, shader_path,
         vk->video.smooth
         ? GLSLANG_FILTER_CHAIN_LINEAR
         : GLSLANG_FILTER_CHAIN_NEAREST);

   if (!vk->filter_chain)
   {
      RARCH_ERR("[Vulkan]: Failed to create preset: \"%s\".\n", shader_path);
      return false;
   }

#ifdef VULKAN_HDR_SWAPCHAIN
   if (vk->context->flags & VK_CTX_FLAG_HDR_ENABLE)
   {
      struct video_shader* shader_preset = vulkan_filter_chain_get_preset(vk->filter_chain);
      VkFormat rt_format = (shader_preset && shader_preset->passes)
         ? vulkan_filter_chain_get_pass_rt_format(vk->filter_chain, shader_preset->passes - 1)
         : VK_FORMAT_UNDEFINED;
      bool emits_hdr10 = shader_preset && shader_preset->passes && vulkan_filter_chain_emits_hdr10(vk->filter_chain);

      if (vulkan_is_hdr10_format(rt_format))
      {
         /* If the last shader pass uses a RGB10A2 back buffer
          * and HDR has been enabled, assume we want to skip
          * the inverse tonemapper and HDR10 conversion.
          * If we just inherited HDR10 format based on backbuffer,
          * we would have used RGBA8, and thus we should do inverse tonemap as expected. */
         vulkan_set_hdr_inverse_tonemap(vk, !emits_hdr10);
         vulkan_set_hdr10(vk, !emits_hdr10);
         vk->flags |= VK_FLAG_SHOULD_RESIZE;
      }
      else if (rt_format == VK_FORMAT_R16G16B16A16_SFLOAT)
      {
         /* If the last shader pass uses a RGBA16 backbuffer
          * and HDR has been enabled, assume we want to
          * skip the inverse tonemapper */
         vulkan_set_hdr_inverse_tonemap(vk, false);
         vulkan_set_hdr10(vk, true);
         vk->flags |= VK_FLAG_SHOULD_RESIZE;
      }
      else
      {
         vulkan_set_hdr_inverse_tonemap(vk, true);
         vulkan_set_hdr10(vk, true);
      }
   }
#endif /* VULKAN_HDR_SWAPCHAIN */

   return true;
}

static bool vulkan_init_filter_chain(vk_t *vk)
{
   const char     *shader_path = video_shader_get_current_shader_preset();
   enum rarch_shader_type type = video_shader_parse_type(shader_path);

   if (string_is_empty(shader_path))
   {
      RARCH_LOG("[Vulkan]: Loading stock shader.\n");
      return vulkan_init_default_filter_chain(vk);
   }

   if (type != RARCH_SHADER_SLANG)
   {
      RARCH_LOG("[Vulkan]: Only Slang shaders are supported, falling back to stock.\n");
      return vulkan_init_default_filter_chain(vk);
   }

   if (!shader_path || !vulkan_init_filter_chain_preset(vk, shader_path))
      vulkan_init_default_filter_chain(vk);

   return true;
}

static void vulkan_init_static_resources(vk_t *vk)
{
   int i;
   uint32_t blank[4 * 4];
   VkCommandPoolCreateInfo pool_info;
   VkPipelineCacheCreateInfo cache;

   /* Create the pipeline cache. */
   cache.sType                = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
   cache.pNext                = NULL;
   cache.flags                = 0;
   cache.initialDataSize      = 0;
   cache.pInitialData         = NULL;

   vkCreatePipelineCache(vk->context->device,
         &cache, NULL, &vk->pipelines.cache);

   pool_info.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
   pool_info.pNext            = NULL;
   pool_info.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
   pool_info.queueFamilyIndex = vk->context->graphics_queue_index;

   vkCreateCommandPool(vk->context->device,
         &pool_info, NULL, &vk->staging_pool);

   for (i = 0; i < 4 * 4; i++)
      blank[i] = -1u;

   vk->display.blank_texture = vulkan_create_texture(vk, NULL,
         4, 4, VK_FORMAT_B8G8R8A8_UNORM,
         blank, NULL, VULKAN_TEXTURE_STATIC);
}

static void vulkan_deinit_static_resources(vk_t *vk)
{
   int i;
   vkDestroyPipelineCache(vk->context->device,
         vk->pipelines.cache, NULL);
   vulkan_destroy_texture(
         vk->context->device,
         &vk->display.blank_texture);

   vkDestroyCommandPool(vk->context->device,
         vk->staging_pool, NULL);
   free(vk->hw.cmd);
   free(vk->hw.wait_dst_stages);
   free(vk->hw.semaphores);

   for (i = 0; i < VULKAN_MAX_SWAPCHAIN_IMAGES; i++)
      if (vk->readback.staging[i].memory != VK_NULL_HANDLE)
         vulkan_destroy_texture(
               vk->context->device,
               &vk->readback.staging[i]);
}

static void vulkan_deinit_menu(vk_t *vk)
{
   int i;
   for (i = 0; i < VULKAN_MAX_SWAPCHAIN_IMAGES; i++)
   {
      if (vk->menu.textures[i].memory)
         vulkan_destroy_texture(
               vk->context->device, &vk->menu.textures[i]);
      if (vk->menu.textures_optimal[i].memory)
         vulkan_destroy_texture(
               vk->context->device, &vk->menu.textures_optimal[i]);
   }
}

#ifdef VULKAN_HDR_SWAPCHAIN
static void vulkan_destroy_hdr_buffer(VkDevice device, struct vk_image *img)
{
   vkDestroyImageView(device, img->view, NULL);
   vkDestroyImage(device, img->image, NULL);
   vkDestroyFramebuffer(device, img->framebuffer, NULL);
   vkFreeMemory(device, img->memory, NULL);
   memset(img, 0, sizeof(*img));
}
#endif

static void vulkan_free(void *data)
{
   vk_t *vk = (vk_t*)data;
   if (!vk)
      return;

   if (vk->context && vk->context->device)
   {
#ifdef HAVE_THREADS
      slock_lock(vk->context->queue_lock);
#endif
      vkQueueWaitIdle(vk->context->queue);
#ifdef HAVE_THREADS
      slock_unlock(vk->context->queue_lock);
#endif
      vulkan_deinit_pipelines(vk);
      vulkan_deinit_framebuffers(vk);
      vulkan_deinit_descriptor_pool(vk);
      vulkan_deinit_textures(vk);
      vulkan_deinit_buffers(vk);
      vulkan_deinit_command_buffers(vk);

      /* No need to init this since textures are create on-demand. */
      vulkan_deinit_menu(vk);

      font_driver_free_osd();

      vulkan_deinit_static_resources(vk);
#ifdef HAVE_OVERLAY
      vulkan_overlay_free(vk);
#endif

      if (vk->filter_chain)
         vulkan_filter_chain_free((vulkan_filter_chain_t*)vk->filter_chain);

#ifdef VULKAN_HDR_SWAPCHAIN
      if (vk->context->flags & VK_CTX_FLAG_HDR_SUPPORT)
      {
         vulkan_destroy_buffer(vk->context->device, &vk->hdr.ubo);
         vulkan_destroy_hdr_buffer(vk->context->device, &vk->main_buffer);
         vulkan_destroy_hdr_buffer(vk->context->device, &vk->readback_image);
         vulkan_deinit_hdr_readback_render_pass(vk);
         video_driver_unset_hdr_support();
      }
#endif /* VULKAN_HDR_SWAPCHAIN */

      if (vk->ctx_driver && vk->ctx_driver->destroy)
         vk->ctx_driver->destroy(vk->ctx_data);
      video_context_driver_free();
   }

   scaler_ctx_gen_reset(&vk->readback.scaler_bgr);
   scaler_ctx_gen_reset(&vk->readback.scaler_rgb);
   free(vk);
}

static uint32_t vulkan_get_sync_index(void *handle)
{
   vk_t *vk = (vk_t*)handle;
   return vk->context->current_frame_index;
}

static uint32_t vulkan_get_sync_index_mask(void *handle)
{
   vk_t *vk = (vk_t*)handle;
   return (1 << vk->context->num_swapchain_images) - 1;
}

static void vulkan_set_image(void *handle,
      const struct retro_vulkan_image *image,
      uint32_t num_semaphores,
      const VkSemaphore *semaphores,
      uint32_t src_queue_family)
{
   vk_t *vk              = (vk_t*)handle;

   vk->hw.image          = image;
   vk->hw.num_semaphores = num_semaphores;

   if (num_semaphores > 0)
   {
      int i;

      /* Allocate one extra in case we need to use WSI acquire semaphores. */
      VkPipelineStageFlags *stage_flags = (VkPipelineStageFlags*)realloc(vk->hw.wait_dst_stages,
            sizeof(VkPipelineStageFlags) * (vk->hw.num_semaphores + 1));

      VkSemaphore *new_semaphores = (VkSemaphore*)realloc(vk->hw.semaphores,
            sizeof(VkSemaphore) * (vk->hw.num_semaphores + 1));

      vk->hw.wait_dst_stages = stage_flags;
      vk->hw.semaphores      = new_semaphores;

      for (i = 0; i < (int) vk->hw.num_semaphores; i++)
      {
         vk->hw.wait_dst_stages[i] = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
         vk->hw.semaphores[i]      = semaphores[i];
      }

      vk->flags                   |= VK_FLAG_HW_VALID_SEMAPHORE;
      vk->hw.src_queue_family      = src_queue_family;
   }
}

static void vulkan_wait_sync_index(void *handle)
{
   /* no-op. RetroArch already waits for this
    * in gfx_ctx_swap_buffers(). */
}

static void vulkan_set_command_buffers(void *handle, uint32_t num_cmd,
      const VkCommandBuffer *cmd)
{
   vk_t *vk                   = (vk_t*)handle;
   unsigned required_capacity = num_cmd + 1;
   if (required_capacity > vk->hw.capacity_cmd)
   {
      VkCommandBuffer *hw_cmd = (VkCommandBuffer*)
         realloc(vk->hw.cmd,
            sizeof(VkCommandBuffer) * required_capacity);

      vk->hw.cmd              = hw_cmd;
      vk->hw.capacity_cmd     = required_capacity;
   }

   vk->hw.num_cmd             = num_cmd;
   memcpy(vk->hw.cmd, cmd, sizeof(VkCommandBuffer) * num_cmd);
}

static void vulkan_lock_queue(void *handle)
{
#ifdef HAVE_THREADS
   vk_t *vk = (vk_t*)handle;
   slock_lock(vk->context->queue_lock);
#endif
}

static void vulkan_unlock_queue(void *handle)
{
#ifdef HAVE_THREADS
   vk_t *vk = (vk_t*)handle;
   slock_unlock(vk->context->queue_lock);
#endif
}

static void vulkan_set_signal_semaphore(void *handle, VkSemaphore semaphore)
{
   vk_t *vk = (vk_t*)handle;
   vk->hw.signal_semaphore = semaphore;
}

static void vulkan_init_hw_render(vk_t *vk)
{
   struct retro_hw_render_interface_vulkan *iface   =
      &vk->hw.iface;
   struct retro_hw_render_callback *hwr =
      video_driver_get_hw_context();

   if (hwr->context_type != RETRO_HW_CONTEXT_VULKAN)
      return;

   vk->flags                    |= VK_FLAG_HW_ENABLE;

   iface->interface_type         = RETRO_HW_RENDER_INTERFACE_VULKAN;
   iface->interface_version      = RETRO_HW_RENDER_INTERFACE_VULKAN_VERSION;
   iface->instance               = vk->context->instance;
   iface->gpu                    = vk->context->gpu;
   iface->device                 = vk->context->device;

   iface->queue                  = vk->context->queue;
   iface->queue_index            = vk->context->graphics_queue_index;

   iface->handle                 = vk;
   iface->set_image              = vulkan_set_image;
   iface->get_sync_index         = vulkan_get_sync_index;
   iface->get_sync_index_mask    = vulkan_get_sync_index_mask;
   iface->wait_sync_index        = vulkan_wait_sync_index;
   iface->set_command_buffers    = vulkan_set_command_buffers;
   iface->lock_queue             = vulkan_lock_queue;
   iface->unlock_queue           = vulkan_unlock_queue;
   iface->set_signal_semaphore   = vulkan_set_signal_semaphore;

   iface->get_device_proc_addr   = vkGetDeviceProcAddr;
   iface->get_instance_proc_addr = vulkan_symbol_wrapper_instance_proc_addr();
}

static void vulkan_init_readback(vk_t *vk, settings_t *settings)
{
   /* Only bother with this if we're doing GPU recording.
    * Check recording_st->enable and not
    * driver.recording_data, because recording is
    * not initialized yet.
    */
   recording_state_t
      *recording_st        = recording_state_get_ptr();
   bool recording_enabled  = recording_st->enable;
   bool video_gpu_record   = settings->bools.video_gpu_record;

   if (!(video_gpu_record && recording_enabled))
   {
      vk->flags                       &= ~VK_FLAG_READBACK_STREAMED;
      return;
   }

   vk->flags                          |=  VK_FLAG_READBACK_STREAMED;

   vk->readback.scaler_bgr.in_width    = vk->vp.width;
   vk->readback.scaler_bgr.in_height   = vk->vp.height;
   vk->readback.scaler_bgr.out_width   = vk->vp.width;
   vk->readback.scaler_bgr.out_height  = vk->vp.height;
   vk->readback.scaler_bgr.in_fmt      = SCALER_FMT_ARGB8888;
   vk->readback.scaler_bgr.out_fmt     = SCALER_FMT_BGR24;
   vk->readback.scaler_bgr.scaler_type = SCALER_TYPE_POINT;

   vk->readback.scaler_rgb.in_width    = vk->vp.width;
   vk->readback.scaler_rgb.in_height   = vk->vp.height;
   vk->readback.scaler_rgb.out_width   = vk->vp.width;
   vk->readback.scaler_rgb.out_height  = vk->vp.height;
   vk->readback.scaler_rgb.in_fmt      = SCALER_FMT_ABGR8888;
   vk->readback.scaler_rgb.out_fmt     = SCALER_FMT_BGR24;
   vk->readback.scaler_rgb.scaler_type = SCALER_TYPE_POINT;

   if (!scaler_ctx_gen_filter(&vk->readback.scaler_bgr))
   {
      vk->flags &= ~VK_FLAG_READBACK_STREAMED;
      RARCH_ERR("[Vulkan]: Failed to initialize scaler context.\n");
   }

   if (!scaler_ctx_gen_filter(&vk->readback.scaler_rgb))
   {
      vk->flags &= ~VK_FLAG_READBACK_STREAMED;
      RARCH_ERR("[Vulkan]: Failed to initialize scaler context.\n");
   }
}

static void *vulkan_init(const video_info_t *video,
      input_driver_t **input,
      void **input_data)
{
   unsigned full_x, full_y;
   unsigned win_width;
   unsigned win_height;
   unsigned mode_width                = 0;
   unsigned mode_height               = 0;
   int interval                       = 0;
   unsigned temp_width                = 0;
   unsigned temp_height               = 0;
   bool force_fullscreen              = false;
   const gfx_ctx_driver_t *ctx_driver = NULL;
   settings_t *settings               = config_get_ptr();
#ifdef VULKAN_HDR_SWAPCHAIN
   vulkan_hdr_uniform_t* mapped_ubo   = NULL;
#endif
   vk_t *vk                           = (vk_t*)calloc(1, sizeof(*vk));
   if (!vk)
      return NULL;
   ctx_driver                         = vulkan_get_context(vk, settings);
   if (!ctx_driver)
   {
      RARCH_ERR("[Vulkan]: Failed to get Vulkan context.\n");
      goto error;
   }

#ifdef VULKAN_HDR_SWAPCHAIN
   vk->hdr.max_output_nits             = settings->floats.video_hdr_max_nits;
   vk->hdr.min_output_nits             = 0.001f;
   vk->hdr.max_cll                     = 0.0f;
   vk->hdr.max_fall                    = 0.0f;
#endif /* VULKAN_HDR_SWAPCHAIN */

   vk->video                          = *video;
   vk->ctx_driver                     = ctx_driver;

   video_context_driver_set((const gfx_ctx_driver_t*)ctx_driver);

   RARCH_LOG("[Vulkan]: Found vulkan context: \"%s\".\n", ctx_driver->ident);

   if (vk->ctx_driver->get_video_size)
      vk->ctx_driver->get_video_size(vk->ctx_data,
            &mode_width, &mode_height);

   if (!video->fullscreen && !vk->ctx_driver->has_windowed)
   {
      RARCH_DBG("[Vulkan]: Config requires windowed mode, but context driver does not support it. "
                "Forcing fullscreen for this session.\n");
      force_fullscreen = true;
   }

   full_x                             = mode_width;
   full_y                             = mode_height;
   mode_width                         = 0;
   mode_height                        = 0;

   RARCH_LOG("[Vulkan]: Detecting screen resolution: %ux%u.\n", full_x, full_y);
   interval = video->vsync ? video->swap_interval : 0;

   if (ctx_driver->swap_interval)
   {
      bool adaptive_vsync_enabled            = video_driver_test_all_flags(
            GFX_CTX_FLAGS_ADAPTIVE_VSYNC) && video->adaptive_vsync;
      if (adaptive_vsync_enabled && interval == 1)
         interval = -1;
      ctx_driver->swap_interval(vk->ctx_data, interval);
   }

   win_width  = video->width;
   win_height = video->height;

   if (video->fullscreen && (win_width == 0) && (win_height == 0))
   {
      win_width  = full_x;
      win_height = full_y;
   }
   /* If fullscreen had to be forced, video->width/height is incorrect */
   else if (force_fullscreen)
   {
      win_width  = settings->uints.video_fullscreen_x;
      win_height = settings->uints.video_fullscreen_y;
   }

   if (     !vk->ctx_driver->set_video_mode
         || !vk->ctx_driver->set_video_mode(vk->ctx_data,
            win_width, win_height, (video->fullscreen || force_fullscreen)))
   {
      RARCH_ERR("[Vulkan]: Failed to set video mode.\n");
      goto error;
   }

   if (vk->ctx_driver->get_video_size)
      vk->ctx_driver->get_video_size(vk->ctx_data,
            &mode_width, &mode_height);

   temp_width  = mode_width;
   temp_height = mode_height;

   if (temp_width != 0 && temp_height != 0)
      video_driver_set_size(temp_width, temp_height);
   video_driver_get_size(&temp_width, &temp_height);
   vk->video_width       = temp_width;
   vk->video_height      = temp_height;
   vk->translate_x       = 0.0;
   vk->translate_y       = 0.0;

   RARCH_LOG("[Vulkan]: Using resolution %ux%u.\n", temp_width, temp_height);

   if (!vk->ctx_driver || !vk->ctx_driver->get_context_data)
   {
      RARCH_ERR("[Vulkan]: Failed to get context data.\n");
      goto error;
   }

   *(void**)&vk->context = vk->ctx_driver->get_context_data(vk->ctx_data);

   if (video->vsync)
      vk->flags         |=  VK_FLAG_VSYNC;
   else
      vk->flags         &= ~VK_FLAG_VSYNC;
   if (video->fullscreen || force_fullscreen)
      vk->flags         |=  VK_FLAG_FULLSCREEN;
   else
      vk->flags         &= ~VK_FLAG_FULLSCREEN;
   vk->tex_w             = RARCH_SCALE_BASE * video->input_scale;
   vk->tex_h             = RARCH_SCALE_BASE * video->input_scale;
   vk->tex_fmt           = video->rgb32 ? VK_FORMAT_B8G8R8A8_UNORM : VK_FORMAT_R5G6B5_UNORM_PACK16;
   if (video->force_aspect)
      vk->flags         |=  VK_FLAG_KEEP_ASPECT;
   else
      vk->flags         &= ~VK_FLAG_KEEP_ASPECT;
   RARCH_LOG("[Vulkan]: Using %s format.\n", video->rgb32 ? "BGRA8888" : "RGB565");

   /* Set the viewport to fix recording, since it needs to know
    * the viewport sizes before we start running. */
   vulkan_set_viewport(vk, temp_width, temp_height, false, true);

#ifdef VULKAN_HDR_SWAPCHAIN
   vk->hdr.ubo                  = vulkan_create_buffer(vk->context, sizeof(vulkan_hdr_uniform_t), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

   mapped_ubo                   = (vulkan_hdr_uniform_t*)vk->hdr.ubo.mapped;

   mapped_ubo->mvp              = vk->mvp_no_rot;
   mapped_ubo->max_nits         = settings->floats.video_hdr_max_nits;
   mapped_ubo->paper_white_nits = settings->floats.video_hdr_paper_white_nits;
   mapped_ubo->contrast         = VIDEO_HDR_MAX_CONTRAST - settings->floats.video_hdr_display_contrast;
   mapped_ubo->expand_gamut     = settings->bools.video_hdr_expand_gamut;
   mapped_ubo->inverse_tonemap  = 1.0f;     /* Use this to turn on/off the inverse tonemap */
   mapped_ubo->hdr10            = 1.0f;     /* Use this to turn on/off the hdr10 */
#endif /* VULKAN_HDR_SWAPCHAIN */

   vulkan_init_hw_render(vk);
   if (vk->context)
   {
      int i;
      static const VkDescriptorPoolSize pool_sizes[4] = {
         { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         VULKAN_DESCRIPTOR_MANAGER_BLOCK_SETS },
         { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VULKAN_DESCRIPTOR_MANAGER_BLOCK_SETS * 2 },
         { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          VULKAN_DESCRIPTOR_MANAGER_BLOCK_SETS },
         { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         VULKAN_DESCRIPTOR_MANAGER_BLOCK_SETS },
      };

      vulkan_init_static_resources(vk);

      vk->num_swapchain_images = vk->context->num_swapchain_images;

      vulkan_init_render_pass(vk);
#ifdef VULKAN_HDR_SWAPCHAIN
      if (vk->context->flags & VK_CTX_FLAG_HDR_SUPPORT)
         vulkan_init_hdr_readback_render_pass(vk);
#endif
      vulkan_init_framebuffers(vk);
      vulkan_init_pipelines(vk);
      vulkan_init_samplers(vk);
      vulkan_init_textures(vk);

      for (i = 0; i < (int) vk->num_swapchain_images; i++)
      {
         VkCommandPoolCreateInfo pool_info;
         VkCommandBufferAllocateInfo info;

         vk->swapchain[i].descriptor_manager =
            vulkan_create_descriptor_manager(
                  vk->context->device,
                  pool_sizes, 4, vk->pipelines.set_layout);
         vk->swapchain[i].vbo                =
            vulkan_buffer_chain_init(
               VULKAN_BUFFER_BLOCK_SIZE, 16,
               VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
         vk->swapchain[i].ubo                =
            vulkan_buffer_chain_init(
               VULKAN_BUFFER_BLOCK_SIZE,
               vk->context->gpu_properties.limits.minUniformBufferOffsetAlignment,
               VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

         pool_info.sType            =
            VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
         pool_info.pNext            = NULL;
         /* RESET_COMMAND_BUFFER_BIT allows command buffer to be reset. */
         pool_info.flags            =
            VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
         pool_info.queueFamilyIndex = vk->context->graphics_queue_index;

         vkCreateCommandPool(vk->context->device,
               &pool_info, NULL, &vk->swapchain[i].cmd_pool);

         info.sType                 =
            VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
         info.pNext                 = NULL;
         info.commandPool           = vk->swapchain[i].cmd_pool;
         info.level                 = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
         info.commandBufferCount    = 1;

         vkAllocateCommandBuffers(vk->context->device,
               &info, &vk->swapchain[i].cmd);
      }
   }

   if (!vulkan_init_filter_chain(vk))
   {
      RARCH_ERR("[Vulkan]: Failed to init filter chain.\n");
      goto error;
   }

   if (vk->ctx_driver->input_driver)
   {
      const char *joypad_name = settings->arrays.input_joypad_driver;
      vk->ctx_driver->input_driver(
            vk->ctx_data, joypad_name,
            input, input_data);
   }

   if (video->font_enable)
      font_driver_init_osd(vk,
            video,
            false,
            video->is_threaded,
            FONT_DRIVER_RENDER_VULKAN_API);

   /* The MoltenVK driver needs this, particularly after driver reinit
      Also it is required for HDR to not break during reinit, while not ideal it
      is the simplest solution unless reinit tracking is done */
   vk->flags |= VK_FLAG_SHOULD_RESIZE;

   vulkan_init_readback(vk, settings);
   return vk;

error:
   vulkan_free(vk);
   return NULL;
}

static void vulkan_check_swapchain(vk_t *vk)
{
   struct vulkan_filter_chain_swapchain_info filter_info;

#ifdef HAVE_THREADS
   slock_lock(vk->context->queue_lock);
#endif
   vkQueueWaitIdle(vk->context->queue);
#ifdef HAVE_THREADS
   slock_unlock(vk->context->queue_lock);
#endif
   vulkan_deinit_pipelines(vk);
   vulkan_deinit_framebuffers(vk);
   vulkan_deinit_descriptor_pool(vk);
   vulkan_deinit_textures(vk);
   vulkan_deinit_buffers(vk);
   vulkan_deinit_command_buffers(vk);
#ifdef VULKAN_HDR_SWAPCHAIN
   if (vk->context->flags & VK_CTX_FLAG_HDR_SUPPORT)
      vulkan_deinit_hdr_readback_render_pass(vk);
#endif
   if (vk->context)
   {
      int i;
      static const VkDescriptorPoolSize pool_sizes[4] = {
         { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         VULKAN_DESCRIPTOR_MANAGER_BLOCK_SETS },
         { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VULKAN_DESCRIPTOR_MANAGER_BLOCK_SETS * 2 },
         { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          VULKAN_DESCRIPTOR_MANAGER_BLOCK_SETS },
         { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         VULKAN_DESCRIPTOR_MANAGER_BLOCK_SETS },
      };
      vk->num_swapchain_images = vk->context->num_swapchain_images;

      vulkan_init_render_pass(vk);
#ifdef VULKAN_HDR_SWAPCHAIN
      if (vk->context->flags & VK_CTX_FLAG_HDR_SUPPORT)
         vulkan_init_hdr_readback_render_pass(vk);
#endif
      vulkan_init_framebuffers(vk);
      vulkan_init_pipelines(vk);
      vulkan_init_samplers(vk);
      vulkan_init_textures(vk);

      for (i = 0; i < (int) vk->num_swapchain_images; i++)
      {
         VkCommandPoolCreateInfo pool_info;
         VkCommandBufferAllocateInfo info;

         vk->swapchain[i].descriptor_manager =
            vulkan_create_descriptor_manager(
                  vk->context->device,
                  pool_sizes, 4, vk->pipelines.set_layout);

         vk->swapchain[i].vbo       = vulkan_buffer_chain_init(
               VULKAN_BUFFER_BLOCK_SIZE,
               16,
               VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
         vk->swapchain[i].ubo       = vulkan_buffer_chain_init(
               VULKAN_BUFFER_BLOCK_SIZE,
               vk->context->gpu_properties.limits.minUniformBufferOffsetAlignment,
               VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

         pool_info.sType            =
            VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
         pool_info.pNext            = NULL;
         /* RESET_COMMAND_BUFFER_BIT allows command buffer to be reset. */
         pool_info.flags            =
            VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
         pool_info.queueFamilyIndex = vk->context->graphics_queue_index;

         vkCreateCommandPool(vk->context->device,
               &pool_info, NULL, &vk->swapchain[i].cmd_pool);

         info.sType                 =
            VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
         info.pNext                 = NULL;
         info.commandPool           = vk->swapchain[i].cmd_pool;
         info.level                 = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
         info.commandBufferCount    = 1;

         vkAllocateCommandBuffers(vk->context->device,
               &info, &vk->swapchain[i].cmd);
      }
   }
   vk->context->flags              &= ~VK_CTX_FLAG_INVALID_SWAPCHAIN;

   filter_info.viewport             = vk->vk_vp;
   filter_info.format               = vk->context->swapchain_format;
   filter_info.render_pass          = vk->render_pass;
   filter_info.num_indices          = vk->context->num_swapchain_images;
   if (
       !vulkan_filter_chain_update_swapchain_info(
          (vulkan_filter_chain_t*)vk->filter_chain,
          &filter_info)
      )
      RARCH_ERR("Failed to update filter chain info. This will probably lead to a crash ...\n");
}

static void vulkan_set_nonblock_state(void *data, bool state,
      bool adaptive_vsync_enabled,
      unsigned swap_interval)
{
   vk_t *vk                    = (vk_t*)data;

   if (!vk)
      return;

   if (vk->ctx_driver->swap_interval)
   {
      int interval             = 0;
      if (!state)
         interval = swap_interval;
      if (adaptive_vsync_enabled && interval == 1)
         interval = -1;
      vk->ctx_driver->swap_interval(vk->ctx_data, interval);
   }

   /* Changing vsync might require recreating the swapchain,
    * which means new VkImages to render into. */
   if (vk->context->flags & VK_CTX_FLAG_INVALID_SWAPCHAIN)
      vulkan_check_swapchain(vk);
}

static bool vulkan_alive(void *data)
{
   bool ret             = false;
   bool quit            = false;
   bool resize          = false;
   vk_t *vk             = (vk_t*)data;
   unsigned temp_width  = vk->video_width;
   unsigned temp_height = vk->video_height;

   vk->ctx_driver->check_window(vk->ctx_data,
            &quit, &resize, &temp_width, &temp_height);

   if (quit)
      vk->flags |= VK_FLAG_QUITTING;
   else if (resize)
      vk->flags |= VK_FLAG_SHOULD_RESIZE;

   ret = (!(vk->flags & VK_FLAG_QUITTING));

   if (temp_width != 0 && temp_height != 0)
   {
      video_driver_set_size(temp_width, temp_height);
      vk->video_width  = temp_width;
      vk->video_height = temp_height;
   }

   return ret;
}

static bool vulkan_suppress_screensaver(void *data, bool enable)
{
   bool enabled = enable;
   vk_t *vk     = (vk_t*)data;

   if (vk->ctx_data && vk->ctx_driver->suppress_screensaver)
      return vk->ctx_driver->suppress_screensaver(vk->ctx_data, enabled);
   return false;
}

static bool vulkan_set_shader(void *data,
      enum rarch_shader_type type, const char *path)
{
   vk_t *vk = (vk_t*)data;
   if (!vk)
      return false;

   if (vk->filter_chain)
      vulkan_filter_chain_free((vulkan_filter_chain_t*)vk->filter_chain);
   vk->filter_chain = NULL;

   if (!string_is_empty(path) && type != RARCH_SHADER_SLANG)
   {
      RARCH_WARN("[Vulkan]: Only Slang shaders are supported. Falling back to stock.\n");
      path = NULL;
   }

   if (string_is_empty(path))
   {
      vulkan_init_default_filter_chain(vk);
      return true;
   }

   if (!vulkan_init_filter_chain_preset(vk, path))
   {
      RARCH_ERR("[Vulkan]: Failed to create filter chain: \"%s\". Falling back to stock.\n", path);
      vulkan_init_default_filter_chain(vk);
      return false;
   }

   return true;
}

static void vulkan_set_projection(vk_t *vk,
      struct video_ortho *ortho, bool allow_rotate)
{
   float radians, cosine, sine;
   static math_matrix_4x4 rot     = {
      {  0.0f,     0.0f,    0.0f,    0.0f ,
         0.0f,     0.0f,    0.0f,    0.0f ,
         0.0f,     0.0f,    0.0f,    0.0f ,
         0.0f,     0.0f,    0.0f,    1.0f }
   };
   math_matrix_4x4 trn     = {
      {  1.0f,     0.0f,    0.0f,    0.0f ,
         0.0f,     1.0f,    0.0f,    0.0f ,
         0.0f,     0.0f,    1.0f,    0.0f ,
         vk->translate_x/(float)vk->vp.width,
         vk->translate_y/(float)vk->vp.height,
         0.0f,
         1.0f }
   };
   math_matrix_4x4 tmp     = {
      {  1.0f,     0.0f,    0.0f,    0.0f ,
         0.0f,     1.0f,    0.0f,    0.0f ,
         0.0f,     0.0f,    1.0f,    0.0f ,
         0.0f,     0.0f,    0.0f,    1.0f }
   };

   /* Calculate projection. */
   matrix_4x4_ortho(vk->mvp_no_rot, ortho->left, ortho->right,
         ortho->bottom, ortho->top, ortho->znear, ortho->zfar);

   if (!allow_rotate)
      tmp = vk->mvp_no_rot;
   else
   {
      radians                 = M_PI * vk->rotation / 180.0f;
      cosine                  = cosf(radians);
      sine                    = sinf(radians);
      MAT_ELEM_4X4(rot, 0, 0) = cosine;
      MAT_ELEM_4X4(rot, 0, 1) = -sine;
      MAT_ELEM_4X4(rot, 1, 0) = sine;
      MAT_ELEM_4X4(rot, 1, 1) = cosine;
      matrix_4x4_multiply(tmp, rot, vk->mvp_no_rot);
   }
   matrix_4x4_multiply(vk->mvp, trn, tmp);

   /* Required for translate_x+y / negative offsets to also work in RGUI */
   matrix_4x4_multiply(vk->mvp_menu, trn, vk->mvp_no_rot);
}

static void vulkan_set_rotation(void *data, unsigned rotation)
{
   vk_t *vk               = (vk_t*)data;
   struct video_ortho ortho = {0, 1, 0, 1, -1, 1};

   if (!vk)
      return;

   vk->rotation = 270 * rotation;
   vulkan_set_projection(vk, &ortho, true);
}

static void vulkan_set_video_mode(void *data,
      unsigned width, unsigned height,
      bool fullscreen)
{
   vk_t *vk               = (vk_t*)data;
   if (vk->ctx_driver->set_video_mode)
      vk->ctx_driver->set_video_mode(vk->ctx_data,
            width, height, fullscreen);
}

static void vulkan_set_viewport(void *data, unsigned viewport_width,
      unsigned viewport_height, bool force_full, bool allow_rotate)
{
   float device_aspect       = (float)viewport_width / viewport_height;
   struct video_ortho ortho  = {0, 1, 0, 1, -1, 1};
   settings_t *settings      = config_get_ptr();
   bool video_scale_integer  = settings->bools.video_scale_integer;
   vk_t *vk                  = (vk_t*)data;

   if (vk->ctx_driver->translate_aspect)
      device_aspect         = vk->ctx_driver->translate_aspect(
            vk->ctx_data, viewport_width, viewport_height);

   if (video_scale_integer && !force_full)
   {
      video_viewport_get_scaled_integer(&vk->vp,
            viewport_width, viewport_height,
            video_driver_get_aspect_ratio(),
            vk->flags & VK_FLAG_KEEP_ASPECT,
            true);
      viewport_width  = vk->vp.width;
      viewport_height = vk->vp.height;
   }
   else if ((vk->flags & VK_FLAG_KEEP_ASPECT) && !force_full)
   {
      video_viewport_get_scaled_aspect2(&vk->vp, viewport_width, viewport_height,
            true, device_aspect, video_driver_get_aspect_ratio());
      viewport_width  = vk->vp.width;
      viewport_height = vk->vp.height;
   }
   else
   {
      vk->vp.x      = 0;
      vk->vp.y      = 0;
      vk->vp.width  = viewport_width;
      vk->vp.height = viewport_height;
   }

   if (vk->vp.x < 0)
   {
      vk->translate_x = (float)vk->vp.x * 2;
      vk->vp.x        = 0.0;
   }
   else
      vk->translate_x = 0.0;

   if (vk->vp.y < 0)
   {
      vk->translate_y = (float)vk->vp.y * 2;
      vk->vp.y        = 0.0;
   }
   else
      vk->translate_y = 0.0;

   vulkan_set_projection(vk, &ortho, allow_rotate);

   /* Set last backbuffer viewport. */
   if (!force_full)
   {
      vk->vp_out_width  = viewport_width;
      vk->vp_out_height = viewport_height;
   }

   vk->vk_vp.x          = (float)vk->vp.x;
   vk->vk_vp.y          = (float)vk->vp.y;
   vk->vk_vp.width      = (float)vk->vp.width;
   vk->vk_vp.height     = (float)vk->vp.height;
   vk->vk_vp.minDepth   = 0.0f;
   vk->vk_vp.maxDepth   = 1.0f;

   vk->tracker.dirty |= VULKAN_DIRTY_DYNAMIC_BIT;
}

static void vulkan_readback(vk_t *vk, struct vk_image *readback_image)
{
   VkBufferImageCopy region;
   struct vk_texture *staging;
   struct video_viewport vp;
   VkMemoryBarrier barrier;

   vp.x                                   = 0;
   vp.y                                   = 0;
   vp.width                               = 0;
   vp.height                              = 0;
   vp.full_width                          = 0;
   vp.full_height                         = 0;

   vulkan_viewport_info(vk, &vp);

   region.bufferOffset                    = 0;
   region.bufferRowLength                 = 0;
   region.bufferImageHeight               = 0;
   region.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
   region.imageSubresource.mipLevel       = 0;
   region.imageSubresource.baseArrayLayer = 0;
   region.imageSubresource.layerCount     = 1;
   region.imageOffset.x                   = vp.x;
   region.imageOffset.y                   = vp.y;
   region.imageOffset.z                   = 0;
   region.imageExtent.width               = vp.width + vk->translate_x;
   region.imageExtent.height              = vp.height + vk->translate_y;
   region.imageExtent.depth               = 1;

   staging  = &vk->readback.staging[vk->context->current_frame_index];
   *staging = vulkan_create_texture(vk,
         staging->memory != VK_NULL_HANDLE ? staging : NULL,
         vk->vp.width, vk->vp.height,
         VK_FORMAT_B8G8R8A8_UNORM, /* Formats don't matter for readback since it's a raw copy. */
         NULL, NULL, VULKAN_TEXTURE_READBACK);

   vkCmdCopyImageToBuffer(vk->cmd, readback_image->image,
         VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
         staging->buffer,
         1, &region);

   /* Make the data visible to host. */
   barrier.sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
   barrier.pNext         = NULL;
   barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
   barrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
   vkCmdPipelineBarrier(vk->cmd,
         VK_PIPELINE_STAGE_TRANSFER_BIT,
         VK_PIPELINE_STAGE_HOST_BIT, 0,
         1, &barrier, 0, NULL, 0, NULL);
}

static void vulkan_inject_black_frame(vk_t *vk, video_frame_info_t *video_info)
{
   VkSubmitInfo submit_info;
   VkCommandBufferBeginInfo begin_info;
   const VkImageSubresourceRange range = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
   const VkClearColorValue clear_color = {{ 0.0f, 0.0f, 0.0f, 1.0f }};
   unsigned frame_index                = vk->context->current_frame_index;
   unsigned swapchain_index            = vk->context->current_swapchain_index;
   struct vk_per_frame *chain          = &vk->swapchain[frame_index];
   struct vk_image *backbuffer         = &vk->backbuffers[swapchain_index];
   vk->chain                           = chain;
   vk->cmd                             = chain->cmd;

   begin_info.sType                    = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
   begin_info.pNext                    = NULL;
   begin_info.flags                    = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
   begin_info.pInheritanceInfo         = NULL;
   vkResetCommandBuffer(vk->cmd, 0);
   vkBeginCommandBuffer(vk->cmd, &begin_info);

   VULKAN_IMAGE_LAYOUT_TRANSITION(vk->cmd, backbuffer->image,
         VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
         0, VK_ACCESS_TRANSFER_WRITE_BIT,
         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
         VK_PIPELINE_STAGE_TRANSFER_BIT);

   vkCmdClearColorImage(vk->cmd, backbuffer->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
         &clear_color, 1, &range);

   VULKAN_IMAGE_LAYOUT_TRANSITION(vk->cmd, backbuffer->image,
         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
         VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT,
         VK_PIPELINE_STAGE_TRANSFER_BIT,
         VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

   vkEndCommandBuffer(vk->cmd);

   submit_info.sType                   = VK_STRUCTURE_TYPE_SUBMIT_INFO;
   submit_info.pNext                   = NULL;
   submit_info.waitSemaphoreCount      = 0;
   submit_info.pWaitSemaphores         = NULL;
   submit_info.pWaitDstStageMask       = NULL;
   submit_info.commandBufferCount      = 1;
   submit_info.pCommandBuffers         = &vk->cmd;
   submit_info.signalSemaphoreCount    = 0;
   submit_info.pSignalSemaphores       = NULL;

   if (
            (vk->context->flags & VK_CTX_FLAG_HAS_ACQUIRED_SWAPCHAIN)
         && (vk->context->swapchain_semaphores[swapchain_index] !=
            VK_NULL_HANDLE))
   {
      submit_info.signalSemaphoreCount = 1;
      submit_info.pSignalSemaphores    = &vk->context->swapchain_semaphores[swapchain_index];
   }

   if (     (vk->context->flags & VK_CTX_FLAG_HAS_ACQUIRED_SWAPCHAIN)
         && (vk->context->swapchain_acquire_semaphore != VK_NULL_HANDLE))
   {
      static const VkPipelineStageFlags wait_stage        =
         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

      vk->context->swapchain_wait_semaphores[frame_index] =
         vk->context->swapchain_acquire_semaphore;
      vk->context->swapchain_acquire_semaphore            = VK_NULL_HANDLE;
      submit_info.waitSemaphoreCount                      = 1;
      submit_info.pWaitSemaphores                         = &vk->context->swapchain_wait_semaphores[frame_index];
      submit_info.pWaitDstStageMask                       = &wait_stage;
   }

#ifdef HAVE_THREADS
   slock_lock(vk->context->queue_lock);
#endif
   vkQueueSubmit(vk->context->queue, 1,
         &submit_info, vk->context->swapchain_fences[frame_index]);
   vk->context->swapchain_fences_signalled[frame_index] = true;
#ifdef HAVE_THREADS
   slock_unlock(vk->context->queue_lock);
#endif
}

#if defined(HAVE_MENU)
/* VBO will be written to here. */
static void vulkan_draw_quad(vk_t *vk, const struct vk_draw_quad *quad)
{
   if (quad->texture && quad->texture->image)
      vulkan_transition_texture(vk, vk->cmd, quad->texture);

   if (quad->pipeline != vk->tracker.pipeline)
   {
      VkRect2D sci;
      vkCmdBindPipeline(vk->cmd,
            VK_PIPELINE_BIND_POINT_GRAPHICS, quad->pipeline);

      vk->tracker.pipeline = quad->pipeline;
      /* Changing pipeline invalidates dynamic state. */
      vk->tracker.dirty   |= VULKAN_DIRTY_DYNAMIC_BIT;
      if (vk->flags & VK_FLAG_TRACKER_USE_SCISSOR)
         sci               = vk->tracker.scissor;
      else
      {
         /* No scissor -> viewport */
         sci.offset.x      = vk->vp.x;
         sci.offset.y      = vk->vp.y;
         sci.extent.width  = vk->vp.width;
         sci.extent.height = vk->vp.height;
      }

      vkCmdSetViewport(vk->cmd, 0, 1, &vk->vk_vp);
      vkCmdSetScissor (vk->cmd, 0, 1, &sci);

      vk->tracker.dirty &= ~VULKAN_DIRTY_DYNAMIC_BIT;
   }
   else if (vk->tracker.dirty & VULKAN_DIRTY_DYNAMIC_BIT)
   {
      VkRect2D sci;
      if (vk->flags & VK_FLAG_TRACKER_USE_SCISSOR)
         sci               = vk->tracker.scissor;
      else
      {
         /* No scissor -> viewport */
         sci.offset.x      = vk->vp.x;
         sci.offset.y      = vk->vp.y;
         sci.extent.width  = vk->vp.width;
         sci.extent.height = vk->vp.height;
      }

      vkCmdSetViewport(vk->cmd, 0, 1, &vk->vk_vp);
      vkCmdSetScissor (vk->cmd, 0, 1, &sci);

      vk->tracker.dirty &= ~VULKAN_DIRTY_DYNAMIC_BIT;
   }

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

      {
         struct vk_vertex         *pv = (struct vk_vertex*)range.data;
         const struct vk_color *color = &quad->color;

         VULKAN_WRITE_QUAD_VBO(pv, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, color);
      }

      vkCmdBindVertexBuffers(vk->cmd, 0, 1,
            &range.buffer, &range.offset);
   }

   /* Draw the quad */
   vkCmdDraw(vk->cmd, 6, 1, 0, 0);
}
#endif

static void vulkan_init_render_target(struct vk_image* image, uint32_t width, uint32_t height, VkFormat format, VkRenderPass render_pass, vulkan_context_t* ctx)
{
   VkMemoryRequirements mem_reqs;
   VkImageCreateInfo image_info;
   VkMemoryAllocateInfo alloc;
   VkImageViewCreateInfo view;
   VkFramebufferCreateInfo info;

   memset(image, 0, sizeof(struct vk_image));

   /* Create the image */
   image_info.sType                = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
   image_info.pNext                = NULL;
   image_info.flags                = 0;
   image_info.imageType            = VK_IMAGE_TYPE_2D;
   image_info.format               = format;
   image_info.extent.width         = width;
   image_info.extent.height        = height;
   image_info.extent.depth         = 1;
   image_info.mipLevels            = 1;
   image_info.arrayLayers          = 1;
   image_info.samples              = VK_SAMPLE_COUNT_1_BIT;
   image_info.tiling               = VK_IMAGE_TILING_OPTIMAL;
   image_info.usage                = VK_IMAGE_USAGE_SAMPLED_BIT |
      VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
      VK_IMAGE_USAGE_TRANSFER_DST_BIT |
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
   image_info.sharingMode          = VK_SHARING_MODE_EXCLUSIVE;
   image_info.queueFamilyIndexCount= 0;
   image_info.pQueueFamilyIndices  = NULL;
   image_info.initialLayout        = VK_IMAGE_LAYOUT_UNDEFINED;

   vkCreateImage(ctx->device, &image_info, NULL, &image->image);
   vulkan_debug_mark_image(ctx->device, image->image);
   vkGetImageMemoryRequirements(ctx->device, image->image, &mem_reqs);
   alloc.sType                     = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
   alloc.pNext                     = NULL;
   alloc.allocationSize            = mem_reqs.size;
   alloc.memoryTypeIndex           = vulkan_find_memory_type(
         &ctx->memory_properties,
         mem_reqs.memoryTypeBits,
         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

   vkAllocateMemory(ctx->device, &alloc, NULL, &image->memory);
   vulkan_debug_mark_memory(ctx->device, image->memory);

   vkBindImageMemory(ctx->device, image->image, image->memory, 0);

   /* Create an image view which we can render into. */
   view.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
   view.pNext                           = NULL;
   view.flags                           = 0;
   view.image                           = image->image;
   view.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
   view.format                          = format;
   view.components.r                    = VK_COMPONENT_SWIZZLE_R;
   view.components.g                    = VK_COMPONENT_SWIZZLE_G;
   view.components.b                    = VK_COMPONENT_SWIZZLE_B;
   view.components.a                    = VK_COMPONENT_SWIZZLE_A;
   view.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
   view.subresourceRange.baseMipLevel   = 0;
   view.subresourceRange.levelCount     = 1;
   view.subresourceRange.baseArrayLayer = 0;
   view.subresourceRange.layerCount     = 1;

   vkCreateImageView(ctx->device, &view, NULL, &image->view);

   /* Create the framebuffer */
   info.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
   info.pNext           = NULL;
   info.flags           = 0;
   info.renderPass      = render_pass;
   info.attachmentCount = 1;
   info.pAttachments    = &image->view;
   info.width           = ctx->swapchain_width;
   info.height          = ctx->swapchain_height;
   info.layers          = 1;

   vkCreateFramebuffer(ctx->device, &info, NULL, &image->framebuffer);
}

static void vulkan_run_hdr_pipeline(VkPipeline pipeline, VkRenderPass render_pass, const struct vk_image* source_image, struct vk_image* render_target, vk_t* vk)
{
   vulkan_hdr_uniform_t* mapped_ubo = (vulkan_hdr_uniform_t*)vk->hdr.ubo.mapped;
   VkRenderPassBeginInfo rp_info;
   VkClearValue clear_color;

   mapped_ubo->mvp                  = vk->mvp_no_rot;

   rp_info.sType                    = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
   rp_info.pNext                    = NULL;
   rp_info.renderPass               = render_pass;
   rp_info.framebuffer              = render_target->framebuffer;
   rp_info.renderArea.offset.x      = 0;
   rp_info.renderArea.offset.y      = 0;
   rp_info.renderArea.extent.width  = vk->context->swapchain_width;
   rp_info.renderArea.extent.height = vk->context->swapchain_height;
   rp_info.clearValueCount          = 1;
   rp_info.pClearValues             = &clear_color;

   clear_color.color.float32[0]     = 0.0f;
   clear_color.color.float32[1]     = 0.0f;
   clear_color.color.float32[2]     = 0.0f;
   clear_color.color.float32[3]     = 0.0f;

   /* Begin render pass and set up viewport */
   vkCmdBeginRenderPass(vk->cmd, &rp_info, VK_SUBPASS_CONTENTS_INLINE);

   {
      if (pipeline != vk->tracker.pipeline)
      {
         vkCmdBindPipeline(vk->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

         vk->tracker.pipeline = pipeline;
         /* Changing pipeline invalidates dynamic state. */
         vk->tracker.dirty   |= VULKAN_DIRTY_DYNAMIC_BIT;
      }
   }

   {
      VkWriteDescriptorSet write;
      VkDescriptorImageInfo image_info;
      VkDescriptorSet set = vulkan_descriptor_manager_alloc(
            vk->context->device,
            &vk->chain->descriptor_manager);

      VULKAN_SET_UNIFORM_BUFFER(vk->context->device,
            set,
            0,
            vk->hdr.ubo.buffer,
            0,
            vk->hdr.ubo.size);

      image_info.sampler              = vk->samplers.nearest;
      image_info.imageView            = source_image->view;
      image_info.imageLayout          = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

      write.sType                     = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      write.pNext                     = NULL;
      write.dstSet                    = set;
      write.dstBinding                = 2;
      write.dstArrayElement           = 0;
      write.descriptorCount           = 1;
      write.descriptorType            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      write.pImageInfo                = &image_info;
      write.pBufferInfo               = NULL;
      write.pTexelBufferView          = NULL;

      vkUpdateDescriptorSets(vk->context->device, 1, &write, 0, NULL);

      vkCmdBindDescriptorSets(vk->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
            vk->pipelines.layout, 0,
            1, &set, 0, NULL);

      vk->tracker.view    = source_image->view;
      vk->tracker.sampler = vk->samplers.nearest;
   }

   {
      VkViewport viewport;
      VkRect2D sci;

      viewport.x             = 0.0f;
      viewport.y             = 0.0f;
      viewport.width         = vk->context->swapchain_width;
      viewport.height        = vk->context->swapchain_height;
      viewport.minDepth      = 0.0f;
      viewport.maxDepth      = 1.0f;

      sci.offset.x           = (int32_t)viewport.x;
      sci.offset.y           = (int32_t)viewport.y;
      sci.extent.width       = (uint32_t)viewport.width;
      sci.extent.height      = (uint32_t)viewport.height;
      vkCmdSetViewport(vk->cmd, 0, 1, &viewport);
      vkCmdSetScissor(vk->cmd,  0, 1, &sci);
   }

   /* Upload VBO */
   {
      struct vk_buffer_range range;

      vulkan_buffer_chain_alloc(vk->context, &vk->chain->vbo, 6 * sizeof(struct vk_vertex), &range);

      {
         struct vk_vertex  *pv = (struct vk_vertex*)range.data;
         struct vk_color   color;

         color.r = 1.0f;
         color.g = 1.0f;
         color.b = 1.0f;
         color.a = 1.0f;

         VULKAN_WRITE_QUAD_VBO(pv, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, &color);
      }

      vkCmdBindVertexBuffers(vk->cmd, 0, 1,
            &range.buffer, &range.offset);
   }

   vkCmdDraw(vk->cmd, 6, 1, 0, 0);

   vkCmdEndRenderPass(vk->cmd);
}

static bool vulkan_frame(void *data, const void *frame,
      unsigned frame_width, unsigned frame_height,
      uint64_t frame_count,
      unsigned pitch, const char *msg, video_frame_info_t *video_info)
{
   int i, j, k;
   VkSubmitInfo submit_info;
   VkClearValue clear_color;
   VkRenderPassBeginInfo rp_info;
   VkCommandBufferBeginInfo begin_info;
   VkSemaphore signal_semaphores[2];
   vk_t *vk                                      = (vk_t*)data;
   bool waits_for_semaphores                     = false;
   unsigned width                                = video_info->width;
   unsigned height                               = video_info->height;
   bool statistics_show                          = video_info->statistics_show;
   const char *stat_text                         = video_info->stat_text;
   unsigned black_frame_insertion                = video_info->black_frame_insertion;
   int bfi_light_frames;
   unsigned n;
   bool input_driver_nonblock_state              = video_info->input_driver_nonblock_state;
   bool runloop_is_slowmotion                    = video_info->runloop_is_slowmotion;
   bool runloop_is_paused                        = video_info->runloop_is_paused;
   unsigned video_width                          = video_info->width;
   unsigned video_height                         = video_info->height;
   struct font_params *osd_params                = (struct font_params*)
      &video_info->osd_stat_params;
#ifdef HAVE_MENU
   bool menu_is_alive                            = (video_info->menu_st_flags & MENU_ST_FLAG_ALIVE) ? true : false;
#endif
#ifdef HAVE_GFX_WIDGETS
   bool widgets_active                           = video_info->widgets_active;
#endif
   unsigned frame_index                          =
      vk->context->current_frame_index;
   unsigned swapchain_index                      =
      vk->context->current_swapchain_index;
   bool overlay_behind_menu                      = video_info->overlay_behind_menu;

#ifdef VULKAN_HDR_SWAPCHAIN
   bool use_main_buffer                          =
         ( vk->context->flags & VK_CTX_FLAG_HDR_ENABLE)
      && (!vk->filter_chain || !vulkan_filter_chain_emits_hdr10(vk->filter_chain));
#endif /* VULKAN_HDR_SWAPCHAIN */

   /* Bookkeeping on start of frame. */
   struct vk_per_frame *chain                    = &vk->swapchain[frame_index];
   struct vk_image *backbuffer                   = &vk->backbuffers[swapchain_index];
   struct vk_descriptor_manager *manager         = &chain->descriptor_manager;
   struct vk_buffer_chain *buff_chain_vbo        = &chain->vbo;
   struct vk_buffer_chain *buff_chain_ubo        = &chain->ubo;

   vk->chain                                     = chain;
   vk->backbuffer                                = backbuffer;

   VK_DESCRIPTOR_MANAGER_RESTART(manager);
   VK_BUFFER_CHAIN_DISCARD(buff_chain_vbo);
   VK_BUFFER_CHAIN_DISCARD(buff_chain_ubo);

   /* Start recording the command buffer. */
   vk->cmd                                       = chain->cmd;

   begin_info.sType                              =
      VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
   begin_info.pNext                              = NULL;
   begin_info.flags                              =
      VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
   begin_info.pInheritanceInfo                   = NULL;

   vkResetCommandBuffer(vk->cmd, 0);

   vkBeginCommandBuffer(vk->cmd, &begin_info);

   vk->tracker.dirty                 = 0;
   vk->tracker.scissor.offset.x      = 0;
   vk->tracker.scissor.offset.y      = 0;
   vk->tracker.scissor.extent.width  = 0;
   vk->tracker.scissor.extent.height = 0;
   vk->flags                        &= ~VK_FLAG_TRACKER_USE_SCISSOR;
   vk->tracker.pipeline              = VK_NULL_HANDLE;
   vk->tracker.view                  = VK_NULL_HANDLE;
   vk->tracker.sampler               = VK_NULL_HANDLE;
   for (i = 0; i < 16; i++)
      vk->tracker.mvp.data[i]        = 0.0f;

   waits_for_semaphores              =
          (vk->flags & VK_FLAG_HW_ENABLE)
       && frame
       && !vk->hw.num_cmd
       && (vk->flags & VK_FLAG_HW_VALID_SEMAPHORE);

   if (   waits_for_semaphores
       && (vk->hw.src_queue_family != VK_QUEUE_FAMILY_IGNORED)
       && (vk->hw.src_queue_family != vk->context->graphics_queue_index))
   {
      /* Acquire ownership of image from other queue family. */
      VULKAN_TRANSFER_IMAGE_OWNERSHIP(vk->cmd,
            vk->hw.image->create_info.image,
            vk->hw.image->image_layout,
            /* Create a dependency chain from semaphore wait. */
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            vk->hw.src_queue_family, vk->context->graphics_queue_index);
   }

   /* Upload texture */
   if (frame && (!(vk->flags & VK_FLAG_HW_ENABLE)))
   {
      unsigned y;
      uint8_t *dst        = NULL;
      const uint8_t *src  = (const uint8_t*)frame;
      unsigned bpp        = vk->video.rgb32 ? 4 : 2;

      if (     chain->texture.width  != frame_width
            || chain->texture.height != frame_height)
      {
         chain->texture = vulkan_create_texture(vk, &chain->texture,
               frame_width, frame_height, chain->texture.format, NULL, NULL,
               chain->texture_optimal.memory
               ? VULKAN_TEXTURE_STAGING : VULKAN_TEXTURE_STREAMED);

         {
            struct vk_texture *texture = &chain->texture;
            VK_MAP_PERSISTENT_TEXTURE(vk->context->device, texture);
         }

         if (chain->texture.type == VULKAN_TEXTURE_STAGING)
            chain->texture_optimal = vulkan_create_texture(
                  vk,
                  &chain->texture_optimal,
                  frame_width, frame_height,
                  chain->texture.format, /* Ensure we use the original format and not any remapped format. */
                  NULL, NULL, VULKAN_TEXTURE_DYNAMIC);
      }

      if (frame != chain->texture.mapped)
      {
         dst = (uint8_t*)chain->texture.mapped;
         if (     (chain->texture.stride == pitch )
               && pitch == frame_width * bpp)
            memcpy(dst, src, frame_width * frame_height * bpp);
         else
            for (y = 0; y < frame_height; y++,
                  dst += chain->texture.stride, src += pitch)
               memcpy(dst, src, frame_width * bpp);
      }

      VULKAN_SYNC_TEXTURE_TO_GPU_COND_OBJ(vk, chain->texture);

      /* If we have an optimal texture, copy to that now. */
      if (chain->texture_optimal.memory != VK_NULL_HANDLE)
      {
         struct vk_texture *dynamic = &chain->texture_optimal;
         struct vk_texture *staging = &chain->texture;
         vulkan_copy_staging_to_dynamic(vk, vk->cmd, dynamic, staging);
      }

      vk->last_valid_index = frame_index;
   }

   /* Notify filter chain about the new sync index. */
   vulkan_filter_chain_notify_sync_index(
         (vulkan_filter_chain_t*)vk->filter_chain, frame_index);
   vulkan_filter_chain_set_frame_count(
         (vulkan_filter_chain_t*)vk->filter_chain, frame_count);

   /* Sub-frame info for multiframe shaders (per real content frame).
      Should always be 1 for non-use of subframes*/
   if (!(vk->context->flags & VK_CTX_FLAG_SWAP_INTERVAL_EMULATION_LOCK))
   {
     if (     black_frame_insertion
           || input_driver_nonblock_state
           || runloop_is_slowmotion
           || runloop_is_paused
           || (vk->context->swap_interval > 1)
           || (vk->flags & VK_FLAG_MENU_ENABLE))
        vulkan_filter_chain_set_shader_subframes(
           (vulkan_filter_chain_t*)vk->filter_chain, 1);
     else
        vulkan_filter_chain_set_shader_subframes(
           (vulkan_filter_chain_t*)vk->filter_chain, video_info->shader_subframes);

     vulkan_filter_chain_set_current_shader_subframe(
           (vulkan_filter_chain_t*)vk->filter_chain, 1);
   }

#ifdef VULKAN_ROLLING_SCANLINE_SIMULATION
   if (      (video_info->shader_subframes > 1)
         &&  (video_info->scan_subframes)
         &&  (backbuffer->image != VK_NULL_HANDLE)
         &&  !black_frame_insertion
         &&  !input_driver_nonblock_state
         &&  !runloop_is_slowmotion
         &&  !runloop_is_paused
         &&  (!(vk->flags & VK_FLAG_MENU_ENABLE))
         &&  !(vk->context->swap_interval > 1))
      vulkan_filter_chain_set_simulate_scanline(
            (vulkan_filter_chain_t*)vk->filter_chain, true);
   else
      vulkan_filter_chain_set_simulate_scanline(
            (vulkan_filter_chain_t*)vk->filter_chain, false);
#endif /* VULKAN_ROLLING_SCANLINE_SIMULATION */

#ifdef HAVE_REWIND
   vulkan_filter_chain_set_frame_direction(
         (vulkan_filter_chain_t*)vk->filter_chain,
         state_manager_frame_is_reversed() ? -1 : 1);
#else
   vulkan_filter_chain_set_frame_direction(
         (vulkan_filter_chain_t*)vk->filter_chain,
         1);
#endif
   vulkan_filter_chain_set_rotation(
         (vulkan_filter_chain_t*)vk->filter_chain, retroarch_get_rotation());

   /* Render offscreen filter chain passes. */
   {
      /* Set the source texture in the filter chain */
      struct vulkan_filter_chain_texture input;

      if (vk->flags & VK_FLAG_HW_ENABLE)
      {
         /* Does this make that this can happen at all? */
         if (vk->hw.image && vk->hw.image->create_info.image)
         {
            if (frame)
            {
               input.width     = frame_width;
               input.height    = frame_height;
            }
            else
            {
               input.width     = vk->hw.last_width;
               input.height    = vk->hw.last_height;
            }

            input.image        = vk->hw.image->create_info.image;
            input.view         = vk->hw.image->image_view;
            input.layout       = vk->hw.image->image_layout;

            /* The format can change on a whim. */
            input.format       = vk->hw.image->create_info.format;
         }
         else
         {
            /* Fall back to the default, black texture.
             * This can happen if we restart the video
             * driver while in the menu. */
            input.width        = vk->default_texture.width;
            input.height       = vk->default_texture.height;
            input.image        = vk->default_texture.image;
            input.view         = vk->default_texture.view;
            input.layout       = vk->default_texture.layout;
            input.format       = vk->default_texture.format;
         }

         vk->hw.last_width     = input.width;
         vk->hw.last_height    = input.height;
      }
      else
      {
         struct vk_texture *tex = &vk->swapchain[vk->last_valid_index].texture;
         if (vk->swapchain[vk->last_valid_index].texture_optimal.memory
               != VK_NULL_HANDLE)
            tex = &vk->swapchain[vk->last_valid_index].texture_optimal;
         else if (tex->image)
            vulkan_transition_texture(vk, vk->cmd, tex);

         input.image  = tex->image;
         input.view   = tex->view;
         input.layout = tex->layout;
         input.width  = tex->width;
         input.height = tex->height;
         input.format = VK_FORMAT_UNDEFINED; /* It's already configured. */
      }

      vulkan_filter_chain_set_input_texture((vulkan_filter_chain_t*)
            vk->filter_chain, &input);
   }

   vulkan_set_viewport(vk, width, height, false, true);

   vulkan_filter_chain_build_offscreen_passes(
         (vulkan_filter_chain_t*)vk->filter_chain,
         vk->cmd, &vk->vk_vp);

#if defined(HAVE_MENU)
   /* Upload menu texture. */
   if (vk->flags & VK_FLAG_MENU_ENABLE)
   {
       if (   vk->menu.textures[vk->menu.last_index].image  != VK_NULL_HANDLE
           || vk->menu.textures[vk->menu.last_index].buffer != VK_NULL_HANDLE)
       {
           struct vk_texture *optimal = &vk->menu.textures_optimal[vk->menu.last_index];
           struct vk_texture *texture = &vk->menu.textures[vk->menu.last_index];

           if (optimal->memory != VK_NULL_HANDLE)
           {
               if (vk->menu.dirty[vk->menu.last_index])
               {
                  struct vk_texture *dynamic = optimal;
                  struct vk_texture *staging = texture;
                  VULKAN_SYNC_TEXTURE_TO_GPU_COND_PTR(vk, staging);
                  vulkan_copy_staging_to_dynamic(vk, vk->cmd,
                        dynamic, staging);
                  vk->menu.dirty[vk->menu.last_index] = false;
               }
           }
       }
   }
#endif

#ifdef VULKAN_HDR_SWAPCHAIN
   if (use_main_buffer)
      backbuffer = &vk->main_buffer;
#endif /* VULKAN_HDR_SWAPCHAIN */

   /* Render to backbuffer. */
   if (     (backbuffer->image != VK_NULL_HANDLE)
         && (vk->context->flags & VK_CTX_FLAG_HAS_ACQUIRED_SWAPCHAIN))
   {
      rp_info.sType                    = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
      rp_info.pNext                    = NULL;
      rp_info.renderPass               = vk->render_pass;
      rp_info.framebuffer              = backbuffer->framebuffer;
      rp_info.renderArea.offset.x      = 0;
      rp_info.renderArea.offset.y      = 0;
      rp_info.renderArea.extent.width  = vk->context->swapchain_width;
      rp_info.renderArea.extent.height = vk->context->swapchain_height;
      rp_info.clearValueCount          = 1;
      rp_info.pClearValues             = &clear_color;

      clear_color.color.float32[0]     = 0.0f;
      clear_color.color.float32[1]     = 0.0f;
      clear_color.color.float32[2]     = 0.0f;
      clear_color.color.float32[3]     = 0.0f;

      /* Begin render pass and set up viewport */
      vkCmdBeginRenderPass(vk->cmd, &rp_info, VK_SUBPASS_CONTENTS_INLINE);

      vulkan_filter_chain_build_viewport_pass(
            (vulkan_filter_chain_t*)vk->filter_chain, vk->cmd,
            &vk->vk_vp, vk->mvp.data);

#ifdef HAVE_OVERLAY
      if ((vk->flags & VK_FLAG_OVERLAY_ENABLE) && overlay_behind_menu)
         vulkan_render_overlay(vk, video_width, video_height);
#endif

#if defined(HAVE_MENU)
      if (vk->flags & VK_FLAG_MENU_ENABLE)
      {
         menu_driver_frame(menu_is_alive, video_info);

         if (vk->menu.textures[vk->menu.last_index].image  != VK_NULL_HANDLE ||
             vk->menu.textures[vk->menu.last_index].buffer != VK_NULL_HANDLE)
         {
            struct vk_draw_quad quad;
            struct vk_texture *optimal = &vk->menu.textures_optimal[vk->menu.last_index];
            settings_t *settings       = config_get_ptr();
            bool menu_linear_filter    = settings->bools.menu_linear_filter;

            vulkan_set_viewport(vk, width, height, ((vk->flags &
                     VK_FLAG_MENU_FULLSCREEN) > 0), false);

            quad.pipeline              = vk->pipelines.alpha_blend;
            quad.texture               = &vk->menu.textures[vk->menu.last_index];

            if (optimal->memory != VK_NULL_HANDLE)
               quad.texture = optimal;

            if (menu_linear_filter)
               quad.sampler = (optimal->flags & VK_TEX_FLAG_MIPMAP)
                  ? vk->samplers.mipmap_linear : vk->samplers.linear;
            else
               quad.sampler = (optimal->flags & VK_TEX_FLAG_MIPMAP)
                  ? vk->samplers.mipmap_nearest : vk->samplers.nearest;

            quad.mvp        = &vk->mvp_menu;
            quad.color.r    = 1.0f;
            quad.color.g    = 1.0f;
            quad.color.b    = 1.0f;
            quad.color.a    = vk->menu.alpha;
            vulkan_draw_quad(vk, &quad);
         }
      }
      else if (statistics_show)
      {
         if (osd_params)
            font_driver_render_msg(vk,
                  stat_text,
                  osd_params, NULL);
      }
#endif

#ifdef HAVE_OVERLAY
      if ((vk->flags & VK_FLAG_OVERLAY_ENABLE) && !overlay_behind_menu)
         vulkan_render_overlay(vk, video_width, video_height);
#endif

      if (!string_is_empty(msg))
         font_driver_render_msg(vk, msg, NULL, NULL);

#ifdef HAVE_GFX_WIDGETS
      if (widgets_active)
         gfx_widgets_frame(video_info);
#endif

      /* End the render pass. We're done rendering to backbuffer now. */
      vkCmdEndRenderPass(vk->cmd);

#ifdef VULKAN_HDR_SWAPCHAIN
      /* Copy over back buffer to swap chain render targets */
      if (use_main_buffer)
      {
         backbuffer = &vk->backbuffers[swapchain_index];
         /* Prepare source buffer for reading */
         VULKAN_IMAGE_LAYOUT_TRANSITION(vk->cmd, vk->main_buffer.image,
               VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
               VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
               VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
               VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

         vulkan_run_hdr_pipeline(vk->pipelines.hdr, vk->render_pass, &vk->main_buffer, backbuffer, vk);
      }
#endif /* VULKAN_HDR_SWAPCHAIN */
   }

   /* End the filter chain frame.
    * This must happen outside a render pass.
    */
   vulkan_filter_chain_end_frame((vulkan_filter_chain_t*)vk->filter_chain, vk->cmd);

   if (
            (backbuffer->image != VK_NULL_HANDLE)
         && (vk->context->flags & VK_CTX_FLAG_HAS_ACQUIRED_SWAPCHAIN)
      )
   {
      if (     (vk->flags & VK_FLAG_READBACK_PENDING)
            || (vk->flags & VK_FLAG_READBACK_STREAMED))
      {
         VkImageLayout backbuffer_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
#ifdef VULKAN_HDR_SWAPCHAIN
         struct vk_image* readback_source = backbuffer;
         if (vk->context->flags & VK_CTX_FLAG_HDR_ENABLE)
         {
            if (use_main_buffer)
            {
               /* Read directly from sdr main buffer instead of tonemapping */
               readback_source = &vk->main_buffer;
               /* No need to transition layout, it's already read-only optimal */
            }
            else
            {
               /* Prepare backbuffer for reading */
               backbuffer_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
               VULKAN_IMAGE_LAYOUT_TRANSITION(vk->cmd, backbuffer->image,
                  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, backbuffer_layout,
                  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                  VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                  VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
            }
            vulkan_run_hdr_pipeline(vk->pipelines.hdr_to_sdr, vk->readback_render_pass, readback_source, &vk->readback_image, vk);
            readback_source = &vk->readback_image;
         }
#endif /* VULKAN_HDR_SWAPCHAIN */
         /* We cannot safely read back from an image which
          * has already been presented as we need to
          * maintain the PRESENT_SRC_KHR layout.
          *
          * If we're reading back,
          * perform the readback before presenting.
          */
         VULKAN_IMAGE_LAYOUT_TRANSITION(
               vk->cmd,
               backbuffer->image,
               backbuffer_layout,
               VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
               VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
               VK_ACCESS_TRANSFER_READ_BIT,
               VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
               VK_PIPELINE_STAGE_TRANSFER_BIT);

         vulkan_readback(vk, readback_source);

         /* Prepare for presentation after transfers are complete. */
         VULKAN_IMAGE_LAYOUT_TRANSITION(
               vk->cmd,
               backbuffer->image,
               VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
               VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
               0,
               VK_ACCESS_MEMORY_READ_BIT,
               VK_PIPELINE_STAGE_TRANSFER_BIT,
               VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

         vk->flags &= ~VK_FLAG_READBACK_PENDING;
      }
      else
      {
         /* Prepare backbuffer for presentation. */
         VULKAN_IMAGE_LAYOUT_TRANSITION(
               vk->cmd,
               backbuffer->image,
               VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
               VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
               VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
               0,
               VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
               VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
      }
   }

   if (    waits_for_semaphores
       && (vk->hw.src_queue_family != VK_QUEUE_FAMILY_IGNORED)
       && (vk->hw.src_queue_family != vk->context->graphics_queue_index))
   {
      /* Release ownership of image back to other queue family. */
      VULKAN_TRANSFER_IMAGE_OWNERSHIP(vk->cmd,
            vk->hw.image->create_info.image,
            vk->hw.image->image_layout,
            VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            vk->context->graphics_queue_index, vk->hw.src_queue_family);
   }

   vkEndCommandBuffer(vk->cmd);

   /* Submit command buffers to GPU. */
   submit_info.sType                 = VK_STRUCTURE_TYPE_SUBMIT_INFO;
   submit_info.pNext                 = NULL;

   if (vk->hw.num_cmd)
   {
      /* vk->hw.cmd has already been allocated for this. */
      vk->hw.cmd[vk->hw.num_cmd]     = vk->cmd;

      submit_info.commandBufferCount = vk->hw.num_cmd + 1;
      submit_info.pCommandBuffers    = vk->hw.cmd;

      vk->hw.num_cmd                 = 0;
   }
   else
   {
      submit_info.commandBufferCount = 1;
      submit_info.pCommandBuffers    = &vk->cmd;
   }

   if (waits_for_semaphores)
   {
      submit_info.waitSemaphoreCount = vk->hw.num_semaphores;
      submit_info.pWaitSemaphores    = vk->hw.semaphores;
      submit_info.pWaitDstStageMask  = vk->hw.wait_dst_stages;

      /* Consume the semaphores. */
      vk->flags                     &= ~VK_FLAG_HW_VALID_SEMAPHORE;

      /* We allocated space for this. */
      if (    (vk->context->flags & VK_CTX_FLAG_HAS_ACQUIRED_SWAPCHAIN)
           && (vk->context->swapchain_acquire_semaphore != VK_NULL_HANDLE))
      {
         vk->context->swapchain_wait_semaphores[frame_index]    =
            vk->context->swapchain_acquire_semaphore;
         vk->context->swapchain_acquire_semaphore               = VK_NULL_HANDLE;

         vk->hw.semaphores[submit_info.waitSemaphoreCount]      = vk->context->swapchain_wait_semaphores[frame_index];
         vk->hw.wait_dst_stages[submit_info.waitSemaphoreCount] = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
         submit_info.waitSemaphoreCount++;
      }
   }
   else if ((vk->context->flags & VK_CTX_FLAG_HAS_ACQUIRED_SWAPCHAIN)
         && (vk->context->swapchain_acquire_semaphore != VK_NULL_HANDLE))
   {
      static const VkPipelineStageFlags wait_stage        =
         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

      vk->context->swapchain_wait_semaphores[frame_index] =
         vk->context->swapchain_acquire_semaphore;
      vk->context->swapchain_acquire_semaphore            = VK_NULL_HANDLE;

      submit_info.waitSemaphoreCount = 1;
      submit_info.pWaitSemaphores    = &vk->context->swapchain_wait_semaphores[frame_index];
      submit_info.pWaitDstStageMask  = &wait_stage;
   }
   else
   {
      submit_info.waitSemaphoreCount = 0;
      submit_info.pWaitSemaphores    = NULL;
      submit_info.pWaitDstStageMask  = NULL;
   }

   submit_info.signalSemaphoreCount  = 0;

   if ((vk->context->swapchain_semaphores[swapchain_index]
         != VK_NULL_HANDLE)
         && (vk->context->flags & VK_CTX_FLAG_HAS_ACQUIRED_SWAPCHAIN))
      signal_semaphores[submit_info.signalSemaphoreCount++] = vk->context->swapchain_semaphores[swapchain_index];

   if (vk->hw.signal_semaphore != VK_NULL_HANDLE)
   {
      signal_semaphores[submit_info.signalSemaphoreCount++] = vk->hw.signal_semaphore;
      vk->hw.signal_semaphore = VK_NULL_HANDLE;
   }
   submit_info.pSignalSemaphores = submit_info.signalSemaphoreCount ? signal_semaphores : NULL;

#ifdef HAVE_THREADS
   slock_lock(vk->context->queue_lock);
#endif
   vkQueueSubmit(vk->context->queue, 1,
         &submit_info, vk->context->swapchain_fences[frame_index]);
   vk->context->swapchain_fences_signalled[frame_index] = true;
#ifdef HAVE_THREADS
   slock_unlock(vk->context->queue_lock);
#endif

   if (vk->ctx_driver->swap_buffers)
      vk->ctx_driver->swap_buffers(vk->ctx_data);

   if (!(vk->context->flags & VK_CTX_FLAG_SWAP_INTERVAL_EMULATION_LOCK))
   {
      if (vk->ctx_driver->update_window_title)
         vk->ctx_driver->update_window_title(vk->ctx_data);
   }

   /* Handle spurious swapchain invalidations as soon as we can,
    * i.e. right after swap buffers. */
#ifdef VULKAN_HDR_SWAPCHAIN
   bool video_hdr_enable = video_driver_supports_hdr() && video_info->hdr_enable;
   if (       (vk->flags & VK_FLAG_SHOULD_RESIZE)
         || (((vk->context->flags & VK_CTX_FLAG_HDR_ENABLE) > 0)
         != video_hdr_enable))
#else
   if (vk->flags & VK_FLAG_SHOULD_RESIZE)
#endif /* VULKAN_HDR_SWAPCHAIN */
   {
#ifdef VULKAN_HDR_SWAPCHAIN
      if (video_hdr_enable)
      {
         vk->context->flags |= VK_CTX_FLAG_HDR_ENABLE;
#ifdef HAVE_THREADS
         slock_lock(vk->context->queue_lock);
#endif
         vkQueueWaitIdle(vk->context->queue);
#ifdef HAVE_THREADS
         slock_unlock(vk->context->queue_lock);
#endif
         vulkan_destroy_hdr_buffer(vk->context->device, &vk->main_buffer);
         vulkan_destroy_hdr_buffer(vk->context->device, &vk->readback_image);
      }
      else
         vk->context->flags &= ~VK_CTX_FLAG_HDR_ENABLE;

#endif /* VULKAN_HDR_SWAPCHAIN */

      gfx_ctx_mode_t mode;
      mode.width  = width;
      mode.height = height;

      if (vk->ctx_driver->set_resize)
         vk->ctx_driver->set_resize(vk->ctx_data, mode.width, mode.height);

#ifdef VULKAN_HDR_SWAPCHAIN
      if (vk->context->flags & VK_CTX_FLAG_HDR_ENABLE)
      {
         /* Create intermediary buffer to render filter chain output to */
         vulkan_init_render_target(&vk->main_buffer, video_width, video_height,
                                  vk->context->swapchain_format, vk->render_pass, vk->context);
         /* Create image for readback target in bgra8 format */
         vulkan_init_render_target(&vk->readback_image, video_width, video_height,
                                    VK_FORMAT_B8G8R8A8_UNORM, vk->readback_render_pass, vk->context);
      }
#endif /* VULKAN_HDR_SWAPCHAIN */
      vk->flags &= ~VK_FLAG_SHOULD_RESIZE;
   }

   if (vk->context->flags & VK_CTX_FLAG_INVALID_SWAPCHAIN)
      vulkan_check_swapchain(vk);

   /* Disable BFI during fast forward, slow-motion,
    * pause, and menu to prevent flicker. */
   if (
            (backbuffer->image != VK_NULL_HANDLE)
         && (vk->context->flags & VK_CTX_FLAG_HAS_ACQUIRED_SWAPCHAIN)
         && black_frame_insertion
         && !input_driver_nonblock_state
         && !runloop_is_slowmotion
         && !runloop_is_paused
         && !(vk->context->swap_interval > 1)
         && !(video_info->shader_subframes > 1)
         && (!(vk->flags & VK_FLAG_MENU_ENABLE)))
   {
      if (video_info->bfi_dark_frames > video_info->black_frame_insertion)
         video_info->bfi_dark_frames = video_info->black_frame_insertion;

      /* BFI now handles variable strobe strength, like on-off-off, vs on-on-off for 180hz.
         This needs to be done with duping frames instead of increased swap intervals for
         a couple reasons. Swap interval caps out at 4 in most all apis as of coding,
         and seems to be flat ignored >1 at least in modern Windows for some older APIs. */
      bfi_light_frames = video_info->black_frame_insertion - video_info->bfi_dark_frames;
      if (bfi_light_frames > 0 && !(vk->context->flags & VK_CTX_FLAG_SWAP_INTERVAL_EMULATION_LOCK))
      {
         vk->context->flags |= VK_CTX_FLAG_SWAP_INTERVAL_EMULATION_LOCK;
         while (bfi_light_frames > 0)
         {
            if (!(vulkan_frame(vk, NULL, 0, 0, frame_count, 0, msg, video_info)))
            {
               vk->context->flags &= ~VK_CTX_FLAG_SWAP_INTERVAL_EMULATION_LOCK;
               return false;
            }
            --bfi_light_frames;
         }
         vk->context->flags &= ~VK_CTX_FLAG_SWAP_INTERVAL_EMULATION_LOCK;
      }

      for (n = 0; n < video_info->bfi_dark_frames; ++n)
      {
         if (!(vk->context->flags & VK_CTX_FLAG_SWAP_INTERVAL_EMULATION_LOCK))
         {
            vulkan_inject_black_frame(vk, video_info);
            if (vk->ctx_driver->swap_buffers)
               vk->ctx_driver->swap_buffers(vk->ctx_data);
         }
      }
   }

   /* Frame duping for Shader Subframes, don't combine with swap_interval > 1, BFI.
      Also, a major logical use of shader sub-frames will still be shader implemented BFI
      or even rolling scan bfi, so we need to protect the menu/ff/etc from bad flickering
      from improper settings, and unnecessary performance overhead for ff, screenshots etc. */
   if (      (video_info->shader_subframes > 1)
         &&  (backbuffer->image != VK_NULL_HANDLE)
         &&  (vk->context->flags & VK_CTX_FLAG_HAS_ACQUIRED_SWAPCHAIN)
         &&  !black_frame_insertion
         &&  !input_driver_nonblock_state
         &&  !runloop_is_slowmotion
         &&  !runloop_is_paused
         &&  (!(vk->flags & VK_FLAG_MENU_ENABLE))
         &&  !(vk->context->swap_interval > 1)
         &&  (!(vk->context->flags & VK_CTX_FLAG_SWAP_INTERVAL_EMULATION_LOCK)))
   {
      vk->context->flags |= VK_CTX_FLAG_SWAP_INTERVAL_EMULATION_LOCK;
      for (j = 1; j < (int) video_info->shader_subframes; j++)
      {
         vulkan_filter_chain_set_shader_subframes(
               (vulkan_filter_chain_t*)vk->filter_chain, video_info->shader_subframes);
         vulkan_filter_chain_set_current_shader_subframe(
               (vulkan_filter_chain_t*)vk->filter_chain, j+1);
         if (!vulkan_frame(vk, NULL, 0, 0, frame_count, 0, msg,
                  video_info))
         {
            vk->context->flags &= ~VK_CTX_FLAG_SWAP_INTERVAL_EMULATION_LOCK;
            return false;
         }
      }
      vk->context->flags &= ~VK_CTX_FLAG_SWAP_INTERVAL_EMULATION_LOCK;
   }


   /* Vulkan doesn't directly support swap_interval > 1,
    * so we fake it by duping out more frames. Shader subframes
      uses same concept but split above so sub_frame logic the
      same as the other apis that do support real swap_interval  */
   if (      (vk->context->swap_interval > 1)
         &&  !(video_info->shader_subframes > 1)
         &&  !black_frame_insertion
         &&  (!(vk->context->flags & VK_CTX_FLAG_SWAP_INTERVAL_EMULATION_LOCK)))
   {
      vk->context->flags |= VK_CTX_FLAG_SWAP_INTERVAL_EMULATION_LOCK;
      for (k = 1; k < (int) vk->context->swap_interval; k++)
      {
         if (!vulkan_frame(vk, NULL, 0, 0, frame_count, 0, msg,
                  video_info))
         {
            vk->context->flags &= ~VK_CTX_FLAG_SWAP_INTERVAL_EMULATION_LOCK;
            return false;
         }
      }
      vk->context->flags &= ~VK_CTX_FLAG_SWAP_INTERVAL_EMULATION_LOCK;
   }

   return true;
}

static void vulkan_set_aspect_ratio(void *data, unsigned aspect_ratio_idx)
{
   vk_t *vk = (vk_t*)data;
   if (vk)
      vk->flags |= VK_FLAG_KEEP_ASPECT | VK_FLAG_SHOULD_RESIZE;
}

static void vulkan_apply_state_changes(void *data)
{
   vk_t *vk = (vk_t*)data;
   if (vk)
      vk->flags |= VK_FLAG_SHOULD_RESIZE;
}

static void vulkan_show_mouse(void *data, bool state)
{
   vk_t                            *vk = (vk_t*)data;

   if (vk && vk->ctx_driver->show_mouse)
      vk->ctx_driver->show_mouse(vk->ctx_data, state);
}

static struct video_shader *vulkan_get_current_shader(void *data)
{
   vk_t *vk = (vk_t*)data;
   if (vk && vk->filter_chain)
      return vulkan_filter_chain_get_preset((vulkan_filter_chain_t*)vk->filter_chain);
   return NULL;
}

static bool vulkan_get_current_sw_framebuffer(void *data,
      struct retro_framebuffer *framebuffer)
{
   struct vk_per_frame *chain = NULL;
   vk_t *vk                   = (vk_t*)data;
   vk->chain                  =
      &vk->swapchain[vk->context->current_frame_index];
   chain                      = vk->chain;

   if (chain->texture.width != framebuffer->width ||
         chain->texture.height != framebuffer->height)
   {
      chain->texture   = vulkan_create_texture(vk, &chain->texture,
            framebuffer->width, framebuffer->height, chain->texture.format,
            NULL, NULL, VULKAN_TEXTURE_STREAMED);
      {
         struct vk_texture *texture = &chain->texture;
         VK_MAP_PERSISTENT_TEXTURE(vk->context->device, texture);
      }

      if (chain->texture.type == VULKAN_TEXTURE_STAGING)
      {
         chain->texture_optimal = vulkan_create_texture(
               vk,
               &chain->texture_optimal,
               framebuffer->width,
               framebuffer->height,
               chain->texture.format, /* Ensure we use the non-remapped format. */
               NULL, NULL, VULKAN_TEXTURE_DYNAMIC);
      }
   }

   framebuffer->data         = chain->texture.mapped;
   framebuffer->pitch        = chain->texture.stride;
   framebuffer->format       = vk->video.rgb32
      ? RETRO_PIXEL_FORMAT_XRGB8888 : RETRO_PIXEL_FORMAT_RGB565;
   framebuffer->memory_flags = 0;

   if (vk->context->memory_properties.memoryTypes[
         chain->texture.memory_type].propertyFlags &
         VK_MEMORY_PROPERTY_HOST_CACHED_BIT)
      framebuffer->memory_flags |= RETRO_MEMORY_TYPE_CACHED;

   return true;
}

static bool vulkan_is_mapped_swapchain_texture_ptr(const vk_t* vk,
      const void* ptr)
{
   int i;
   for (i = 0; i < (int) vk->num_swapchain_images; i++)
   {
      if (ptr == vk->swapchain[i].texture.mapped)
         return true;
   }

   return false;
}

static bool vulkan_get_hw_render_interface(void *data,
      const struct retro_hw_render_interface **iface)
{
   vk_t *vk = (vk_t*)data;
   *iface   = (const struct retro_hw_render_interface*)&vk->hw.iface;
   return ((vk->flags & VK_FLAG_HW_ENABLE) > 0);
}

static void vulkan_set_texture_frame(void *data,
      const void *frame, bool rgb32, unsigned width, unsigned height,
      float alpha)
{
   size_t y;
   unsigned stride;
   uint8_t *ptr                          = NULL;
   uint8_t *dst                          = NULL;
   const uint8_t *src                    = NULL;
   vk_t *vk                              = (vk_t*)data;
   unsigned idx                          = 0;
   struct vk_texture *texture            = NULL;
   struct vk_texture *texture_optimal    = NULL;
   VkFormat fmt                          = VK_FORMAT_B8G8R8A8_UNORM;
   bool do_memcpy                        = true;
   const VkComponentMapping *ptr_swizzle = NULL;

   if (!vk)
      return;

   if (!rgb32)
   {
       VkFormatProperties formatProperties;
       vkGetPhysicalDeviceFormatProperties(vk->context->gpu, VK_FORMAT_B4G4R4A4_UNORM_PACK16, &formatProperties);
       if (formatProperties.optimalTilingFeatures != 0)
       {
          static const VkComponentMapping br_swizzle =
          {VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_A};
          /* B4G4R4A4 must be supported, but R4G4B4A4 is optional,
           * just apply the swizzle in the image view instead. */
          fmt          = VK_FORMAT_B4G4R4A4_UNORM_PACK16;
          ptr_swizzle  = &br_swizzle;
       }
       else
           do_memcpy   = false;
   }

   idx                 = vk->context->current_frame_index;
   texture             = &vk->menu.textures[idx];
   texture_optimal     = &vk->menu.textures_optimal[idx];

   *texture            = vulkan_create_texture(vk,
           texture->memory
         ? texture
         : NULL,
         width,
         height,
         fmt,
         NULL,
         ptr_swizzle,
           texture_optimal->memory
         ? VULKAN_TEXTURE_STAGING
         : VULKAN_TEXTURE_STREAMED);

   vkMapMemory(vk->context->device, texture->memory,
         texture->offset, texture->size, 0, (void**)&ptr);

   dst       = ptr;
   src       = (const uint8_t*)frame;
   stride    = (rgb32 ? sizeof(uint32_t) : sizeof(uint16_t)) * width;

   if (do_memcpy)
   {
      for (y = 0; y < height; y++, dst += texture->stride, src += stride)
         memcpy(dst, src, stride);
   }
   else
   {
      for (y = 0; y < height; y++, dst += texture->stride, src += stride)
      {
         size_t x;
         uint16_t *srcpix = (uint16_t*)src;
         uint32_t *dstpix = (uint32_t*)dst;
         for (x = 0; x < width; x++, srcpix++, dstpix++)
         {
            uint32_t pix = *srcpix;
            *dstpix      = (
                  (pix & 0xf000) >>  8)
               | ((pix & 0x0f00) <<  4)
               | ((pix & 0x00f0) << 16)
               | ((pix & 0x000f) << 28);
         }
      }
   }

   vk->menu.alpha      = alpha;
   vk->menu.last_index = idx;

   if (texture->type == VULKAN_TEXTURE_STAGING)
      *texture_optimal = vulkan_create_texture(vk,
              texture_optimal->memory
            ? texture_optimal
            : NULL,
            width,
            height,
            fmt,
            NULL,
            ptr_swizzle,
            VULKAN_TEXTURE_DYNAMIC);
   else
   {
      VULKAN_SYNC_TEXTURE_TO_GPU_COND_PTR(vk, texture);
   }

   vkUnmapMemory(vk->context->device, texture->memory);
   vk->menu.dirty[idx] = true;
}

static void vulkan_set_texture_enable(void *data, bool state, bool fullscreen)
{
   vk_t *vk = (vk_t*)data;
   if (!vk)
      return;

   if (state)
      vk->flags        |=  VK_FLAG_MENU_ENABLE;
   else
      vk->flags        &= ~VK_FLAG_MENU_ENABLE;
   if (fullscreen)
      vk->flags        |=  VK_FLAG_MENU_FULLSCREEN;
   else
      vk->flags        &= ~VK_FLAG_MENU_FULLSCREEN;
}

#define VK_T0 0xff000000u
#define VK_T1 0xffffffffu

static uintptr_t vulkan_load_texture(void *video_data, void *data,
      bool threaded, enum texture_filter_type filter_type)
{
   struct vk_texture *texture  = NULL;
   vk_t *vk                    = (vk_t*)video_data;
   struct texture_image *image = (struct texture_image*)data;
   if (!image)
      return 0;

   if (!(texture = (struct vk_texture*)calloc(1, sizeof(*texture))))
      return 0;

   if (!image->pixels || !image->width || !image->height)
   {
      /* Create a dummy texture instead. */
      static const uint32_t checkerboard[] = {
         VK_T0, VK_T1, VK_T0, VK_T1, VK_T0, VK_T1, VK_T0, VK_T1,
         VK_T1, VK_T0, VK_T1, VK_T0, VK_T1, VK_T0, VK_T1, VK_T0,
         VK_T0, VK_T1, VK_T0, VK_T1, VK_T0, VK_T1, VK_T0, VK_T1,
         VK_T1, VK_T0, VK_T1, VK_T0, VK_T1, VK_T0, VK_T1, VK_T0,
         VK_T0, VK_T1, VK_T0, VK_T1, VK_T0, VK_T1, VK_T0, VK_T1,
         VK_T1, VK_T0, VK_T1, VK_T0, VK_T1, VK_T0, VK_T1, VK_T0,
         VK_T0, VK_T1, VK_T0, VK_T1, VK_T0, VK_T1, VK_T0, VK_T1,
         VK_T1, VK_T0, VK_T1, VK_T0, VK_T1, VK_T0, VK_T1, VK_T0,
      };
      *texture                = vulkan_create_texture(vk, NULL,
            8, 8, VK_FORMAT_B8G8R8A8_UNORM,
            checkerboard, NULL, VULKAN_TEXTURE_STATIC);
      texture->flags         &= ~(VK_TEX_FLAG_DEFAULT_SMOOTH
                                | VK_TEX_FLAG_MIPMAP);
   }
   else
   {
      *texture = vulkan_create_texture(vk, NULL,
            image->width, image->height, VK_FORMAT_B8G8R8A8_UNORM,
            image->pixels, NULL, VULKAN_TEXTURE_STATIC);
      if (filter_type == TEXTURE_FILTER_MIPMAP_LINEAR || filter_type ==
            TEXTURE_FILTER_LINEAR)
         texture->flags |= VK_TEX_FLAG_DEFAULT_SMOOTH;
      if (filter_type == TEXTURE_FILTER_MIPMAP_LINEAR)
         texture->flags |= VK_TEX_FLAG_MIPMAP;
   }

   return (uintptr_t)texture;
}

static void vulkan_unload_texture(void *data,
      bool threaded, uintptr_t handle)
{
   vk_t *vk                         = (vk_t*)data;
   struct vk_texture *texture       = (struct vk_texture*)handle;
   if (!texture || !vk)
      return;

   /* TODO: We really want to defer this deletion instead,
    * but this will do for now. */
#ifdef HAVE_THREADS
   slock_lock(vk->context->queue_lock);
#endif
   vkQueueWaitIdle(vk->context->queue);
#ifdef HAVE_THREADS
   slock_unlock(vk->context->queue_lock);
#endif
   vulkan_destroy_texture(
         vk->context->device, texture);
   free(texture);
}

static float vulkan_get_refresh_rate(void *data)
{
   float refresh_rate;

   if (video_context_driver_get_refresh_rate(&refresh_rate))
       return refresh_rate;

   return 0.0f;
}

static uint32_t vulkan_get_flags(void *data)
{
   uint32_t flags = 0;

   BIT32_SET(flags, GFX_CTX_FLAGS_CUSTOMIZABLE_SWAPCHAIN_IMAGES);
   BIT32_SET(flags, GFX_CTX_FLAGS_BLACK_FRAME_INSERTION);
   BIT32_SET(flags, GFX_CTX_FLAGS_MENU_FRAME_FILTERING);
   BIT32_SET(flags, GFX_CTX_FLAGS_SCREENSHOTS_SUPPORTED);
   BIT32_SET(flags, GFX_CTX_FLAGS_OVERLAY_BEHIND_MENU_SUPPORTED);
   BIT32_SET(flags, GFX_CTX_FLAGS_SUBFRAME_SHADERS);

   return flags;
}

static void vulkan_get_video_output_size(void *data,
      unsigned *width, unsigned *height, char *desc, size_t desc_len)
{
   vk_t *vk = (vk_t*)data;
   if (vk && vk->ctx_driver && vk->ctx_driver->get_video_output_size)
      vk->ctx_driver->get_video_output_size(
            vk->ctx_data,
            width, height, desc, desc_len);
}

static void vulkan_get_video_output_prev(void *data)
{
   vk_t *vk = (vk_t*)data;
   if (vk && vk->ctx_driver && vk->ctx_driver->get_video_output_prev)
      vk->ctx_driver->get_video_output_prev(vk->ctx_data);
}

static void vulkan_get_video_output_next(void *data)
{
   vk_t *vk = (vk_t*)data;
   if (vk && vk->ctx_driver && vk->ctx_driver->get_video_output_next)
      vk->ctx_driver->get_video_output_next(vk->ctx_data);
}

static const video_poke_interface_t vulkan_poke_interface = {
   vulkan_get_flags,
   vulkan_load_texture,
   vulkan_unload_texture,
   vulkan_set_video_mode,
   vulkan_get_refresh_rate,
   NULL, /* set_filtering */
   vulkan_get_video_output_size,
   vulkan_get_video_output_prev,
   vulkan_get_video_output_next,
   NULL, /* get_current_framebuffer */
   NULL, /* get_proc_address */
   vulkan_set_aspect_ratio,
   vulkan_apply_state_changes,
   vulkan_set_texture_frame,
   vulkan_set_texture_enable,
   font_driver_render_msg,
   vulkan_show_mouse,
   NULL, /* grab_mouse_toggle */
   vulkan_get_current_shader,
   vulkan_get_current_sw_framebuffer,
   vulkan_get_hw_render_interface,
#ifdef VULKAN_HDR_SWAPCHAIN
   vulkan_set_hdr_max_nits,
   vulkan_set_hdr_paper_white_nits,
   vulkan_set_hdr_contrast,
   vulkan_set_hdr_expand_gamut
#else
   NULL, /* set_hdr_max_nits */
   NULL, /* set_hdr_paper_white_nits */
   NULL, /* set_hdr_contrast */
   NULL  /* set_hdr_expand_gamut */
#endif /* VULKAN_HDR_SWAPCHAIN */
};

static void vulkan_get_poke_interface(void *data,
      const video_poke_interface_t **iface)
{
   (void)data;
   *iface = &vulkan_poke_interface;
}

static void vulkan_viewport_info(void *data, struct video_viewport *vp)
{
   unsigned width, height;
   vk_t *vk = (vk_t*)data;

   if (!vk)
      return;

   width           = vk->video_width;
   height          = vk->video_height;
   /* Make sure we get the correct viewport. */
   vulkan_set_viewport(vk, width, height, false, true);

   *vp             = vk->vp;
   vp->full_width  = width;
   vp->full_height = height;
}

static bool vulkan_read_viewport(void *data, uint8_t *buffer, bool is_idle)
{
   struct vk_texture *staging       = NULL;
   vk_t *vk                         = (vk_t*)data;

   if (!vk)
      return false;

   staging = &vk->readback.staging[vk->context->current_frame_index];

   VkFormat format = vk->context->swapchain_format;
#ifdef VULKAN_HDR_SWAPCHAIN
   if (vk->context->flags & VK_CTX_FLAG_HDR_ENABLE)
   {
      /* Hdr readback is implemented through format conversion on the GPU */
      format = VK_FORMAT_B8G8R8A8_UNORM;
   }
#endif /* VULKAN_HDR_SWAPCHAIN */
   if (vk->flags & VK_FLAG_READBACK_STREAMED)
   {
      const uint8_t *src     = NULL;
      struct scaler_ctx *ctx = NULL;

      switch (format)
      {
         case VK_FORMAT_R8G8B8A8_UNORM:
         case VK_FORMAT_A8B8G8R8_UNORM_PACK32:
            ctx = &vk->readback.scaler_rgb;
            break;

         case VK_FORMAT_B8G8R8A8_UNORM:
            ctx = &vk->readback.scaler_bgr;
            break;

         default:
            RARCH_ERR("[Vulkan]: Unexpected swapchain format. Cannot readback.\n");
            break;
      }

      if (ctx)
      {
         if (staging->memory == VK_NULL_HANDLE)
            return false;

         buffer += 3 * (vk->vp.height - 1) * vk->vp.width;
         vkMapMemory(vk->context->device, staging->memory,
               staging->offset, staging->size, 0, (void**)&src);

         if (     (staging->flags & VK_TEX_FLAG_NEED_MANUAL_CACHE_MANAGEMENT)
               && (staging->memory != VK_NULL_HANDLE))
            VULKAN_SYNC_TEXTURE_TO_CPU(vk->context->device, staging->memory);

         ctx->in_stride  =  (int)staging->stride;
         ctx->out_stride = -(int)vk->vp.width * 3;
         scaler_ctx_scale_direct(ctx, buffer, src);

         vkUnmapMemory(vk->context->device, staging->memory);
      }
   }
   else
   {
      /* Synchronous path only for now. */
      /* TODO: How will we deal with format conversion?
       * For now, take the simplest route and use image blitting
       * with conversion. */
      vk->flags |= VK_FLAG_READBACK_PENDING;

      if (!is_idle)
         video_driver_cached_frame();

#ifdef HAVE_THREADS
      slock_lock(vk->context->queue_lock);
#endif
      vkQueueWaitIdle(vk->context->queue);
#ifdef HAVE_THREADS
      slock_unlock(vk->context->queue_lock);
#endif

      if (!staging->memory)
      {
         RARCH_ERR("[Vulkan]: Attempted to readback synchronously, but no image is present.\nThis can happen if vsync is disabled on Windows systems due to mailbox emulation.\n");
         return false;
      }

      if (!staging->mapped)
      {
         VK_MAP_PERSISTENT_TEXTURE(vk->context->device, staging);
      }

      if (     (staging->flags & VK_TEX_FLAG_NEED_MANUAL_CACHE_MANAGEMENT)
            && (staging->memory != VK_NULL_HANDLE))
         VULKAN_SYNC_TEXTURE_TO_CPU(vk->context->device, staging->memory);

      {
         int y;
         unsigned vp_width  = (vk->vp.width  > vk->video_width)  ? vk->video_width  : vk->vp.width;
         unsigned vp_height = (vk->vp.height > vk->video_height) ? vk->video_height : vk->vp.height;
         const uint8_t *src = (const uint8_t*)staging->mapped;

         buffer            += 3 * (vp_height - 1) * vp_width;

         switch (format)
         {
            case VK_FORMAT_B8G8R8A8_UNORM:
               for (y = 0; y < (int) vp_height; y++,
                     src += staging->stride, buffer -= 3 * vp_width)
               {
                  int x;
                  for (x = 0; x < (int) vp_width; x++)
                  {
                     buffer[3 * x + 0] = src[4 * x + 0];
                     buffer[3 * x + 1] = src[4 * x + 1];
                     buffer[3 * x + 2] = src[4 * x + 2];
                  }
               }
               break;

            case VK_FORMAT_R8G8B8A8_UNORM:
            case VK_FORMAT_A8B8G8R8_UNORM_PACK32:
               for (y = 0; y < (int) vp_height; y++,
                     src += staging->stride, buffer -= 3 * vp_width)
               {
                  int x;
                  for (x = 0; x < (int) vp_width; x++)
                  {
                     buffer[3 * x + 2] = src[4 * x + 0];
                     buffer[3 * x + 1] = src[4 * x + 1];
                     buffer[3 * x + 0] = src[4 * x + 2];
                  }
               }
               break;

            default:
               RARCH_ERR("[Vulkan]: Unexpected swapchain format.\n");
               break;
         }
      }
      vulkan_destroy_texture(
            vk->context->device, staging);
   }
   return true;
}

#ifdef HAVE_OVERLAY
static void vulkan_overlay_enable(void *data, bool enable)
{
   vk_t *vk = (vk_t*)data;
   if (!vk)
      return;

   if (enable)
      vk->flags |=  VK_FLAG_OVERLAY_ENABLE;
   else
      vk->flags &= ~VK_FLAG_OVERLAY_ENABLE;
   if (vk->ctx_driver->show_mouse)
      vk->ctx_driver->show_mouse(vk->ctx_data, enable);
}

static void vulkan_overlay_full_screen(void *data, bool enable)
{
   vk_t *vk = (vk_t*)data;
   if (!vk)
      return;

   if (enable)
      vk->flags |=  VK_FLAG_OVERLAY_FULLSCREEN;
   else
      vk->flags &= ~VK_FLAG_OVERLAY_FULLSCREEN;
}

static void vulkan_overlay_free(vk_t *vk)
{
   int i;
   if (!vk)
      return;

   free(vk->overlay.vertex);
   for (i = 0; i < (int) vk->overlay.count; i++)
      if (vk->overlay.images[i].memory != VK_NULL_HANDLE)
         vulkan_destroy_texture(
               vk->context->device,
               &vk->overlay.images[i]);

   if (vk->overlay.images)
      free(vk->overlay.images);

   memset(&vk->overlay, 0, sizeof(vk->overlay));
}

static void vulkan_overlay_set_alpha(void *data,
      unsigned image, float mod)
{
   int i;
   struct vk_vertex *pv;
   vk_t *vk = (vk_t*)data;

   if (!vk)
      return;

   pv = &vk->overlay.vertex[image * 4];
   for (i = 0; i < 4; i++)
   {
      pv[i].color.r = 1.0f;
      pv[i].color.g = 1.0f;
      pv[i].color.b = 1.0f;
      pv[i].color.a = mod;
   }
}

static void vulkan_render_overlay(vk_t *vk, unsigned width,
      unsigned height)
{
   int i;
   struct video_viewport vp;

   if (!vk)
      return;

   vp                       = vk->vp;
   vulkan_set_viewport(vk, width, height,
         ((vk->flags & VK_FLAG_OVERLAY_FULLSCREEN) > 0),
         false);

   for (i = 0; i < (int) vk->overlay.count; i++)
   {
      struct vk_draw_triangles call;
      struct vk_buffer_range range;

      if (!vulkan_buffer_chain_alloc(vk->context, &vk->chain->vbo,
               4 * sizeof(struct vk_vertex), &range))
         break;

      memcpy(range.data, &vk->overlay.vertex[i * 4],
            4 * sizeof(struct vk_vertex));

      call.vertices     = 4;
      call.uniform_size = sizeof(vk->mvp);
      call.uniform      = &vk->mvp;
      call.vbo          = &range;
      call.texture      = &vk->overlay.images[i];
      call.pipeline     = vk->display.pipelines[3]; /* Strip with blend */
      call.sampler      = (call.texture->flags & VK_TEX_FLAG_MIPMAP)
         ? vk->samplers.mipmap_linear : vk->samplers.linear;
      vulkan_draw_triangles(vk, &call);
   }

   /* Restore the viewport so we don't mess with recording. */
   vk->vp = vp;
}

static void vulkan_overlay_vertex_geom(void *data, unsigned image,
      float x, float y,
      float w, float h)
{
   struct vk_vertex *pv = NULL;
   vk_t             *vk = (vk_t*)data;
   if (!vk)
      return;

   pv      = &vk->overlay.vertex[4 * image];

   pv[0].x = x;
   pv[0].y = y;
   pv[1].x = x;
   pv[1].y = y + h;
   pv[2].x = x + w;
   pv[2].y = y;
   pv[3].x = x + w;
   pv[3].y = y + h;
}

static void vulkan_overlay_tex_geom(void *data, unsigned image,
      float x, float y,
      float w, float h)
{
   struct vk_vertex *pv = NULL;
   vk_t *vk             = (vk_t*)data;
   if (!vk)
      return;

   pv          = &vk->overlay.vertex[4 * image];

   pv[0].tex_x = x;
   pv[0].tex_y = y;
   pv[1].tex_x = x;
   pv[1].tex_y = y + h;
   pv[2].tex_x = x + w;
   pv[2].tex_y = y;
   pv[3].tex_x = x + w;
   pv[3].tex_y = y + h;
}

static bool vulkan_overlay_load(void *data,
      const void *image_data, unsigned num_images)
{
   int i;
   bool old_enabled                   = false;
   const struct texture_image *images =
      (const struct texture_image*)image_data;
   vk_t *vk                           = (vk_t*)data;
   static const struct vk_color white = {
      1.0f, 1.0f, 1.0f, 1.0f,
   };

   if (!vk)
      return false;

#ifdef HAVE_THREADS
   slock_lock(vk->context->queue_lock);
#endif
   vkQueueWaitIdle(vk->context->queue);
#ifdef HAVE_THREADS
   slock_unlock(vk->context->queue_lock);
#endif
   if (vk->flags & VK_FLAG_OVERLAY_ENABLE)
      old_enabled           = true;
   vulkan_overlay_free(vk);

   if (!(vk->overlay.images = (struct vk_texture*)
            calloc(num_images, sizeof(*vk->overlay.images))))
      goto error;
   vk->overlay.count        = num_images;

   if (!(vk->overlay.vertex = (struct vk_vertex*)
      calloc(4 * num_images, sizeof(*vk->overlay.vertex))))
      goto error;

   for (i = 0; i < (int) num_images; i++)
   {
      int j;
      vk->overlay.images[i] = vulkan_create_texture(vk, NULL,
            images[i].width, images[i].height,
            VK_FORMAT_B8G8R8A8_UNORM, images[i].pixels,
            NULL, VULKAN_TEXTURE_STATIC);

      vulkan_overlay_tex_geom(vk, i, 0, 0, 1, 1);
      vulkan_overlay_vertex_geom(vk, i, 0, 0, 1, 1);
      for (j = 0; j < 4; j++)
         vk->overlay.vertex[4 * i + j].color = white;
   }

   if (old_enabled)
      vk->flags |=  VK_FLAG_OVERLAY_ENABLE;
   else
      vk->flags &= ~VK_FLAG_OVERLAY_ENABLE;

   return true;

error:
   vulkan_overlay_free(vk);
   return false;
}

static const video_overlay_interface_t vulkan_overlay_interface = {
   vulkan_overlay_enable,
   vulkan_overlay_load,
   vulkan_overlay_tex_geom,
   vulkan_overlay_vertex_geom,
   vulkan_overlay_full_screen,
   vulkan_overlay_set_alpha,
};

static void vulkan_get_overlay_interface(void *data,
      const video_overlay_interface_t **iface) { *iface = &vulkan_overlay_interface; }
#endif

#ifdef HAVE_GFX_WIDGETS
static bool vulkan_gfx_widgets_enabled(void *data) { return true; }
#endif

static bool vulkan_has_windowed(void *data)
{
   vk_t *vk        = (vk_t*)data;
   if (vk && vk->ctx_driver)
      return vk->ctx_driver->has_windowed;
   return false;
}

static bool vulkan_focus(void *data)
{
   vk_t *vk        = (vk_t*)data;
   if (vk && vk->ctx_driver && vk->ctx_driver->has_focus)
      return vk->ctx_driver->has_focus(vk->ctx_data);
   return true;
}

video_driver_t video_vulkan = {
   vulkan_init,
   vulkan_frame,
   vulkan_set_nonblock_state,
   vulkan_alive,
   vulkan_focus,
   vulkan_suppress_screensaver,
   vulkan_has_windowed,
   vulkan_set_shader,
   vulkan_free,
   "vulkan",
   vulkan_set_viewport,
   vulkan_set_rotation,
   vulkan_viewport_info,
   vulkan_read_viewport,
   NULL, /* read_frame_raw */
#ifdef HAVE_OVERLAY
   vulkan_get_overlay_interface,
#endif
   vulkan_get_poke_interface,
   NULL, /* wrap_type_to_enum */
#ifdef HAVE_GFX_WIDGETS
   vulkan_gfx_widgets_enabled
#endif
};
