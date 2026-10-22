/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#ifndef VULKAN_ANDROID_PIPELINE_H
#define VULKAN_ANDROID_PIPELINE_H
#include <vulkan/vulkan_core.h>
#include <boolean.h>
#include <retro_common_api.h>
RETRO_BEGIN_DECLS
typedef struct vulkan_android_pipeline_ops
{
   PFN_vkCreateDescriptorSetLayout create_set_layout;
   PFN_vkDestroyDescriptorSetLayout destroy_set_layout;
   PFN_vkCreatePipelineLayout create_layout;
   PFN_vkDestroyPipelineLayout destroy_layout;
   PFN_vkCreateDescriptorPool create_pool;
   PFN_vkDestroyDescriptorPool destroy_pool;
   PFN_vkAllocateDescriptorSets allocate_sets;
   PFN_vkUpdateDescriptorSets update_sets;
   PFN_vkCreateShaderModule create_shader;
   PFN_vkDestroyShaderModule destroy_shader;
   PFN_vkCreateComputePipelines create_pipeline;
   PFN_vkDestroyPipeline destroy_pipeline;
} vulkan_android_pipeline_ops_t;
typedef struct vulkan_android_pipeline
{
   VkDescriptorSetLayout set_layout;
   VkPipelineLayout layout;
   VkDescriptorPool pool;
   VkPipeline pipeline;
   VkDescriptorSet sets[32];
   unsigned count;
} vulkan_android_pipeline_t;
/* Cold cache construction. sampler must outlive this pipeline/layout/pool and
 * match source views' YCbCr conversion. sampler_descriptors reserves the
 * external-format pool budget, using the maintenance6 maximum when available.
 * Native allocation must succeed, including for legacy reservations. Shader is
 * external_rgb_to_rgba*, with output storage format matching destination.
 * Failure unwinds partial work. */
bool vulkan_android_pipeline_create(VkDevice device,
      const vulkan_android_pipeline_ops_t *ops, VkSampler sampler,
      unsigned sampler_descriptors, unsigned slots, const uint32_t *shader,
      size_t shader_bytes, vulkan_android_pipeline_t *out);
/* Renderer owner, only when this set has no pending GPU use. Native source view
 * must use the immutable sampler's conversion. No allocation/submission/wait.
 */
bool vulkan_android_pipeline_update(VkDevice device,
      const vulkan_android_pipeline_ops_t *ops,
      const vulkan_android_pipeline_t *pipeline, unsigned slot,
      VkImageView source, VkImageView destination);
/* Cold owner after every GPU/pipeline/descriptor reference has retired. */
void vulkan_android_pipeline_destroy(VkDevice device,
      const vulkan_android_pipeline_ops_t *ops,
      vulkan_android_pipeline_t *pipeline);
RETRO_END_DECLS
#endif
