/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RARCH_VAAPI_HEVC_VULKAN_RESOURCES_H
#define RARCH_VAAPI_HEVC_VULKAN_RESOURCES_H
#include <vulkan/vulkan_core.h>
#include <boolean.h>
#include <retro_common_api.h>
RETRO_BEGIN_DECLS

typedef struct vaapi_hevc_vulkan_resource_ops
{
   PFN_vkCreateSampler create_sampler;
   PFN_vkDestroySampler destroy_sampler;
   PFN_vkCreateDescriptorSetLayout create_set_layout;
   PFN_vkDestroyDescriptorSetLayout destroy_set_layout;
   PFN_vkCreatePipelineLayout create_layout;
   PFN_vkDestroyPipelineLayout destroy_layout;
   PFN_vkCreateDescriptorPool create_pool;
   PFN_vkDestroyDescriptorPool destroy_pool;
   PFN_vkAllocateDescriptorSets allocate_sets;
   PFN_vkCreateShaderModule create_shader;
   PFN_vkDestroyShaderModule destroy_shader;
   PFN_vkCreateComputePipelines create_pipeline;
   PFN_vkDestroyPipeline destroy_pipeline;
   PFN_vkCreateCommandPool create_commands;
   PFN_vkDestroyCommandPool destroy_commands;
   PFN_vkAllocateCommandBuffers allocate_commands;
   PFN_vkCreateFence create_fence;
   PFN_vkDestroyFence destroy_fence;
} vaapi_hevc_vulkan_resource_ops_t;

typedef struct vaapi_hevc_vulkan_resources
{
   vaapi_hevc_vulkan_resource_ops_t ops;
   VkDevice device;
   VkSampler sampler;
   VkDescriptorSetLayout set_layout;
   VkPipelineLayout layout;
   VkDescriptorPool pool;
   VkPipeline pipeline;
   VkCommandPool command_pool;
   VkDescriptorSet sets[32];
   VkCommandBuffer commands[32];
   VkFence fences[32];
   VkFormat format;
   unsigned count;
} vaapi_hevc_vulkan_resources_t;

/* Cold renderer setup, zeroed storage; 1..32 independent descriptor/command/
 * fence slots sharing the existing sampled-YUV shader variant. Owns every
 * created Vulkan child. Refusal unwinds partial resources; no GPU submission.
 */
bool vaapi_hevc_vulkan_resources_create(vaapi_hevc_vulkan_resources_t *out,
      VkDevice device, unsigned family, unsigned count, bool fp16,
      const vaapi_hevc_vulkan_resource_ops_t *ops);
/* Cold renderer owner AFTER all command/descriptor/GPU use has retired.
 * Does not poll or wait. Destroy before imported source views or destination.
 */
void vaapi_hevc_vulkan_resources_destroy(
      vaapi_hevc_vulkan_resources_t *resources);
/* Only AFTER destruction of the exact device. No Vulkan calls. */
bool vaapi_hevc_vulkan_resources_after_device_destroy(
      vaapi_hevc_vulkan_resources_t *resources, VkDevice device);
RETRO_END_DECLS
#endif
