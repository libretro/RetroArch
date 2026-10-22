/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#if defined(__linux__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif
#include <assert.h>
#include <stdio.h>
#if defined(__linux__) && !defined(__ANDROID__)
#include <fcntl.h>
#include <unistd.h>
#endif
#include "../../../gfx/common/vulkan_prime_image.h"
static vulkan_prime_format_t plan;
static vulkan_prime_plane_t planes[4];
static VkPhysicalDeviceMemoryProperties memory;
static vulkan_prime_image_t image;
static unsigned fault, fail_slot, current, live_image, live_view, bound;
static unsigned fds[4], memories[4], duplicates, closed, consumed, frees;
static VkDevice device;
static int failed(unsigned kind) { return fault == kind && current == fail_slot; }
static VKAPI_ATTR VkResult VKAPI_CALL create_image(VkDevice d, const VkImageCreateInfo *info,
      const VkAllocationCallbacks *allocator, VkImage *out)
{
   const VkExternalMemoryImageCreateInfo *external = (const VkExternalMemoryImageCreateInfo*)info->pNext;
   const VkImageDrmFormatModifierExplicitCreateInfoEXT *drm =
      (const VkImageDrmFormatModifierExplicitCreateInfoEXT*)external->pNext;
   unsigned i;
   assert(d == device && !allocator && !live_image);
   assert(info->sType == VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO && info->flags == plan.flags
         && info->format == plan.format && info->imageType == VK_IMAGE_TYPE_2D
         && info->extent.width == 64 && info->extent.height == 32 && info->extent.depth == 1
         && info->mipLevels == 1 && info->arrayLayers == 1 && info->samples == VK_SAMPLE_COUNT_1_BIT);
   assert(info->tiling == VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT
         && info->usage == VK_IMAGE_USAGE_SAMPLED_BIT && info->sharingMode == VK_SHARING_MODE_EXCLUSIVE
         && info->initialLayout == VK_IMAGE_LAYOUT_UNDEFINED && !info->queueFamilyIndexCount);
   assert(external->sType == VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO
         && external->handleTypes == VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT);
   assert(drm->sType == VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_EXPLICIT_CREATE_INFO_EXT
         && !drm->pNext && drm->drmFormatModifier == plan.modifier
         && drm->drmFormatModifierPlaneCount == plan.memory_planes);
   for (i = 0; i < plan.memory_planes; i++)
      assert(drm->pPlaneLayouts[i].offset == planes[i].offset
            && drm->pPlaneLayouts[i].rowPitch == planes[i].pitch
            && !drm->pPlaneLayouts[i].size && !drm->pPlaneLayouts[i].arrayPitch
            && !drm->pPlaneLayouts[i].depthPitch);
   if (fault == 1) return VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT;
   live_image = 1; *out = (VkImage)(uintptr_t)7;
   return VK_SUCCESS;
}
static VKAPI_ATTR void VKAPI_CALL destroy_image(VkDevice d, VkImage value, const VkAllocationCallbacks *a)
{
   assert(d == device && value == (VkImage)(uintptr_t)7 && !a && live_image && !live_view);
   live_image = 0;
}
static VKAPI_ATTR void VKAPI_CALL requirements(VkDevice d, const VkImageMemoryRequirementsInfo2 *info,
      VkMemoryRequirements2 *out)
{
   VkMemoryDedicatedRequirements *dedicated = (VkMemoryDedicatedRequirements*)out->pNext;
   assert(d == device && live_image && info->sType == VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2
         && info->image == (VkImage)(uintptr_t)7 && out->sType == VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2
         && dedicated && dedicated->sType == VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS && !dedicated->pNext);
   current = duplicates;
   if (plan.flags)
   {
      const VkImagePlaneMemoryRequirementsInfo *aspect = (const VkImagePlaneMemoryRequirementsInfo*)info->pNext;
      assert(aspect && aspect->sType == VK_STRUCTURE_TYPE_IMAGE_PLANE_MEMORY_REQUIREMENTS_INFO
            && !aspect->pNext && aspect->planeAspect == (VkImageAspectFlagBits)(VK_IMAGE_ASPECT_MEMORY_PLANE_0_BIT_EXT << current));
   }
   else assert(!info->pNext);
   out->memoryRequirements.size = failed(2) ? 0 : failed(3) ? 16384 : 4096;
   out->memoryRequirements.alignment = failed(4) ? 0 : 256;
   out->memoryRequirements.memoryTypeBits = failed(5) ? 0 : 3;
   dedicated->requiresDedicatedAllocation = failed(6) ? VK_TRUE : VK_FALSE;
   dedicated->prefersDedicatedAllocation = failed(7) ? VK_TRUE : VK_FALSE;
}
static int duplicate_fd(int fd)
{
   assert(fd == planes[current].fd && current < 4);
   if (failed(8)) return -1;
   assert(!fds[current]); fds[current] = 1; duplicates++;
   return (int)current + 8;
}
static bool size_fd(int fd, VkDeviceSize *size)
{
   assert(fd == (int)current + 8 && fds[current]);
   if (failed(9)) return false;
   *size = failed(10) ? 2048 : failed(11) ? 16384 : 8192;
   return true;
}
static void close_fd(int fd)
{
   assert(fd >= 8 && fd < 12 && fds[fd-8]);
   fds[fd-8] = 0; closed++;
}
static VKAPI_ATTR VkResult VKAPI_CALL fd_properties(VkDevice d, VkExternalMemoryHandleTypeFlagBits type,
      int fd, VkMemoryFdPropertiesKHR *out)
{
   assert(d == device && type == VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT
         && fd == (int)current + 8 && fds[current]
         && out->sType == VK_STRUCTURE_TYPE_MEMORY_FD_PROPERTIES_KHR && !out->pNext);
   if (failed(12)) return VK_ERROR_INVALID_EXTERNAL_HANDLE;
   out->memoryTypeBits = failed(13) ? 4 : 2;
   return VK_SUCCESS;
}
static VKAPI_ATTR VkResult VKAPI_CALL allocate(VkDevice d, const VkMemoryAllocateInfo *info,
      const VkAllocationCallbacks *a, VkDeviceMemory *out)
{
   const VkImportMemoryFdInfoKHR *import = (const VkImportMemoryFdInfoKHR*)info->pNext;
   const VkMemoryDedicatedAllocateInfo *dedicated = (const VkMemoryDedicatedAllocateInfo*)import->pNext;
   bool wants_dedicated = plan.dedicated || failed(6) || (!plan.flags && failed(7));
   assert(d == device && !a && live_image && info->sType == VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO
         && info->allocationSize == 8192 && info->memoryTypeIndex == 1);
   assert(import && import->sType == VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR
         && import->handleType == VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT
         && import->fd == (int)current + 8 && fds[current]);
   if (wants_dedicated)
      assert(dedicated && dedicated->sType == VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO
            && !dedicated->pNext && !dedicated->buffer && dedicated->image == (VkImage)(uintptr_t)7);
   else assert(!dedicated);
   if (failed(14)) return VK_ERROR_OUT_OF_DEVICE_MEMORY;
   fds[current] = 0; consumed++; memories[current] = 1;
   *out = (VkDeviceMemory)(uintptr_t)(20 + current);
   return VK_SUCCESS;
}
static VKAPI_ATTR void VKAPI_CALL free_memory(VkDevice d, VkDeviceMemory value, const VkAllocationCallbacks *a)
{
   unsigned index = (unsigned)(uintptr_t)value - 20;
   assert(d == device && !a && !live_image && !live_view && index < 4 && memories[index]);
   memories[index] = 0; frees++;
}
static VKAPI_ATTR VkResult VKAPI_CALL bind(VkDevice d, uint32_t count, const VkBindImageMemoryInfo *info)
{
   unsigned i;
   assert(d == device && count == (plan.flags ? plan.memory_planes : 1));
   for (i = 0; i < count; i++)
   {
      assert(info[i].sType == VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO && info[i].image == (VkImage)(uintptr_t)7
            && info[i].memory == (VkDeviceMemory)(uintptr_t)(20+i) && !info[i].memoryOffset && memories[i]);
      if (plan.flags)
      {
         const VkBindImagePlaneMemoryInfo *aspect = (const VkBindImagePlaneMemoryInfo*)info[i].pNext;
         assert(aspect && aspect->sType == VK_STRUCTURE_TYPE_BIND_IMAGE_PLANE_MEMORY_INFO && !aspect->pNext
               && aspect->planeAspect == (VkImageAspectFlagBits)(VK_IMAGE_ASPECT_MEMORY_PLANE_0_BIT_EXT << i));
      }
      else assert(!info[i].pNext);
   }
   if (fault == 15) return VK_ERROR_DEVICE_LOST;
   bound = 1;
   return VK_SUCCESS;
}
static VKAPI_ATTR VkResult VKAPI_CALL create_view(VkDevice d, const VkImageViewCreateInfo *info,
      const VkAllocationCallbacks *a, VkImageView *out)
{
   assert(d == device && !a && bound && !live_view && info->sType == VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO
         && !info->pNext && info->image == (VkImage)(uintptr_t)7 && info->format == plan.format
         && info->viewType == VK_IMAGE_VIEW_TYPE_2D && info->subresourceRange.aspectMask == VK_IMAGE_ASPECT_COLOR_BIT
         && info->subresourceRange.layerCount == 1 && info->subresourceRange.levelCount == 1);
   if (fault == 16) return VK_ERROR_OUT_OF_HOST_MEMORY;
   live_view = 1; *out = (VkImageView)(uintptr_t)30;
   return VK_SUCCESS;
}
static VKAPI_ATTR void VKAPI_CALL destroy_view(VkDevice d, VkImageView value, const VkAllocationCallbacks *a)
{
   assert(d == device && value == (VkImageView)(uintptr_t)30 && !a && live_view && live_image);
   live_view = 0;
}
static const vulkan_prime_image_ops_t ops = { create_image, destroy_image, requirements,
   fd_properties, allocate, free_memory, bind, create_view, destroy_view, duplicate_fd, size_fd, close_fd };
static void reset(unsigned disjoint, unsigned count)
{
   unsigned i;
   assert(!live_image && !live_view);
   for (i = 0; i < 4; i++) assert(!fds[i] && !memories[i]);
   memset(&plan, 0, sizeof(plan)); memset(&memory, 0, sizeof(memory));
   memset(&image, 0, sizeof(image)); memset(planes, 0, sizeof(planes));
   device = (VkDevice)(uintptr_t)1;
   plan.width = 64; plan.height = 32; plan.format = VK_FORMAT_R8_UNORM;
   plan.flags = disjoint ? VK_IMAGE_CREATE_DISJOINT_BIT : 0;
   plan.memory_planes = count; plan.modifier = (uint64_t)1 << 56; plan.max_resource_size = 8192;
   for (i = 0; i < count; i++)
   {
      planes[i].fd = disjoint ? (int)i : 0;
      planes[i].allocation_size = 8192; planes[i].pitch = 64; planes[i].offset = i*1024;
   }
   memory.memoryTypeCount = 2; memory.memoryHeapCount = 1; memory.memoryHeaps[0].size = 65536;
   fault = fail_slot = current = duplicates = closed = consumed = frees = bound = 0;
}
static void empty(void)
{
   vulkan_prime_image_t zero;
   unsigned i;
   memset(&zero, 0, sizeof(zero));
   assert(!memcmp(&zero, &image, sizeof(image)) && !live_image && !live_view
         && duplicates == closed + consumed && frees == consumed);
   for (i = 0; i < 4; i++) assert(!fds[i] && !memories[i]);
}
static bool create(void)
{
   return vulkan_prime_image_create(device, &ops, &memory, &plan, planes, &image);
}
static void native_io(void)
{
   vulkan_prime_image_ops_t io;
   memset(&io, 0, sizeof(io));
#if defined(__linux__) && !defined(__ANDROID__)
   {
      FILE *file = tmpfile();
      VkDeviceSize size = 0;
      int fd, copy;
      assert(file && vulkan_prime_image_bind_io(&io));
      fd = fileno(file);
      assert(ftruncate(fd, 8192) == 0);
      copy = io.duplicate(fd);
      assert(copy >= 0 && copy != fd && (fcntl(copy, F_GETFD) & FD_CLOEXEC));
      assert(io.size(copy, &size) && size == 8192 && lseek(copy, 0, SEEK_CUR) == 0);
      io.close(copy);
      assert(fcntl(fd, F_GETFD) >= 0 && fclose(file) == 0);
      assert(io.duplicate(-1) < 0 && !io.size(-1, &size));
      puts("PASS: native CLOEXEC duplication/size discovery on a temporary file; not a DMA-BUF import");
   }
#else
   assert(!vulkan_prime_image_bind_io(&io) && !io.duplicate && !io.size && !io.close);
#endif
   assert(!vulkan_prime_image_bind_io(NULL));
}
int main(void)
{
   unsigned disjoint, count, kind, slot;
   for (disjoint = 0; disjoint < 2; disjoint++)
      for (count = 1; count <= 4; count++)
      {
         reset(disjoint, count); assert(create());
         assert(image.view && image.image && consumed == (disjoint ? count : 1) && !closed);
         assert(!create());
         vulkan_prime_image_destroy(device, &ops, &image); empty();
         vulkan_prime_image_destroy(device, &ops, &image); empty();
         for (slot = 0; slot < (disjoint ? count : 1); slot++)
            for (kind = 1; kind <= 16; kind++)
            {
               bool expected;
               reset(disjoint, count); fault = kind; fail_slot = slot;
               expected = kind == 7 || (kind == 6 && !disjoint);
               assert(create() == expected);
               if (expected) vulkan_prime_image_destroy(device, &ops, &image);
               empty();
            }
      }
   reset(0, 1); plan.dedicated = true; assert(create());
   vulkan_prime_image_destroy(device, &ops, &image); empty();
   for (kind = 0; kind < 12; kind++)
   {
      reset(0, 2);
      switch (kind)
      {
         case 0: memory.memoryTypes[1].propertyFlags = VK_MEMORY_PROPERTY_PROTECTED_BIT; break;
         case 1: memory.memoryTypes[1].propertyFlags = VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT; break;
         case 2: memory.memoryTypes[1].heapIndex = 1; break;
         case 3: memory.memoryHeaps[0].size = 1; break;
         case 4: memory.memoryTypeCount = 33; break;
         case 5: memory.memoryHeapCount = 0; break;
         case 6: planes[1].fd = 1; break;
         case 7: planes[1].allocation_size = 1; break;
         case 8: planes[1].pitch = 0; break;
         case 9: planes[1].offset = 8192; break;
         case 10: planes[0].fd = -1; break;
         case 11: plan.flags = VK_IMAGE_CREATE_DISJOINT_BIT; plan.dedicated = true; break;
      }
      assert(!create()); empty();
   }
   puts("PASS: PRIME explicit layouts, memory-plane requirements/types/sizes, duplicate FD ownership, binding/view and partial cleanup; no native import");
   native_io();
   return 0;
}
