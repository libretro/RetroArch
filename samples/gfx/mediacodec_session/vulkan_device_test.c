/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../../gfx/common/vulkan_android_device.h"
static int no_conversion, sem_flags = 3, compatible = 1, descriptor_count = 6,
                          queried;
static VKAPI_ATTR void VKAPI_CALL features(
      VkPhysicalDevice gpu, VkPhysicalDeviceFeatures2 *p)
{
   VkPhysicalDeviceSamplerYcbcrConversionFeatures *f
         = (VkPhysicalDeviceSamplerYcbcrConversionFeatures *)p->pNext;
   assert(gpu
         && f->sType
               == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_YCBCR_CONVERSION_FEATURES);
   f->samplerYcbcrConversion = no_conversion ? VK_FALSE : VK_TRUE;
}
static VKAPI_ATTR void VKAPI_CALL semaphores(VkPhysicalDevice gpu,
      const VkPhysicalDeviceExternalSemaphoreInfo *p,
      VkExternalSemaphoreProperties *out)
{
   assert(
         gpu && p->handleType == VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT);
   out->externalSemaphoreFeatures = (VkExternalSemaphoreFeatureFlags)sem_flags;
   out->compatibleHandleTypes
         = compatible ? VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT : 0;
}
static VKAPI_ATTR void VKAPI_CALL properties(
      VkPhysicalDevice gpu, VkPhysicalDeviceProperties2 *p)
{
   VkPhysicalDeviceMaintenance6PropertiesKHR *m
         = (VkPhysicalDeviceMaintenance6PropertiesKHR *)p->pNext;
   assert(gpu
         && m->sType
               == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_6_PROPERTIES_KHR);
   m->maxCombinedImageSamplerDescriptorCount = (unsigned)descriptor_count;
   queried++;
}
int main(void)
{
   const char *names[] = {"VK_ANDROID_external_memory_android_hardware_buffer",
         "VK_EXT_queue_family_foreign", "VK_KHR_external_semaphore_fd",
         "VK_KHR_maintenance6"};
   const char *head_names[]
         = {"VK_ANDROID_external_memory_android_hardware_buffer",
               "VK_EXT_queue_family_foreign", "VK_KHR_external_semaphore_fd",
               "VK_KHR_swapchain"};
   static vulkan_android_device_plan_t p, empty;
   VkPhysicalDevice gpu = (VkPhysicalDevice)(uintptr_t)1;
   unsigned i, count;
   const void *chain;
   VkPhysicalDeviceTimelineSemaphoreFeatures outer;
   assert(vulkan_android_device_plan_query(gpu, VK_API_VERSION_1_1, names, 3,
         features, semaphores, properties, &p));
   assert(p.supported && p.legacy_descriptor_budget
         && p.sampler_descriptors == 3 && !queried);
   assert(vulkan_android_device_plan_chain(&p, &empty) == &p.conversion
         && p.conversion.pNext == &empty);
   assert(vulkan_android_device_plan_query(gpu, VK_API_VERSION_1_1, names, 4,
         features, semaphores, properties, &p));
   assert(!p.legacy_descriptor_budget && p.sampler_descriptors == 6
         && queried == 1);
   for (i = 0; i < 3; i++)
   {
      const char *saved = names[i];
      names[i] = "VK_FAKE_missing_required";
      assert(!vulkan_android_device_plan_query(gpu, VK_API_VERSION_1_1, names,
            4, features, semaphores, properties, &p));
      assert(!memcmp(&p, &empty, sizeof(p)));
      names[i] = saved;
   }
   no_conversion = 1;
   assert(!vulkan_android_device_plan_query(gpu, VK_API_VERSION_1_1, names, 4,
         features, semaphores, properties, &p));
   no_conversion = 0;
   sem_flags = 1;
   assert(!vulkan_android_device_plan_query(gpu, VK_API_VERSION_1_1, names, 4,
         features, semaphores, properties, &p));
   sem_flags = 3;
   compatible = 0;
   assert(!vulkan_android_device_plan_query(gpu, VK_API_VERSION_1_1, names, 4,
         features, semaphores, properties, &p));
   compatible = 1;
   descriptor_count = 0;
   assert(!vulkan_android_device_plan_query(gpu, VK_API_VERSION_1_1, names, 4,
         features, semaphores, properties, &p));
   assert(!vulkan_android_device_plan_query(gpu, VK_API_VERSION_1_0, names, 4,
         features, semaphores, properties, &p));
   assert(!vulkan_android_device_plan_query(
         gpu, VK_API_VERSION_1_1, names, 4, NULL, semaphores, properties, &p));
   assert(!memcmp(&p, &empty, sizeof(p)));
   assert(vulkan_android_device_plan_chain(&p, &empty) == &empty);
   for (i = 0; i < 4; i++)
      assert(vulkan_android_device_extension(names[i]));
   assert(vulkan_android_device_extension("VK_KHR_bind_memory2"));
   assert(!vulkan_android_device_extension("VK_KHR_swapchain")
         && !vulkan_android_device_extension(NULL));
   descriptor_count = 6;
   assert(vulkan_android_device_plan_query(gpu, VK_API_VERSION_1_1, names, 4,
         features, semaphores, properties, &p));
   memset(&outer, 0, sizeof(outer));
   outer.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES;
   outer.timelineSemaphore = VK_TRUE;
   outer.pNext = (void *)vulkan_android_device_plan_chain(&p, &empty);
   chain = &outer;
   names[3] = "VK_KHR_swapchain";
   count = 4;
   assert(vulkan_android_device_plan_disable(&p, &chain, names, &count));
   assert(chain == &outer && outer.pNext == &empty && outer.timelineSemaphore
         && count == 1);
   assert(!strcmp(names[0], "VK_KHR_swapchain") && !p.supported);
   assert(!vulkan_android_device_plan_disable(&p, &chain, names, &count));
   assert(vulkan_android_device_plan_query(gpu, VK_API_VERSION_1_1, head_names,
         3, features, semaphores, properties, &p));
   chain = vulkan_android_device_plan_chain(&p, &outer);
   count = 4;
   assert(vulkan_android_device_plan_disable(&p, &chain, head_names, &count));
   assert(chain == &outer && outer.pNext == &empty && count == 1
         && !p.supported);
   puts("PASS: Android Vulkan own-device feature chain, required extensions, "
        "binary sync capabilities and descriptor budgets");
   return 0;
}
