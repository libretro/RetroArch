/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#define VK_NO_PROTOTYPES
#define RARCH_ERR hevc_render_error
#include "import_test.h"
#include "hevc_render.h"

void hevc_render_close(void *opaque)
{
   import_oracle_t *t = (import_oracle_t*)opaque;
   if (!t) return;
   vkDeviceWaitIdle(t->vk.context->device);
   oracle_cleanup(t);
   free(t);
}

void *hevc_render_open(vulkan_context_t *ctx, unsigned width, unsigned height)
{
   import_oracle_t *t = (import_oracle_t*)calloc(1, sizeof(*t));
   VkImageViewCreateInfo view = {0};
   VkCommandPoolCreateInfo pool = {0};
   VkCommandBufferAllocateInfo command = {0};
   VkFenceCreateInfo fence = {0};
   video_device_info_t advertised;
   if (!t) return NULL;
   t->vk.context = ctx; t->width = width; t->height = height;
   t->output.dims = VIDEO_SCALE_PACK(width, height);
   t->output.format = VK_FORMAT_R8G8B8A8_UNORM;
   if (!oracle_buffer(t, &t->readback, &t->readback_memory, &t->mapped_readback,
            (size_t)width * height * 4, VK_BUFFER_USAGE_TRANSFER_DST_BIT)
         || !oracle_image(t, &t->output.image, &t->output_memory, t->output.format,
            width, height, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
               | VK_IMAGE_USAGE_TRANSFER_SRC_BIT)) goto fail;
   view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
   view.image = t->output.image; view.viewType = VK_IMAGE_VIEW_TYPE_2D;
   view.format = t->output.format; view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
   view.subresourceRange.levelCount = view.subresourceRange.layerCount = 1;
   if (vkCreateImageView(ctx->device, &view, NULL, &t->output.view) != VK_SUCCESS
         || !oracle_pipeline(t)
         || !vulkan_get_device(&t->vk, &advertised)
         || advertised.api != VIDEO_DEVICE_VULKAN
         || advertised.import_formats != (IMAGE_PIXFMT_NV12 | IMAGE_PIXFMT_P010)) goto fail;
   pool.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
   pool.queueFamilyIndex = ctx->graphics_queue_index;
   pool.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
   if (vkCreateCommandPool(ctx->device, &pool, NULL, &t->vk.staging_pool) != VK_SUCCESS) goto fail;
   command.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
   command.commandPool = t->vk.staging_pool;
   command.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; command.commandBufferCount = 1;
   fence.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
   if (vkAllocateCommandBuffers(ctx->device, &command, &t->cb) != VK_SUCCESS
         || vkCreateFence(ctx->device, &fence, NULL, &t->fence) != VK_SUCCESS) goto fail;
   return t;
fail:
   hevc_render_close(t);
   return NULL;
}

static unsigned render_sample(const uint8_t *data, size_t offset, unsigned bytes)
{
   return bytes == 2 ? (data[offset] | ((unsigned)data[offset + 1] << 8)) >> 6 : data[offset];
}

int hevc_render_submit(void *opaque, video_hw_frame_t *hw)
{
   import_oracle_t *t = (import_oracle_t*)opaque;
   video_frame_import_t frame;
   enum video_texture_update result;
   if (hw->width != t->width || hw->height != t->height) return 0;
   memset(&frame, 0, sizeof(frame));
   frame.plane_views[0] = hw->plane_views[0]; frame.plane_views[1] = hw->plane_views[1];
   frame.image = hw->image; frame.wait = hw->wait; frame.wait_value = hw->wait_value;
   frame.done = hw->done; frame.done_value = hw->done_value;
   frame.layout = hw->layout; frame.pixfmt = hw->pixfmt; frame.yuv = hw->yuv;
   frame.dims = VIDEO_SCALE_PACK(hw->width, hw->height);
   if (getenv("VULKAN_TEST_REQUIRE_SAMPLED") && (!frame.plane_views[0] || !frame.plane_views[1])) return 0;
   if (getenv("VULKAN_TEST_REJECT_SAMPLED") && (frame.plane_views[0] || frame.plane_views[1])) return 0;
   plane_copies = 0;
   copy_image_original = vulkan_symbol_wrapper_vkCmdCopyImageToBuffer;
   vulkan_symbol_wrapper_vkCmdCopyImageToBuffer = oracle_copy_image;
   result = vulkan_import_convert(&t->vk, &t->output, &frame);
   vulkan_symbol_wrapper_vkCmdCopyImageToBuffer = copy_image_original;
   if (result != VIDEO_TEXTURE_UPDATE_DONE || plane_copies != (frame.plane_views[0] ? 0u : 1u)) return 0;
   hw->abandon = NULL;
   if (getenv("VULKAN_TEST_REQUIRE_SAMPLED"))
   {
      unsigned i;
      for (i = 0; i < VK_STREAM_SLOTS; i++)
         if (t->vk.stream_states->staging[i].size != 0) return 0;
   }
   return 1;
}

int hevc_render_frame(void *opaque, const video_hw_frame_t *hw,
      const uint8_t *planes, size_t uv_offset, unsigned bytes)
{
   import_oracle_t *t = (import_oracle_t*)opaque;
   struct vulkan_filter_chain_texture input;
   VkBufferImageCopy copy = {0};
   VkBufferMemoryBarrier barrier = {0};
   VkSemaphore done;
   uint64_t value = 0;
   unsigned x, y, channel, depth = bytes == 2 ? 10 : 8;
   unsigned full = !!(hw->yuv & IMAGE_YUV_FLAG_FULL_RANGE);
   PFN_vkGetSemaphoreCounterValueKHR counter = (PFN_vkGetSemaphoreCounterValueKHR)
      vkGetDeviceProcAddr(t->vk.context->device, "vkGetSemaphoreCounterValueKHR");
   if (!counter || hw->width != t->width || hw->height != t->height) return 0;
   vulkan_movie_input(&t->output, &input);
   if (input.image != t->output.image || input.view != t->output.view
         || input.dims != VIDEO_SCALE_PACK(hw->width, hw->height) || input.padded_dims || input.origin
         || input.format != VK_FORMAT_R8G8B8A8_UNORM
         || input.layout != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL || !oracle_begin(t)) return 0;
   VULKAN_IMAGE_LAYOUT_TRANSITION(t->cb, input.image, input.layout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
         VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
   copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
   copy.imageSubresource.layerCount = 1;
   copy.imageExtent.width = t->width; copy.imageExtent.height = t->height; copy.imageExtent.depth = 1;
   vkCmdCopyImageToBuffer(t->cb, input.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, t->readback, 1, &copy);
   VULKAN_IMAGE_LAYOUT_TRANSITION(t->cb, input.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, input.layout,
         VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_READ_BIT,
         VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
   barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
   barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT; barrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
   barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
   barrier.buffer = t->readback; barrier.size = VK_WHOLE_SIZE;
   vkCmdPipelineBarrier(t->cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
         0, 0, NULL, 1, &barrier, 0, NULL);
   memcpy(&done, &hw->done, sizeof(done));
   if (!oracle_submit(t) || counter(t->vk.context->device, done, &value) != VK_SUCCESS
         || value < hw->done_value) return 0;
   for (y = 0; y < t->height; y++)
      for (x = 0; x < t->width; x++)
      {
         size_t chroma = uv_offset + ((y / 2) * ((t->width + 1) / 2) + x / 2) * 2 * bytes;
         double scale = full ? (double)((1u << depth) - 1) : (double)(219u << (depth - 8));
         double cscale = full ? scale : (double)(224u << (depth - 8));
         double yy = (render_sample(planes, ((size_t)y * t->width + x) * bytes, bytes)
               - (full ? 0.0 : (double)(16u << (depth - 8)))) / scale;
         double cb = (render_sample(planes, chroma, bytes) - (double)(1u << (depth - 1))) / cscale;
         double cr = (render_sample(planes, chroma + bytes, bytes) - (double)(1u << (depth - 1))) / cscale;
         double rgb[3];
         unsigned bt709 = !!(hw->yuv & IMAGE_YUV_FLAG_BT709);
         rgb[0] = yy + (bt709 ? 1.5748 : 1.402) * cr;
         rgb[1] = yy - (bt709 ? 0.187324 : 0.344136) * cb - (bt709 ? 0.468124 : 0.714136) * cr;
         rgb[2] = yy + (bt709 ? 1.8556 : 1.772) * cb;
         for (channel = 0; channel < 3; channel++)
         {
            int expected = (int)floor((rgb[channel] < 0 ? 0 : rgb[channel] > 1 ? 1 : rgb[channel]) * 255.0 + 0.5);
            int actual = t->mapped_readback[((size_t)y * t->width + x) * 4 + channel];
            if (abs(actual - expected) > 2)
            {
               printf("HEVC import pixel %u,%u/%u: %d != %d\n", x, y, channel, actual, expected);
               return 0;
            }
         }
         if (t->mapped_readback[((size_t)y * t->width + x) * 4 + 3] != 255) return 0;
      }
   return !import_errors;
}
