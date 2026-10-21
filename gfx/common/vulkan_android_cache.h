/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef VULKAN_ANDROID_CACHE_H
#define VULKAN_ANDROID_CACHE_H
#include "vulkan_android_cache_entry.h"
RETRO_BEGIN_DECLS
typedef struct vulkan_android_cache vulkan_android_cache_t;
/* Cold renderer-owner creation, 1..32 native buffers. Copies operation tables
 * and memory properties. Device/queue/resettable command pool remain borrowed;
 * their owner retains them until cache retirement. Fixed output encoding. */
vulkan_android_cache_t *vulkan_android_cache_new(const vulkan_android_entry_config_t *config,
      unsigned count,bool fp16);
/* Cold renderer-owner resource preparation, outside frame submission. The
 * original frame lease remains with the caller on all ordinary results:
 * 1 ready; 0 cache full with GPU work pending; -1 refusal; -2 invalid lease
 * handoff (claimed lease retained). Safe eviction never waits for GPU work. */
int vulkan_android_cache_prepare(vulkan_android_cache_t *cache,const video_hw_frame_t *frame);
/* Warm presentation: no resource creation/destruction, allocation or waits.
 * Same result/consumption contract as submission_present. On cache miss, returns
 * 0 and sets prepare_needed; caller retains frame for a later cold preparation.
 * A cold request must retain that frame's lease through prepare or cancellation. */
int vulkan_android_cache_present(vulkan_android_cache_t *cache,const video_hw_frame_t *frame,
      VkImage destination,VkImageView view,VkImageLayout layout,bool *prepare_needed);
void vulkan_android_cache_close(vulkan_android_cache_t *cache);
/* Cold renderer owner, only after close. False retains pending entries/cache;
 * call again later. True consumes cache. No waits/device-idle. */
bool vulkan_android_cache_free(vulkan_android_cache_t *cache);
/* Cold renderer owner only AFTER its VkDevice has actually been destroyed. */
bool vulkan_android_cache_after_device_destroy(vulkan_android_cache_t *cache);
RETRO_END_DECLS
#endif
