/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RECORD_GPU_RENDERER_VULKAN_H
#define RECORD_GPU_RENDERER_VULKAN_H
#include "record_gpu_dispatch_vulkan.h"
RETRO_BEGIN_DECLS
typedef struct record_gpu_renderer_vulkan record_gpu_renderer_vulkan_t;
enum record_gpu_renderer_vulkan_result
{
   RECORD_VKR_DEVICE_LOST = -2
};
/* Cold construction. Borrows the job through successful finish. All other
 * calls belong to one producer; transfer only after its calls quiesce. */
record_gpu_renderer_vulkan_t *record_gpu_renderer_vulkan_new(
      record_gpu_dispatch_vulkan_t *job, unsigned duration, unsigned gop,
      PFN_vkGetSemaphoreCounterValue counter);
/* Outside a render pass, at most one recorded batch. Source and command stay
 * alive through GPU retirement. BUSY takes nothing; false capture records
 * nothing. Timestamps are monotonic in the encoder's timescale. */
int record_gpu_renderer_vulkan_record(record_gpu_renderer_vulkan_t *renderer,
      VkCommandBuffer command, const vulkan_encode_capture_source_t *source,
      uint64_t timestamp);
/* Append ready/done timeline dependencies to the renderer's existing batch.
 * Caller arrays/chains remain untouched. Borrowed result lasts until the next
 * record/submission call. NULL requires command reset and discard, not submit.
 * Existing timeline nodes are accepted only at the head of the chain.
 * Device-group, D3D12-fence and protected submissions are refused. */
const VkSubmitInfo *record_gpu_renderer_vulkan_submission(
      record_gpu_renderer_vulkan_t *renderer, const VkSubmitInfo *submit);
/* Exactly once after native submission. Failure retains uncertain ownership;
 * success publishes only after the ready signal was accepted by the queue. */
bool record_gpu_renderer_vulkan_submitted(record_gpu_renderer_vulkan_t *renderer,
      VkResult result);
/* Only after resetting/discarding the unsubmitted command, never after a
 * submission attempt. Releases a reservation without inventing GPU proof. */
bool record_gpu_renderer_vulkan_discard(record_gpu_renderer_vulkan_t *renderer);
/* Stop producer admission. Zero-timeout query proves successful submissions
 * retired before closing the job's video producer. quiesced additionally
 * promises all recorded work is permanently discarded or GPU-complete, and
 * no submission can execute later. Native errors alone are not that proof. */
int record_gpu_renderer_vulkan_finish(record_gpu_renderer_vulkan_t *renderer,
      bool quiesced);
/* Terminal cleanup only after this device reported VK_ERROR_DEVICE_LOST.
 * Cancels output, never treats undefined GPU contents as a completed frame.
 * Vulkan Lost Device permits freeing objects no longer considered in-use. */
bool record_gpu_renderer_vulkan_abandon_lost(record_gpu_renderer_vulkan_t *renderer,
      VkResult native_result);
/* Refuses before successful finish; does not collect or destroy the job. */
bool record_gpu_renderer_vulkan_free(record_gpu_renderer_vulkan_t *renderer);
RETRO_END_DECLS
#endif
