/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../../gfx/common/vulkan_android_pipeline.h"
#define HANDLE(t, n) ((t)(uintptr_t)(n))
static int step, fail, live, updates;
static VkResult action(void)
{
   return ++step == fail ? VK_ERROR_OUT_OF_DEVICE_MEMORY : VK_SUCCESS;
}
static VKAPI_ATTR VkResult VKAPI_CALL set_layout(VkDevice d,
      const VkDescriptorSetLayoutCreateInfo *p, const VkAllocationCallbacks *a,
      VkDescriptorSetLayout *out)
{
   assert(d && !a && p->bindingCount == 2);
   assert(p->pBindings[0].binding == 1 && p->pBindings[0].descriptorCount == 1
         && *p->pBindings[0].pImmutableSamplers == HANDLE(VkSampler, 9));
   assert(p->pBindings[1].binding == 3 && !p->pBindings[1].pImmutableSamplers);
   if (action() != VK_SUCCESS)
      return VK_ERROR_OUT_OF_DEVICE_MEMORY;
   *out = HANDLE(VkDescriptorSetLayout, 1);
   live |= 1;
   return VK_SUCCESS;
}
static VKAPI_ATTR VkResult VKAPI_CALL layout(VkDevice d,
      const VkPipelineLayoutCreateInfo *p, const VkAllocationCallbacks *a,
      VkPipelineLayout *out)
{
   assert(d && !a && p->setLayoutCount == 1
         && *p->pSetLayouts == HANDLE(VkDescriptorSetLayout, 1));
   assert(p->pushConstantRangeCount == 1 && p->pPushConstantRanges->size == 80
         && p->pPushConstantRanges->stageFlags == VK_SHADER_STAGE_COMPUTE_BIT);
   if (action() != VK_SUCCESS)
      return VK_ERROR_OUT_OF_DEVICE_MEMORY;
   *out = HANDLE(VkPipelineLayout, 2);
   live |= 2;
   return VK_SUCCESS;
}
static VKAPI_ATTR VkResult VKAPI_CALL pool(VkDevice d,
      const VkDescriptorPoolCreateInfo *p, const VkAllocationCallbacks *a,
      VkDescriptorPool *out)
{
   assert(d && !a && p->maxSets == 2 && p->poolSizeCount == 2);
   assert(p->pPoolSizes[0].descriptorCount == 6
         && p->pPoolSizes[1].descriptorCount == 2);
   if (action() != VK_SUCCESS)
      return VK_ERROR_OUT_OF_DEVICE_MEMORY;
   *out = HANDLE(VkDescriptorPool, 3);
   live |= 4;
   return VK_SUCCESS;
}
static VKAPI_ATTR VkResult VKAPI_CALL sets(
      VkDevice d, const VkDescriptorSetAllocateInfo *p, VkDescriptorSet *out)
{
   assert(d && p->descriptorSetCount == 2
         && p->descriptorPool == HANDLE(VkDescriptorPool, 3));
   if (action() != VK_SUCCESS)
      return VK_ERROR_OUT_OF_DEVICE_MEMORY;
   out[0] = HANDLE(VkDescriptorSet, 4);
   out[1] = HANDLE(VkDescriptorSet, 5);
   return VK_SUCCESS;
}
static VKAPI_ATTR VkResult VKAPI_CALL shader(VkDevice d,
      const VkShaderModuleCreateInfo *p, const VkAllocationCallbacks *a,
      VkShaderModule *out)
{
   assert(d && !a && p->codeSize == 20 && p->pCode[0] == 0x07230203);
   if (action() != VK_SUCCESS)
      return VK_ERROR_OUT_OF_DEVICE_MEMORY;
   *out = HANDLE(VkShaderModule, 6);
   live |= 8;
   return VK_SUCCESS;
}
static VKAPI_ATTR VkResult VKAPI_CALL pipeline(VkDevice d,
      VkPipelineCache cache, uint32_t count,
      const VkComputePipelineCreateInfo *p, const VkAllocationCallbacks *a,
      VkPipeline *out)
{
   assert(d && !cache && count == 1 && !a
         && p->layout == HANDLE(VkPipelineLayout, 2));
   assert(p->stage.module == HANDLE(VkShaderModule, 6)
         && !strcmp(p->stage.pName, "main"));
   if (action() != VK_SUCCESS)
   {
      /* Vulkan may return a partial pipeline even on an error. */
      *out = HANDLE(VkPipeline, 7);
      live |= 16;
      return VK_ERROR_OUT_OF_DEVICE_MEMORY;
   }
   *out = HANDLE(VkPipeline, 7);
   live |= 16;
   return VK_SUCCESS;
}
static VKAPI_ATTR void VKAPI_CALL update(VkDevice d, uint32_t count,
      const VkWriteDescriptorSet *p, uint32_t nc, const VkCopyDescriptorSet *c)
{
   assert(d && count == 2 && !nc && !c
         && p[0].dstSet == HANDLE(VkDescriptorSet, 5));
   assert(p[0].dstBinding == 1 && !p[0].pImageInfo->sampler
         && p[0].pImageInfo->imageView == HANDLE(VkImageView, 10));
   assert(p[1].dstBinding == 3
         && p[1].pImageInfo->imageLayout == VK_IMAGE_LAYOUT_GENERAL);
   updates++;
}
#define DESTROY(name, type, bit, prereq) \
   static VKAPI_ATTR void VKAPI_CALL name( \
         VkDevice d, type h, const VkAllocationCallbacks *a) \
   { \
      assert(d && h && !a && (live & (bit)) && !(live & (prereq))); \
      live &= ~(bit); \
   }
DESTROY(dset, VkDescriptorSetLayout, 1, 30)
DESTROY(dlayout, VkPipelineLayout, 2, 28)
DESTROY(dpool, VkDescriptorPool, 4, 24)
DESTROY(dshader, VkShaderModule, 8, 0)
DESTROY(dpipeline, VkPipeline, 16, 0)
int main(void)
{
   vulkan_android_pipeline_ops_t ops;
   vulkan_android_pipeline_t p, empty;
   static const uint32_t code[] = {0x07230203, 0, 0, 0, 0};
   int i;
   memset(&ops, 0, sizeof(ops));
   memset(&empty, 0, sizeof(empty));
   ops.create_set_layout = set_layout;
   ops.destroy_set_layout = dset;
   ops.create_layout = layout;
   ops.destroy_layout = dlayout;
   ops.create_pool = pool;
   ops.destroy_pool = dpool;
   ops.allocate_sets = sets;
   ops.update_sets = update;
   ops.create_shader = shader;
   ops.destroy_shader = dshader;
   ops.create_pipeline = pipeline;
   ops.destroy_pipeline = dpipeline;
   for (i = 1; i <= 6; i++)
   {
      fail = i;
      step = 0;
      assert(!vulkan_android_pipeline_create(HANDLE(VkDevice, 1), &ops,
            HANDLE(VkSampler, 9), 3, 2, code, sizeof(code), &p));
      assert(!live && !memcmp(&p, &empty, sizeof(p)));
   }
   fail = 0;
   step = 0;
   assert(vulkan_android_pipeline_create(HANDLE(VkDevice, 1), &ops,
         HANDLE(VkSampler, 9), 3, 2, code, sizeof(code), &p));
   assert(live == 23 && p.count == 2);
   assert(!vulkan_android_pipeline_update(HANDLE(VkDevice, 1), &ops, &p, 2,
         HANDLE(VkImageView, 10), HANDLE(VkImageView, 11)));
   assert(!updates);
   assert(vulkan_android_pipeline_update(HANDLE(VkDevice, 1), &ops, &p, 1,
         HANDLE(VkImageView, 10), HANDLE(VkImageView, 11)));
   assert(updates == 1);
   vulkan_android_pipeline_destroy(HANDLE(VkDevice, 1), &ops, &p);
   assert(!live);
   puts("PASS: immutable sampler pipeline, expanded descriptor budgeting, "
        "bounded sets and partial pipeline unwind");
   return 0;
}
