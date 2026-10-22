/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../../gfx/common/vaapi_hevc_vulkan_resources.h"
enum { SAMPLER=1, SET_LAYOUT=2, LAYOUT=4, POOL=8, SHADER=16, PIPELINE=32, COMMAND_POOL=64, SETS=128, COMMANDS=256 };
static vaapi_hevc_vulkan_resources_t resources, before;
static unsigned live, calls, fault, count, next_fence, destroyed;
static uint32_t fences;
static VkDevice device;
static bool fail(void) { calls++; return calls == fault; }
#define CREATE_START(bit) \
   assert(d == device && !a && info && out && !(live & (bit))); \
   if (fail()) return VK_ERROR_OUT_OF_HOST_MEMORY; \
   live |= (bit)
#define DESTROY_START(bit) \
   assert(d == device && !a && (live & (bit))); destroyed++; live &= ~(bit)
static VKAPI_ATTR VkResult VKAPI_CALL sampler_create(VkDevice d, const VkSamplerCreateInfo *info,
      const VkAllocationCallbacks *a, VkSampler *out)
{
   CREATE_START(SAMPLER);
   assert(info->sType == VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO && !info->pNext
         && info->magFilter == VK_FILTER_NEAREST && info->minFilter == VK_FILTER_NEAREST
         && info->mipmapMode == VK_SAMPLER_MIPMAP_MODE_NEAREST && !info->anisotropyEnable
         && !info->compareEnable && !info->unnormalizedCoordinates
         && info->addressModeU == VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE
         && info->addressModeV == info->addressModeU && info->addressModeW == info->addressModeU);
   *out = (VkSampler)(uintptr_t)SAMPLER; return VK_SUCCESS;
}
static VKAPI_ATTR void VKAPI_CALL sampler_destroy(VkDevice d, VkSampler value, const VkAllocationCallbacks *a)
{ assert((uintptr_t)value == SAMPLER && live == SAMPLER); DESTROY_START(SAMPLER); }
static VKAPI_ATTR VkResult VKAPI_CALL set_create(VkDevice d, const VkDescriptorSetLayoutCreateInfo *info,
      const VkAllocationCallbacks *a, VkDescriptorSetLayout *out)
{
   unsigned i;
   CREATE_START(SET_LAYOUT);
   assert(info->sType == VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO && !info->pNext && info->bindingCount == 3);
   for (i = 0; i < 3; i++) assert(info->pBindings[i].binding == i+1 && info->pBindings[i].descriptorCount == 1
         && info->pBindings[i].stageFlags == VK_SHADER_STAGE_COMPUTE_BIT && !info->pBindings[i].pImmutableSamplers
         && info->pBindings[i].descriptorType == (i < 2 ? VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER : VK_DESCRIPTOR_TYPE_STORAGE_IMAGE));
   *out = (VkDescriptorSetLayout)(uintptr_t)SET_LAYOUT; return VK_SUCCESS;
}
static VKAPI_ATTR void VKAPI_CALL set_destroy(VkDevice d, VkDescriptorSetLayout value, const VkAllocationCallbacks *a)
{ assert((uintptr_t)value == SET_LAYOUT && live == (SAMPLER|SET_LAYOUT)); DESTROY_START(SET_LAYOUT); }
static VKAPI_ATTR VkResult VKAPI_CALL layout_create(VkDevice d, const VkPipelineLayoutCreateInfo *info,
      const VkAllocationCallbacks *a, VkPipelineLayout *out)
{
   CREATE_START(LAYOUT);
   assert(info->sType == VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO && !info->pNext
         && info->setLayoutCount == 1 && info->pSetLayouts[0] == (VkDescriptorSetLayout)(uintptr_t)SET_LAYOUT
         && info->pushConstantRangeCount == 1 && info->pPushConstantRanges[0].offset == 0
         && info->pPushConstantRanges[0].size == 80 && info->pPushConstantRanges[0].stageFlags == VK_SHADER_STAGE_COMPUTE_BIT);
   *out = (VkPipelineLayout)(uintptr_t)LAYOUT; return VK_SUCCESS;
}
static VKAPI_ATTR void VKAPI_CALL layout_destroy(VkDevice d, VkPipelineLayout value, const VkAllocationCallbacks *a)
{ assert((uintptr_t)value == LAYOUT && live == (SAMPLER|SET_LAYOUT|LAYOUT)); DESTROY_START(LAYOUT); }
static VKAPI_ATTR VkResult VKAPI_CALL pool_create(VkDevice d, const VkDescriptorPoolCreateInfo *info,
      const VkAllocationCallbacks *a, VkDescriptorPool *out)
{
   CREATE_START(POOL);
   assert(info->sType == VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO && !info->pNext && info->maxSets == count
         && info->poolSizeCount == 2 && info->pPoolSizes[0].type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
         && info->pPoolSizes[0].descriptorCount == 2*count && info->pPoolSizes[1].type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE
         && info->pPoolSizes[1].descriptorCount == count);
   *out = (VkDescriptorPool)(uintptr_t)POOL; return VK_SUCCESS;
}
static VKAPI_ATTR void VKAPI_CALL pool_destroy(VkDevice d, VkDescriptorPool value, const VkAllocationCallbacks *a)
{ assert((uintptr_t)value == POOL && !(live & (PIPELINE|COMMAND_POOL|SHADER))); live &= ~SETS; DESTROY_START(POOL); }
static VKAPI_ATTR VkResult VKAPI_CALL sets_allocate(VkDevice d, const VkDescriptorSetAllocateInfo *info, VkDescriptorSet *out)
{
   unsigned i;
   assert(d == device && info->sType == VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO && !info->pNext
         && info->descriptorPool == (VkDescriptorPool)(uintptr_t)POOL && info->descriptorSetCount == count);
   if (fail()) return VK_ERROR_OUT_OF_POOL_MEMORY;
   live |= SETS;
   for (i = 0; i < count; i++)
   { assert(info->pSetLayouts[i] == (VkDescriptorSetLayout)(uintptr_t)SET_LAYOUT); out[i] = (VkDescriptorSet)(uintptr_t)(1000+i); }
   return VK_SUCCESS;
}
static VKAPI_ATTR VkResult VKAPI_CALL shader_create(VkDevice d, const VkShaderModuleCreateInfo *info,
      const VkAllocationCallbacks *a, VkShaderModule *out)
{
   CREATE_START(SHADER);
   assert(info->sType == VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO && !info->pNext
         && info->codeSize >= 20 && !(info->codeSize & 3) && info->pCode[0] == 0x07230203);
   *out = (VkShaderModule)(uintptr_t)SHADER; return VK_SUCCESS;
}
static VKAPI_ATTR void VKAPI_CALL shader_destroy(VkDevice d, VkShaderModule value, const VkAllocationCallbacks *a)
{ assert((uintptr_t)value == SHADER); DESTROY_START(SHADER); }
static VKAPI_ATTR VkResult VKAPI_CALL pipeline_create(VkDevice d, VkPipelineCache cache, uint32_t n,
      const VkComputePipelineCreateInfo *info, const VkAllocationCallbacks *a, VkPipeline *out)
{
   CREATE_START(PIPELINE);
   assert(!cache && n == 1 && info->sType == VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO && !info->pNext
         && info->layout == (VkPipelineLayout)(uintptr_t)LAYOUT && info->stage.sType == VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO
         && info->stage.module == (VkShaderModule)(uintptr_t)SHADER && info->stage.stage == VK_SHADER_STAGE_COMPUTE_BIT
         && !strcmp(info->stage.pName, "main"));
   *out = (VkPipeline)(uintptr_t)PIPELINE; return VK_SUCCESS;
}
static VKAPI_ATTR void VKAPI_CALL pipeline_destroy(VkDevice d, VkPipeline value, const VkAllocationCallbacks *a)
{ assert((uintptr_t)value == PIPELINE && !(live & (SHADER|COMMAND_POOL))); DESTROY_START(PIPELINE); }
static VKAPI_ATTR VkResult VKAPI_CALL commands_create(VkDevice d, const VkCommandPoolCreateInfo *info,
      const VkAllocationCallbacks *a, VkCommandPool *out)
{
   CREATE_START(COMMAND_POOL);
   assert(info->sType == VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO && !info->pNext
         && info->flags == VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT && info->queueFamilyIndex == 2);
   *out = (VkCommandPool)(uintptr_t)COMMAND_POOL; return VK_SUCCESS;
}
static VKAPI_ATTR void VKAPI_CALL commands_destroy(VkDevice d, VkCommandPool value, const VkAllocationCallbacks *a)
{ assert((uintptr_t)value == COMMAND_POOL && !fences); live &= ~COMMANDS; DESTROY_START(COMMAND_POOL); }
static VKAPI_ATTR VkResult VKAPI_CALL commands_allocate(VkDevice d, const VkCommandBufferAllocateInfo *info, VkCommandBuffer *out)
{
   unsigned i;
   assert(d == device && info->sType == VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO && !info->pNext
         && info->commandPool == (VkCommandPool)(uintptr_t)COMMAND_POOL
         && info->level == VK_COMMAND_BUFFER_LEVEL_PRIMARY && info->commandBufferCount == count);
   if (fail()) return VK_ERROR_OUT_OF_DEVICE_MEMORY;
   live |= COMMANDS;
   for (i = 0; i < count; i++) out[i] = (VkCommandBuffer)(uintptr_t)(2000+i);
   return VK_SUCCESS;
}
static VKAPI_ATTR VkResult VKAPI_CALL fence_create(VkDevice d, const VkFenceCreateInfo *info,
      const VkAllocationCallbacks *a, VkFence *out)
{
   assert(d == device && !a && info->sType == VK_STRUCTURE_TYPE_FENCE_CREATE_INFO && !info->pNext
         && info->flags == VK_FENCE_CREATE_SIGNALED_BIT && next_fence < count);
   if (fail()) return VK_ERROR_OUT_OF_HOST_MEMORY;
   fences |= (uint32_t)1 << next_fence; *out = (VkFence)(uintptr_t)(3000+next_fence++); return VK_SUCCESS;
}
static VKAPI_ATTR void VKAPI_CALL fence_destroy(VkDevice d, VkFence value, const VkAllocationCallbacks *a)
{
   unsigned i = (unsigned)(uintptr_t)value-3000;
   assert(d == device && !a && i < count && (fences & ((uint32_t)1 << i)) && (live & COMMAND_POOL));
   fences &= ~((uint32_t)1 << i); destroyed++;
}
static const vaapi_hevc_vulkan_resource_ops_t ops = {
   sampler_create, sampler_destroy, set_create, set_destroy, layout_create, layout_destroy,
   pool_create, pool_destroy, sets_allocate, shader_create, shader_destroy, pipeline_create,
   pipeline_destroy, commands_create, commands_destroy, commands_allocate, fence_create, fence_destroy
};
int main(void)
{
   unsigned fp16, f, calls_before;
   device = (VkDevice)(uintptr_t)1;
   for (fp16 = 0; fp16 < 2; fp16++) for (count = 1; count <= 32; count += 31)
   {
      for (f = 1; f <= 9+count; f++)
      {
         calls = next_fence = destroyed = 0; fault = f;
         assert(!vaapi_hevc_vulkan_resources_create(&resources, device, 2, count, fp16 != 0, &ops));
         assert(!live && !fences && !resources.device && !resources.count);
      }
      calls = next_fence = destroyed = fault = 0;
      resources.ops = ops;
      assert(vaapi_hevc_vulkan_resources_create(&resources, device, 2, count, fp16 != 0, &resources.ops));
      assert(resources.count == count && calls == 9+count && next_fence == count && !(live & SHADER));
      assert(resources.format == (fp16 ? VK_FORMAT_R16G16B16A16_SFLOAT : VK_FORMAT_R8G8B8A8_UNORM));
      before = resources; calls_before = calls;
      assert(!vaapi_hevc_vulkan_resources_create(&resources, device, 2, count, fp16 != 0, &ops));
      assert(calls == calls_before && !memcmp(&resources, &before, sizeof(resources)));
      vaapi_hevc_vulkan_resources_destroy(&resources);
      assert(!live && !fences && !resources.device);
      vaapi_hevc_vulkan_resources_destroy(&resources);
      calls = next_fence = destroyed = fault = 0;
      assert(vaapi_hevc_vulkan_resources_create(&resources, device, 2, count, fp16 != 0, &ops));
      assert(!vaapi_hevc_vulkan_resources_after_device_destroy(&resources, (VkDevice)(uintptr_t)2));
      calls_before = destroyed; live = fences = 0;
      assert(vaapi_hevc_vulkan_resources_after_device_destroy(&resources, device) && destroyed == calls_before && !resources.device);
   }
   assert(!vaapi_hevc_vulkan_resources_create(&resources, device, 2, 0, false, &ops));
   assert(!vaapi_hevc_vulkan_resources_create(&resources, device, 2, 33, false, &ops));
   assert(!vaapi_hevc_vulkan_resources_create(&resources, device, VK_QUEUE_FAMILY_FOREIGN_EXT, 1, false, &ops));
   resources.ops = ops; resources.ops.destroy_fence = NULL; before = resources;
   assert(!vaapi_hevc_vulkan_resources_create(&resources, device, 2, 1, false, &resources.ops)
         && !memcmp(&resources, &before, sizeof(resources)));
   puts("PASS: cold 1/32-slot VA Vulkan resources, every creation failure, owned teardown order and exact-device invalidation");
   return 0;
}
