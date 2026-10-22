/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#ifndef VULKAN_ANDROID_DEVICE_H
#define VULKAN_ANDROID_DEVICE_H
#include <vulkan/vulkan_core.h>
#include <boolean.h>
#include <retro_common_api.h>
RETRO_BEGIN_DECLS
typedef struct vulkan_android_device_plan
{
   VkPhysicalDeviceSamplerYcbcrConversionFeatures conversion;
   unsigned sampler_descriptors;
   bool supported, legacy_descriptor_budget;
} vulkan_android_device_plan_t;
/* Cold own-device creation only. extensions are the exact names that will be
 * enabled, not merely advertised. Requires Vulkan 1.1, AHB/FOREIGN/SYNC_FD,
 * conversion feature support, and binary SYNC_FD import/export support.
 * api_version is the minimum of requested instance and physical-device API.
 * Query does not enable or advertise a backend. Failure clears plan. */
bool vulkan_android_device_plan_query(VkPhysicalDevice gpu,
      unsigned api_version, const char *const *extensions, unsigned count,
      PFN_vkGetPhysicalDeviceFeatures2 features,
      PFN_vkGetPhysicalDeviceExternalSemaphoreProperties semaphores,
      PFN_vkGetPhysicalDeviceProperties2 properties,
      vulkan_android_device_plan_t *plan);
/* Names added only for this optional frontend-owned import path. */
bool vulkan_android_device_extension(const char *name);
/* Keep plan alive through vkCreateDevice. Only success of that creation proves
 * conversion enabled. Never append to a core-owned device's feature chain. */
const void *vulkan_android_device_plan_chain(
      vulkan_android_device_plan_t *plan, const void *previous);
/* Cold failed-create fallback, own mutable feature chain only. Removes this
 * plan's feature and its optional extension names, preserving other requests.
 * True means something was removed and retry is useful. Bounded chain scan;
 * malformed/foreign chains refuse without mutation. */
bool vulkan_android_device_plan_disable(vulkan_android_device_plan_t *plan,
      const void **chain, const char **extensions, unsigned *count);
RETRO_END_DECLS
#endif
