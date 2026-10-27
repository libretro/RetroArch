/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#ifndef VULKAN_ANDROID_SUBMIT_H
#define VULKAN_ANDROID_SUBMIT_H
#include "vulkan_android_image.h"
#include "vulkan_android_pipeline.h"
#include "vulkan_android_sync.h"
#include "android_frame.h"
RETRO_BEGIN_DECLS
typedef struct vulkan_android_submit_ops
{
   PFN_vkGetFenceStatus fence_status;
   PFN_vkResetFences reset_fences;
   PFN_vkResetCommandBuffer reset_command;
   PFN_vkBeginCommandBuffer begin;
   PFN_vkEndCommandBuffer end;
   PFN_vkCmdPipelineBarrier barrier;
   PFN_vkCmdBindPipeline bind_pipeline;
   PFN_vkCmdBindDescriptorSets bind_sets;
   PFN_vkCmdPushConstants push;
   PFN_vkCmdDispatch dispatch;
   /* SUCCESS means submitted; NOT_READY means queued for the renderer's
    * existing submission lock. TIMEOUT refuses without submitting or retaining
    * pointers. A queued submission retains its frame until fence completion.
    * The callback must copy submit data before returning NOT_READY. */
   VkResult (*submit)(void *context, VkQueue queue, uint32_t count,
         const VkSubmitInfo *submits, VkFence fence);
   void *submit_context;
} vulkan_android_submit_ops_t;
typedef struct vulkan_android_submission
{
   VkDevice device;
   VkQueue queue;
   unsigned family;
   VkCommandBuffer command;
   VkFence fence;
   vulkan_android_image_t image;
   vulkan_android_pipeline_t pipeline;
   vulkan_android_sync_t sync;
   vulkan_android_pipeline_ops_t pipeline_ops;
   vulkan_android_sync_ops_t sync_ops;
   vulkan_android_submit_ops_t ops;
   /* Cold-prepared native buffer identity and immutable colour/crop contract.
    */
   android_frame_view_t source;
   uint32_t header[20];
   void *held_owner;
   uint64_t held_slot, held_generation;
   int inflight, broken;
} vulkan_android_submission_t;
/* Cold metadata preparation for this cached entry. Resources/command/fence and
 * operation tables are already populated. Does not take source's image ticket.
 * Supports BT.709/SMPTE 170M SDR and BT.709/2020 PQ/HLG. */
bool vulkan_android_submission_configure(vulkan_android_submission_t *entry,
      const android_frame_view_t *source, bool fp16);
/* Renderer-owner zero-timeout query. 1 reusable; 0 pending; -1 sticky failure.
 * If SYNC_FD export failed, returns the retained image only after fence proof.
 * Never destroy entry/resources on pending/error without cold device
 * retirement. */
int vulkan_android_submission_poll(vulkan_android_submission_t *entry);
/* Cold shutdown ONLY AFTER vkDestroyDevice has returned for entry's device.
 * No Vulkan call is made. Lost-device work can no longer read the source, so
 * return the retained ticket (its original ready fence still belongs to the
 * image queue). Clear invalidated GPU handles; entry cannot be reused. */
bool vulkan_android_submission_after_device_destroy(
      vulkan_android_submission_t *entry);
/* Uses a warm entry, never creates resources. 1 consumes the frame; 0 BUSY or
 * -1 refusal take nothing; -2 fatal after possible GPU submission consumes and
 * retains the frame in the broken entry. Destination lifetime extends through
 * entry fence completion. Layout must match its tracked current Vulkan layout.
 * Destination ends SHADER_READ_ONLY for normal fragment presentation. */
int vulkan_android_submission_present(vulkan_android_submission_t *entry,
      const video_hw_frame_t *frame, VkImage destination, VkImageView view,
      VkImageLayout layout);
RETRO_END_DECLS
#endif
