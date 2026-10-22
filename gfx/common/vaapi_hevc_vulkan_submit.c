/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "vaapi_hevc_vulkan_submit.h"
bool vaapi_hevc_vulkan_submission_configure(vaapi_hevc_vulkan_submission_t *e,
      vaapi_hevc_vulkan_frame_t *frame, unsigned x, unsigned y, unsigned width,
      unsigned height, unsigned yuv, const video_color_metadata_t *color, bool fp16)
{
   vulkan_video_layout_t layout;
   unsigned pixfmt;
   if (!e || !frame || !frame->active || frame->pending || frame->broken
         || (e->frame && (e->frame->pending || e->frame->broken))
         || !width || !height || ((x | y) & 1)
         || x >= frame->source.descriptor.width || y >= frame->source.descriptor.height
         || width > frame->source.descriptor.width - x || height > frame->source.descriptor.height - y)
      return false;
   pixfmt = frame->source.descriptor.fourcc == VA_FOURCC_P010 ? IMAGE_PIXFMT_P010 : IMAGE_PIXFMT_NV12;
   if (!vulkan_video_layout(&layout, width, height, pixfmt, yuv)
         || !vulkan_video_color(&layout, yuv, color))
      return false;
   layout.header[2] = x;
   layout.header[3] = y;
   if (fp16)
   {
      float linear = -1.0f;
      memcpy(&layout.header[19], &linear, sizeof(linear));
   }
   memcpy(e->header, layout.header, sizeof(e->header));
   e->frame = frame;
   e->pool = frame->source.pool;
   e->ticket = frame->source.ticket;
   e->slot = frame->source.slot;
   e->device = frame->device;
   e->output_format = fp16 ? VK_FORMAT_R16G16B16A16_SFLOAT : VK_FORMAT_R8G8B8A8_UNORM;
   return true;
}
static void vaapi_hevc_vulkan_destination(vaapi_hevc_vulkan_submission_t *e,
      VkImage image, VkImageLayout old, bool before)
{
   VkImageMemoryBarrier barrier;
   memset(&barrier, 0, sizeof(barrier));
   barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
   barrier.image = image;
   barrier.oldLayout = before ? old : VK_IMAGE_LAYOUT_GENERAL;
   barrier.newLayout = before ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
   barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
   barrier.srcAccessMask = before ? (old == VK_IMAGE_LAYOUT_UNDEFINED ? 0 :
         old == VK_IMAGE_LAYOUT_GENERAL ? VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT : VK_ACCESS_SHADER_READ_BIT)
      : VK_ACCESS_SHADER_WRITE_BIT;
   barrier.dstAccessMask = before ? VK_ACCESS_SHADER_WRITE_BIT : VK_ACCESS_SHADER_READ_BIT;
   barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
   barrier.subresourceRange.levelCount = barrier.subresourceRange.layerCount = 1;
   e->ops.barrier(e->command, before ? VK_PIPELINE_STAGE_ALL_COMMANDS_BIT : VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
         before ? VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT : VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
         0, 0, NULL, 0, NULL, 1, &barrier);
}
int vaapi_hevc_vulkan_submission_present(vaapi_hevc_vulkan_submission_t *e,
      VkImage destination, VkImageView view, VkFormat format, unsigned width,
      unsigned height, VkImageLayout layout)
{
   VkDescriptorImageInfo images[3];
   VkWriteDescriptorSet writes[3];
   VkCommandBufferBeginInfo begin;
   VkSubmitInfo submit;
   unsigned i;
   int status;
   if (!e || !e->frame || !e->frame->active || !destination || !view
         || !e->device || e->device != e->frame->device || !e->queue || !e->command || !e->fence
         || !e->pipeline || !e->layout || !e->set || !e->sampler || e->family >= VK_QUEUE_FAMILY_FOREIGN_EXT
         || width != e->header[0] || height != e->header[1] || format != e->output_format
         || destination == e->frame->layers[0].image || destination == e->frame->layers[1].image
         || view == e->frame->layers[0].view || view == e->frame->layers[1].view
         || (layout != VK_IMAGE_LAYOUT_UNDEFINED && layout != VK_IMAGE_LAYOUT_GENERAL
            && layout != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
         || e->pool != e->frame->source.pool || e->slot != e->frame->source.slot || e->ticket != e->frame->source.ticket
         || !e->ops.fence_status || !e->ops.reset_fences || !e->ops.reset_command || !e->ops.begin || !e->ops.end
         || !e->ops.update || !e->ops.barrier || !e->ops.bind_pipeline || !e->ops.bind_sets || !e->ops.push
         || !e->ops.dispatch || !e->ops.submit)
      return -1;
   status = vaapi_hevc_vulkan_frame_poll(e->frame, e->ops.fence_status);
   if (status <= 0)
      return status;
   memset(images, 0, sizeof(images));
   memset(writes, 0, sizeof(writes));
   for (i = 0; i < 3; i++)
   {
      images[i].imageView = i < 2 ? e->frame->layers[i].view : view;
      images[i].sampler = i < 2 ? e->sampler : VK_NULL_HANDLE;
      images[i].imageLayout = i < 2 ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_GENERAL;
      writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      writes[i].dstSet = e->set;
      writes[i].dstBinding = i + 1;
      writes[i].descriptorCount = 1;
      writes[i].descriptorType = i < 2 ? VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER : VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
      writes[i].pImageInfo = &images[i];
   }
   e->ops.update(e->device, 3, writes, 0, NULL);
   if (e->ops.reset_command(e->command, 0) != VK_SUCCESS)
      return -1;
   memset(&begin, 0, sizeof(begin));
   begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
   begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
   if (e->ops.begin(e->command, &begin) != VK_SUCCESS)
      return -1;
   if (!vaapi_hevc_vulkan_frame_barrier(e->frame, e->command, e->ops.barrier, e->family,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, true))
      return -1;
   vaapi_hevc_vulkan_destination(e, destination, layout, true);
   e->ops.bind_pipeline(e->command, VK_PIPELINE_BIND_POINT_COMPUTE, e->pipeline);
   e->ops.bind_sets(e->command, VK_PIPELINE_BIND_POINT_COMPUTE, e->layout, 0, 1, &e->set, 0, NULL);
   e->ops.push(e->command, e->layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(e->header), e->header);
   e->ops.dispatch(e->command, (width + 7) / 8, (height + 7) / 8, 1);
   vaapi_hevc_vulkan_destination(e, destination, layout, false);
   if (!vaapi_hevc_vulkan_frame_barrier(e->frame, e->command, e->ops.barrier, e->family,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, false)
         || e->ops.end(e->command) != VK_SUCCESS || e->ops.reset_fences(e->device, 1, &e->fence) != VK_SUCCESS)
      return -1;
   memset(&submit, 0, sizeof(submit));
   submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
   submit.commandBufferCount = 1;
   submit.pCommandBuffers = &e->command;
   if (!vaapi_hevc_vulkan_frame_track(e->frame, e->fence))
      return -1;
   if (e->ops.submit(e->ops.context, e->queue, 1, &submit, e->fence) != VK_SUCCESS)
   {
      e->frame->broken = true;
      return -2;
   }
   return 1;
}
