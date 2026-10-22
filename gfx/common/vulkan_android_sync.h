/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#ifndef VULKAN_ANDROID_SYNC_H
#define VULKAN_ANDROID_SYNC_H
#include <vulkan/vulkan_core.h>
#include <boolean.h>
#include <retro_common_api.h>
RETRO_BEGIN_DECLS
typedef struct vulkan_android_sync_ops
{
   PFN_vkCreateSemaphore create;
   PFN_vkDestroySemaphore destroy;
   PFN_vkImportSemaphoreFdKHR import_fd;
   PFN_vkGetSemaphoreFdKHR export_fd;
} vulkan_android_sync_ops_t;
typedef struct vulkan_android_sync
{
   VkSemaphore ready, done;
} vulkan_android_sync_t;
/* Cold renderer-owner cache setup. External semaphore SYNC_FD import/export
 * support must be queried/enabled first. Both semaphores are binary. */
bool vulkan_android_sync_create(VkDevice device,
      const vulkan_android_sync_ops_t *ops, vulkan_android_sync_t *out);
/* Owner only, after every submitted use has completed. No device-idle wait. */
void vulkan_android_sync_destroy(VkDevice device,
      const vulkan_android_sync_ops_t *ops, vulkan_android_sync_t *sync);
/* Owner only, semaphore must have NO pending queue operations. Duplicates the
 * borrowed AImage ready fd and transfers only the duplicate on success. -1
 * imports the already-signaled SYNC_FD payload. Submit a binary GPU wait before
 * touching the image. Ownership transfer requires wait stage ALL_COMMANDS.
 * No CPU fence wait, heap allocation or native image operation. */
bool vulkan_android_sync_import(VkDevice device,
      const vulkan_android_sync_ops_t *ops, VkSemaphore ready, int borrowed_fd);
/* Call after successfully submitting a signal of done, covering FOREIGN release
 * and all image reads. SYNC_FD export consumes that signal payload; caller owns
 * returned fd, including valid -1 for already complete. No pending wait may use
 * done. Retain resources/ticket on failure; never return an unproven image. */
bool vulkan_android_sync_export(VkDevice device,
      const vulkan_android_sync_ops_t *ops, VkSemaphore done, int *owned_fd);
RETRO_END_DECLS
#endif
