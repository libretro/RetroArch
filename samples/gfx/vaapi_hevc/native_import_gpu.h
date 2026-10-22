/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "../../../gfx/common/vaapi_hevc_vulkan_resources.h"
#include "../../../gfx/common/vaapi_hevc_vulkan_submit.h"
#include "native_rgb.h"

#define IMPORT_OK(call) assert((call) == VK_SUCCESS)

static unsigned import_family, import_errors, import_pixels, import_max_error;
static VkQueue import_queue;

static VKAPI_ATTR VkBool32 VKAPI_CALL import_debug(
      VkDebugUtilsMessageSeverityFlagBitsEXT severity,
      VkDebugUtilsMessageTypeFlagsEXT type,
      const VkDebugUtilsMessengerCallbackDataEXT *message, void *user)
{
   (void)type;
   (void)user;
   if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
   {
      import_errors++;
      fprintf(stderr, "Vulkan validation: %s\n", message->pMessage);
   }
   return VK_FALSE;
}

static unsigned import_memory_type(uint32_t bits, VkMemoryPropertyFlags flags)
{
   unsigned i;
   for (i = 0; i < import_memory.memoryTypeCount; i++)
      if ((bits & ((uint32_t)1 << i))
            && (import_memory.memoryTypes[i].propertyFlags & flags) == flags)
         return i;
   assert(0);
   return 0;
}

static VkResult import_submit(void *context, VkQueue queue, uint32_t count,
      const VkSubmitInfo *info, VkFence fence)
{
   (void)context;
   assert(queue == import_queue && count == 1);
   return vkQueueSubmit(queue, count, info, fence);
}

static unsigned import_sample(const uint8_t *data, size_t offset, bool ten)
{
   return ten ? ((unsigned)data[offset] | ((unsigned)data[offset + 1] << 8))
                      >> 6
              : data[offset];
}

static void import_compare(const vaapi_frame_view_t *view, const VAImage *image,
      const uint8_t *data, const uint8_t *rgba)
{
   bool ten       = image->format.fourcc == VA_FOURCC_P010;
   unsigned bytes = ten ? 2 : 1, row, col, i;
   unsigned full  = view->color.range
                          ? view->color.range == 2
                          : !!(view->yuv & IMAGE_YUV_FLAG_FULL_RANGE);
   unsigned bt709 = view->color.matrix == 1
                    || ((view->color.matrix == 0 || view->color.matrix == 2)
                          && (view->yuv & IMAGE_YUV_FLAG_BT709));
   assert(view->color.transfer != 16 && view->color.transfer != 18);
   assert(view->color.matrix == 0 || view->color.matrix == 1
          || view->color.matrix == 2 || view->color.matrix == 5
          || view->color.matrix == 6);
   for (row = 0; row < view->height; row++)
      for (col = 0; col < view->width; col++)
      {
         uint8_t expected[4];
         size_t y = image->offsets[0]
                    + (view->y + row) * (size_t)image->pitches[0]
                    + (view->x + col) * bytes;
         size_t uv = image->offsets[1]
                     + ((view->y + row) / 2) * (size_t)image->pitches[1]
                     + ((view->x + col) / 2) * 2 * bytes;
         size_t output = ((size_t)row * view->width + col) * 4;
         native_rgb(import_sample(data, y, ten), import_sample(data, uv, ten),
               import_sample(data, uv + bytes, ten), ten, full, bt709,
               expected);
         for (i = 0; i < 4; i++)
         {
            unsigned actual = rgba[output + i];
            unsigned delta  = actual > expected[i] ? actual - expected[i]
                                                   : expected[i] - actual;
            if (delta > import_max_error)
               import_max_error = delta;
            if (delta > (i == 3 ? 0u : 1u))
               fprintf(stderr,
                     "Native import pixel %u,%u channel %u: %u expected %u\n",
                     col, row, i, actual, (unsigned)expected[i]);
            assert(delta <= (i == 3 ? 0u : 1u));
         }
         import_pixels++;
      }
}

/* Only this oracle allocates, waits and reads back converted pixels. */
static void native_import_convert(vaapi_hevc_vulkan_frame_t *frame,
      const vaapi_frame_view_t *view, const VAImage *image, const uint8_t *data)
{
   static const vaapi_hevc_vulkan_resource_ops_t ops = {vkCreateSampler,
         vkDestroySampler, vkCreateDescriptorSetLayout,
         vkDestroyDescriptorSetLayout, vkCreatePipelineLayout,
         vkDestroyPipelineLayout, vkCreateDescriptorPool,
         vkDestroyDescriptorPool, vkAllocateDescriptorSets,
         vkCreateShaderModule, vkDestroyShaderModule, vkCreateComputePipelines,
         vkDestroyPipeline, vkCreateCommandPool, vkDestroyCommandPool,
         vkAllocateCommandBuffers, vkCreateFence, vkDestroyFence};
   static vaapi_hevc_vulkan_resources_t resources;
   static vaapi_hevc_vulkan_submission_t entry;
   static VkImageCreateInfo ii;
   static VkImageViewCreateInfo vi;
   static VkMemoryAllocateInfo ai;
   static VkMemoryRequirements req;
   static VkBufferCreateInfo bi;
   static VkCommandBufferBeginInfo begin;
   static VkImageMemoryBarrier barrier;
   static VkBufferMemoryBarrier host;
   static VkBufferImageCopy copy;
   static VkSubmitInfo submit;
   VkImage destination;
   VkImageView destination_view;
   VkDeviceMemory destination_memory, buffer_memory;
   VkBuffer buffer;
   VkCommandBuffer command;
   uint8_t *mapped   = NULL;
   VkDeviceSize size = (VkDeviceSize)view->width * view->height * 4;
   memset(&ii, 0, sizeof(ii));
   ii.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
   ii.imageType     = VK_IMAGE_TYPE_2D;
   ii.format        = VK_FORMAT_R8G8B8A8_UNORM;
   ii.extent.width  = view->width;
   ii.extent.height = view->height;
   ii.extent.depth  = 1;
   ii.mipLevels = ii.arrayLayers = 1;
   ii.samples                    = VK_SAMPLE_COUNT_1_BIT;
   ii.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
              | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
   IMPORT_OK(vkCreateImage(import_device, &ii, NULL, &destination));
   vkGetImageMemoryRequirements(import_device, destination, &req);
   memset(&ai, 0, sizeof(ai));
   ai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
   ai.allocationSize  = req.size;
   ai.memoryTypeIndex = import_memory_type(req.memoryTypeBits, 0);
   IMPORT_OK(vkAllocateMemory(import_device, &ai, NULL, &destination_memory));
   IMPORT_OK(
         vkBindImageMemory(import_device, destination, destination_memory, 0));
   memset(&vi, 0, sizeof(vi));
   vi.sType                       = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
   vi.image                       = destination;
   vi.format                      = ii.format;
   vi.viewType                    = VK_IMAGE_VIEW_TYPE_2D;
   vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
   vi.subresourceRange.layerCount = vi.subresourceRange.levelCount = 1;
   IMPORT_OK(vkCreateImageView(import_device, &vi, NULL, &destination_view));
   memset(&bi, 0, sizeof(bi));
   bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
   bi.size  = size;
   bi.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
   IMPORT_OK(vkCreateBuffer(import_device, &bi, NULL, &buffer));
   vkGetBufferMemoryRequirements(import_device, buffer, &req);
   ai.allocationSize  = req.size;
   ai.memoryTypeIndex = import_memory_type(
         req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                                   | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
   IMPORT_OK(vkAllocateMemory(import_device, &ai, NULL, &buffer_memory));
   IMPORT_OK(vkBindBufferMemory(import_device, buffer, buffer_memory, 0));
   assert(vaapi_hevc_vulkan_resources_create(
         &resources, import_device, import_family, 2, false, &ops));
   memset(&entry, 0, sizeof(entry));
   assert(vaapi_hevc_vulkan_submission_configure(&entry, frame, view->x,
         view->y, view->width, view->height, view->yuv, &view->color, false));
   entry.queue             = import_queue;
   entry.command           = resources.commands[0];
   entry.fence             = resources.fences[0];
   entry.pipeline          = resources.pipeline;
   entry.layout            = resources.layout;
   entry.set               = resources.sets[0];
   entry.sampler           = resources.sampler;
   entry.family            = import_family;
   entry.ops.fence_status  = vkGetFenceStatus;
   entry.ops.reset_fences  = vkResetFences;
   entry.ops.reset_command = vkResetCommandBuffer;
   entry.ops.begin         = vkBeginCommandBuffer;
   entry.ops.end           = vkEndCommandBuffer;
   entry.ops.update        = vkUpdateDescriptorSets;
   entry.ops.barrier       = vkCmdPipelineBarrier;
   entry.ops.bind_pipeline = vkCmdBindPipeline;
   entry.ops.bind_sets     = vkCmdBindDescriptorSets;
   entry.ops.push          = vkCmdPushConstants;
   entry.ops.dispatch      = vkCmdDispatch;
   entry.ops.submit        = import_submit;
   assert(vaapi_hevc_vulkan_submission_present(&entry, destination,
                destination_view, ii.format, view->width, view->height,
                VK_IMAGE_LAYOUT_UNDEFINED)
          == 1);
   IMPORT_OK(vkWaitForFences(
         import_device, 1, &entry.fence, VK_TRUE, (uint64_t)5000000000.0));
   assert(vaapi_hevc_vulkan_frame_poll(frame, vkGetFenceStatus) == 1);
   command = resources.commands[1];
   memset(&begin, 0, sizeof(begin));
   begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
   IMPORT_OK(vkBeginCommandBuffer(command, &begin));
   memset(&barrier, 0, sizeof(barrier));
   barrier.sType     = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
   barrier.image     = destination;
   barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
   barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
   barrier.srcAccessMask =
         VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
   barrier.dstAccessMask       = VK_ACCESS_TRANSFER_READ_BIT;
   barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex =
         VK_QUEUE_FAMILY_IGNORED;
   barrier.subresourceRange = vi.subresourceRange;
   vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);
   memset(&copy, 0, sizeof(copy));
   copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
   copy.imageSubresource.layerCount = 1;
   copy.imageExtent                 = ii.extent;
   vkCmdCopyImageToBuffer(command, destination,
         VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buffer, 1, &copy);
   memset(&host, 0, sizeof(host));
   host.sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
   host.buffer              = buffer;
   host.size                = size;
   host.srcAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT;
   host.dstAccessMask       = VK_ACCESS_HOST_READ_BIT;
   host.srcQueueFamilyIndex = host.dstQueueFamilyIndex =
         VK_QUEUE_FAMILY_IGNORED;
   vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT,
         VK_PIPELINE_STAGE_HOST_BIT, 0, 0, NULL, 1, &host, 0, NULL);
   IMPORT_OK(vkEndCommandBuffer(command));
   memset(&submit, 0, sizeof(submit));
   submit.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
   submit.commandBufferCount = 1;
   submit.pCommandBuffers    = &command;
   IMPORT_OK(vkResetFences(import_device, 1, &resources.fences[1]));
   IMPORT_OK(vkQueueSubmit(import_queue, 1, &submit, resources.fences[1]));
   IMPORT_OK(vkWaitForFences(import_device, 1, &resources.fences[1], VK_TRUE,
         (uint64_t)5000000000.0));
   IMPORT_OK(vkMapMemory(
         import_device, buffer_memory, 0, size, 0, (void **)&mapped));
   import_compare(view, image, data, mapped);
   vkUnmapMemory(import_device, buffer_memory);
   vaapi_hevc_vulkan_resources_destroy(&resources);
   vkDestroyBuffer(import_device, buffer, NULL);
   vkFreeMemory(import_device, buffer_memory, NULL);
   vkDestroyImageView(import_device, destination_view, NULL);
   vkDestroyImage(import_device, destination, NULL);
   vkFreeMemory(import_device, destination_memory, NULL);
   assert(!import_errors);
}
