/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef VULKAN_ENCODE_DEVICE_H
#define VULKAN_ENCODE_DEVICE_H
#include "../include/vulkan/vulkan.h"
#include <boolean.h>
#include <retro_common_api.h>
RETRO_BEGIN_DECLS
typedef struct vulkan_encode_device_plan vulkan_encode_device_plan_t;
typedef struct vulkan_encode_device_ops
{
   PFN_vkGetPhysicalDeviceFeatures2 features;
   PFN_vkGetPhysicalDeviceQueueFamilyProperties2 families;
   PFN_vkEnumerateDeviceExtensionProperties extensions;
} vulkan_encode_device_ops_t;
/* Cold optional augmentation. Uses the lesser of actual instance/device API
 * versions. Preserves all caller structures and explicit feature opt-outs.
 * Reserves an unused encode queue, never a caller's existing queue handle.
 * NULL leaves ordinary device creation available without recording. */
vulkan_encode_device_plan_t *vulkan_encode_device_plan_new(
      VkPhysicalDevice physical, uint32_t api_version, unsigned graphics_family,
      const VkDeviceCreateInfo *info, const vulkan_encode_device_ops_t *ops);
/* Caller input pointers remain borrowed until plan_free. Consume the planned
 * create info synchronously, before freeing the plan. */
const VkDeviceCreateInfo *vulkan_encode_device_plan_info(const vulkan_encode_device_plan_t *plan);
bool vulkan_encode_device_plan_queue(const vulkan_encode_device_plan_t *plan,
      uint32_t *family, uint32_t *index, bool *storage_images);
/* True only when HEVC is enabled on the reserved encode queue. */
bool vulkan_encode_device_plan_hevc(const vulkan_encode_device_plan_t *plan);
void vulkan_encode_device_plan_free(vulkan_encode_device_plan_t *plan);
RETRO_END_DECLS
#endif
