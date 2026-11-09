/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RARCH_VULKAN_ENCODE_CAPTURE_H
#define RARCH_VULKAN_ENCODE_CAPTURE_H
#include "vulkan_encode_pool.h"
RETRO_BEGIN_DECLS
typedef struct vulkan_encode_capture vulkan_encode_capture_t;
typedef struct vulkan_encode_capture_ops
{
   PFN_vkCreateDescriptorSetLayout create_set_layout;
   PFN_vkDestroyDescriptorSetLayout destroy_set_layout;
   PFN_vkCreateDescriptorPool create_descriptor_pool;
   PFN_vkDestroyDescriptorPool destroy_descriptor_pool;
   PFN_vkAllocateDescriptorSets allocate_sets;
   PFN_vkUpdateDescriptorSets update_sets;
   PFN_vkCreateSampler create_sampler;
   PFN_vkDestroySampler destroy_sampler;
   PFN_vkCreatePipelineLayout create_pipeline_layout;
   PFN_vkDestroyPipelineLayout destroy_pipeline_layout;
   PFN_vkCreateShaderModule create_shader;
   PFN_vkDestroyShaderModule destroy_shader;
   PFN_vkCreateComputePipelines create_pipelines;
   PFN_vkDestroyPipeline destroy_pipeline;
   PFN_vkCmdPipelineBarrier2 barrier;
   PFN_vkCmdBindPipeline bind_pipeline;
   PFN_vkCmdBindDescriptorSets bind_sets;
   PFN_vkCmdPushConstants push;
   PFN_vkCmdDispatch dispatch;
   PFN_vkCmdCopyBufferToImage copy;
} vulkan_encode_capture_ops_t;
typedef struct vulkan_encode_capture_config
{
   const vulkan_encode_pool_t *pool;
   const uint32_t *code;
   size_t code_bytes;
} vulkan_encode_capture_config_t;
typedef struct vulkan_encode_capture_source
{
   VkImage image;
   VkImageView view;
   VkFormat format;
   VkImageLayout layout;
   VkPipelineStageFlags2 stage;
   VkAccessFlags2 access;
   unsigned width, height, crop_x, crop_y, valid_width, valid_height;
   bool limited_range, flip_y;
} vulkan_encode_capture_source_t;
/* Cold worker startup. Shader matches the pool's direct-plane or packed
 * buffer mode. Code is borrowed only during construction; pool/device/ops
 * through free. Packed NV12 requires a coded width divisible by four.
 * Shader: source sampler at0; direct Y/UV at1/2 or packed NV12 buffer at1.
 * Workgroup8x8, each invocation covers2x2(direct) or4x2(buffer) pixels.
 * Push words: coded width/height,crop x/y,valid width/height,limited,flip. */
vulkan_encode_capture_t *vulkan_encode_capture_new(
      const vulkan_encode_capture_config_t *config,
      const vulkan_encode_capture_ops_t *ops);
/* Cold startup with the in-tree RSLC_EXTENDED compiler. No generated shader
 * binary asset; source text and compiler output are released before return. */
vulkan_encode_capture_t *vulkan_encode_capture_new_compiled(
      const vulkan_encode_pool_t *pool, const vulkan_encode_capture_ops_t *ops,
      char *error, size_t capacity);
/* One producer, outside a render pass, on its existing command buffer.
 * The caller owns a free pool slot, waits for its previous done value on the
 * GPU, then signals ready after submission. Source image/view and this
 * component survive producer completion. Source is sampled SDR UNORM with
 * BT.709 primaries. Encoded transfer is preserved and tagged by the encoder.
 * Source is restored to its supplied layout/stage/access.
 * No submission, allocation, polling or host readback. False records nothing.
 * initialized describes whether this slot's image already reached GENERAL. */
bool vulkan_encode_capture_record(vulkan_encode_capture_t *capture,
      unsigned slot, VkCommandBuffer command,
      const vulkan_encode_capture_source_t *source, bool initialized);
/* After all producer commands and source borrows retire. */
void vulkan_encode_capture_free(vulkan_encode_capture_t *capture);
RETRO_END_DECLS
#endif
