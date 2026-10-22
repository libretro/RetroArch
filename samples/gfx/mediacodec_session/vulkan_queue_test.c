/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../../gfx/common/vulkan_android_queue.h"
#define H(t, n) ((t)(uintptr_t)(n))
static vulkan_android_queue_t pending;
static unsigned calls, fail_at;
static int locked;
static VKAPI_ATTR VkResult VKAPI_CALL submit(VkQueue queue, uint32_t count,
      const VkSubmitInfo *info, VkFence fence)
{
   assert(locked && queue == H(VkQueue, 1) && count == 1);
   assert(fence == H(VkFence, 5));
   assert(*info->pCommandBuffers == H(VkCommandBuffer, 2));
   assert(*info->pWaitSemaphores == H(VkSemaphore, 3));
   assert(*info->pSignalSemaphores == H(VkSemaphore, 4));
   assert(*info->pWaitDstStageMask == VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
   calls++;
   return calls == fail_at ? VK_ERROR_OUT_OF_DEVICE_MEMORY : VK_SUCCESS;
}
static VkResult enqueue(void)
{
   VkSubmitInfo info;
   VkCommandBuffer command = H(VkCommandBuffer, 2);
   VkSemaphore ready = H(VkSemaphore, 3), done = H(VkSemaphore, 4);
   VkPipelineStageFlags stage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
   memset(&info, 0, sizeof(info));
   info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
   info.waitSemaphoreCount = info.commandBufferCount
         = info.signalSemaphoreCount = 1;
   info.pCommandBuffers = &command;
   info.pWaitSemaphores = &ready;
   info.pSignalSemaphores = &done;
   info.pWaitDstStageMask = &stage;
   return vulkan_android_queue_push(&pending, H(VkQueue, 1), 1, &info,
         H(VkFence, 5));
}
int main(void)
{
   unsigned i;
   for (i = 0; i < VULKAN_ANDROID_QUEUE_CAPACITY; i++)
      assert(enqueue() == VK_NOT_READY && !calls);
   assert(enqueue() == VK_TIMEOUT && !calls);
   locked = 1;
   assert(vulkan_android_queue_flush(&pending, submit) == VK_SUCCESS);
   assert(calls == VULKAN_ANDROID_QUEUE_CAPACITY && !pending.count);
   assert(vulkan_android_queue_flush(&pending, submit) == VK_SUCCESS);
   locked = 0;
   assert(enqueue() == VK_NOT_READY);
   assert(enqueue() == VK_NOT_READY);
   fail_at = calls + 2;
   locked = 1;
   assert(vulkan_android_queue_flush(&pending, submit)
         == VK_ERROR_OUT_OF_DEVICE_MEMORY);
   i = calls;
   assert(pending.count == 2);
   assert(vulkan_android_queue_flush(&pending, submit)
         == VK_ERROR_OUT_OF_DEVICE_MEMORY);
   assert(enqueue() == VK_ERROR_OUT_OF_DEVICE_MEMORY && calls == i);
   puts("PASS: copied bounded submissions, owner-lock drain and sticky failure");
   return 0;
}
