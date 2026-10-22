/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "vaapi_hevc_vulkan_resources.h"
#include <string.h>
static const uint32_t vaapi_hevc_rgba8_shader[] =
#include "../drivers/vulkan_shaders/yuv_image_to_rgba8888.comp.inc"
;
static const uint32_t vaapi_hevc_fp16_shader[] =
#include "../drivers/vulkan_shaders/yuv_image_to_rgba16f.comp.inc"
;

void vaapi_hevc_vulkan_resources_destroy(vaapi_hevc_vulkan_resources_t *r)
{
   unsigned i;
   if (!r || !r->device)
      return;
   for (i = 0; i < 32; i++)
      if (r->fences[i])
         r->ops.destroy_fence(r->device, r->fences[i], NULL);
   if (r->command_pool)
      r->ops.destroy_commands(r->device, r->command_pool, NULL);
   if (r->pipeline)
      r->ops.destroy_pipeline(r->device, r->pipeline, NULL);
   if (r->pool)
      r->ops.destroy_pool(r->device, r->pool, NULL);
   if (r->layout)
      r->ops.destroy_layout(r->device, r->layout, NULL);
   if (r->set_layout)
      r->ops.destroy_set_layout(r->device, r->set_layout, NULL);
   if (r->sampler)
      r->ops.destroy_sampler(r->device, r->sampler, NULL);
   memset(r, 0, sizeof(*r));
}

bool vaapi_hevc_vulkan_resources_after_device_destroy(
      vaapi_hevc_vulkan_resources_t *r, VkDevice device)
{
   if (!r || !device || r->device != device)
      return false;
   memset(r, 0, sizeof(*r));
   return true;
}

bool vaapi_hevc_vulkan_resources_create(vaapi_hevc_vulkan_resources_t *r,
      VkDevice device, unsigned family, unsigned count, bool fp16,
      const vaapi_hevc_vulkan_resource_ops_t *ops)
{
   VkSamplerCreateInfo sampler;
   VkDescriptorSetLayoutBinding bindings[3];
   VkDescriptorSetLayoutCreateInfo set;
   VkPipelineLayoutCreateInfo layout;
   VkPushConstantRange push;
   VkDescriptorPoolSize sizes[2];
   VkDescriptorPoolCreateInfo pool;
   VkDescriptorSetAllocateInfo allocation;
   VkDescriptorSetLayout layouts[32];
   VkShaderModuleCreateInfo shader;
   VkComputePipelineCreateInfo pipeline;
   VkCommandPoolCreateInfo commands;
   VkCommandBufferAllocateInfo buffers;
   VkFenceCreateInfo fence;
   VkShaderModule module = VK_NULL_HANDLE;
   vaapi_hevc_vulkan_resource_ops_t native;
   unsigned i;
   if (!r || r->device || !device || !count || count > 32
         || family >= VK_QUEUE_FAMILY_FOREIGN_EXT || !ops
         || !ops->create_sampler || !ops->destroy_sampler
         || !ops->create_set_layout || !ops->destroy_set_layout
         || !ops->create_layout || !ops->destroy_layout || !ops->create_pool
         || !ops->destroy_pool || !ops->allocate_sets || !ops->create_shader
         || !ops->destroy_shader || !ops->create_pipeline
         || !ops->destroy_pipeline || !ops->create_commands
         || !ops->destroy_commands || !ops->allocate_commands
         || !ops->create_fence || !ops->destroy_fence)
      return false;
   native = *ops;
   memset(r, 0, sizeof(*r));
   r->ops                          = native;
   r->device                       = device;
   ops                             = &r->ops;
   sampler.pNext                   = NULL;
   sampler.flags                   = 0;
   sampler.magFilter               = VK_FILTER_NEAREST;
   sampler.minFilter               = VK_FILTER_NEAREST;
   sampler.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_NEAREST;
   sampler.mipLodBias              = 0.0f;
   sampler.anisotropyEnable        = VK_FALSE;
   sampler.maxAnisotropy           = 0.0f;
   sampler.compareEnable           = VK_FALSE;
   sampler.compareOp               = VK_COMPARE_OP_NEVER;
   sampler.minLod                  = 0.0f;
   sampler.maxLod                  = 0.0f;
   sampler.borderColor             = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
   sampler.unnormalizedCoordinates = VK_FALSE;
   sampler.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
   sampler.addressModeU = sampler.addressModeV = sampler.addressModeW =
         VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
   if (ops->create_sampler(device, &sampler, NULL, &r->sampler) != VK_SUCCESS)
      goto failed;

   for (i = 0; i < 3; i++)
   {
      bindings[i].pImmutableSamplers = NULL;
      bindings[i].binding            = i + 1;
      bindings[i].descriptorCount    = 1;
      bindings[i].stageFlags         = VK_SHADER_STAGE_COMPUTE_BIT;
      bindings[i].descriptorType =
            i < 2 ? VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
                  : VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
   }
   set.pNext        = NULL;
   set.flags        = 0;
   set.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
   set.bindingCount = 3;
   set.pBindings    = bindings;
   if (ops->create_set_layout(device, &set, NULL, &r->set_layout) != VK_SUCCESS)
      goto failed;
   push.offset           = 0;
   push.stageFlags       = VK_SHADER_STAGE_COMPUTE_BIT;
   push.size             = 80;
   layout.pNext          = NULL;
   layout.flags          = 0;
   layout.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
   layout.setLayoutCount = 1;
   layout.pSetLayouts    = &r->set_layout;
   layout.pushConstantRangeCount = 1;
   layout.pPushConstantRanges    = &push;
   if (ops->create_layout(device, &layout, NULL, &r->layout) != VK_SUCCESS)
      goto failed;

   sizes[0].type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
   sizes[0].descriptorCount = 2 * count;
   sizes[1].type            = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
   sizes[1].descriptorCount = count;
   pool.pNext               = NULL;
   pool.flags               = 0;
   pool.sType               = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
   pool.maxSets             = count;
   pool.poolSizeCount       = 2;
   pool.pPoolSizes          = sizes;
   if (ops->create_pool(device, &pool, NULL, &r->pool) != VK_SUCCESS)
      goto failed;
   for (i = 0; i < count; i++)
      layouts[i] = r->set_layout;
   allocation.pNext          = NULL;
   allocation.sType          = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
   allocation.descriptorPool = r->pool;
   allocation.descriptorSetCount = count;
   allocation.pSetLayouts        = layouts;
   if (ops->allocate_sets(device, &allocation, r->sets) != VK_SUCCESS)
      goto failed;
   shader.pNext    = NULL;
   shader.flags    = 0;
   shader.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
   shader.pCode    = fp16 ? vaapi_hevc_fp16_shader : vaapi_hevc_rgba8_shader;
   shader.codeSize = fp16 ? sizeof(vaapi_hevc_fp16_shader)
                          : sizeof(vaapi_hevc_rgba8_shader);
   if (ops->create_shader(device, &shader, NULL, &module) != VK_SUCCESS)
      goto failed;
   pipeline.pNext                     = NULL;
   pipeline.flags                     = 0;
   pipeline.basePipelineHandle        = VK_NULL_HANDLE;
   pipeline.basePipelineIndex         = 0;
   pipeline.stage.pNext               = NULL;
   pipeline.stage.flags               = 0;
   pipeline.stage.pSpecializationInfo = NULL;
   pipeline.sType        = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
   pipeline.layout       = r->layout;
   pipeline.stage.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
   pipeline.stage.stage  = VK_SHADER_STAGE_COMPUTE_BIT;
   pipeline.stage.module = module;
   pipeline.stage.pName  = "main";
   if (ops->create_pipeline(
             device, VK_NULL_HANDLE, 1, &pipeline, NULL, &r->pipeline)
         != VK_SUCCESS)
      goto failed;
   ops->destroy_shader(device, module, NULL);
   module                    = VK_NULL_HANDLE;
   commands.pNext            = NULL;
   commands.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
   commands.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
   commands.queueFamilyIndex = family;
   if (ops->create_commands(device, &commands, NULL, &r->command_pool)
         != VK_SUCCESS)
      goto failed;
   buffers.pNext              = NULL;
   buffers.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
   buffers.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
   buffers.commandPool        = r->command_pool;
   buffers.commandBufferCount = count;
   if (ops->allocate_commands(device, &buffers, r->commands) != VK_SUCCESS)
      goto failed;
   fence.pNext = NULL;
   fence.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
   fence.flags = VK_FENCE_CREATE_SIGNALED_BIT;
   for (i = 0; i < count; i++)
      if (ops->create_fence(device, &fence, NULL, &r->fences[i]) != VK_SUCCESS)
         goto failed;
   r->format = fp16 ? VK_FORMAT_R16G16B16A16_SFLOAT : VK_FORMAT_R8G8B8A8_UNORM;
   r->count  = count;
   return true;
failed:
   if (module)
      ops->destroy_shader(device, module, NULL);
   vaapi_hevc_vulkan_resources_destroy(r);
   return false;
}
