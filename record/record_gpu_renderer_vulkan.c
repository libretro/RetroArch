/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "record_gpu_renderer_vulkan.h"
#include <stdlib.h>

#define RGV_SEMAPHORES 32
struct record_gpu_renderer_vulkan
{
   uint64_t sequence, last_submit, last_end, pictures;
   uint64_t slot_ready[VULKAN_ENCODE_POOL_SLOTS];
   uint64_t wait_values[RGV_SEMAPHORES], signal_values[RGV_SEMAPHORES];
   VkSemaphore waits[RGV_SEMAPHORES], signals[RGV_SEMAPHORES];
   VkPipelineStageFlags stages[RGV_SEMAPHORES];
   VkSubmitInfo submit;
   VkTimelineSemaphoreSubmitInfo timeline;
   VkCommandBuffer command;
   PFN_vkGetSemaphoreCounterValue counter;
   record_gpu_dispatch_vulkan_t *job;
   const vulkan_encode_pool_t *pool;
   record_gpu_frame_t frame;
   unsigned duration, gop;
   bool recorded, prepared, attempted, uncertain, failed, stopping, closed;
};
record_gpu_renderer_vulkan_t *record_gpu_renderer_vulkan_new(
      record_gpu_dispatch_vulkan_t *job, unsigned duration, unsigned gop,
      PFN_vkGetSemaphoreCounterValue counter)
{
   record_gpu_renderer_vulkan_t *r;
   if (!job || !duration || !gop || !counter)
      return NULL;
   r = (record_gpu_renderer_vulkan_t*)calloc(1, sizeof(*r));
   if (r)
   {
      r->job = job;
      r->duration = duration;
      r->gop = gop;
      r->counter = counter;
   }
   return r;
}
int record_gpu_renderer_vulkan_record(record_gpu_renderer_vulkan_t *r,
      VkCommandBuffer command, const vulkan_encode_capture_source_t *source,
      uint64_t timestamp)
{
   vulkan_encode_capture_t *capture;
   const vulkan_encode_pool_t *pool;
   unsigned slot;
   int result;
   if (!r || r->recorded || r->failed || r->stopping || !command || !source
         || timestamp < r->last_end || timestamp > (uint64_t)-1 - r->duration
         || r->sequence == (uint64_t)-1)
      return RECORD_GPU_QUEUE_ERROR;
   capture = record_gpu_dispatch_vulkan_capture(r->job);
   if (!capture)
      return record_gpu_dispatch_vulkan_status(r->job, NULL, NULL) == RECORD_VKD_STARTING
         ? RECORD_GPU_QUEUE_BUSY : RECORD_GPU_QUEUE_ERROR;
   pool = record_gpu_dispatch_vulkan_pool(r->job);
   if (!pool || !pool->count || pool->count > VULKAN_ENCODE_POOL_SLOTS
         || (r->pool && r->pool != pool))
      return RECORD_GPU_QUEUE_ERROR;
   r->pool = pool;
   result = record_gpu_dispatch_vulkan_acquire(r->job, &slot);
   if (result != RECORD_GPU_QUEUE_OK)
      return result;
   r->frame.slot = slot;
   r->recorded = true;
   r->prepared = r->attempted = false;
   if (slot >= r->pool->count || !vulkan_encode_capture_record(capture, slot,
            command, source, r->slot_ready[slot] != 0))
   {
      if (record_gpu_dispatch_vulkan_cancel_slot(r->job))
         r->recorded = false;
      else
         r->failed = true;
      return RECORD_GPU_QUEUE_ERROR;
   }
   r->frame.timestamp = timestamp;
   r->frame.duration = r->duration;
   r->frame.ready_value = ++r->sequence;
   r->frame.keyframe = r->pictures % r->gop == 0;
   r->command = command;
   return RECORD_GPU_QUEUE_OK;
}
const VkSubmitInfo *record_gpu_renderer_vulkan_submission(
      record_gpu_renderer_vulkan_t *r, const VkSubmitInfo *submit)
{
   const VkBaseInStructure *node;
   const VkTimelineSemaphoreSubmitInfo *timeline = NULL;
   unsigned i, count = 0, commands = 0;
   uint64_t previous;
   if (r)
      r->prepared = false;
   if (!r || !r->recorded || r->failed || r->attempted || !submit
         || submit->sType != VK_STRUCTURE_TYPE_SUBMIT_INFO
         || !submit->commandBufferCount || submit->commandBufferCount > 64 || !submit->pCommandBuffers
         || submit->waitSemaphoreCount >= RGV_SEMAPHORES
         || submit->signalSemaphoreCount >= RGV_SEMAPHORES
         || (submit->waitSemaphoreCount && (!submit->pWaitSemaphores || !submit->pWaitDstStageMask))
         || (submit->signalSemaphoreCount && !submit->pSignalSemaphores))
      return NULL;
   for (i = 0; i < submit->commandBufferCount; i++)
      if (submit->pCommandBuffers[i] == r->command)
         commands++;
   if (commands != 1)
      return NULL;
   for (node = (const VkBaseInStructure*)submit->pNext; node; node = node->pNext)
   {
      if (++count > 64)
         return NULL;
      if (node->sType == VK_STRUCTURE_TYPE_DEVICE_GROUP_SUBMIT_INFO
            || node->sType == VK_STRUCTURE_TYPE_D3D12_FENCE_SUBMIT_INFO_KHR
            || (node->sType == VK_STRUCTURE_TYPE_PROTECTED_SUBMIT_INFO
               && ((const VkProtectedSubmitInfo*)node)->protectedSubmit))
         return NULL;
      if (node->sType == VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO)
      {
         if (timeline || node != submit->pNext)
            return NULL;
         timeline = (const VkTimelineSemaphoreSubmitInfo*)node;
      }
   }
   if (timeline && (timeline->waitSemaphoreValueCount != submit->waitSemaphoreCount
         || timeline->signalSemaphoreValueCount != submit->signalSemaphoreCount
         || (submit->waitSemaphoreCount && !timeline->pWaitSemaphoreValues)
         || (submit->signalSemaphoreCount && !timeline->pSignalSemaphoreValues)))
      return NULL;
   previous = r->slot_ready[r->frame.slot];
   for (i = 0; i < submit->waitSemaphoreCount; i++)
   {
      if (submit->pWaitSemaphores[i] == r->pool->ready || submit->pWaitSemaphores[i] == r->pool->done)
         return NULL;
      r->waits[i] = submit->pWaitSemaphores[i];
      r->stages[i] = submit->pWaitDstStageMask[i];
      r->wait_values[i] = timeline ? timeline->pWaitSemaphoreValues[i] : 0;
   }
   if (previous)
   {
      r->waits[i] = r->pool->done;
      r->stages[i] = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
      r->wait_values[i] = previous;
   }
   for (i = 0; i < submit->signalSemaphoreCount; i++)
   {
      if (submit->pSignalSemaphores[i] == r->pool->ready || submit->pSignalSemaphores[i] == r->pool->done)
         return NULL;
      r->signals[i] = submit->pSignalSemaphores[i];
      r->signal_values[i] = timeline ? timeline->pSignalSemaphoreValues[i] : 0;
   }
   r->signals[i] = r->pool->ready;
   r->signal_values[i] = r->frame.ready_value;
   r->timeline.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
   r->timeline.pNext = timeline ? timeline->pNext : submit->pNext;
   r->timeline.waitSemaphoreValueCount = submit->waitSemaphoreCount + (previous ? 1 : 0);
   r->timeline.pWaitSemaphoreValues = r->wait_values;
   r->timeline.signalSemaphoreValueCount = submit->signalSemaphoreCount + 1;
   r->timeline.pSignalSemaphoreValues = r->signal_values;
   r->submit = *submit;
   r->submit.pNext = &r->timeline;
   r->submit.waitSemaphoreCount = r->timeline.waitSemaphoreValueCount;
   r->submit.pWaitSemaphores = r->submit.waitSemaphoreCount ? r->waits : NULL;
   r->submit.pWaitDstStageMask = r->submit.waitSemaphoreCount ? r->stages : NULL;
   r->submit.signalSemaphoreCount++;
   r->submit.pSignalSemaphores = r->signals;
   r->prepared = true;
   return &r->submit;
}
bool record_gpu_renderer_vulkan_submitted(record_gpu_renderer_vulkan_t *r, VkResult result)
{
   if (!r || !r->recorded || !r->prepared || r->attempted)
      return false;
   r->attempted = true;
   if (result != VK_SUCCESS)
   {
      r->uncertain = r->failed = true;
      return false;
   }
   r->last_submit = r->frame.ready_value;
   r->slot_ready[r->frame.slot] = r->last_submit;
   if (!record_gpu_dispatch_vulkan_submit(r->job, &r->frame))
   {
      r->failed = true;
      return false;
   }
   r->last_end = r->frame.timestamp + r->frame.duration;
   r->pictures++;
   r->recorded = false;
   return true;
}
bool record_gpu_renderer_vulkan_discard(record_gpu_renderer_vulkan_t *r)
{
   if (!r || !r->recorded || r->attempted
         || !record_gpu_dispatch_vulkan_cancel_slot(r->job))
      return false;
   r->recorded = r->prepared = false;
   return true;
}
static int rgv_close(record_gpu_renderer_vulkan_t *r)
{
   if (r->closed)
      return RECORD_GPU_QUEUE_OK;
   if (r->recorded && !record_gpu_dispatch_vulkan_cancel_slot(r->job))
      return RECORD_GPU_QUEUE_BUSY;
   r->recorded = false;
   if (!record_gpu_dispatch_vulkan_video_finish(r->job))
      return RECORD_GPU_QUEUE_BUSY;
   r->closed = true;
   return RECORD_GPU_QUEUE_OK;
}
int record_gpu_renderer_vulkan_finish(record_gpu_renderer_vulkan_t *r, bool quiesced)
{
   uint64_t value, required;
   VkResult result;
   if (!r)
      return RECORD_GPU_QUEUE_ERROR;
   r->stopping = true;
   if (r->closed)
      return RECORD_GPU_QUEUE_OK;
   if (!quiesced)
   {
      if (r->recorded && !r->attempted)
         return RECORD_GPU_QUEUE_BUSY;
      required = r->uncertain ? r->frame.ready_value : r->last_submit;
      if (required)
      {
         result = r->counter(r->pool->device, r->pool->ready, &value);
         if (result != VK_SUCCESS)
            return result == VK_ERROR_DEVICE_LOST ? RECORD_VKR_DEVICE_LOST : RECORD_GPU_QUEUE_ERROR;
         if (value < required)
            return RECORD_GPU_QUEUE_BUSY;
      }
   }
   return rgv_close(r);
}
bool record_gpu_renderer_vulkan_abandon_lost(record_gpu_renderer_vulkan_t *r,
      VkResult native_result)
{
   if (!r || native_result != VK_ERROR_DEVICE_LOST)
      return false;
   r->stopping = r->failed = true;
   record_gpu_dispatch_vulkan_cancel(r->job);
   return rgv_close(r) == RECORD_GPU_QUEUE_OK;
}
bool record_gpu_renderer_vulkan_free(record_gpu_renderer_vulkan_t *r)
{
   if (r && !r->closed)
      return false;
   free(r);
   return true;
}
