/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../../record/record_gpu_renderer_vulkan.c"
static vulkan_encode_pool_t pool;
static record_gpu_dispatch_vulkan_t *job = (record_gpu_dispatch_vulkan_t*)(uintptr_t)1;
static vulkan_encode_capture_t *capture = (vulkan_encode_capture_t*)(uintptr_t)2;
static enum record_gpu_dispatch_vulkan_state state;
static record_gpu_frame_t published;
static VkResult counter_result;
static uint64_t completed;
static unsigned next_slot, records, publications, cancellations, closures, queries;
static bool reserved, busy, capture_ok, publish_ok, cancel_ok, initialized, pool_unavailable;
static bool cancelled;
const vulkan_encode_pool_t *record_gpu_dispatch_vulkan_pool(const record_gpu_dispatch_vulkan_t *j)
{ assert(j == job); return state == RECORD_VKD_READY && !pool_unavailable ? &pool : NULL; }
vulkan_encode_capture_t *record_gpu_dispatch_vulkan_capture(const record_gpu_dispatch_vulkan_t *j)
{ assert(j == job); return state == RECORD_VKD_READY ? capture : NULL; }
enum record_gpu_dispatch_vulkan_state record_gpu_dispatch_vulkan_status(
      const record_gpu_dispatch_vulkan_t *j, enum record_gpu_session_vulkan_error *e, int *f)
{ assert(j == job); (void)e; (void)f; return state; }
int record_gpu_dispatch_vulkan_acquire(record_gpu_dispatch_vulkan_t *j, unsigned *slot)
{
   assert(j == job && !reserved);
   if (busy)
      return RECORD_GPU_QUEUE_BUSY;
   *slot = next_slot;
   reserved = true;
   return RECORD_GPU_QUEUE_OK;
}
bool record_gpu_dispatch_vulkan_cancel_slot(record_gpu_dispatch_vulkan_t *j)
{
   assert(j == job && reserved);
   if (!cancel_ok)
      return false;
   reserved = false;
   cancellations++;
   return true;
}
bool record_gpu_dispatch_vulkan_submit(record_gpu_dispatch_vulkan_t *j, const record_gpu_frame_t *frame)
{
   assert(j == job && reserved);
   if (!publish_ok)
      return false;
   published = *frame;
   publications++;
   reserved = false;
   return true;
}
bool record_gpu_dispatch_vulkan_video_finish(record_gpu_dispatch_vulkan_t *j)
{ assert(j == job && !reserved); closures++; return true; }
void record_gpu_dispatch_vulkan_cancel(record_gpu_dispatch_vulkan_t *j)
{ assert(j == job); cancelled = true; }
bool vulkan_encode_capture_record(vulkan_encode_capture_t *c, unsigned slot,
      VkCommandBuffer command, const vulkan_encode_capture_source_t *source, bool reused)
{
   assert(c == capture && slot == next_slot && command && source);
   initialized = reused;
   records++;
   return capture_ok;
}
static VKAPI_ATTR VkResult VKAPI_CALL query(VkDevice device, VkSemaphore semaphore, uint64_t *value)
{
   assert(device == pool.device && semaphore == pool.ready);
   queries++;
   *value = completed;
   return counter_result;
}
static record_gpu_renderer_vulkan_t *setup(void)
{
   memset(&pool, 0, sizeof(pool));
   pool.count = 2;
   pool.device = (VkDevice)(uintptr_t)3;
   pool.ready = (VkSemaphore)(uintptr_t)4;
   pool.done = (VkSemaphore)(uintptr_t)5;
   state = RECORD_VKD_READY;
   counter_result = VK_SUCCESS;
   completed = 0;
   next_slot = records = publications = cancellations = closures = queries = 0;
   reserved = busy = initialized = pool_unavailable = cancelled = false;
   capture_ok = publish_ok = cancel_ok = true;
   return record_gpu_renderer_vulkan_new(job, 1001, 4, query);
}
static void record_frame(record_gpu_renderer_vulkan_t *r, uint64_t pts)
{
   vulkan_encode_capture_source_t source;
   memset(&source, 0, sizeof(source));
   assert(record_gpu_renderer_vulkan_record(r, (VkCommandBuffer)(uintptr_t)6,
            &source, pts) == RECORD_GPU_QUEUE_OK);
}
static const VkSubmitInfo *batch(record_gpu_renderer_vulkan_t *r, const void *chain)
{
   VkSubmitInfo info;
   static const VkCommandBuffer command = (VkCommandBuffer)(uintptr_t)6;
   static const VkSemaphore wait = (VkSemaphore)(uintptr_t)7;
   static const VkSemaphore signal = (VkSemaphore)(uintptr_t)8;
   static const VkPipelineStageFlags stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
   const VkSubmitInfo *result;
   memset(&info, 0, sizeof(info));
   info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
   info.pNext = chain;
   info.commandBufferCount = info.waitSemaphoreCount = info.signalSemaphoreCount = 1;
   info.pCommandBuffers = &command;
   info.pWaitSemaphores = &wait;
   info.pWaitDstStageMask = &stage;
   info.pSignalSemaphores = &signal;
   result = record_gpu_renderer_vulkan_submission(r, &info);
   assert(info.pNext == chain && info.signalSemaphoreCount == 1 && signal == (VkSemaphore)(uintptr_t)8);
   return result;
}
static void ownership(void)
{
   record_gpu_renderer_vulkan_t *r = setup();
   const VkSubmitInfo *info;
   const VkTimelineSemaphoreSubmitInfo *timeline;
   VkPerformanceQuerySubmitInfoKHR foreign;
   VkTimelineSemaphoreSubmitInfo existing;
   uint64_t wait = 17, signal = 23;
   memset(&foreign, 0, sizeof(foreign));
   foreign.sType = VK_STRUCTURE_TYPE_PERFORMANCE_QUERY_SUBMIT_INFO_KHR;
   record_frame(r, 0);
   assert(!initialized && !record_gpu_renderer_vulkan_free(r));
   info = batch(r, &foreign);
   assert(info && info->signalSemaphoreCount == 2 && info->waitSemaphoreCount == 1);
   timeline = (const VkTimelineSemaphoreSubmitInfo*)info->pNext;
   assert(timeline->pNext == &foreign && timeline->pSignalSemaphoreValues[0] == 0);
   assert(timeline->pSignalSemaphoreValues[1] == 1 && timeline->pWaitSemaphoreValues[0] == 0);
   assert(!publications && record_gpu_renderer_vulkan_submitted(r, VK_SUCCESS));
   assert(publications == 1 && published.keyframe && published.ready_value == 1);
   assert(!record_gpu_renderer_vulkan_submitted(r, VK_SUCCESS));
   record_frame(r, 1001);
   assert(initialized);
   memset(&existing, 0, sizeof(existing));
   existing.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
   existing.pNext = &foreign;
   existing.waitSemaphoreValueCount = existing.signalSemaphoreValueCount = 1;
   existing.pWaitSemaphoreValues = &wait;
   existing.pSignalSemaphoreValues = &signal;
   info = batch(r, &existing);
   assert(info && info->waitSemaphoreCount == 2);
   timeline = (const VkTimelineSemaphoreSubmitInfo*)info->pNext;
   assert(timeline->pNext == &foreign && timeline->pWaitSemaphoreValues[0] == 17);
   assert(timeline->pWaitSemaphoreValues[1] == 1 && timeline->pSignalSemaphoreValues[0] == 23);
   assert(existing.pNext == &foreign && wait == 17 && signal == 23);
   assert(record_gpu_renderer_vulkan_submitted(r, VK_SUCCESS));
   assert(!published.keyframe && published.ready_value == 2);
   {
      vulkan_encode_capture_source_t source;
      memset(&source, 0, sizeof(source));
      pool_unavailable = true;
      assert(record_gpu_renderer_vulkan_record(r, (VkCommandBuffer)(uintptr_t)6, &source, 2002)
            == RECORD_GPU_QUEUE_ERROR);
   }
   state = RECORD_VKD_DRAINING;
   assert(record_gpu_renderer_vulkan_finish(r, false) == RECORD_GPU_QUEUE_BUSY && !closures);
   counter_result = VK_ERROR_DEVICE_LOST;
   assert(record_gpu_renderer_vulkan_finish(r, false) == RECORD_VKR_DEVICE_LOST && !closures);
   counter_result = VK_SUCCESS;
   completed = 2;
   assert(record_gpu_renderer_vulkan_finish(r, false) == RECORD_GPU_QUEUE_OK && closures == 1);
   assert(record_gpu_renderer_vulkan_finish(r, false) == RECORD_GPU_QUEUE_OK && closures == 1);
   assert(record_gpu_renderer_vulkan_free(r));
}
static void failures(void)
{
   record_gpu_renderer_vulkan_t *r;
   VkDeviceGroupSubmitInfo group;
   VkProtectedSubmitInfo protected_info;
   VkTimelineSemaphoreSubmitInfo timeline;
   VkPerformanceQuerySubmitInfoKHR foreign;
   unsigned lane;
   for (lane = 0; lane < 5; lane++)
   {
      r = setup();
      record_frame(r, 0);
      if (lane < 2)
      {
         assert(batch(r, NULL));
         publish_ok = lane != 0;
         assert(!record_gpu_renderer_vulkan_submitted(r, lane ? VK_ERROR_DEVICE_LOST : VK_SUCCESS));
         assert(!record_gpu_renderer_vulkan_discard(r));
         completed = 1;
         assert(record_gpu_renderer_vulkan_finish(r, false) == RECORD_GPU_QUEUE_OK);
      }
      else
      {
         memset(&group, 0, sizeof(group));
         group.sType = VK_STRUCTURE_TYPE_DEVICE_GROUP_SUBMIT_INFO;
         memset(&protected_info, 0, sizeof(protected_info));
         protected_info.sType = VK_STRUCTURE_TYPE_PROTECTED_SUBMIT_INFO;
         protected_info.protectedSubmit = VK_TRUE;
         memset(&timeline, 0, sizeof(timeline));
         timeline.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
         memset(&foreign, 0, sizeof(foreign));
         foreign.sType = VK_STRUCTURE_TYPE_PERFORMANCE_QUERY_SUBMIT_INFO_KHR;
         foreign.pNext = &timeline;
         assert(!batch(r, lane == 2 ? (void*)&group : lane == 3 ? (void*)&protected_info : (void*)&foreign));
         assert(!record_gpu_renderer_vulkan_submitted(r, VK_SUCCESS));
         assert(record_gpu_renderer_vulkan_finish(r, false) == RECORD_GPU_QUEUE_BUSY);
         assert(record_gpu_renderer_vulkan_discard(r));
      }
      assert(record_gpu_renderer_vulkan_finish(r, true) == RECORD_GPU_QUEUE_OK);
      assert(record_gpu_renderer_vulkan_free(r) && !reserved && closures == 1);
   }
   r = setup();
   state = RECORD_VKD_STARTING;
   {
      vulkan_encode_capture_source_t source;
      memset(&source, 0, sizeof(source));
      assert(record_gpu_renderer_vulkan_record(r, (VkCommandBuffer)(uintptr_t)6, &source, 0) == RECORD_GPU_QUEUE_BUSY);
      state = RECORD_VKD_READY;
      busy = true;
      assert(record_gpu_renderer_vulkan_record(r, (VkCommandBuffer)(uintptr_t)6, &source, 0) == RECORD_GPU_QUEUE_BUSY);
      busy = false;
      capture_ok = false;
      assert(record_gpu_renderer_vulkan_record(r, (VkCommandBuffer)(uintptr_t)6, &source, 0) == RECORD_GPU_QUEUE_ERROR);
      assert(!reserved && cancellations == 1);
   }
   assert(record_gpu_renderer_vulkan_finish(r, false) == RECORD_GPU_QUEUE_OK);
   assert(record_gpu_renderer_vulkan_free(r));
   r = setup();
   record_frame(r, 0);
   assert(batch(r, NULL));
   assert(!record_gpu_renderer_vulkan_submitted(r, VK_ERROR_DEVICE_LOST));
   assert(record_gpu_renderer_vulkan_finish(r, false) == RECORD_GPU_QUEUE_BUSY && reserved);
   counter_result = VK_ERROR_DEVICE_LOST;
   assert(record_gpu_renderer_vulkan_finish(r, false) == RECORD_VKR_DEVICE_LOST && reserved);
   assert(!record_gpu_renderer_vulkan_abandon_lost(r, VK_ERROR_OUT_OF_DEVICE_MEMORY) && reserved && !cancelled);
   assert(record_gpu_renderer_vulkan_abandon_lost(r, VK_ERROR_DEVICE_LOST) && !reserved && cancelled);
   assert(record_gpu_renderer_vulkan_free(r));
}
static void bounds(void)
{
   record_gpu_renderer_vulkan_t *r = setup();
   VkSubmitInfo info, saved;
   VkPerformanceQuerySubmitInfoKHR cycle;
   VkTimelineSemaphoreSubmitInfo timeline;
   VkCommandBuffer commands[2];
   static VkSemaphore waits[32], signals[32];
   static VkPipelineStageFlags stages[32];
   vulkan_encode_capture_source_t source;
   unsigned i;
   memset(&info, 0, sizeof(info));
   memset(&cycle, 0, sizeof(cycle));
   memset(&timeline, 0, sizeof(timeline));
   memset(&source, 0, sizeof(source));
   info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
   commands[0] = commands[1] = (VkCommandBuffer)(uintptr_t)6;
   info.commandBufferCount = 1;
   info.pCommandBuffers = commands;
   for (i = 0; i < 32; i++)
   {
      waits[i] = (VkSemaphore)(uintptr_t)(100 + i);
      signals[i] = (VkSemaphore)(uintptr_t)(200 + i);
      stages[i] = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
   }
   info.pWaitSemaphores = waits;
   info.pWaitDstStageMask = stages;
   info.pSignalSemaphores = signals;
   record_frame(r, 0);
   info.waitSemaphoreCount = info.signalSemaphoreCount = 31;
   saved = info;
   assert(record_gpu_renderer_vulkan_submission(r, &info));
   assert(!memcmp(&info, &saved, sizeof(info)));
   info.signalSemaphoreCount = 32;
   assert(!record_gpu_renderer_vulkan_submission(r, &info));
   assert(!record_gpu_renderer_vulkan_submitted(r, VK_SUCCESS));
   info.signalSemaphoreCount = 0;
   info.waitSemaphoreCount = 32;
   assert(!record_gpu_renderer_vulkan_submission(r, &info));
   info.waitSemaphoreCount = 0;
   info.commandBufferCount = 2;
   assert(!record_gpu_renderer_vulkan_submission(r, &info));
   info.commandBufferCount = 1;
   commands[0] = (VkCommandBuffer)(uintptr_t)9;
   assert(!record_gpu_renderer_vulkan_submission(r, &info));
   commands[0] = (VkCommandBuffer)(uintptr_t)6;
   cycle.sType = VK_STRUCTURE_TYPE_PERFORMANCE_QUERY_SUBMIT_INFO_KHR;
   cycle.pNext = &cycle;
   info.pNext = &cycle;
   assert(!record_gpu_renderer_vulkan_submission(r, &info));
   cycle.pNext = NULL;
   cycle.sType = VK_STRUCTURE_TYPE_D3D12_FENCE_SUBMIT_INFO_KHR;
   assert(!record_gpu_renderer_vulkan_submission(r, &info));
   info.pNext = &timeline;
   timeline.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
   info.waitSemaphoreCount = 1;
   timeline.waitSemaphoreValueCount = 1;
   assert(!record_gpu_renderer_vulkan_submission(r, &info));
   info.pNext = NULL;
   waits[0] = pool.ready;
   assert(!record_gpu_renderer_vulkan_submission(r, &info));
   assert(record_gpu_renderer_vulkan_discard(r));
   assert(record_gpu_renderer_vulkan_finish(r, false) == RECORD_GPU_QUEUE_OK && !queries);
   assert(record_gpu_renderer_vulkan_free(r));
   r = setup();
   capture_ok = cancel_ok = false;
   assert(record_gpu_renderer_vulkan_record(r, commands[0], &source, 0) == RECORD_GPU_QUEUE_ERROR && reserved);
   assert(record_gpu_renderer_vulkan_finish(r, true) == RECORD_GPU_QUEUE_BUSY);
   cancel_ok = true;
   assert(record_gpu_renderer_vulkan_finish(r, true) == RECORD_GPU_QUEUE_OK && !reserved);
   assert(record_gpu_renderer_vulkan_free(r));
   r = setup();
   state = RECORD_VKD_STARTING;
   assert(record_gpu_renderer_vulkan_finish(r, false) == RECORD_GPU_QUEUE_OK);
   assert(record_gpu_renderer_vulkan_free(r));
}
int main(void)
{
   ownership();
   failures();
   bounds();
   puts("Vulkan renderer bridge: semaphore chains, publication, discard and GPU retirement PASS");
   return 0;
}
