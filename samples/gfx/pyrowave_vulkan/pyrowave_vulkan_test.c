/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "../../../libretro-common/samples/formats/pyrowave/gpu_vulkan_context.h"
#include "../../../record/record_pyrowave_vulkan.h"
#define RPYROWAVE_SHADER_SPIRV
#include "rpyrowave_shaders.h"

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "Vulkan adapter failed line %u: %s\n", \
   (unsigned)__LINE__, #x); return 0; } } while (0)

typedef struct test_pyrowave_vk
{
   rpy_vk_test_t *base;
   record_pyrowave_vulkan_t *send, *receive;
   rpyrowave_decoder_t *cpu;
   PFN_vkCreateImage create_image;
   PFN_vkDestroyImage destroy_image;
   PFN_vkGetImageMemoryRequirements image_requirements;
   PFN_vkBindImageMemory bind_image;
   PFN_vkCreateImageView create_view;
   PFN_vkDestroyImageView destroy_view;
   PFN_vkCmdCopyBufferToImage copy_to_image;
   PFN_vkCmdCopyImageToBuffer copy_from_image;
   PFN_vkResetCommandPool reset_pool;
   PFN_vkDeviceWaitIdle idle;
   PFN_vkGetPhysicalDeviceFormatProperties format_properties;
   PFN_vkAllocateMemory allocate;
   PFN_vkMapMemory map;
   PFN_vkFlushMappedMemoryRanges flush;
   PFN_vkInvalidateMappedMemoryRanges invalidate;
   PFN_vkGetFenceStatus fence_status;
   VkCommandPool pool;
   VkCommandBuffer commands[4];
   VkFence fences[4];
   VkFence fail_fence;
   VkImage source;
   VkImageView view;
   VkDeviceMemory memory;
   rpy_vk_buffer_t upload, download;
   unsigned width, height, mode, forbidden, flushes, invalidates;
   unsigned allocation_calls, fail_allocation;
   bool hot;
} test_pyrowave_vk_t;

static test_pyrowave_vk_t *test_pyrowave_current;

static VKAPI_ATTR VkResult VKAPI_CALL test_pyrowave_fence_status(VkDevice device, VkFence fence)
{
   test_pyrowave_vk_t *t = test_pyrowave_current;
   if (fence == t->fail_fence) return VK_ERROR_DEVICE_LOST;
   return t->fence_status(device, fence);
}

static VKAPI_ATTR VkResult VKAPI_CALL test_pyrowave_allocate(VkDevice device,
      const VkMemoryAllocateInfo *info, const VkAllocationCallbacks *allocator, VkDeviceMemory *memory)
{
   test_pyrowave_vk_t *t = test_pyrowave_current;
   if (t->hot) t->forbidden++;
   t->allocation_calls++;
   if (t->fail_allocation && t->allocation_calls == t->fail_allocation)
   {
      *memory = VK_NULL_HANDLE;
      return VK_ERROR_OUT_OF_DEVICE_MEMORY;
   }
   return t->allocate(device, info, allocator, memory);
}

static VKAPI_ATTR VkResult VKAPI_CALL test_pyrowave_map(VkDevice device, VkDeviceMemory memory,
      VkDeviceSize offset, VkDeviceSize size, VkMemoryMapFlags flags, void **data)
{
   test_pyrowave_vk_t *t = test_pyrowave_current;
   if (t->hot) t->forbidden++;
   return t->map(device, memory, offset, size, flags, data);
}

static VKAPI_ATTR VkResult VKAPI_CALL test_pyrowave_flush(VkDevice device,
      uint32_t count, const VkMappedMemoryRange *ranges)
{
   test_pyrowave_vk_t *t = test_pyrowave_current;
   t->flushes++;
   return t->flush(device, count, ranges);
}

static VKAPI_ATTR VkResult VKAPI_CALL test_pyrowave_invalidate(VkDevice device,
      uint32_t count, const VkMappedMemoryRange *ranges)
{
   test_pyrowave_vk_t *t = test_pyrowave_current;
   t->invalidates++;
   return t->invalidate(device, count, ranges);
}

static VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL test_pyrowave_proc(VkDevice device, const char *name)
{
   PFN_vkVoidFunction result;
#define TEST_VK_REPLACE(text, type, function) \
   if (!strcmp(name, text)) { type entry = function; memcpy(&result, &entry, sizeof(result)); return result; }
   TEST_VK_REPLACE("vkAllocateMemory", PFN_vkAllocateMemory, test_pyrowave_allocate)
   TEST_VK_REPLACE("vkGetFenceStatus", PFN_vkGetFenceStatus, test_pyrowave_fence_status)
   TEST_VK_REPLACE("vkMapMemory", PFN_vkMapMemory, test_pyrowave_map)
   TEST_VK_REPLACE("vkFlushMappedMemoryRanges", PFN_vkFlushMappedMemoryRanges, test_pyrowave_flush)
   TEST_VK_REPLACE("vkInvalidateMappedMemoryRanges", PFN_vkInvalidateMappedMemoryRanges, test_pyrowave_invalidate)
#undef TEST_VK_REPLACE
   if (!strcmp(name, "vkQueueSubmit") || !strcmp(name, "vkWaitForFences") || !strcmp(name, "vkDeviceWaitIdle"))
      return NULL;
   return test_pyrowave_current->base->vkGetDeviceProcAddr(device, name);
}

static int test_pyrowave_functions(test_pyrowave_vk_t *t)
{
   PFN_vkVoidFunction entry;
#define TEST_VK_LOAD(name, member) \
   entry = t->base->vkGetDeviceProcAddr(t->base->device, #name); \
   CHECK(entry); memcpy(&t->member, &entry, sizeof(entry));
   TEST_VK_LOAD(vkCreateImage, create_image)
   TEST_VK_LOAD(vkDestroyImage, destroy_image)
   TEST_VK_LOAD(vkGetImageMemoryRequirements, image_requirements)
   TEST_VK_LOAD(vkBindImageMemory, bind_image)
   TEST_VK_LOAD(vkCreateImageView, create_view)
   TEST_VK_LOAD(vkDestroyImageView, destroy_view)
   TEST_VK_LOAD(vkCmdCopyBufferToImage, copy_to_image)
   TEST_VK_LOAD(vkCmdCopyImageToBuffer, copy_from_image)
   TEST_VK_LOAD(vkResetCommandPool, reset_pool)
   TEST_VK_LOAD(vkDeviceWaitIdle, idle)
   TEST_VK_LOAD(vkAllocateMemory, allocate)
   TEST_VK_LOAD(vkMapMemory, map)
   TEST_VK_LOAD(vkFlushMappedMemoryRanges, flush)
   TEST_VK_LOAD(vkInvalidateMappedMemoryRanges, invalidate)
   TEST_VK_LOAD(vkGetFenceStatus, fence_status)
#undef TEST_VK_LOAD
   entry = t->base->vkGetInstanceProcAddr(t->base->instance, "vkGetPhysicalDeviceFormatProperties");
   CHECK(entry); memcpy(&t->format_properties, &entry, sizeof(entry));
   return 1;
}

static int test_pyrowave_commands(test_pyrowave_vk_t *t)
{
   VkCommandPoolCreateInfo pool;
   VkCommandBufferAllocateInfo allocate;
   VkFenceCreateInfo fence;
   unsigned i;
   memset(&pool, 0, sizeof(pool));
   pool.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
   pool.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
   pool.queueFamilyIndex = t->base->family;
   CHECK(t->base->vkCreateCommandPool(t->base->device, &pool, NULL, &t->pool) == VK_SUCCESS);
   memset(&allocate, 0, sizeof(allocate));
   allocate.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
   allocate.commandPool = t->pool; allocate.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; allocate.commandBufferCount = 4;
   CHECK(t->base->vkAllocateCommandBuffers(t->base->device, &allocate, t->commands) == VK_SUCCESS);
   memset(&fence, 0, sizeof(fence)); fence.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
   for (i = 0; i < 4; i++) CHECK(t->base->vkCreateFence(t->base->device, &fence, NULL, &t->fences[i]) == VK_SUCCESS);
   return 1;
}

static int test_pyrowave_begin(test_pyrowave_vk_t *t, unsigned i)
{
   VkCommandBufferBeginInfo begin;
   CHECK(t->base->vkResetFences(t->base->device, 1, &t->fences[i]) == VK_SUCCESS);
   CHECK(t->base->vkResetCommandBuffer(t->commands[i], 0) == VK_SUCCESS);
   memset(&begin, 0, sizeof(begin)); begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
   begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
   CHECK(t->base->vkBeginCommandBuffer(t->commands[i], &begin) == VK_SUCCESS);
   return 1;
}

static int test_pyrowave_submit(test_pyrowave_vk_t *t, unsigned i)
{
   VkSubmitInfo submit;
   bool hot = t->hot;
   t->hot = false;
   CHECK(t->base->vkEndCommandBuffer(t->commands[i]) == VK_SUCCESS);
   memset(&submit, 0, sizeof(submit)); submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
   submit.commandBufferCount = 1; submit.pCommandBuffers = &t->commands[i];
   CHECK(t->base->vkQueueSubmit(t->base->queue, 1, &submit, t->fences[i]) == VK_SUCCESS);
   /* Offline oracle only. The shipping adapter does not submit or wait. */
   CHECK(t->base->vkWaitForFences(t->base->device, 1, &t->fences[i], VK_TRUE, (uint64_t)3000000000U) == VK_SUCCESS);
   t->hot = hot;
   return 1;
}

static void test_pyrowave_image_barrier(test_pyrowave_vk_t *t, VkCommandBuffer commands,
      VkImage image, VkImageLayout from, VkImageLayout to, VkAccessFlags source, VkAccessFlags destination,
      VkPipelineStageFlags first, VkPipelineStageFlags second)
{
   VkImageMemoryBarrier barrier;
   memset(&barrier, 0, sizeof(barrier)); barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
   barrier.oldLayout = from; barrier.newLayout = to;
   barrier.srcAccessMask = source; barrier.dstAccessMask = destination;
   barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
   barrier.image = image; barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
   barrier.subresourceRange.levelCount = barrier.subresourceRange.layerCount = 1;
   t->base->vkCmdPipelineBarrier(commands, first, second, 0, 0, NULL, 0, NULL, 1, &barrier);
}

static int test_pyrowave_source(test_pyrowave_vk_t *t)
{
   VkImageCreateInfo image;
   VkImageViewCreateInfo view;
   VkMemoryRequirements requirements;
   VkMemoryAllocateInfo allocate;
   VkBufferImageCopy copy;
   uint32_t type;
   unsigned x, y, channel;
   float *pixels;
   static const float code[3] = { 0.8f, 0.2f, 0.05f };
   static const float hdr[3] = { 20.756137525f, -1.556880938f, -0.226884538f };
   size_t bytes = (size_t)(t->width + 4) * (t->height + 6) * 4 * sizeof(float);
   memset(&image, 0, sizeof(image)); image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
   image.imageType = VK_IMAGE_TYPE_2D; image.format = VK_FORMAT_R32G32B32A32_SFLOAT;
   image.extent.width = t->width + 4; image.extent.height = t->height + 6; image.extent.depth = 1;
   image.mipLevels = image.arrayLayers = 1; image.samples = VK_SAMPLE_COUNT_1_BIT;
   image.tiling = VK_IMAGE_TILING_OPTIMAL; image.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
   CHECK(t->create_image(t->base->device, &image, NULL, &t->source) == VK_SUCCESS);
   t->image_requirements(t->base->device, t->source, &requirements);
   for (type = 0; type < t->base->memory_properties.memoryTypeCount; type++)
      if ((requirements.memoryTypeBits & ((uint32_t)1 << type))
            && (t->base->memory_properties.memoryTypes[type].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) break;
   CHECK(type < t->base->memory_properties.memoryTypeCount);
   memset(&allocate, 0, sizeof(allocate)); allocate.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
   allocate.memoryTypeIndex = type; allocate.allocationSize = requirements.size;
   CHECK(t->base->vkAllocateMemory(t->base->device, &allocate, NULL, &t->memory) == VK_SUCCESS);
   CHECK(t->bind_image(t->base->device, t->source, t->memory, 0) == VK_SUCCESS);
   memset(&view, 0, sizeof(view)); view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
   view.image = t->source; view.viewType = VK_IMAGE_VIEW_TYPE_2D; view.format = image.format;
   view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
   view.subresourceRange.levelCount = view.subresourceRange.layerCount = 1;
   CHECK(t->create_view(t->base->device, &view, NULL, &t->view) == VK_SUCCESS);
   CHECK(rpy_vk_buffer_create(t->base, &t->upload, bytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));
   CHECK(rpy_vk_buffer_create(t->base, &t->download, (size_t)t->width * t->height * 4 * sizeof(float),
            VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));
   pixels = (float*)t->upload.mapped;
   for (y = 0; y < t->height + 6; y++)
      for (x = 0; x < t->width + 4; x++)
         for (channel = 0; channel < 4; channel++)
            pixels[((size_t)y * (t->width + 4) + x) * 4 + channel] = channel == 3 ? 0.25f
               : (x >= 2 && x < t->width + 2 && y >= 3 && y < t->height + 3
                  ? (t->mode ? hdr[channel] : code[channel]) : 0.01f);
   CHECK(test_pyrowave_begin(t, 3));
   test_pyrowave_image_barrier(t, t->commands[3], t->source, VK_IMAGE_LAYOUT_UNDEFINED,
         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, VK_ACCESS_TRANSFER_WRITE_BIT,
         VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
   memset(&copy, 0, sizeof(copy)); copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
   copy.imageSubresource.layerCount = 1; copy.imageExtent = image.extent;
   t->copy_to_image(t->commands[3], t->upload.buffer, t->source, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
   test_pyrowave_image_barrier(t, t->commands[3], t->source, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
         VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
   CHECK(test_pyrowave_submit(t, 3));
   return 1;
}

static uint32_t test_pyrowave_hash(const record_pyrowave_packet_t *packet)
{
   const uint8_t *p = (const uint8_t*)packet->data;
   uint32_t hash = 2166136261U;
   size_t i;
   for (i = 0; i < packet->bytes; i++) hash = (hash ^ p[i]) * 16777619U;
   return hash;
}

static int test_pyrowave_pixels(test_pyrowave_vk_t *t, const record_pyrowave_vulkan_image_t *image)
{
   VkBufferImageCopy copy;
   VkBufferMemoryBarrier barrier;
   size_t i;
   const float *pixels = (const float*)t->download.mapped;
   static const float code[3] = { 0.8f, 0.2f, 0.05f };
   static const float hdr[3] = { 20.756137525f, -1.556880938f, -0.226884538f };
   double maximum = 0.0;
   bool late = (image->frame.ready_value & 1) != 0;
   CHECK(image->layout == VK_IMAGE_LAYOUT_GENERAL && image->queue_family == t->base->family
         && image->texture_mode == t->mode);
   CHECK(test_pyrowave_begin(t, 3));
   test_pyrowave_image_barrier(t, t->commands[3], image->image, image->layout, image->layout,
         VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
   memset(&copy, 0, sizeof(copy)); copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
   copy.imageSubresource.layerCount = 1; copy.imageExtent.width = t->width;
   copy.imageExtent.height = t->height; copy.imageExtent.depth = 1;
   t->copy_from_image(t->commands[3], image->image, image->layout, t->download.buffer, 1, &copy);
   memset(&barrier, 0, sizeof(barrier)); barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
   barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT; barrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
   barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
   barrier.buffer = t->download.buffer; barrier.size = (VkDeviceSize)-1;
   t->base->vkCmdPipelineBarrier(t->commands[3], VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
         0, 0, NULL, 1, &barrier, 0, NULL);
   if (!late) CHECK(record_pyrowave_vulkan_present(t->receive, image->frame.slot, t->fences[3]));
   CHECK(!record_pyrowave_vulkan_free(t->receive));
   CHECK(test_pyrowave_submit(t, 3));
   if (late) CHECK(record_pyrowave_vulkan_present(t->receive, image->frame.slot, t->fences[3]));
   CHECK(record_pyrowave_vulkan_fence_borrowed(t->receive, t->fences[3]));
   CHECK(!record_pyrowave_vulkan_idle(t->receive));
   CHECK(record_pyrowave_vulkan_poll(t->receive));
   CHECK(!record_pyrowave_vulkan_fence_borrowed(t->receive, t->fences[3]));
   CHECK(record_pyrowave_vulkan_idle(t->receive));
   for (i = 0; i < (size_t)t->width * t->height * 4; i++)
   {
      double expected = i % 4 == 3 ? 1.0 : (t->mode ? hdr[i % 4] : code[i % 4]);
      double error = fabs(pixels[i] - expected);
      if (error > maximum) maximum = error;
      CHECK(error < (t->mode ? 0.02 + fabs(expected) * 0.003 : 0.003));
   }
   return 1;
}

static int test_pyrowave_packet(test_pyrowave_vk_t *t, const record_pyrowave_packet_t *packet,
      const record_gpu_frame_t *frame)
{
   rpyrowave_image_t image;
   double code = t->mode ? 0.751827096247 : 0.0;
   double luma = t->mode ? code * 0.2627 : 0.8 * 0.2126 + 0.2 * 0.7152 + 0.05 * 0.0722;
   double center = t->mode ? 512.0 / 1023.0 : 128.0 / 255.0;
   double cb = center + ((t->mode ? 0.0 : 0.05) - luma) / (t->mode ? 1.8814 : 1.8556);
   double cr = center + ((t->mode ? code : 0.8) - luma) / (t->mode ? 1.4746 : 1.5748);
   CHECK(!rpyrowave_decode_frame(t->cpu, packet->data, packet->bytes, 0, &image));
   CHECK(image.sequence.sequence == (frame->ready_value & 7)
         && image.sequence.transfer_pq == t->mode && image.sequence.primaries_bt2020 == t->mode);
   CHECK(fabs(image.planes[0][0] - luma) < 0.003 && fabs(image.planes[1][0] - cb) < 0.003
         && fabs(image.planes[2][0] - cr) < 0.003);
   return 1;
}

#ifdef PYROWAVE_PLAYER_FACTORY
#include "pyrowave_vulkan_player_test.h"
#endif

static int test_pyrowave_shared_display(test_pyrowave_vk_t *t,
      const record_pyrowave_vulkan_config_t *cfg, const record_pyrowave_packet_t *packet)
{
   record_pyrowave_vulkan_config_t setup = *cfg;
   record_pyrowave_vulkan_image_t image;
   void *span;
   size_t capacity;
   unsigned pass, i;
   memset(&image, 0, sizeof(image));
   CHECK(record_pyrowave_vulkan_free(t->receive)); t->receive = NULL;
   setup.slots = 2;
   for (pass = 0; pass < 2; pass++)
   {
      t->hot = false;
      t->receive = record_pyrowave_vulkan_new(&setup);
      CHECK(t->receive);
      t->hot = true;
      for (i = 0; i < 2; i++)
      {
         CHECK(record_pyrowave_vulkan_receive_acquire(t->receive, &span, &capacity) == RECORD_GPU_QUEUE_OK);
         CHECK(capacity >= packet->bytes);
         memcpy(span, packet->data, packet->bytes);
         CHECK(record_pyrowave_vulkan_receive_submit(t->receive, packet->bytes,
                  i * (uint64_t)16667, 16667) == RECORD_GPU_QUEUE_OK);
         CHECK(test_pyrowave_begin(t, 2));
         CHECK(record_pyrowave_vulkan_decode(t->receive, t->commands[2], t->fences[2]) == RECORD_GPU_QUEUE_OK);
         if (!i)
         {
            CHECK(test_pyrowave_submit(t, 2) && record_pyrowave_vulkan_poll(t->receive));
            CHECK(record_pyrowave_vulkan_image(t->receive, &image) == RECORD_GPU_QUEUE_OK);
         }
      }
      CHECK(record_pyrowave_vulkan_present(t->receive, image.frame.slot, t->fences[2]));
      CHECK(record_pyrowave_vulkan_fence_borrowed(t->receive, t->fences[2]));
      if (!pass)
      {
         CHECK(t->base->vkResetCommandBuffer(t->commands[2], 0) == VK_SUCCESS);
         CHECK(record_pyrowave_vulkan_discard(t->receive, t->fences[2]));
         CHECK(!record_pyrowave_vulkan_fence_borrowed(t->receive, t->fences[2]));
         CHECK(record_pyrowave_vulkan_idle(t->receive));
         CHECK(record_pyrowave_vulkan_free(t->receive)); t->receive = NULL;
      }
      else
      {
         CHECK(test_pyrowave_submit(t, 2));
         CHECK(record_pyrowave_vulkan_fence_borrowed(t->receive, t->fences[2]));
         CHECK(record_pyrowave_vulkan_poll(t->receive));
         CHECK(!record_pyrowave_vulkan_fence_borrowed(t->receive, t->fences[2]));
         CHECK(record_pyrowave_vulkan_image(t->receive, &image) == RECORD_GPU_QUEUE_OK);
         CHECK(record_pyrowave_vulkan_image_drop(t->receive, image.frame.slot));
         CHECK(record_pyrowave_vulkan_idle(t->receive));
      }
   }
   return 1;
}

static int test_pyrowave_run(test_pyrowave_vk_t *t, unsigned fallback, unsigned iterations)
{
   record_pyrowave_vulkan_config_t cfg;
   VkPhysicalDeviceMemoryProperties memory = t->base->memory_properties;
   VkFormatProperties properties;
   record_gpu_frame_t held, frame;
   record_pyrowave_packet_t old, packet;
   record_pyrowave_vulkan_image_t image;
   uint64_t ticket = 0, rejected;
   uint32_t hash;
   unsigned i;
   void *span;
   size_t capacity;
   memset(&held, 0, sizeof(held));
   memset(&cfg, 0, sizeof(cfg));
   cfg.get_proc = test_pyrowave_proc; cfg.device = t->base->device; cfg.queue = t->base->queue;
   cfg.queue_flags = VK_QUEUE_COMPUTE_BIT; cfg.queue_family = t->base->family;
   if (fallback)
      for (i = 0; i < memory.memoryTypeCount; i++)
         memory.memoryTypes[i].propertyFlags &= ~(VK_MEMORY_PROPERTY_HOST_CACHED_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
   cfg.memory = &memory; cfg.limits = &t->base->properties.limits;
   t->format_properties(t->base->physical, VK_FORMAT_R32G32B32A32_SFLOAT, &properties);
   cfg.rgba32f_features = properties.optimalTilingFeatures;
   cfg.shaders[0] = rpyrowave_shader_color_spirv; cfg.shader_bytes[0] = sizeof(rpyrowave_shader_color_spirv);
   cfg.shaders[1] = rpyrowave_shader_dwt_spirv; cfg.shader_bytes[1] = sizeof(rpyrowave_shader_dwt_spirv);
   cfg.shaders[2] = rpyrowave_shader_packet_spirv; cfg.shader_bytes[2] = sizeof(rpyrowave_shader_packet_spirv);
   cfg.sequence.width = t->width; cfg.sequence.height = t->height;
   cfg.sequence.primaries_bt2020 = cfg.sequence.transfer_pq = cfg.sequence.transform_bt2020 = t->mode;
   cfg.bit_depth = t->mode ? 10 : 8; cfg.texture_mode = t->mode;
   cfg.maximum_bytes = 262144; cfg.slots = 3;
   cfg.texture_width = t->width + 4; cfg.texture_height = t->height + 6; cfg.x = 2; cfg.y = 3;
   for (i = 1; i < 5; i++)
   {
      t->allocation_calls = 0; t->fail_allocation = i;
      CHECK(!record_pyrowave_vulkan_new(&cfg));
   }
   t->fail_allocation = 0;
   t->send = record_pyrowave_vulkan_new(&cfg);
   cfg.decode = true; cfg.slots = 1;
   t->receive = record_pyrowave_vulkan_new(&cfg);
   t->cpu = rpyrowave_decoder_new(t->width, t->height, 0,
         (size_t)(t->width + 127) * (t->height + 127));
   CHECK(t->send && t->receive && t->cpu && test_pyrowave_source(t));
   t->hot = true;
   for (i = 0; i < 3; i++)
   {
      CHECK(test_pyrowave_begin(t, i));
      CHECK(record_pyrowave_vulkan_push(t->send, t->commands[i], t->fences[i], t->view,
               VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 16667 * (uint64_t)(i + 1), 16667, &ticket) == RECORD_GPU_QUEUE_OK);
      CHECK(record_pyrowave_vulkan_take(t->send, &frame) == RECORD_GPU_QUEUE_OK);
      CHECK(record_pyrowave_vulkan_packet(t->send, frame.slot, &packet) == RECORD_GPU_QUEUE_BUSY);
      if (!i) held = frame;
      else
      {
         CHECK(t->base->vkResetCommandBuffer(t->commands[i], 0) == VK_SUCCESS);
         CHECK(record_pyrowave_vulkan_discard(t->send, t->fences[i]));
         CHECK(record_pyrowave_vulkan_packet(t->send, frame.slot, &packet) == RECORD_GPU_QUEUE_ERROR);
         CHECK(record_pyrowave_vulkan_release(t->send, frame.slot));
         CHECK(!record_pyrowave_vulkan_source_retired(t->send));
      }
   }
   CHECK(test_pyrowave_begin(t, 1)); rejected = ticket;
   CHECK(record_pyrowave_vulkan_push(t->send, t->commands[1], t->fences[1], t->view,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, 16667, &rejected) == RECORD_GPU_QUEUE_BUSY && rejected == ticket);
   CHECK(test_pyrowave_submit(t, 0) && record_pyrowave_vulkan_poll(t->send));
   CHECK(record_pyrowave_vulkan_source_retired(t->send) == 3);
   CHECK(record_pyrowave_vulkan_packet(t->send, held.slot, &old) == RECORD_GPU_QUEUE_OK);
   hash = test_pyrowave_hash(&old);
   for (i = 0; i < iterations; i++)
   {
      CHECK(test_pyrowave_begin(t, 1));
      CHECK(record_pyrowave_vulkan_push(t->send, t->commands[1], t->fences[1], t->view,
               VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, (ticket + 1) * 16667, 16667, &ticket) == RECORD_GPU_QUEUE_OK);
      CHECK(record_pyrowave_vulkan_take(t->send, &frame) == RECORD_GPU_QUEUE_OK);
      CHECK(record_pyrowave_vulkan_packet(t->send, frame.slot, &packet) == RECORD_GPU_QUEUE_BUSY);
      CHECK(test_pyrowave_submit(t, 1) && record_pyrowave_vulkan_poll(t->send));
      CHECK(record_pyrowave_vulkan_packet(t->send, frame.slot, &packet) == RECORD_GPU_QUEUE_OK);
      CHECK(test_pyrowave_packet(t, &packet, &frame) && test_pyrowave_hash(&old) == hash);
      CHECK(record_pyrowave_vulkan_receive_acquire(t->receive, &span, &capacity) == RECORD_GPU_QUEUE_OK && capacity >= packet.bytes);
      memcpy(span, packet.data, packet.bytes);
      if (!i)
      {
         ((uint8_t*)span)[7] ^= 0x10;
         CHECK(record_pyrowave_vulkan_receive_submit(t->receive, packet.bytes, frame.timestamp, frame.duration) == RECORD_GPU_QUEUE_ERROR);
         CHECK(!record_pyrowave_vulkan_finish(t->receive));
         ((uint8_t*)span)[7] ^= 0x10;
      }
      CHECK(record_pyrowave_vulkan_receive_submit(t->receive, packet.bytes, frame.timestamp, frame.duration) == RECORD_GPU_QUEUE_OK);
      CHECK(test_pyrowave_begin(t, 2));
      CHECK(record_pyrowave_vulkan_decode(t->receive, t->commands[2], t->fences[2]) == RECORD_GPU_QUEUE_OK);
      CHECK(record_pyrowave_vulkan_image(t->receive, &image) == RECORD_GPU_QUEUE_BUSY);
      CHECK(record_pyrowave_vulkan_fence_borrowed(t->receive, t->fences[2]));
      CHECK(test_pyrowave_submit(t, 2));
      CHECK(record_pyrowave_vulkan_fence_borrowed(t->receive, t->fences[2]));
      CHECK(record_pyrowave_vulkan_poll(t->receive));
      CHECK(!record_pyrowave_vulkan_fence_borrowed(t->receive, t->fences[2]));
      CHECK(record_pyrowave_vulkan_image(t->receive, &image) == RECORD_GPU_QUEUE_OK);
      CHECK(record_pyrowave_vulkan_receive_acquire(t->receive, &span, &capacity) == RECORD_GPU_QUEUE_BUSY);
      CHECK(test_pyrowave_pixels(t, &image));
      CHECK(record_pyrowave_vulkan_release(t->send, frame.slot));
   }
   CHECK(test_pyrowave_shared_display(t, &cfg, &old));
   CHECK(record_pyrowave_vulkan_finish(t->send) && record_pyrowave_vulkan_finish(t->receive));
   CHECK(record_pyrowave_vulkan_take(t->send, &frame) == RECORD_GPU_QUEUE_END);
   CHECK(!record_pyrowave_vulkan_free(t->send));
   CHECK(record_pyrowave_vulkan_release(t->send, held.slot));
   CHECK(!t->forbidden && !t->base->errors);
   if (fallback) CHECK(t->flushes && t->invalidates);
   t->hot = false;
   CHECK(record_pyrowave_vulkan_free(t->send)); t->send = NULL;
   CHECK(record_pyrowave_vulkan_free(t->receive)); t->receive = NULL;
   cfg.decode = false;
   t->send = record_pyrowave_vulkan_new(&cfg);
   CHECK(t->send && test_pyrowave_begin(t, 0));
   t->hot = true;
   CHECK(record_pyrowave_vulkan_push(t->send, t->commands[0], t->fences[0], t->view,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, 16667, &ticket) == RECORD_GPU_QUEUE_OK);
   CHECK(record_pyrowave_vulkan_take(t->send, &frame) == RECORD_GPU_QUEUE_OK);
   t->fail_fence = t->fences[0];
   CHECK(!record_pyrowave_vulkan_poll(t->send));
   CHECK(!record_pyrowave_vulkan_source_retired(t->send));
   CHECK(!record_pyrowave_vulkan_free(t->send));
   CHECK(record_pyrowave_vulkan_packet(t->send, frame.slot, &packet) == RECORD_GPU_QUEUE_BUSY);
   CHECK(!record_pyrowave_vulkan_release(t->send, frame.slot));
   t->fail_fence = VK_NULL_HANDLE;
   t->hot = false;
   CHECK(t->idle(t->base->device) == VK_SUCCESS);
   CHECK(t->base->vkResetCommandBuffer(t->commands[0], 0) == VK_SUCCESS);
   CHECK(record_pyrowave_vulkan_abandon(t->send)); t->send = NULL;
   CHECK(!t->forbidden && !t->base->errors);
   printf("Vulkan async %ux%u mode%u fallback%u: %u frames, source/display fences and immutable packets PASS\n",
         t->width, t->height, t->mode, fallback, iterations);
   return 1;
}

static void test_pyrowave_case_free(test_pyrowave_vk_t *t)
{
   t->hot = false;
   if (t->idle) t->idle(t->base->device); /* Offline failure cleanup only. */
   if (t->pool) t->reset_pool(t->base->device, t->pool, 0);
   if (t->send) record_pyrowave_vulkan_abandon(t->send);
   if (t->receive) record_pyrowave_vulkan_abandon(t->receive);
   rpyrowave_decoder_free(t->cpu);
   t->send = t->receive = NULL; t->cpu = NULL;
   rpy_vk_buffer_free(t->base, &t->upload); rpy_vk_buffer_free(t->base, &t->download);
   memset(&t->upload, 0, sizeof(t->upload)); memset(&t->download, 0, sizeof(t->download));
   if (t->view) t->destroy_view(t->base->device, t->view, NULL);
   if (t->source) t->destroy_image(t->base->device, t->source, NULL);
   if (t->memory) t->base->vkFreeMemory(t->base->device, t->memory, NULL);
   t->view = VK_NULL_HANDLE; t->source = VK_NULL_HANDLE; t->memory = VK_NULL_HANDLE;
}

int main(int argc, char **argv)
{
   test_pyrowave_vk_t *t = (test_pyrowave_vk_t*)calloc(1, sizeof(*t));
   unsigned mode, fallback, i;
   int success = 0;
   (void)rpy_vk_pipeline; (void)rpy_vk_descriptors; (void)rpy_vk_dispatch;
   (void)rpy_vk_parameters;
   (void)rpy_vk_begin; (void)rpy_vk_submit; (void)rpy_gpu_vector_load; (void)rpy_gpu_vector_compare;
   if (!t) return EXIT_FAILURE;
#ifdef PYROWAVE_PLAYER_FACTORY
   if (argc != 2) { free(t); return EXIT_FAILURE; }
   test_pyrowave_player_directory = argv[1];
#else
   (void)argc; (void)argv;
#endif
   t->base = (rpy_vk_test_t*)calloc(1, sizeof(*t->base)); test_pyrowave_current = t;
   if (!t->base || !rpy_vk_initialize(t->base) || !test_pyrowave_functions(t) || !test_pyrowave_commands(t)) goto end;
   for (mode = 0; mode < 2; mode++)
      for (fallback = 0; fallback < 2; fallback++)
      {
         t->width = 130; t->height = 98; t->mode = mode;
         if (!test_pyrowave_run(t, fallback, 17)) goto end;
         test_pyrowave_case_free(t);
      }
   t->width = 1920; t->height = 1080; t->mode = 0;
   if (!test_pyrowave_run(t, 0, 2)) goto end;
   success = 1;
end:
   if (t->base && t->base->device)
   {
      test_pyrowave_case_free(t);
      for (i = 0; i < 4; i++) if (t->fences[i]) t->base->vkDestroyFence(t->base->device, t->fences[i], NULL);
      if (t->pool) t->base->vkDestroyCommandPool(t->base->device, t->pool, NULL);
      t->base->vkDestroyDevice(t->base->device, NULL); t->base->device = VK_NULL_HANDLE;
      if (t->base->errors) success = 0;
   }
   rpy_vk_free(t->base); free(t);
   return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
