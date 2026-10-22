/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include "../../../gfx/common/vulkan_android_sync.h"
#define HANDLE(t, n) ((t)(uintptr_t)(n))
static int calls, fail, live, seen, import_fail, export_fail, export_result;
static VKAPI_ATTR VkResult VKAPI_CALL create(VkDevice d,
      const VkSemaphoreCreateInfo *info, const VkAllocationCallbacks *a,
      VkSemaphore *out)
{
   const VkExportSemaphoreCreateInfo *e
         = (const VkExportSemaphoreCreateInfo *)info->pNext;
   assert(d && !a && info->sType == VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO);
   calls++;
   if (calls == fail)
      return VK_ERROR_OUT_OF_DEVICE_MEMORY;
   if (calls == 2)
      assert(e
            && e->handleTypes == VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT);
   else
      assert(!e);
   *out = HANDLE(VkSemaphore, calls);
   live++;
   return VK_SUCCESS;
}
static VKAPI_ATTR void VKAPI_CALL destroy(
      VkDevice d, VkSemaphore s, const VkAllocationCallbacks *a)
{
   assert(d && s && !a && live);
   live--;
}
static VKAPI_ATTR VkResult VKAPI_CALL import_fd(
      VkDevice d, const VkImportSemaphoreFdInfoKHR *info)
{
   assert(d && info->semaphore
         && info->flags == VK_SEMAPHORE_IMPORT_TEMPORARY_BIT);
   assert(info->handleType == VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT);
   seen = info->fd;
   if (seen >= 0)
      assert(fcntl(seen, F_GETFD) & FD_CLOEXEC);
   if (import_fail)
      return VK_ERROR_INVALID_EXTERNAL_HANDLE;
   if (seen >= 0)
      close(seen); /* driver owns the successful import */
   return VK_SUCCESS;
}
static VKAPI_ATTR VkResult VKAPI_CALL export_fd(
      VkDevice d, const VkSemaphoreGetFdInfoKHR *info, int *fd)
{
   assert(d && info->semaphore
         && info->handleType == VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT);
   *fd = export_result;
   return export_fail ? VK_ERROR_DEVICE_LOST : VK_SUCCESS;
}
int main(void)
{
   vulkan_android_sync_ops_t ops;
   vulkan_android_sync_t sync;
   VkDevice device = HANDLE(VkDevice, 1);
   int i, p[2], fd;
   memset(&ops, 0, sizeof(ops));
   ops.create = create;
   ops.destroy = destroy;
   ops.import_fd = import_fd;
   ops.export_fd = export_fd;
   for (i = 1; i <= 2; i++)
   {
      fail = i;
      calls = 0;
      assert(!vulkan_android_sync_create(device, &ops, &sync));
      assert(!live && !sync.ready && !sync.done);
   }
   fail = 0;
   calls = 0;
   assert(vulkan_android_sync_create(device, &ops, &sync));
   assert(live == 2);
   assert(pipe(p) == 0);
   import_fail = 1;
   assert(!vulkan_android_sync_import(device, &ops, sync.ready, p[0]));
   assert(seen != p[0] && fcntl(seen, F_GETFD) == -1
         && fcntl(p[0], F_GETFD) >= 0);
   import_fail = 0;
   assert(vulkan_android_sync_import(device, &ops, sync.ready, p[0]));
   assert(fcntl(p[0], F_GETFD) >= 0 && fcntl(seen, F_GETFD) == -1);
   assert(vulkan_android_sync_import(device, &ops, sync.ready, -1)
         && seen == -1);
   assert(!vulkan_android_sync_import(device, &ops, sync.ready, -2));
   close(p[0]);
   close(p[1]);
   assert(!vulkan_android_sync_import(device, &ops, sync.ready, p[0]));
   assert(pipe(p) == 0);
   export_result = p[0];
   assert(vulkan_android_sync_export(device, &ops, sync.done, &fd)
         && fd == p[0]);
   close(fd);
   close(p[1]);
   export_result = -1;
   assert(vulkan_android_sync_export(device, &ops, sync.done, &fd) && fd == -1);
   assert(pipe(p) == 0);
   export_result = p[0];
   export_fail = 1;
   assert(!vulkan_android_sync_export(device, &ops, sync.done, &fd) && fd == -1
         && fcntl(p[0], F_GETFD) == -1);
   close(p[1]);
   vulkan_android_sync_destroy(device, &ops, &sync);
   assert(!live);
   puts("PASS: Vulkan binary SYNC_FD startup, borrowed-fd duplication, failure "
        "closure and completion export");
   return 0;
}
