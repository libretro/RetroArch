/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#define main export_unit_main
#include "export_test.c"
#undef main
#include "../../../gfx/common/vaapi_hevc_vulkan_frame.h"
static vaapi_hevc_vulkan_frame_t vf;
static vaapi_hevc_export_t vf_returned, vf_snapshot;
static VkPhysicalDeviceMemoryProperties vf_memory;
static unsigned vf_fault, vf_layer, vf_plane, vf_images, vf_views, vf_allocations;
static unsigned vf_fds, vf_calls, vf_fence_calls, vf_barriers, vf_consumed, vf_closed;
static VkDeviceSize vf_sizes[8];
static VkResult vf_fence_result;
static bool vf_acquire;
static VkPipelineStageFlags vf_stage;
static VkDevice vf_device;
static unsigned vf_format_layer(VkFormat format)
{
   assert(format == VK_FORMAT_R8_UNORM || format == VK_FORMAT_R16_UNORM
         || format == VK_FORMAT_R8G8_UNORM || format == VK_FORMAT_R16G16_UNORM);
   return format == VK_FORMAT_R8G8_UNORM || format == VK_FORMAT_R16G16_UNORM;
}
static VKAPI_ATTR void VKAPI_CALL vf_format(VkPhysicalDevice gpu, VkFormat format, VkFormatProperties2 *out)
{
   VkDrmFormatModifierPropertiesListEXT *list = (VkDrmFormatModifierPropertiesListEXT*)out->pNext;
   unsigned layer = vf_format_layer(format);
   assert(gpu == (VkPhysicalDevice)(uintptr_t)1 && out->sType == VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2);
   assert(list && list->sType == VK_STRUCTURE_TYPE_DRM_FORMAT_MODIFIER_PROPERTIES_LIST_EXT);
   if (!list->pDrmFormatModifierProperties) { list->drmFormatModifierCount = 1; return; }
   assert(list->drmFormatModifierCount == 1);
   list->pDrmFormatModifierProperties[0].drmFormatModifier = ex_mode == 2 ? ((uint64_t)1<<56)|7 : 0;
   list->pDrmFormatModifierProperties[0].drmFormatModifierPlaneCount = ex_frame.descriptor.layers[layer].num_planes;
   list->pDrmFormatModifierProperties[0].drmFormatModifierTilingFeatures =
      VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_DISJOINT_BIT;
}
static VKAPI_ATTR VkResult VKAPI_CALL vf_format_image(VkPhysicalDevice gpu,
      const VkPhysicalDeviceImageFormatInfo2 *info, VkImageFormatProperties2 *out)
{
   VkExternalImageFormatProperties *memory = (VkExternalImageFormatProperties*)out->pNext;
   unsigned layer = vf_format_layer(info->format);
   assert(gpu == (VkPhysicalDevice)(uintptr_t)1 && memory);
   if (vf_fault == 4 && layer) return VK_ERROR_FORMAT_NOT_SUPPORTED;
   out->imageFormatProperties.maxExtent.width = out->imageFormatProperties.maxExtent.height = 64;
   out->imageFormatProperties.maxExtent.depth = out->imageFormatProperties.maxMipLevels = 1;
   out->imageFormatProperties.maxArrayLayers = 1;
   out->imageFormatProperties.sampleCounts = VK_SAMPLE_COUNT_1_BIT;
   out->imageFormatProperties.maxResourceSize = 65536;
   memory->externalMemoryProperties.externalMemoryFeatures = VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT;
   memory->externalMemoryProperties.compatibleHandleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;
   return VK_SUCCESS;
}
static VKAPI_ATTR VkResult VKAPI_CALL vf_create_image(VkDevice device, const VkImageCreateInfo *info,
      const VkAllocationCallbacks *a, VkImage *out)
{
   unsigned layer = vf_format_layer(info->format);
   assert(device == vf_device && !a && ex_pool.exports == 1 && !ex_closes);
   assert(info->extent.width == (layer ? 32u : 64u) && info->extent.height == info->extent.width);
   vf_calls++;
   if (vf_fault == 1 && layer) return VK_ERROR_OUT_OF_DEVICE_MEMORY;
   assert(!(vf_images & (1u << layer)));
   vf_images |= 1u << layer; *out = (VkImage)(uintptr_t)(100+layer);
   return VK_SUCCESS;
}
static VKAPI_ATTR void VKAPI_CALL vf_destroy_image(VkDevice device, VkImage image, const VkAllocationCallbacks *a)
{
   unsigned layer = (unsigned)(uintptr_t)image - 100;
   assert(device == vf_device && !a && layer < 2 && (vf_images & (1u << layer))
         && !(vf_views & (1u << layer)) && ex_pool.exports == 1 && !ex_closes);
   vf_calls++; vf_images &= ~(1u << layer);
}
static VKAPI_ATTR void VKAPI_CALL vf_requirements(VkDevice device, const VkImageMemoryRequirementsInfo2 *info,
      VkMemoryRequirements2 *out)
{
   vf_layer = (unsigned)(uintptr_t)info->image - 100; vf_plane = 0;
   assert(device == vf_device && vf_layer < 2 && (vf_images & (1u << vf_layer)));
   if (info->pNext)
   {
      const VkImagePlaneMemoryRequirementsInfo *plane = (const VkImagePlaneMemoryRequirementsInfo*)info->pNext;
      assert(plane->planeAspect == VK_IMAGE_ASPECT_MEMORY_PLANE_0_BIT_EXT
            || plane->planeAspect == VK_IMAGE_ASPECT_MEMORY_PLANE_1_BIT_EXT);
      vf_plane = plane->planeAspect == VK_IMAGE_ASPECT_MEMORY_PLANE_1_BIT_EXT;
   }
   out->memoryRequirements.size = 1024; out->memoryRequirements.alignment = 256;
   out->memoryRequirements.memoryTypeBits = 1;
}
static int vf_duplicate(int fd)
{
   unsigned index = 4*vf_layer+vf_plane, i;
   assert(fd >= 0 && fd < 4 && ex_live[fd] && !(vf_fds & (1u << index)));
   for (i = 0; i < ex_frame.descriptor.num_objects; i++)
      if (ex_frame.descriptor.objects[i].fd == fd) break;
   assert(i < ex_frame.descriptor.num_objects);
   vf_sizes[index] = ex_frame.descriptor.objects[i].size;
   vf_fds |= 1u << index;
   return (int)index+16;
}
static bool vf_size(int fd, VkDeviceSize *out)
{
   unsigned index = (unsigned)fd-16;
   assert(index < 8 && (vf_fds & (1u << index))); *out = vf_sizes[index]; return true;
}
static void vf_close(int fd)
{
   unsigned index = (unsigned)fd-16;
   assert(index < 8 && (vf_fds & (1u << index))); vf_fds &= ~(1u << index); vf_closed++;
}
static VKAPI_ATTR VkResult VKAPI_CALL vf_fd_properties(VkDevice device, VkExternalMemoryHandleTypeFlagBits type,
      int fd, VkMemoryFdPropertiesKHR *out)
{
   assert(device == vf_device && type == VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT && fd >= 16 && fd < 24);
   out->memoryTypeBits = 1; return VK_SUCCESS;
}
static VKAPI_ATTR VkResult VKAPI_CALL vf_allocate(VkDevice device, const VkMemoryAllocateInfo *info,
      const VkAllocationCallbacks *a, VkDeviceMemory *out)
{
   const VkImportMemoryFdInfoKHR *import = (const VkImportMemoryFdInfoKHR*)info->pNext;
   unsigned index = (unsigned)import->fd-16;
   assert(device == vf_device && !a && index < 8 && (vf_fds & (1u << index)));
   if (vf_fault == 2 && vf_layer) return VK_ERROR_OUT_OF_DEVICE_MEMORY;
   vf_fds &= ~(1u << index); vf_consumed++; vf_allocations |= 1u << index;
   *out = (VkDeviceMemory)(uintptr_t)(200+index); return VK_SUCCESS;
}
static VKAPI_ATTR void VKAPI_CALL vf_free(VkDevice device, VkDeviceMemory memory, const VkAllocationCallbacks *a)
{
   unsigned index = (unsigned)(uintptr_t)memory-200;
   assert(device == vf_device && !a && index < 8 && (vf_allocations & (1u << index)));
   assert(!(vf_images & (1u << (index/4))) && ex_pool.exports == 1 && !ex_closes);
   vf_allocations &= ~(1u << index); vf_calls++;
}
static VKAPI_ATTR VkResult VKAPI_CALL vf_bind(VkDevice device, uint32_t count, const VkBindImageMemoryInfo *info)
{
   assert(device == vf_device && count == (ex_mode == 2 ? 2u : 1u));
   assert(info[0].image == (VkImage)(uintptr_t)(100+vf_layer)); return VK_SUCCESS;
}
static VKAPI_ATTR VkResult VKAPI_CALL vf_create_view(VkDevice device, const VkImageViewCreateInfo *info,
      const VkAllocationCallbacks *a, VkImageView *out)
{
   unsigned layer = (unsigned)(uintptr_t)info->image-100;
   assert(device == vf_device && !a && layer < 2 && !(vf_views & (1u << layer)));
   if (vf_fault == 3 && layer) return VK_ERROR_OUT_OF_HOST_MEMORY;
   vf_views |= 1u << layer; *out = (VkImageView)(uintptr_t)(300+layer); return VK_SUCCESS;
}
static VKAPI_ATTR void VKAPI_CALL vf_destroy_view(VkDevice device, VkImageView view, const VkAllocationCallbacks *a)
{
   unsigned layer = (unsigned)(uintptr_t)view-300;
   assert(device == vf_device && !a && layer < 2 && (vf_views & (1u << layer)) && (vf_images & (1u << layer)));
   vf_views &= ~(1u << layer); vf_calls++;
}
static const vulkan_prime_image_ops_t vf_ops = { vf_create_image, vf_destroy_image, vf_requirements,
   vf_fd_properties, vf_allocate, vf_free, vf_bind, vf_create_view, vf_destroy_view, vf_duplicate, vf_size, vf_close };
static VKAPI_ATTR VkResult VKAPI_CALL vf_status(VkDevice device, VkFence fence)
{
   assert(device == vf_device && fence == (VkFence)(uintptr_t)9); vf_fence_calls++; return vf_fence_result;
}
static VKAPI_ATTR void VKAPI_CALL vf_barrier(VkCommandBuffer command, VkPipelineStageFlags src,
      VkPipelineStageFlags dst, VkDependencyFlags dependency, uint32_t memories,
      const VkMemoryBarrier *memory, uint32_t buffers, const VkBufferMemoryBarrier *buffer,
      uint32_t count, const VkImageMemoryBarrier *images)
{
   unsigned i;
   assert(command == (VkCommandBuffer)(uintptr_t)8 && !dependency && !memories && !memory && !buffers && !buffer && count == 2);
   assert(src == (vf_acquire ? VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT : vf_stage)
         && dst == (vf_acquire ? vf_stage : VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT));
   for (i = 0; i < 2; i++)
   {
      assert(images[i].sType == VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER && !images[i].pNext
            && images[i].image == (VkImage)(uintptr_t)(100+i));
      assert(images[i].srcQueueFamilyIndex == (vf_acquire ? VK_QUEUE_FAMILY_FOREIGN_EXT : 2)
            && images[i].dstQueueFamilyIndex == (vf_acquire ? 2 : VK_QUEUE_FAMILY_FOREIGN_EXT));
      assert(images[i].oldLayout == (vf_acquire ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
            && images[i].newLayout == (vf_acquire ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_GENERAL));
      assert(images[i].srcAccessMask == (vf_acquire ? 0 : VK_ACCESS_SHADER_READ_BIT)
            && images[i].dstAccessMask == (vf_acquire ? VK_ACCESS_SHADER_READ_BIT : 0));
      assert(images[i].subresourceRange.aspectMask == VK_IMAGE_ASPECT_COLOR_BIT
            && images[i].subresourceRange.levelCount == 1 && images[i].subresourceRange.layerCount == 1);
   }
   vf_barriers++;
}
static void vf_reset(void)
{
   assert(!vf.active && !vf_returned.active && !vf_images && !vf_views && !vf_allocations && !vf_fds);
   ex_reset(); assert(vaapi_hevc_export_open(&ex_frame, &ex_pool, 0, &ex_ops));
   vf_snapshot = ex_frame; vf_device = (VkDevice)(uintptr_t)1;
   memset(&vf_memory, 0, sizeof(vf_memory)); vf_memory.memoryTypeCount = vf_memory.memoryHeapCount = 1;
   vf_memory.memoryHeaps[0].size = 65536;
   vf_fault = vf_calls = vf_fence_calls = vf_barriers = vf_consumed = vf_closed = 0;
   vf_fence_result = VK_NOT_READY;
}
static bool vf_create(void)
{
   return vaapi_hevc_vulkan_frame_create(&vf, &ex_frame, (VkPhysicalDevice)(uintptr_t)1,
         vf_device, true, &vf_memory, &vf_ops, vf_format, vf_format_image);
}
static void vf_finish(void)
{
   assert(!vf.active && !vf_images && !vf_views && !vf_allocations && !vf_fds);
   assert(vf_returned.active && ex_pool.exports == 1 && vaapi_hevc_pool_held(&ex_pool) == 1 && !ex_closes);
   assert(vaapi_hevc_export_close(&vf_returned) && !ex_pool.exports && ex_closes == (ex_mode == 2 ? 4u : ex_mode == 1 ? 2u : 1u));
   assert(vaapi_hevc_pool_close(&ex_pool));
}
#ifndef VAAPI_PRIME_FRAME_TEST_MAIN
#define VAAPI_PRIME_FRAME_TEST_MAIN main
#endif
int VAAPI_PRIME_FRAME_TEST_MAIN(void)
{
   unsigned fault, i, calls;
   assert(!export_unit_main());
   for (ex_depth = 8; ex_depth <= 10; ex_depth += 2)
      for (ex_mode = 0; ex_mode < 3; ex_mode++)
      {
         for (fault = 1; fault <= 4; fault++)
         {
            vf_reset(); vf_fault = fault; assert(!vf_create());
            assert(!vf.active && !vf_images && !vf_views && !vf_allocations && !vf_fds);
            assert(!memcmp(&ex_frame, &vf_snapshot, sizeof(ex_frame)) && ex_pool.exports == 1 && !ex_closes);
            assert(vaapi_hevc_export_close(&ex_frame) && vaapi_hevc_pool_close(&ex_pool));
         }
         vf_reset(); assert(vf_create() && !ex_frame.active && vf.source.active && ex_pool.exports == 1);
         assert(vaapi_hevc_export_close(&ex_frame) && ex_pool.exports == 1);
         assert(!vaapi_hevc_vulkan_frame_track(&vf, VK_NULL_HANDLE));
         for (i = 0; i < 4; i++)
         {
            vf_acquire = !(i & 1); vf_stage = i < 2 ? VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT : VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            assert(vaapi_hevc_vulkan_frame_barrier(&vf, (VkCommandBuffer)(uintptr_t)8, vf_barrier, 2, vf_stage, vf_acquire));
         }
         assert(vf_barriers == 4 && !vaapi_hevc_vulkan_frame_barrier(&vf, (VkCommandBuffer)(uintptr_t)8,
                  vf_barrier, VK_QUEUE_FAMILY_FOREIGN_EXT, vf_stage, true));
         assert(!vaapi_hevc_vulkan_frame_barrier(&vf, (VkCommandBuffer)(uintptr_t)8, vf_barrier, 2, 0, true));
         assert(vaapi_hevc_vulkan_frame_track(&vf, (VkFence)(uintptr_t)9));
         assert(!vaapi_hevc_vulkan_frame_track(&vf, (VkFence)(uintptr_t)9));
         calls = vf_calls;
         assert(vaapi_hevc_vulkan_frame_poll(&vf, vf_status) == 0 && vf_fence_calls == 1);
         assert(!vaapi_hevc_vulkan_frame_retire(&vf, &vf_returned) && vf_calls == calls && ex_pool.exports == 1);
         assert(!vaapi_hevc_vulkan_frame_barrier(&vf, (VkCommandBuffer)(uintptr_t)8, vf_barrier, 2, vf_stage, true));
         vf_fence_result = VK_SUCCESS;
         assert(vaapi_hevc_vulkan_frame_poll(&vf, vf_status) == 1 && vf_fence_calls == 2);
         assert(vaapi_hevc_vulkan_frame_poll(&vf, vf_status) == 1 && vf_fence_calls == 2);
         assert(vaapi_hevc_vulkan_frame_track(&vf, (VkFence)(uintptr_t)9));
         assert(vaapi_hevc_vulkan_frame_poll(&vf, vf_status) == 1 && vf_fence_calls == 3);
         vf_returned.active = true;
         assert(!vaapi_hevc_vulkan_frame_retire(&vf, &vf_returned));
         vf_returned.active = false;
         assert(!vaapi_hevc_vulkan_frame_retire(&vf, &vf.source));
         assert(vaapi_hevc_vulkan_frame_retire(&vf, &vf_returned));
         assert(!vaapi_hevc_vulkan_frame_retire(&vf, &ex_frame)); vf_finish();
         vf_reset(); assert(vf_create() && vaapi_hevc_vulkan_frame_track(&vf, (VkFence)(uintptr_t)9));
         vf_fence_result = VK_ERROR_DEVICE_LOST;
         assert(vaapi_hevc_vulkan_frame_poll(&vf, vf_status) == -1 && vf_fence_calls == 1);
         vf_fence_result = VK_SUCCESS;
         assert(vaapi_hevc_vulkan_frame_poll(&vf, vf_status) == -1 && vf_fence_calls == 1);
         assert(!vaapi_hevc_vulkan_frame_retire(&vf, &vf_returned)); calls = vf_calls;
         assert(!vaapi_hevc_vulkan_frame_after_device_destroy(&vf, (VkDevice)(uintptr_t)2, &vf_returned));
         /* Simulate the caller's completed vkDestroyDevice. */
         vf_images = vf_views = vf_allocations = 0;
         assert(vaapi_hevc_vulkan_frame_after_device_destroy(&vf, vf_device, &vf_returned) && vf_calls == calls);
         vf_finish();
      }
   puts("PASS: paired VA imports, foreign barriers, retained tickets through pending/error fences and VA-owner return after GPU/device retirement; no native GPU execution");
   return 0;
}
