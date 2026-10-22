/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include "vulkan_android_pipeline.h"
#include <string.h>
void vulkan_android_pipeline_destroy(VkDevice device,
      const vulkan_android_pipeline_ops_t *ops, vulkan_android_pipeline_t *p)
{
   if (!p || !ops)
      return;
   if (p->pipeline)
      ops->destroy_pipeline(device, p->pipeline, NULL);
   if (p->pool)
      ops->destroy_pool(device, p->pool, NULL);
   if (p->layout)
      ops->destroy_layout(device, p->layout, NULL);
   if (p->set_layout)
      ops->destroy_set_layout(device, p->set_layout, NULL);
   memset(p, 0, sizeof(*p));
}
bool vulkan_android_pipeline_create(VkDevice device,
      const vulkan_android_pipeline_ops_t *ops, VkSampler sampler,
      unsigned sampler_descriptors, unsigned slots, const uint32_t *shader,
      size_t shader_bytes, vulkan_android_pipeline_t *out)
{
   VkDescriptorSetLayoutBinding bindings[2];
   VkDescriptorSetLayoutCreateInfo set_info;
   VkPipelineLayoutCreateInfo layout;
   VkPushConstantRange push;
   VkDescriptorPoolSize sizes[2];
   VkDescriptorPoolCreateInfo pool;
   VkDescriptorSetAllocateInfo sets;
   VkDescriptorSetLayout layouts[32];
   VkShaderModuleCreateInfo module_info;
   VkComputePipelineCreateInfo pipeline;
   VkShaderModule module = VK_NULL_HANDLE;
   VkResult result;
   unsigned i;
   if (!out)
      return false;
   memset(out, 0, sizeof(*out));
   if (!device || !sampler || !sampler_descriptors || !slots || slots > 32
         || sampler_descriptors > UINT32_MAX / slots || !shader
         || ((uintptr_t)shader & 3) || shader_bytes < 20 || shader_bytes % 4
         || shader[0] != 0x07230203 || !ops || !ops->create_set_layout
         || !ops->destroy_set_layout || !ops->create_layout
         || !ops->destroy_layout || !ops->create_pool || !ops->destroy_pool
         || !ops->allocate_sets || !ops->update_sets || !ops->create_shader
         || !ops->destroy_shader || !ops->create_pipeline
         || !ops->destroy_pipeline)
      return false;
   memset(bindings, 0, sizeof(bindings));
   bindings[0].binding = 1;
   bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
   bindings[0].descriptorCount = 1;
   bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
   bindings[0].pImmutableSamplers = &sampler;
   bindings[1].binding = 3;
   bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
   bindings[1].descriptorCount = 1;
   bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
   memset(&set_info, 0, sizeof(set_info));
   set_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
   set_info.bindingCount = 2;
   set_info.pBindings = bindings;
   if (ops->create_set_layout(device, &set_info, NULL, &out->set_layout)
         != VK_SUCCESS)
      goto fail;
   memset(&push, 0, sizeof(push));
   push.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
   push.size = 80;
   memset(&layout, 0, sizeof(layout));
   layout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
   layout.setLayoutCount = 1;
   layout.pSetLayouts = &out->set_layout;
   layout.pushConstantRangeCount = 1;
   layout.pPushConstantRanges = &push;
   if (ops->create_layout(device, &layout, NULL, &out->layout) != VK_SUCCESS)
      goto fail;
   memset(sizes, 0, sizeof(sizes));
   sizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
   sizes[0].descriptorCount = sampler_descriptors * slots;
   sizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
   sizes[1].descriptorCount = slots;
   memset(&pool, 0, sizeof(pool));
   pool.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
   pool.maxSets = slots;
   pool.poolSizeCount = 2;
   pool.pPoolSizes = sizes;
   if (ops->create_pool(device, &pool, NULL, &out->pool) != VK_SUCCESS)
      goto fail;
   for (i = 0; i < slots; i++)
      layouts[i] = out->set_layout;
   memset(&sets, 0, sizeof(sets));
   sets.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
   sets.descriptorPool = out->pool;
   sets.descriptorSetCount = slots;
   sets.pSetLayouts = layouts;
   if (ops->allocate_sets(device, &sets, out->sets) != VK_SUCCESS)
      goto fail;
   memset(&module_info, 0, sizeof(module_info));
   module_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
   module_info.codeSize = shader_bytes;
   module_info.pCode = shader;
   if (ops->create_shader(device, &module_info, NULL, &module) != VK_SUCCESS)
      goto fail;
   memset(&pipeline, 0, sizeof(pipeline));
   pipeline.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
   pipeline.layout = out->layout;
   pipeline.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
   pipeline.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
   pipeline.stage.module = module;
   pipeline.stage.pName = "main";
   result = ops->create_pipeline(
         device, VK_NULL_HANDLE, 1, &pipeline, NULL, &out->pipeline);
   ops->destroy_shader(device, module, NULL);
   if (result != VK_SUCCESS)
      goto fail;
   out->count = slots;
   return true;
fail:
   vulkan_android_pipeline_destroy(device, ops, out);
   return false;
}
bool vulkan_android_pipeline_update(VkDevice device,
      const vulkan_android_pipeline_ops_t *ops,
      const vulkan_android_pipeline_t *p, unsigned slot, VkImageView source,
      VkImageView destination)
{
   VkDescriptorImageInfo images[2];
   VkWriteDescriptorSet writes[2];
   unsigned i;
   if (!device || !ops || !ops->update_sets || !p || slot >= p->count
         || p->count > 32 || !p->sets[slot] || !source || !destination)
      return false;
   memset(images, 0, sizeof(images));
   memset(writes, 0, sizeof(writes));
   images[0].imageView = source;
   images[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
   images[1].imageView = destination;
   images[1].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
   for (i = 0; i < 2; i++)
   {
      writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      writes[i].dstSet = p->sets[slot];
      writes[i].dstBinding = i ? 3 : 1;
      writes[i].descriptorCount = 1;
      writes[i].pImageInfo = &images[i];
      writes[i].descriptorType = i ? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE
                                   : VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
   }
   ops->update_sets(device, 2, writes, 0, NULL);
   return true;
}
