/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#ifndef VULKAN_ANDROID_QUEUE_H
#define VULKAN_ANDROID_QUEUE_H
#include <vulkan/vulkan_core.h>
#include <retro_common_api.h>
RETRO_BEGIN_DECLS
#define VULKAN_ANDROID_QUEUE_CAPACITY 32

typedef struct vulkan_android_queued_submit
{
   VkCommandBuffer command;
   VkSemaphore ready, done;
   VkFence fence;
   VkPipelineStageFlags stage;
} vulkan_android_queued_submit_t;

typedef struct vulkan_android_queue
{
   VkQueue queue;
   vulkan_android_queued_submit_t entries[VULKAN_ANDROID_QUEUE_CAPACITY];
   unsigned count;
   VkResult failure;
} vulkan_android_queue_t;

/* Renderer owner only. One command, optional single wait/signal semaphore.
 * Zero-initialize before use. Copies all submit data.
 * NOT_READY accepts; TIMEOUT leaves ownership with caller; errors are sticky.
 */
VkResult vulkan_android_queue_push(void *context, VkQueue queue, uint32_t count,
      const VkSubmitInfo *submits, VkFence fence);
/* Caller already holds its existing queue serialization. Failed work stays
 * retained until cold device destruction; never retry an ambiguous failure. */
VkResult vulkan_android_queue_flush(
      vulkan_android_queue_t *queue, PFN_vkQueueSubmit submit);
RETRO_END_DECLS
#endif
