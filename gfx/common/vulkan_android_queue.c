/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include "vulkan_android_queue.h"
#include <string.h>

VkResult vulkan_android_queue_push(void *context, VkQueue queue,
      uint32_t count, const VkSubmitInfo *submits, VkFence fence)
{
   vulkan_android_queue_t *pending = (vulkan_android_queue_t *)context;
   vulkan_android_queued_submit_t *entry;
   if (!pending || !queue || !fence || count != 1 || !submits
         || submits->sType != VK_STRUCTURE_TYPE_SUBMIT_INFO || submits->pNext
         || submits->waitSemaphoreCount != 1 || !submits->pWaitSemaphores
         || !submits->pWaitDstStageMask || submits->commandBufferCount != 1
         || !submits->pCommandBuffers || submits->signalSemaphoreCount != 1
         || !submits->pSignalSemaphores || !*submits->pWaitSemaphores
         || !*submits->pCommandBuffers || !*submits->pSignalSemaphores
         || !*submits->pWaitDstStageMask)
      return VK_TIMEOUT;
   if (pending->failure != VK_SUCCESS)
      return pending->failure;
   if ((pending->queue && pending->queue != queue)
         || pending->count >= VULKAN_ANDROID_QUEUE_CAPACITY)
      return VK_TIMEOUT;
   entry = &pending->entries[pending->count++];
   entry->command = *submits->pCommandBuffers;
   entry->ready = *submits->pWaitSemaphores;
   entry->done = *submits->pSignalSemaphores;
   entry->stage = *submits->pWaitDstStageMask;
   entry->fence = fence;
   pending->queue = queue;
   return VK_NOT_READY;
}

VkResult vulkan_android_queue_flush(vulkan_android_queue_t *pending,
      PFN_vkQueueSubmit submit)
{
   unsigned i;
   VkSubmitInfo info;
   if (!pending || !submit)
      return VK_ERROR_INITIALIZATION_FAILED;
   if (pending->failure != VK_SUCCESS)
      return pending->failure;
   memset(&info, 0, sizeof(info));
   info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
   info.waitSemaphoreCount = info.commandBufferCount
         = info.signalSemaphoreCount = 1;
   for (i = 0; i < pending->count; i++)
   {
      vulkan_android_queued_submit_t *entry = &pending->entries[i];
      VkResult result;
      info.pCommandBuffers = &entry->command;
      info.pWaitSemaphores = &entry->ready;
      info.pSignalSemaphores = &entry->done;
      info.pWaitDstStageMask = &entry->stage;
      result = submit(pending->queue, 1, &info, entry->fence);
      if (result != VK_SUCCESS)
      {
         pending->failure = result < 0 ? result : VK_ERROR_DEVICE_LOST;
         return pending->failure;
      }
   }
   pending->count = 0;
   return VK_SUCCESS;
}
