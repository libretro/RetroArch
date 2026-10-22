/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RARCH_VAAPI_HEVC_VULKAN_SUBMIT_H
#define RARCH_VAAPI_HEVC_VULKAN_SUBMIT_H
#include "vaapi_hevc_vulkan_frame.h"
#include "vulkan_video_import.h"
RETRO_BEGIN_DECLS

typedef struct vaapi_hevc_vulkan_submit_ops
{
   PFN_vkGetFenceStatus fence_status;
   PFN_vkResetFences reset_fences;
   PFN_vkResetCommandBuffer reset_command;
   PFN_vkBeginCommandBuffer begin;
   PFN_vkEndCommandBuffer end;
   PFN_vkUpdateDescriptorSets update;
   PFN_vkCmdPipelineBarrier barrier;
   PFN_vkCmdBindPipeline bind_pipeline;
   PFN_vkCmdBindDescriptorSets bind_sets;
   PFN_vkCmdPushConstants push;
   PFN_vkCmdDispatch dispatch;
   /* Existing renderer queue serialization, never direct shared-queue access.
    */
   VkResult (*submit)(void *, VkQueue, uint32_t, const VkSubmitInfo *, VkFence);
   void *context;
} vaapi_hevc_vulkan_submit_ops_t;

typedef struct vaapi_hevc_vulkan_submission
{
   vaapi_hevc_vulkan_frame_t *frame;
   vaapi_hevc_pool_t *pool;
   vaapi_hevc_vulkan_submit_ops_t ops;
   uint64_t ticket;
   VkDevice device;
   VkQueue queue;
   VkCommandBuffer command;
   VkFence fence;
   VkPipeline pipeline;
   VkPipelineLayout layout;
   VkDescriptorSet set;
   VkSampler sampler;
   VkFormat output_format;
   uint32_t header[20];
   unsigned slot, family;
} vaapi_hevc_vulkan_submission_t;

/* Cold immutable crop/color preparation. Borrow the renderer's sampled-YUV
 * pipeline (rgba8 or rgba16f as selected), 80-byte layout, nearest sampler,
 * unique descriptor set and command/fence. These outlive all pending use.
 * Does not move or release frame's source. */
bool vaapi_hevc_vulkan_submission_configure(
      vaapi_hevc_vulkan_submission_t *entry, vaapi_hevc_vulkan_frame_t *frame,
      unsigned x, unsigned y, unsigned width, unsigned height, unsigned yuv,
      const video_color_metadata_t *color, bool fp16);
/* Renderer owner, warm resources only. Destination is a matching
 * SAMPLED|STORAGE image and view; layout is its tracked current layout. Ends
 * SHADER_READ_ONLY after conversion. 1 queued, 0 busy, -1 refused before GPU
 * submission, -2 uncertain submission failure: source/destination must survive
 * exact device retirement. Poll/retire through frame helpers and the existing
 * decoder-owner return path. */
int vaapi_hevc_vulkan_submission_present(vaapi_hevc_vulkan_submission_t *entry,
      VkImage destination, VkImageView view, VkFormat format, unsigned width,
      unsigned height, VkImageLayout layout);
RETRO_END_DECLS
#endif
