/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <formats/image.h>
#include "../../../gfx/common/vulkan_android_submit.h"
#define H(t, n) ((t)(uintptr_t)(n))
static VkResult submit_result = VK_SUCCESS;
static int exports;
static int native, deleted, submits, barriers, export_fail, end_fail,
      submit_fail, done_fd = -1;
static VkResult fence_status = VK_SUCCESS;
static float pushed_hdr[4];
static int acquire(void *r, void **im, int *fd)
{
   assert(r == &native);
   *im = &native;
   *fd = -1;
   return 1;
}
static bool describe(void *im, android_image_view_t *v)
{
   assert(im == &native);
   v->buffer = &native;
   v->width = v->right = v->buffer_width = 64;
   v->height = v->bottom = v->buffer_height = 32;
   return true;
}
static void del_image(void *im)
{
   assert(im == &native);
   deleted++;
}
static void del_reader(void *r)
{
   assert(r == &native);
}
static VKAPI_ATTR VkResult VKAPI_CALL status(VkDevice d, VkFence f)
{
   assert(d && f);
   return fence_status;
}
static VKAPI_ATTR VkResult VKAPI_CALL reset_fences(
      VkDevice d, uint32_t n, const VkFence *f)
{
   assert(d && n == 1 && f);
   return VK_SUCCESS;
}
static VKAPI_ATTR VkResult VKAPI_CALL reset_command(
      VkCommandBuffer c, VkCommandBufferResetFlags f)
{
   assert(c && !f);
   return VK_SUCCESS;
}
static VKAPI_ATTR VkResult VKAPI_CALL begin(
      VkCommandBuffer c, const VkCommandBufferBeginInfo *p)
{
   assert(c && p);
   return VK_SUCCESS;
}
static VKAPI_ATTR VkResult VKAPI_CALL end(VkCommandBuffer c)
{
   assert(c);
   return end_fail ? VK_ERROR_OUT_OF_HOST_MEMORY : VK_SUCCESS;
}
static VKAPI_ATTR void VKAPI_CALL barrier(VkCommandBuffer c,
      VkPipelineStageFlags s, VkPipelineStageFlags d, VkDependencyFlags f,
      uint32_t nm, const VkMemoryBarrier *m, uint32_t nb,
      const VkBufferMemoryBarrier *b, uint32_t ni,
      const VkImageMemoryBarrier *im)
{
   assert(c && s && d && !f && !nm && !m && !nb && !b && ni == 1 && im);
   barriers++;
}
static VKAPI_ATTR void VKAPI_CALL bind_pipeline(
      VkCommandBuffer c, VkPipelineBindPoint b, VkPipeline p)
{
   assert(c && b == VK_PIPELINE_BIND_POINT_COMPUTE && p);
}
static VKAPI_ATTR void VKAPI_CALL bind_sets(VkCommandBuffer c,
      VkPipelineBindPoint b, VkPipelineLayout l, uint32_t first, uint32_t n,
      const VkDescriptorSet *s, uint32_t nd, const uint32_t *offset)
{
   assert(c && b == VK_PIPELINE_BIND_POINT_COMPUTE && l && !first && n == 1 && s
         && !nd && !offset);
}
static VKAPI_ATTR void VKAPI_CALL push(VkCommandBuffer c, VkPipelineLayout l,
      VkShaderStageFlags s, uint32_t offset, uint32_t bytes, const void *data)
{
   assert(c && l && s == VK_SHADER_STAGE_COMPUTE_BIT && !offset && bytes == 80
         && ((const uint32_t *)data)[0] == 64);
   memcpy(pushed_hdr, (const unsigned char *)data + 64, sizeof(pushed_hdr));
}
static VKAPI_ATTR void VKAPI_CALL dispatch(
      VkCommandBuffer c, uint32_t x, uint32_t y, uint32_t z)
{
   assert(c && x == 8 && y == 4 && z == 1);
}
static VKAPI_ATTR void VKAPI_CALL update(VkDevice d, uint32_t n,
      const VkWriteDescriptorSet *w, uint32_t nc,
      const VkCopyDescriptorSet *copy)
{
   assert(d && n == 2 && w && !nc && !copy);
}
static VKAPI_ATTR VkResult VKAPI_CALL import_fd(
      VkDevice d, const VkImportSemaphoreFdInfoKHR *p)
{
   assert(d && p->fd == -1 && p->flags == VK_SEMAPHORE_IMPORT_TEMPORARY_BIT);
   return VK_SUCCESS;
}
static VKAPI_ATTR VkResult VKAPI_CALL export_fd(
      VkDevice d, const VkSemaphoreGetFdInfoKHR *p, int *fd)
{
   assert(d && p);
   exports++;
   if (export_fail)
      return VK_ERROR_TOO_MANY_OBJECTS;
   *fd = done_fd;
   done_fd = -1;
   return VK_SUCCESS;
}
static VkResult submit(
      void *context, VkQueue q, uint32_t n, const VkSubmitInfo *p, VkFence f)
{
   assert(context == &native);
   assert(q && n == 1 && f && p->waitSemaphoreCount == 1
         && p->signalSemaphoreCount == 1 && p->commandBufferCount == 1);
   assert(*p->pWaitDstStageMask == VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
   submits++;
   fence_status = VK_NOT_READY;
   return submit_fail ? VK_ERROR_DEVICE_LOST : submit_result;
}
static vulkan_android_submission_t entry;
static void setup(const android_frame_view_t *v)
{
   memset(&entry, 0, sizeof(entry));
   entry.device = H(VkDevice, 1);
   entry.queue = H(VkQueue, 1);
   entry.command = H(VkCommandBuffer, 1);
   entry.fence = H(VkFence, 1);
   entry.image.image = H(VkImage, 2);
   entry.image.view = H(VkImageView, 2);
   entry.pipeline.pipeline = H(VkPipeline, 3);
   entry.pipeline.layout = H(VkPipelineLayout, 3);
   entry.pipeline.count = 1;
   entry.pipeline.sets[0] = H(VkDescriptorSet, 3);
   entry.sync.ready = H(VkSemaphore, 4);
   entry.sync.done = H(VkSemaphore, 5);
   entry.pipeline_ops.update_sets = update;
   entry.sync_ops.import_fd = import_fd;
   entry.sync_ops.export_fd = export_fd;
   entry.ops.fence_status = status;
   entry.ops.reset_fences = reset_fences;
   entry.ops.reset_command = reset_command;
   entry.ops.begin = begin;
   entry.ops.end = end;
   entry.ops.barrier = barrier;
   entry.ops.bind_pipeline = bind_pipeline;
   entry.ops.bind_sets = bind_sets;
   entry.ops.push = push;
   entry.ops.dispatch = dispatch;
   entry.ops.submit = submit;
   entry.ops.submit_context = &native;
   assert(vulkan_android_submission_configure(&entry, v, false));
}
int main(void)
{
   android_image_ops_t ops;
   android_image_queue_t *queue;
   android_frame_pool_t *pool;
   android_frame_view_t v;
   video_hw_frame_t f;
   int pipe_fd[2];
   memset(&ops, 0, sizeof(ops));
   ops.acquire = acquire;
   ops.describe = describe;
   ops.delete_image = del_image;
   ops.delete_reader = del_reader;
   queue = android_image_queue_new(&native, 1, &ops);
   pool = android_frame_pool_new(1);
   assert(queue && pool);
   memset(&v, 0, sizeof(v));
   v.format.serial = 1;
   v.pixfmt = IMAGE_PIXFMT_NV12;
   v.color.primaries = v.color.matrix = v.color.range = 1;
   v.color.transfer = 13;
   assert(android_image_queue_acquire(queue, &v.image) == 1);
   assert(!android_frame_export(pool, &v, &f));
   setup(&v);
   v.color.primaries = 6;
   v.color.transfer = 6;
   assert(vulkan_android_submission_configure(&entry, &v, false));
   assert(entry.header[6] == 6 && entry.header[7] == 6);
   v.color.transfer = 16;
   assert(!vulkan_android_submission_configure(&entry, &v, false));
   v.color.primaries = 5;
   assert(!vulkan_android_submission_configure(&entry, &v, false));
   v.color.transfer = 6;
   assert(vulkan_android_submission_configure(&entry, &v, false));
   assert(entry.header[6] == 5 && entry.header[7] == 6);
   v.color.transfer = 13;
   assert(vulkan_android_submission_configure(&entry, &v, true));
   assert(entry.header[6] == 5 && entry.header[7] == 13);
   v.color.primaries = 1;
   v.color.transfer = 0;
   assert(vulkan_android_submission_configure(&entry, &v, false));
   assert(entry.header[7] == 0);
   assert(!vulkan_android_submission_configure(&entry, &v, true));
   v.color.transfer = 2;
   assert(vulkan_android_submission_configure(&entry, &v, false));
   assert(entry.header[7] == 2);
   v.pixfmt = IMAGE_PIXFMT_P010;
   assert(!vulkan_android_submission_configure(&entry, &v, false));
   v.pixfmt = IMAGE_PIXFMT_NV12;
   v.color.max_cll = 1000;
   assert(!vulkan_android_submission_configure(&entry, &v, false));
   v.color.max_cll = 0;
   v.color.transfer = 13;
   setup(&v);
   entry.source.color.range = 2;
   assert(vulkan_android_submission_present(&entry, &f, H(VkImage, 9),
                H(VkImageView, 9), VK_IMAGE_LAYOUT_UNDEFINED)
               == -1
         && !submits);
   entry.source.color.range = 1;
   end_fail = 1;
   assert(vulkan_android_submission_present(&entry, &f, H(VkImage, 9),
                H(VkImageView, 9), VK_IMAGE_LAYOUT_UNDEFINED)
               == -1
         && !submits);
   end_fail = 0;
   assert(pipe(pipe_fd) == 0);
   done_fd = pipe_fd[0];
   assert(vulkan_android_submission_present(&entry, &f, H(VkImage, 9),
                H(VkImageView, 9), VK_IMAGE_LAYOUT_UNDEFINED)
         == 1);
   assert(submits == 1 && !entry.held_owner && entry.inflight
         && !android_image_queue_poll(queue));
   assert(vulkan_android_submission_poll(&entry) == 0);
   assert(write(pipe_fd[1], "x", 1) == 1);
   fence_status = VK_SUCCESS;
   assert(vulkan_android_submission_poll(&entry) == 1);
   assert(android_image_queue_poll(queue) == 1);
   close(pipe_fd[1]);
   assert(android_image_queue_acquire(queue, &v.image) == 1);
   assert(!android_frame_export(pool, &v, &f));
   export_fail = 1;
   assert(vulkan_android_submission_present(&entry, &f, H(VkImage, 9),
                H(VkImageView, 9), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
         == 1);
   assert(entry.held_owner && !android_image_queue_poll(queue));
   assert(vulkan_android_submission_poll(&entry) == 0);
   export_fail = 0;
   fence_status = VK_SUCCESS;
   assert(vulkan_android_submission_poll(&entry) == 1 && !entry.held_owner);
   assert(android_image_queue_poll(queue) == 1);
   assert(android_image_queue_acquire(queue, &v.image) == 1);
   assert(!android_frame_export(pool, &v, &f));
   export_fail = 1;
   assert(vulkan_android_submission_present(&entry, &f, H(VkImage, 9),
                H(VkImageView, 9), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
         == 1);
   fence_status = VK_SUCCESS;
   assert(vulkan_android_submission_poll(&entry) == -1 && entry.broken
         && !entry.inflight && !entry.held_owner);
   assert(android_image_queue_poll(queue) == 1 && deleted == 3
         && barriers == 16);
   assert(android_image_queue_acquire(queue, &v.image) == 1);
   assert(!android_frame_export(pool, &v, &f));
   setup(&v);
   submit_fail = 1;
   assert(vulkan_android_submission_present(&entry, &f, H(VkImage, 9),
                H(VkImageView, 9), VK_IMAGE_LAYOUT_UNDEFINED)
         == -2);
   video_hw_frame_abandon(&f);
   assert(entry.held_owner && !android_image_queue_poll(queue));
   /* Mock cold device destruction: no submitted GPU work can remain afterward.
    */
   assert(vulkan_android_submission_after_device_destroy(&entry));
   assert(!entry.device && !entry.held_owner);
   assert(android_image_queue_poll(queue) == 1 && deleted == 4);
   assert(android_image_queue_acquire(queue, &v.image) == 1);
   assert(!android_frame_export(pool, &v, &f));
   setup(&v);
   submit_fail = export_fail = 0;
   submit_result = VK_TIMEOUT;
   exports = 0;
   assert(vulkan_android_submission_present(&entry, &f, H(VkImage, 9),
                H(VkImageView, 9), VK_IMAGE_LAYOUT_UNDEFINED) == 0);
   assert(!entry.held_owner && !entry.inflight && !entry.broken && !exports);
   submit_result = VK_NOT_READY;
   assert(vulkan_android_submission_present(&entry, &f, H(VkImage, 9),
                H(VkImageView, 9), VK_IMAGE_LAYOUT_UNDEFINED) == 1);
   assert(entry.held_owner && entry.inflight && !exports);
   video_hw_frame_abandon(&f);
   assert(vulkan_android_submission_poll(&entry) == 0 && !exports);
   assert(!android_image_queue_poll(queue));
   fence_status = VK_SUCCESS;
   assert(vulkan_android_submission_poll(&entry) == 1 && exports == 1);
   assert(!entry.held_owner && android_image_queue_poll(queue) == 1);
   submit_result = VK_SUCCESS;
   android_frame_pool_close(pool);
   assert(android_frame_pool_free(pool));
   android_image_queue_close(queue);
   assert(android_image_queue_free(queue));
   puts("PASS: warm Android Vulkan submission, refusal retry, GPU fence "
        "retention and failed binary-signal export retirement");
   return 0;
}
