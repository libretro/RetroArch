/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>
#ifdef _WIN32
#define VK_NO_PROTOTYPES
#define VK_USE_PLATFORM_WIN32_KHR
#include <windows.h>
#include <assert.h>
#include <string.h>
#include "../../../gfx/include/vulkan/vulkan.h"
#include "../../../gfx/video_driver.h"

static unsigned step, fail_step, images, memories, views, semaphores, serial;
static bool memory_mismatch;
static bool fails(void) { return ++step == fail_step; }
#define HANDLE_VALUE(type) ((type)(uintptr_t)++serial)

static VkResult vkCreateImage(VkDevice d, const VkImageCreateInfo *i,
      const VkAllocationCallbacks *a, VkImage *out)
{
   (void)d; (void)a;
   assert(i->format == VK_FORMAT_R16G16B16A16_SFLOAT);
   if (fails()) return VK_ERROR_OUT_OF_DEVICE_MEMORY;
   *out = HANDLE_VALUE(VkImage); images++; return VK_SUCCESS;
}
static void vkDestroyImage(VkDevice d, VkImage i, const VkAllocationCallbacks *a)
{ (void)d; (void)a; if (i) { assert(images); images--; } }
static void vkGetImageMemoryRequirements(VkDevice d, VkImage i, VkMemoryRequirements *r)
{ (void)d; (void)i; memset(r, 0, sizeof(*r)); r->size = 4096; r->memoryTypeBits = 1; }
static VkResult VKAPI_CALL get_handle_properties(VkDevice d, VkExternalMemoryHandleTypeFlagBits t,
      HANDLE h, VkMemoryWin32HandlePropertiesKHR *p)
{
   (void)d; (void)t; (void)h;
   if (fails()) return VK_ERROR_INVALID_EXTERNAL_HANDLE;
   p->memoryTypeBits = memory_mismatch ? 2 : 1; return VK_SUCCESS;
}
static VkResult vkAllocateMemory(VkDevice d, const VkMemoryAllocateInfo *i,
      const VkAllocationCallbacks *a, VkDeviceMemory *out)
{
   const VkImportMemoryWin32HandleInfoKHR *imported = (const VkImportMemoryWin32HandleInfoKHR*)i->pNext;
   const VkMemoryDedicatedAllocateInfo *dedicated = (const VkMemoryDedicatedAllocateInfo*)imported->pNext;
   (void)d; (void)a; assert(dedicated->image && imported->handle && i->memoryTypeIndex == 0);
   if (fails()) return VK_ERROR_OUT_OF_DEVICE_MEMORY;
   *out = HANDLE_VALUE(VkDeviceMemory); memories++; return VK_SUCCESS;
}
static void vkFreeMemory(VkDevice d, VkDeviceMemory m, const VkAllocationCallbacks *a)
{ (void)d; (void)a; if (m) { assert(memories); memories--; } }
static VkResult vkBindImageMemory(VkDevice d, VkImage i, VkDeviceMemory m, VkDeviceSize o)
{ (void)d; (void)i; (void)m; assert(!o); return fails() ? VK_ERROR_OUT_OF_DEVICE_MEMORY : VK_SUCCESS; }
static VkResult vkCreateImageView(VkDevice d, const VkImageViewCreateInfo *i,
      const VkAllocationCallbacks *a, VkImageView *out)
{
   (void)d; (void)i; (void)a;
   if (fails()) return VK_ERROR_OUT_OF_DEVICE_MEMORY;
   *out = HANDLE_VALUE(VkImageView); views++; return VK_SUCCESS;
}
static void vkDestroyImageView(VkDevice d, VkImageView v, const VkAllocationCallbacks *a)
{ (void)d; (void)a; if (v) { assert(views); views--; } }
static VkResult vkCreateSemaphore(VkDevice d, const VkSemaphoreCreateInfo *i,
      const VkAllocationCallbacks *a, VkSemaphore *out)
{
   const VkSemaphoreTypeCreateInfo *type = (const VkSemaphoreTypeCreateInfo*)i->pNext;
   (void)d; (void)a; assert(type->semaphoreType == VK_SEMAPHORE_TYPE_TIMELINE);
   if (fails()) return VK_ERROR_OUT_OF_DEVICE_MEMORY;
   *out = HANDLE_VALUE(VkSemaphore); semaphores++; return VK_SUCCESS;
}
static void vkDestroySemaphore(VkDevice d, VkSemaphore s, const VkAllocationCallbacks *a)
{ (void)d; (void)a; if (s) { assert(semaphores); semaphores--; } }
static VkResult VKAPI_CALL import_semaphore(VkDevice d, const VkImportSemaphoreWin32HandleInfoKHR *i)
{
   (void)d; assert(i->handleType == VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_D3D12_FENCE_BIT && i->handle);
   return fails() ? VK_ERROR_INVALID_EXTERNAL_HANDLE : VK_SUCCESS;
}
static PFN_vkVoidFunction VKAPI_CALL device_proc(VkDevice d, const char *name)
{
   (void)d;
   if (!strcmp(name, "vkGetMemoryWin32HandlePropertiesKHR")) return (PFN_vkVoidFunction)get_handle_properties;
   if (!strcmp(name, "vkImportSemaphoreWin32HandleKHR")) return (PFN_vkVoidFunction)import_semaphore;
   return NULL;
}
static PFN_vkVoidFunction VKAPI_CALL instance_proc(VkInstance i, const char *name)
{ (void)i; assert(!strcmp(name, "vkGetDeviceProcAddr")); return (PFN_vkVoidFunction)device_proc; }
static PFN_vkGetInstanceProcAddr vulkan_symbol_wrapper_instance_proc_addr(void)
{ return instance_proc; }
typedef struct test_context
{
   VkDevice device;
   VkInstance instance;
   VkPhysicalDeviceMemoryProperties memory_properties;
} test_context_t;
typedef struct test_vk { test_context_t *context; } vk_t;
#include "../../../gfx/drivers/vulkan_win32_video.h"

int main(void)
{
   test_context_t context;
   vk_t vk;
   vk_win32_video_import_t imported, zero;
   video_frame_import_t frame, translated;
   unsigned failure;
   memset(&context, 0, sizeof(context));
   memset(&zero, 0, sizeof(zero));
   memset(&frame, 0, sizeof(frame));
   vk.context = &context; context.memory_properties.memoryTypeCount = 1;
   frame.image = 100; frame.wait = 200; frame.done = 201;
   frame.wait_value = 17; frame.done_value = 23; frame.layout = 9;
   frame.dims = VIDEO_SCALE_PACK(64, 32); frame.pixfmt = IMAGE_PIXFMT_YUV10_FP16;
   frame.color.transfer = 18; frame.color.max_cll = 1000;
   for (failure = 1; failure <= 9; failure++)
   {
      imported = zero; step = 0; fail_step = failure;
      assert(!vulkan_win32_video_get(&vk, &imported, &frame, &translated));
      assert(!images && !memories && !views && !semaphores);
      assert(!memcmp(&imported, &zero, sizeof(zero)));
   }
   fail_step = 0; step = 0; memory_mismatch = true;
   assert(!vulkan_win32_video_get(&vk, &imported, &frame, &translated));
   assert(!images && !memories && !views && !semaphores);
   memory_mismatch = false; step = 0;
   assert(vulkan_win32_video_get(&vk, &imported, &frame, &translated) && step == 9);
   assert(images == 1 && memories == 1 && views == 1 && semaphores == 2);
   assert(translated.layout == VK_IMAGE_LAYOUT_GENERAL && translated.plane_views[0]
         && translated.plane_views[0] == translated.plane_views[1]
         && translated.wait_value == 17 && translated.done_value == 23
         && translated.pixfmt == frame.pixfmt && translated.color.max_cll == 1000
         && translated.color.transfer == 18);
   assert(vulkan_win32_video_get(&vk, &imported, &frame, &translated) && step == 9);
   frame.layout++;
   assert(vulkan_win32_video_get(&vk, &imported, &frame, &translated) && step == 18);
   frame.crop_x = 2;
   assert(!vulkan_win32_video_get(&vk, &imported, &frame, &translated) && step == 18);
   frame.crop_x = 0; frame.wait = 0;
   assert(!vulkan_win32_video_get(&vk, &imported, &frame, &translated) && step == 18);
   vulkan_win32_video_free(context.device, &imported);
   assert(!images && !memories && !views && !semaphores);
   puts("Win32 Vulkan import: allocation failures, cache generation and metadata PASS");
   return 0;
}
#else
int main(void) { puts("SKIP Win32 import API: run the mingw-w64 executable with Wine"); return 0; }
#endif
