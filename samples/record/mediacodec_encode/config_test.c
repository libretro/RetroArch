/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#else
#include <unistd.h>
#endif
#include "../../../gfx/video_driver.h"
#include "../../../record/record_android_capture_config.h"

static unsigned live, procedures, memories, submissions, native_operations;
static unsigned bad_memory;
static bool fail_allocation, core_query_missing, locked, native_allowed = true;
static const char *missing;
static const VkSubmitInfo *expected_submit;
static VkResult native_result;
static const char *const names[]
      = {"vkGetAndroidHardwareBufferPropertiesANDROID", "vkCreateImage",
            "vkDestroyImage", "vkAllocateMemory", "vkFreeMemory",
            "vkBindImageMemory", "vkCreateCommandPool", "vkDestroyCommandPool",
            "vkAllocateCommandBuffers", "vkCreateFence", "vkDestroyFence",
            "vkCreateSemaphore", "vkDestroySemaphore", "vkResetCommandBuffer",
            "vkBeginCommandBuffer", "vkEndCommandBuffer",
            "vkCmdPipelineBarrier", "vkCmdCopyImage", "vkResetFences",
            "vkGetFenceStatus", "vkGetSemaphoreFdKHR", "vkQueueSubmit"};
static void *config_calloc(size_t count, size_t size)
{
   void *p;
   if (fail_allocation)
      return NULL;
   p = calloc(count, size);
   if (p)
      live++;
   return p;
}
static void config_free(void *p)
{
   if (p)
   {
      assert(live);
      live--;
   }
   free(p);
}
#define calloc config_calloc
#define free config_free
#include "../../../record/record_android_capture_config.c"
#undef calloc
#undef free

static void VKAPI_CALL resource(void)
{
   native_operations++;
   assert(!"configuration must not allocate native resources");
}
static VkResult VKAPI_CALL queue_submit(
      VkQueue queue, uint32_t count, const VkSubmitInfo *submits, VkFence fence)
{
   assert(native_allowed && locked && queue == (VkQueue)(uintptr_t)3);
   assert(count == 1 && submits == expected_submit
         && fence == (VkFence)(uintptr_t)77);
   submissions++;
   return native_result;
}
static void VKAPI_CALL memory(
      VkPhysicalDevice physical, VkPhysicalDeviceMemoryProperties *out)
{
   assert(native_allowed && physical == (VkPhysicalDevice)(uintptr_t)1);
   memories++;
   memset(out, 0, sizeof(*out));
   out->memoryTypeCount = 1;
   out->memoryHeapCount = 1;
   out->memoryTypes[0].propertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
   out->memoryHeaps[0].size = 1048576;
   if (bad_memory == 1)
      out->memoryTypeCount = 0;
   if (bad_memory == 2)
      out->memoryTypeCount = VK_MAX_MEMORY_TYPES + 1;
   if (bad_memory == 3)
      out->memoryHeapCount = 0;
   if (bad_memory == 4)
      out->memoryHeapCount = VK_MAX_MEMORY_HEAPS + 1;
   if (bad_memory == 5)
      out->memoryTypes[0].heapIndex = 1;
}
static PFN_vkVoidFunction VKAPI_CALL device_proc(
      VkDevice device, const char *name)
{
   unsigned i;
   assert(native_allowed && device == (VkDevice)(uintptr_t)2);
   procedures++;
   for (i = 0; i < sizeof(names) / sizeof(names[0]); i++)
      if (!strcmp(names[i], name))
      {
         if (missing && !strcmp(missing, name))
            return NULL;
         return !strcmp(name, "vkQueueSubmit")
               ? (PFN_vkVoidFunction)queue_submit
               : resource;
      }
   assert(!"unexpected device procedure");
   return NULL;
}
static PFN_vkVoidFunction VKAPI_CALL instance_proc(
      VkInstance instance, const char *name)
{
   assert(native_allowed && instance == (VkInstance)(uintptr_t)10);
   procedures++;
   if (missing && !strcmp(missing, name))
      return NULL;
   if (!strcmp(name, "vkGetDeviceProcAddr"))
      return (PFN_vkVoidFunction)device_proc;
   if (!strcmp(name, "vkGetPhysicalDeviceMemoryProperties"))
      return (PFN_vkVoidFunction)memory;
   if (!strcmp(name, "vkGetPhysicalDeviceImageFormatProperties2"))
      return core_query_missing ? NULL : resource;
   assert(!strcmp(name, "vkGetPhysicalDeviceImageFormatProperties2KHR"));
   return missing
               && !strcmp(missing, "vkGetPhysicalDeviceImageFormatProperties2")
         ? NULL
         : resource;
}
static video_device_info_t info(void)
{
   video_device_info_t d;
   memset(&d, 0, sizeof(d));
   d.api = VIDEO_DEVICE_VULKAN;
   d.instance = (void *)(uintptr_t)10;
   d.physical_device = (void *)(uintptr_t)1;
   d.device = (void *)(uintptr_t)2;
   d.queue = (void *)(uintptr_t)3;
   d.queue_lock = (void *)(uintptr_t)4;
   d.queue_family = 5;
   d.recording_caps = VIDEO_DEVICE_CAP_ANDROID_CAPTURE_INTEROP;
   d.get_instance_proc_addr = (retro_proc_address_t)instance_proc;
   return d;
}
static void reset(void)
{
   assert(!live);
   procedures = memories = submissions = native_operations = bad_memory = 0;
   missing = NULL;
   fail_allocation = core_query_missing = locked = false;
   native_allowed = true;
   native_result = VK_SUCCESS;
}
static void normal(unsigned mode, bool fallback)
{
   video_device_info_t d = info();
   record_android_capture_config_t *c;
   const record_android_capture_pool_config_t *p;
   VkSubmitInfo submit;
   int descriptors[2];
   reset();
   core_query_missing = fallback;
   c = record_android_capture_config_new(&d, 128, 96, 4, mode, fallback);
   assert(c && live == 1 && memories == 1 && !native_operations);
   p = record_android_capture_config_pool(c);
   assert(p && p->device == (VkDevice)d.device
         && p->physical == (VkPhysicalDevice)d.physical_device);
   assert(p->queue == (VkQueue)d.queue && p->family == d.queue_family);
   assert(p->memory == &c->memory && p->memory->memoryTypeCount == 1);
   assert(p->gpu.width == 128 && p->gpu.height == 96 && p->gpu.count == 4);
   assert(p->gpu.mode == mode && p->gpu.flip_y == fallback);
   assert(p->submit.context == c && p->submit.submit && p->submit.close_fd);
   memset(&submit, 0, sizeof(submit));
   submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
   expected_submit = &submit;
   locked = true;
   assert(p->submit.submit(c, p->queue, 1, &submit, (VkFence)(uintptr_t)77)
         == VK_SUCCESS);
   native_result = VK_ERROR_DEVICE_LOST;
   assert(p->submit.submit(c, p->queue, 1, &submit, (VkFence)(uintptr_t)77)
         == VK_ERROR_DEVICE_LOST);
   assert(p->submit.submit(
                c, (VkQueue)(uintptr_t)999, 1, &submit, VK_NULL_HANDLE)
         == VK_ERROR_INITIALIZATION_FAILED);
   assert(p->submit.submit(c, p->queue, 0, &submit, VK_NULL_HANDLE)
         == VK_ERROR_INITIALIZATION_FAILED);
   assert(p->submit.submit(c, p->queue, 1, NULL, VK_NULL_HANDLE)
         == VK_ERROR_INITIALIZATION_FAILED);
   assert(p->submit.submit(NULL, p->queue, 1, &submit, VK_NULL_HANDLE)
         == VK_ERROR_INITIALIZATION_FAILED);
   assert(submissions == 2);
   locked = false;
   errno = EDOM;
   p->submit.close_fd(c, -1);
   assert(errno == EDOM);
#ifdef _WIN32
   assert(_pipe(descriptors, 256, _O_BINARY) == 0);
   p->submit.close_fd(c, descriptors[0]);
   assert(_close(descriptors[0]) == -1 && errno == EBADF);
   assert(_close(descriptors[1]) == 0);
#else
   assert(pipe(descriptors) == 0);
   p->submit.close_fd(c, descriptors[0]);
   assert(close(descriptors[0]) == -1 && errno == EBADF);
   assert(close(descriptors[1]) == 0);
#endif
   native_allowed = false;
   record_android_capture_config_free(c);
   assert(!live && memories == 1 && !native_operations);
}
static void failures(void)
{
   video_device_info_t d;
   unsigned i;
   const char *const instance_names[]
         = {"vkGetDeviceProcAddr", "vkGetPhysicalDeviceMemoryProperties",
               "vkGetPhysicalDeviceImageFormatProperties2"};
   for (i = 0; i < sizeof(names) / sizeof(names[0]); i++)
   {
      reset();
      d = info();
      missing = names[i];
      assert(!record_android_capture_config_new(&d, 128, 96, 4, 0, false));
      assert(!live && !memories && !native_operations);
   }
   for (i = 0; i < sizeof(instance_names) / sizeof(instance_names[0]); i++)
   {
      reset();
      d = info();
      missing = instance_names[i];
      assert(!record_android_capture_config_new(&d, 128, 96, 4, 0, false));
      assert(!live && !memories && !native_operations);
   }
   for (i = 1; i <= 5; i++)
   {
      reset();
      d = info();
      bad_memory = i;
      assert(!record_android_capture_config_new(&d, 128, 96, 4, 0, false));
      assert(!live && memories == 1 && !native_operations);
   }
   reset();
   d = info();
   fail_allocation = true;
   assert(!record_android_capture_config_new(&d, 128, 96, 4, 0, false));
   assert(!live && !memories);
}
static void bad_inputs(void)
{
   unsigned i;
   for (i = 0; i < 15; i++)
   {
      video_device_info_t d = info();
      unsigned width = 128, height = 96, count = 4, mode = 0;
      reset();
      if (i == 0)
         d.api = VIDEO_DEVICE_METAL;
      if (i == 1)
         d.instance = NULL;
      if (i == 2)
         d.physical_device = NULL;
      if (i == 3)
         d.device = NULL;
      if (i == 4)
         d.queue = NULL;
      if (i == 5)
         d.queue_lock = NULL;
      if (i == 6)
         d.recording_caps = 0;
      if (i == 7)
         d.get_instance_proc_addr = NULL;
      if (i == 8)
         d.queue_family = VK_QUEUE_FAMILY_IGNORED;
      if (i == 9)
         width = 0;
      if (i == 10)
         height = 0;
      if (i == 11)
         width = 16385;
      if (i == 12)
         count = 0;
      if (i == 13)
         count = RECORD_GPU_QUEUE_SLOTS + 1;
      if (i == 14)
         mode = 3;
      assert(!record_android_capture_config_new(
            &d, width, height, count, mode, false));
      assert(!live && !procedures && !memories);
   }
}
int main(void)
{
   unsigned mode;
   for (mode = 0; mode < 3; mode++)
   {
      normal(mode, false);
      normal(mode, true);
   }
   failures();
   bad_inputs();
   assert(!record_android_capture_config_new(NULL, 1, 1, 1, 0, false));
   assert(!record_android_capture_config_pool(NULL));
   record_android_capture_config_free(NULL);
   puts("PASS enabled Android capture admission, complete Vulkan resolver, "
        "memory snapshot, "
        "serialized submit adapter and fd ownership; native device mocked");
   return 0;
}
