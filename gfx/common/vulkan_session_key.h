/* Copyright (C) 2026 The RetroArch team. GPL-3.0-or-later */
#ifndef VULKAN_SESSION_KEY_H
#define VULKAN_SESSION_KEY_H

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <vulkan/vulkan.h>
#include <boolean.h>

typedef struct vulkan_session_key
{
   unsigned char *data;
   size_t size;
   VkStructureType unsupported_type;
   bool failed;
} vulkan_session_key_t;

static void vulkan_session_key_append(vulkan_session_key_t *key,
      const void *data, size_t size)
{
   unsigned char *next;
   if (key->failed || !size)
      return;
   if (!data || size > (size_t)-1 - key->size
         || !(next = (unsigned char*)realloc(key->data, key->size + size)))
   {
      key->failed = true;
      return;
   }
   key->data = next;
   memcpy(key->data + key->size, data, size);
   key->size += size;
}

#define VULKAN_SESSION_VALUE(key, value) \
   vulkan_session_key_append(key, &(value), sizeof(value))

static void vulkan_session_key_names(vulkan_session_key_t *key,
      uint32_t count, const char *const *names)
{
   uint32_t i;
   VULKAN_SESSION_VALUE(key, count);
   for (i = 0; i < count && !key->failed; i++)
   {
      if (!names || !names[i])
         key->failed = true;
      else
         vulkan_session_key_append(key, names[i], strlen(names[i]) + 1);
   }
}

static bool vulkan_session_key_equal(const vulkan_session_key_t *a,
      const vulkan_session_key_t *b)
{
   return a && b && !a->failed && !b->failed && a->size == b->size
      && (!a->size || !memcmp(a->data, b->data, a->size));
}

static void vulkan_session_key_free(vulkan_session_key_t *key)
{
   free(key->data);
   memset(key, 0, sizeof(*key));
}

static vulkan_session_key_t vulkan_session_instance_key(
      const VkInstanceCreateInfo *info)
{
   vulkan_session_key_t key;
   const char *names[2];
   memset(&key, 0, sizeof(key));
   if (info->pNext || !info->pApplicationInfo
         || info->pApplicationInfo->pNext)
   {
      key.failed = true;
      return key;
   }
   names[0] = info->pApplicationInfo->pApplicationName
      ? info->pApplicationInfo->pApplicationName : "";
   names[1] = info->pApplicationInfo->pEngineName
      ? info->pApplicationInfo->pEngineName : "";
   VULKAN_SESSION_VALUE(&key, info->flags);
   VULKAN_SESSION_VALUE(&key, info->pApplicationInfo->apiVersion);
   VULKAN_SESSION_VALUE(&key, info->pApplicationInfo->applicationVersion);
   VULKAN_SESSION_VALUE(&key, info->pApplicationInfo->engineVersion);
   vulkan_session_key_names(&key, 2, names);
   vulkan_session_key_names(&key, info->enabledExtensionCount,
         info->ppEnabledExtensionNames);
   vulkan_session_key_names(&key, info->enabledLayerCount,
         info->ppEnabledLayerNames);
   return key;
}

/* Unknown chains must create a new device. Never guess their layout. */
static void vulkan_session_feature_key(vulkan_session_key_t *key,
      const VkBaseInStructure *node)
{
   for (; node && !key->failed; node = node->pNext)
   {
      size_t offset = 0, size = 0;
      VULKAN_SESSION_VALUE(key, node->sType);
#define VULKAN_SESSION_FEATURE(stype, type, first, last) \
      case stype: \
         offset = offsetof(type, first); \
         size = offsetof(type, last) + sizeof(((type*)0)->last) - offset; \
         break
      /* Compatibility tags may not be members of the SDK's enum. */
      switch ((uint32_t)node->sType)
      {
         VULKAN_SESSION_FEATURE(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
               VkPhysicalDeviceFeatures2, features, features);
         VULKAN_SESSION_FEATURE(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
               VkPhysicalDeviceVulkan11Features, storageBuffer16BitAccess, shaderDrawParameters);
         VULKAN_SESSION_FEATURE(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
               VkPhysicalDeviceVulkan12Features, samplerMirrorClampToEdge, subgroupBroadcastDynamicId);
         VULKAN_SESSION_FEATURE(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
               VkPhysicalDeviceVulkan13Features, robustImageAccess, maintenance4);
         VULKAN_SESSION_FEATURE(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES,
               VkPhysicalDeviceTimelineSemaphoreFeatures, timelineSemaphore, timelineSemaphore);
         VULKAN_SESSION_FEATURE(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES,
               VkPhysicalDeviceBufferDeviceAddressFeatures, bufferDeviceAddress, bufferDeviceAddressMultiDevice);
         VULKAN_SESSION_FEATURE(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES,
               VkPhysicalDeviceSynchronization2Features, synchronization2, synchronization2);
         VULKAN_SESSION_FEATURE(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES,
               VkPhysicalDeviceDynamicRenderingFeatures, dynamicRendering, dynamicRendering);
         VULKAN_SESSION_FEATURE(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_FEATURES,
               VkPhysicalDeviceSubgroupSizeControlFeatures, subgroupSizeControl, computeFullSubgroups);
         VULKAN_SESSION_FEATURE(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES,
               VkPhysicalDeviceScalarBlockLayoutFeatures, scalarBlockLayout, scalarBlockLayout);
         VULKAN_SESSION_FEATURE(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_UNIFORM_BUFFER_STANDARD_LAYOUT_FEATURES,
               VkPhysicalDeviceUniformBufferStandardLayoutFeatures, uniformBufferStandardLayout, uniformBufferStandardLayout);
         VULKAN_SESSION_FEATURE(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PRIORITY_FEATURES_EXT,
               VkPhysicalDeviceMemoryPriorityFeaturesEXT, memoryPriority, memoryPriority);
         VULKAN_SESSION_FEATURE(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PIPELINE_CREATION_CACHE_CONTROL_FEATURES,
               VkPhysicalDevicePipelineCreationCacheControlFeatures, pipelineCreationCacheControl, pipelineCreationCacheControl);
         VULKAN_SESSION_FEATURE(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES,
               VkPhysicalDeviceDescriptorIndexingFeatures, shaderInputAttachmentArrayDynamicIndexing, runtimeDescriptorArray);
         VULKAN_SESSION_FEATURE(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT,
               VkPhysicalDeviceRobustness2FeaturesEXT, robustBufferAccess2, nullDescriptor);
         VULKAN_SESSION_FEATURE(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES,
               VkPhysicalDeviceShaderFloat16Int8Features, shaderFloat16, shaderInt8);
         VULKAN_SESSION_FEATURE(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES,
               VkPhysicalDevice16BitStorageFeatures, storageBuffer16BitAccess, storageInputOutput16);
         VULKAN_SESSION_FEATURE(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_8BIT_STORAGE_FEATURES,
               VkPhysicalDevice8BitStorageFeatures, storageBuffer8BitAccess, storagePushConstant8);
         VULKAN_SESSION_FEATURE(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT,
               VkPhysicalDeviceExtendedDynamicStateFeaturesEXT, extendedDynamicState, extendedDynamicState);
         VULKAN_SESSION_FEATURE(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_2_FEATURES_EXT,
               VkPhysicalDeviceExtendedDynamicState2FeaturesEXT, extendedDynamicState2, extendedDynamicState2PatchControlPoints);
         VULKAN_SESSION_FEATURE(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_3_FEATURES_EXT,
               VkPhysicalDeviceExtendedDynamicState3FeaturesEXT, extendedDynamicState3TessellationDomainOrigin, extendedDynamicState3ShadingRateImageEnable);
#ifdef VK_KHR_unified_image_layouts
         VULKAN_SESSION_FEATURE(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_UNIFIED_IMAGE_LAYOUTS_FEATURES_KHR,
               VkPhysicalDeviceUnifiedImageLayoutsFeaturesKHR, unifiedImageLayouts, unifiedImageLayoutsVideo);
#endif
         default:
            key->failed = true;
            key->unsupported_type = node->sType;
            break;
      }
#undef VULKAN_SESSION_FEATURE
      vulkan_session_key_append(key, (const unsigned char*)node + offset, size);
   }
}

static vulkan_session_key_t vulkan_session_device_key(
      VkPhysicalDevice gpu, const VkDeviceCreateInfo *info)
{
   vulkan_session_key_t key;
   VkPhysicalDeviceFeatures features;
   uint32_t i;
   memset(&key, 0, sizeof(key));
   memset(&features, 0, sizeof(features));
   if (info->pEnabledFeatures)
      features = *info->pEnabledFeatures;
   VULKAN_SESSION_VALUE(&key, gpu);
   VULKAN_SESSION_VALUE(&key, info->flags);
   VULKAN_SESSION_VALUE(&key, features);
   VULKAN_SESSION_VALUE(&key, info->queueCreateInfoCount);
   for (i = 0; i < info->queueCreateInfoCount; i++)
   {
      const VkDeviceQueueCreateInfo *q = &info->pQueueCreateInfos[i];
      if (q->pNext)
         key.failed = true;
      VULKAN_SESSION_VALUE(&key, q->flags);
      VULKAN_SESSION_VALUE(&key, q->queueFamilyIndex);
      VULKAN_SESSION_VALUE(&key, q->queueCount);
      vulkan_session_key_append(&key, q->pQueuePriorities,
            q->queueCount * sizeof(float));
   }
   vulkan_session_key_names(&key, info->enabledExtensionCount,
         info->ppEnabledExtensionNames);
   vulkan_session_key_names(&key, info->enabledLayerCount,
         info->ppEnabledLayerNames);
   vulkan_session_feature_key(&key, (const VkBaseInStructure*)info->pNext);
   return key;
}

#undef VULKAN_SESSION_VALUE
#endif
