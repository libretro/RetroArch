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

#include "vulkan_common.h"
#include <retro_assert.h>

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
      uint32_t device_reqs, uint32_t host_reqs)
{
   uint32_t i;
   for (i = 0; i < VK_MAX_MEMORY_TYPES; i++)
   {
      if ((device_reqs & (1u << i)) &&
            (mem_props->memoryTypes[i].propertyFlags & host_reqs) == host_reqs)
         return i;
   }

   return vulkan_find_memory_type(mem_props, device_reqs, 0);
}

void vulkan_map_persistent_texture(VkDevice device, struct vk_texture *texture)
{
   vkMapMemory(device, texture->memory, texture->offset, texture->size, 0, &texture->mapped);
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
   /* TODO: Evaluate how we should do texture uploads on discrete cards optimally.
    * For integrated GPUs, using linear texture is highly desirable to avoid extra copies, but
    * we might need to take a DMA transfer with block interleave on desktop GPUs.
    *
    * Also, Vulkan drivers are not required to support sampling from linear textures
    * (only TRANSFER), but seems to work fine on GPUs I've tested so far. */

   VkDevice device = vk->context->device;
   struct vk_texture tex;
   VkImageCreateInfo info = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };

   VkImageViewCreateInfo view = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
   VkMemoryAllocateInfo alloc = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
   VkImageSubresource subresource = { VK_IMAGE_ASPECT_COLOR_BIT };
   VkMemoryRequirements mem_reqs;
   VkSubresourceLayout layout;

   if (type == VULKAN_TEXTURE_STATIC && !initial)
      retro_assert(0 && "Static textures must have initial data.\n");

   memset(&tex, 0, sizeof(tex));

   info.imageType = VK_IMAGE_TYPE_2D;
   info.format = format;
   info.extent.width = width;
   info.extent.height = height;
   info.extent.depth = 1;
   info.mipLevels = 1;
   info.arrayLayers = 1;
   info.samples = VK_SAMPLE_COUNT_1_BIT;
   info.tiling = type != VULKAN_TEXTURE_STATIC ? VK_IMAGE_TILING_LINEAR : VK_IMAGE_TILING_OPTIMAL;
   info.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
   if (type == VULKAN_TEXTURE_STATIC)
      info.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
   if (type == VULKAN_TEXTURE_READBACK)
      info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
   info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

   /* We'll transition this on first use for streamed textures. */
   info.initialLayout = type == VULKAN_TEXTURE_STREAMED ?
      VK_IMAGE_LAYOUT_PREINITIALIZED :
      VK_IMAGE_LAYOUT_UNDEFINED;

   vkCreateImage(device, &info, NULL, &tex.image);
#if 0
   vulkan_track_alloc(tex.image);
#endif

   vkGetImageMemoryRequirements(device, tex.image, &mem_reqs);

   alloc.allocationSize = mem_reqs.size;

   if (type == VULKAN_TEXTURE_STATIC)
   {
      alloc.memoryTypeIndex = vulkan_find_memory_type_fallback(&vk->context->memory_properties,
            mem_reqs.memoryTypeBits,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
   }
   else
   {
      /* This must exist. */
      alloc.memoryTypeIndex = vulkan_find_memory_type(&vk->context->memory_properties,
            mem_reqs.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
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
      tex.memory = old->memory;
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

   view.image = tex.image;
   view.viewType = VK_IMAGE_VIEW_TYPE_2D;
   view.format = format;
   if (swizzle)
      view.components = *swizzle;
   else
   {
      view.components.r = VK_COMPONENT_SWIZZLE_R;
      view.components.g = VK_COMPONENT_SWIZZLE_G;
      view.components.b = VK_COMPONENT_SWIZZLE_B;
      view.components.a = VK_COMPONENT_SWIZZLE_A;
   }
   view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
   view.subresourceRange.levelCount = 1;
   view.subresourceRange.layerCount = 1;

   vkCreateImageView(device, &view, NULL, &tex.view);

   vkGetImageSubresourceLayout(device, tex.image, &subresource, &layout);
   tex.stride = layout.rowPitch;
   tex.offset = layout.offset;
   tex.size = layout.size;
   tex.layout = info.initialLayout;

   tex.width = width;
   tex.height = height;
   tex.format = format;

   if (initial && type == VULKAN_TEXTURE_STREAMED)
   {
      unsigned bpp = vulkan_format_to_bpp(tex.format);
      unsigned stride = tex.width * bpp;
      unsigned x, y;
      uint8_t *dst;
      const uint8_t *src;
      void *ptr;

      vkMapMemory(device, tex.memory, tex.offset, tex.size, 0, &ptr);

      dst = (uint8_t*)ptr;
      src = (const uint8_t*)initial;
      for (y = 0; y < tex.height; y++, dst += tex.stride, src += stride)
         memcpy(dst, src, width * bpp);

      vkUnmapMemory(device, tex.memory);
   }
   else if (initial && type == VULKAN_TEXTURE_STATIC)
   {
      VkCommandBufferAllocateInfo info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
      VkCommandBufferBeginInfo begin_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
      VkSubmitInfo submit_info = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
      VkImageCopy region;

      VkCommandBuffer staging;
      unsigned bpp = vulkan_format_to_bpp(tex.format);
      struct vk_texture tmp = vulkan_create_texture(vk, NULL,
            width, height, format, initial, NULL, VULKAN_TEXTURE_STREAMED);

      info.commandPool = vk->staging_pool;
      info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
      info.commandBufferCount = 1;
      vkAllocateCommandBuffers(vk->context->device, &info, &staging);

      begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
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
      region.extent.width = width;
      region.extent.height = height;
      region.extent.depth = 1;
      region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      region.srcSubresource.layerCount = 1;
      region.dstSubresource = region.srcSubresource;

      vkCmdCopyImage(staging,
            tmp.image, VK_IMAGE_LAYOUT_GENERAL,
            tex.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, &region);

      vulkan_image_layout_transition(vk, staging, tex.image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);

      vkEndCommandBuffer(staging);
      submit_info.commandBufferCount = 1;
      submit_info.pCommandBuffers = &staging;

      slock_lock(vk->context->queue_lock);
      vkQueueSubmit(vk->context->queue, 1, &submit_info, VK_NULL_HANDLE);
      /* TODO: Very crude, but texture uploads only happen during init,
       * so waiting for GPU to complete transfer and blocking isn't a big deal. */
      vkQueueWaitIdle(vk->context->queue);
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
      const struct vk_texture *texture,
      VkSampler sampler)
{
   VkWriteDescriptorSet write = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
   VkDescriptorImageInfo image_info;

   image_info.sampler = sampler;
   image_info.imageView = texture->view;
   image_info.imageLayout = texture->layout;

   write.dstSet = set;
   write.dstBinding = 0;
   write.descriptorCount = 1;
   write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
   write.pImageInfo = &image_info;

   vkUpdateDescriptorSets(device, 1, &write, 0, NULL);
}

void vulkan_transition_texture(vk_t *vk, struct vk_texture *texture)
{
   /* Transition to GENERAL layout for linear streamed textures.
    * We're using linear textures here, so only GENERAL layout is supported.
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
      const VkRect2D sci = {{ vk->vp.x, vk->vp.y }, { vk->vp.width, vk->vp.height }};
      vkCmdSetViewport(vk->cmd, 0, 1, &vk->vk_vp);
      vkCmdSetScissor(vk->cmd, 0, 1, &sci);

      vk->tracker.dirty &= ~VULKAN_DIRTY_DYNAMIC_BIT;
   }
}

void vulkan_draw_triangles(vk_t *vk, const struct vk_draw_triangles *call)
{
   vulkan_transition_texture(vk, call->texture);

   if (call->pipeline != vk->tracker.pipeline)
   {
      vkCmdBindPipeline(vk->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, call->pipeline);
      vk->tracker.pipeline = call->pipeline;

      /* Changing pipeline invalidates dynamic state. */
      vk->tracker.dirty |= VULKAN_DIRTY_DYNAMIC_BIT;
   }

   vulkan_check_dynamic_state(vk);

   /* Upload descriptors */
   {
      struct vk_draw_uniform ubo;
      VkDescriptorSet set;

      ubo.mvp = *call->mvp;
      ubo.texsize[0] = call->texture->width;
      ubo.texsize[1] = call->texture->height;
      ubo.texsize[2] = 1.0f / call->texture->width;
      ubo.texsize[3] = 1.0f / call->texture->height;

      if (call->texture->view != vk->tracker.view || call->sampler != vk->tracker.sampler)
      {
         set = vulkan_descriptor_manager_alloc(vk->context->device, &vk->chain->descriptor_manager);
         vulkan_write_quad_descriptors(vk->context->device,
               set,
               call->texture,
               call->sampler);

         vkCmdBindDescriptorSets(vk->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
               vk->pipelines.layout, 0,
               1, &set, 0, NULL);

         vk->tracker.view = call->texture->view;
         vk->tracker.sampler = call->sampler;
      }

      vkCmdPushConstants(vk->cmd, vk->pipelines.layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            0, sizeof(struct vk_draw_uniform), &ubo);
   }

   /* VBO is already uploaded. */
   vkCmdBindVertexBuffers(vk->cmd, 0, 1,
         &call->vbo->buffer, &call->vbo->offset);

   /* Draw the quad */
   vkCmdDraw(vk->cmd, call->vertices, 1, 0, 0);
}

void vulkan_draw_quad(vk_t *vk, const struct vk_draw_quad *quad)
{
   vulkan_transition_texture(vk, quad->texture);

   if (quad->pipeline != vk->tracker.pipeline)
   {
      vkCmdBindPipeline(vk->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, quad->pipeline);
      vk->tracker.pipeline = quad->pipeline;

      /* Changing pipeline invalidates dynamic state. */
      vk->tracker.dirty |= VULKAN_DIRTY_DYNAMIC_BIT;
   }

   vulkan_check_dynamic_state(vk);

   /* Upload descriptors */
   {
      struct vk_draw_uniform ubo;
      VkDescriptorSet set;
      struct vk_buffer_range range;
      if (!vulkan_buffer_chain_alloc(vk->context, &vk->chain->ubo,
               sizeof(struct vk_draw_uniform), &range))
         return;

      ubo.mvp = *quad->mvp;
      ubo.texsize[0] = quad->texture->width;
      ubo.texsize[1] = quad->texture->height;
      ubo.texsize[2] = 1.0f / quad->texture->width;
      ubo.texsize[3] = 1.0f / quad->texture->height;

      if (quad->texture->view != vk->tracker.view || quad->sampler != vk->tracker.sampler)
      {
         set = vulkan_descriptor_manager_alloc(vk->context->device, &vk->chain->descriptor_manager);
         vulkan_write_quad_descriptors(vk->context->device,
               set,
               quad->texture,
               quad->sampler);

         vkCmdBindDescriptorSets(vk->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
               vk->pipelines.layout, 0,
               1, &set, 0, NULL);

         vk->tracker.view = quad->texture->view;
         vk->tracker.sampler = quad->sampler;
      }

      vkCmdPushConstants(vk->cmd, vk->pipelines.layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            0, sizeof(struct vk_draw_uniform), &ubo);
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

void vulkan_image_layout_transition(vk_t *vk, VkCommandBuffer cmd, VkImage image,
      VkImageLayout old_layout, VkImageLayout new_layout,
      VkAccessFlags srcAccess, VkAccessFlags dstAccess,
      VkPipelineStageFlags srcStages, VkPipelineStageFlags dstStages)
{
   VkImageMemoryBarrier barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };

   barrier.srcAccessMask = srcAccess;
   barrier.dstAccessMask = dstAccess;
   barrier.oldLayout = old_layout;
   barrier.newLayout = new_layout;
   barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
   barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
   barrier.image = image;
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
   VkBufferCreateInfo info = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };

   info.size = size;
   info.usage = usage;
   info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
   vkCreateBuffer(context->device, &info, NULL, &buffer.buffer);

   vkGetBufferMemoryRequirements(context->device, buffer.buffer, &mem_reqs);

   alloc.allocationSize = mem_reqs.size;
   alloc.memoryTypeIndex = vulkan_find_memory_type(&context->memory_properties,
         mem_reqs.memoryTypeBits,
         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
   vkAllocateMemory(context->device, &alloc, NULL, &buffer.memory);
   vkBindBufferMemory(context->device, buffer.buffer, buffer.memory, 0);

   buffer.size = alloc.allocationSize;

   vkMapMemory(context->device, buffer.memory, 0, buffer.size, 0, &buffer.mapped);
   return buffer;
}

void vulkan_destroy_buffer(VkDevice device, struct vk_buffer *buffer)
{
   vkUnmapMemory(device, buffer->memory);
   vkFreeMemory(device, buffer->memory, NULL);
   vkDestroyBuffer(device, buffer->buffer, NULL);
   memset(buffer, 0, sizeof(*buffer));
}

static struct vk_descriptor_pool *vulkan_alloc_descriptor_pool(VkDevice device,
      const struct vk_descriptor_manager *manager)
{
   unsigned i;
   VkDescriptorPoolCreateInfo pool_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
   VkDescriptorSetAllocateInfo alloc_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };

   struct vk_descriptor_pool *pool = (struct vk_descriptor_pool*)calloc(1, sizeof(*pool));
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

VkDescriptorSet vulkan_descriptor_manager_alloc(VkDevice device, struct vk_descriptor_manager *manager)
{
   if (manager->count < VULKAN_DESCRIPTOR_MANAGER_BLOCK_SETS)
      return manager->current->sets[manager->count++];

   while (manager->current->next)
   {
      manager->current = manager->current->next;
      manager->count = 0;
      return manager->current->sets[manager->count++];
   }

   manager->current->next = vulkan_alloc_descriptor_pool(device, manager);
   retro_assert(manager->current->next);

   manager->current = manager->current->next;
   manager->count = 0;
   return manager->current->sets[manager->count++];
}

void vulkan_descriptor_manager_restart(struct vk_descriptor_manager *manager)
{
   manager->current = manager->head;
   manager->count = 0;
}

struct vk_descriptor_manager vulkan_create_descriptor_manager(VkDevice device,
      const VkDescriptorPoolSize *sizes, unsigned num_sizes, VkDescriptorSetLayout set_layout)
{
   struct vk_descriptor_manager manager;
   memset(&manager, 0, sizeof(manager));
   retro_assert(num_sizes <= VULKAN_MAX_DESCRIPTOR_POOL_SIZES);
   memcpy(manager.sizes, sizes, num_sizes * sizeof(*sizes));
   manager.num_sizes = num_sizes;
   manager.set_layout = set_layout;

   manager.head = vulkan_alloc_descriptor_pool(device, &manager);
   retro_assert(manager.head);
   return manager;
}

void vulkan_destroy_descriptor_manager(VkDevice device, struct vk_descriptor_manager *manager)
{
   struct vk_descriptor_pool *node = manager->head;

   while (node)
   {
      struct vk_descriptor_pool *next = node->next;

      vkFreeDescriptorSets(device, node->pool, VULKAN_DESCRIPTOR_MANAGER_BLOCK_SETS, node->sets);
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

static bool vulkan_buffer_chain_suballoc(struct vk_buffer_chain *chain, size_t size, struct vk_buffer_range *range)
{
   VkDeviceSize next_offset = chain->offset + size;
   if (next_offset <= chain->current->buffer.size)
   {
      range->data = (uint8_t*)chain->current->buffer.mapped + chain->offset;
      range->buffer = chain->current->buffer.buffer;
      range->offset = chain->offset;
      chain->offset = (next_offset + chain->alignment - 1) & ~(chain->alignment - 1);
      return true;
   }
   else
      return false;
}

static struct vk_buffer_node *vulkan_buffer_chain_alloc_node(
      const struct vulkan_context *context,
      size_t size, VkBufferUsageFlags usage)
{
   struct vk_buffer_node *node = (struct vk_buffer_node*)calloc(1, sizeof(*node));
   if (!node)
      return NULL;

   node->buffer = vulkan_create_buffer(context, size, usage);
   return node;
}

struct vk_buffer_chain vulkan_buffer_chain_init(VkDeviceSize block_size,
      VkDeviceSize alignment,
      VkBufferUsageFlags usage)
{
   struct vk_buffer_chain chain = { block_size, alignment, 0, usage, NULL, NULL };
   return chain;
}

void vulkan_buffer_chain_discard(struct vk_buffer_chain *chain)
{
   chain->current = chain->head;
   chain->offset = 0;
}

bool vulkan_buffer_chain_alloc(const struct vulkan_context *context,
      struct vk_buffer_chain *chain, size_t size, struct vk_buffer_range *range)
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
    * buffer here than block_size in case we have a very large allocation. */
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

