/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RARCH_VULKAN_PRIME_FORMAT_H
#define RARCH_VULKAN_PRIME_FORMAT_H
#include "vulkan_drm_identity.h"
#ifndef VULKAN_PRIME_FORMAT_ALLOC
#define VULKAN_PRIME_FORMAT_ALLOC malloc
#endif
typedef struct vulkan_prime_format
{
   VkDeviceSize max_resource_size;
   uint64_t modifier;
   VkFormat format;
   VkImageCreateFlags flags;
   uint32_t width, height, memory_planes;
   bool dedicated;
} vulkan_prime_format_t;

/* Cold preflight, before taking FDs. Commands must be legal for the actual
 * instance, and the device must already have enabled the PRIME prerequisites.
 * FD memory types, allocation sizes and binding remain importer checks. */
static INLINE bool vulkan_prime_format_query(vulkan_prime_format_t *out,
      VkPhysicalDevice gpu, VkFormat format, uint32_t width, uint32_t height,
      uint64_t modifier, uint32_t planes, bool disjoint,
      PFN_vkGetPhysicalDeviceFormatProperties2 get_format,
      PFN_vkGetPhysicalDeviceImageFormatProperties2 get_image)
{
   VkFormatProperties2 properties;
   VkDrmFormatModifierPropertiesListEXT list;
   VkDrmFormatModifierPropertiesEXT *entries;
   VkPhysicalDeviceImageFormatInfo2 info;
   VkPhysicalDeviceExternalImageFormatInfo external;
   VkPhysicalDeviceImageDrmFormatModifierInfoEXT drm;
   VkImageFormatProperties2 image;
   VkExternalImageFormatProperties memory;
   vulkan_prime_format_t result;
   VkFormatFeatureFlags required = VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
   VkExternalMemoryFeatureFlags features;
   uint32_t capacity, i, matches = 0;
   bool supported = false;
   if (!out || !gpu || !get_format || !get_image || !width || !height
         || !planes || planes > 4
         || (modifier & (((uint64_t)1 << 56) - 1)) == (((uint64_t)1 << 56) - 1)
         || (format != VK_FORMAT_R8_UNORM && format != VK_FORMAT_R8G8_UNORM
            && format != VK_FORMAT_R16_UNORM && format != VK_FORMAT_R16G16_UNORM))
      return false;
   if (disjoint)
      required |= VK_FORMAT_FEATURE_DISJOINT_BIT;
   memset(&properties, 0, sizeof(properties));
   memset(&list, 0, sizeof(list));
   properties.sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2;
   properties.pNext = &list;
   list.sType = VK_STRUCTURE_TYPE_DRM_FORMAT_MODIFIER_PROPERTIES_LIST_EXT;
   get_format(gpu, format, &properties);
   capacity = list.drmFormatModifierCount;
   if (!capacity || capacity > 4096)
      return false;
   entries = (VkDrmFormatModifierPropertiesEXT*)VULKAN_PRIME_FORMAT_ALLOC(
         capacity * sizeof(*entries));
   if (!entries)
      return false;
   list.pDrmFormatModifierProperties = entries;
   memset(entries, 0, capacity * sizeof(*entries));
   get_format(gpu, format, &properties);
   if (list.drmFormatModifierCount <= capacity)
      for (i = 0; i < list.drmFormatModifierCount; i++)
         if (entries[i].drmFormatModifier == modifier)
         {
            matches++;
            supported = entries[i].drmFormatModifierPlaneCount == planes
               && (entries[i].drmFormatModifierTilingFeatures & required) == required;
         }
   free(entries);
   if (!supported || matches != 1)
      return false;
   memset(&info, 0, sizeof(info));
   memset(&external, 0, sizeof(external));
   memset(&drm, 0, sizeof(drm));
   memset(&image, 0, sizeof(image));
   memset(&memory, 0, sizeof(memory));
   info.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2;
   info.pNext = &external;
   info.format = format;
   info.type = VK_IMAGE_TYPE_2D;
   info.tiling = VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT;
   info.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
   info.flags = disjoint ? VK_IMAGE_CREATE_DISJOINT_BIT : 0;
   external.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO;
   external.pNext = &drm;
   external.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;
   drm.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_DRM_FORMAT_MODIFIER_INFO_EXT;
   drm.drmFormatModifier = modifier;
   drm.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
   image.sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2;
   image.pNext = &memory;
   memory.sType = VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES;
   if (get_image(gpu, &info, &image) != VK_SUCCESS)
      return false;
   features = memory.externalMemoryProperties.externalMemoryFeatures;
   if (!(features & VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT)
         || !(memory.externalMemoryProperties.compatibleHandleTypes
            & VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT)
         || (disjoint && (features & VK_EXTERNAL_MEMORY_FEATURE_DEDICATED_ONLY_BIT))
         || image.imageFormatProperties.maxExtent.width < width
         || image.imageFormatProperties.maxExtent.height < height
         || !image.imageFormatProperties.maxExtent.depth
         || !image.imageFormatProperties.maxMipLevels
         || !image.imageFormatProperties.maxArrayLayers
         || !(image.imageFormatProperties.sampleCounts & VK_SAMPLE_COUNT_1_BIT)
         || !image.imageFormatProperties.maxResourceSize)
      return false;
   memset(&result, 0, sizeof(result));
   result.max_resource_size = image.imageFormatProperties.maxResourceSize;
   result.modifier = modifier;
   result.format = format;
   result.flags = info.flags;
   result.width = width;
   result.height = height;
   result.memory_planes = planes;
   result.dedicated = !!(features & VK_EXTERNAL_MEMORY_FEATURE_DEDICATED_ONLY_BIT);
   *out = result;
   return true;
}
#endif
