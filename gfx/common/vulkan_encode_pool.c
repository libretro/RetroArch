/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "vulkan_encode_pool.h"
#include <stdlib.h>
#include <stdint.h>

static bool vep_video_format(const vulkan_encode_pool_config_t *c,
      const vulkan_encode_pool_ops_t *ops, const VkVideoProfileListInfoKHR *profiles,
      VkImageUsageFlags usage, VkImageCreateFlags flags)
{
   VkPhysicalDeviceVideoFormatInfoKHR info;
   VkVideoFormatPropertiesKHR *formats;
   uint32_t count = 0, capacity, i;
   bool valid = false;
   info.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VIDEO_FORMAT_INFO_KHR;
   info.pNext = profiles;
   info.imageUsage = usage;
   if (ops->video_formats(c->physical, &info, &count, NULL) != VK_SUCCESS ||
         !count || count > 64)
      return false;
   capacity = count;
   formats = (VkVideoFormatPropertiesKHR*)calloc(count, sizeof(*formats));
   if (!formats)
      return false;
   for (i = 0; i < count; i++)
   {
      formats[i].sType = VK_STRUCTURE_TYPE_VIDEO_FORMAT_PROPERTIES_KHR;
      formats[i].pNext = NULL;
   }
   if (ops->video_formats(c->physical, &info, &count, formats) == VK_SUCCESS &&
         count <= capacity)
      for (i = 0; i < count; i++)
         if (formats[i].format == c->format &&
             formats[i].imageType == VK_IMAGE_TYPE_2D &&
             formats[i].imageTiling == VK_IMAGE_TILING_OPTIMAL &&
             (formats[i].imageUsageFlags & usage) == usage &&
             (formats[i].imageCreateFlags & flags) == flags)
            valid = true;
   free(formats);
   return valid;
}
static bool vep_image_format(const vulkan_encode_pool_config_t *c,
      const vulkan_encode_pool_ops_t *ops, const VkVideoProfileListInfoKHR *profiles,
      VkImageUsageFlags usage, VkImageCreateFlags flags)
{
   VkPhysicalDeviceImageFormatInfo2 info;
   VkImageFormatProperties2 properties;
   info.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2;
   info.pNext = profiles;
   info.format = c->format;
   info.type = VK_IMAGE_TYPE_2D;
   info.tiling = VK_IMAGE_TILING_OPTIMAL;
   info.usage = usage;
   info.flags = flags;
   properties.sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2;
   properties.pNext = NULL;
   return ops->image_properties(c->physical, &info, &properties) == VK_SUCCESS &&
         properties.imageFormatProperties.maxExtent.width >= c->extent.width &&
         properties.imageFormatProperties.maxExtent.height >= c->extent.height &&
         properties.imageFormatProperties.maxExtent.depth >= 1 &&
         properties.imageFormatProperties.maxMipLevels >= 1 &&
         properties.imageFormatProperties.maxArrayLayers >= 1 &&
         (properties.imageFormatProperties.sampleCounts & VK_SAMPLE_COUNT_1_BIT);
}
static bool vep_storage(const vulkan_encode_pool_config_t *c,
      const vulkan_encode_pool_ops_t *ops, const VkVideoProfileListInfoKHR *profiles)
{
   VkFormatProperties2 properties;
   VkFormat formats[2];
   unsigned i;
   VkImageCreateFlags flags = VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT |
         VK_IMAGE_CREATE_EXTENDED_USAGE_BIT;
   if (!c->storage_image_extended_formats ||
       !vep_video_format(c, ops, profiles, VK_IMAGE_USAGE_VIDEO_ENCODE_SRC_BIT_KHR, flags) ||
       !vep_image_format(c, ops, profiles, VK_IMAGE_USAGE_VIDEO_ENCODE_SRC_BIT_KHR |
             VK_IMAGE_USAGE_STORAGE_BIT, flags))
      return false;
   formats[0] = VK_FORMAT_R8_UNORM;
   formats[1] = VK_FORMAT_R8G8_UNORM;
   for (i = 0; i < 2; i++)
   {
      properties.sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2;
      properties.pNext = NULL;
      ops->format_properties(c->physical, formats[i], &properties);
      if (!(properties.formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT))
         return false;
   }
   return true;
}
static bool vep_allocate(vulkan_encode_pool_t *p,
      const VkPhysicalDeviceMemoryProperties *properties,
      const VkMemoryRequirements *requirements, VkImage image, VkBuffer buffer,
      VkMemoryPropertyFlags required, VkMemoryPropertyFlags preferred,
      VkDeviceMemory *memory, VkMemoryPropertyFlags *selected)
{
   VkMemoryDedicatedAllocateInfo dedicated;
   VkMemoryAllocateInfo allocate;
   uint32_t i, chosen = VK_MAX_MEMORY_TYPES;
   for (i = 0; i < properties->memoryTypeCount; i++)
   {
      VkMemoryPropertyFlags flags = properties->memoryTypes[i].propertyFlags;
      if (!(requirements->memoryTypeBits & (1u << i)) ||
          (flags & required) != required || (flags & VK_MEMORY_PROPERTY_PROTECTED_BIT))
         continue;
      if (chosen == VK_MAX_MEMORY_TYPES)
         chosen = i;
      if ((flags & preferred) == preferred)
      {
         chosen = i;
         break;
      }
   }
   if (chosen == VK_MAX_MEMORY_TYPES || !requirements->size)
      return false;
   dedicated.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO;
   dedicated.pNext = NULL;
   dedicated.image = image;
   dedicated.buffer = buffer;
   allocate.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
   allocate.pNext = &dedicated;
   allocate.allocationSize = requirements->size;
   allocate.memoryTypeIndex = chosen;
   if (p->ops->allocate(p->device, &allocate, NULL, memory) != VK_SUCCESS)
   {
      *memory = VK_NULL_HANDLE;
      return false;
   }
   if (selected)
      *selected = properties->memoryTypes[chosen].propertyFlags;
   return true;
}
static bool vep_view(vulkan_encode_pool_t *p, VkImage image,
      VkFormat format, VkImageAspectFlags aspect, VkImageUsageFlags usage,
      VkImageView *view)
{
   VkImageViewUsageCreateInfo view_usage;
   VkImageViewCreateInfo create;
   view_usage.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO;
   view_usage.pNext = NULL;
   view_usage.usage = usage;
   create.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
   create.pNext = &view_usage;
   create.flags = 0;
   create.image = image;
   create.viewType = VK_IMAGE_VIEW_TYPE_2D;
   create.format = format;
   create.components.r = create.components.g = create.components.b =
         create.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
   create.subresourceRange.aspectMask = aspect;
   create.subresourceRange.baseMipLevel = 0;
   create.subresourceRange.levelCount = 1;
   create.subresourceRange.baseArrayLayer = 0;
   create.subresourceRange.layerCount = 1;
   if (p->ops->create_view(p->device, &create, NULL, view) != VK_SUCCESS)
   {
      *view = VK_NULL_HANDLE;
      return false;
   }
   return true;
}
static bool vep_image(vulkan_encode_pool_t *p,
      const vulkan_encode_pool_config_t *c, const VkVideoProfileListInfoKHR *profiles,
      const VkPhysicalDeviceMemoryProperties *properties, bool input,
      VkImage *image, VkDeviceMemory *memory, VkImageView *view, VkImageView *planes)
{
   VkImageCreateInfo create;
   VkMemoryRequirements requirements;
   uint32_t families[2];
   VkImageUsageFlags video_usage = input ? VK_IMAGE_USAGE_VIDEO_ENCODE_SRC_BIT_KHR :
         VK_IMAGE_USAGE_VIDEO_ENCODE_DPB_BIT_KHR;
   create.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
   create.pNext = profiles;
   create.flags = input && p->storage_images ?
         VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT | VK_IMAGE_CREATE_EXTENDED_USAGE_BIT : 0;
   create.imageType = VK_IMAGE_TYPE_2D;
   create.format = c->format;
   create.extent.width = c->extent.width;
   create.extent.height = c->extent.height;
   create.extent.depth = 1;
   create.mipLevels = create.arrayLayers = 1;
   create.samples = VK_SAMPLE_COUNT_1_BIT;
   create.tiling = VK_IMAGE_TILING_OPTIMAL;
   create.usage = video_usage;
   if (input)
      create.usage |= p->storage_images ? VK_IMAGE_USAGE_STORAGE_BIT : VK_IMAGE_USAGE_TRANSFER_DST_BIT;
   families[0] = c->encode_family;
   families[1] = c->producer_family;
   create.sharingMode = input && families[0] != families[1] ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE;
   create.queueFamilyIndexCount = create.sharingMode == VK_SHARING_MODE_CONCURRENT ? 2 : 0;
   create.pQueueFamilyIndices = create.queueFamilyIndexCount ? families : NULL;
   create.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
   if (p->ops->create_image(p->device, &create, NULL, image) != VK_SUCCESS)
   {
      *image = VK_NULL_HANDLE;
      return false;
   }
   p->ops->image_memory(p->device, *image, &requirements);
   if (!vep_allocate(p, properties, &requirements, *image, VK_NULL_HANDLE, 0,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, memory, NULL) ||
       p->ops->bind_image(p->device, *image, *memory, 0) != VK_SUCCESS ||
       !vep_view(p, *image, c->format, VK_IMAGE_ASPECT_COLOR_BIT, video_usage, view))
      return false;
   if (input && p->storage_images)
      return vep_view(p, *image, VK_FORMAT_R8_UNORM, VK_IMAGE_ASPECT_PLANE_0_BIT,
                  VK_IMAGE_USAGE_STORAGE_BIT, &planes[0]) &&
            vep_view(p, *image, VK_FORMAT_R8G8_UNORM, VK_IMAGE_ASPECT_PLANE_1_BIT,
                  VK_IMAGE_USAGE_STORAGE_BIT, &planes[1]);
   return true;
}
static bool vep_buffer(vulkan_encode_pool_t *p,
      const VkPhysicalDeviceMemoryProperties *properties,
      const VkVideoProfileListInfoKHR *profiles, VkDeviceSize bytes,
      bool output, vulkan_encode_buffer_t *buffer)
{
   VkBufferCreateInfo create;
   VkMemoryRequirements requirements;
   create.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
   create.pNext = output ? profiles : NULL;
   create.flags = 0;
   create.size = bytes;
   create.usage = output ? VK_BUFFER_USAGE_VIDEO_ENCODE_DST_BIT_KHR :
         VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
   create.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
   create.queueFamilyIndexCount = 0;
   create.pQueueFamilyIndices = NULL;
   if (p->ops->create_buffer(p->device, &create, NULL, &buffer->buffer) != VK_SUCCESS)
   {
      buffer->buffer = VK_NULL_HANDLE;
      return false;
   }
   buffer->size = bytes;
   p->ops->buffer_memory(p->device, buffer->buffer, &requirements);
   if (!vep_allocate(p, properties, &requirements, VK_NULL_HANDLE, buffer->buffer,
            output ? VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT : 0,
            output ? VK_MEMORY_PROPERTY_HOST_CACHED_BIT : VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            &buffer->memory, &buffer->properties) ||
         p->ops->bind_buffer(p->device, buffer->buffer, buffer->memory, 0) != VK_SUCCESS)
      return false;
   if (output && p->ops->map(p->device, buffer->memory, 0, bytes, 0,
            &buffer->mapped) != VK_SUCCESS)
   {
      buffer->mapped = NULL;
      return false;
   }
   return true;
}
static void vep_free_buffer(vulkan_encode_pool_t *p, vulkan_encode_buffer_t *buffer)
{
   if (buffer->mapped)
      p->ops->unmap(p->device, buffer->memory);
   if (buffer->buffer)
      p->ops->destroy_buffer(p->device, buffer->buffer, NULL);
   if (buffer->memory)
      p->ops->free_memory(p->device, buffer->memory, NULL);
}
void vulkan_encode_pool_free(vulkan_encode_pool_t *p)
{
   unsigned i, j;
   if (!p)
      return;
   if (p->commands)
      p->ops->destroy_commands(p->device, p->commands, NULL);
   if (p->queries)
      p->ops->destroy_queries(p->device, p->queries, NULL);
   if (p->ready)
      p->ops->destroy_semaphore(p->device, p->ready, NULL);
   if (p->done)
      p->ops->destroy_semaphore(p->device, p->done, NULL);
   for (i = 0; i < p->count; i++)
   {
      vulkan_encode_pool_slot_t *slot = &p->slot[i];
      if (slot->fence)
         p->ops->destroy_fence(p->device, slot->fence, NULL);
      vep_free_buffer(p, &slot->output);
      vep_free_buffer(p, &slot->conversion);
      for (j = 0; j < 2; j++)
         if (slot->planes[j])
            p->ops->destroy_view(p->device, slot->planes[j], NULL);
      if (slot->view)
         p->ops->destroy_view(p->device, slot->view, NULL);
      if (slot->image)
         p->ops->destroy_image(p->device, slot->image, NULL);
      if (slot->memory)
         p->ops->free_memory(p->device, slot->memory, NULL);
   }
   for (i = 0; i < 2; i++)
   {
      if (p->dpb.view[i])
         p->ops->destroy_view(p->device, p->dpb.view[i], NULL);
      if (p->dpb.image[i])
         p->ops->destroy_image(p->device, p->dpb.image[i], NULL);
      if (p->dpb_memory[i])
         p->ops->free_memory(p->device, p->dpb_memory[i], NULL);
   }
   free(p);
}

vulkan_encode_pool_t *vulkan_encode_pool_new(
      const vulkan_encode_pool_config_t *c, const vulkan_encode_pool_ops_t *ops)
{
   vulkan_encode_pool_t *p;
   VkVideoProfileListInfoKHR profiles;
   VkPhysicalDeviceMemoryProperties properties;
   VkCommandPoolCreateInfo commands;
   VkCommandBufferAllocateInfo command;
   VkFenceCreateInfo fence;
   VkQueryPoolVideoEncodeFeedbackCreateInfoKHR feedback;
   VkQueryPoolCreateInfo queries;
   VkSemaphoreTypeCreateInfo type;
   VkSemaphoreCreateInfo semaphore;
   VkDeviceSize bytes, alignment;
   unsigned i;
   if (!c || !ops || !c->device || !c->physical || !c->profile || !c->capabilities ||
       !c->slots || c->slots > VULKAN_ENCODE_POOL_SLOTS ||
       !c->extent.width || c->extent.width > 8192 || (c->extent.width & 1) ||
       !c->extent.height || c->extent.height > 8192 || (c->extent.height & 1) ||
       c->format != VK_FORMAT_G8_B8R8_2PLANE_420_UNORM ||
       c->encode_family == UINT32_MAX || c->producer_family == UINT32_MAX ||
       !c->packet_size || c->packet_size > 64u * 1024u * 1024u ||
       !ops->memory_properties || !ops->video_formats || !ops->image_properties ||
       !ops->format_properties || !ops->create_image || !ops->destroy_image ||
       !ops->image_memory || !ops->bind_image || !ops->create_view || !ops->destroy_view ||
       !ops->create_buffer || !ops->destroy_buffer || !ops->buffer_memory ||
       !ops->bind_buffer || !ops->map || !ops->unmap || !ops->allocate || !ops->free_memory ||
       !ops->create_commands || !ops->destroy_commands || !ops->allocate_command ||
       !ops->create_fence || !ops->destroy_fence || !ops->create_queries ||
       !ops->destroy_queries || !ops->create_semaphore || !ops->destroy_semaphore)
      return NULL;
   alignment = c->capabilities->minBitstreamBufferSizeAlignment;
   if (!alignment || alignment > 64u * 1024u * 1024u ||
       !c->capabilities->pictureAccessGranularity.width ||
       !c->capabilities->pictureAccessGranularity.height ||
       c->extent.width % c->capabilities->pictureAccessGranularity.width ||
       c->extent.height % c->capabilities->pictureAccessGranularity.height ||
       c->capabilities->maxDpbSlots < 2 || c->capabilities->maxActiveReferencePictures < 1 ||
       c->extent.width < c->capabilities->minCodedExtent.width ||
       c->extent.height < c->capabilities->minCodedExtent.height ||
       c->extent.width > c->capabilities->maxCodedExtent.width ||
       c->extent.height > c->capabilities->maxCodedExtent.height)
      return NULL;
   bytes = ((c->packet_size + alignment - 1) / alignment) * alignment;
   if (bytes > 64u * 1024u * 1024u)
      return NULL;
   profiles.sType = VK_STRUCTURE_TYPE_VIDEO_PROFILE_LIST_INFO_KHR;
   profiles.pNext = NULL;
   profiles.profileCount = 1;
   profiles.pProfiles = c->profile;
   p = (vulkan_encode_pool_t*)calloc(1, sizeof(*p));
   if (!p)
      return NULL;
   p->device = c->device;
   p->ops = ops;
   p->extent = c->extent;
   p->format = c->format;
   p->count = c->slots;
   ops->memory_properties(c->physical, &properties);
   if (!properties.memoryTypeCount || properties.memoryTypeCount > VK_MAX_MEMORY_TYPES)
      goto fail;
   p->storage_images = vep_storage(c, ops, &profiles);
   if ((!p->storage_images &&
          (!vep_video_format(c, ops, &profiles, VK_IMAGE_USAGE_VIDEO_ENCODE_SRC_BIT_KHR, 0) ||
           !vep_image_format(c, ops, &profiles, VK_IMAGE_USAGE_VIDEO_ENCODE_SRC_BIT_KHR |
                 VK_IMAGE_USAGE_TRANSFER_DST_BIT, 0))) ||
       !vep_video_format(c, ops, &profiles, VK_IMAGE_USAGE_VIDEO_ENCODE_DPB_BIT_KHR, 0) ||
       !vep_image_format(c, ops, &profiles, VK_IMAGE_USAGE_VIDEO_ENCODE_DPB_BIT_KHR, 0))
      goto fail;
   commands.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
   commands.pNext = NULL;
   commands.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
   commands.queueFamilyIndex = c->encode_family;
   if (ops->create_commands(p->device, &commands, NULL, &p->commands) != VK_SUCCESS)
   {
      p->commands = VK_NULL_HANDLE;
      goto fail;
   }
   feedback.sType = VK_STRUCTURE_TYPE_QUERY_POOL_VIDEO_ENCODE_FEEDBACK_CREATE_INFO_KHR;
   feedback.pNext = c->profile;
   feedback.encodeFeedbackFlags = VK_VIDEO_ENCODE_FEEDBACK_BITSTREAM_BUFFER_OFFSET_BIT_KHR |
         VK_VIDEO_ENCODE_FEEDBACK_BITSTREAM_BYTES_WRITTEN_BIT_KHR;
   queries.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
   queries.pNext = &feedback;
   queries.flags = 0;
   queries.queryType = VK_QUERY_TYPE_VIDEO_ENCODE_FEEDBACK_KHR;
   queries.queryCount = c->slots;
   queries.pipelineStatistics = 0;
   if (ops->create_queries(p->device, &queries, NULL, &p->queries) != VK_SUCCESS)
   {
      p->queries = VK_NULL_HANDLE;
      goto fail;
   }
   type.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
   type.pNext = NULL;
   type.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
   type.initialValue = 0;
   semaphore.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
   semaphore.pNext = &type;
   semaphore.flags = 0;
   if (ops->create_semaphore(p->device, &semaphore, NULL, &p->ready) != VK_SUCCESS)
   {
      p->ready = VK_NULL_HANDLE;
      goto fail;
   }
   if (ops->create_semaphore(p->device, &semaphore, NULL, &p->done) != VK_SUCCESS)
   {
      p->done = VK_NULL_HANDLE;
      goto fail;
   }
   for (i = 0; i < 2; i++)
      if (!vep_image(p, c, &profiles, &properties, false,
               &p->dpb.image[i], &p->dpb_memory[i], &p->dpb.view[i], NULL))
         goto fail;
   command.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
   command.pNext = NULL;
   command.commandPool = p->commands;
   command.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
   command.commandBufferCount = 1;
   fence.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
   fence.pNext = NULL;
   fence.flags = 0;
   for (i = 0; i < p->count; i++)
   {
      vulkan_encode_pool_slot_t *slot = &p->slot[i];
      if (ops->allocate_command(p->device, &command, &slot->command) != VK_SUCCESS)
         goto fail;
      if (ops->create_fence(p->device, &fence, NULL, &slot->fence) != VK_SUCCESS)
      {
         slot->fence = VK_NULL_HANDLE;
         goto fail;
      }
      if (!vep_image(p, c, &profiles, &properties, true,
               &slot->image, &slot->memory, &slot->view, slot->planes) ||
          !vep_buffer(p, &properties, &profiles, bytes, true, &slot->output) ||
          (!p->storage_images && !vep_buffer(p, &properties, &profiles,
                (VkDeviceSize)c->extent.width * c->extent.height * 3 / 2,
                false, &slot->conversion)))
         goto fail;
   }
   return p;
fail:
   vulkan_encode_pool_free(p);
   return NULL;
}
