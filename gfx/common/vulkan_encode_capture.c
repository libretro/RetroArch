/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "vulkan_encode_capture.h"
#include <stdlib.h>

struct vulkan_encode_capture
{
   const vulkan_encode_pool_t *pool;
   const vulkan_encode_capture_ops_t *ops;
   VkDescriptorSetLayout set_layout;
   VkDescriptorPool descriptors;
   VkDescriptorSet sets[VULKAN_ENCODE_POOL_SLOTS];
   VkPipelineLayout layout;
   VkPipeline pipeline;
   VkSampler sampler;
};

void vulkan_encode_capture_free(vulkan_encode_capture_t *c)
{
   VkDevice device;
   if (!c)
      return;
   device = c->pool->device;
   if (c->pipeline)
      c->ops->destroy_pipeline(device, c->pipeline, NULL);
   if (c->layout)
      c->ops->destroy_pipeline_layout(device, c->layout, NULL);
   if (c->descriptors)
      c->ops->destroy_descriptor_pool(device, c->descriptors, NULL);
   if (c->sampler)
      c->ops->destroy_sampler(device, c->sampler, NULL);
   if (c->set_layout)
      c->ops->destroy_set_layout(device, c->set_layout, NULL);
   free(c);
}

static bool vec_bindings(vulkan_encode_capture_t *c)
{
   VkDescriptorSetLayoutBinding bindings[3];
   VkDescriptorPoolSize sizes[2];
   VkDescriptorSetLayout layouts[VULKAN_ENCODE_POOL_SLOTS];
   VkDescriptorSetLayoutCreateInfo set;
   VkDescriptorPoolCreateInfo pool;
   VkDescriptorSetAllocateInfo allocate;
   unsigned i, count = c->pool->storage_images ? 3 : 2;
   for (i = 0; i < count; i++)
   {
      bindings[i].binding = i;
      bindings[i].descriptorType = i == 0 ? VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
         : c->pool->storage_images ? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE
         : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
      bindings[i].descriptorCount = 1;
      bindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
      bindings[i].pImmutableSamplers = NULL;
   }
   set.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
   set.pNext = NULL;
   set.flags = 0;
   set.bindingCount = count;
   set.pBindings = bindings;
   if (c->ops->create_set_layout(c->pool->device, &set, NULL, &c->set_layout) != VK_SUCCESS)
   {
      c->set_layout = VK_NULL_HANDLE;
      return false;
   }
   sizes[0].type = bindings[0].descriptorType;
   sizes[0].descriptorCount = c->pool->count;
   sizes[1].type = bindings[1].descriptorType;
   sizes[1].descriptorCount = c->pool->count * (count - 1);
   pool.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
   pool.pNext = NULL;
   pool.flags = 0;
   pool.maxSets = c->pool->count;
   pool.poolSizeCount = 2;
   pool.pPoolSizes = sizes;
   if (c->ops->create_descriptor_pool(c->pool->device, &pool, NULL, &c->descriptors) != VK_SUCCESS)
   {
      c->descriptors = VK_NULL_HANDLE;
      return false;
   }
   for (i = 0; i < c->pool->count; i++)
      layouts[i] = c->set_layout;
   allocate.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
   allocate.pNext = NULL;
   allocate.descriptorPool = c->descriptors;
   allocate.descriptorSetCount = c->pool->count;
   allocate.pSetLayouts = layouts;
   return c->ops->allocate_sets(c->pool->device, &allocate, c->sets) == VK_SUCCESS;
}

static bool vec_pipeline(vulkan_encode_capture_t *c, const uint32_t *code,
      size_t bytes)
{
   VkSamplerCreateInfo sampler;
   VkPushConstantRange range;
   VkPipelineLayoutCreateInfo layout;
   VkShaderModuleCreateInfo shader;
   VkComputePipelineCreateInfo pipeline;
   VkShaderModule module;
   VkResult result;
   sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
   sampler.pNext = NULL;
   sampler.flags = 0;
   sampler.magFilter = VK_FILTER_NEAREST;
   sampler.minFilter = VK_FILTER_NEAREST;
   sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
   sampler.addressModeU = sampler.addressModeV = sampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
   sampler.mipLodBias = 0.0f;
   sampler.anisotropyEnable = VK_FALSE;
   sampler.maxAnisotropy = 1.0f;
   sampler.compareEnable = VK_FALSE;
   sampler.compareOp = VK_COMPARE_OP_ALWAYS;
   sampler.minLod = sampler.maxLod = 0.0f;
   sampler.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
   sampler.unnormalizedCoordinates = VK_FALSE;
   if (c->ops->create_sampler(c->pool->device, &sampler, NULL, &c->sampler) != VK_SUCCESS)
   {
      c->sampler = VK_NULL_HANDLE;
      return false;
   }
   range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
   range.offset = 0;
   range.size = 32;
   layout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
   layout.pNext = NULL;
   layout.flags = 0;
   layout.setLayoutCount = 1;
   layout.pSetLayouts = &c->set_layout;
   layout.pushConstantRangeCount = 1;
   layout.pPushConstantRanges = &range;
   if (c->ops->create_pipeline_layout(c->pool->device, &layout, NULL, &c->layout) != VK_SUCCESS)
   {
      c->layout = VK_NULL_HANDLE;
      return false;
   }
   shader.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
   shader.pNext = NULL;
   shader.flags = 0;
   shader.codeSize = bytes;
   shader.pCode = code;
   if (c->ops->create_shader(c->pool->device, &shader, NULL, &module) != VK_SUCCESS)
      return false;
   pipeline.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
   pipeline.pNext = NULL;
   pipeline.flags = 0;
   pipeline.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
   pipeline.stage.pNext = NULL;
   pipeline.stage.flags = 0;
   pipeline.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
   pipeline.stage.module = module;
   pipeline.stage.pName = "main";
   pipeline.stage.pSpecializationInfo = NULL;
   pipeline.layout = c->layout;
   pipeline.basePipelineHandle = VK_NULL_HANDLE;
   pipeline.basePipelineIndex = -1;
   result = c->ops->create_pipelines(c->pool->device, VK_NULL_HANDLE, 1,
         &pipeline, NULL, &c->pipeline);
   c->ops->destroy_shader(c->pool->device, module, NULL);
   return result == VK_SUCCESS;
}

vulkan_encode_capture_t *vulkan_encode_capture_new(
      const vulkan_encode_capture_config_t *config,
      const vulkan_encode_capture_ops_t *ops)
{
   vulkan_encode_capture_t *c;
   const vulkan_encode_pool_t *pool;
   unsigned i;
   VkDeviceSize bytes;
   if (!config || !ops || !(pool = config->pool) || !pool->device ||
       !pool->count || pool->count > VULKAN_ENCODE_POOL_SLOTS ||
       !pool->extent.width || !pool->extent.height ||
       pool->extent.width > INT32_MAX || pool->extent.height > INT32_MAX ||
       (pool->extent.width & 1) || (pool->extent.height & 1) ||
       pool->format != VK_FORMAT_G8_B8R8_2PLANE_420_UNORM ||
       (!pool->storage_images && (pool->extent.width & 3)) ||
       !config->code || config->code_bytes < 20 || (config->code_bytes & 3) ||
       config->code[0] != 0x07230203 ||
       !ops->create_set_layout || !ops->destroy_set_layout ||
       !ops->create_descriptor_pool || !ops->destroy_descriptor_pool ||
       !ops->allocate_sets || !ops->update_sets || !ops->create_sampler ||
       !ops->destroy_sampler || !ops->create_pipeline_layout ||
       !ops->destroy_pipeline_layout || !ops->create_shader || !ops->destroy_shader ||
       !ops->create_pipelines || !ops->destroy_pipeline || !ops->barrier ||
       !ops->bind_pipeline || !ops->bind_sets || !ops->push || !ops->dispatch ||
       (!pool->storage_images && !ops->copy))
      return NULL;
   bytes = (VkDeviceSize)pool->extent.width * pool->extent.height;
   if (bytes > UINT32_MAX / 3 * 2)
      return NULL;
   bytes = bytes * 3 / 2;
   for (i = 0; i < pool->count; i++)
      if (!pool->slot[i].image || (pool->storage_images
            ? !pool->slot[i].planes[0] || !pool->slot[i].planes[1]
            : !pool->slot[i].conversion.buffer || pool->slot[i].conversion.size < bytes))
         return NULL;
   c = (vulkan_encode_capture_t*)calloc(1, sizeof(*c));
   if (!c)
      return NULL;
   c->pool = pool;
   c->ops = ops;
   if (vec_bindings(c) && vec_pipeline(c, config->code, config->code_bytes))
      return c;
   vulkan_encode_capture_free(c);
   return NULL;
}

static void vec_barrier(VkImageMemoryBarrier2 *b, VkImage image,
      VkImageLayout before, VkImageLayout after,
      VkPipelineStageFlags2 src, VkAccessFlags2 src_access,
      VkPipelineStageFlags2 dst, VkAccessFlags2 dst_access)
{
   b->sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
   b->pNext = NULL;
   b->srcStageMask = src;
   b->srcAccessMask = src_access;
   b->dstStageMask = dst;
   b->dstAccessMask = dst_access;
   b->oldLayout = before;
   b->newLayout = after;
   b->srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
   b->dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
   b->image = image;
   b->subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
   b->subresourceRange.baseMipLevel = 0;
   b->subresourceRange.levelCount = 1;
   b->subresourceRange.baseArrayLayer = 0;
   b->subresourceRange.layerCount = 1;
}

static void vec_descriptors(vulkan_encode_capture_t *c, unsigned index,
      const vulkan_encode_capture_source_t *source, VkImageLayout layout)
{
   const vulkan_encode_pool_slot_t *slot = &c->pool->slot[index];
   VkWriteDescriptorSet writes[3];
   VkDescriptorImageInfo images[3];
   VkDescriptorBufferInfo buffer;
   unsigned i, count = c->pool->storage_images ? 3 : 2;
   images[0].sampler = c->sampler;
   images[0].imageView = source->view;
   images[0].imageLayout = layout;
   buffer.buffer = slot->conversion.buffer;
   buffer.offset = 0;
   buffer.range = slot->conversion.size;
   for (i = 0; i < count; i++)
   {
      writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      writes[i].pNext = NULL;
      writes[i].dstSet = c->sets[index];
      writes[i].dstBinding = i;
      writes[i].dstArrayElement = 0;
      writes[i].descriptorCount = 1;
      writes[i].descriptorType = i == 0 ? VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
         : c->pool->storage_images ? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE
         : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
      writes[i].pImageInfo = NULL;
      writes[i].pBufferInfo = NULL;
      writes[i].pTexelBufferView = NULL;
      if (i == 0 || c->pool->storage_images)
      {
         if (i)
         {
            images[i].sampler = VK_NULL_HANDLE;
            images[i].imageView = slot->planes[i - 1];
            images[i].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
         }
         writes[i].pImageInfo = &images[i];
      }
      else
         writes[i].pBufferInfo = &buffer;
   }
   c->ops->update_sets(c->pool->device, count, writes, 0, NULL);
}

static void vec_copy(vulkan_encode_capture_t *c, unsigned index,
      VkCommandBuffer command, VkDependencyInfo *dependency)
{
   const vulkan_encode_pool_slot_t *slot = &c->pool->slot[index];
   VkBufferMemoryBarrier2 barrier;
   VkBufferImageCopy regions[2];
   unsigned i;
   barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
   barrier.pNext = NULL;
   barrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
   barrier.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
   barrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
   barrier.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
   barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
   barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
   barrier.buffer = slot->conversion.buffer;
   barrier.offset = 0;
   barrier.size = slot->conversion.size;
   dependency->bufferMemoryBarrierCount = 1;
   dependency->pBufferMemoryBarriers = &barrier;
   dependency->imageMemoryBarrierCount = 0;
   c->ops->barrier(command, dependency);
   dependency->bufferMemoryBarrierCount = 0;
   dependency->pBufferMemoryBarriers = NULL;
   for (i = 0; i < 2; i++)
   {
      regions[i].bufferOffset = i ? (VkDeviceSize)c->pool->extent.width * c->pool->extent.height : 0;
      regions[i].bufferRowLength = 0;
      regions[i].bufferImageHeight = 0;
      regions[i].imageSubresource.aspectMask = i ? VK_IMAGE_ASPECT_PLANE_1_BIT : VK_IMAGE_ASPECT_PLANE_0_BIT;
      regions[i].imageSubresource.mipLevel = 0;
      regions[i].imageSubresource.baseArrayLayer = 0;
      regions[i].imageSubresource.layerCount = 1;
      regions[i].imageOffset.x = regions[i].imageOffset.y = regions[i].imageOffset.z = 0;
      regions[i].imageExtent.width = c->pool->extent.width >> i;
      regions[i].imageExtent.height = c->pool->extent.height >> i;
      regions[i].imageExtent.depth = 1;
   }
   c->ops->copy(command, slot->conversion.buffer, slot->image,
         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 2, regions);
}

bool vulkan_encode_capture_record(vulkan_encode_capture_t *c,
      unsigned index, VkCommandBuffer command,
      const vulkan_encode_capture_source_t *source, bool initialized)
{
   VkImageMemoryBarrier2 barriers[2];
   VkDependencyInfo dependency;
   VkImageLayout sampled, destination;
   const vulkan_encode_pool_slot_t *slot;
   uint32_t push[8];
   bool direct;
   if (!c || index >= c->pool->count || !command || !source ||
       !source->image || !source->view || !source->stage ||
       (source->format != VK_FORMAT_R8G8B8A8_UNORM &&
        source->format != VK_FORMAT_B8G8R8A8_UNORM) ||
       (source->layout != VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL &&
        source->layout != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL &&
        source->layout != VK_IMAGE_LAYOUT_GENERAL) ||
       !source->width || !source->height || source->width > INT32_MAX ||
       source->height > INT32_MAX || !source->valid_width || !source->valid_height ||
       source->valid_width > c->pool->extent.width ||
       source->valid_height > c->pool->extent.height ||
       source->crop_x > source->width || source->crop_y > source->height ||
       source->valid_width > source->width - source->crop_x ||
       source->valid_height > source->height - source->crop_y)
      return false;
   slot = &c->pool->slot[index];
   if (source->image == slot->image)
      return false;
   direct = c->pool->storage_images;
   sampled = source->layout == VK_IMAGE_LAYOUT_GENERAL ? VK_IMAGE_LAYOUT_GENERAL
      : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
   destination = direct ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
   vec_descriptors(c, index, source, sampled);
   vec_barrier(&barriers[0], source->image, source->layout, sampled,
         source->stage, source->access,
         VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT);
   vec_barrier(&barriers[1], slot->image,
         initialized ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_UNDEFINED,
         destination, VK_PIPELINE_STAGE_2_NONE, 0,
         direct ? VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT : VK_PIPELINE_STAGE_2_TRANSFER_BIT,
         direct ? VK_ACCESS_2_SHADER_WRITE_BIT : VK_ACCESS_2_TRANSFER_WRITE_BIT);
   dependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
   dependency.pNext = NULL;
   dependency.dependencyFlags = 0;
   dependency.memoryBarrierCount = 0;
   dependency.pMemoryBarriers = NULL;
   dependency.bufferMemoryBarrierCount = 0;
   dependency.pBufferMemoryBarriers = NULL;
   dependency.imageMemoryBarrierCount = 2;
   dependency.pImageMemoryBarriers = barriers;
   c->ops->barrier(command, &dependency);
   push[0] = c->pool->extent.width;
   push[1] = c->pool->extent.height;
   push[2] = source->crop_x;
   push[3] = source->crop_y;
   push[4] = source->valid_width;
   push[5] = source->valid_height;
   push[6] = source->limited_range ? 1 : 0;
   push[7] = source->flip_y ? 1 : 0;
   c->ops->bind_pipeline(command, VK_PIPELINE_BIND_POINT_COMPUTE, c->pipeline);
   c->ops->bind_sets(command, VK_PIPELINE_BIND_POINT_COMPUTE,
         c->layout, 0, 1, &c->sets[index], 0, NULL);
   c->ops->push(command, c->layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), push);
   c->ops->dispatch(command, (push[0] + (direct ? 15 : 31)) / (direct ? 16 : 32),
         (push[1] + 15) / 16, 1);
   if (!direct)
      vec_copy(c, index, command, &dependency);
   vec_barrier(&barriers[0], source->image, sampled, source->layout,
         VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT,
         source->stage, source->access);
   vec_barrier(&barriers[1], slot->image, destination, VK_IMAGE_LAYOUT_GENERAL,
         direct ? VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT : VK_PIPELINE_STAGE_2_TRANSFER_BIT,
         direct ? VK_ACCESS_2_SHADER_WRITE_BIT : VK_ACCESS_2_TRANSFER_WRITE_BIT,
         VK_PIPELINE_STAGE_2_NONE, 0);
   dependency.imageMemoryBarrierCount = 2;
   c->ops->barrier(command, &dependency);
   return true;
}
