/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#ifndef VULKAN_ANDROID_CACHE_H
#define VULKAN_ANDROID_CACHE_H
#include "vulkan_android_cache_entry.h"
RETRO_BEGIN_DECLS
typedef struct vulkan_android_cache vulkan_android_cache_t;
/* Cold renderer-owner creation, 1..32 native buffers. Copies operation tables
 * and memory properties. Device/queue remain borrowed. The resettable command
 * pool is borrowed unless destroy_owned_pool transfers ownership on success.
 * Borrowed owners retain their resources until cache retirement. Fixed output
 * encoding. */
vulkan_android_cache_t *vulkan_android_cache_new(
      const vulkan_android_entry_config_t *config, unsigned count, bool fp16);
/* Cold renderer-owner resource preparation, outside frame submission. The
 * original frame lease remains with the caller on all ordinary results:
 * 1 ready; 0 cache full with GPU work pending; -1 refusal; -2 invalid lease
 * handoff (claimed lease retained). Safe eviction never waits for GPU work. */
int vulkan_android_cache_prepare(
      vulkan_android_cache_t *cache, const video_hw_frame_t *frame);
/* Warm presentation: no resource creation/destruction, allocation or waits.
 * Same result/consumption contract as submission_present. On cache miss,
 * returns 0 and sets prepare_needed; caller retains frame for a later cold
 * preparation. A cold request must retain that frame's lease through prepare or
 * cancellation. */
int vulkan_android_cache_present(vulkan_android_cache_t *cache,
      const video_hw_frame_t *frame, VkImage destination, VkImageView view,
      VkImageLayout layout, bool *prepare_needed);
/* Renderer-owner zero-timeout completion polling, including idle playback.
 * 1 all idle; 0 pending; -1 at least one failed entry. */
int vulkan_android_cache_poll(vulkan_android_cache_t *cache);
void vulkan_android_cache_close(vulkan_android_cache_t *cache);
/* Cold renderer owner, only after close. False retains pending entries/cache;
 * call again later. True consumes cache. No waits/device-idle. */
bool vulkan_android_cache_free(vulkan_android_cache_t *cache);
/* Cold context owner only AFTER device_destroying and actual VkDevice
 * destruction. Refuses if Vulkan children have not been retired first. */
bool vulkan_android_cache_after_device_destroy(vulkan_android_cache_t *cache);
/* Cold renderer shutdown transfers ownership of a closed, unfreeable cache.
 * Calls are serialized by the existing renderer/context lifecycle owner.
 * A pending device must not be cached: destroy it and notify afterward. */
void vulkan_android_cache_defer(vulkan_android_cache_t *cache);
bool vulkan_android_cache_device_pending(VkDevice device);
/* Cold lifecycle owner. Collect only entries whose fences prove completion.
 * Pending or ambiguous work remains retained; no waits. */
void vulkan_android_cache_device_collect(VkDevice device);
/* Cold context teardown after vkDeviceWaitIdle returned success/device-lost,
 * before vkDestroyDevice. Destroys Vulkan children while retaining image
 * leases. */
void vulkan_android_cache_device_destroying(VkDevice device);
/* Only after vkDestroyDevice returned. Releases matching caches, without any
 * call through their Vulkan operation tables. Other devices remain retained. */
void vulkan_android_cache_device_destroyed(VkDevice device);
RETRO_END_DECLS
#endif
