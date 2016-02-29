/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2016 - Hans-Kristian Arntzen
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

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifdef HAVE_XCB
#include <X11/Xlib-xcb.h>
#endif

#include <retro_assert.h>
#include <dynamic/dylib.h>

#include "vulkan_common.h"

#ifdef HAVE_VULKAN
static dylib_t vulkan_library;
static VkInstance cached_instance;
static VkDevice cached_device;
#endif

#define VKSYM(vk, entrypoint) do {                                                         \
   vk->context.fp.vk##entrypoint = (PFN_vk##entrypoint) dylib_proc(vulkan_library, "vk"#entrypoint);\
   if (vk->context.fp.vk##entrypoint == NULL) {                                                    \
      RARCH_ERR("vkGetInstanceProcAddr failed to find vk%s\n", #entrypoint);               \
      return false;                                                                        \
   }                                                                                       \
} while(0)

#define VK_GET_INSTANCE_PROC_ADDR(vk, inst, entrypoint) do {                               \
   vk->context.fp.vk##entrypoint = (PFN_vk##entrypoint) vkGetInstanceProcAddr(inst, "vk"#entrypoint); \
   if (vk->context.fp.vk##entrypoint == NULL) {                                                    \
      RARCH_ERR("vkGetInstanceProcAddr failed to find vk%s\n", #entrypoint);               \
      return false;                                                                        \
   }                                                                                       \
} while(0)

#define VK_GET_DEVICE_PROC_ADDR(vk, dev, entrypoint) do {                                \
   vk->context.fp.vk##entrypoint = (PFN_vk##entrypoint) vkGetDeviceProcAddr(dev, "vk" #entrypoint); \
   if (vk->context.fp.vk##entrypoint == NULL) {                                                  \
      RARCH_ERR("vkGetDeviceProcAddr failed to find vk%s\n", #entrypoint);               \
      return false;                                                                      \
   }                                                                                     \
} while(0)

uint32_t vulkan_find_memory_type(const VkPhysicalDeviceMemoryProperties *mem_props,
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

uint32_t vulkan_find_memory_type_fallback(const VkPhysicalDeviceMemoryProperties *mem_props,
      uint32_t device_reqs, uint32_t host_reqs_first, uint32_t host_reqs_second)
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

   return vulkan_find_memory_type_fallback(mem_props, device_reqs, host_reqs_second, 0);
}

void vulkan_map_persistent_texture(VkDevice device, struct vk_texture *texture)
{
   vkMapMemory(device, texture->memory, texture->offset, texture->size, 0, &texture->mapped);
}

void vulkan_copy_staging_to_dynamic(vk_t *vk, VkCommandBuffer cmd,
      struct vk_texture *dynamic,
      struct vk_texture *staging)
{
   VkImageCopy region;
   retro_assert(dynamic->type == VULKAN_TEXTURE_DYNAMIC);
   retro_assert(staging->type == VULKAN_TEXTURE_STAGING);

   vulkan_transition_texture(vk, staging);

   /* We don't have to sync against previous TRANSFER, since we observed the completion
    * by fences. If we have a single texture_optimal, we would need to sync against
    * previous transfers to avoid races.
    *
    * We would also need to optionally maintain extra textures due to changes in resolution,
    * so this seems like the sanest and simplest solution. */
   vulkan_image_layout_transition(vk, vk->cmd, dynamic->image,
         VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
         0, VK_ACCESS_TRANSFER_WRITE_BIT,
         VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
         VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);

   memset(&region, 0, sizeof(region));
   region.extent.width = dynamic->width;
   region.extent.height = dynamic->height;
   region.extent.depth = 1;
   region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
   region.srcSubresource.layerCount = 1;
   region.dstSubresource = region.srcSubresource;

   vk->context->fp.vkCmdCopyImage(vk->cmd,
         staging->image, VK_IMAGE_LAYOUT_GENERAL,
         dynamic->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
         1, &region);

   vulkan_image_layout_transition(vk, vk->cmd, dynamic->image,
         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
         VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
         VK_PIPELINE_STAGE_TRANSFER_BIT,
         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
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
   RARCH_LOG("[Vulkan]: Alloc %llu (%u).\n", (unsigned long long)image, track_seq);
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
         memmove(vk_images + i, vk_images + 1 + i, sizeof(VkImage) * (vk_count - i));
         return;
      }
   }
   retro_assert(0 && "Couldn't find VkImage in dealloc!");
}
#endif

struct vk_texture vulkan_create_texture(vk_t *vk,
      struct vk_texture *old,
      unsigned width, unsigned height,
      VkFormat format,
      const void *initial, const VkComponentMapping *swizzle, enum vk_texture_type type)
{
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
   info.mipLevels     = 1;
   info.arrayLayers   = 1;
   info.samples = VK_SAMPLE_COUNT_1_BIT;

   if (type == VULKAN_TEXTURE_STREAMED)
   {
      VkFormatProperties format_properties;
      VkFormatFeatureFlags required = VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT |
         VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
      vkGetPhysicalDeviceFormatProperties(vk->context->gpu, format, &format_properties);

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
         info.tiling = VK_IMAGE_TILING_OPTIMAL;
         info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
         info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
         break;

      case VULKAN_TEXTURE_DYNAMIC:
         retro_assert(!initial && "Dynamic textures must not have initial data.\n");
         info.tiling = VK_IMAGE_TILING_OPTIMAL;
         info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
         info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
         break;

      case VULKAN_TEXTURE_STREAMED:
         info.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
         info.tiling = VK_IMAGE_TILING_LINEAR;
         info.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
         break;

      case VULKAN_TEXTURE_STAGING:
         info.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
         info.tiling = VK_IMAGE_TILING_LINEAR;
         info.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
         break;

      case VULKAN_TEXTURE_READBACK:
         info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
         info.tiling = VK_IMAGE_TILING_LINEAR;
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
         alloc.memoryTypeIndex = vulkan_find_memory_type_fallback(&vk->context->memory_properties,
               mem_reqs.memoryTypeBits,
               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0);
         break;

      default:
         alloc.memoryTypeIndex = vulkan_find_memory_type_fallback(&vk->context->memory_properties,
               mem_reqs.memoryTypeBits,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
               VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
               VK_MEMORY_PROPERTY_HOST_CACHED_BIT,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
               VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
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
      type = VULKAN_TEXTURE_STAGING;
      vkDestroyImage(device, tex.image, NULL);
      info.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
      vkCreateImage(device, &info, NULL, &tex.image);
      vkGetImageMemoryRequirements(device, tex.image, &mem_reqs);
      alloc.allocationSize = mem_reqs.size;
      alloc.memoryTypeIndex = vulkan_find_memory_type_fallback(&vk->context->memory_properties,
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
      old->memory = VK_NULL_HANDLE;
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
   view.subresourceRange.levelCount = 1;
   view.subresourceRange.layerCount = 1;

   vkCreateImageView(device, &view, NULL, &tex.view);

   vkGetImageSubresourceLayout(device, tex.image, &subresource, &layout);
   tex.stride = layout.rowPitch;
   tex.offset = layout.offset;
   tex.size   = layout.size;
   tex.layout = info.initialLayout;

   tex.width  = width;
   tex.height = height;
   tex.format = format;
   tex.type = type;

   if (initial && (type == VULKAN_TEXTURE_STREAMED || type == VULKAN_TEXTURE_STAGING))
   {
      unsigned x, y;
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

      vkUnmapMemory(device, tex.memory);
   }
   else if (initial && type == VULKAN_TEXTURE_STATIC)
   {
      VkImageCopy region;
      VkCommandBuffer staging;
      unsigned bpp = vulkan_format_to_bpp(tex.format);
      struct vk_texture tmp = vulkan_create_texture(vk, NULL,
            width, height, format, initial, NULL, VULKAN_TEXTURE_STAGING);

      cmd_info.commandPool        = vk->staging_pool;
      cmd_info.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
      cmd_info.commandBufferCount = 1;
      vkAllocateCommandBuffers(vk->context->device, &cmd_info, &staging);

      begin_info.flags        = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
      vkBeginCommandBuffer(staging, &begin_info);

      vulkan_image_layout_transition(vk, staging, tmp.image,
            VK_IMAGE_LAYOUT_PREINITIALIZED, VK_IMAGE_LAYOUT_GENERAL,
            VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);

      vulkan_image_layout_transition(vk, staging, tex.image,
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            0, VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);

      memset(&region, 0, sizeof(region));
      region.extent.width              = width;
      region.extent.height             = height;
      region.extent.depth              = 1;
      region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      region.srcSubresource.layerCount = 1;
      region.dstSubresource            = region.srcSubresource;

      vk->context->fp.vkCmdCopyImage(staging,
            tmp.image, VK_IMAGE_LAYOUT_GENERAL,
            tex.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, &region);

      vulkan_image_layout_transition(vk, staging, tex.image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);

      vkEndCommandBuffer(staging);
      submit_info.commandBufferCount = 1;
      submit_info.pCommandBuffers    = &staging;

      slock_lock(vk->context->queue_lock);
      vk->context->fp.vkQueueSubmit(vk->context->queue,
            1, &submit_info, VK_NULL_HANDLE);

      /* TODO: Very crude, but texture uploads only happen 
       * during init, so waiting for GPU to complete transfer 
       * and blocking isn't a big deal. */
      vk->context->fp.vkQueueWaitIdle(vk->context->queue);
      slock_unlock(vk->context->queue_lock);

      vkFreeCommandBuffers(vk->context->device, vk->staging_pool, 1, &staging);
      vulkan_destroy_texture(vk->context->device, &tmp);
      tex.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
   }
   return tex;
}

void vulkan_destroy_texture(VkDevice device, struct vk_texture *tex)
{
   if (tex->mapped)
      vkUnmapMemory(device, tex->memory);
   vkFreeMemory(device, tex->memory, NULL);
   vkDestroyImageView(device, tex->view, NULL);
   vkDestroyImage(device, tex->image, NULL);
#ifdef VULKAN_DEBUG_TEXTURE_ALLOC
   vulkan_track_dealloc(tex->image);
#endif
   memset(tex, 0, sizeof(*tex));
}

static void vulkan_write_quad_descriptors(VkDevice device,
      VkDescriptorSet set,
      VkBuffer buffer,
      VkDeviceSize offset,
      VkDeviceSize range,
      const struct vk_texture *texture,
      VkSampler sampler)
{
   VkWriteDescriptorSet write = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
   VkDescriptorImageInfo image_info;
   VkDescriptorBufferInfo buffer_info;

   image_info.sampler     = sampler;
   image_info.imageView   = texture->view;
   image_info.imageLayout = texture->layout;

   buffer_info.buffer     = buffer;
   buffer_info.offset     = offset;
   buffer_info.range      = range;

   write.dstSet           = set;
   write.dstBinding       = 0;
   write.descriptorCount  = 1;
   write.descriptorType   = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
   write.pBufferInfo      = &buffer_info;
   vkUpdateDescriptorSets(device, 1, &write, 0, NULL);

   write.dstSet           = set;
   write.dstBinding       = 1;
   write.descriptorCount  = 1;
   write.descriptorType   = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
   write.pImageInfo       = &image_info;
   vkUpdateDescriptorSets(device, 1, &write, 0, NULL);
}

void vulkan_transition_texture(vk_t *vk, struct vk_texture *texture)
{
   /* Transition to GENERAL layout for linear streamed textures.
    * We're using linear textures here, so only 
    * GENERAL layout is supported.
    */
   if (texture->layout == VK_IMAGE_LAYOUT_PREINITIALIZED)
   {
      vulkan_image_layout_transition(vk, vk->cmd, texture->image,
            texture->layout, VK_IMAGE_LAYOUT_GENERAL,
            VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
      texture->layout = VK_IMAGE_LAYOUT_GENERAL;
   }
}

static void vulkan_check_dynamic_state(vk_t *vk)
{
   if (vk->tracker.dirty & VULKAN_DIRTY_DYNAMIC_BIT)
   {
      const VkRect2D sci = {
         { vk->vp.x, vk->vp.y },
         { vk->vp.width, vk->vp.height }};
      vk->context->fp.vkCmdSetViewport(vk->cmd, 0, 1, &vk->vk_vp);
      vk->context->fp.vkCmdSetScissor(vk->cmd, 0, 1, &sci);

      vk->tracker.dirty &= ~VULKAN_DIRTY_DYNAMIC_BIT;
   }
}

void vulkan_draw_triangles(vk_t *vk, const struct vk_draw_triangles *call)
{
   vulkan_transition_texture(vk, call->texture);

   if (call->pipeline != vk->tracker.pipeline)
   {
      vk->context->fp.vkCmdBindPipeline(vk->cmd,
            VK_PIPELINE_BIND_POINT_GRAPHICS, call->pipeline);
      vk->tracker.pipeline = call->pipeline;

      /* Changing pipeline invalidates dynamic state. */
      vk->tracker.dirty |= VULKAN_DIRTY_DYNAMIC_BIT;
   }

   vulkan_check_dynamic_state(vk);

   /* Upload descriptors */
   {
      VkDescriptorSet set;

      if (memcmp(call->mvp, &vk->tracker.mvp, sizeof(*call->mvp)) 
            || (call->texture->view != vk->tracker.view)
            || (call->sampler != vk->tracker.sampler))
      {
         /* Upload UBO */
         struct vk_buffer_range range;
         if (!vulkan_buffer_chain_alloc(vk->context, &vk->chain->ubo,
                  sizeof(*call->mvp), &range))
            return;
         memcpy(range.data, call->mvp, sizeof(*call->mvp));

         set = vulkan_descriptor_manager_alloc(
               vk->context->device, &vk->chain->descriptor_manager);
         vulkan_write_quad_descriptors(vk->context->device,
               set,
               range.buffer,
               range.offset,
               sizeof(*call->mvp),
               call->texture,
               call->sampler);

         vkCmdBindDescriptorSets(vk->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
               vk->pipelines.layout, 0,
               1, &set, 0, NULL);

         vk->tracker.view = call->texture->view;
         vk->tracker.sampler = call->sampler;
         vk->tracker.mvp = *call->mvp;
      }
   }

   /* VBO is already uploaded. */
   vkCmdBindVertexBuffers(vk->cmd, 0, 1,
         &call->vbo->buffer, &call->vbo->offset);

   /* Draw the quad */
   vk->context->fp.vkCmdDraw(vk->cmd, call->vertices, 1, 0, 0);
}

void vulkan_draw_quad(vk_t *vk, const struct vk_draw_quad *quad)
{
   vulkan_transition_texture(vk, quad->texture);

   if (quad->pipeline != vk->tracker.pipeline)
   {
      vk->context->fp.vkCmdBindPipeline(vk->cmd,
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

      if (memcmp(quad->mvp, &vk->tracker.mvp, sizeof(*quad->mvp)) 
            || quad->texture->view != vk->tracker.view
            || quad->sampler != vk->tracker.sampler)
      {
         /* Upload UBO */
         struct vk_buffer_range range;

         if (!vulkan_buffer_chain_alloc(vk->context, &vk->chain->ubo,
                  sizeof(*quad->mvp), &range))
            return;

         memcpy(range.data, quad->mvp, sizeof(*quad->mvp));

         set = vulkan_descriptor_manager_alloc(vk->context->device,
               &vk->chain->descriptor_manager);

         vulkan_write_quad_descriptors(vk->context->device,
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
   vk->context->fp.vkCmdDraw(vk->cmd, 6, 1, 0, 0);
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
   VkImageMemoryBarrier barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };

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

struct vk_buffer vulkan_create_buffer(const struct vulkan_context *context,
      size_t size, VkBufferUsageFlags usage)
{
   struct vk_buffer buffer;
   VkMemoryRequirements mem_reqs;
   VkMemoryAllocateInfo alloc = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
   VkBufferCreateInfo info    = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };

   info.size        = size;
   info.usage       = usage;
   info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
   vkCreateBuffer(context->device, &info, NULL, &buffer.buffer);

   vkGetBufferMemoryRequirements(context->device, buffer.buffer, &mem_reqs);

   alloc.allocationSize  = mem_reqs.size;
   alloc.memoryTypeIndex = vulkan_find_memory_type(
         &context->memory_properties,
         mem_reqs.memoryTypeBits,
         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | 
         VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
   vkAllocateMemory(context->device, &alloc, NULL, &buffer.memory);
   vkBindBufferMemory(context->device, buffer.buffer, buffer.memory, 0);

   buffer.size = alloc.allocationSize;

   vkMapMemory(context->device,
         buffer.memory, 0, buffer.size, 0, &buffer.mapped);
   return buffer;
}

void vulkan_destroy_buffer(VkDevice device, struct vk_buffer *buffer)
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

   pool_info.maxSets = VULKAN_DESCRIPTOR_MANAGER_BLOCK_SETS;
   pool_info.poolSizeCount = manager->num_sizes;
   pool_info.pPoolSizes = manager->sizes;
   pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
   vkCreateDescriptorPool(device, &pool_info, NULL, &pool->pool);

   /* Just allocate all descriptor sets up front. */
   alloc_info.descriptorPool = pool->pool;
   alloc_info.descriptorSetCount = 1;
   alloc_info.pSetLayouts = &manager->set_layout;
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

void vulkan_destroy_descriptor_manager(VkDevice device,
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
   chain->offset = 0;
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

   node->buffer = vulkan_create_buffer(context, size, usage);
   return node;
}

struct vk_buffer_chain vulkan_buffer_chain_init(VkDeviceSize block_size,
      VkDeviceSize alignment,
      VkBufferUsageFlags usage)
{
   struct vk_buffer_chain chain = { 
      block_size, alignment, 0, usage, NULL, NULL };
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

void vulkan_buffer_chain_free(VkDevice device, struct vk_buffer_chain *chain)
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

bool vulkan_context_init(gfx_ctx_vulkan_data_t *vk,
      enum vulkan_wsi_type type)
{
   unsigned i;
   uint32_t queue_count;
   VkQueueFamilyProperties queue_properties[32];
   VkInstanceCreateInfo info          = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
   VkApplicationInfo app              = { VK_STRUCTURE_TYPE_APPLICATION_INFO };
   VkPhysicalDeviceFeatures features  = { false };
   VkDeviceQueueCreateInfo queue_info = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
   VkDeviceCreateInfo device_info     = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
   uint32_t gpu_count                 = 1;
   bool found_queue                   = false;
   VkPhysicalDevice *gpus             = NULL;
   static const float one             = 1.0f;
   static const char *device_extensions[] = {
      "VK_KHR_swapchain",
   };
   static const char *instance_extensions[2];
   
   instance_extensions[0] = "VK_KHR_surface";

   switch (type)
   {
      case VULKAN_WSI_WAYLAND:
         instance_extensions[1] = "VK_KHR_wayland_surface";
         break;
      case VULKAN_WSI_ANDROID:
         instance_extensions[1] = "VK_KHR_android_surface";
         break;
      case VULKAN_WSI_WIN32:
         instance_extensions[1] = "VK_KHR_win32_surface";
         break;
      case VULKAN_WSI_XLIB:
         instance_extensions[1] = "VK_KHR_xlib_surface";
         break;
      case VULKAN_WSI_XCB:
         instance_extensions[1] = "VK_KHR_xcb_surface";
         break;
      case VULKAN_WSI_MIR:
         instance_extensions[1] = "VK_KHR_mir_surface";
         break;
      case VULKAN_WSI_NONE:
      default:
         instance_extensions[1] = NULL;
         break;
   }

#ifdef _WIN32
   vulkan_library = dylib_load("vulkan-1.dll");
#else
   vulkan_library = dylib_load("libvulkan.so");
#endif

   if (!vulkan_library)
      return false;

   RARCH_LOG("Vulkan dynamic library loaded.\n");
   
   VKSYM(vk, CreateInstance);

   app.pApplicationName              = "RetroArch";
   app.applicationVersion            = 0;
   app.pEngineName                   = "RetroArch";
   app.engineVersion                 = 0;
   app.apiVersion                    = VK_API_VERSION;

   info.pApplicationInfo             = &app;
   info.enabledExtensionCount        = ARRAY_SIZE(instance_extensions);
   info.ppEnabledExtensionNames      = instance_extensions;

   if (cached_instance)
   {
      vk->context.instance           = cached_instance;
      cached_instance                = NULL;
   }
   else if (vk->context.fp.vkCreateInstance(&info, NULL, &vk->context.instance) != VK_SUCCESS)
      return false;

   VK_GET_INSTANCE_PROC_ADDR(vk, vk->context.instance, EnumeratePhysicalDevices);
   VK_GET_INSTANCE_PROC_ADDR(vk, vk->context.instance, GetPhysicalDeviceProperties);
   VK_GET_INSTANCE_PROC_ADDR(vk, vk->context.instance, GetPhysicalDeviceMemoryProperties);
   VK_GET_INSTANCE_PROC_ADDR(vk, vk->context.instance, GetPhysicalDeviceQueueFamilyProperties);
   VK_GET_INSTANCE_PROC_ADDR(vk, vk->context.instance, CreateDevice);
   VK_GET_INSTANCE_PROC_ADDR(vk, vk->context.instance, DestroyDevice);
   VK_GET_INSTANCE_PROC_ADDR(vk, vk->context.instance, GetDeviceQueue);
   VK_GET_INSTANCE_PROC_ADDR(vk, vk->context.instance, QueueWaitIdle);
   VK_GET_INSTANCE_PROC_ADDR(vk, vk->context.instance, QueueSubmit);
   VK_GET_INSTANCE_PROC_ADDR(vk, vk->context.instance, DestroySemaphore);
   VK_GET_INSTANCE_PROC_ADDR(vk, vk->context.instance, CreateSemaphore);
   VK_GET_INSTANCE_PROC_ADDR(vk, vk->context.instance, DestroyFence);
   VK_GET_INSTANCE_PROC_ADDR(vk, vk->context.instance, CreateFence);
   VK_GET_INSTANCE_PROC_ADDR(vk, vk->context.instance, ResetFences);
   VK_GET_INSTANCE_PROC_ADDR(vk, vk->context.instance, WaitForFences);
   VK_GET_INSTANCE_PROC_ADDR(vk, vk->context.instance, CmdCopyImage);
   VK_GET_INSTANCE_PROC_ADDR(vk, vk->context.instance, CmdSetScissor);
   VK_GET_INSTANCE_PROC_ADDR(vk, vk->context.instance, CmdSetViewport);
   VK_GET_INSTANCE_PROC_ADDR(vk, vk->context.instance, CmdBindPipeline);
   VK_GET_INSTANCE_PROC_ADDR(vk, vk->context.instance, CreateRenderPass);
   VK_GET_INSTANCE_PROC_ADDR(vk, vk->context.instance, CreateFramebuffer);
   VK_GET_INSTANCE_PROC_ADDR(vk, vk->context.instance, CreatePipelineLayout);
   VK_GET_INSTANCE_PROC_ADDR(vk, vk->context.instance, CreatePipelineCache);
   VK_GET_INSTANCE_PROC_ADDR(vk, vk->context.instance, CreateCommandPool);

   if (vk->context.fp.vkEnumeratePhysicalDevices(vk->context.instance,
            &gpu_count, NULL) != VK_SUCCESS)
      return false;

   gpus = (VkPhysicalDevice*)calloc(gpu_count, sizeof(*gpus));
   if (!gpus)
      return false;

   if (vk->context.fp.vkEnumeratePhysicalDevices(vk->context.instance,
            &gpu_count, gpus) != VK_SUCCESS)
      return false;

   if (gpu_count < 1)
   {
      RARCH_ERR("[Vulkan]: Failed to enumerate Vulkan physical device.\n");
      free(gpus);
      return false;
   }

   vk->context.gpu = gpus[0];
   free(gpus);

   vk->context.fp.vkGetPhysicalDeviceProperties(vk->context.gpu,
         &vk->context.gpu_properties);
   vk->context.fp.vkGetPhysicalDeviceMemoryProperties(vk->context.gpu,
         &vk->context.memory_properties);
   vk->context.fp.vkGetPhysicalDeviceQueueFamilyProperties(vk->context.gpu,
         &queue_count, NULL);

   if (queue_count < 1 || queue_count > 32)
      return false;

   vk->context.fp.vkGetPhysicalDeviceQueueFamilyProperties(vk->context.gpu,
         &queue_count, queue_properties);

   for (i = 0; i < queue_count; i++)
   {
      VkQueueFlags required = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;
      if ((queue_properties[i].queueFlags & required) == required)
      {
         vk->context.graphics_queue_index = i;
         RARCH_LOG("[Vulkan]: Device supports %u sub-queues.\n",
               queue_properties[i].queueCount);
         found_queue = true;
         break;
      }
   }

   if (!found_queue)
   {
      RARCH_ERR("[Vulkan]: Did not find suitable graphics queue.\n");
      return false;
   }

   queue_info.queueFamilyIndex         = vk->context.graphics_queue_index;
   queue_info.queueCount               = 1;
   queue_info.pQueuePriorities         = &one;

   device_info.queueCreateInfoCount    = 1;
   device_info.pQueueCreateInfos       = &queue_info;
   device_info.enabledExtensionCount   = ARRAY_SIZE(device_extensions);
   device_info.ppEnabledExtensionNames = device_extensions;
   device_info.pEnabledFeatures        = &features;

   if (cached_device)
   {
      vk->context.device = cached_device;
      cached_device = NULL;
      video_driver_ctl(RARCH_DISPLAY_CTL_SET_VIDEO_CACHE_CONTEXT_ACK, NULL);
      RARCH_LOG("[Vulkan]: Using cached Vulkan context.\n");
   }
   else if (vk->context.fp.vkCreateDevice(vk->context.gpu, &device_info,
            NULL, &vk->context.device) != VK_SUCCESS)
      return false;

   vk->context.fp.vkGetDeviceQueue(vk->context.device,
         vk->context.graphics_queue_index, 0, &vk->context.queue);

   VK_GET_INSTANCE_PROC_ADDR(vk, vk->context.instance, CmdDraw);
   VK_GET_INSTANCE_PROC_ADDR(vk,
         vk->context.instance, GetPhysicalDeviceSurfaceSupportKHR);
   VK_GET_INSTANCE_PROC_ADDR(vk,
         vk->context.instance, GetPhysicalDeviceSurfaceCapabilitiesKHR);
   VK_GET_INSTANCE_PROC_ADDR(vk,
         vk->context.instance, GetPhysicalDeviceSurfaceFormatsKHR);
   VK_GET_INSTANCE_PROC_ADDR(vk,
         vk->context.instance, GetPhysicalDeviceSurfacePresentModesKHR);
   VK_GET_INSTANCE_PROC_ADDR(vk,
         vk->context.instance, DestroySurfaceKHR);
   VK_GET_DEVICE_PROC_ADDR(vk,
         vk->context.device, CreateSwapchainKHR);
   VK_GET_DEVICE_PROC_ADDR(vk,
         vk->context.device, DestroySwapchainKHR);
   VK_GET_DEVICE_PROC_ADDR(vk,
         vk->context.device, GetSwapchainImagesKHR);
   VK_GET_DEVICE_PROC_ADDR(vk,
         vk->context.device, AcquireNextImageKHR);
   VK_GET_DEVICE_PROC_ADDR(vk,
         vk->context.device, QueuePresentKHR);

   switch (type)
   {
      case VULKAN_WSI_WAYLAND:
#ifdef HAVE_WAYLAND
         VK_GET_INSTANCE_PROC_ADDR(vk,
               vk->context.instance, CreateWaylandSurfaceKHR);
#endif
         break;
      case VULKAN_WSI_ANDROID:
#ifdef ANDROID
         VK_GET_INSTANCE_PROC_ADDR(vk,
               vk->context.instance, CreateAndroidSurfaceKHR);
#endif
         break;
      case VULKAN_WSI_WIN32:
#ifdef _WIN32
         VK_GET_INSTANCE_PROC_ADDR(vk,
               vk->context.instance, CreateWin32SurfaceKHR);
#endif
         break;
      case VULKAN_WSI_XLIB:
#ifdef HAVE_XLIB
         VK_GET_INSTANCE_PROC_ADDR(vk,
               vk->context.instance, CreateXlibSurfaceKHR);
#endif
         break;
      case VULKAN_WSI_XCB:
#ifdef HAVE_XCB
         VK_GET_INSTANCE_PROC_ADDR(vk,
               vk->context.instance, CreateXcbSurfaceKHR);
#endif
         break;
      case VULKAN_WSI_MIR:
#ifdef HAVE_MIR
         VK_GET_INSTANCE_PROC_ADDR(vk,
               vk->context.instance, CreateMirSurfaceKHR);
#endif
         break;
      case VULKAN_WSI_NONE:
      default:
         break;
   }

   vk->context.queue_lock = slock_new();
   if (!vk->context.queue_lock)
      return false;

   return true;
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
            VkWaylandSurfaceCreateInfoKHR surf_info; 

            memset(&surf_info, 0, sizeof(surf_info));

            surf_info.sType   = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
            surf_info.pNext   = NULL;
            surf_info.flags   = 0;
            surf_info.display = (struct wl_display*)display;
            surf_info.surface = (struct wl_surface*)surface;

            if (vk->context.fp.vkCreateWaylandSurfaceKHR(vk->context.instance,
                     &surf_info, NULL, &vk->vk_surface) != VK_SUCCESS)
               return false;
         }
#endif
         break;
      case VULKAN_WSI_ANDROID:
#ifdef ANDROID
         {
            VkAndroidSurfaceCreateInfoKHR surf_info;

            memset(&surf_info, 0, sizeof(surf_info));

            surf_info.sType  = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
            surf_info.flags  = 0;
            surf_info.window = (ANativeWindow*)surface;

            if (vk->context.fp.vkCreateAndroidSurfaceKHR(vk->context.instance,
                     &surf_info, NULL, &vk->vk_surface) != VK_SUCCESS)
               return false;
         }
#endif
         break;
      case VULKAN_WSI_WIN32:
#ifdef _WIN32
         {
            VkWin32SurfaceCreateInfoKHR surf_info;

            memset(&surf_info, 0, sizeof(surf_info));

            surf_info.sType     = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
            surf_info.flags     = 0;
            surf_info.hinstance = display;
            surf_info.hwnd      = surface;

            if (vk->context.fp.vkCreateWin32SurfaceKHR(vk->context.instance,
                     &surf_info, NULL, &vk->vk_surface) != VK_SUCCESS)
               return false;
         }
#endif
         break;
      case VULKAN_WSI_XLIB:
#ifdef HAVE_XLIB
         {
            VkXlibSurfaceCreateInfoKHR surf_info;

            memset(&surf_info, 0, sizeof(surf_info));

            surf_info.sType  = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
            surf_info.flags  = 0;
            surf_info.dpy    = (Display*)display;
            surf_info.window = *(const Window*)surface;

            if (vk->context.fp.vkCreateXlibSurfaceKHR(vk->context.instance,
                     &surf_info, NULL, &vk->vk_surface) 
                  != VK_SUCCESS)
               return false;
         }
#endif
         break;
      case VULKAN_WSI_XCB:
#ifdef HAVE_XCB
         {
            VkXcbSurfaceCreateInfoKHR surf_info;

            memset(&surf_info, 0, sizeof(surf_info));

            surf_info.sType      = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
            surf_info.flags      = 0;
            surf_info.connection = XGetXCBConnection((Display*)display);
            surf_info.window     = *(const xcb_window_t*)surface;

            if (vk->context.fp.vkCreateXcbSurfaceKHR(vk->context.instance,
                     &surf_info, NULL, &vk->vk_surface) 
                  != VK_SUCCESS)
               return false;
         }
#endif
         break;
      case VULKAN_WSI_MIR:
#ifdef HAVE_MIR
         {
            VkMirSurfaceCreateInfoKHR surf_info;

            memset(&surf_info, 0, sizeof(surf_info));

            surf_info.sType      = VK_STRUCTURE_TYPE_MIR_SURFACE_CREATE_INFO_KHR;
            surf_info.connection = display;
            surf_info.mirSurface = surface;

            if (vk->context.fp.vkCreateMirSurfaceKHR(vk->context.instance,
                     &surf_info, NULL, &vk->vk_surface) 
                  != VK_SUCCESS)
               return false;
         }
#endif
         break;
      case VULKAN_WSI_NONE:
      default:
         return false;
   }

   if (!vulkan_create_swapchain(
            vk, width, height, swap_interval))
      return false;

   return true;
}

void vulkan_present(gfx_ctx_vulkan_data_t *vk, unsigned index)
{
   VkResult result            = VK_SUCCESS;
   VkResult err               = VK_SUCCESS;
   VkPresentInfoKHR present   = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
   present.swapchainCount     = 1;
   present.pSwapchains        = &vk->swapchain;
   present.pImageIndices      = &index;
   present.pResults           = &result;
   present.waitSemaphoreCount = 1;
   present.pWaitSemaphores    = &vk->context.swapchain_semaphores[index];

   /* Better hope QueuePresent doesn't block D: */
   slock_lock(vk->context.queue_lock);
   err = vk->context.fp.vkQueuePresentKHR(vk->context.queue, &present);

   if (err != VK_SUCCESS || result != VK_SUCCESS)
   {
      RARCH_LOG("[Vulkan]: QueuePresent failed, invalidating swapchain.\n");
      vk->context.invalid_swapchain = true;
   }
   slock_unlock(vk->context.queue_lock);
}

void vulkan_context_destroy(gfx_ctx_vulkan_data_t *vk,
      bool destroy_surface)
{
   unsigned i;
   if (vk->context.queue)
      vk->context.fp.vkQueueWaitIdle(vk->context.queue);
   if (vk->swapchain)
      vk->context.fp.vkDestroySwapchainKHR(vk->context.device,
            vk->swapchain, NULL);

   if (destroy_surface && vk->vk_surface != VK_NULL_HANDLE)
      vk->context.fp.vkDestroySurfaceKHR(vk->context.instance,
            vk->vk_surface, NULL);

   for (i = 0; i < VULKAN_MAX_SWAPCHAIN_IMAGES; i++)
   {
      if (vk->context.swapchain_semaphores[i] != VK_NULL_HANDLE)
         vk->context.fp.vkDestroySemaphore(vk->context.device,
               vk->context.swapchain_semaphores[i], NULL);
      if (vk->context.swapchain_fences[i] != VK_NULL_HANDLE)
         vk->context.fp.vkDestroyFence(vk->context.device,
               vk->context.swapchain_fences[i], NULL);
   }

   if (video_driver_ctl(RARCH_DISPLAY_CTL_IS_VIDEO_CACHE_CONTEXT, NULL))
   {
      cached_device   = vk->context.device;
      cached_instance = vk->context.instance;
   }
   else
   {
      if (vk->context.device)
         vk->context.fp.vkDestroyDevice(vk->context.device, NULL);
      if (vk->context.instance)
         vkDestroyInstance(vk->context.instance, NULL);
   }

   if (vulkan_library)
      dylib_close(vulkan_library);

}

void vulkan_acquire_next_image(gfx_ctx_vulkan_data_t *vk)
{
   unsigned index;
   VkResult err;
   VkFence fence;
   VkFence *next_fence;
   VkSemaphoreCreateInfo sem_info = 
   { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
   VkFenceCreateInfo info         = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };

   vk->context.fp.vkCreateFence(vk->context.device, &info, NULL, &fence);

   err = vk->context.fp.vkAcquireNextImageKHR(vk->context.device,
         vk->swapchain, UINT64_MAX,
         VK_NULL_HANDLE, fence, &vk->context.current_swapchain_index);

   index = vk->context.current_swapchain_index;
   if (vk->context.swapchain_semaphores[index] == VK_NULL_HANDLE)
      vk->context.fp.vkCreateSemaphore(vk->context.device, &sem_info,
            NULL, &vk->context.swapchain_semaphores[index]);

   vk->context.fp.vkWaitForFences(vk->context.device, 1, &fence, true, UINT64_MAX);
   vk->context.fp.vkDestroyFence(vk->context.device, fence, NULL);

   next_fence = &vk->context.swapchain_fences[index];
   if (*next_fence != VK_NULL_HANDLE)
   {
      vk->context.fp.vkWaitForFences(vk->context.device, 1, next_fence, true, UINT64_MAX);
      vk->context.fp.vkResetFences(vk->context.device, 1, next_fence);
   }
   else
      vk->context.fp.vkCreateFence(vk->context.device, &info, NULL, next_fence);

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
   uint32_t desired_swapchain_images;
   VkSurfaceCapabilitiesKHR surface_properties;
   VkSurfaceFormatKHR formats[256];
   VkSurfaceFormatKHR format;
   VkExtent2D swapchain_size;
   VkSwapchainKHR old_swapchain;
   VkSurfaceTransformFlagBitsKHR pre_transform;

   /* TODO: Properly query these. */
   VkPresentModeKHR swapchain_present_mode = swap_interval 
      ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_MAILBOX_KHR;
   VkSwapchainCreateInfoKHR info = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };

   RARCH_LOG("[Vulkan]: Creating swapchain with present mode: %u\n",
         (unsigned)swapchain_present_mode);

   vk->context.fp.vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vk->context.gpu,
         vk->vk_surface, &surface_properties);
   vk->context.fp.vkGetPhysicalDeviceSurfaceFormatsKHR(vk->context.gpu,
         vk->vk_surface, &format_count, NULL);
   vk->context.fp.vkGetPhysicalDeviceSurfaceFormatsKHR(vk->context.gpu,
         vk->vk_surface, &format_count, formats);

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

      format = formats[0];
   }

   if (surface_properties.currentExtent.width == -1)
   {
      swapchain_size.width     = width;
      swapchain_size.height    = height;
   }
   else
      swapchain_size           = surface_properties.currentExtent;

   desired_swapchain_images    = surface_properties.minImageCount + 1;

   /* Limit latency. */
   if (desired_swapchain_images > 3)
      desired_swapchain_images = 3;
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

   old_swapchain               = vk->swapchain;

   info.surface                = vk->vk_surface;
   info.minImageCount          = desired_swapchain_images;
   info.imageFormat            = format.format;
   info.imageColorSpace        = format.colorSpace;
   info.imageExtent.width      = swapchain_size.width;
   info.imageExtent.height     = swapchain_size.height;
   info.imageArrayLayers       = 1;
   info.imageUsage             = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT 
      | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
   info.imageSharingMode       = VK_SHARING_MODE_EXCLUSIVE;
   info.preTransform           = pre_transform;
   info.compositeAlpha         = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
   info.presentMode            = swapchain_present_mode;
   info.clipped                = true;
   info.oldSwapchain           = old_swapchain;

   vk->context.fp.vkCreateSwapchainKHR(vk->context.device, &info, NULL, &vk->swapchain);
   if (old_swapchain != VK_NULL_HANDLE)
      vk->context.fp.vkDestroySwapchainKHR(vk->context.device, old_swapchain, NULL);

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

   vk->context.fp.vkGetSwapchainImagesKHR(vk->context.device, vk->swapchain,
         &vk->context.num_swapchain_images, NULL);
   vk->context.fp.vkGetSwapchainImagesKHR(vk->context.device, vk->swapchain,
         &vk->context.num_swapchain_images, vk->context.swapchain_images);

   RARCH_LOG("[Vulkan]: Got %u swapchain images.\n", vk->context.num_swapchain_images);

   for (i = 0; i < vk->context.num_swapchain_images; i++)
   {
      if (vk->context.swapchain_fences[i])
      {
         vk->context.fp.vkDestroyFence(vk->context.device,
               vk->context.swapchain_fences[i], NULL);
         vk->context.swapchain_fences[i] = VK_NULL_HANDLE;
      }
   }

   vulkan_acquire_next_image(vk);

   return true;
}
