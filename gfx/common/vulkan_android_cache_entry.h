/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#ifndef VULKAN_ANDROID_CACHE_ENTRY_H
#define VULKAN_ANDROID_CACHE_ENTRY_H
#include "vulkan_android_submit.h"
RETRO_BEGIN_DECLS
typedef struct vulkan_android_entry_ops
{
   PFN_vkAllocateCommandBuffers allocate_commands;
   PFN_vkFreeCommandBuffers free_commands;
   PFN_vkCreateFence create_fence;
   PFN_vkDestroyFence destroy_fence;
} vulkan_android_entry_ops_t;
typedef struct vulkan_android_entry_config
{
   VkDevice device;
   VkQueue queue;
   VkCommandPool command_pool;
   /* Optional ownership transfer to a successful cache_new. On failure the
    * caller retains the pool; deferred device destruction destroys it
    * implicitly. */
   PFN_vkDestroyCommandPool destroy_owned_pool;
   /* Only a deferred cache invokes this at context teardown, after device idle
    * (or device loss), before vkDestroyDevice. Normal texture retirement owns
    * it. */
   void (*destroy_deferred_destination)(VkDevice device, void *destination);
   void *deferred_destination;
   unsigned family, sampler_descriptors;
   const VkPhysicalDeviceMemoryProperties *memory;
   vulkan_android_image_ops_t image;
   vulkan_android_pipeline_ops_t pipeline;
   vulkan_android_sync_ops_t sync;
   vulkan_android_submit_ops_t submit;
   vulkan_android_entry_ops_t entry;
} vulkan_android_entry_config_t;
typedef struct vulkan_android_cache_entry
{
   vulkan_android_submission_t submission;
   VkCommandPool command_pool;
   vulkan_android_entry_ops_t ops;
   vulkan_android_image_ops_t image_ops;
} vulkan_android_cache_entry_t;
/* Cold renderer-owner setup. No ticket adoption; source AImage stays held by
 * caller throughout. An imported allocation retains its AHB independently.
 * config's device/queue/pool must survive entry retirement. Shader variants
 * are the shipped external-RGB converters. Failure unwinds every resource.
 * out must be fresh storage, never a live cache entry. */
bool vulkan_android_cache_entry_create(
      const vulkan_android_entry_config_t *config,
      const android_frame_view_t *source, bool fp16,
      vulkan_android_cache_entry_t *out);
/* Cold renderer owner. False retains resources if GPU completion is unproven.
 * No waits/device-idle. True clears entry after safe dependency-order teardown.
 */
bool vulkan_android_cache_entry_destroy(vulkan_android_cache_entry_t *entry);
RETRO_END_DECLS
#endif
