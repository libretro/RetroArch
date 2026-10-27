/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include "record_android_capture_submit.h"
#include <stdlib.h>
#include <string.h>

struct record_android_capture_submit
{
   record_android_capture_submit_ops_t ops;
   VkDevice device;
   VkQueue queue;
   VkImage image;
   VkCommandPool pool;
   VkCommandBuffer command;
   VkFence fence;
   VkSemaphore done;
   void *owner;
   void (*release_source)(void *, bool);
   VkFormat format;
   unsigned width, height, family;
   int ready_fd;
   bool initialized, inflight, waiting, exported, taken, broken, gpu_retired;
   bool recorded, committed, completion_pending;
};

static void record_acs_destroy_gpu(record_android_capture_submit_t *e)
{
   if (e->done)
      e->ops.destroy_semaphore(e->device, e->done, NULL);
   if (e->fence)
      e->ops.destroy_fence(e->device, e->fence, NULL);
   if (e->pool)
      e->ops.destroy_pool(e->device, e->pool, NULL);
   e->pool = VK_NULL_HANDLE;
   e->command = VK_NULL_HANDLE;
   e->fence = VK_NULL_HANDLE;
   e->done = VK_NULL_HANDLE;
   e->gpu_retired = true;
}

bool record_android_capture_submit_free(record_android_capture_submit_t *e)
{
   if (!e)
      return true;
   if (e->inflight || e->waiting || e->owner || e->ready_fd >= 0)
      return false;
   record_acs_destroy_gpu(e);
   free(e);
   return true;
}

record_android_capture_submit_t *record_android_capture_submit_new(
      VkDevice device, VkQueue queue, unsigned family, VkImage image,
      VkFormat format, unsigned width, unsigned height,
      const record_android_capture_submit_ops_t *ops)
{
   record_android_capture_submit_t *e;
   VkCommandPoolCreateInfo pool;
   VkCommandBufferAllocateInfo commands;
   VkFenceCreateInfo fence;
   VkSemaphoreCreateInfo semaphore;
   VkExportSemaphoreCreateInfo export;
   if (!device || !queue || !image || family >= VK_QUEUE_FAMILY_FOREIGN_EXT
         || !width || !height || width > 16384 || height > 16384
         || (format != VK_FORMAT_R8G8B8A8_UNORM
               && format != VK_FORMAT_A2B10G10R10_UNORM_PACK32
               && format != VK_FORMAT_R16G16B16A16_SFLOAT)
         || !ops || !ops->create_pool || !ops->destroy_pool
         || !ops->allocate_commands || !ops->create_fence || !ops->destroy_fence
         || !ops->create_semaphore || !ops->destroy_semaphore
         || !ops->reset_command || !ops->begin || !ops->end || !ops->barrier
         || !ops->copy || !ops->reset_fences || !ops->fence_status
         || !ops->export_fd || !ops->submit || !ops->close_fd)
      return NULL;
   e = (record_android_capture_submit_t *)calloc(1, sizeof(*e));
   if (!e)
      return NULL;
   e->ops = *ops;
   e->device = device;
   e->queue = queue;
   e->image = image;
   e->format = format;
   e->width = width;
   e->height = height;
   e->family = family;
   e->ready_fd = -1;
   memset(&pool, 0, sizeof(pool));
   pool.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
   pool.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
   pool.queueFamilyIndex = family;
   if (ops->create_pool(device, &pool, NULL, &e->pool) != VK_SUCCESS)
      goto failed;
   memset(&commands, 0, sizeof(commands));
   commands.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
   commands.commandPool = e->pool;
   commands.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
   commands.commandBufferCount = 1;
   if (ops->allocate_commands(device, &commands, &e->command) != VK_SUCCESS)
      goto failed;
   memset(&fence, 0, sizeof(fence));
   fence.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
   if (ops->create_fence(device, &fence, NULL, &e->fence) != VK_SUCCESS)
      goto failed;
   memset(&export, 0, sizeof(export));
   export.sType = VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO;
   export.handleTypes = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT;
   memset(&semaphore, 0, sizeof(semaphore));
   semaphore.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
   semaphore.pNext = &export;
   if (ops->create_semaphore(device, &semaphore, NULL, &e->done) != VK_SUCCESS)
      goto failed;
   return e;
failed:
   record_android_capture_submit_free(e);
   return NULL;
}

static VkAccessFlags record_acs_access(VkImageLayout layout)
{
   switch (layout)
   {
      case VK_IMAGE_LAYOUT_GENERAL:
         return VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
      case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
         return VK_ACCESS_SHADER_READ_BIT;
      case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
         return VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
               | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
      case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
         return VK_ACCESS_TRANSFER_READ_BIT;
      default:
         return 0;
   }
}

static bool record_acs_export(record_android_capture_submit_t *e)
{
   VkSemaphoreGetFdInfoKHR info;
   VkResult result;
   int fd = -1;
   memset(&info, 0, sizeof(info));
   info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_GET_FD_INFO_KHR;
   info.semaphore = e->done;
   info.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT;
   result = e->ops.export_fd(e->device, &info, &fd);
   if (result != VK_SUCCESS)
   {
      if (fd >= 0)
         e->ops.close_fd(e->ops.context, fd);
      return false;
   }
   e->exported = true;
   if (fd < -1)
   {
      e->broken = true;
      return false;
   }
   e->ready_fd = fd;
   return true;
}

static bool record_acs_source_valid(record_android_capture_submit_t *e,
      const record_android_capture_source_t *s)
{
   return !(!e || e->broken || !s || !s->image || s->image == e->image
         || !s->owner || !s->release || s->ready == e->done
         || (s->format != e->format
               && !(e->format == VK_FORMAT_R8G8B8A8_UNORM
                     && s->format == VK_FORMAT_R8G8B8A8_SRGB))
         || !record_acs_access(s->layout) || s->width > 16384
         || s->height > 16384 || s->x > s->width || s->y > s->height
         || e->width > s->width - s->x || e->height > s->height - s->y);
}

static void record_acs_copy(record_android_capture_submit_t *e,
      const record_android_capture_source_t *s, VkCommandBuffer command)
{
   VkImageMemoryBarrier before[2], after[2];
   VkImageCopy region;
   unsigned i;
   memset(before, 0, sizeof(before));
   for (i = 0; i < 2; i++)
   {
      before[i].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
      before[i].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      before[i].subresourceRange.levelCount
            = before[i].subresourceRange.layerCount = 1;
   }
   before[0].image = s->image;
   before[0].oldLayout = s->layout;
   before[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
   before[0].srcQueueFamilyIndex = before[0].dstQueueFamilyIndex
         = VK_QUEUE_FAMILY_IGNORED;
   before[0].srcAccessMask = record_acs_access(s->layout);
   before[0].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
   before[1].image = e->image;
   before[1].oldLayout
         = e->initialized ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_UNDEFINED;
   before[1].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
   before[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_FOREIGN_EXT;
   before[1].dstQueueFamilyIndex = e->family;
   before[1].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
   e->ops.barrier(command, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 2, before);
   memset(&region, 0, sizeof(region));
   region.srcSubresource.aspectMask = region.dstSubresource.aspectMask
         = VK_IMAGE_ASPECT_COLOR_BIT;
   region.srcSubresource.layerCount = region.dstSubresource.layerCount = 1;
   region.srcOffset.x = (int32_t)s->x;
   region.srcOffset.y = (int32_t)s->y;
   region.extent.width = e->width;
   region.extent.height = e->height;
   region.extent.depth = 1;
   e->ops.copy(command, s->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
         e->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
   memcpy(after, before, sizeof(after));
   after[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
   after[0].newLayout = s->layout;
   after[0].srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
   after[0].dstAccessMask = record_acs_access(s->layout);
   after[1].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
   after[1].newLayout = VK_IMAGE_LAYOUT_GENERAL;
   after[1].srcQueueFamilyIndex = e->family;
   after[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_FOREIGN_EXT;
   after[1].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
   after[1].dstAccessMask = 0;
   e->ops.barrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT,
         VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, NULL, 0, NULL, 2, after);
}

int record_android_capture_submit_push(record_android_capture_submit_t *e,
      const record_android_capture_source_t *s)
{
   VkCommandBufferBeginInfo begin;
   VkSubmitInfo submit;
   VkPipelineStageFlags stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
   VkResult result;
   if (!record_acs_source_valid(e, s))
      return -1;
   if (e->waiting || e->inflight || e->owner)
      return 0;
   if (e->ops.reset_command(e->command, 0) != VK_SUCCESS)
      goto failed;
   memset(&begin, 0, sizeof(begin));
   begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
   begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
   if (e->ops.begin(e->command, &begin) != VK_SUCCESS)
      goto failed;
   record_acs_copy(e, s, e->command);
   if (e->ops.end(e->command) != VK_SUCCESS
         || e->ops.reset_fences(e->device, 1, &e->fence) != VK_SUCCESS)
      goto failed;
   memset(&submit, 0, sizeof(submit));
   submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
   submit.commandBufferCount = 1;
   submit.pCommandBuffers = &e->command;
   submit.waitSemaphoreCount = s->ready ? 1 : 0;
   submit.pWaitSemaphores = s->ready ? &s->ready : NULL;
   submit.pWaitDstStageMask = s->ready ? &stage : NULL;
   submit.signalSemaphoreCount = 1;
   submit.pSignalSemaphores = &e->done;
   e->owner = s->owner;
   e->release_source = s->release;
   e->waiting = e->inflight = true;
   result = e->ops.submit(e->ops.context, e->queue, 1, &submit, e->fence);
   if (result == VK_TIMEOUT)
   {
      e->owner = NULL;
      e->waiting = e->inflight = false;
      return 0;
   }
   e->initialized = true;
   if (result != VK_SUCCESS && result != VK_NOT_READY)
   {
      e->broken = true;
      return -2;
   }
   if (result == VK_SUCCESS)
      record_acs_export(e);
   return e->broken ? -2 : 1;
failed:
   e->broken = true;
   return -1;
}

int record_android_capture_submit_record(record_android_capture_submit_t *e,
      const record_android_capture_source_t *s, VkCommandBuffer command)
{
   VkCommandBufferBeginInfo begin;
   if (!record_acs_source_valid(e, s) || !command || command == e->command
         || s->ready)
      return -1;
   if (e->waiting || e->inflight || e->owner)
      return 0;
   memset(&begin, 0, sizeof(begin));
   begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
   begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
   if (e->ops.reset_command(e->command, 0) != VK_SUCCESS
         || e->ops.begin(e->command, &begin) != VK_SUCCESS
         || e->ops.end(e->command) != VK_SUCCESS
         || e->ops.reset_fences(e->device, 1, &e->fence) != VK_SUCCESS)
   {
      e->broken = true;
      return -1;
   }
   record_acs_copy(e, s, command);
   e->owner = s->owner;
   e->release_source = s->release;
   e->recorded = e->waiting = true;
   return 1;
}

static int record_acs_complete_recorded(record_android_capture_submit_t *e)
{
   VkSubmitInfo submit;
   VkResult result;
   memset(&submit, 0, sizeof(submit));
   submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
   submit.commandBufferCount = 1;
   submit.pCommandBuffers = &e->command;
   submit.signalSemaphoreCount = 1;
   submit.pSignalSemaphores = &e->done;
   result = e->ops.submit(e->ops.context, e->queue, 1, &submit, e->fence);
   if (result == VK_TIMEOUT)
      return 0;
   e->completion_pending = false;
   e->inflight = true;
   if (result != VK_SUCCESS && result != VK_NOT_READY)
   {
      e->broken = true;
      return -1;
   }
   if (result == VK_SUCCESS && !e->broken)
   {
      record_acs_export(e);
      if (e->broken)
         return -1;
   }
   return 1;
}

int record_android_capture_submit_commit(record_android_capture_submit_t *e)
{
   if (!e || e->gpu_retired || !e->recorded || e->committed)
      return -1;
   e->initialized = e->committed = e->completion_pending = true;
   return record_acs_complete_recorded(e);
}

bool record_android_capture_submit_abort_recorded(
      record_android_capture_submit_t *e)
{
   if (!e || e->gpu_retired || !e->recorded || e->committed)
      return false;
   e->release_source(e->owner, false);
   e->owner = NULL;
   e->recorded = e->waiting = false;
   return true;
}

static int record_acs_retire(record_android_capture_submit_t *e)
{
   VkResult result;
   if (e->gpu_retired)
      return e->owner || e->inflight || e->completion_pending ? -1 : 1;
   if (e->recorded && !e->committed)
      return 0;
   if (e->completion_pending)
   {
      int completion = record_acs_complete_recorded(e);
      if (completion <= 0)
         return completion;
   }
   if (!e->inflight)
      return 1;
   result = e->ops.fence_status(e->device, e->fence);
   if (result == VK_NOT_READY)
      return 0;
   if (result != VK_SUCCESS)
   {
      e->broken = true;
      return -1;
   }
   e->inflight = false;
   e->release_source(e->owner, false);
   e->owner = NULL;
   return 1;
}

int record_android_capture_submit_poll(record_android_capture_submit_t *e)
{
   int result;
   if (!e)
      return -1;
   result = record_acs_retire(e);
   if (result <= 0)
      return e->broken ? -1 : result;
   if (e->broken)
      return -1;
   if (e->waiting && !e->exported && !record_acs_export(e))
   {
      e->broken = true;
      return -1;
   }
   return 1;
}

int record_android_capture_submit_take_ready(
      record_android_capture_submit_t *e, int *owned_fd)
{
   if (owned_fd)
      *owned_fd = -1;
   if (!e || !owned_fd || e->broken)
      return -1;
   if (!e->waiting || !e->exported || e->taken)
      return 0;
   *owned_fd = e->ready_fd;
   e->ready_fd = -1;
   e->taken = true;
   return 1;
}

bool record_android_capture_submit_release(record_android_capture_submit_t *e)
{
   if (!e || e->broken || !e->waiting || !e->taken || e->inflight || e->owner)
      return false;
   e->waiting = e->exported = e->taken = false;
   e->recorded = e->committed = e->completion_pending = false;
   return true;
}

int record_android_capture_submit_discard(record_android_capture_submit_t *e)
{
   int result;
   if (!e)
      return -1;
   e->broken = true;
   result = record_acs_retire(e);
   if (result <= 0)
      return result;
   if (e->ready_fd >= 0)
      e->ops.close_fd(e->ops.context, e->ready_fd);
   e->ready_fd = -1;
   e->waiting = e->exported = e->taken = false;
   e->recorded = e->committed = e->completion_pending = false;
   return 1;
}

bool record_android_capture_submit_prepare_device_destroy(
      record_android_capture_submit_t *e, bool queue_quiesced)
{
   VkResult result;
   if (!e || e->gpu_retired)
      return true;
   e->broken = true;
   if (e->recorded && (!e->committed || e->completion_pending)
         && !queue_quiesced)
      return false;
   if (e->inflight && !queue_quiesced)
   {
      result = e->ops.fence_status(e->device, e->fence);
      if (result != VK_SUCCESS && result != VK_ERROR_DEVICE_LOST)
         return false;
   }
   record_acs_destroy_gpu(e);
   return true;
}

bool record_android_capture_submit_after_device_destroy(
      record_android_capture_submit_t *e)
{
   if (!e)
      return true;
   if (!e->gpu_retired)
      return false;
   if (e->owner)
      e->release_source(e->owner, true);
   if (e->ready_fd >= 0)
      e->ops.close_fd(e->ops.context, e->ready_fd);
   e->owner = NULL;
   e->ready_fd = -1;
   e->waiting = e->inflight = e->exported = e->taken = false;
   e->recorded = e->committed = e->completion_pending = false;
   e->broken = true;
   e->device = VK_NULL_HANDLE;
   return true;
}
