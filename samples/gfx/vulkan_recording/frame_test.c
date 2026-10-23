/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#ifdef NDEBUG
#error Assertions are required
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define VK_NO_PROTOTYPES
#include "record/record_android_capture_renderer.h"
#include "gfx/video_defines.h"
/* FLAGS */
#define QUEUE ((VkQueue)(uintptr_t)1)
#define COMMAND ((VkCommandBuffer)(uintptr_t)2)
#define IMAGE ((VkImage)(uintptr_t)3)
#define FENCE ((VkFence)(uintptr_t)4)
struct vk_image
{
   VkImage image;
};
typedef struct video_frame_info
{
   void *recording_gpu;
   uint64_t recording_gpu_timestamp;
   bool recording_gpu_capture, frame_repeat;
} video_frame_info_t;
struct context
{
   VkQueue queue;
   void *queue_lock;
   VkFence swapchain_fences[2];
   VkFormat swapchain_format;
   unsigned flags;
   bool swapchain_fences_signalled[2];
};
typedef struct vk
{
   struct context *context;
   VkCommandBuffer cmd;
   struct
   {
      unsigned pos, dims, full_dims;
   } vp;
   unsigned flags;
} vk_t;
static struct
{
   VkResult end_result, submit_result, callback_result;
   const VkSubmitInfo *submit;
   unsigned records, callbacks, submits, ends, presents, services, checks;
   unsigned mode, releases;
   bool queue_locked, accept, lease, menu;
} m;
static int record(void *user, const record_android_capture_source_t *s,
      VkCommandBuffer command, uint64_t pts, unsigned dims, unsigned mode)
{
   assert(user == &m && !m.queue_locked && command == COMMAND);
   assert(s->image == IMAGE && s->format == VK_FORMAT_R8G8B8A8_UNORM);
   assert(s->layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL && !s->ready);
   assert(s->width == 640 && s->height == 480);
   assert(s->x == (m.menu ? 0 : 8) && s->y == (m.menu ? 0 : 4));
   assert(s->owner == &m && s->release);
   assert(dims == (m.menu ? VIDEO_SCALE_PACK(640, 480)
         : VIDEO_SCALE_PACK(320, 240)) && pts == 123456);
   assert(mode == m.mode);
   m.records++;
   if (m.accept)
   {
      assert(!m.lease);
      m.lease = true;
   }
   return m.accept ? 1 : 0;
}
static void released(void *user, bool destroyed)
{
   assert(user == &m && m.lease && destroyed);
   m.lease = false;
   m.releases++;
}
static void submitted(void *user, VkResult result)
{
   assert(user == &m && m.lease);
   if (result == VK_SUCCESS)
      assert(m.queue_locked && m.submits == 1 && !m.presents);
   m.callback_result = result;
   m.callbacks++;
}
static void service(void *user)
{
   assert(user == &m && m.queue_locked && !m.submits);
   m.services++;
}
static VkResult vkQueueSubmit(
      VkQueue queue, uint32_t count, const VkSubmitInfo *submits, VkFence fence)
{
   assert(m.queue_locked && queue == QUEUE && count == 1 && fence == FENCE);
   assert(submits == m.submit && !m.presents);
   assert(submits->commandBufferCount == 2 && submits->waitSemaphoreCount == 1
         && submits->signalSemaphoreCount == 1);
   m.submits++;
   return m.submit_result;
}
static VkResult vkEndCommandBuffer(VkCommandBuffer command)
{
   assert(command == COMMAND && !m.queue_locked && !m.submits);
   m.ends++;
   return m.end_result;
}
static void vulkan_check_device_lost(vk_t *vk, VkResult result)
{
   assert(vk && (result == m.end_result || result == m.submit_result));
   m.checks++;
}
static void slock_unlock(void *lock)
{
   assert(lock == &m && m.queue_locked);
   m.queue_locked = false;
}
/* FUNCTIONS */
static bool frame(vk_t *vk, video_frame_info_t *video_info,
      const struct vk_image *image, const VkSubmitInfo *input)
{
   record_android_capture_renderer_t *recording;
   VkResult recording_result;
   VkSubmitInfo submit_info = *input;
   unsigned frame_index = 0;
   recording = vulkan_record_gpu(vk, video_info, image);
   /* END_FRAME */
   m.queue_locked = true;
   m.submit = &submit_info;
   /* SUBMIT */
   slock_unlock(&m);
   m.presents++;
   return true;
}
static void reset(vk_t *vk, struct context *ctx, video_frame_info_t *info,
      record_android_capture_renderer_t *binding)
{
   memset(&m, 0, sizeof(m));
   memset(vk, 0, sizeof(*vk));
   memset(ctx, 0, sizeof(*ctx));
   memset(info, 0, sizeof(*info));
   memset(binding, 0, sizeof(*binding));
   m.accept = true;
   m.end_result = m.submit_result = VK_SUCCESS;
   vk->context = ctx;
   vk->cmd = COMMAND;
   vk->vp.pos = VIDEO_POS_PACK(8, 4);
   vk->vp.dims = VIDEO_SCALE_PACK(320, 240);
   vk->vp.full_dims = VIDEO_SCALE_PACK(640, 480);
   ctx->queue = QUEUE;
   ctx->queue_lock = &m;
   ctx->swapchain_fences[0] = FENCE;
   ctx->swapchain_format = VK_FORMAT_R8G8B8A8_UNORM;
   ctx->flags = VK_CTX_FLAG_ANDROID_IMAGES | VK_CTX_FLAG_HAS_ACQUIRED_SWAPCHAIN;
   binding->context = &m;
   binding->record = record;
   binding->submitted = submitted;
   binding->release = released;
   binding->service = service;
   info->recording_gpu = binding;
   info->recording_gpu_capture = true;
   info->recording_gpu_timestamp = 123456;
}
int main(void)
{
   vk_t vk;
   struct context ctx;
   video_frame_info_t info;
   record_android_capture_renderer_t binding;
   struct vk_image image;
   VkSubmitInfo input;
   unsigned i;
   memset(&input, 0, sizeof(input));
   input.commandBufferCount = 2;
   input.waitSemaphoreCount = input.signalSemaphoreCount = 1;
   image.image = IMAGE;
   for (i = 0; i < 3; i++)
   {
      reset(&vk, &ctx, &info, &binding);
      if (i)
         ctx.flags |= VK_CTX_FLAG_HDR_ENABLE;
      if (i == 2)
         ctx.flags |= VK_CTX_FLAG_HDR_SCRGB;
#ifdef VULKAN_HDR_SWAPCHAIN
      m.mode = i;
#endif
      assert(frame(&vk, &info, &image, &input));
      assert(m.records == 1 && m.callbacks == 1 && m.services == 1);
      assert(m.ends == 1 && m.submits == 1 && m.presents == 1);
      assert(m.callback_result == VK_SUCCESS && !m.releases && m.lease);
      assert(ctx.swapchain_fences_signalled[0]);
      binding.release(binding.context, true);
   }
   /* A repeated core frame can still contain changing menu media. Capture the
    * whole composed window, independently of the dummy core viewport. */
   reset(&vk, &ctx, &info, &binding);
   binding.flags = RECORD_ANDROID_CAPTURE_MENU;
   m.menu = true;
   info.frame_repeat = true;
   vk.vp.pos = VIDEO_POS_PACK(-8, -4);
   assert(frame(&vk, &info, &image, &input));
   assert(m.records == 1 && m.callbacks == 1 && m.lease);
   binding.release(binding.context, true);
   for (i = 0; i < 16; i++)
   {
      reset(&vk, &ctx, &info, &binding);
      image.image = IMAGE;
      switch (i)
      {
         case 0:
            info.recording_gpu = NULL;
            break;
         case 1:
            binding.context = NULL;
            break;
         case 2:
            binding.record = NULL;
            break;
         case 3:
            binding.release = NULL;
            break;
         case 4:
            binding.submitted = NULL;
            break;
         case 5:
            info.recording_gpu_capture = false;
            break;
         case 6:
            info.frame_repeat = true;
            break;
         case 7:
            vk.cmd = VK_NULL_HANDLE;
            break;
         case 8:
            ctx.flags &= ~VK_CTX_FLAG_HAS_ACQUIRED_SWAPCHAIN;
            break;
         case 9:
            ctx.flags &= ~VK_CTX_FLAG_ANDROID_IMAGES;
            break;
         case 10:
            ctx.flags |= VK_CTX_FLAG_SWAP_INTERVAL_EMULATION_LOCK;
            break;
         case 11:
            vk.flags |= VK_FLAG_OPEN_XR;
            break;
         case 12:
            vk.vp.pos = VIDEO_POS_PACK(-1, 0);
            break;
         case 13:
            image.image = VK_NULL_HANDLE;
            break;
         case 14:
            vk.vp.dims = VIDEO_SCALE_PACK(0, 240);
            break;
         case 15:
            vk.vp.pos = VIDEO_POS_PACK(0, -1);
            break;
      }
      assert(!vulkan_record_gpu(&vk, &info, &image));
      assert(!m.records && !m.callbacks && !m.submits && !m.releases);
   }
   reset(&vk, &ctx, &info, &binding);
   m.accept = false;
   assert(frame(&vk, &info, &image, &input));
   assert(m.records == 1 && !m.callbacks && !m.lease && m.presents == 1);
   for (i = 0; i < 2; i++)
   {
      reset(&vk, &ctx, &info, &binding);
      m.submit_result = i ? VK_ERROR_OUT_OF_HOST_MEMORY : VK_ERROR_DEVICE_LOST;
      ctx.swapchain_fences_signalled[0] = true;
      assert(!frame(&vk, &info, &image, &input));
      assert(m.submits == 1 && m.callbacks == 1 && !m.presents);
      assert(m.callback_result == m.submit_result && !m.queue_locked);
      assert(!ctx.swapchain_fences_signalled[0] && m.lease && !m.releases);
      binding.release(binding.context, true);
   }
   reset(&vk, &ctx, &info, &binding);
   m.end_result = VK_ERROR_OUT_OF_DEVICE_MEMORY;
   assert(!frame(&vk, &info, &image, &input));
   assert(m.ends == 1 && !m.submits && !m.presents && !m.services);
   assert(m.callbacks == 1 && m.callback_result == m.end_result && m.lease);
   binding.release(binding.context, true);
   reset(&vk, &ctx, &info, &binding);
   info.recording_gpu = NULL;
   assert(frame(&vk, &info, &image, &input));
   assert(!m.records && !m.callbacks && !m.services && m.presents == 1);
   puts("PASS: shipping Vulkan capture guards, source metadata, accepted queue "
        "ordering and failure retention; native queue mocked");
   return 0;
}
