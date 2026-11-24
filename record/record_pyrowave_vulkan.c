/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#define VK_NO_PROTOTYPES
#include "record_pyrowave_vulkan.h"
#include "record_pyrowave_session_internal.h"
#include <stdlib.h>
#include <string.h>
#include <retro_atomic.h>

#define RPY_VK_FUNCTIONS(X) \
   X(vkCreateBuffer, create_buffer) X(vkDestroyBuffer, destroy_buffer) \
   X(vkGetBufferMemoryRequirements, buffer_requirements) X(vkAllocateMemory, allocate) \
   X(vkFreeMemory, free_memory) X(vkBindBufferMemory, bind_buffer) \
   X(vkMapMemory, map) X(vkUnmapMemory, unmap) \
   X(vkFlushMappedMemoryRanges, flush) X(vkInvalidateMappedMemoryRanges, invalidate) \
   X(vkCreateImage, create_image) X(vkDestroyImage, destroy_image) \
   X(vkGetImageMemoryRequirements, image_requirements) X(vkBindImageMemory, bind_image) \
   X(vkCreateImageView, create_view) X(vkDestroyImageView, destroy_view) \
   X(vkCreateDescriptorSetLayout, create_set_layout) X(vkDestroyDescriptorSetLayout, destroy_set_layout) \
   X(vkCreateDescriptorPool, create_pool) X(vkDestroyDescriptorPool, destroy_pool) \
   X(vkAllocateDescriptorSets, allocate_sets) X(vkUpdateDescriptorSets, update_sets) \
   X(vkCreatePipelineLayout, create_pipeline_layout) X(vkDestroyPipelineLayout, destroy_pipeline_layout) \
   X(vkCreateShaderModule, create_shader) X(vkDestroyShaderModule, destroy_shader) \
   X(vkCreateComputePipelines, create_pipeline) X(vkDestroyPipeline, destroy_pipeline) \
   X(vkCmdBindPipeline, cmd_pipeline) X(vkCmdBindDescriptorSets, cmd_sets) \
   X(vkCmdDispatch, cmd_dispatch) X(vkCmdPipelineBarrier, cmd_barrier) \
   X(vkCmdCopyBuffer, cmd_copy) X(vkGetFenceStatus, fence_status)

typedef struct record_pyrowave_vk_api
{
#define RPY_VK_DECLARE(name, member) PFN_##name member;
   RPY_VK_FUNCTIONS(RPY_VK_DECLARE)
#undef RPY_VK_DECLARE
} record_pyrowave_vk_api_t;

typedef struct record_pyrowave_vk_buffer
{
   VkBuffer handle;
   VkDeviceMemory memory;
   VkDeviceSize bytes;
   void *mapped;
   bool coherent, cached;
} record_pyrowave_vk_buffer_t;

typedef struct record_pyrowave_vk_texture
{
   VkImage image;
   VkImageView view;
   VkDeviceMemory memory;
} record_pyrowave_vk_texture_t;

typedef struct record_pyrowave_vk_slot
{
   record_gpu_frame_t frame;
   record_pyrowave_vk_buffer_t buffer;
   record_pyrowave_vk_texture_t texture;
   rpyrowave_packet_parameters_t parameters;
   uint8_t *host;
   VkFence fence;
   VkDescriptorSet color, packet;
   retro_atomic_int_t ready;
   unsigned state;
} record_pyrowave_vk_slot_t;

struct record_pyrowave_vulkan
{
   record_pyrowave_vk_api_t api;
   record_pyrowave_vk_slot_t slots[RECORD_GPU_QUEUE_SLOTS];
   record_pyrowave_vk_buffer_t buffers[5];
   record_pyrowave_vk_texture_t dummy;
   record_gpu_queue_t *queue;
   VkDevice device;
   VkDescriptorPool pool;
   VkDescriptorSetLayout sets[3];
   VkPipelineLayout layouts[3];
   VkPipeline pipelines[3];
   VkDescriptorSet dwt[2];
   VkCommandBuffer commands;
   rpyrowave_sequence_t sequence;
   rpyrowave_gpu_layout_t layout;
   VkDeviceSize stride, dwt_offset, packet_offset;
   uint64_t next_ticket, retired;
   unsigned retired_marks[RECORD_GPU_QUEUE_SLOTS];
   uint32_t family;
   unsigned count, mode, usability, reservation, owned, cursor, command_slot;
   bool decode, failed;
};

enum record_pyrowave_vk_state
{
   RPY_VK_IDLE, RPY_VK_DECODING, RPY_VK_READY, RPY_VK_LENT, RPY_VK_DISPLAYING
};

static uint32_t record_pyrowave_vk_load32(const uint8_t *p)
{
   return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static int record_pyrowave_vk_memory(const VkPhysicalDeviceMemoryProperties *p,
      uint32_t bits, VkMemoryPropertyFlags required)
{
   unsigned i;
   unsigned best_score = 0;
   int selected = -1;
   for (i = 0; i < p->memoryTypeCount && i < VK_MAX_MEMORY_TYPES; i++)
   {
      VkMemoryPropertyFlags flags = p->memoryTypes[i].propertyFlags;
      unsigned score;
      if (!(bits & ((uint32_t)1 << i)) || (flags & required) != required) continue;
      score = ((flags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) ? 4U : 0U)
         + ((flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) ? 2U : 0U)
         + ((flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) ? 1U : 0U);
      if (selected < 0 || score > best_score)
      {
         selected = (int)i;
         best_score = score;
      }
   }
   return selected;
}

static bool record_pyrowave_vk_buffer_new(record_pyrowave_vulkan_t *c,
      const VkPhysicalDeviceMemoryProperties *p, record_pyrowave_vk_buffer_t *b,
      VkDeviceSize bytes, VkBufferUsageFlags usage, bool host)
{
   VkBufferCreateInfo info;
   VkMemoryRequirements requirements;
   VkMemoryAllocateInfo allocate;
   int type;
   memset(&info, 0, sizeof(info));
   info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
   info.size = b->bytes = bytes;
   info.usage = usage;
   info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
   if (c->api.create_buffer(c->device, &info, NULL, &b->handle) != VK_SUCCESS) return false;
   c->api.buffer_requirements(c->device, b->handle, &requirements);
   type = record_pyrowave_vk_memory(p, requirements.memoryTypeBits,
         host ? VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT : VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
   if (type < 0) return false;
   memset(&allocate, 0, sizeof(allocate));
   allocate.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
   allocate.allocationSize = requirements.size;
   allocate.memoryTypeIndex = (uint32_t)type;
   if (c->api.allocate(c->device, &allocate, NULL, &b->memory) != VK_SUCCESS
         || c->api.bind_buffer(c->device, b->handle, b->memory, 0) != VK_SUCCESS) return false;
   b->coherent = (p->memoryTypes[type].propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0;
   b->cached = (p->memoryTypes[type].propertyFlags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) != 0;
   return !host || c->api.map(c->device, b->memory, 0, (VkDeviceSize)-1, 0, &b->mapped) == VK_SUCCESS;
}

static bool record_pyrowave_vk_cache(record_pyrowave_vulkan_t *c,
      record_pyrowave_vk_buffer_t *b, bool invalidate)
{
   VkMappedMemoryRange range;
   if (b->coherent) return true;
   memset(&range, 0, sizeof(range));
   range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
   range.memory = b->memory;
   range.size = (VkDeviceSize)-1;
   return (invalidate ? c->api.invalidate(c->device, 1, &range)
         : c->api.flush(c->device, 1, &range)) == VK_SUCCESS;
}

static bool record_pyrowave_vk_texture_new(record_pyrowave_vulkan_t *c,
      const VkPhysicalDeviceMemoryProperties *p, record_pyrowave_vk_texture_t *t,
      unsigned width, unsigned height)
{
   VkImageCreateInfo image;
   VkMemoryAllocateInfo allocate;
   VkMemoryRequirements requirements;
   VkImageViewCreateInfo view;
   int type;
   memset(&image, 0, sizeof(image));
   image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
   image.imageType = VK_IMAGE_TYPE_2D;
   image.format = VK_FORMAT_R32G32B32A32_SFLOAT;
   image.extent.width = width; image.extent.height = height; image.extent.depth = 1;
   image.mipLevels = image.arrayLayers = 1;
   image.samples = VK_SAMPLE_COUNT_1_BIT;
   image.tiling = VK_IMAGE_TILING_OPTIMAL;
   image.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
   image.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
   if (c->api.create_image(c->device, &image, NULL, &t->image) != VK_SUCCESS) return false;
   c->api.image_requirements(c->device, t->image, &requirements);
   type = record_pyrowave_vk_memory(p, requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
   if (type < 0) return false;
   memset(&allocate, 0, sizeof(allocate));
   allocate.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
   allocate.allocationSize = requirements.size; allocate.memoryTypeIndex = (uint32_t)type;
   if (c->api.allocate(c->device, &allocate, NULL, &t->memory) != VK_SUCCESS
         || c->api.bind_image(c->device, t->image, t->memory, 0) != VK_SUCCESS) return false;
   memset(&view, 0, sizeof(view));
   view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
   view.image = t->image;
   view.viewType = VK_IMAGE_VIEW_TYPE_2D;
   view.format = image.format;
   view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
   view.subresourceRange.levelCount = view.subresourceRange.layerCount = 1;
   return c->api.create_view(c->device, &view, NULL, &t->view) == VK_SUCCESS;
}

static void record_pyrowave_vk_buffer_free(record_pyrowave_vulkan_t *c, record_pyrowave_vk_buffer_t *b)
{
   if (b->mapped) c->api.unmap(c->device, b->memory);
   if (b->handle) c->api.destroy_buffer(c->device, b->handle, NULL);
   if (b->memory) c->api.free_memory(c->device, b->memory, NULL);
}

static void record_pyrowave_vk_texture_free(record_pyrowave_vulkan_t *c, record_pyrowave_vk_texture_t *t)
{
   if (t->view) c->api.destroy_view(c->device, t->view, NULL);
   if (t->image) c->api.destroy_image(c->device, t->image, NULL);
   if (t->memory) c->api.free_memory(c->device, t->memory, NULL);
}

static void record_pyrowave_vk_destroy(record_pyrowave_vulkan_t *c)
{
   unsigned i;
   if (c->pool) c->api.destroy_pool(c->device, c->pool, NULL);
   for (i = 0; i < 3; i++)
   {
      if (c->pipelines[i]) c->api.destroy_pipeline(c->device, c->pipelines[i], NULL);
      if (c->layouts[i]) c->api.destroy_pipeline_layout(c->device, c->layouts[i], NULL);
      if (c->sets[i]) c->api.destroy_set_layout(c->device, c->sets[i], NULL);
   }
   for (i = 0; i < 5; i++) record_pyrowave_vk_buffer_free(c, &c->buffers[i]);
   record_pyrowave_vk_texture_free(c, &c->dummy);
   for (i = 0; i < c->count; i++)
   {
      record_pyrowave_vk_slot_t *s = &c->slots[i];
      if (s->host != s->buffer.mapped) free(s->host);
      record_pyrowave_vk_buffer_free(c, &s->buffer);
      record_pyrowave_vk_texture_free(c, &s->texture);
   }
   free(c);
}

static bool record_pyrowave_vk_pipeline(record_pyrowave_vulkan_t *c, unsigned kind,
      const uint32_t *words, size_t bytes)
{
   VkDescriptorSetLayoutBinding bindings[5];
   VkDescriptorSetLayoutCreateInfo set;
   VkPipelineLayoutCreateInfo layout;
   VkShaderModuleCreateInfo shader;
   VkComputePipelineCreateInfo pipeline;
   VkShaderModule module = VK_NULL_HANDLE;
   unsigned count = kind == 1 ? 3 : 5;
   unsigned i;
   VkResult result;
   memset(bindings, 0, sizeof(bindings));
   for (i = 0; i < count; i++)
   {
      bindings[i].binding = i;
      bindings[i].descriptorCount = 1;
      bindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
      bindings[i].descriptorType = i == count - 1 ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC
         : (!kind && i == 0 ? VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE
         : (!kind && i == 3 ? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER));
   }
   memset(&set, 0, sizeof(set));
   set.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
   set.bindingCount = count; set.pBindings = bindings;
   if (c->api.create_set_layout(c->device, &set, NULL, &c->sets[kind]) != VK_SUCCESS) return false;
   memset(&layout, 0, sizeof(layout));
   layout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
   layout.setLayoutCount = 1; layout.pSetLayouts = &c->sets[kind];
   if (c->api.create_pipeline_layout(c->device, &layout, NULL, &c->layouts[kind]) != VK_SUCCESS) return false;
   memset(&shader, 0, sizeof(shader));
   shader.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
   shader.codeSize = bytes; shader.pCode = words;
   if (c->api.create_shader(c->device, &shader, NULL, &module) != VK_SUCCESS) return false;
   memset(&pipeline, 0, sizeof(pipeline));
   pipeline.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
   pipeline.layout = c->layouts[kind];
   pipeline.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
   pipeline.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
   pipeline.stage.module = module;
   pipeline.stage.pName = "main";
   result = c->api.create_pipeline(c->device, VK_NULL_HANDLE, 1, &pipeline, NULL, &c->pipelines[kind]);
   c->api.destroy_shader(c->device, module, NULL);
   return result == VK_SUCCESS;
}

static bool record_pyrowave_vk_set(record_pyrowave_vulkan_t *c,
      unsigned kind, unsigned slot, VkDescriptorSet *result)
{
   VkDescriptorSetAllocateInfo allocate;
   VkWriteDescriptorSet writes[5];
   VkDescriptorBufferInfo buffers[5];
   VkDescriptorImageInfo images[5];
   unsigned count = kind == 1 ? 3 : 5;
   unsigned i;
   memset(&allocate, 0, sizeof(allocate));
   allocate.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
   allocate.descriptorPool = c->pool;
   allocate.descriptorSetCount = 1; allocate.pSetLayouts = &c->sets[kind];
   if (c->api.allocate_sets(c->device, &allocate, result) != VK_SUCCESS) return false;
   memset(writes, 0, sizeof(writes)); memset(buffers, 0, sizeof(buffers)); memset(images, 0, sizeof(images));
   for (i = 0; i < count; i++)
   {
      record_pyrowave_vk_buffer_t *b = NULL;
      writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      writes[i].dstSet = *result; writes[i].dstBinding = i; writes[i].descriptorCount = 1;
      if (i == count - 1)
      {
         writes[i].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
         b = &c->buffers[4];
         buffers[i].range = sizeof(rpyrowave_color_parameters_t);
      }
      else if (!kind && (i == 0 || i == 3))
      {
         writes[i].descriptorType = i ? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE : VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
         images[i].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
         images[i].imageView = i && c->decode ? c->slots[slot].texture.view : c->dummy.view;
         writes[i].pImageInfo = &images[i];
      }
      else
      {
         writes[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
         if (kind == 1) b = &c->buffers[i ? (slot ^ 1) : slot];
         else if (!kind) b = &c->buffers[i == 1 ? (c->decode ? 0 : 1) : (c->decode ? 1 : 0)];
         else if (i == 0) b = &c->buffers[c->decode ? 1 : 0];
         else if (i == 1) b = &c->buffers[2];
         else if (i == 2) b = c->decode ? &c->slots[slot].buffer : &c->buffers[3];
         else b = &c->buffers[c->decode ? 0 : 1];
         buffers[i].range = b->bytes;
      }
      if (b)
      {
         buffers[i].buffer = b->handle;
         writes[i].pBufferInfo = &buffers[i];
      }
   }
   c->api.update_sets(c->device, count, writes, 0, NULL);
   return true;
}

static int record_pyrowave_vk_dwt_constants(void *user, const rpyrowave_dwt_parameters_t *p,
      unsigned source, unsigned x, unsigned y, unsigned z)
{
   record_pyrowave_vulkan_t *c = (record_pyrowave_vulkan_t*)user;
   (void)source; (void)x; (void)y; (void)z;
   memcpy((uint8_t*)c->buffers[4].mapped + (size_t)(c->dwt_offset + c->cursor++ * c->stride), p, sizeof(*p));
   return 1;
}

static int record_pyrowave_vk_packet_constants(void *user, const rpyrowave_packet_parameters_t *p,
      unsigned x, unsigned y)
{
   record_pyrowave_vulkan_t *c = (record_pyrowave_vulkan_t*)user;
   rpyrowave_packet_parameters_t value = *p;
   unsigned index = p->sequence * 6 + p->operation;
   (void)x; (void)y;
   c->usability = p->usability;
   if (c->decode)
   {
      if (p->operation != RPYROWAVE_PACKET_PACK) return 1;
      index = p->sequence; value.operation = RPYROWAVE_PACKET_UNPACK;
   }
   memcpy((uint8_t*)c->buffers[4].mapped + (size_t)(c->packet_offset + index * c->stride), &value, sizeof(value));
   return 1;
}

record_pyrowave_vulkan_t *record_pyrowave_vulkan_new(const record_pyrowave_vulkan_config_t *cfg)
{
   record_pyrowave_vulkan_t *c;
   rpyrowave_color_parameters_t color;
   VkDescriptorPoolSize sizes[4];
   VkDescriptorPoolCreateInfo pool;
   VkDeviceSize atlas_bytes, constants, staging;
   unsigned i, packets;
   if (!cfg || !cfg->get_proc || !cfg->device || !cfg->queue || !cfg->memory || !cfg->limits
         || !cfg->slots || cfg->slots > RECORD_GPU_QUEUE_SLOTS || cfg->texture_mode > (unsigned)(cfg->decode ? RPYROWAVE_COLOR_SDR709 : RPYROWAVE_COLOR_SCRGB)
         || !(cfg->queue_flags & VK_QUEUE_COMPUTE_BIT)
         || (cfg->rgba32f_features & (VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT))
            != (VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT)
         || !cfg->limits->minUniformBufferOffsetAlignment || cfg->limits->minUniformBufferOffsetAlignment > 65536
         || cfg->limits->maxUniformBufferRange < sizeof(color) || cfg->limits->maxComputeWorkGroupInvocations < 128
         || cfg->limits->maxComputeWorkGroupSize[0] < 128 || cfg->limits->maxComputeWorkGroupSize[1] < 8
         || cfg->limits->maxComputeWorkGroupCount[0] < 65535 || cfg->limits->maxComputeWorkGroupCount[1] < 65535
         || cfg->limits->maxPerStageDescriptorStorageBuffers < 4
         || cfg->sequence.width > cfg->limits->maxImageDimension2D || cfg->sequence.height > cfg->limits->maxImageDimension2D
         || rpyrowave_gpu_color_parameters_ex(&cfg->sequence, cfg->bit_depth, cfg->decode ? 1 : 0,
            cfg->decode ? 0 : (cfg->flip_y ? 1 : 0), cfg->decode ? cfg->sequence.width : cfg->texture_width,
            cfg->decode ? cfg->sequence.height : cfg->texture_height,
            cfg->decode ? 0 : cfg->x, cfg->decode ? 0 : cfg->y, cfg->scale_source ? 1 : 0, &color)) return NULL;
   for (i = 0; i < 3; i++)
      if (!cfg->shaders[i] || cfg->shader_bytes[i] < 20 || (cfg->shader_bytes[i] & 3)
            || cfg->shaders[i][0] != 0x07230203U) return NULL;
   c = (record_pyrowave_vulkan_t*)calloc(1, sizeof(*c));
   if (!c) return NULL;
   c->device = cfg->device; c->count = cfg->slots; c->decode = cfg->decode;
   c->sequence = cfg->sequence; c->mode = cfg->texture_mode; c->family = cfg->queue_family;
   c->reservation = RECORD_GPU_QUEUE_SLOTS; c->next_ticket = 1;
#define RPY_VK_LOAD(name, member) \
   { PFN_vkVoidFunction entry = cfg->get_proc(c->device, #name); \
     memcpy(&c->api.member, &entry, sizeof(entry)); if (!entry) goto error; }
   RPY_VK_FUNCTIONS(RPY_VK_LOAD)
#undef RPY_VK_LOAD
   if (rpyrowave_gpu_layout(&cfg->sequence, cfg->maximum_bytes, &c->layout)) goto error;
   atlas_bytes = (VkDeviceSize)c->layout.image_width * c->layout.image_height * 3 * sizeof(float);
   if (atlas_bytes > cfg->limits->maxStorageBufferRange || c->layout.workspace_bytes > cfg->limits->maxStorageBufferRange)
      goto error;
   c->stride = (sizeof(color) + cfg->limits->minUniformBufferOffsetAlignment - 1)
      / cfg->limits->minUniformBufferOffsetAlignment * cfg->limits->minUniformBufferOffsetAlignment;
   c->dwt_offset = c->stride; c->packet_offset = c->dwt_offset + c->stride * 50;
   constants = c->packet_offset + c->stride * (c->decode ? 8 : 48);
   staging = c->decode ? c->layout.workspace_bytes : 16 + ((VkDeviceSize)cfg->maximum_bytes + 3) / 4 * 4;
   c->queue = record_gpu_queue_new(c->count);
   if (!c->queue) goto error;
   for (i = 0; i < 2; i++)
      if (!record_pyrowave_vk_buffer_new(c, cfg->memory, &c->buffers[i], atlas_bytes,
               VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, false)) goto error;
   if (!record_pyrowave_vk_buffer_new(c, cfg->memory, &c->buffers[2],
            c->layout.block_count * sizeof(rpyrowave_gpu_block_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, true)
         || !record_pyrowave_vk_buffer_new(c, cfg->memory, &c->buffers[4], constants,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, true)) goto error;
   if (!c->decode && !record_pyrowave_vk_buffer_new(c, cfg->memory, &c->buffers[3], c->layout.workspace_bytes,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, false)) goto error;
   color.texture_mode = cfg->texture_mode;
   memset(c->buffers[4].mapped, 0, (size_t)constants);
   memcpy(c->buffers[4].mapped, &color, sizeof(color));
   if (rpyrowave_gpu_blocks(&c->layout, (rpyrowave_gpu_block_t*)c->buffers[2].mapped, c->layout.block_count)
         || rpyrowave_dwt_record(c->layout.image_width, c->layout.image_height, c->layout.chroma_444,
            c->decode ? 1 : 0, record_pyrowave_vk_dwt_constants, c)) goto error;
   for (i = 0; i < 8; i++)
   {
      rpyrowave_sequence_t sequence = cfg->sequence;
      sequence.sequence = i;
      if (rpyrowave_gpu_encode_record(&c->layout, &sequence, cfg->maximum_bytes, -1,
               record_pyrowave_vk_packet_constants, c)) goto error;
   }
   if (!record_pyrowave_vk_cache(c, &c->buffers[2], false) || !record_pyrowave_vk_cache(c, &c->buffers[4], false)
         || !record_pyrowave_vk_texture_new(c, cfg->memory, &c->dummy, 1, 1)) goto error;
   for (i = 0; i < c->count; i++)
   {
      record_pyrowave_vk_slot_t *s = &c->slots[i];
      retro_atomic_int_init(&s->ready, 0);
      if (!record_pyrowave_vk_buffer_new(c, cfg->memory, &s->buffer, staging,
               c->decode ? VK_BUFFER_USAGE_STORAGE_BUFFER_BIT : VK_BUFFER_USAGE_TRANSFER_DST_BIT, true)) goto error;
      s->host = s->buffer.cached ? (uint8_t*)s->buffer.mapped : (uint8_t*)malloc((size_t)staging);
      if (!s->host || (c->decode && !record_pyrowave_vk_texture_new(c, cfg->memory, &s->texture,
                  cfg->sequence.width, cfg->sequence.height))) goto error;
   }
   for (i = 0; i < 3; i++)
      if (!record_pyrowave_vk_pipeline(c, i, cfg->shaders[i], cfg->shader_bytes[i])) goto error;
   packets = c->decode ? c->count : 1;
   sizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; sizes[0].descriptorCount = 4 + packets * 4 + c->count * 2;
   sizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC; sizes[1].descriptorCount = 2 + packets + c->count;
   sizes[2].type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE; sizes[2].descriptorCount = c->count;
   sizes[3].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE; sizes[3].descriptorCount = c->count;
   memset(&pool, 0, sizeof(pool));
   pool.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
   pool.maxSets = 2 + packets + c->count; pool.poolSizeCount = 4; pool.pPoolSizes = sizes;
   if (c->api.create_pool(c->device, &pool, NULL, &c->pool) != VK_SUCCESS) goto error;
   for (i = 0; i < 2; i++) if (!record_pyrowave_vk_set(c, 1, i, &c->dwt[i])) goto error;
   for (i = 0; i < c->count; i++)
   {
      if (!record_pyrowave_vk_set(c, 0, i, &c->slots[i].color)) goto error;
      if ((c->decode || !i) && !record_pyrowave_vk_set(c, 2, i, &c->slots[i].packet)) goto error;
   }
   return c;
error:
   record_gpu_queue_free(c->queue);
   record_pyrowave_vk_destroy(c);
   return NULL;
}

static void record_pyrowave_vk_barrier(record_pyrowave_vulkan_t *c,
      VkPipelineStageFlags from, VkPipelineStageFlags to, VkAccessFlags reads, VkAccessFlags writes)
{
   VkMemoryBarrier barrier;
   memset(&barrier, 0, sizeof(barrier));
   barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
   barrier.srcAccessMask = reads; barrier.dstAccessMask = writes;
   c->api.cmd_barrier(c->commands, from, to, 0, 1, &barrier, 0, NULL, 0, NULL);
}

static void record_pyrowave_vk_image_start(record_pyrowave_vulkan_t *c, VkImage image)
{
   VkImageMemoryBarrier barrier;
   memset(&barrier, 0, sizeof(barrier));
   barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
   barrier.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
   barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
   barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED; barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
   barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
   barrier.image = image;
   barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
   barrier.subresourceRange.levelCount = barrier.subresourceRange.layerCount = 1;
   c->api.cmd_barrier(c->commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
         0, 0, NULL, 0, NULL, 1, &barrier);
}

static void record_pyrowave_vk_bind(record_pyrowave_vulkan_t *c,
      unsigned kind, VkDescriptorSet set, VkDeviceSize offset)
{
   uint32_t dynamic = (uint32_t)offset;
   c->api.cmd_sets(c->commands, VK_PIPELINE_BIND_POINT_COMPUTE, c->layouts[kind], 0, 1, &set, 1, &dynamic);
}

static int record_pyrowave_vk_dwt_dispatch(void *user, const rpyrowave_dwt_parameters_t *p,
      unsigned source, unsigned x, unsigned y, unsigned z)
{
   record_pyrowave_vulkan_t *c = (record_pyrowave_vulkan_t*)user;
   (void)p;
   record_pyrowave_vk_bind(c, 1, c->dwt[source], c->dwt_offset + c->cursor++ * c->stride);
   c->api.cmd_dispatch(c->commands, x, y, z);
   record_pyrowave_vk_barrier(c, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
         VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
   return 1;
}

static int record_pyrowave_vk_packet_dispatch(void *user, const rpyrowave_packet_parameters_t *p,
      unsigned x, unsigned y)
{
   record_pyrowave_vulkan_t *c = (record_pyrowave_vulkan_t*)user;
   unsigned index = c->decode ? p->sequence : p->sequence * 6 + p->operation;
   record_pyrowave_vk_bind(c, 2, c->slots[c->decode ? c->command_slot : 0].packet,
         c->packet_offset + index * c->stride);
   c->api.cmd_dispatch(c->commands, x, y, 1);
   record_pyrowave_vk_barrier(c, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
         VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
   return 1;
}

static bool record_pyrowave_vk_fence_available(record_pyrowave_vulkan_t *c, VkFence fence)
{
   unsigned i;
   if (!fence || c->api.fence_status(c->device, fence) != VK_NOT_READY) return false;
   for (i = 0; i < c->count; i++) if (c->slots[i].fence == fence) return false;
   return true;
}

static void record_pyrowave_vk_retire(record_pyrowave_vulkan_t *c, uint64_t ticket)
{
   unsigned i;
   c->retired_marks[ticket % RECORD_GPU_QUEUE_SLOTS] = 1;
   for (i = 0; i < c->count; i++)
   {
      unsigned index = (unsigned)((c->retired + 1) % RECORD_GPU_QUEUE_SLOTS);
      if (!c->retired_marks[index]) break;
      c->retired_marks[index] = 0;
      c->retired++;
   }
}

int record_pyrowave_vulkan_push(record_pyrowave_vulkan_t *c, VkCommandBuffer commands,
      VkFence completion, VkImageView source, VkImageLayout layout,
      uint64_t timestamp, uint64_t duration, uint64_t *ticket)
{
   record_pyrowave_vk_slot_t *s;
   VkDescriptorImageInfo image;
   VkWriteDescriptorSet write;
   VkBufferCopy copies[2];
   rpyrowave_sequence_t sequence;
   unsigned slot;
   int result;
   if (!c || c->decode || c->failed || !commands || !source || !ticket || !c->next_ticket
         || !duration || timestamp > (uint64_t)-1 - duration
         || (layout != VK_IMAGE_LAYOUT_GENERAL && layout != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
         || !record_pyrowave_vk_fence_available(c, completion)) return RECORD_GPU_QUEUE_ERROR;
   if (c->next_ticket - c->retired > c->count) return RECORD_GPU_QUEUE_BUSY;
   result = record_gpu_queue_acquire(c->queue, &slot);
   if (result != RECORD_GPU_QUEUE_OK) return result;
   c->reservation = slot;
   s = &c->slots[slot];
   s->frame.timestamp = timestamp; s->frame.duration = duration;
   s->frame.ready_value = c->next_ticket++; s->frame.slot = slot; s->frame.keyframe = true;
   s->fence = completion;
   *ticket = s->frame.ready_value;
   retro_atomic_store_release_int(&s->ready, 0);
   memset(&image, 0, sizeof(image));
   image.imageView = source; image.imageLayout = layout;
   memset(&write, 0, sizeof(write));
   write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
   write.dstSet = s->color; write.descriptorCount = 1;
   write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE; write.pImageInfo = &image;
   c->api.update_sets(c->device, 1, &write, 0, NULL);
   c->commands = commands;
   record_pyrowave_vk_barrier(c, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
         VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
   record_pyrowave_vk_image_start(c, c->dummy.image);
   c->api.cmd_pipeline(commands, VK_PIPELINE_BIND_POINT_COMPUTE, c->pipelines[0]);
   record_pyrowave_vk_bind(c, 0, s->color, 0);
   c->api.cmd_dispatch(commands, (c->layout.image_width + 7) / 8, (c->layout.image_height + 7) / 8, 3);
   record_pyrowave_vk_barrier(c, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
         VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
   c->api.cmd_pipeline(commands, VK_PIPELINE_BIND_POINT_COMPUTE, c->pipelines[1]);
   c->cursor = 0;
   rpyrowave_dwt_record(c->layout.image_width, c->layout.image_height, c->layout.chroma_444,
         0, record_pyrowave_vk_dwt_dispatch, c);
   c->api.cmd_pipeline(commands, VK_PIPELINE_BIND_POINT_COMPUTE, c->pipelines[2]);
   sequence = c->sequence; sequence.sequence = (unsigned)(*ticket & 7);
   rpyrowave_gpu_encode_record(&c->layout, &sequence, c->layout.maximum_bytes, -1,
         record_pyrowave_vk_packet_dispatch, c);
   record_pyrowave_vk_barrier(c, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
         VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
   copies[0].srcOffset = copies[0].dstOffset = 0; copies[0].size = 16;
   copies[1].srcOffset = c->layout.wire_offset; copies[1].dstOffset = 16;
   copies[1].size = s->buffer.bytes - 16;
   c->api.cmd_copy(commands, c->buffers[3].handle, s->buffer.handle, 2, copies);
   record_pyrowave_vk_barrier(c, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
         VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
   if (!record_gpu_queue_submit(c->queue, &s->frame))
   {
      c->failed = true;
      return RECORD_GPU_QUEUE_ERROR;
   }
   c->reservation = RECORD_GPU_QUEUE_SLOTS;
   return RECORD_GPU_QUEUE_OK;
}

uint64_t record_pyrowave_vulkan_source_retired(const record_pyrowave_vulkan_t *c)
{
   return c ? c->retired : 0;
}

int record_pyrowave_vulkan_take(record_pyrowave_vulkan_t *c, record_gpu_frame_t *frame)
{
   int result;
   if (!c || c->decode || !frame) return RECORD_GPU_QUEUE_ERROR;
   result = record_gpu_queue_take(c->queue, frame);
   if (result == RECORD_GPU_QUEUE_OK) c->owned |= (unsigned)1 << frame->slot;
   return result;
}

int record_pyrowave_vulkan_packet(record_pyrowave_vulkan_t *c, unsigned slot, record_pyrowave_packet_t *packet)
{
   record_pyrowave_vk_slot_t *s;
   uint32_t bytes;
   int ready;
   if (!c || c->decode || !packet || slot >= c->count || !(c->owned & ((unsigned)1 << slot)))
      return RECORD_GPU_QUEUE_ERROR;
   s = &c->slots[slot];
   ready = retro_atomic_load_acquire_int(&s->ready);
   if (!ready) return RECORD_GPU_QUEUE_BUSY;
   if (ready != 1) return RECORD_GPU_QUEUE_ERROR;
   bytes = record_pyrowave_vk_load32(s->host + 4);
   if (bytes < 8 || bytes > c->layout.maximum_bytes || record_pyrowave_vk_load32(s->host + 12))
      return RECORD_GPU_QUEUE_ERROR;
   packet->data = s->host + 16; packet->bytes = bytes;
   return RECORD_GPU_QUEUE_OK;
}

bool record_pyrowave_vulkan_release(record_pyrowave_vulkan_t *c, unsigned slot)
{
   if (!c || c->decode || slot >= c->count || !(c->owned & ((unsigned)1 << slot))
         || !retro_atomic_load_acquire_int(&c->slots[slot].ready)) return false;
   retro_atomic_store_release_int(&c->slots[slot].ready, 0);
   c->owned &= ~((unsigned)1 << slot);
   return record_gpu_queue_release(c->queue, slot);
}

int record_pyrowave_vulkan_receive_acquire(record_pyrowave_vulkan_t *c, void **data, size_t *capacity)
{
   int result;
   if (data) *data = NULL;
   if (capacity) *capacity = 0;
   if (!c || !c->decode || !data || !capacity) return RECORD_GPU_QUEUE_ERROR;
   result = record_gpu_queue_acquire(c->queue, &c->reservation);
   if (result != RECORD_GPU_QUEUE_OK) return result;
   *data = c->slots[c->reservation].host + c->layout.wire_offset;
   *capacity = c->layout.maximum_bytes;
   return RECORD_GPU_QUEUE_OK;
}

int record_pyrowave_vulkan_receive_submit(record_pyrowave_vulkan_t *c,
      size_t bytes, uint64_t timestamp, uint64_t duration)
{
   record_pyrowave_vk_slot_t *s;
   if (!c || !c->decode || c->reservation >= c->count || !c->next_ticket
         || !duration || timestamp > (uint64_t)-1 - duration) return RECORD_GPU_QUEUE_ERROR;
   s = &c->slots[c->reservation];
   if (rpyrowave_gpu_index_frame(&c->layout, s->host, c->layout.workspace_bytes, bytes, 0, &s->parameters)
         || s->parameters.usability != c->usability) return RECORD_GPU_QUEUE_ERROR;
   s->frame.timestamp = timestamp; s->frame.duration = duration; s->frame.ready_value = c->next_ticket;
   s->frame.slot = c->reservation; s->frame.keyframe = true;
   if (!record_gpu_queue_submit(c->queue, &s->frame)) return RECORD_GPU_QUEUE_ERROR;
   c->next_ticket++; c->reservation = RECORD_GPU_QUEUE_SLOTS;
   return RECORD_GPU_QUEUE_OK;
}

bool record_pyrowave_vulkan_receive_cancel(record_pyrowave_vulkan_t *c)
{
   if (!c || !c->decode || !record_gpu_queue_cancel(c->queue)) return false;
   c->reservation = RECORD_GPU_QUEUE_SLOTS;
   return true;
}

int record_pyrowave_vulkan_receive_peek(record_pyrowave_vulkan_t *c, record_gpu_frame_t *frame)
{
   return c && c->decode ? record_gpu_queue_peek(c->queue, frame) : RECORD_GPU_QUEUE_ERROR;
}

int record_pyrowave_vulkan_receive_drop(record_pyrowave_vulkan_t *c, record_gpu_frame_t *frame)
{
   int result;
   if (!c || !c->decode || !frame) return RECORD_GPU_QUEUE_ERROR;
   result = record_gpu_queue_take(c->queue, frame);
   if (result == RECORD_GPU_QUEUE_OK && !record_gpu_queue_release(c->queue, frame->slot)) return RECORD_GPU_QUEUE_ERROR;
   return result;
}

int record_pyrowave_vulkan_decode(record_pyrowave_vulkan_t *c, VkCommandBuffer commands, VkFence completion)
{
   record_gpu_frame_t frame;
   record_pyrowave_vk_slot_t *s;
   size_t bytes;
   int result;
   if (!c || !c->decode || c->failed || !commands || !record_pyrowave_vk_fence_available(c, completion))
      return RECORD_GPU_QUEUE_ERROR;
   result = record_gpu_queue_take(c->queue, &frame);
   if (result != RECORD_GPU_QUEUE_OK) return result;
   s = &c->slots[frame.slot];
   bytes = c->layout.wire_offset + s->parameters.maximum_bytes;
   if (s->host != s->buffer.mapped) memcpy(s->buffer.mapped, s->host, bytes);
   if (!record_pyrowave_vk_cache(c, &s->buffer, false))
   {
      c->failed = true; record_gpu_queue_release(c->queue, frame.slot); return RECORD_GPU_QUEUE_ERROR;
   }
   s->state = RPY_VK_DECODING; s->fence = completion;
   c->commands = commands; c->command_slot = frame.slot;
   record_pyrowave_vk_barrier(c, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_HOST_BIT,
         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT
         | VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
   record_pyrowave_vk_image_start(c, c->dummy.image);
   record_pyrowave_vk_image_start(c, s->texture.image);
   c->api.cmd_pipeline(commands, VK_PIPELINE_BIND_POINT_COMPUTE, c->pipelines[2]);
   rpyrowave_gpu_decode_record(&c->layout, &s->parameters, record_pyrowave_vk_packet_dispatch, c);
   c->api.cmd_pipeline(commands, VK_PIPELINE_BIND_POINT_COMPUTE, c->pipelines[1]);
   c->cursor = 0;
   rpyrowave_dwt_record(c->layout.image_width, c->layout.image_height, c->layout.chroma_444,
         1, record_pyrowave_vk_dwt_dispatch, c);
   c->api.cmd_pipeline(commands, VK_PIPELINE_BIND_POINT_COMPUTE, c->pipelines[0]);
   record_pyrowave_vk_bind(c, 0, s->color, 0);
   c->api.cmd_dispatch(commands, (c->layout.width + 7) / 8, (c->layout.height + 7) / 8, 1);
   record_pyrowave_vk_barrier(c, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
         VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_SHADER_WRITE_BIT,
         VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_HOST_READ_BIT);
   return RECORD_GPU_QUEUE_OK;
}

int record_pyrowave_vulkan_image(record_pyrowave_vulkan_t *c, record_pyrowave_vulkan_image_t *image)
{
   unsigned i, selected = RECORD_GPU_QUEUE_SLOTS;
   if (!c || !c->decode || !image) return RECORD_GPU_QUEUE_ERROR;
   for (i = 0; i < c->count; i++)
      if (c->slots[i].state == RPY_VK_READY && (selected == RECORD_GPU_QUEUE_SLOTS
               || c->slots[i].frame.ready_value < c->slots[selected].frame.ready_value)) selected = i;
   if (selected == RECORD_GPU_QUEUE_SLOTS) return RECORD_GPU_QUEUE_BUSY;
   image->frame = c->slots[selected].frame; image->image = c->slots[selected].texture.image;
   image->view = c->slots[selected].texture.view; image->layout = VK_IMAGE_LAYOUT_GENERAL;
   image->queue_family = c->family; image->texture_mode = c->mode;
   c->slots[selected].state = RPY_VK_LENT;
   return RECORD_GPU_QUEUE_OK;
}

bool record_pyrowave_vulkan_image_drop(record_pyrowave_vulkan_t *c, unsigned slot)
{
   if (!c || !c->decode || slot >= c->count || c->slots[slot].state != RPY_VK_LENT) return false;
   c->slots[slot].state = RPY_VK_IDLE;
   return record_gpu_queue_release(c->queue, slot);
}

bool record_pyrowave_vulkan_present(record_pyrowave_vulkan_t *c, unsigned slot, VkFence completion)
{
   VkResult status;
   if (!c || !c->decode || c->failed || !completion || slot >= c->count
         || c->slots[slot].state != RPY_VK_LENT) return false;
   status = c->api.fence_status(c->device, completion);
   if (status != VK_SUCCESS && status != VK_NOT_READY) return false;
   /* A held image can return after its last draw completed. Multiple loans
    * can share that submission; its owner must retain the signal until ALL
    * native references and its own display pins have acknowledged it. */
   c->slots[slot].fence = completion; c->slots[slot].state = RPY_VK_DISPLAYING;
   return true;
}

bool record_pyrowave_vulkan_fence_borrowed(const record_pyrowave_vulkan_t *c, VkFence fence)
{
   unsigned i;
   if (!c || !fence) return false;
   for (i = 0; i < c->count; i++) if (c->slots[i].fence == fence) return true;
   return false;
}

bool record_pyrowave_vulkan_idle(const record_pyrowave_vulkan_t *c)
{
   unsigned i;
   if (!c) return false;
   for (i = 0; i < c->count; i++) if (c->slots[i].fence) return false;
   return true;
}

static bool record_pyrowave_vk_encode_complete(record_pyrowave_vulkan_t *c, unsigned slot, bool discard)
{
   record_pyrowave_vk_slot_t *s = &c->slots[slot];
   int ready = 2;
   if (!discard && record_pyrowave_vk_cache(c, &s->buffer, true))
   {
      uint32_t bytes;
      if (s->host != s->buffer.mapped) memcpy(s->host, s->buffer.mapped, 16);
      bytes = record_pyrowave_vk_load32(s->host + 4);
      if (bytes >= 8 && bytes <= c->layout.maximum_bytes && !record_pyrowave_vk_load32(s->host + 12))
      {
         if (s->host != s->buffer.mapped) memcpy(s->host + 16, (uint8_t*)s->buffer.mapped + 16, bytes);
         ready = 1;
      }
   }
   record_pyrowave_vk_retire(c, s->frame.ready_value);
   if (c->reservation == slot)
   {
      record_gpu_queue_cancel(c->queue); c->reservation = RECORD_GPU_QUEUE_SLOTS;
   }
   else retro_atomic_store_release_int(&s->ready, ready);
   return ready == 1 || discard;
}

bool record_pyrowave_vulkan_poll(record_pyrowave_vulkan_t *c)
{
   unsigned i;
   bool valid = true;
   if (!c) return false;
   for (i = 0; i < c->count; i++)
   {
      record_pyrowave_vk_slot_t *s = &c->slots[i];
      VkResult status;
      if (!s->fence) continue;
      status = c->api.fence_status(c->device, s->fence);
      if (status == VK_NOT_READY) continue;
      if (status != VK_SUCCESS)
      {
         c->failed = true;
         continue;
      }
      s->fence = VK_NULL_HANDLE;
      if (!c->decode)
      {
         if (!record_pyrowave_vk_encode_complete(c, i, false)) valid = false;
      }
      else if (s->state == RPY_VK_DECODING && record_pyrowave_vk_cache(c, &s->buffer, true)
            && !record_pyrowave_vk_load32((uint8_t*)s->buffer.mapped + 12)) s->state = RPY_VK_READY;
      else
      {
         if (s->state == RPY_VK_DECODING) valid = false;
         s->state = RPY_VK_IDLE; record_gpu_queue_release(c->queue, i);
      }
   }
   return valid && !c->failed;
}

bool record_pyrowave_vulkan_discard(record_pyrowave_vulkan_t *c, VkFence completion)
{
   unsigned i;
   bool found = false;
   if (!c || !completion) return false;
   for (i = 0; i < c->count; i++)
   {
      record_pyrowave_vk_slot_t *s = &c->slots[i];
      if (s->fence != completion) continue;
      s->fence = VK_NULL_HANDLE;
      if (!c->decode) record_pyrowave_vk_encode_complete(c, i, true);
      else
      {
         s->state = RPY_VK_IDLE;
         record_gpu_queue_release(c->queue, i);
      }
      found = true;
   }
   return found;
}

bool record_pyrowave_vulkan_finish(record_pyrowave_vulkan_t *c)
{
   return c && record_gpu_queue_finish(c->queue);
}

bool record_pyrowave_vulkan_free(record_pyrowave_vulkan_t *c)
{
   unsigned i;
   if (!c) return true;
   for (i = 0; i < c->count; i++) if (c->slots[i].fence) return false;
   if (!record_gpu_queue_free(c->queue)) return false;
   record_pyrowave_vk_destroy(c);
   return true;
}

bool record_pyrowave_vulkan_abandon(record_pyrowave_vulkan_t *c)
{
   record_gpu_frame_t frame;
   unsigned i;
   if (!c) return true;
   if (c->reservation != RECORD_GPU_QUEUE_SLOTS) record_gpu_queue_cancel(c->queue);
   for (i = 0; i < c->count; i++) if (record_gpu_queue_take(c->queue, &frame) != RECORD_GPU_QUEUE_OK) break;
   for (i = 0; i < c->count; i++) record_gpu_queue_release(c->queue, i);
   if (!record_gpu_queue_free(c->queue)) return false;
   record_pyrowave_vk_destroy(c);
   return true;
}

RECORD_PYROWAVE_SESSION_BINDING(record_pyrowave_vulkan, record_pyrowave_vulkan_t, record_pyrowave_vulkan_receive_acquire)
