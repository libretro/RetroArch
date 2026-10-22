/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include "vulkan_android_device.h"
#include <string.h>
#include <stddef.h>
bool vulkan_android_device_plan_query(VkPhysicalDevice gpu,
      unsigned api_version, const char *const *extensions, unsigned count,
      PFN_vkGetPhysicalDeviceFeatures2 features,
      PFN_vkGetPhysicalDeviceExternalSemaphoreProperties semaphores,
      PFN_vkGetPhysicalDeviceProperties2 properties,
      vulkan_android_device_plan_t *plan)
{
   VkPhysicalDeviceFeatures2 f;
   VkPhysicalDeviceExternalSemaphoreInfo info;
   VkExternalSemaphoreProperties semaphore;
   VkPhysicalDeviceMaintenance6PropertiesKHR limits;
   VkPhysicalDeviceProperties2 props;
   bool ahb = false, foreign = false, sync_fd = false, maintenance6 = false;
   unsigned i;
   if (!plan)
      return false;
   memset(plan, 0, sizeof(*plan));
   if (!gpu || api_version < VK_API_VERSION_1_1 || !features || !semaphores
         || !extensions || !count || count > 256)
      return false;
   for (i = 0; i < count; i++)
   {
      if (!extensions[i])
         return false;
      if (!strcmp(extensions[i],
                "VK_ANDROID_external_memory_android_hardware_buffer"))
         ahb = true;
      if (!strcmp(extensions[i], "VK_EXT_queue_family_foreign"))
         foreign = true;
      if (!strcmp(extensions[i], "VK_KHR_external_semaphore_fd"))
         sync_fd = true;
      if (!strcmp(extensions[i], "VK_KHR_maintenance6"))
         maintenance6 = true;
   }
   if (!ahb || !foreign || !sync_fd)
      return false;
   memset(&f, 0, sizeof(f));
   f.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
   plan->conversion.sType
         = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_YCBCR_CONVERSION_FEATURES;
   f.pNext = &plan->conversion;
   features(gpu, &f);
   if (!plan->conversion.samplerYcbcrConversion)
      goto fail;
   memset(&info, 0, sizeof(info));
   info.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_SEMAPHORE_INFO;
   info.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT;
   memset(&semaphore, 0, sizeof(semaphore));
   semaphore.sType = VK_STRUCTURE_TYPE_EXTERNAL_SEMAPHORE_PROPERTIES;
   semaphores(gpu, &info, &semaphore);
   if ((semaphore.externalSemaphoreFeatures
             & (VK_EXTERNAL_SEMAPHORE_FEATURE_IMPORTABLE_BIT
                   | VK_EXTERNAL_SEMAPHORE_FEATURE_EXPORTABLE_BIT))
               != (VK_EXTERNAL_SEMAPHORE_FEATURE_IMPORTABLE_BIT
                     | VK_EXTERNAL_SEMAPHORE_FEATURE_EXPORTABLE_BIT)
         || !(semaphore.compatibleHandleTypes
               & VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT))
      goto fail;
   if (maintenance6 || api_version >= VK_MAKE_VERSION(1, 4, 0))
   {
      if (!properties)
         goto fail;
      memset(&limits, 0, sizeof(limits));
      limits.sType
            = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_6_PROPERTIES_KHR;
      memset(&props, 0, sizeof(props));
      props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
      props.pNext = &limits;
      properties(gpu, &props);
      if (!limits.maxCombinedImageSamplerDescriptorCount)
         goto fail;
      plan->sampler_descriptors = limits.maxCombinedImageSamplerDescriptorCount;
   }
   else
   {
      /* Legacy three-plane reservation; allocation remains cold admission. */
      plan->sampler_descriptors = 3;
      plan->legacy_descriptor_budget = true;
   }
   plan->conversion.pNext = NULL;
   plan->supported = true;
   return true;
fail:
   memset(plan, 0, sizeof(*plan));
   return false;
}
const void *vulkan_android_device_plan_chain(
      vulkan_android_device_plan_t *plan, const void *previous)
{
   if (!plan || !plan->supported)
      return previous;
   plan->conversion.pNext = (void *)previous;
   return &plan->conversion;
}

bool vulkan_android_device_extension(const char *name)
{
   static const char *const names[]
         = {"VK_ANDROID_external_memory_android_hardware_buffer",
               "VK_EXT_queue_family_foreign", "VK_KHR_external_semaphore_fd",
               "VK_KHR_external_memory", "VK_KHR_external_semaphore",
               "VK_KHR_sampler_ycbcr_conversion", "VK_KHR_bind_memory2",
               "VK_KHR_maintenance1", "VK_KHR_get_memory_requirements2",
               "VK_KHR_dedicated_allocation", "VK_KHR_maintenance6"};
   unsigned i;
   if (!name)
      return false;
   for (i = 0; i < sizeof(names) / sizeof(names[0]); i++)
      if (!strcmp(name, names[i]))
         return true;
   return false;
}
bool vulkan_android_device_plan_disable(vulkan_android_device_plan_t *plan,
      const void **chain, const char **extensions, unsigned *count)
{
   const void *node, *next;
   unsigned i, kept = 0;
   bool removed = false;
   if (!plan || !chain || !extensions || !count || *count > 256)
      return false;
   for (i = 0; i < *count; i++)
      if (!extensions[i])
         return false;
   if (plan->supported)
   {
      if (*chain == &plan->conversion)
      {
         *chain = plan->conversion.pNext;
         removed = true;
      }
      else
         for (node = *chain, i = 0; node && i < 32; i++, node = next)
         {
            /* Vulkan chain prefixes have common offsets. memcpy avoids aliasing
             * a caller's typed feature structure through VkBaseOutStructure. */
            memcpy(&next,
                  (const char *)node + offsetof(VkBaseOutStructure, pNext),
                  sizeof(next));
            if (next == &plan->conversion)
            {
               next = plan->conversion.pNext;
               memcpy((char *)node + offsetof(VkBaseOutStructure, pNext), &next,
                     sizeof(next));
               removed = true;
               break;
            }
         }
      if (!removed)
         return false;
   }
   for (i = 0; i < *count; i++)
   {
      if (vulkan_android_device_extension(extensions[i]))
         removed = true;
      else
         extensions[kept++] = extensions[i];
   }
   *count = kept;
   memset(plan, 0, sizeof(*plan));
   return removed;
}
