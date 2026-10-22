/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RARCH_VAAPI_HEVC_VULKAN_CACHE_H
#define RARCH_VAAPI_HEVC_VULKAN_CACHE_H
#include "vaapi_hevc_vulkan_lease.h"
#include "vaapi_hevc_vulkan_resources.h"
RETRO_BEGIN_DECLS

typedef struct vaapi_hevc_vulkan_cache vaapi_hevc_vulkan_cache_t;

typedef struct vaapi_hevc_vulkan_cache_config
{
   const VkPhysicalDeviceMemoryProperties *memory;
   vulkan_prime_image_ops_t image;
   vaapi_hevc_vulkan_submit_ops_t submit;
   vaapi_hevc_vulkan_resource_ops_t resources;
   PFN_vkGetPhysicalDeviceFormatProperties2 get_format;
   PFN_vkGetPhysicalDeviceImageFormatProperties2 get_image;
   void (*destroy_deferred_destination)(VkDevice, void *);
   void *deferred_destination;
   VkPhysicalDevice gpu;
   VkDevice device;
   VkQueue queue;
   unsigned family;
   bool prime_device;
} vaapi_hevc_vulkan_cache_config_t;

/* Cold renderer owner; copies memory properties and operation tables.
 * Owns 1..32 conversion slots, borrowing the device and serialized queue. */
vaapi_hevc_vulkan_cache_t *vaapi_hevc_vulkan_cache_new(
      const vaapi_hevc_vulkan_cache_config_t *config, unsigned count,
      bool fp16);
/* Cold preparation: 1 ready, 0 pending/full, -1 refusal, -2 broken handoff.
 * Ordinary results preserve the caller's pending frame. No GPU wait. */
int vaapi_hevc_vulkan_cache_prepare(
      vaapi_hevc_vulkan_cache_t *cache, const video_hw_frame_t *frame);
/* Warm renderer tick, including paused playback: one status query per occupied
 * slot; 1 all idle, 0 pending, -1 retained error. Never closes native FDs. */
int vaapi_hevc_vulkan_cache_poll(vaapi_hevc_vulkan_cache_t *cache);
/* Warm path, no allocation/resource creation/destruction. Lease-present result
 * contract; a miss returns 0 and prepare_needed=true with the input untouched.
 * Destination must outlive its submission and any deferred device retirement.
 */
int vaapi_hevc_vulkan_cache_present(vaapi_hevc_vulkan_cache_t *cache,
      const video_hw_frame_t *frame, VkImage destination, VkImageView view,
      VkFormat format, unsigned width, unsigned height, VkImageLayout layout,
      bool *prepare_needed);
void vaapi_hevc_vulkan_cache_close(vaapi_hevc_vulkan_cache_t *cache);
/* Cold owner after close; false retains the cache for a later retry/defer. */
bool vaapi_hevc_vulkan_cache_free(vaapi_hevc_vulkan_cache_t *cache);
/* Existing serialized renderer/context lifecycle only. A deferred device must
 * not be cached. No frame/producer thread may access the deferred list. */
void vaapi_hevc_vulkan_cache_defer(vaapi_hevc_vulkan_cache_t *cache);
bool vaapi_hevc_vulkan_cache_device_pending(VkDevice device);
/* After existing idle/device-lost proof, before actual device destruction:
 * retire children but retain external leases. No polling or waits here. */
void vaapi_hevc_vulkan_cache_device_destroying(VkDevice device);
/* Only after destruction of this exact device; returns retained leases without
 * any Vulkan/VA call. Failed publication remains retained for another retry. */
void vaapi_hevc_vulkan_cache_device_destroyed(VkDevice device);
RETRO_END_DECLS
#endif
