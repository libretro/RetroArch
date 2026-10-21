/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <math.h>
#include <formats/rwebm_video.h>
#include <stdarg.h>
#include <rthreads/rthreads.h>
#include <queues/mpsc_stack.h>
#include "../../../gfx/common/vksym.h"
#include "../../../gfx/common/vulkan_common.h"
#include "../../../gfx/video_driver.h"
#include "../../../gfx/drivers_shader/shader_vulkan.h"
#include "../../../gfx/common/vulkan_video_import.h"
#include "../../../gfx/common/vulkan_video_planes.h"
#define VK_STREAM_SLOTS 2
#define VK_PLANAR_HEADER VULKAN_VIDEO_HEADER_WORDS
#if defined(_WIN32) && !defined(__WINRT__)
#include "import_win32_types.inc"
#endif
#include "import_types.inc"
typedef struct vk
{
   vulkan_context_t *context;
   VkCommandPool staging_pool;
   struct vk_stream_state *stream_states;
   struct { VkSampler nearest; } samplers;
   struct {
      VkPipeline yuv420_to_rgba8888, yuv_image_to_rgba8888;
      VkPipeline yuv420_to_rgba16f, yuv_image_to_rgba16f;
#if defined(_WIN32) && !defined(__WINRT__)
      VkPipeline yuv_packed_to_rgba8888, yuv_packed_to_rgba16f;
#endif
      VkPipelineLayout layout;
      VkDescriptorSetLayout set_layout;
   } pipelines;
} vk_t;

void RARCH_ERR(const char *format, ...)
{
   va_list args;
   va_start(args, format); vfprintf(stderr, format, args); va_end(args);
}
static unsigned import_errors;
static void vulkan_upload_batch_submit(vk_t *vk) { (void)vk; }
static void vulkan_check_device_lost(vk_t *vk, VkResult result)
{ (void)vk; if (result != VK_SUCCESS) import_errors++; }
#if defined(_WIN32) && !defined(__WINRT__)
#include "import_win32_functions.inc"
#endif
#include "import_functions.inc"

static unsigned plane_copies;
static PFN_vkCmdCopyImageToBuffer copy_image_original;
static void VKAPI_CALL oracle_copy_image(VkCommandBuffer cmd, VkImage image,
      VkImageLayout layout, VkBuffer buffer, uint32_t count, const VkBufferImageCopy *regions)
{
   plane_copies++;
   copy_image_original(cmd, image, layout, buffer, count, regions);
}


typedef struct import_oracle
{
   vk_t vk;
   struct vk_texture output;
   VkDeviceMemory output_memory;
   VkBuffer upload, readback;
   VkDeviceMemory upload_memory, readback_memory;
   uint8_t *mapped_upload, *mapped_readback;
   VkCommandBuffer cb;
   VkFence fence;
   VkSemaphore ready, done;
   VkPipeline ui_pipeline;
   unsigned width, height;
} import_oracle_t;

static int oracle_buffer(import_oracle_t *t, VkBuffer *buffer, VkDeviceMemory *memory,
      uint8_t **map, size_t size, VkBufferUsageFlags usage)
{
   VkBufferCreateInfo info = {0};
   VkMemoryAllocateInfo alloc = {0};
   VkMemoryRequirements requirements;
   VkDevice device = t->vk.context->device;
   uint32_t i;
   info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO; info.size = size; info.usage = usage;
   if (vkCreateBuffer(device, &info, NULL, buffer) != VK_SUCCESS) return 0;
   vkGetBufferMemoryRequirements(device, *buffer, &requirements);
   for (i = 0; i < t->vk.context->memory_properties.memoryTypeCount; i++)
      if ((requirements.memoryTypeBits & (UINT32_C(1) << i))
            && (t->vk.context->memory_properties.memoryTypes[i].propertyFlags
               & (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
               == (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) break;
   if (i == t->vk.context->memory_properties.memoryTypeCount) return 0;
   alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO; alloc.allocationSize = requirements.size; alloc.memoryTypeIndex = i;
   return vkAllocateMemory(device, &alloc, NULL, memory) == VK_SUCCESS
      && vkBindBufferMemory(device, *buffer, *memory, 0) == VK_SUCCESS
      && vkMapMemory(device, *memory, 0, VK_WHOLE_SIZE, 0, (void**)map) == VK_SUCCESS;
}

static int oracle_image(import_oracle_t *t, VkImage *image, VkDeviceMemory *memory,
      VkFormat format, unsigned width, unsigned height, VkImageUsageFlags usage)
{
   VkImageCreateInfo info = {0};
   VkMemoryAllocateInfo alloc = {0};
   VkMemoryRequirements requirements;
   VkDevice device = t->vk.context->device;
   info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO; info.imageType = VK_IMAGE_TYPE_2D;
   info.format = format; info.extent.width = width; info.extent.height = height; info.extent.depth = 1;
   info.mipLevels = info.arrayLayers = 1; info.samples = VK_SAMPLE_COUNT_1_BIT;
   info.tiling = VK_IMAGE_TILING_OPTIMAL; info.usage = usage;
   if ((usage & VK_IMAGE_USAGE_SAMPLED_BIT) && (format == VK_FORMAT_G8_B8R8_2PLANE_420_UNORM
         || format == VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16))
      info.flags = VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
   if (vkCreateImage(device, &info, NULL, image) != VK_SUCCESS) return 0;
   vkGetImageMemoryRequirements(device, *image, &requirements);
   alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO; alloc.allocationSize = requirements.size;
   alloc.memoryTypeIndex = vulkan_find_memory_type_fallback(&t->vk.context->memory_properties,
         requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0);
   return vkAllocateMemory(device, &alloc, NULL, memory) == VK_SUCCESS
      && vkBindImageMemory(device, *image, *memory, 0) == VK_SUCCESS;
}

static int oracle_begin(import_oracle_t *t)
{
   VkCommandBufferBeginInfo info = {0};
   info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO; info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
   return vkResetCommandBuffer(t->cb, 0) == VK_SUCCESS && vkBeginCommandBuffer(t->cb, &info) == VK_SUCCESS;
}
static int oracle_submit(import_oracle_t *t)
{
   VkSubmitInfo info = {0};
   VkDevice device = t->vk.context->device;
   info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO; info.commandBufferCount = 1; info.pCommandBuffers = &t->cb;
   return vkEndCommandBuffer(t->cb) == VK_SUCCESS && vkResetFences(device, 1, &t->fence) == VK_SUCCESS
      && vkQueueSubmit(t->vk.context->queue, 1, &info, t->fence) == VK_SUCCESS
      && vkWaitForFences(device, 1, &t->fence, VK_TRUE, UINT64_C(1000000000)) == VK_SUCCESS;
}

#include "../d3d_p010_import/fp16_reference.h"
static int output_fp16;
static int output_ui_roundtrip;

static int oracle_pipeline(import_oracle_t *t)
{
   static const uint32_t spirv[] =
#include "../../../gfx/drivers/vulkan_shaders/yuv420_to_rgba8888.comp.inc"
   ;
   static const uint32_t sampled_spirv[] =
#include "../../../gfx/drivers/vulkan_shaders/yuv_image_to_rgba8888.comp.inc"
   ;
   static const uint32_t float_spirv[] =
#include "../../../gfx/drivers/vulkan_shaders/yuv420_to_rgba16f.comp.inc"
   ;
   static const uint32_t float_sampled_spirv[] =
#include "../../../gfx/drivers/vulkan_shaders/yuv_image_to_rgba16f.comp.inc"
   ;
   static const uint32_t ui_spirv[] =
#include "ui_roundtrip.comp.inc"
   ;
   VkSamplerCreateInfo sampler = {0};
   VkDescriptorSetLayoutBinding bindings[4];
   VkDescriptorSetLayoutCreateInfo set = {0};
   VkPipelineLayoutCreateInfo layout = {0};
   VkPushConstantRange push = {VK_SHADER_STAGE_COMPUTE_BIT, 0, VULKAN_VIDEO_HEADER_WORDS * 4};
   VkShaderModuleCreateInfo module = {0};
   VkComputePipelineCreateInfo pipeline = {0};
   VkShaderModule shader;
   VkDevice device = t->vk.context->device;
   int ok;
   memset(bindings, 0, sizeof(bindings));
   bindings[0].binding = 3; bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
   bindings[0].descriptorCount = 1; bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
   bindings[1] = bindings[0]; bindings[1].binding = 4; bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
   bindings[2] = bindings[0]; bindings[2].binding = 1;
   bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
   bindings[3] = bindings[2]; bindings[3].binding = 2;
   set.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO; set.bindingCount = 4; set.pBindings = bindings;
   if (vkCreateDescriptorSetLayout(device, &set, NULL, &t->vk.pipelines.set_layout) != VK_SUCCESS) return 0;
   layout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO; layout.setLayoutCount = 1; layout.pSetLayouts = &t->vk.pipelines.set_layout;
   layout.pushConstantRangeCount = 1; layout.pPushConstantRanges = &push;
   if (vkCreatePipelineLayout(device, &layout, NULL, &t->vk.pipelines.layout) != VK_SUCCESS) return 0;
   module.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO; module.codeSize = sizeof(spirv); module.pCode = spirv;
   if (vkCreateShaderModule(device, &module, NULL, &shader) != VK_SUCCESS) return 0;
   pipeline.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
   pipeline.layout = t->vk.pipelines.layout; pipeline.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
   pipeline.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT; pipeline.stage.module = shader; pipeline.stage.pName = "main";
   ok = vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipeline, NULL, &t->vk.pipelines.yuv420_to_rgba8888) == VK_SUCCESS;
   vkDestroyShaderModule(device, shader, NULL);
   module.codeSize = sizeof(sampled_spirv); module.pCode = sampled_spirv;
   if (!ok || vkCreateShaderModule(device, &module, NULL, &shader) != VK_SUCCESS) return 0;
   pipeline.stage.module = shader;
   ok = vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipeline, NULL,
         &t->vk.pipelines.yuv_image_to_rgba8888) == VK_SUCCESS;
   vkDestroyShaderModule(device, shader, NULL);
   module.codeSize = sizeof(float_spirv); module.pCode = float_spirv;
   if (!ok || vkCreateShaderModule(device, &module, NULL, &shader) != VK_SUCCESS) return 0;
   pipeline.stage.module = shader;
   ok = vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipeline, NULL,
         &t->vk.pipelines.yuv420_to_rgba16f) == VK_SUCCESS;
   vkDestroyShaderModule(device, shader, NULL);
   module.codeSize = sizeof(float_sampled_spirv); module.pCode = float_sampled_spirv;
   if (!ok || vkCreateShaderModule(device, &module, NULL, &shader) != VK_SUCCESS) return 0;
   pipeline.stage.module = shader;
   ok = vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipeline, NULL,
         &t->vk.pipelines.yuv_image_to_rgba16f) == VK_SUCCESS;
   vkDestroyShaderModule(device, shader, NULL);
   if (output_ui_roundtrip)
   {
      module.codeSize = sizeof(ui_spirv); module.pCode = ui_spirv;
      if (!ok || vkCreateShaderModule(device, &module, NULL, &shader) != VK_SUCCESS) return 0;
      pipeline.stage.module = shader;
      ok = vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipeline, NULL,
            &t->ui_pipeline) == VK_SUCCESS;
      vkDestroyShaderModule(device, shader, NULL);
   }
   sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
   sampler.magFilter = sampler.minFilter = VK_FILTER_NEAREST;
   sampler.addressModeU = sampler.addressModeV = sampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
   return ok && vkCreateSampler(device, &sampler, NULL, &t->vk.samplers.nearest) == VK_SUCCESS;
}

#ifndef HEVC_RENDER_TEST
static unsigned oracle_sample(unsigned plane, unsigned x, unsigned y, unsigned depth, unsigned full)
{
   unsigned shift = depth == 10 ? 2 : 0;
   if (!plane)
   {
      if (!y && x == 0) return full ? 0 : 16u << shift;
      if (!y && x == 1) return full ? (1u << depth) - 1 : 235u << shift;
      return (64 + (x * 37 + y * 23) % 150) << shift;
   }
   if (!x && !y) return 128u << shift;
   return (64 + (x * 29 + y * 43 + plane * 31) % 128) << shift;
}

static VkResult VKAPI_CALL oracle_busy(VkDevice device, VkFence fence)
{ (void)device; (void)fence; return VK_NOT_READY; }

static VkFence growth_busy_fence;
static PFN_vkGetFenceStatus growth_fence_status;
static VkResult VKAPI_CALL oracle_growth_busy(VkDevice d, VkFence f)
{ return f == growth_busy_fence ? VK_NOT_READY : growth_fence_status(d, f); }

static unsigned header_updates, header_pushes;
static PFN_vkCmdUpdateBuffer header_update;
static PFN_vkCmdPushConstants header_push;
static void VKAPI_CALL oracle_header_update(VkCommandBuffer c, VkBuffer b,
      VkDeviceSize o, VkDeviceSize n, const void *p)
{
   header_updates++;
   header_update(c, b, o, n, p);
}
static void VKAPI_CALL oracle_header_push(VkCommandBuffer c, VkPipelineLayout l,
      VkShaderStageFlags f, uint32_t o, uint32_t n, const void *p)
{
   header_pushes++;
   header_push(c, l, f, o, n, p);
}

static unsigned import_allocations;
static VkDeviceSize import_largest_buffer;
static PFN_vkCreateBuffer import_create_buffer;
static VkResult VKAPI_CALL oracle_import_allocation(VkDevice d, const VkBufferCreateInfo *i,
      const VkAllocationCallbacks *a, VkBuffer *b)
{
   import_allocations++;
   if (i->size > import_largest_buffer) import_largest_buffer = i->size;
   return import_create_buffer(d, i, a, b);
}

static unsigned buffer_fault, buffer_fault_hits, buffer_destroys, memory_frees;
static PFN_vkCreateBuffer buffer_create;
static PFN_vkAllocateMemory memory_allocate;
static PFN_vkBindBufferMemory buffer_bind;
static PFN_vkDestroyBuffer buffer_destroy;
static PFN_vkFreeMemory memory_free;
static VkResult VKAPI_CALL oracle_buffer_fault(VkDevice d, const VkBufferCreateInfo *i,
      const VkAllocationCallbacks *a, VkBuffer *b)
{
   if (buffer_fault == 1) { buffer_fault_hits++; *b = VK_NULL_HANDLE; return VK_ERROR_OUT_OF_DEVICE_MEMORY; }
   return buffer_create(d, i, a, b);
}
static VkResult VKAPI_CALL oracle_memory_fault(VkDevice d, const VkMemoryAllocateInfo *i,
      const VkAllocationCallbacks *a, VkDeviceMemory *m)
{
   if (buffer_fault == 2) { buffer_fault_hits++; *m = VK_NULL_HANDLE; return VK_ERROR_OUT_OF_DEVICE_MEMORY; }
   return memory_allocate(d, i, a, m);
}
static VkResult VKAPI_CALL oracle_bind_fault(VkDevice d, VkBuffer b, VkDeviceMemory m, VkDeviceSize o)
{
   if (buffer_fault == 3) { buffer_fault_hits++; return VK_ERROR_OUT_OF_DEVICE_MEMORY; }
   return buffer_bind(d, b, m, o);
}
static void VKAPI_CALL oracle_buffer_destroy(VkDevice d, VkBuffer b, const VkAllocationCallbacks *a)
{ buffer_destroys++; buffer_destroy(d, b, a); }
static void VKAPI_CALL oracle_memory_free(VkDevice d, VkDeviceMemory m, const VkAllocationCallbacks *a)
{ memory_frees++; memory_free(d, m, a); }

static int oracle_growth_failures(import_oracle_t *t, const video_frame_import_t *frame)
{
   struct vk_stream_state *st = t->vk.stream_states;
   unsigned slot, i;
   VkBuffer old_buffer;
   VkDeviceMemory old_memory;
   size_t old_size;
   uint64_t before = 0, after = 0;
   uint32_t limit = t->vk.context->gpu_properties.limits.maxStorageBufferRange;
   PFN_vkGetSemaphoreCounterValueKHR counter = (PFN_vkGetSemaphoreCounterValueKHR)
      vkGetDeviceProcAddr(t->vk.context->device, "vkGetSemaphoreCounterValueKHR");
   int ok = counter && counter(t->vk.context->device, t->done, &before) == VK_SUCCESS;
   if (!st) return 0;
   slot = st->next_slot;
   old_buffer = st->staging[slot].buffer;
   old_memory = st->staging[slot].memory;
   old_size = st->staging[slot].size;
   t->vk.context->gpu_properties.limits.maxStorageBufferRange = VULKAN_VIDEO_HEADER_WORDS * 4;
   if (vulkan_import_convert(&t->vk, &t->output, frame) != VIDEO_TEXTURE_UPDATE_REFUSED) ok = 0;
   t->vk.context->gpu_properties.limits.maxStorageBufferRange = limit;
   buffer_create = vulkan_symbol_wrapper_vkCreateBuffer;
   memory_allocate = vulkan_symbol_wrapper_vkAllocateMemory;
   buffer_bind = vulkan_symbol_wrapper_vkBindBufferMemory;
   buffer_destroy = vulkan_symbol_wrapper_vkDestroyBuffer;
   memory_free = vulkan_symbol_wrapper_vkFreeMemory;
   for (i = 1; i <= 3; i++)
   {
      enum video_texture_update result;
      buffer_fault = i; buffer_fault_hits = buffer_destroys = memory_frees = 0;
      vulkan_symbol_wrapper_vkCreateBuffer = oracle_buffer_fault;
      vulkan_symbol_wrapper_vkAllocateMemory = oracle_memory_fault;
      vulkan_symbol_wrapper_vkBindBufferMemory = oracle_bind_fault;
      vulkan_symbol_wrapper_vkDestroyBuffer = oracle_buffer_destroy;
      vulkan_symbol_wrapper_vkFreeMemory = oracle_memory_free;
      result = vulkan_import_convert(&t->vk, &t->output, frame);
      vulkan_symbol_wrapper_vkCreateBuffer = buffer_create;
      vulkan_symbol_wrapper_vkAllocateMemory = memory_allocate;
      vulkan_symbol_wrapper_vkBindBufferMemory = buffer_bind;
      vulkan_symbol_wrapper_vkDestroyBuffer = buffer_destroy;
      vulkan_symbol_wrapper_vkFreeMemory = memory_free;
      if (result != VIDEO_TEXTURE_UPDATE_REFUSED || buffer_fault_hits != 1
            || buffer_destroys != (i > 1 ? 1u : 0u) || memory_frees != (i == 3 ? 1u : 0u)
            || st->next_slot != slot || st->staging[slot].buffer != old_buffer
            || st->staging[slot].memory != old_memory || st->staging[slot].size != old_size) ok = 0;
   }
   buffer_fault = 0;
   if (!counter || counter(t->vk.context->device, t->done, &after) != VK_SUCCESS || before != after) ok = 0;
   printf("Vulkan import growth failures preserve idle slot and lease: %s\n", ok ? "PASS" : "FAIL");
   return ok;
}

static int oracle_case(import_oracle_t *t, unsigned depth, unsigned flags, uint64_t serial, unsigned mode, unsigned transfer)
{
   vulkan_video_layout_t layout;
   video_frame_import_t frame;
   VkImageLayout input_layout = getenv("VULKAN_TEST_GENERAL_IMPORT")
      ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
   VkImage input = VK_NULL_HANDLE;
   VkImageView planes[2] = {VK_NULL_HANDLE, VK_NULL_HANDLE};
   VkPipeline sampled_pipeline = t->vk.pipelines.yuv_image_to_rgba8888;
   VkPipeline sampled_float = t->vk.pipelines.yuv_image_to_rgba16f;
   VkDeviceMemory memory = VK_NULL_HANDLE;
   VkBufferImageCopy regions[2];
   VkSemaphoreSignalInfoKHR signal = {0};
   VkDevice device = t->vk.context->device;
   unsigned plane, x, y, bps = depth == 10 ? 2 : 1;
   unsigned full = !!(flags & IMAGE_YUV_FLAG_FULL_RANGE);
   int ok = 0;
   PFN_vkSignalSemaphoreKHR signal_sem = (PFN_vkSignalSemaphoreKHR)vkGetDeviceProcAddr(device, "vkSignalSemaphoreKHR");
   PFN_vkGetSemaphoreCounterValueKHR counter = (PFN_vkGetSemaphoreCounterValueKHR)vkGetDeviceProcAddr(device, "vkGetSemaphoreCounterValueKHR");
   uint64_t value = 0;
   uint32_t saved_limit = t->vk.context->gpu_properties.limits.maxStorageBufferRange;
   if (!signal_sem || !counter || !vulkan_video_layout(&layout, t->width, t->height,
            depth == 10 ? IMAGE_PIXFMT_P010 : IMAGE_PIXFMT_NV12, flags)) goto done;
   memset(t->mapped_upload, 0, layout.bytes);
   for (plane = 0; plane < 3; plane++)
      for (y = 0; y < (plane ? (t->height + 1) / 2 : t->height); y++)
         for (x = 0; x < (plane ? (t->width + 1) / 2 : t->width); x++)
         {
            size_t offset = plane ? layout.header[4] + (y * ((t->width + 1) / 2) * 2 + 2 * x + plane - 1) * bps
               : layout.header[2] + (y * t->width + x) * bps;
            unsigned sample = oracle_sample(plane, x, y, depth, full);
            if (bps == 2) sample <<= 6;
            t->mapped_upload[offset] = (uint8_t)sample;
            if (bps == 2) t->mapped_upload[offset + 1] = (uint8_t)(sample >> 8);
         }
   if (!oracle_image(t, &input, &memory, depth == 10 ? VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16
            : VK_FORMAT_G8_B8R8_2PLANE_420_UNORM, (t->width + 1) & ~1u, (t->height + 1) & ~1u,
            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
               | (mode ? VK_IMAGE_USAGE_SAMPLED_BIT : 0)) || !oracle_begin(t)) goto done;
   VULKAN_IMAGE_LAYOUT_TRANSITION(t->cb, input, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
         0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
   memset(regions, 0, sizeof(regions));
   regions[0].bufferOffset = layout.header[2]; regions[0].imageSubresource.aspectMask = VK_IMAGE_ASPECT_PLANE_0_BIT;
   regions[0].imageSubresource.layerCount = 1; regions[0].imageExtent.width = t->width;
   regions[0].imageExtent.height = t->height; regions[0].imageExtent.depth = 1;
   regions[1] = regions[0]; regions[1].bufferOffset = layout.header[4];
   regions[1].imageSubresource.aspectMask = VK_IMAGE_ASPECT_PLANE_1_BIT;
   regions[1].imageExtent.width = (t->width + 1) / 2; regions[1].imageExtent.height = (t->height + 1) / 2;
   vkCmdCopyBufferToImage(t->cb, t->upload, input, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 2, regions);
   VULKAN_IMAGE_LAYOUT_TRANSITION(t->cb, input, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, input_layout,
         VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
   if (!oracle_submit(t)) goto done;
   memset(&frame, 0, sizeof(frame));
   memcpy(&frame.image, &input, sizeof(input)); memcpy(&frame.wait, &t->ready, sizeof(t->ready));
   memcpy(&frame.done, &t->done, sizeof(t->done));
   frame.wait_value = frame.done_value = serial; frame.dims = VIDEO_SCALE_PACK(t->width, t->height);
   frame.output_pixfmt = output_fp16 ? IMAGE_PIXFMT_FP16 : 0;
   frame.pixfmt = depth == 10 ? IMAGE_PIXFMT_P010 : IMAGE_PIXFMT_NV12;
   frame.yuv = flags; frame.layout = input_layout;
   if (transfer)
   {
      frame.color.transfer = transfer;
      frame.color.matrix = frame.color.primaries = 9;
      frame.color.max_cll = 1000;
   }
   if (mode)
   {
      if (!vulkan_video_plane_views(device, input, frame.pixfmt, planes)) goto done;
      memcpy(&frame.plane_views[0], &planes[0], sizeof(VkImageView));
      memcpy(&frame.plane_views[1], &planes[1], sizeof(VkImageView));
   }
   if (mode == 2)
   {
      t->vk.pipelines.yuv_image_to_rgba8888 = VK_NULL_HANDLE;
      t->vk.pipelines.yuv_image_to_rgba16f = VK_NULL_HANDLE;
   }
   signal.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO_KHR; signal.semaphore = t->ready; signal.value = serial;
   if (signal_sem(device, &signal) != VK_SUCCESS) goto done;
   if (serial == 13 && !oracle_growth_failures(t, &frame)) goto done;
   if (mode == 1) t->vk.context->gpu_properties.limits.maxStorageBufferRange = 0;
   plane_copies = 0;
   copy_image_original = vulkan_symbol_wrapper_vkCmdCopyImageToBuffer;
   vulkan_symbol_wrapper_vkCmdCopyImageToBuffer = oracle_copy_image;
   header_updates = header_pushes = 0;
   header_update = vulkan_symbol_wrapper_vkCmdUpdateBuffer;
   header_push = vulkan_symbol_wrapper_vkCmdPushConstants;
   vulkan_symbol_wrapper_vkCmdUpdateBuffer = oracle_header_update;
   vulkan_symbol_wrapper_vkCmdPushConstants = oracle_header_push;
   import_allocations = 0; import_largest_buffer = 0;
   import_create_buffer = vulkan_symbol_wrapper_vkCreateBuffer;
   vulkan_symbol_wrapper_vkCreateBuffer = oracle_import_allocation;
   {
      enum video_texture_update result;
      VkBuffer held_buffer = VK_NULL_HANDLE;
      if (serial == 13)
      {
         struct vk_stream_state *st = t->vk.stream_states;
         growth_busy_fence = st->fence[st->next_slot];
         held_buffer = st->staging[st->next_slot].buffer;
         growth_fence_status = vulkan_symbol_wrapper_vkGetFenceStatus;
         vulkan_symbol_wrapper_vkGetFenceStatus = oracle_growth_busy;
      }
      frame.output_pixfmt = IMAGE_PIXFMT_P010;
      if (vulkan_import_convert(&t->vk, &t->output, &frame) != VIDEO_TEXTURE_UPDATE_REFUSED) goto done;
      frame.output_pixfmt = output_fp16 ? IMAGE_PIXFMT_8888 : IMAGE_PIXFMT_FP16;
      if (vulkan_import_convert(&t->vk, &t->output, &frame) != VIDEO_TEXTURE_UPDATE_REFUSED) goto done;
      frame.output_pixfmt = output_fp16 ? IMAGE_PIXFMT_FP16 : 0;
      result = vulkan_import_convert(&t->vk, &t->output, &frame);
      if (serial == 13)
      {
         struct vk_stream_state *st = t->vk.stream_states;
         unsigned held = st->fence[0] == growth_busy_fence ? 0 : 1;
         vulkan_symbol_wrapper_vkGetFenceStatus = growth_fence_status;
         if (st->staging[held].buffer != held_buffer || st->staging[held].size != 0) result = VIDEO_TEXTURE_UPDATE_REFUSED;
      }
      vulkan_symbol_wrapper_vkCmdCopyImageToBuffer = copy_image_original;
      vulkan_symbol_wrapper_vkCreateBuffer = import_create_buffer;
      vulkan_symbol_wrapper_vkCmdUpdateBuffer = header_update;
      vulkan_symbol_wrapper_vkCmdPushConstants = header_push;
      if (header_updates != (mode == 1 ? 0u : 1u)
            || header_pushes != (mode == 1 ? 1u : 0u)) result = VIDEO_TEXTURE_UPDATE_REFUSED;
      if ((serial <= 12 && import_allocations)
            || (serial >= 18 && import_allocations)) result = VIDEO_TEXTURE_UPDATE_REFUSED;
      t->vk.context->gpu_properties.limits.maxStorageBufferRange = saved_limit;
      if (serial <= 12 && (t->vk.stream_states->staging[0].size != 0
            || t->vk.stream_states->staging[1].size != 0)) goto done;
      if (result != VIDEO_TEXTURE_UPDATE_DONE || plane_copies != (mode == 1 ? 0u : 1u)) goto done;
   }
   if (!oracle_begin(t)) goto done;
   if (output_ui_roundtrip)
   {
      static const float white[4] = {80,203,400,1000};
      float params[4];
      struct vk_stream_state *state = t->vk.stream_states;
      params[0] = white[(serial / 4) % 4];
      params[1] = (float)(serial % 4);
      params[2] = (float)((serial / 16) % 2 + 1); params[3] = 0;
      VULKAN_IMAGE_LAYOUT_TRANSITION(t->cb, t->output.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
      vkCmdBindPipeline(t->cb, VK_PIPELINE_BIND_POINT_COMPUTE, t->ui_pipeline);
      vkCmdBindDescriptorSets(t->cb, VK_PIPELINE_BIND_POINT_COMPUTE, t->vk.pipelines.layout,
            0, 1, &state->set[0], 0, NULL);
      vkCmdPushConstants(t->cb, t->vk.pipelines.layout, VK_SHADER_STAGE_COMPUTE_BIT,
            0, sizeof(params), params);
      vkCmdDispatch(t->cb, t->width, t->height, 1);
      VULKAN_IMAGE_LAYOUT_TRANSITION(t->cb, t->output.image, VK_IMAGE_LAYOUT_GENERAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
   }
   VULKAN_IMAGE_LAYOUT_TRANSITION(t->cb, t->output.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
         VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
   memset(regions, 0, sizeof(regions)); regions[0].imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
   regions[0].imageSubresource.layerCount = 1; regions[0].imageExtent.width = t->width;
   regions[0].imageExtent.height = t->height; regions[0].imageExtent.depth = 1;
   vkCmdCopyImageToBuffer(t->cb, t->output.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, t->readback, 1, regions);
   VULKAN_IMAGE_LAYOUT_TRANSITION(t->cb, t->output.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_READ_BIT,
         VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
   {
      VkBufferMemoryBarrier barrier = {0};
      barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
      barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT; barrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
      barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      barrier.buffer = t->readback; barrier.size = VK_WHOLE_SIZE;
      vkCmdPipelineBarrier(t->cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
            0, 0, NULL, 1, &barrier, 0, NULL);
   }
   if (!oracle_submit(t) || counter(device, t->done, &value) != VK_SUCCESS || value != serial) goto done;
   ok = 1;
   for (y = 0; y < t->height; y++)
      for (x = 0; x < t->width; x++)
      {
         double scale = full ? (double)((1u << depth) - 1) : (double)(219u << (depth == 10 ? 2 : 0));
         double chroma_scale = full ? scale : (double)(224u << (depth == 10 ? 2 : 0));
         double yy = (oracle_sample(0, x, y, depth, full) - (full ? 0.0 : (double)(16u << (depth == 10 ? 2 : 0)))) / scale;
         double cb = (oracle_sample(1, x / 2, y / 2, depth, full) - (double)(1u << (depth - 1))) / chroma_scale;
         double cr = (oracle_sample(2, x / 2, y / 2, depth, full) - (double)(1u << (depth - 1))) / chroma_scale;
         double rgb[3];
         rgb[0] = yy + ((flags & IMAGE_YUV_FLAG_BT709) ? 1.5748 : 1.402) * cr;
         rgb[1] = yy - ((flags & IMAGE_YUV_FLAG_BT709) ? 0.187324 : 0.344136) * cb
            - ((flags & IMAGE_YUV_FLAG_BT709) ? 0.468124 : 0.714136) * cr;
         rgb[2] = yy + ((flags & IMAGE_YUV_FLAG_BT709) ? 1.8556 : 1.772) * cb;
         if (output_fp16)
         {
            float expected[3];
            const uint16_t *pixel = (const uint16_t*)t->mapped_readback + (y * t->width + x) * 4;
            fp16_reference(depth, full, transfer, flags,
                  oracle_sample(0, x, y, depth, full),
                  oracle_sample(1, x / 2, y / 2, depth, full),
                  oracle_sample(2, x / 2, y / 2, depth, full), expected);
            if (output_ui_roundtrip)
            {
               double peak = fabs(expected[0]);
               if (fabs(expected[1]) > peak) peak = fabs(expected[1]);
               if (fabs(expected[2]) > peak) peak = fabs(expected[2]);
               for (plane = 0; plane < 4; plane++)
               {
                  double actual = fp16_reference_half(pixel[plane]);
                  double ref = plane == 3 ? 1.0 : expected[plane];
                  if (actual != actual || fabs(actual - ref) > .003 + .005 * peak)
                  {
                     printf("UI roundtrip %u,%u/%u: %.6f != %.6f\n", x, y, plane, actual, ref);
                     ok = 0;
                  }
               }
            }
            else if (!fp16_reference_check(pixel, expected)) ok = 0;
            continue;
         }
         if (transfer)
         {
            uint16_t yy = (uint16_t)(oracle_sample(0, x, y, depth, full) << (depth == 10 ? 0 : 2));
            uint16_t uu = (uint16_t)(oracle_sample(1, x / 2, y / 2, depth, full) << (depth == 10 ? 0 : 2));
            uint16_t vv = (uint16_t)(oracle_sample(2, x / 2, y / 2, depth, full) << (depth == 10 ? 0 : 2));
            uint32_t pixel;
            rwebm_video_blit_i420_hbd(&pixel, 1, 1, 1, &yy, 1, &uu, &vv, 1,
                  9, transfer, full ? 2 : 1, 1000, 1);
            for (plane = 0; plane < 3; plane++)
               rgb[plane] = ((pixel >> (plane * 8)) & 255) / 255.0;
         }
         for (plane = 0; plane < 3; plane++)
         {
            int expected = (int)floor((rgb[plane] < 0 ? 0 : rgb[plane] > 1 ? 1 : rgb[plane]) * 255.0 + 0.5);
            int actual = t->mapped_readback[(y * t->width + x) * 4 + plane];
            if (abs(actual - expected) > (transfer ? 4 : 1)) { printf("pixel %u,%u/%u: %d != %d\n", x, y, plane, actual, expected); ok = 0; }
         }
         if (t->mapped_readback[(y * t->width + x) * 4 + 3] != 255) ok = 0;
      }
   {
      PFN_vkGetFenceStatus original = vulkan_symbol_wrapper_vkGetFenceStatus;
      vulkan_symbol_wrapper_vkGetFenceStatus = oracle_busy;
      frame.done_value = serial + 1;
      if (vulkan_import_convert(&t->vk, &t->output, &frame) != VIDEO_TEXTURE_UPDATE_DROPPED) ok = 0;
      vulkan_symbol_wrapper_vkGetFenceStatus = original;
      if (counter(device, t->done, &value) != VK_SUCCESS || value != serial) ok = 0;
   }
done:
   t->vk.context->gpu_properties.limits.maxStorageBufferRange = saved_limit;
   t->vk.pipelines.yuv_image_to_rgba8888 = sampled_pipeline;
   t->vk.pipelines.yuv_image_to_rgba16f = sampled_float;
   vkDeviceWaitIdle(device);
   if (planes[0]) vkDestroyImageView(device, planes[0], NULL);
   if (planes[1]) vkDestroyImageView(device, planes[1], NULL);
   if (input) vkDestroyImage(device, input, NULL);
   if (memory) vkFreeMemory(device, memory, NULL);
   printf("Vulkan import %u-bit flags %u mode %u transfer %u: %s\n", depth, flags, mode, transfer, ok ? "PASS" : "FAIL");
   return ok;
}

#endif

static void oracle_cleanup(import_oracle_t *t)
{
   VkDevice device = t->vk.context->device;
   struct vk_stream_state *st = t->vk.stream_states;
   while (st)
   {
      struct vk_stream_state *next = st->next;
      unsigned i;
      for (i = 0; i < VK_STREAM_SLOTS; i++)
      {
         if (st->fence[i]) vkDestroyFence(device, st->fence[i], NULL);
         if (st->staging[i].buffer) vkDestroyBuffer(device, st->staging[i].buffer, NULL);
         if (st->staging[i].memory) vkFreeMemory(device, st->staging[i].memory, NULL);
      }
      if (st->pool) vkDestroyDescriptorPool(device, st->pool, NULL);
      free(st); st = next;
   }
   if (t->vk.staging_pool) vkDestroyCommandPool(device, t->vk.staging_pool, NULL);
   if (t->fence) vkDestroyFence(device, t->fence, NULL);
   if (t->ready) vkDestroySemaphore(device, t->ready, NULL);
   if (t->done) vkDestroySemaphore(device, t->done, NULL);
   if (t->output.view) vkDestroyImageView(device, t->output.view, NULL);
   if (t->output.image) vkDestroyImage(device, t->output.image, NULL);
   if (t->output_memory) vkFreeMemory(device, t->output_memory, NULL);
   if (t->mapped_upload) vkUnmapMemory(device, t->upload_memory);
   if (t->mapped_readback) vkUnmapMemory(device, t->readback_memory);
   if (t->upload) vkDestroyBuffer(device, t->upload, NULL);
   if (t->readback) vkDestroyBuffer(device, t->readback, NULL);
   if (t->upload_memory) vkFreeMemory(device, t->upload_memory, NULL);
   if (t->readback_memory) vkFreeMemory(device, t->readback_memory, NULL);
   if (t->ui_pipeline) vkDestroyPipeline(device, t->ui_pipeline, NULL);
   if (t->vk.pipelines.yuv420_to_rgba16f) vkDestroyPipeline(device, t->vk.pipelines.yuv420_to_rgba16f, NULL);
   if (t->vk.pipelines.yuv_image_to_rgba16f) vkDestroyPipeline(device, t->vk.pipelines.yuv_image_to_rgba16f, NULL);
   if (t->vk.pipelines.yuv420_to_rgba8888) vkDestroyPipeline(device, t->vk.pipelines.yuv420_to_rgba8888, NULL);
   if (t->vk.pipelines.yuv_image_to_rgba8888) vkDestroyPipeline(device, t->vk.pipelines.yuv_image_to_rgba8888, NULL);
   if (t->vk.samplers.nearest) vkDestroySampler(device, t->vk.samplers.nearest, NULL);
   if (t->vk.pipelines.layout) vkDestroyPipelineLayout(device, t->vk.pipelines.layout, NULL);
   if (t->vk.pipelines.set_layout) vkDestroyDescriptorSetLayout(device, t->vk.pipelines.set_layout, NULL);
}

#ifndef HEVC_RENDER_TEST
static int import_test_gpu_format(VkInstance instance, VkPhysicalDevice gpu,
      uint32_t family, PFN_vkGetInstanceProcAddr get)
{
   static const char *extensions[] = {"VK_KHR_timeline_semaphore"};
   VkPhysicalDeviceFeatures2 features = {0};
   VkPhysicalDeviceTimelineSemaphoreFeaturesKHR timeline = {0};
   VkDeviceQueueCreateInfo queue = {0};
   VkDeviceCreateInfo info = {0};
   VkCommandPoolCreateInfo pool = {0};
   VkCommandBufferAllocateInfo cb = {0};
   VkFenceCreateInfo fence = {0};
   VkSemaphoreTypeCreateInfoKHR type = {0};
   VkSemaphoreCreateInfo semaphore = {0};
   VkImageViewCreateInfo view = {0};
   VkDevice device;
   vulkan_context_t *ctx = (vulkan_context_t*)calloc(1, sizeof(*ctx));
   import_oracle_t *t = (import_oracle_t*)calloc(1, sizeof(*t));
   video_device_info_t advertised;
   struct vulkan_filter_chain_texture input;
   vulkan_video_layout_t capacity;
   float priority = 1.0f;
   unsigned i;
   int ok = 0;
   PFN_vkCreateDevice create = (PFN_vkCreateDevice)get(instance, "vkCreateDevice");
   PFN_vkGetPhysicalDeviceFeatures2 query_features = (PFN_vkGetPhysicalDeviceFeatures2)get(instance, "vkGetPhysicalDeviceFeatures2");
   if (!t || !ctx || !create || !query_features) goto free_host;
   timeline.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES_KHR;
   features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2; features.pNext = &timeline;
   query_features(gpu, &features);
   if (!timeline.timelineSemaphore) { puts("SKIP: no timeline semaphores for import"); ok = 1; goto free_host; }
   queue.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
   queue.queueFamilyIndex = family; queue.queueCount = 1; queue.pQueuePriorities = &priority;
   info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO; info.pNext = &timeline;
   info.queueCreateInfoCount = 1; info.pQueueCreateInfos = &queue;
   info.enabledExtensionCount = 1; info.ppEnabledExtensionNames = extensions;
   if (create(gpu, &info, NULL, &device) != VK_SUCCESS) goto free_host;
   vulkan_symbol_wrapper_init(get);
   if (!vulkan_symbol_wrapper_load_core_instance_symbols(instance)
         || !vulkan_symbol_wrapper_load_core_device_symbols(device)) goto destroy_device;
   ctx->instance = instance; ctx->gpu = gpu; ctx->device = device; ctx->graphics_queue_index = family;
   ctx->flags = VK_CTX_FLAG_TIMELINE_SEMAPHORE;
   vkGetPhysicalDeviceProperties(gpu, &ctx->gpu_properties);
   vkGetPhysicalDeviceMemoryProperties(gpu, &ctx->memory_properties);
   vkGetDeviceQueue(device, family, 0, &ctx->queue);
   t->vk.context = ctx; t->width = 5; t->height = 3;
   t->output.dims = VIDEO_SCALE_PACK(t->width, t->height);
   t->output.format = output_fp16 ? VK_FORMAT_R16G16B16A16_SFLOAT : VK_FORMAT_R8G8B8A8_UNORM;
   if (!vulkan_video_layout(&capacity, t->width, t->height, IMAGE_PIXFMT_P010, 0)
         || !oracle_buffer(t, &t->upload, &t->upload_memory, &t->mapped_upload, capacity.bytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT)
         || !oracle_buffer(t, &t->readback, &t->readback_memory, &t->mapped_readback, t->width * t->height * (output_fp16 ? 8 : 4), VK_BUFFER_USAGE_TRANSFER_DST_BIT)
         || !oracle_image(t, &t->output.image, &t->output_memory, t->output.format, t->width, t->height,
            VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT)) goto cleanup;
   view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO; view.image = t->output.image;
   view.viewType = VK_IMAGE_VIEW_TYPE_2D; view.format = t->output.format;
   view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
   view.subresourceRange.levelCount = view.subresourceRange.layerCount = 1;
   if (vkCreateImageView(device, &view, NULL, &t->output.view) != VK_SUCCESS || !oracle_pipeline(t)) goto cleanup;
   pool.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
   pool.queueFamilyIndex = family; pool.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
   if (vkCreateCommandPool(device, &pool, NULL, &t->vk.staging_pool) != VK_SUCCESS) goto cleanup;
   cb.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO; cb.commandPool = t->vk.staging_pool;
   cb.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; cb.commandBufferCount = 1;
   fence.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
   type.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO_KHR; type.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE_KHR;
   semaphore.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO; semaphore.pNext = &type;
   if (vkAllocateCommandBuffers(device, &cb, &t->cb) != VK_SUCCESS
         || vkCreateFence(device, &fence, NULL, &t->fence) != VK_SUCCESS
         || vkCreateSemaphore(device, &semaphore, NULL, &t->ready) != VK_SUCCESS
         || vkCreateSemaphore(device, &semaphore, NULL, &t->done) != VK_SUCCESS) goto cleanup;
   if (!vulkan_get_device(&t->vk, &advertised)
         || advertised.import_formats != (IMAGE_PIXFMT_NV12 | IMAGE_PIXFMT_P010)) goto cleanup;
   ok = 1;
   for (i = 0; i < 96; i++)
      if (!oracle_case(t, i % 3 == 1 ? 10 : 8,
               (((i % 12) / 3) & 1 ? IMAGE_YUV_FLAG_FULL_RANGE : 0)
               | (((i % 12) / 3) & 2 ? IMAGE_YUV_FLAG_BT709 : 0), i + 1, i % 48 < 12 || i % 48 >= 36 ? 1 : i % 48 < 24 ? 0 : 2, i < 48 ? 0 : i < 72 ? 16 : 18)) ok = 0;
   vulkan_movie_input(&t->output, &input);
   if (input.image != t->output.image || input.view != t->output.view
         || input.layout != t->output.layout || input.dims != t->output.dims
         || input.format != t->output.format
         || input.padded_dims || input.origin) ok = 0;
   if (import_errors) ok = 0;
cleanup:
   vkDeviceWaitIdle(device); oracle_cleanup(t);
destroy_device:
   vkDestroyDevice(device, NULL);
free_host:
   free(ctx); free(t);
   return ok;
}

static int import_test_gpu(VkInstance instance, VkPhysicalDevice gpu,
      unsigned family, PFN_vkGetInstanceProcAddr get)
{
   int ok;
   output_fp16 = 0;
   ok = import_test_gpu_format(instance, gpu, family, get);
   output_fp16 = 1;
   ok = import_test_gpu_format(instance, gpu, family, get) && ok;
   output_ui_roundtrip = 1;
   ok = import_test_gpu_format(instance, gpu, family, get) && ok;
   output_ui_roundtrip = 0;
   output_fp16 = 0;
   return ok;
}
#endif
