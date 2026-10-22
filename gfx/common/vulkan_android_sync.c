/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include "vulkan_android_sync.h"
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
void vulkan_android_sync_destroy(VkDevice device,
      const vulkan_android_sync_ops_t *ops, vulkan_android_sync_t *sync)
{
   if (!sync || !ops || !ops->destroy)
      return;
   if (sync->ready)
      ops->destroy(device, sync->ready, NULL);
   if (sync->done)
      ops->destroy(device, sync->done, NULL);
   memset(sync, 0, sizeof(*sync));
}
bool vulkan_android_sync_create(VkDevice device,
      const vulkan_android_sync_ops_t *ops, vulkan_android_sync_t *out)
{
   VkSemaphoreCreateInfo info;
   VkExportSemaphoreCreateInfo export;
   if (!out)
      return false;
   memset(out, 0, sizeof(*out));
   if (!device || !ops || !ops->create || !ops->destroy || !ops->import_fd
         || !ops->export_fd)
      return false;
   memset(&info, 0, sizeof(info));
   info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
   if (ops->create(device, &info, NULL, &out->ready) != VK_SUCCESS)
      goto fail;
   memset(&export, 0, sizeof(export));
   export.sType = VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO;
   export.handleTypes = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT;
   info.pNext = &export;
   if (ops->create(device, &info, NULL, &out->done) != VK_SUCCESS)
      goto fail;
   return true;
fail:
   vulkan_android_sync_destroy(device, ops, out);
   return false;
}
bool vulkan_android_sync_import(VkDevice device,
      const vulkan_android_sync_ops_t *ops, VkSemaphore ready, int borrowed_fd)
{
   VkImportSemaphoreFdInfoKHR info;
   int fd = -1;
   if (!device || !ops || !ops->import_fd || !ready || borrowed_fd < -1)
      return false;
   if (borrowed_fd >= 0)
   {
      fd = fcntl(borrowed_fd, F_DUPFD_CLOEXEC, 0);
      if (fd < 0)
         return false;
   }
   memset(&info, 0, sizeof(info));
   info.sType = VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_FD_INFO_KHR;
   info.semaphore = ready;
   info.flags = VK_SEMAPHORE_IMPORT_TEMPORARY_BIT;
   info.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT;
   info.fd = fd;
   if (ops->import_fd(device, &info) == VK_SUCCESS)
      return true;
   if (fd >= 0)
      close(fd);
   return false;
}
bool vulkan_android_sync_export(VkDevice device,
      const vulkan_android_sync_ops_t *ops, VkSemaphore done, int *owned_fd)
{
   VkSemaphoreGetFdInfoKHR info;
   int fd = -1;
   if (!owned_fd)
      return false;
   *owned_fd = -1;
   if (!device || !ops || !ops->export_fd || !done)
      return false;
   memset(&info, 0, sizeof(info));
   info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_GET_FD_INFO_KHR;
   info.semaphore = done;
   info.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT;
   if (ops->export_fd(device, &info, &fd) != VK_SUCCESS)
   {
      if (fd >= 0)
         close(fd);
      return false;
   }
   if (fd < -1)
      return false;
   *owned_fd = fd;
   return true;
}
