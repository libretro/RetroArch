/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#if defined(__linux__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif
#include "vulkan_prime_image.h"
#if defined(__linux__) && !defined(__ANDROID__)
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
static int vulkan_prime_fd_duplicate(int fd)
{
#ifdef F_DUPFD_CLOEXEC
   return fcntl(fd, F_DUPFD_CLOEXEC, 0);
#else
   (void)fd;
   return -1;
#endif
}
static bool vulkan_prime_fd_size(int fd, VkDeviceSize *out)
{
   off_t size = lseek(fd, 0, SEEK_END);
   if (size <= 0 || lseek(fd, 0, SEEK_SET) != 0)
      return false;
   *out = (VkDeviceSize)size;
   return true;
}
static void vulkan_prime_fd_close(int fd) { close(fd); }
#endif
bool vulkan_prime_image_bind_io(vulkan_prime_image_ops_t *ops)
{
#if defined(__linux__) && !defined(__ANDROID__) && defined(F_DUPFD_CLOEXEC)
   if (!ops)
      return false;
   ops->duplicate = vulkan_prime_fd_duplicate;
   ops->size = vulkan_prime_fd_size;
   ops->close = vulkan_prime_fd_close;
   return true;
#else
   (void)ops;
   return false;
#endif
}
void vulkan_prime_image_destroy(VkDevice device, const vulkan_prime_image_ops_t *ops,
      vulkan_prime_image_t *image)
{
   unsigned i;
   if (!image || !ops)
      return;
   if (image->view) ops->destroy_view(device, image->view, NULL);
   if (image->image) ops->destroy_image(device, image->image, NULL);
   for (i = 0; i < 4; i++)
      if (image->memory[i]) ops->free_memory(device, image->memory[i], NULL);
   memset(image, 0, sizeof(*image));
}
static bool vulkan_prime_image_memory(VkDevice device, const vulkan_prime_image_ops_t *ops,
      const VkPhysicalDeviceMemoryProperties *memory, const vulkan_prime_format_t *plan,
      const vulkan_prime_plane_t *plane, unsigned index, vulkan_prime_image_t *out)
{
   VkImageMemoryRequirementsInfo2 info;
   VkImagePlaneMemoryRequirementsInfo aspect;
   VkMemoryRequirements2 requirements;
   VkMemoryDedicatedRequirements required;
   VkMemoryFdPropertiesKHR properties;
   VkImportMemoryFdInfoKHR import;
   VkMemoryDedicatedAllocateInfo dedicated;
   VkMemoryAllocateInfo allocation;
   VkDeviceSize size = 0;
   VkResult status;
   uint32_t i, bits;
   int fd;
   bool disjoint = !!(plan->flags & VK_IMAGE_CREATE_DISJOINT_BIT);
   memset(&info, 0, sizeof(info));
   memset(&aspect, 0, sizeof(aspect));
   memset(&requirements, 0, sizeof(requirements));
   memset(&required, 0, sizeof(required));
   info.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2;
   info.image = out->image;
   aspect.sType = VK_STRUCTURE_TYPE_IMAGE_PLANE_MEMORY_REQUIREMENTS_INFO;
   aspect.planeAspect = (VkImageAspectFlagBits)(VK_IMAGE_ASPECT_MEMORY_PLANE_0_BIT_EXT << index);
   if (disjoint) info.pNext = &aspect;
   requirements.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2;
   requirements.pNext = &required;
   required.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS;
   ops->requirements(device, &info, &requirements);
   if (!requirements.memoryRequirements.size || !requirements.memoryRequirements.alignment
         || requirements.memoryRequirements.size > plan->max_resource_size
         || (disjoint && (plan->dedicated || required.requiresDedicatedAllocation)))
      return false;
   fd = ops->duplicate(plane->fd);
   if (fd < 0)
      return false;
   memset(&properties, 0, sizeof(properties));
   properties.sType = VK_STRUCTURE_TYPE_MEMORY_FD_PROPERTIES_KHR;
   if (!ops->size(fd, &size) || size != plane->allocation_size
         || size < requirements.memoryRequirements.size
         || ops->fd_properties(device, VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
            fd, &properties) != VK_SUCCESS)
      goto fail;
   bits = properties.memoryTypeBits & requirements.memoryRequirements.memoryTypeBits;
   for (i = 0; i < memory->memoryTypeCount; i++)
      if ((bits & ((uint32_t)1 << i))
            && !(memory->memoryTypes[i].propertyFlags
               & (VK_MEMORY_PROPERTY_PROTECTED_BIT | VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT))
            && memory->memoryTypes[i].heapIndex < memory->memoryHeapCount
            && memory->memoryHeaps[memory->memoryTypes[i].heapIndex].size >= size)
         break;
   if (i == memory->memoryTypeCount)
      goto fail;
   memset(&import, 0, sizeof(import));
   memset(&dedicated, 0, sizeof(dedicated));
   memset(&allocation, 0, sizeof(allocation));
   import.sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR;
   import.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;
   import.fd = fd;
   dedicated.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO;
   dedicated.image = out->image;
   if (plan->dedicated || required.requiresDedicatedAllocation
         || (!disjoint && required.prefersDedicatedAllocation))
      import.pNext = &dedicated;
   allocation.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
   allocation.pNext = &import;
   allocation.allocationSize = size;
   allocation.memoryTypeIndex = i;
   status = ops->allocate(device, &allocation, NULL, &out->memory[index]);
   if (status == VK_SUCCESS)
      return true;
   out->memory[index] = VK_NULL_HANDLE;
fail:
   ops->close(fd);
   return false;
}
bool vulkan_prime_image_create(VkDevice device, const vulkan_prime_image_ops_t *ops,
      const VkPhysicalDeviceMemoryProperties *memory, const vulkan_prime_format_t *plan,
      const vulkan_prime_plane_t *planes, vulkan_prime_image_t *out)
{
   VkImageCreateInfo image;
   VkExternalMemoryImageCreateInfo external;
   VkImageDrmFormatModifierExplicitCreateInfoEXT drm;
   VkSubresourceLayout layouts[4];
   VkBindImageMemoryInfo bindings[4];
   VkBindImagePlaneMemoryInfo aspects[4];
   VkImageViewCreateInfo view;
   unsigned i, count;
   bool disjoint;
   if (!out || out->image || out->view || out->memory[0] || out->memory[1]
         || out->memory[2] || out->memory[3] || !device || !memory || !plan || !planes
         || !plan->width || !plan->height || !plan->max_resource_size
         || !plan->memory_planes || plan->memory_planes > 4
         || (plan->flags & ~VK_IMAGE_CREATE_DISJOINT_BIT)
         || (plan->format != VK_FORMAT_R8_UNORM && plan->format != VK_FORMAT_R8G8_UNORM
            && plan->format != VK_FORMAT_R16_UNORM && plan->format != VK_FORMAT_R16G16_UNORM)
         || !memory->memoryTypeCount || memory->memoryTypeCount > 32
         || !memory->memoryHeapCount || memory->memoryHeapCount > VK_MAX_MEMORY_HEAPS
         || !ops || !ops->create_image || !ops->destroy_image || !ops->requirements
         || !ops->fd_properties || !ops->allocate || !ops->free_memory || !ops->bind
         || !ops->create_view || !ops->destroy_view || !ops->duplicate || !ops->size || !ops->close)
      return false;
   disjoint = !!(plan->flags & VK_IMAGE_CREATE_DISJOINT_BIT);
   if (disjoint && plan->dedicated)
      return false;
   memset(layouts, 0, sizeof(layouts));
   for (i = 0; i < plan->memory_planes; i++)
   {
      if (planes[i].fd < 0 || !planes[i].pitch || !planes[i].allocation_size
            || planes[i].offset >= planes[i].allocation_size
            || (!disjoint && (planes[i].fd != planes[0].fd
               || planes[i].allocation_size != planes[0].allocation_size)))
         return false;
      layouts[i].offset = planes[i].offset;
      layouts[i].rowPitch = planes[i].pitch;
   }
   memset(&image, 0, sizeof(image));
   memset(&external, 0, sizeof(external));
   memset(&drm, 0, sizeof(drm));
   image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
   image.pNext = &external;
   image.flags = plan->flags;
   image.imageType = VK_IMAGE_TYPE_2D;
   image.format = plan->format;
   image.extent.width = plan->width;
   image.extent.height = plan->height;
   image.extent.depth = image.mipLevels = image.arrayLayers = 1;
   image.samples = VK_SAMPLE_COUNT_1_BIT;
   image.tiling = VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT;
   image.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
   image.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
   image.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
   external.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
   external.pNext = &drm;
   external.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;
   drm.sType = VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_EXPLICIT_CREATE_INFO_EXT;
   drm.drmFormatModifier = plan->modifier;
   drm.drmFormatModifierPlaneCount = plan->memory_planes;
   drm.pPlaneLayouts = layouts;
   if (ops->create_image(device, &image, NULL, &out->image) != VK_SUCCESS)
   {
      out->image = VK_NULL_HANDLE;
      return false;
   }
   count = disjoint ? plan->memory_planes : 1;
   memset(bindings, 0, sizeof(bindings));
   memset(aspects, 0, sizeof(aspects));
   for (i = 0; i < count; i++)
   {
      if (!vulkan_prime_image_memory(device, ops, memory, plan, &planes[i], i, out))
         goto fail;
      bindings[i].sType = VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO;
      bindings[i].image = out->image;
      bindings[i].memory = out->memory[i];
      aspects[i].sType = VK_STRUCTURE_TYPE_BIND_IMAGE_PLANE_MEMORY_INFO;
      aspects[i].planeAspect = (VkImageAspectFlagBits)(VK_IMAGE_ASPECT_MEMORY_PLANE_0_BIT_EXT << i);
      if (disjoint) bindings[i].pNext = &aspects[i];
   }
   if (ops->bind(device, count, bindings) != VK_SUCCESS)
      goto fail;
   memset(&view, 0, sizeof(view));
   view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
   view.image = out->image;
   view.viewType = VK_IMAGE_VIEW_TYPE_2D;
   view.format = plan->format;
   view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
   view.subresourceRange.levelCount = view.subresourceRange.layerCount = 1;
   if (ops->create_view(device, &view, NULL, &out->view) != VK_SUCCESS)
   {
      out->view = VK_NULL_HANDLE;
      goto fail;
   }
   return true;
fail:
   vulkan_prime_image_destroy(device, ops, out);
   return false;
}
