/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../../record/record_android_capture_submit.h"
#include "../../../record/record_android_capture_pipe.h"
#include "../../../gfx/common/vulkan_android_queue.h"
#define DEVICE ((VkDevice)(uintptr_t)1)
#define QUEUE ((VkQueue)(uintptr_t)2)
#define DEST ((VkImage)(uintptr_t)3)
#define SOURCE ((VkImage)(uintptr_t)4)
#define POOL ((VkCommandPool)(uintptr_t)5)
#define COMMAND ((VkCommandBuffer)(uintptr_t)6)
#define FENCE ((VkFence)(uintptr_t)7)
#define DONE ((VkSemaphore)(uintptr_t)8)
#define READY ((VkSemaphore)(uintptr_t)9)
#define RENDER_COMMAND ((VkCommandBuffer)(uintptr_t)10)
typedef struct model
{
   vulkan_android_queue_t queued;
   VkResult result, fence_error;
   VkImageLayout source_layout, target_layout;
   unsigned new_failure, record_failure, barriers, copies, submits, exports;
   unsigned refs, releases, fd_refs, close_calls, export_failures;
   bool pool, command, fence, semaphore, pending, complete, signal;
   bool deferred, auto_complete, device_alive, gone_release, ready;
   bool partial_fd, invalid_fd, signaled_fd, external, abort_proof;
} model_t;
static model_t m;

static VkResult VKAPI_CALL create_pool(VkDevice d,
      const VkCommandPoolCreateInfo *i, const VkAllocationCallbacks *a,
      VkCommandPool *out)
{
   assert(m.device_alive && d == DEVICE && !a && !m.pool);
   assert(i->sType == VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO
         && i->queueFamilyIndex == 3
         && i->flags == VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
   m.pool = true;
   *out = POOL;
   return m.new_failure == 1 ? VK_ERROR_OUT_OF_DEVICE_MEMORY : VK_SUCCESS;
}
static void VKAPI_CALL destroy_pool(
      VkDevice d, VkCommandPool p, const VkAllocationCallbacks *a)
{
   assert(m.device_alive && d == DEVICE && p == POOL && !a && m.pool);
   assert(!m.pending || m.complete || m.fence_error == VK_ERROR_DEVICE_LOST);
   m.pool = m.command = false;
}
static VkResult VKAPI_CALL allocate_commands(
      VkDevice d, const VkCommandBufferAllocateInfo *i, VkCommandBuffer *out)
{
   assert(d == DEVICE && m.pool
         && i->sType == VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO
         && i->commandPool == POOL
         && i->level == VK_COMMAND_BUFFER_LEVEL_PRIMARY
         && i->commandBufferCount == 1);
   m.command = true;
   *out = COMMAND;
   return m.new_failure == 2 ? VK_ERROR_OUT_OF_DEVICE_MEMORY : VK_SUCCESS;
}
static VkResult VKAPI_CALL create_fence(VkDevice d, const VkFenceCreateInfo *i,
      const VkAllocationCallbacks *a, VkFence *out)
{
   assert(d == DEVICE && !a && !i->flags
         && i->sType == VK_STRUCTURE_TYPE_FENCE_CREATE_INFO);
   m.fence = true;
   *out = FENCE;
   return m.new_failure == 3 ? VK_ERROR_OUT_OF_DEVICE_MEMORY : VK_SUCCESS;
}
static void VKAPI_CALL destroy_fence(
      VkDevice d, VkFence f, const VkAllocationCallbacks *a)
{
   assert(m.device_alive && d == DEVICE && f == FENCE && !a && m.fence);
   assert(!m.pending || m.complete || m.fence_error == VK_ERROR_DEVICE_LOST);
   m.fence = false;
}
static VkResult VKAPI_CALL create_semaphore(VkDevice d,
      const VkSemaphoreCreateInfo *i, const VkAllocationCallbacks *a,
      VkSemaphore *out)
{
   const VkExportSemaphoreCreateInfo *e = i->pNext;
   assert(d == DEVICE && !a
         && i->sType == VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO && !i->flags);
   assert(e->sType == VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO
         && e->handleTypes == VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT
         && !e->pNext);
   m.semaphore = true;
   *out = DONE;
   return m.new_failure == 4 ? VK_ERROR_OUT_OF_DEVICE_MEMORY : VK_SUCCESS;
}
static void VKAPI_CALL destroy_semaphore(
      VkDevice d, VkSemaphore s, const VkAllocationCallbacks *a)
{
   assert(m.device_alive && d == DEVICE && s == DONE && !a && m.semaphore);
   assert(!m.pending || m.complete || m.fence_error == VK_ERROR_DEVICE_LOST);
   m.semaphore = false;
}
static VkResult VKAPI_CALL reset_command(
      VkCommandBuffer c, VkCommandBufferResetFlags flags)
{
   assert(c == COMMAND && !flags && m.command && (!m.pending || m.complete));
   m.barriers = m.copies = 0;
   return m.record_failure == 1 ? VK_ERROR_DEVICE_LOST : VK_SUCCESS;
}
static VkResult VKAPI_CALL begin(
      VkCommandBuffer c, const VkCommandBufferBeginInfo *i)
{
   assert(c == COMMAND
         && i->sType == VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO
         && i->flags == VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
   return m.record_failure == 2 ? VK_ERROR_DEVICE_LOST : VK_SUCCESS;
}
static VkResult VKAPI_CALL end(VkCommandBuffer c)
{
   assert(c == COMMAND);
   assert(m.external ? (!m.barriers && !m.copies)
                     : (m.barriers == 2 && m.copies == 1));
   return m.record_failure == 3 ? VK_ERROR_DEVICE_LOST : VK_SUCCESS;
}
static void VKAPI_CALL barrier(VkCommandBuffer c, VkPipelineStageFlags src,
      VkPipelineStageFlags dst, VkDependencyFlags flags, uint32_t nm,
      const VkMemoryBarrier *mb, uint32_t nb, const VkBufferMemoryBarrier *bb,
      uint32_t ni, const VkImageMemoryBarrier *b)
{
   assert(c == (m.external ? RENDER_COMMAND : COMMAND) && !flags && !nm && !mb
         && !nb && !bb && ni == 2);
   assert(b[0].sType == VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER
         && b[0].image == SOURCE);
   assert(b[1].sType == VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER
         && b[1].image == DEST);
   assert(b[0].srcQueueFamilyIndex == VK_QUEUE_FAMILY_IGNORED
         && b[0].dstQueueFamilyIndex == VK_QUEUE_FAMILY_IGNORED);
   assert(b[0].subresourceRange.aspectMask == VK_IMAGE_ASPECT_COLOR_BIT
         && b[0].subresourceRange.layerCount == 1
         && b[0].subresourceRange.levelCount == 1);
   assert(b[1].subresourceRange.aspectMask == VK_IMAGE_ASPECT_COLOR_BIT
         && b[1].subresourceRange.layerCount == 1
         && b[1].subresourceRange.levelCount == 1);
   if (!m.barriers)
   {
      assert(src == VK_PIPELINE_STAGE_ALL_COMMANDS_BIT
            && dst == VK_PIPELINE_STAGE_TRANSFER_BIT);
      assert(b[0].oldLayout == m.source_layout
            && b[0].newLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
            && b[0].dstAccessMask == VK_ACCESS_TRANSFER_READ_BIT);
      assert(b[1].oldLayout == m.target_layout
            && b[1].newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
      assert(b[1].srcQueueFamilyIndex == VK_QUEUE_FAMILY_FOREIGN_EXT
            && b[1].dstQueueFamilyIndex == 3 && !b[1].srcAccessMask
            && b[1].dstAccessMask == VK_ACCESS_TRANSFER_WRITE_BIT);
   }
   else
   {
      assert(src == VK_PIPELINE_STAGE_TRANSFER_BIT
            && dst == VK_PIPELINE_STAGE_ALL_COMMANDS_BIT && m.copies == 1);
      assert(b[0].oldLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
            && b[0].newLayout == m.source_layout
            && b[0].srcAccessMask == VK_ACCESS_TRANSFER_READ_BIT);
      assert(b[1].oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
            && b[1].newLayout == VK_IMAGE_LAYOUT_GENERAL);
      assert(b[1].srcQueueFamilyIndex == 3
            && b[1].dstQueueFamilyIndex == VK_QUEUE_FAMILY_FOREIGN_EXT
            && b[1].srcAccessMask == VK_ACCESS_TRANSFER_WRITE_BIT
            && !b[1].dstAccessMask);
   }
   m.barriers++;
}
static void VKAPI_CALL copy(VkCommandBuffer c, VkImage src, VkImageLayout sl,
      VkImage dst, VkImageLayout dl, uint32_t count, const VkImageCopy *r)
{
   assert(c == (m.external ? RENDER_COMMAND : COMMAND) && src == SOURCE
         && dst == DEST && count == 1 && m.barriers == 1);
   assert(sl == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
         && dl == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
   assert(r->srcOffset.x == 8 && r->srcOffset.y == 6 && !r->srcOffset.z
         && !r->dstOffset.x && !r->dstOffset.y && !r->dstOffset.z);
   assert(r->srcSubresource.layerCount == 1 && r->dstSubresource.layerCount == 1
         && r->srcSubresource.aspectMask == VK_IMAGE_ASPECT_COLOR_BIT
         && r->dstSubresource.aspectMask == VK_IMAGE_ASPECT_COLOR_BIT);
   assert(r->extent.width == 128 && r->extent.height == 96
         && r->extent.depth == 1);
   m.copies++;
}
static VkResult VKAPI_CALL reset_fences(
      VkDevice d, uint32_t n, const VkFence *f)
{
   assert(d == DEVICE && n == 1 && *f == FENCE && (!m.pending || m.complete));
   m.pending = m.complete = false;
   assert(!m.signal);
   return m.record_failure == 4 ? VK_ERROR_DEVICE_LOST : VK_SUCCESS;
}
static VkResult VKAPI_CALL fence_status(VkDevice d, VkFence f)
{
   assert(m.device_alive && d == DEVICE && f == FENCE);
   return m.fence_error != VK_SUCCESS ? m.fence_error
         : m.pending && m.complete    ? VK_SUCCESS
                                      : VK_NOT_READY;
}
static VkResult VKAPI_CALL native_submit(
      VkQueue q, uint32_t n, const VkSubmitInfo *s, VkFence f)
{
   assert(q == QUEUE && n == 1 && f == FENCE
         && s->sType == VK_STRUCTURE_TYPE_SUBMIT_INFO && !s->pNext);
   assert(s->commandBufferCount == 1 && *s->pCommandBuffers == COMMAND
         && s->signalSemaphoreCount == 1 && *s->pSignalSemaphores == DONE);
   assert(s->waitSemaphoreCount == (m.ready ? 1u : 0u));
   if (m.ready)
      assert(*s->pWaitSemaphores == READY
            && *s->pWaitDstStageMask == VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
   m.submits++;
   m.pending = m.signal = true;
   m.complete = m.auto_complete;
   return m.result;
}
static VkResult submit(
      void *context, VkQueue q, uint32_t n, const VkSubmitInfo *s, VkFence f)
{
   assert(context == &m);
   if (m.result == VK_TIMEOUT)
      return VK_TIMEOUT;
   if (m.deferred)
      return vulkan_android_queue_push(&m.queued, q, n, s, f);
   return native_submit(q, n, s, f);
}
static VkResult VKAPI_CALL export_fd(
      VkDevice d, const VkSemaphoreGetFdInfoKHR *i, int *fd)
{
   assert(d == DEVICE && m.pending && m.signal
         && i->sType == VK_STRUCTURE_TYPE_SEMAPHORE_GET_FD_INFO_KHR
         && i->semaphore == DONE
         && i->handleType == VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT);
   m.exports++;
   if (m.export_failures)
   {
      m.export_failures--;
      if (m.partial_fd)
      {
         *fd = 100;
         m.fd_refs++;
      }
      return VK_ERROR_TOO_MANY_OBJECTS;
   }
   m.signal = false;
   if (m.signaled_fd)
   {
      assert(m.complete);
      *fd = -1;
   }
   else if (m.invalid_fd)
      *fd = -2;
   else
   {
      *fd = 100;
      m.fd_refs++;
   }
   return VK_SUCCESS;
}
static void close_fd(void *context, int fd)
{
   assert(context == &m && fd == 100 && m.fd_refs);
   m.fd_refs--;
   m.close_calls++;
}
static void release_source(void *owner, bool destroyed)
{
   assert(owner == &m && m.refs == 1);
   assert(destroyed
               ? !m.device_alive
               : (m.pending && m.complete) || (m.abort_proof && !m.pending));
   m.refs--;
   m.releases++;
   m.gone_release = destroyed;
}
static const record_android_capture_submit_ops_t ops = {create_pool,
      destroy_pool, allocate_commands, create_fence, destroy_fence,
      create_semaphore, destroy_semaphore, reset_command, begin, end, barrier,
      copy, reset_fences, fence_status, export_fd, submit, close_fd, &m};
static void reset(void)
{
   assert(!m.pool && !m.fence && !m.semaphore && !m.refs && !m.fd_refs);
   memset(&m, 0, sizeof(m));
   m.device_alive = true;
   m.source_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
   m.target_layout = VK_IMAGE_LAYOUT_UNDEFINED;
}
static record_android_capture_submit_t *create(void)
{
   return record_android_capture_submit_new(
         DEVICE, QUEUE, 3, DEST, VK_FORMAT_R8G8B8A8_UNORM, 128, 96, &ops);
}
static record_android_capture_source_t source(void)
{
   record_android_capture_source_t s;
   memset(&s, 0, sizeof(s));
   s.image = SOURCE;
   s.owner = &m;
   s.release = release_source;
   s.format = VK_FORMAT_R8G8B8A8_UNORM;
   s.layout = m.source_layout;
   s.width = 160;
   s.height = 120;
   s.x = 8;
   s.y = 6;
   s.ready = m.ready ? READY : VK_NULL_HANDLE;
   m.refs = 1;
   return s;
}
static void cycles(bool deferred, bool ready)
{
   record_android_capture_submit_t *e;
   record_android_capture_source_t s;
   unsigned i;
   int fd;
   reset();
   m.deferred = deferred;
   m.ready = ready;
   e = create();
   assert(e);
   for (i = 0; i < 1000; i++)
   {
      s = source();
      s.format = i & 1 ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;
      assert(record_android_capture_submit_push(e, &s) == 1);
      assert(record_android_capture_submit_push(e, &s) == 0);
      assert(!record_android_capture_submit_free(e));
      assert(record_android_capture_submit_poll(e) == 0 && m.refs == 1);
      if (deferred)
      {
         assert(record_android_capture_submit_take_ready(e, &fd) == 0);
         s.ready = VK_NULL_HANDLE;
         assert(vulkan_android_queue_flush(&m.queued, native_submit)
               == VK_SUCCESS);
      }
      else
      {
         assert(record_android_capture_submit_take_ready(e, &fd) == 1
               && fd == 100);
         close_fd(&m, fd);
         assert(!record_android_capture_submit_release(e));
      }
      m.complete = true;
      assert(record_android_capture_submit_poll(e) == 1 && !m.refs);
      if (deferred)
      {
         assert(record_android_capture_submit_take_ready(e, &fd) == 1
               && fd == 100);
         close_fd(&m, fd);
      }
      assert(record_android_capture_submit_take_ready(e, &fd) == 0);
      assert(record_android_capture_submit_release(e));
      assert(!record_android_capture_submit_release(e));
      m.target_layout = VK_IMAGE_LAYOUT_GENERAL;
   }
   assert(m.releases == 1000 && record_android_capture_submit_free(e));
}
static void faults(void)
{
   record_android_capture_submit_t *e;
   record_android_capture_source_t s;
   unsigned i;
   int fd;
   for (i = 1; i <= 4; i++)
   {
      reset();
      m.new_failure = i;
      assert(!create());
      reset();
      e = create();
      assert(e);
      s = source();
      m.record_failure = i;
      assert(record_android_capture_submit_push(e, &s) == -1 && m.refs == 1);
      m.refs = 0;
      assert(record_android_capture_submit_free(e));
   }
   reset();
   e = create();
   assert(e);
   s = source();
   m.result = VK_TIMEOUT;
   assert(record_android_capture_submit_push(e, &s) == 0 && m.refs == 1);
   m.refs = 0;
   assert(record_android_capture_submit_free(e));
   reset();
   e = create();
   assert(e);
   s = source();
   m.export_failures = 1;
   m.partial_fd = true;
   assert(record_android_capture_submit_push(e, &s) == 1 && !m.fd_refs
         && m.close_calls == 1);
   m.complete = true;
   assert(record_android_capture_submit_poll(e) == 1 && m.exports == 2
         && !m.refs);
   assert(record_android_capture_submit_take_ready(e, &fd) == 1);
   close_fd(&m, fd);
   assert(record_android_capture_submit_release(e)
         && record_android_capture_submit_free(e));
   reset();
   e = create();
   assert(e);
   s = source();
   m.export_failures = 2;
   assert(record_android_capture_submit_push(e, &s) == 1);
   m.complete = true;
   assert(record_android_capture_submit_poll(e) == -1 && !m.refs && m.signal);
   assert(!record_android_capture_submit_free(e));
   assert(record_android_capture_submit_discard(e) == 1);
   assert(record_android_capture_submit_free(e));
   reset();
   e = create();
   assert(e);
   s = source();
   m.invalid_fd = true;
   assert(record_android_capture_submit_push(e, &s) == -2 && m.refs == 1);
   assert(record_android_capture_submit_take_ready(e, &fd) == -1);
   assert(record_android_capture_submit_discard(e) == 0);
   m.complete = true;
   assert(record_android_capture_submit_discard(e) == 1 && m.exports == 1
         && !m.refs);
   assert(record_android_capture_submit_free(e));
   reset();
   e = create();
   assert(e);
   s = source();
   m.auto_complete = m.signaled_fd = true;
   assert(record_android_capture_submit_push(e, &s) == 1);
   assert(record_android_capture_submit_take_ready(e, &fd) == 1 && fd == -1);
   assert(record_android_capture_submit_poll(e) == 1
         && record_android_capture_submit_release(e));
   assert(record_android_capture_submit_free(e));
   reset();
   e = create();
   assert(e);
   s = source();
   m.result = VK_ERROR_DEVICE_LOST;
   assert(record_android_capture_submit_push(e, &s) == -2);
   m.fence_error = VK_ERROR_DEVICE_LOST;
   assert(record_android_capture_submit_discard(e) == -1 && m.refs == 1);
   assert(!record_android_capture_submit_free(e));
   assert(record_android_capture_submit_prepare_device_destroy(e, false));
   assert(!m.pool && !m.command && !m.fence && !m.semaphore && m.refs == 1);
   assert(record_android_capture_submit_poll(e) == -1);
   m.device_alive = false;
   assert(record_android_capture_submit_after_device_destroy(e));
   assert(!m.refs && m.gone_release && record_android_capture_submit_free(e));
   reset();
   e = create();
   assert(e);
   s = source();
   assert(record_android_capture_submit_push(e, &s) == 1 && m.fd_refs == 1);
   assert(record_android_capture_submit_discard(e) == 0 && m.refs == 1);
   m.complete = true;
   assert(record_android_capture_submit_discard(e) == 1 && !m.fd_refs
         && !m.refs);
   assert(record_android_capture_submit_free(e));
}

static void contracts(void)
{
   record_android_capture_submit_t *e;
   record_android_capture_submit_ops_t missing;
   record_android_capture_source_t s;
   reset();
#define MISSING(member)                                                        \
   do                                                                          \
   {                                                                           \
      missing = ops;                                                           \
      missing.member = NULL;                                                   \
      assert(!record_android_capture_submit_new(DEVICE, QUEUE, 3, DEST,        \
            VK_FORMAT_R8G8B8A8_UNORM, 128, 96, &missing));                     \
   } while (0)
   MISSING(create_pool);
   MISSING(destroy_pool);
   MISSING(allocate_commands);
   MISSING(create_fence);
   MISSING(destroy_fence);
   MISSING(create_semaphore);
   MISSING(destroy_semaphore);
   MISSING(reset_command);
   MISSING(begin);
   MISSING(end);
   MISSING(barrier);
   MISSING(copy);
   MISSING(reset_fences);
   MISSING(fence_status);
   MISSING(export_fd);
   MISSING(submit);
   MISSING(close_fd);
#undef MISSING
   e = create();
   assert(e);
   s = source();
   s.layout = VK_IMAGE_LAYOUT_UNDEFINED;
   assert(record_android_capture_submit_push(e, &s) == -1 && m.refs == 1);
   s = source();
   s.image = DEST;
   assert(record_android_capture_submit_push(e, &s) == -1 && m.refs == 1);
   s = source();
   s.ready = DONE;
   assert(record_android_capture_submit_push(e, &s) == -1 && m.refs == 1);
   s = source();
   s.x = 159;
   assert(record_android_capture_submit_push(e, &s) == -1 && m.refs == 1);
   s = source();
   s.owner = NULL;
   assert(record_android_capture_submit_push(e, &s) == -1 && m.refs == 1);
   m.refs = 0;
   assert(record_android_capture_submit_free(e));
   reset();
   e = create();
   assert(e);
   s = source();
   assert(record_android_capture_submit_push(e, &s) == 1);
   assert(!record_android_capture_submit_prepare_device_destroy(e, false));
   assert(m.pool && m.fence && m.semaphore && m.refs == 1);
   m.complete = true;
   assert(record_android_capture_submit_prepare_device_destroy(e, false));
   assert(!m.pool && !m.fence && !m.semaphore && m.refs == 1);
   m.device_alive = false;
   assert(record_android_capture_submit_after_device_destroy(e));
   assert(!m.refs && !m.fd_refs && record_android_capture_submit_free(e));
   reset();
   e = create();
   assert(e);
   s = source();
   m.deferred = true;
   assert(
         record_android_capture_submit_push(e, &s) == 1 && m.queued.count == 1);
   assert(!record_android_capture_submit_prepare_device_destroy(e, false));
   m.queued.count = 0;
   assert(record_android_capture_submit_prepare_device_destroy(e, true));
   assert(!m.pool && !m.fence && !m.semaphore && m.refs == 1);
   m.device_alive = false;
   assert(record_android_capture_submit_after_device_destroy(e));
   assert(!m.refs && record_android_capture_submit_free(e));
}

static void formats_and_layouts(void)
{
   VkFormat formats[3] = {VK_FORMAT_R8G8B8A8_UNORM,
         VK_FORMAT_A2B10G10R10_UNORM_PACK32, VK_FORMAT_R16G16B16A16_SFLOAT};
   VkImageLayout layouts[4]
         = {VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
               VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
               VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL};
   record_android_capture_submit_t *e;
   record_android_capture_source_t s;
   unsigned f, l;
   int fd;
   for (f = 0; f < 3; f++)
      for (l = 0; l < 4; l++)
      {
         reset();
         m.source_layout = layouts[l];
         e = record_android_capture_submit_new(
               DEVICE, QUEUE, 3, DEST, formats[f], 128, 96, &ops);
         assert(e);
         s = source();
         s.format = formats[f];
         assert(record_android_capture_submit_push(e, &s) == 1);
         m.complete = true;
         assert(record_android_capture_submit_poll(e) == 1);
         assert(record_android_capture_submit_take_ready(e, &fd) == 1);
         close_fd(&m, fd);
         assert(record_android_capture_submit_release(e));
         assert(record_android_capture_submit_free(e));
      }
}
struct record_gpu_dispatch_mediacodec
{
   record_gpu_queue_t *queue;
   enum record_dispatch_mediacodec_state state;
   bool cancelled;
};
int record_gpu_dispatch_mediacodec_acquire(
      record_gpu_dispatch_mediacodec_t *g, unsigned *slot)
{
   return record_gpu_queue_acquire(g->queue, slot);
}
bool record_gpu_dispatch_mediacodec_submit(
      record_gpu_dispatch_mediacodec_t *g, const record_gpu_frame_t *frame)
{
   return !g->cancelled && record_gpu_queue_submit(g->queue, frame);
}
bool record_gpu_dispatch_mediacodec_cancel_slot(
      record_gpu_dispatch_mediacodec_t *g)
{
   return record_gpu_queue_cancel(g->queue);
}
bool record_gpu_dispatch_mediacodec_finish(
      record_gpu_dispatch_mediacodec_t *g, int64_t end_us)
{
   (void)end_us;
   return record_gpu_queue_finish(g->queue);
}
void record_gpu_dispatch_mediacodec_cancel(record_gpu_dispatch_mediacodec_t *g)
{
   g->cancelled = true;
}
enum record_dispatch_mediacodec_state record_gpu_dispatch_mediacodec_status(
      const record_gpu_dispatch_mediacodec_t *g, int *error, int *system_error)
{
   (void)error;
   (void)system_error;
   return g->state;
}
static void pipe_lifetime(bool deferred, bool cancel)
{
   record_android_capture_submit_t *entry;
   record_android_capture_pipe_t *pipe;
   record_android_capture_source_t s;
   record_gpu_dispatch_mediacodec_t gpu;
   record_gpu_frame_t frame;
   reset();
   m.deferred = deferred;
   entry = create();
   assert(entry);
   memset(&gpu, 0, sizeof(gpu));
   gpu.queue = record_gpu_queue_new(1);
   gpu.state = RECORD_MC_DISPATCH_READY;
   assert(gpu.queue);
   pipe = record_android_capture_pipe_new(&gpu, &entry, 1, close_fd, &m);
   assert(pipe);
   s = source();
   assert(record_android_capture_pipe_push(pipe, &s, 0, 10) == 1);
   assert(m.refs == 1 && m.submits == (deferred ? 0u : 1u));
   if (cancel)
   {
      record_android_capture_pipe_cancel(pipe);
      assert(record_android_capture_pipe_finish(pipe, 10));
      assert(gpu.cancelled);
      gpu.state = RECORD_MC_DISPATCH_FAILED;
      assert(record_android_capture_pipe_step(pipe) == 0);
      assert(m.refs == 1 && !record_android_capture_pipe_free(pipe));
   }
   else
   {
      assert(!record_android_capture_pipe_finish(pipe, 10));
      assert(record_android_capture_pipe_step(pipe) == 0);
      if (deferred)
         assert(record_gpu_queue_take(gpu.queue, &frame)
               == RECORD_GPU_QUEUE_BUSY);
   }
   if (deferred)
      assert(
            vulkan_android_queue_flush(&m.queued, native_submit) == VK_SUCCESS);
   if (!cancel && deferred)
   {
      assert(record_android_capture_pipe_step(pipe) == 0);
      assert(m.refs == 1 && m.exports == 0 && m.submits == 1);
      m.complete = true;
      assert(record_android_capture_pipe_step(pipe) == 0);
      assert(m.refs == 0 && m.exports == 1);
   }
   if (!cancel)
   {
      assert(record_gpu_queue_take(gpu.queue, &frame) == RECORD_GPU_QUEUE_OK);
      assert(frame.ready_value == 101 && frame.timestamp == 0
            && frame.duration == 10);
      close_fd(&m, (int)frame.ready_value - 1);
      assert(record_android_capture_pipe_finish(pipe, 10));
      gpu.state = RECORD_MC_DISPATCH_DRAINING;
      assert(record_android_capture_pipe_step(pipe) == 0 && !gpu.cancelled);
      assert(!record_android_capture_pipe_free(pipe));
      assert(record_gpu_queue_release(gpu.queue, frame.slot));
   }
   gpu.state = cancel ? RECORD_MC_DISPATCH_FAILED : RECORD_MC_DISPATCH_DONE;
   if (!m.complete)
   {
      assert(record_android_capture_pipe_step(pipe) == 0);
      assert(m.refs == 1 && !record_android_capture_submit_free(entry));
      assert(!record_android_capture_pipe_free(pipe));
   }
   m.complete = true;
   assert(record_android_capture_pipe_step(pipe) == 1);
   assert(!m.refs && !m.fd_refs && m.submits == 1 && m.releases == 1);
   assert(record_android_capture_pipe_free(pipe));
   assert(record_android_capture_submit_free(entry));
   assert(record_gpu_queue_free(gpu.queue));
}
static void pipe_device_loss(void)
{
   record_android_capture_submit_t *entry;
   record_android_capture_pipe_t *pipe;
   record_android_capture_source_t s;
   record_gpu_dispatch_mediacodec_t gpu;
   reset();
   entry = create();
   assert(entry);
   memset(&gpu, 0, sizeof(gpu));
   gpu.queue = record_gpu_queue_new(1);
   gpu.state = RECORD_MC_DISPATCH_READY;
   assert(gpu.queue);
   pipe = record_android_capture_pipe_new(&gpu, &entry, 1, close_fd, &m);
   assert(pipe);
   s = source();
   assert(record_android_capture_pipe_push(pipe, &s, 0, 10) == 1);
   record_android_capture_pipe_cancel(pipe);
   assert(record_android_capture_pipe_finish(pipe, 10));
   gpu.state = RECORD_MC_DISPATCH_FAILED;
   m.fence_error = VK_ERROR_DEVICE_LOST;
   assert(record_android_capture_pipe_step(pipe) == -1);
   assert(m.refs == 1 && !record_android_capture_pipe_free(pipe));
   assert(record_android_capture_submit_prepare_device_destroy(entry, false));
   assert(m.refs == 1 && !m.pool && !m.fence && !m.semaphore);
   m.device_alive = false;
   assert(record_android_capture_submit_after_device_destroy(entry));
   assert(!m.refs && !m.fd_refs && m.gone_release);
   assert(record_android_capture_pipe_step(pipe) == 1);
   assert(record_android_capture_pipe_free(pipe));
   assert(record_android_capture_submit_free(entry));
   assert(record_gpu_queue_free(gpu.queue));
}
static void pipe_source_retirement(void)
{
   record_android_capture_submit_t *entry;
   record_android_capture_pipe_t *pipe;
   record_android_capture_source_t s;
   record_gpu_dispatch_mediacodec_t gpu;
   record_gpu_frame_t frame;
   reset();
   entry = create();
   assert(entry);
   memset(&gpu, 0, sizeof(gpu));
   gpu.queue = record_gpu_queue_new(1);
   gpu.state = RECORD_MC_DISPATCH_READY;
   assert(gpu.queue);
   pipe = record_android_capture_pipe_new(&gpu, &entry, 1, close_fd, &m);
   assert(pipe && record_android_capture_pipe_sources_retired(pipe));
   s = source();
   assert(record_android_capture_pipe_push(pipe, &s, 0, 10) == 1);
   assert(!record_android_capture_pipe_sources_retired(pipe));
   assert(record_android_capture_pipe_step(pipe) == 0);
   assert(record_gpu_queue_take(gpu.queue, &frame) == RECORD_GPU_QUEUE_OK);
   assert(record_android_capture_pipe_step(pipe) == 0 && m.refs == 1);
   assert(!record_android_capture_pipe_sources_retired(pipe));
   m.complete = true;
   assert(record_android_capture_pipe_step(pipe) == 0);
   assert(!m.refs && m.releases == 1 && m.fd_refs == 1);
   assert(record_android_capture_pipe_sources_retired(pipe));
   assert(!record_android_capture_submit_free(entry));
   assert(!record_gpu_queue_free(gpu.queue));
   assert(record_android_capture_pipe_step(pipe) == 0 && m.releases == 1);
   assert(record_android_capture_pipe_finish(pipe, 10));
   close_fd(&m, (int)frame.ready_value - 1);
   assert(record_gpu_queue_release(gpu.queue, frame.slot));
   gpu.state = RECORD_MC_DISPATCH_DONE;
   assert(record_android_capture_pipe_step(pipe) == 1);
   assert(record_android_capture_pipe_free(pipe));
   assert(record_android_capture_submit_free(entry));
   assert(record_gpu_queue_free(gpu.queue));
}
static void renderer_recording(bool deferred, bool busy)
{
   record_android_capture_submit_t *e;
   record_android_capture_source_t s;
   int fd;
   unsigned submits;
   reset();
   m.external = true;
   m.deferred = deferred;
   e = create();
   assert(e);
   s = source();
   s.ready = READY;
   assert(record_android_capture_submit_record(e, &s, RENDER_COMMAND) == -1);
   s.ready = VK_NULL_HANDLE;
   assert(record_android_capture_submit_record(e, &s, RENDER_COMMAND) == 1);
   assert(m.copies == 1 && !m.submits && m.refs == 1);
   assert(record_android_capture_submit_poll(e) == 0);
   assert(!record_android_capture_submit_free(e));
   assert(record_android_capture_submit_record(e, &s, RENDER_COMMAND) == 0);
   assert(!record_android_capture_submit_prepare_device_destroy(e, false));
   /* The retirement probe above is terminal; abort the never-submitted frame.
    */
   m.abort_proof = true;
   assert(record_android_capture_submit_abort_recorded(e));
   assert(!m.refs && record_android_capture_submit_free(e));
   reset();
   m.external = true;
   m.deferred = deferred;
   e = create();
   s = source();
   assert(record_android_capture_submit_record(e, &s, RENDER_COMMAND) == 1);
   if (busy)
      m.result = VK_TIMEOUT;
   assert(record_android_capture_submit_commit(e) == (busy ? 0 : 1));
   assert(!record_android_capture_submit_abort_recorded(e));
   assert(record_android_capture_submit_commit(e) == -1);
   m.result = VK_SUCCESS;
   assert(record_android_capture_submit_poll(e) == 0);
   if (deferred)
      assert(
            vulkan_android_queue_flush(&m.queued, native_submit) == VK_SUCCESS);
   assert(m.copies == 1 && m.submits == 1 && m.refs == 1);
   submits = m.submits;
   m.complete = true;
   assert(record_android_capture_submit_poll(e) == 1);
   assert(!m.refs && m.submits == submits && m.copies == 1);
   assert(record_android_capture_submit_take_ready(e, &fd) == 1);
   close_fd(&m, fd);
   assert(record_android_capture_submit_release(e));
   assert(record_android_capture_submit_free(e));
}

static void renderer_handoff(bool deferred, unsigned disposition)
{
   record_android_capture_submit_t *entry;
   record_android_capture_pipe_t *pipe;
   record_android_capture_source_t s;
   record_gpu_dispatch_mediacodec_t gpu;
   record_gpu_frame_t frame;
   int result;
   reset();
   m.external = true;
   m.deferred = deferred;
   entry = create();
   memset(&gpu, 0, sizeof(gpu));
   gpu.queue = record_gpu_queue_new(1);
   gpu.state = RECORD_MC_DISPATCH_READY;
   assert(entry && gpu.queue);
   pipe = record_android_capture_pipe_new(&gpu, &entry, 1, close_fd, &m);
   assert(pipe);
   s = source();
   assert(record_android_capture_pipe_record(pipe, &s, 0, 10, VK_NULL_HANDLE)
         == -1);
   assert(record_android_capture_pipe_record(pipe, &s, 0, 10, RENDER_COMMAND)
         == 1);
   assert(record_android_capture_pipe_step(pipe) == 0);
   assert(m.copies == 1 && !m.submits && m.refs == 1);
   assert(record_gpu_queue_take(gpu.queue, &frame) == RECORD_GPU_QUEUE_BUSY);
   assert(!record_android_capture_pipe_finish(pipe, 10));
   if (disposition == 1 || disposition == 2)
   {
      if (disposition == 2)
      {
         record_android_capture_pipe_cancel(pipe);
         assert(record_android_capture_pipe_finish(pipe, 10));
         gpu.state = RECORD_MC_DISPATCH_FAILED;
         assert(record_android_capture_pipe_step(pipe) == 0);
      }
      m.abort_proof = true;
      assert(record_android_capture_pipe_abort_recorded(pipe));
      assert(record_android_capture_pipe_sources_retired(pipe) && !m.refs);
      assert(record_android_capture_pipe_commit(pipe) == -1);
   }
   else
   {
      if (disposition == 3)
      {
         record_android_capture_pipe_cancel(pipe);
         assert(record_android_capture_pipe_finish(pipe, 10));
         gpu.state = RECORD_MC_DISPATCH_FAILED;
         assert(record_android_capture_pipe_step(pipe) == 0);
      }
      if (disposition == 4)
         m.result = VK_TIMEOUT;
      if (disposition == 5)
         m.result = VK_ERROR_DEVICE_LOST;
      result = record_android_capture_pipe_commit(pipe);
      assert(result
            == (disposition == 4                        ? 0
                        : disposition == 5 && !deferred ? -1
                                                        : 1));
      assert(!record_android_capture_pipe_abort_recorded(pipe));
      assert(record_android_capture_pipe_commit(pipe) == -1);
      if (disposition >= 3)
      {
         record_android_capture_pipe_cancel(pipe);
         assert(record_android_capture_pipe_finish(pipe, 10));
         gpu.state = RECORD_MC_DISPATCH_FAILED;
      }
      m.result = VK_SUCCESS;
      assert(record_android_capture_pipe_step(pipe) == 0);
      if (deferred)
      {
         if (disposition == 5)
            m.result = VK_ERROR_DEVICE_LOST;
         assert(vulkan_android_queue_flush(&m.queued, native_submit)
               == (disposition == 5 ? VK_ERROR_DEVICE_LOST : VK_SUCCESS));
      }
      assert(m.copies == 1 && m.submits == 1 && m.refs == 1);
      m.complete = true;
      if (!disposition)
      {
         assert(record_android_capture_pipe_step(pipe) == 0);
         assert(
               record_gpu_queue_take(gpu.queue, &frame) == RECORD_GPU_QUEUE_OK);
         assert(frame.timestamp == 0 && frame.duration == 10
               && frame.ready_value == 101);
         assert(record_android_capture_pipe_step(pipe) == 0);
         assert(record_android_capture_pipe_sources_retired(pipe));
         close_fd(&m, (int)frame.ready_value - 1);
         assert(record_gpu_queue_release(gpu.queue, frame.slot));
      }
   }
   assert(record_android_capture_pipe_finish(pipe, 10));
   gpu.state = disposition >= 2 ? RECORD_MC_DISPATCH_FAILED
                                : RECORD_MC_DISPATCH_DONE;
   assert(record_android_capture_pipe_step(pipe) == 1);
   assert(!m.refs && !m.fd_refs);
   assert(m.copies == 1
         && m.submits == (disposition == 1 || disposition == 2 ? 0u : 1u));
   assert(record_android_capture_pipe_free(pipe));
   assert(record_android_capture_submit_free(entry));
   assert(record_gpu_queue_free(gpu.queue));
}

static void renderer_faults(void)
{
   record_android_capture_submit_t *e;
   record_android_capture_source_t source_frame;
   unsigned i;
   for (i = 1; i <= 4; i++)
   {
      reset();
      m.external = true;
      e = create();
      source_frame = source();
      m.record_failure = i;
      assert(record_android_capture_submit_record(
                   e, &source_frame, RENDER_COMMAND)
            == -1);
      assert(!m.copies && !m.submits && m.refs == 1);
      m.refs = 0;
      assert(record_android_capture_submit_free(e));
   }
   reset();
   m.external = true;
   e = create();
   source_frame = source();
   assert(record_android_capture_submit_record(e, &source_frame, RENDER_COMMAND)
         == 1);
   m.abort_proof = true;
   assert(record_android_capture_submit_abort_recorded(e));
   source_frame = source();
   assert(record_android_capture_submit_record(e, &source_frame, RENDER_COMMAND)
         == 1);
   m.result = VK_TIMEOUT;
   assert(record_android_capture_submit_commit(e) == 0);
   assert(record_android_capture_submit_discard(e) == 0);
   assert(m.refs == 1 && !m.submits && m.copies == 1);
   m.result = VK_SUCCESS;
   assert(record_android_capture_submit_discard(e) == 0);
   assert(m.submits == 1 && m.refs == 1 && m.copies == 1);
   m.complete = true;
   assert(record_android_capture_submit_discard(e) == 1);
   assert(!m.refs && record_android_capture_submit_free(e));
   reset();
   m.external = true;
   e = create();
   source_frame = source();
   assert(record_android_capture_submit_record(e, &source_frame, RENDER_COMMAND)
         == 1);
   m.result = VK_ERROR_DEVICE_LOST;
   assert(record_android_capture_submit_commit(e) == -1);
   assert(m.refs == 1 && m.submits == 1);
   assert(record_android_capture_submit_poll(e) == -1);
   m.complete = true;
   assert(record_android_capture_submit_discard(e) == 1);
   assert(!m.refs && record_android_capture_submit_free(e));
   reset();
   m.external = true;
   e = create();
   source_frame = source();
   assert(record_android_capture_submit_record(e, &source_frame, RENDER_COMMAND)
         == 1);
   assert(record_android_capture_submit_prepare_device_destroy(e, true));
   assert(!record_android_capture_submit_abort_recorded(e));
   assert(record_android_capture_submit_poll(e) == -1);
   assert(!m.submits && m.refs == 1);
   m.device_alive = false;
   assert(record_android_capture_submit_after_device_destroy(e));
   assert(!m.refs && m.gone_release);
   assert(record_android_capture_submit_free(e));
}

int main(void)
{
   unsigned disposition;
   for (disposition = 0; disposition < 6; disposition++)
   {
      renderer_handoff(false, disposition);
      renderer_handoff(true, disposition);
   }
   assert(record_android_capture_pipe_sources_retired(NULL));
   renderer_faults();
   renderer_recording(false, false);
   renderer_recording(false, true);
   renderer_recording(true, false);
   renderer_recording(true, true);
   pipe_source_retirement();
   pipe_device_loss();
   pipe_lifetime(false, false);
   pipe_lifetime(true, false);
   pipe_lifetime(false, true);
   pipe_lifetime(true, true);
   cycles(false, false);
   cycles(false, true);
   cycles(true, false);
   cycles(true, true);
   faults();
   contracts();
   formats_and_layouts();
   assert(record_android_capture_submit_free(NULL));
   puts("PASS: Android capture copies, foreign ownership, deferred queue, "
        "source leases, SYNC_FD consumption and retirement; Vulkan mocked");
   return 0;
}
