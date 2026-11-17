/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "vulkan_encode_device.h"
#include <stdlib.h>
#include <string.h>

struct vulkan_encode_device_plan
{
   VkDeviceCreateInfo info;
   VkPhysicalDeviceTimelineSemaphoreFeatures timeline;
   VkPhysicalDeviceSynchronization2Features sync;
   VkDeviceQueueCreateInfo queues[65];
   const char *extensions[518];
   float priorities[65];
   unsigned family, index;
   bool storage, hevc;
};
struct vulkan_encode_device_probe
{
   VkPhysicalDeviceFeatures2 features;
   VkPhysicalDeviceTimelineSemaphoreFeatures timeline;
   VkPhysicalDeviceSynchronization2Features sync;
   VkQueueFamilyProperties2 families[64];
   VkQueueFamilyVideoPropertiesKHR video[64];
   VkExtensionProperties *extensions;
   uint32_t count, extension_count;
};
static bool ved_extension(vulkan_encode_device_plan_t *p,
      const struct vulkan_encode_device_probe *probe, const char *name)
{
   unsigned i;
   for (i = 0; i < probe->extension_count; i++)
      if (!strcmp(probe->extensions[i].extensionName, name))
         break;
   if (i == probe->extension_count)
      return false;
   for (i = 0; i < p->info.enabledExtensionCount; i++)
      if (!strcmp(p->extensions[i], name))
         return true;
   p->extensions[p->info.enabledExtensionCount++] = name;
   return true;
}
static bool ved_features(vulkan_encode_device_plan_t *p,
      const struct vulkan_encode_device_probe *probe)
{
   const VkBaseInStructure *node = (const VkBaseInStructure*)p->info.pNext;
   bool timeline = false, sync = false;
   unsigned count = 0;
   p->storage = p->info.pEnabledFeatures && p->info.pEnabledFeatures->shaderStorageImageExtendedFormats;
   for (; node; node = node->pNext)
   {
      if (++count > 64)
         return false;
      switch (node->sType)
      {
         case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2:
            if (p->info.pEnabledFeatures)
               return false;
            p->storage = ((const VkPhysicalDeviceFeatures2*)node)->features.shaderStorageImageExtendedFormats != 0;
            break;
         case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES:
            if (timeline || !((const VkPhysicalDeviceVulkan12Features*)node)->timelineSemaphore)
               return false;
            timeline = true;
            break;
         case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES:
            if (timeline || !((const VkPhysicalDeviceTimelineSemaphoreFeatures*)node)->timelineSemaphore)
               return false;
            timeline = true;
            break;
         case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES:
            if (sync || !((const VkPhysicalDeviceVulkan13Features*)node)->synchronization2)
               return false;
            sync = true;
            break;
         case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES:
            if (sync || !((const VkPhysicalDeviceSynchronization2Features*)node)->synchronization2)
               return false;
            sync = true;
            break;
         default:
            break;
      }
   }
   if (!probe->timeline.timelineSemaphore || !probe->sync.synchronization2)
      return false;
   if (!timeline)
   {
      p->timeline.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES;
      p->timeline.timelineSemaphore = VK_TRUE;
      p->timeline.pNext = (void*)p->info.pNext;
      p->info.pNext = &p->timeline;
   }
   if (!sync)
   {
      p->sync.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES;
      p->sync.synchronization2 = VK_TRUE;
      p->sync.pNext = (void*)p->info.pNext;
      p->info.pNext = &p->sync;
   }
   return true;
}
static bool ved_queue(vulkan_encode_device_plan_t *p,
      const struct vulkan_encode_device_probe *probe, unsigned graphics)
{
   unsigned pass, family, q, i;
   if (graphics >= probe->count ||
         (probe->families[graphics].queueFamilyProperties.queueFlags
            & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT))
            != (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT))
      return false;
   for (q = 0; q < p->info.queueCreateInfoCount; q++)
      if (p->queues[q].queueFamilyIndex == graphics && p->queues[q].queueCount)
         break;
   if (q == p->info.queueCreateInfoCount)
      return false;
   for (pass = 0; pass < 2; pass++)
      for (family = 0; family < probe->count; family++)
      {
         const VkQueueFamilyProperties *properties = &probe->families[family].queueFamilyProperties;
         if (!(properties->queueFlags & VK_QUEUE_VIDEO_ENCODE_BIT_KHR)
               || !(probe->video[family].videoCodecOperations & VK_VIDEO_CODEC_OPERATION_ENCODE_H264_BIT_KHR)
               || !properties->queueCount)
            continue;
         for (q = 0; q < p->info.queueCreateInfoCount; q++)
            if (p->queues[q].queueFamilyIndex == family)
               break;
         for (i = q + 1; i < p->info.queueCreateInfoCount; i++)
            if (p->queues[i].queueFamilyIndex == family)
               break;
         if (i < p->info.queueCreateInfoCount)
            continue;
         if (!pass && q == p->info.queueCreateInfoCount)
         {
            p->priorities[0] = 1.0f;
            p->queues[q].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            p->queues[q].queueFamilyIndex = family;
            p->queues[q].queueCount = 1;
            p->queues[q].pQueuePriorities = p->priorities;
            p->info.queueCreateInfoCount++;
            p->family = family;
            return true;
         }
         if (pass && q < p->info.queueCreateInfoCount && !p->queues[q].flags
               && p->queues[q].queueCount < properties->queueCount
               && p->queues[q].queueCount < 64 && p->queues[q].pQueuePriorities)
         {
            p->index = p->queues[q].queueCount;
            for (i = 0; i < p->index; i++)
               p->priorities[i] = p->queues[q].pQueuePriorities[i];
            p->priorities[p->index] = 1.0f;
            p->queues[q].pQueuePriorities = p->priorities;
            p->queues[q].queueCount++;
            p->family = family;
            return true;
         }
      }
   return false;
}
vulkan_encode_device_plan_t *vulkan_encode_device_plan_new(
      VkPhysicalDevice physical, uint32_t api_version, unsigned graphics,
      const VkDeviceCreateInfo *info, const vulkan_encode_device_ops_t *ops)
{
   vulkan_encode_device_plan_t *p = NULL;
   struct vulkan_encode_device_probe *probe;
   unsigned i, capacity;
   if (!physical || api_version < VK_API_VERSION_1_1 || !info || !ops
         || !ops->features || !ops->families || !ops->extensions
         || !info->queueCreateInfoCount || info->queueCreateInfoCount > 64
         || !info->pQueueCreateInfos || info->enabledExtensionCount > 512
         || (info->enabledExtensionCount && !info->ppEnabledExtensionNames))
      return NULL;
   probe = (struct vulkan_encode_device_probe*)calloc(1, sizeof(*probe));
   if (!probe)
      return NULL;
   p = (vulkan_encode_device_plan_t*)calloc(1, sizeof(*p));
   if (!p)
      goto fail;
   p->info = *info;
   memcpy(p->queues, info->pQueueCreateInfos, info->queueCreateInfoCount * sizeof(*p->queues));
   for (i = 0; i < info->enabledExtensionCount; i++)
   {
      if (!info->ppEnabledExtensionNames[i])
         goto fail;
      p->extensions[i] = info->ppEnabledExtensionNames[i];
   }
   p->info.pQueueCreateInfos = p->queues;
   p->info.ppEnabledExtensionNames = p->extensions;
   if (ops->extensions(physical, NULL, &probe->extension_count, NULL) != VK_SUCCESS
         || !probe->extension_count || probe->extension_count > 512)
      goto fail;
   capacity = probe->extension_count;
   probe->extensions = (VkExtensionProperties*)calloc(capacity, sizeof(*probe->extensions));
   if (!probe->extensions || ops->extensions(physical, NULL, &probe->extension_count,
            probe->extensions) != VK_SUCCESS || probe->extension_count > capacity
         || !ved_extension(p, probe, VK_KHR_VIDEO_QUEUE_EXTENSION_NAME)
         || !ved_extension(p, probe, VK_KHR_VIDEO_ENCODE_QUEUE_EXTENSION_NAME)
         || !ved_extension(p, probe, VK_KHR_VIDEO_ENCODE_H264_EXTENSION_NAME)
         || (api_version < VK_API_VERSION_1_3 && !ved_extension(p, probe, VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME))
         || (api_version < VK_API_VERSION_1_2 && !ved_extension(p, probe, VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME)))
      goto fail;
   ops->families(physical, &probe->count, NULL);
   if (!probe->count || probe->count > 64)
      goto fail;
   capacity = probe->count;
   for (i = 0; i < capacity; i++)
   {
      probe->families[i].sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2;
      probe->families[i].pNext = &probe->video[i];
      probe->video[i].sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_VIDEO_PROPERTIES_KHR;
   }
   ops->families(physical, &probe->count, probe->families);
   if (probe->count > capacity || !ved_queue(p, probe, graphics))
      goto fail;
   p->hevc = (probe->video[p->family].videoCodecOperations
         & VK_VIDEO_CODEC_OPERATION_ENCODE_H265_BIT_KHR) != 0
      && ved_extension(p, probe, VK_KHR_VIDEO_ENCODE_H265_EXTENSION_NAME);
   probe->features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
   probe->features.pNext = &probe->sync;
   probe->sync.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES;
   probe->sync.pNext = &probe->timeline;
   probe->timeline.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES;
   ops->features(physical, &probe->features);
   if (!ved_features(p, probe))
      goto fail;
   free(probe->extensions);
   free(probe);
   return p;
fail:
   free(probe->extensions);
   free(probe);
   free(p);
   return NULL;
}
const VkDeviceCreateInfo *vulkan_encode_device_plan_info(const vulkan_encode_device_plan_t *p)
{
   return p ? &p->info : NULL;
}
bool vulkan_encode_device_plan_queue(const vulkan_encode_device_plan_t *p,
      uint32_t *family, uint32_t *index, bool *storage)
{
   if (!p || !family || !index || !storage)
      return false;
   *family = p->family;
   *index = p->index;
   *storage = p->storage;
   return true;
}
bool vulkan_encode_device_plan_hevc(const vulkan_encode_device_plan_t *p)
{
   return p && p->hevc;
}
void vulkan_encode_device_plan_free(vulkan_encode_device_plan_t *p)
{
   free(p);
}
