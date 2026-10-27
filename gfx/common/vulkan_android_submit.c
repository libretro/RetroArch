/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include "vulkan_android_submit.h"
#include "video_import_color.h"
#include <formats/image.h>
#include <string.h>
#include <unistd.h>
bool vulkan_android_submission_configure(vulkan_android_submission_t *e,
      const android_frame_view_t *source, bool fp16)
{
   unsigned yuv = 0;
   float coefficients[6], hdr[4];
   /* Coded RGBA8 needs no inferred EOTF; linear output does. */
   bool encoded = source && !fp16 && source->pixfmt == IMAGE_PIXFMT_NV12
         && source->color.primaries == 1
         && (!source->color.transfer || source->color.transfer == 2)
         && !source->color.max_cll && !source->color.max_fall
         && !source->color.mastering.present;
   if (!e || !source || e->inflight || e->held_owner || e->broken
         || !source->image.buffer || !source->image.buffer_width
         || !source->image.buffer_height
         || source->image.left >= source->image.right
         || source->image.top >= source->image.bottom
         || source->image.right > source->image.buffer_width
         || source->image.bottom > source->image.buffer_height
         || (!encoded && source->color.transfer != 1 && source->color.transfer != 6
               && source->color.transfer != 13 && source->color.transfer != 14
               && source->color.transfer != 15 && source->color.transfer != 16
               && source->color.transfer != 18)
         || (source->color.primaries != 1 && source->color.primaries != 5
               && source->color.primaries != 6
               && source->color.primaries != 9)
         || ((source->color.primaries == 5 || source->color.primaries == 6)
               && source->color.transfer != 1
               && source->color.transfer != 6 && source->color.transfer != 13)
         || ((source->color.transfer != 16 && source->color.transfer != 18)
               && source->color.primaries == 9)
         || !video_import_color(&yuv, &source->color, coefficients, hdr))
      return false;
   memset(e->header, 0, sizeof(e->header));
   e->header[0] = source->image.right - source->image.left;
   e->header[1] = source->image.bottom - source->image.top;
   e->header[2] = source->image.left;
   e->header[3] = source->image.top;
   e->header[4] = source->image.buffer_width;
   e->header[5] = source->image.buffer_height;
   e->header[6] = source->color.primaries;
   e->header[7] = source->color.transfer;
   if (fp16)
      hdr[3] = -1.0f;
   memcpy(e->header + 16, hdr, sizeof(hdr));
   e->source = *source;
   return true;
}
int vulkan_android_submission_poll(vulkan_android_submission_t *e)
{
   VkResult status;
   if (!e || e->broken || !e->ops.fence_status)
      return -1;
   if (!e->inflight)
      return 1;
   status = e->ops.fence_status(e->device, e->fence);
   if (status == VK_NOT_READY)
      return 0;
   if (status != VK_SUCCESS)
   {
      e->broken = 1;
      return -1;
   }
   if (e->held_owner)
   {
      int fd;
      bool exported = vulkan_android_sync_export(
            e->device, &e->sync_ops, e->sync.done, &fd);
      if (exported && fd >= 0)
         close(fd);
      if (!android_frame_finish(
                e->held_owner, e->held_slot, e->held_generation, -1))
      {
         e->broken = 1;
         return -1;
      }
      e->held_owner = NULL;
      /* Without consuming the binary signal payload, this entry cannot signal
       * done again. Fence proof permits image return, but not semaphore reuse.
       */
      if (!exported)
      {
         e->inflight = 0;
         e->broken = 1;
         return -1;
      }
   }
   e->inflight = 0;
   return 1;
}
static void vulkan_android_destination_barrier(vulkan_android_submission_t *e,
      VkImage image, VkImageLayout old, VkImageLayout next, bool before)
{
   VkImageMemoryBarrier b;
   memset(&b, 0, sizeof(b));
   b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
   b.image = image;
   b.oldLayout = old;
   b.newLayout = next;
   b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
   b.srcAccessMask = before ? (old == VK_IMAGE_LAYOUT_UNDEFINED ? 0
                                          : old == VK_IMAGE_LAYOUT_GENERAL
                                          ? (VK_ACCESS_MEMORY_READ_BIT
                                                  | VK_ACCESS_MEMORY_WRITE_BIT)
                                          : VK_ACCESS_SHADER_READ_BIT)
                            : VK_ACCESS_SHADER_WRITE_BIT;
   b.dstAccessMask
         = before ? VK_ACCESS_SHADER_WRITE_BIT : VK_ACCESS_SHADER_READ_BIT;
   b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
   b.subresourceRange.levelCount = b.subresourceRange.layerCount = 1;
   e->ops.barrier(e->command,
         before ? VK_PIPELINE_STAGE_ALL_COMMANDS_BIT
                : VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
         before ? VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
                : VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
         0, 0, NULL, 0, NULL, 1, &b);
}
bool vulkan_android_submission_after_device_destroy(
      vulkan_android_submission_t *e)
{
   if (!e)
      return true;
   if (e->held_owner
         && !android_frame_finish(
               e->held_owner, e->held_slot, e->held_generation, -1))
      return false;
   e->held_owner = NULL;
   e->inflight = 0;
   e->broken = 1;
   e->device = VK_NULL_HANDLE;
   e->queue = VK_NULL_HANDLE;
   e->command = VK_NULL_HANDLE;
   e->fence = VK_NULL_HANDLE;
   memset(&e->image, 0, sizeof(e->image));
   memset(&e->pipeline, 0, sizeof(e->pipeline));
   memset(&e->sync, 0, sizeof(e->sync));
   return true;
}
int vulkan_android_submission_present(vulkan_android_submission_t *e,
      const video_hw_frame_t *frame, VkImage destination, VkImageView view,
      VkImageLayout layout)
{
   android_frame_view_t source;
   VkCommandBufferBeginInfo begin;
   VkSubmitInfo submit;
   VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
   VkResult result;
   int status, fd;
   if (!e || !frame || !android_frame_callback(frame->abandon) || !destination
         || !view
         || (layout != VK_IMAGE_LAYOUT_UNDEFINED
               && layout != VK_IMAGE_LAYOUT_GENERAL
               && layout != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
         || !e->device || !e->queue || !e->command || !e->fence
         || !e->image.image || !e->image.view || !e->pipeline.pipeline
         || !e->pipeline.count || !e->sync.ready || !e->sync.done
         || !e->ops.reset_fences || !e->ops.reset_command || !e->ops.begin
         || !e->ops.end || !e->ops.barrier || !e->ops.bind_pipeline
         || !e->ops.bind_sets || !e->ops.push || !e->ops.dispatch
         || !e->ops.submit || e->family >= VK_QUEUE_FAMILY_FOREIGN_EXT)
      return -1;
   status = vulkan_android_submission_poll(e);
   if (status <= 0)
      return status;
   if (!android_frame_claim(frame->owner, frame->done, frame->done_value,
             frame->image, &source))
      return -1;
   if (!android_frame_resolve_color(&source, &frame->color))
      goto refuse;
   if (source.image.buffer != e->source.image.buffer
         || source.image.buffer_width != e->source.image.buffer_width
         || source.image.buffer_height != e->source.image.buffer_height
         || source.image.left != e->source.image.left
         || source.image.top != e->source.image.top
         || source.image.right != e->source.image.right
         || source.image.bottom != e->source.image.bottom
         || source.pixfmt != e->source.pixfmt
         || memcmp(&source.color, &e->source.color, sizeof(source.color)))
      goto refuse;
   if (!vulkan_android_pipeline_update(
             e->device, &e->pipeline_ops, &e->pipeline, 0, e->image.view, view)
         || !vulkan_android_sync_import(
               e->device, &e->sync_ops, e->sync.ready, source.image.ready_fd)
         || e->ops.reset_command(e->command, 0) != VK_SUCCESS)
      goto refuse;
   memset(&begin, 0, sizeof(begin));
   begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
   if (e->ops.begin(e->command, &begin) != VK_SUCCESS)
      goto refuse;
   vulkan_android_image_barrier(e->command, e->ops.barrier, e->image.image,
         e->family, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, true);
   vulkan_android_destination_barrier(
         e, destination, layout, VK_IMAGE_LAYOUT_GENERAL, true);
   e->ops.bind_pipeline(
         e->command, VK_PIPELINE_BIND_POINT_COMPUTE, e->pipeline.pipeline);
   e->ops.bind_sets(e->command, VK_PIPELINE_BIND_POINT_COMPUTE,
         e->pipeline.layout, 0, 1, e->pipeline.sets, 0, NULL);
   e->ops.push(e->command, e->pipeline.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
         sizeof(e->header), e->header);
   e->ops.dispatch(
         e->command, (e->header[0] + 7) / 8, (e->header[1] + 7) / 8, 1);
   vulkan_android_destination_barrier(e, destination, VK_IMAGE_LAYOUT_GENERAL,
         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, false);
   vulkan_android_image_barrier(e->command, e->ops.barrier, e->image.image,
         e->family, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, false);
   if (e->ops.end(e->command) != VK_SUCCESS
         || e->ops.reset_fences(e->device, 1, &e->fence) != VK_SUCCESS)
      goto refuse;
   memset(&submit, 0, sizeof(submit));
   submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
   submit.commandBufferCount = 1;
   submit.pCommandBuffers = &e->command;
   submit.waitSemaphoreCount = 1;
   submit.pWaitSemaphores = &e->sync.ready;
   submit.pWaitDstStageMask = &wait_stage;
   submit.signalSemaphoreCount = 1;
   submit.pSignalSemaphores = &e->sync.done;
   e->held_owner = frame->owner;
   e->held_slot = frame->done;
   e->held_generation = frame->done_value;
   e->inflight = 1;
   result = e->ops.submit(
         e->ops.submit_context, e->queue, 1, &submit, e->fence);
   if (result == VK_NOT_READY)
      return 1;
   if (result == VK_TIMEOUT)
   {
      e->held_owner = NULL;
      e->inflight = 0;
      if (!android_frame_unclaim(frame->owner, frame->done, frame->done_value))
      {
         e->broken = 1;
         return -2;
      }
      return 0;
   }
   if (result != VK_SUCCESS)
   {
      e->broken = 1;
      return -2;
   }
   if (vulkan_android_sync_export(e->device, &e->sync_ops, e->sync.done, &fd))
   {
      if (!android_frame_finish(
                e->held_owner, e->held_slot, e->held_generation, fd))
      {
         if (fd >= 0)
            close(fd);
         e->broken = 1;
         return -2;
      }
      e->held_owner = NULL;
   }
   /* An export failure retains the frame until a later fence query proves done.
    */
   return 1;
refuse:
   if (!android_frame_unclaim(frame->owner, frame->done, frame->done_value))
   {
      e->broken = 1;
      return -2;
   }
   return -1;
}
