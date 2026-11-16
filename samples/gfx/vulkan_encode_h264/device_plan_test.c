/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "../../../gfx/common/vulkan_encode_device.h"
static unsigned allocations, live, fail_at;
static void *device_allocate(size_t count, size_t size)
{
   void *p;
   if (++allocations == fail_at)
      return NULL;
   p = calloc(count, size);
   if (p) live++;
   return p;
}
static void device_release(void *p)
{
   if (p) { assert(live); live--; }
   free(p);
}
#define calloc device_allocate
#define free device_release
#include "../../../gfx/common/vulkan_encode_device.c"
#undef calloc
#undef free
static const char *available[] = {
   VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_KHR_VIDEO_QUEUE_EXTENSION_NAME,
   VK_KHR_VIDEO_ENCODE_QUEUE_EXTENSION_NAME, VK_KHR_VIDEO_ENCODE_H264_EXTENSION_NAME,
   VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME, VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
   VK_KHR_VIDEO_ENCODE_H265_EXTENSION_NAME};
static VkDeviceCreateInfo request;
static VkDeviceQueueCreateInfo queues[3], saved_queues[3];
static VkPhysicalDeviceFeatures requested_features;
static VkPhysicalDeviceVulkan12Features version12;
static VkPhysicalDeviceVulkan13Features version13;
static VkPhysicalDeviceTimelineSemaphoreFeatures timeline;
static VkPhysicalDeviceSynchronization2Features sync2;
static VkPhysicalDeviceVulkan11Features version11;
static float priorities[] = {.25f,.75f,.5f,.125f};
static bool extensions_missing, features_off, compute_off, exhausted, grow, no_dedicated;
static unsigned features_calls, family_calls;
static bool hevc_extension, hevc_queue;
static VKAPI_ATTR VkResult VKAPI_CALL device_extensions(VkPhysicalDevice physical,
      const char *layer, uint32_t *count, VkExtensionProperties *out)
{
   unsigned i, n = extensions_missing ? 1 : hevc_extension ? 7 : 6;
   assert(physical && !layer);
   if (out)
   {
      assert(*count >= n);
      for (i = 0; i < n; i++)
         strcpy(out[i].extensionName, available[i]);
   }
   *count = n;
   return VK_SUCCESS;
}
static VKAPI_ATTR void VKAPI_CALL device_families(VkPhysicalDevice physical,
      uint32_t *count, VkQueueFamilyProperties2 *out)
{
   unsigned i;
   assert(physical);
   family_calls++;
   if (out)
   {
      assert(*count == 3);
      for (i = 0; i < 3; i++)
      {
         VkQueueFamilyVideoPropertiesKHR *video = (VkQueueFamilyVideoPropertiesKHR*)out[i].pNext;
         assert(out[i].sType == VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2);
         assert(video->sType == VK_STRUCTURE_TYPE_QUEUE_FAMILY_VIDEO_PROPERTIES_KHR);
         out[i].queueFamilyProperties.queueCount = i == 2 ? no_dedicated ? 0 : 1 : exhausted ? 1 : 4;
         out[i].queueFamilyProperties.queueFlags = i ? VK_QUEUE_VIDEO_ENCODE_BIT_KHR :
            VK_QUEUE_GRAPHICS_BIT | (compute_off ? 0 : VK_QUEUE_COMPUTE_BIT);
         video->videoCodecOperations = i ? VK_VIDEO_CODEC_OPERATION_ENCODE_H264_BIT_KHR : 0;
         if (i && hevc_queue)
            video->videoCodecOperations |= VK_VIDEO_CODEC_OPERATION_ENCODE_H265_BIT_KHR;
      }
   }
   *count = out && grow ? 4 : 3;
}
static VKAPI_ATTR void VKAPI_CALL device_features(VkPhysicalDevice physical,
      VkPhysicalDeviceFeatures2 *out)
{
   VkBaseOutStructure *node;
   assert(physical);
   features_calls++;
   out->features.shaderStorageImageExtendedFormats = VK_TRUE;
   for (node = (VkBaseOutStructure*)out->pNext; node; node = node->pNext)
      if (node->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES)
         ((VkPhysicalDeviceTimelineSemaphoreFeatures*)node)->timelineSemaphore = features_off ? VK_FALSE : VK_TRUE;
      else if (node->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES)
         ((VkPhysicalDeviceSynchronization2Features*)node)->synchronization2 = features_off ? VK_FALSE : VK_TRUE;
      else assert(!"unexpected queried feature");
}
static const vulkan_encode_device_ops_t device_ops = {device_features, device_families, device_extensions};
static void setup(void)
{
   unsigned i;
   assert(!live);
   allocations = fail_at = features_calls = family_calls = 0;
   hevc_extension = hevc_queue = false;
   extensions_missing = features_off = compute_off = exhausted = grow = no_dedicated = false;
   memset(&request, 0, sizeof(request));
   memset(queues, 0, sizeof(queues));
   memset(&requested_features, 0, sizeof(requested_features));
   request.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
   request.queueCreateInfoCount = 2;
   request.pQueueCreateInfos = queues;
   request.enabledExtensionCount = 1;
   request.ppEnabledExtensionNames = available;
   request.pEnabledFeatures = &requested_features;
   for (i = 0; i < 3; i++)
   {
      queues[i].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
      queues[i].queueFamilyIndex = i;
      queues[i].queueCount = 1;
      queues[i].pQueuePriorities = priorities;
   }
   memcpy(saved_queues, queues, sizeof(queues));
}
static vulkan_encode_device_plan_t *plan(uint32_t version)
{
   return vulkan_encode_device_plan_new((VkPhysicalDevice)(uintptr_t)1, version, 0, &request, &device_ops);
}
static void normal(void)
{
   vulkan_encode_device_plan_t *p;
   const VkDeviceCreateInfo *out;
   const VkBaseInStructure *node;
   uint32_t version, family, index;
   bool storage;
   unsigned i, count;
   for (version = VK_API_VERSION_1_1; version <= VK_API_VERSION_1_3; version += VK_MAKE_API_VERSION(0,0,1,0))
   {
      setup();
      p = plan(version);
      assert(p && live == 1);
      out = vulkan_encode_device_plan_info(p);
      assert(out != &request && out->queueCreateInfoCount == 3);
      assert(vulkan_encode_device_plan_queue(p, &family, &index, &storage));
      assert(family == 2 && !index && !storage);
      assert(out->pEnabledFeatures == &requested_features && out->flags == request.flags);
      assert(out->enabledExtensionCount == (version == VK_API_VERSION_1_1 ? 6u : version == VK_API_VERSION_1_2 ? 5u : 4u));
      assert(!memcmp(queues, saved_queues, sizeof(queues)));
      for (i = 0; i < 2; i++)
         assert(!memcmp(&out->pQueueCreateInfos[i], &queues[i], sizeof(queues[i])));
      count = 0;
      for (node = (const VkBaseInStructure*)out->pNext; node; node = node->pNext) count++;
      assert(count == 2 && !request.pNext && !requested_features.shaderStorageImageExtendedFormats);
      vulkan_encode_device_plan_free(p);
   }
   setup();
   request.queueCreateInfoCount = 3;
   p = plan(VK_API_VERSION_1_1);
   assert(p && vulkan_encode_device_plan_queue(p, &family, &index, &storage));
   assert(family == 1 && index == 1);
   out = vulkan_encode_device_plan_info(p);
   assert(out->queueCreateInfoCount == 3 && out->pQueueCreateInfos[1].queueCount == 2);
   assert(out->pQueueCreateInfos[1].pQueuePriorities[0] == priorities[0]);
   assert(out->pQueueCreateInfos[1].pQueuePriorities[1] == 1.0f);
   assert(queues[1].queueCount == 1 && queues[1].pQueuePriorities == priorities);
   vulkan_encode_device_plan_free(p);
}
static void feature_chains(void)
{
   vulkan_encode_device_plan_t *p;
   unsigned n;
   setup();
   memset(&version12, 0, sizeof(version12));
   memset(&version13, 0, sizeof(version13));
   version12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
   version12.pNext = &version13;
   version13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
   version12.timelineSemaphore = version13.synchronization2 = VK_TRUE;
   request.pNext = &version12;
   p = plan(VK_API_VERSION_1_3);
   assert(p && vulkan_encode_device_plan_info(p)->pNext == &version12);
   vulkan_encode_device_plan_free(p);
   for (n = 0; n < 4; n++)
   {
      memset(&timeline, 0, sizeof(timeline));
      memset(&sync2, 0, sizeof(sync2));
      timeline.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES;
      sync2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES;
      request.pNext = n == 0 ? (void*)&timeline : n == 1 ? (void*)&sync2 : n == 2 ? (void*)&version12 : (void*)&version13;
      if (n == 2) version12.timelineSemaphore = VK_FALSE;
      if (n == 3) version13.synchronization2 = VK_FALSE;
      assert(!plan(VK_API_VERSION_1_3) && !live);
   }
   timeline.timelineSemaphore = sync2.synchronization2 = VK_TRUE;
   timeline.pNext = &sync2;
   request.pNext = &timeline;
   p = plan(VK_API_VERSION_1_1);
   assert(p && vulkan_encode_device_plan_info(p)->pNext == &timeline);
   vulkan_encode_device_plan_free(p);
   version12.timelineSemaphore = VK_TRUE;
   version13.synchronization2 = VK_TRUE;
   timeline.pNext = &version12;
   assert(!plan(VK_API_VERSION_1_3) && !live);
   sync2.pNext = &version13;
   request.pNext = &sync2;
   assert(!plan(VK_API_VERSION_1_3) && !live);
   memset(&version11, 0, sizeof(version11));
   version11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
   version11.storageBuffer16BitAccess = VK_TRUE;
   request.pNext = &version11;
   p = plan(VK_API_VERSION_1_1);
   assert(p);
   {
      const VkBaseInStructure *node = (const VkBaseInStructure*)vulkan_encode_device_plan_info(p)->pNext;
      assert(node->pNext->pNext == (const VkBaseInStructure*)&version11);
      assert(!version11.pNext && version11.storageBuffer16BitAccess);
   }
   vulkan_encode_device_plan_free(p);
   version11.pNext = &version11;
   assert(!plan(VK_API_VERSION_1_1) && !live);
}
static void refusal(void)
{
   unsigned n;
   setup();
   extensions_missing = true;
   assert(!plan(VK_API_VERSION_1_1) && !features_calls && !family_calls && !live);
   setup();
   assert(!plan(VK_API_VERSION_1_0) && !features_calls && !family_calls && !allocations);
   setup(); features_off = true;
   assert(!plan(VK_API_VERSION_1_1) && !live);
   setup(); compute_off = true;
   assert(!plan(VK_API_VERSION_1_1) && !live);
   setup(); queues[0].queueFamilyIndex = 2;
   assert(!plan(VK_API_VERSION_1_1) && !live);
   setup(); grow = true;
   assert(!plan(VK_API_VERSION_1_1) && !live);
   setup(); exhausted = true; request.queueCreateInfoCount = 3;
   assert(!plan(VK_API_VERSION_1_1) && !live);
   setup(); queues[1].flags = VK_DEVICE_QUEUE_CREATE_PROTECTED_BIT; request.queueCreateInfoCount = 3;
   assert(!plan(VK_API_VERSION_1_1) && !live);
   setup(); no_dedicated = true; request.queueCreateInfoCount = 3;
   queues[1].queueCount = 3;
   queues[2].queueFamilyIndex = 1;
   queues[2].flags = VK_DEVICE_QUEUE_CREATE_PROTECTED_BIT;
   assert(!plan(VK_API_VERSION_1_1) && !live);
   for (n = 1; n <= 3; n++)
   {
      setup(); fail_at = n;
      assert(!plan(VK_API_VERSION_1_1) && !live);
   }
}
static void optional_hevc(void)
{
   unsigned mode, i, count;
   vulkan_encode_device_plan_t *p;
   const VkDeviceCreateInfo *info;
   assert(!vulkan_encode_device_plan_hevc(NULL));
   for (mode = 0; mode < 4; mode++)
   {
      setup();
      hevc_extension = (mode & 1) != 0;
      hevc_queue = (mode & 2) != 0;
      p = plan(VK_API_VERSION_1_1);
      assert(p && vulkan_encode_device_plan_hevc(p) == (mode == 3));
      info = vulkan_encode_device_plan_info(p);
      count = 0;
      for (i = 0; i < info->enabledExtensionCount; i++)
         if (!strcmp(info->ppEnabledExtensionNames[i], VK_KHR_VIDEO_ENCODE_H265_EXTENSION_NAME))
            count++;
      assert(count == (mode == 3 ? 1u : 0u));
      vulkan_encode_device_plan_free(p);
      assert(!live);
   }
}
int main(void)
{
   normal();
   feature_chains();
   refusal();
   optional_hevc();
   puts("Vulkan encode device: dedicated queues, feature-chain preservation, extension dependencies and refusal PASS");
   return 0;
}
