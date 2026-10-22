/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
static unsigned allocation_failure;
static void *format_alloc(size_t size)
{
   return allocation_failure ? NULL : malloc(size);
}
#define VULKAN_PRIME_FORMAT_ALLOC format_alloc
#include "../../../gfx/common/vulkan_prime_format.h"
static unsigned fault, format_calls, image_calls;
static uint64_t expected_modifier;
static uint32_t expected_planes;
static bool expected_disjoint;
static VkFormat expected_format;
static VKAPI_ATTR void VKAPI_CALL format_properties(VkPhysicalDevice gpu,
      VkFormat format, VkFormatProperties2 *properties)
{
   VkDrmFormatModifierPropertiesListEXT *list =
      (VkDrmFormatModifierPropertiesListEXT*)properties->pNext;
   VkDrmFormatModifierPropertiesEXT *entry;
   assert(gpu == (VkPhysicalDevice)(uintptr_t)1 && format == expected_format);
   assert(properties->sType == VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2);
   assert(list && list->sType == VK_STRUCTURE_TYPE_DRM_FORMAT_MODIFIER_PROPERTIES_LIST_EXT
         && !list->pNext);
   format_calls++;
   if (!list->pDrmFormatModifierProperties)
   {
      list->drmFormatModifierCount = fault == 1 ? 0 : fault == 2 ? 4097 : 2;
      return;
   }
   assert(list->drmFormatModifierCount == 2);
   entry = list->pDrmFormatModifierProperties;
   entry[0].drmFormatModifier = expected_modifier;
   entry[0].drmFormatModifierPlaneCount = fault == 4 ? 0 : expected_planes;
   entry[0].drmFormatModifierTilingFeatures = VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT
      | VK_FORMAT_FEATURE_DISJOINT_BIT;
   if (fault == 5) entry[0].drmFormatModifierTilingFeatures &= ~VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
   if (fault == 6) entry[0].drmFormatModifierTilingFeatures &= ~VK_FORMAT_FEATURE_DISJOINT_BIT;
   entry[1] = entry[0];
   entry[1].drmFormatModifier = fault == 7 ? expected_modifier : expected_modifier + 1;
   if (fault == 8) entry[0].drmFormatModifier = expected_modifier + 2;
   if (fault == 3) list->drmFormatModifierCount = 3;
   if (fault == 9) list->drmFormatModifierCount = 0;
}
static VKAPI_ATTR VkResult VKAPI_CALL image_properties(VkPhysicalDevice gpu,
      const VkPhysicalDeviceImageFormatInfo2 *info, VkImageFormatProperties2 *out)
{
   const VkPhysicalDeviceExternalImageFormatInfo *external =
      (const VkPhysicalDeviceExternalImageFormatInfo*)info->pNext;
   const VkPhysicalDeviceImageDrmFormatModifierInfoEXT *drm =
      (const VkPhysicalDeviceImageDrmFormatModifierInfoEXT*)external->pNext;
   VkExternalImageFormatProperties *memory = (VkExternalImageFormatProperties*)out->pNext;
   VkImageFormatProperties *image = &out->imageFormatProperties;
   assert(gpu == (VkPhysicalDevice)(uintptr_t)1);
   assert(info->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2
         && info->format == expected_format && info->type == VK_IMAGE_TYPE_2D
         && info->tiling == VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT
         && info->usage == VK_IMAGE_USAGE_SAMPLED_BIT
         && info->flags == (expected_disjoint ? VK_IMAGE_CREATE_DISJOINT_BIT : 0));
   assert(external->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO
         && external->handleType == VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT);
   assert(drm && drm->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_DRM_FORMAT_MODIFIER_INFO_EXT
         && drm->drmFormatModifier == expected_modifier && drm->sharingMode == VK_SHARING_MODE_EXCLUSIVE
         && !drm->queueFamilyIndexCount && !drm->pQueueFamilyIndices && !drm->pNext);
   assert(out->sType == VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2 && memory
         && memory->sType == VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES && !memory->pNext);
   image_calls++;
   if (fault == 10) return VK_ERROR_FORMAT_NOT_SUPPORTED;
   memory->externalMemoryProperties.externalMemoryFeatures = fault == 11 ? 0
      : VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT;
   if (fault == 13 || fault == 21)
      memory->externalMemoryProperties.externalMemoryFeatures |= VK_EXTERNAL_MEMORY_FEATURE_DEDICATED_ONLY_BIT;
   memory->externalMemoryProperties.compatibleHandleTypes = fault == 12 ? 0
      : VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;
   image->maxExtent.width = fault == 14 ? 63 : 64;
   image->maxExtent.height = fault == 15 ? 31 : 32;
   image->maxExtent.depth = fault == 16 ? 0 : 1;
   image->maxMipLevels = fault == 17 ? 0 : 1;
   image->maxArrayLayers = fault == 18 ? 0 : 1;
   image->sampleCounts = fault == 19 ? VK_SAMPLE_COUNT_2_BIT : VK_SAMPLE_COUNT_1_BIT;
   image->maxResourceSize = fault == 20 ? 0 : 65536;
   return VK_SUCCESS;
}
static bool query(vulkan_prime_format_t *out)
{
   return vulkan_prime_format_query(out, (VkPhysicalDevice)(uintptr_t)1,
         expected_format, 64, 32, expected_modifier, expected_planes,
         expected_disjoint, format_properties, image_properties);
}
int main(void)
{
   static const VkFormat formats[] = { VK_FORMAT_R8_UNORM, VK_FORMAT_R8G8_UNORM,
      VK_FORMAT_R16_UNORM, VK_FORMAT_R16G16_UNORM };
   vulkan_prime_format_t out, before;
   unsigned i, j;
   memset(&before, 0x5a, sizeof(before));
   expected_planes = 2;
   expected_disjoint = true;
   expected_modifier = (uint64_t)1 << 56;
   expected_format = formats[0];
   for (fault = 1; fault <= 20; fault++)
   {
      out = before;
      assert(!query(&out) && !memcmp(&out, &before, sizeof(out)));
   }
   fault = 0;
   allocation_failure = 1;
   out = before;
   assert(!query(&out) && !memcmp(&out, &before, sizeof(out)));
   allocation_failure = 0;
   for (i = 0; i < 4; i++)
      for (j = 1; j <= 4; j++)
      {
         expected_format = formats[i]; expected_planes = j;
         assert(query(&out));
         assert(out.format == formats[i] && out.width == 64 && out.height == 32
               && out.modifier == expected_modifier && out.memory_planes == j
               && out.flags == VK_IMAGE_CREATE_DISJOINT_BIT && !out.dedicated
               && out.max_resource_size == 65536);
      }
   expected_disjoint = false; expected_planes = 1; expected_modifier = 0;
   fault = 21;
   assert(query(&out) && out.dedicated && !out.flags && !out.modifier);
   fault = 0;
   format_calls = image_calls = 0;
   expected_format = VK_FORMAT_R8G8B8A8_UNORM;
   assert(!query(&out));
   expected_format = formats[0]; expected_planes = 0;
   assert(!query(&out));
   expected_planes = 5;
   assert(!query(&out));
   expected_planes = 1; expected_modifier = ~(uint64_t)0;
   assert(!query(&out));
   expected_modifier = ((uint64_t)1 << 56) - 1;
   assert(!query(&out));
   expected_modifier = 0;
   assert(!vulkan_prime_format_query(&out, (VkPhysicalDevice)(uintptr_t)1,
         expected_format, 0, 32, 0, 1, false, format_properties, image_properties));
   assert(!vulkan_prime_format_query(&out, (VkPhysicalDevice)(uintptr_t)1,
         expected_format, 64, 0, 0, 1, false, format_properties, image_properties));
   assert(!vulkan_prime_format_query(NULL, (VkPhysicalDevice)(uintptr_t)1,
         expected_format, 64, 32, 0, 1, false, format_properties, image_properties));
   assert(!vulkan_prime_format_query(&out, VK_NULL_HANDLE,
         expected_format, 64, 32, 0, 1, false, format_properties, image_properties));
   assert(!vulkan_prime_format_query(&out, (VkPhysicalDevice)(uintptr_t)1,
         expected_format, 64, 32, 0, 1, false, NULL, image_properties));
   assert(!vulkan_prime_format_query(&out, (VkPhysicalDevice)(uintptr_t)1,
         expected_format, 64, 32, 0, 1, false, format_properties, NULL));
   assert(!format_calls && !image_calls);
   puts("PASS: PRIME modifier/plane/features, DMA-BUF import limits and dedicated/disjoint admission");
   return 0;
}
