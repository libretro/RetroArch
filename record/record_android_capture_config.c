/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include "record_android_capture_config.h"
#if defined(__ANDROID__) || defined(RECORD_ANDROID_CAPTURE_CONFIG_TEST)
#include "../gfx/video_driver.h"
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>
#define record_acconfig_close _close
#else
#include <unistd.h>
#define record_acconfig_close close
#endif

struct record_android_capture_config
{
   record_android_capture_pool_config_t pool;
   VkPhysicalDeviceMemoryProperties memory;
   PFN_vkQueueSubmit submit;
};

static VkResult record_acconfig_submit(void *context, VkQueue queue,
      uint32_t count, const VkSubmitInfo *submits, VkFence fence)
{
   record_android_capture_config_t *c
         = (record_android_capture_config_t *)context;
   if (!c || queue != c->pool.queue || count != 1 || !submits)
      return VK_ERROR_INITIALIZATION_FAILED;
   return c->submit(queue, count, submits, fence);
}

static void record_acconfig_close_fd(void *context, int fd)
{
   (void)context;
   if (fd >= 0)
      record_acconfig_close(fd);
}

record_android_capture_config_t *record_android_capture_config_new(
      const struct video_device_info *device, unsigned width, unsigned height,
      unsigned count, unsigned mode, bool flip_y)
{
   record_android_capture_config_t *c;
   PFN_vkGetInstanceProcAddr instance_proc;
   PFN_vkGetDeviceProcAddr device_proc;
   PFN_vkGetPhysicalDeviceMemoryProperties memory;
   unsigned i;
   if (!device || device->api != VIDEO_DEVICE_VULKAN || !device->instance
         || !device->physical_device || !device->device || !device->queue
         || !device->queue_lock || !device->get_instance_proc_addr
         || !(device->recording_caps & VIDEO_DEVICE_CAP_ANDROID_CAPTURE_INTEROP)
         || device->queue_family >= VK_QUEUE_FAMILY_FOREIGN_EXT || !width
         || !height || width > 16384 || height > 16384 || !count
         || count > RECORD_GPU_QUEUE_SLOTS || mode > 2)
      return NULL;
   instance_proc = (PFN_vkGetInstanceProcAddr)device->get_instance_proc_addr;
   device_proc = (PFN_vkGetDeviceProcAddr)instance_proc(
         (VkInstance)device->instance, "vkGetDeviceProcAddr");
   memory = (PFN_vkGetPhysicalDeviceMemoryProperties)instance_proc(
         (VkInstance)device->instance, "vkGetPhysicalDeviceMemoryProperties");
   if (!device_proc || !memory)
      return NULL;
   c = (record_android_capture_config_t *)calloc(1, sizeof(*c));
   if (!c)
      return NULL;
   c->pool.physical = (VkPhysicalDevice)device->physical_device;
   c->pool.device = (VkDevice)device->device;
   c->pool.queue = (VkQueue)device->queue;
   c->pool.family = device->queue_family;
   c->pool.memory = &c->memory;
   c->pool.gpu.width = width;
   c->pool.gpu.height = height;
   c->pool.gpu.count = count;
   c->pool.gpu.mode = mode;
   c->pool.gpu.flip_y = flip_y;
   c->pool.images.query
         = (PFN_vkGetPhysicalDeviceImageFormatProperties2)instance_proc(
               (VkInstance)device->instance,
               "vkGetPhysicalDeviceImageFormatProperties2");
   if (!c->pool.images.query)
      c->pool.images.query
            = (PFN_vkGetPhysicalDeviceImageFormatProperties2)instance_proc(
                  (VkInstance)device->instance,
                  "vkGetPhysicalDeviceImageFormatProperties2KHR");
   if (!c->pool.images.query)
      goto failed;
#define RECORD_ACCONFIG_LOAD(target, type, name)                               \
   do                                                                          \
   {                                                                           \
      target = (type)device_proc(c->pool.device, name);                        \
      if (!target)                                                             \
         goto failed;                                                          \
   } while (0)
   RECORD_ACCONFIG_LOAD(c->pool.images.properties,
         PFN_vkGetAndroidHardwareBufferPropertiesANDROID,
         "vkGetAndroidHardwareBufferPropertiesANDROID");
   RECORD_ACCONFIG_LOAD(
         c->pool.images.create, PFN_vkCreateImage, "vkCreateImage");
   RECORD_ACCONFIG_LOAD(
         c->pool.images.destroy, PFN_vkDestroyImage, "vkDestroyImage");
   RECORD_ACCONFIG_LOAD(
         c->pool.images.allocate, PFN_vkAllocateMemory, "vkAllocateMemory");
   RECORD_ACCONFIG_LOAD(
         c->pool.images.free_memory, PFN_vkFreeMemory, "vkFreeMemory");
   RECORD_ACCONFIG_LOAD(
         c->pool.images.bind, PFN_vkBindImageMemory, "vkBindImageMemory");
   RECORD_ACCONFIG_LOAD(c->pool.submit.create_pool, PFN_vkCreateCommandPool,
         "vkCreateCommandPool");
   RECORD_ACCONFIG_LOAD(c->pool.submit.destroy_pool, PFN_vkDestroyCommandPool,
         "vkDestroyCommandPool");
   RECORD_ACCONFIG_LOAD(c->pool.submit.allocate_commands,
         PFN_vkAllocateCommandBuffers, "vkAllocateCommandBuffers");
   RECORD_ACCONFIG_LOAD(
         c->pool.submit.create_fence, PFN_vkCreateFence, "vkCreateFence");
   RECORD_ACCONFIG_LOAD(
         c->pool.submit.destroy_fence, PFN_vkDestroyFence, "vkDestroyFence");
   RECORD_ACCONFIG_LOAD(c->pool.submit.create_semaphore, PFN_vkCreateSemaphore,
         "vkCreateSemaphore");
   RECORD_ACCONFIG_LOAD(c->pool.submit.destroy_semaphore,
         PFN_vkDestroySemaphore, "vkDestroySemaphore");
   RECORD_ACCONFIG_LOAD(c->pool.submit.reset_command, PFN_vkResetCommandBuffer,
         "vkResetCommandBuffer");
   RECORD_ACCONFIG_LOAD(c->pool.submit.begin, PFN_vkBeginCommandBuffer,
         "vkBeginCommandBuffer");
   RECORD_ACCONFIG_LOAD(
         c->pool.submit.end, PFN_vkEndCommandBuffer, "vkEndCommandBuffer");
   RECORD_ACCONFIG_LOAD(c->pool.submit.barrier, PFN_vkCmdPipelineBarrier,
         "vkCmdPipelineBarrier");
   RECORD_ACCONFIG_LOAD(
         c->pool.submit.copy, PFN_vkCmdCopyImage, "vkCmdCopyImage");
   RECORD_ACCONFIG_LOAD(
         c->pool.submit.reset_fences, PFN_vkResetFences, "vkResetFences");
   RECORD_ACCONFIG_LOAD(
         c->pool.submit.fence_status, PFN_vkGetFenceStatus, "vkGetFenceStatus");
   RECORD_ACCONFIG_LOAD(c->pool.submit.export_fd, PFN_vkGetSemaphoreFdKHR,
         "vkGetSemaphoreFdKHR");
   RECORD_ACCONFIG_LOAD(c->submit, PFN_vkQueueSubmit, "vkQueueSubmit");
#undef RECORD_ACCONFIG_LOAD
   memory(c->pool.physical, &c->memory);
   if (!c->memory.memoryTypeCount
         || c->memory.memoryTypeCount > VK_MAX_MEMORY_TYPES
         || !c->memory.memoryHeapCount
         || c->memory.memoryHeapCount > VK_MAX_MEMORY_HEAPS)
      goto failed;
   for (i = 0; i < c->memory.memoryTypeCount; i++)
      if (c->memory.memoryTypes[i].heapIndex >= c->memory.memoryHeapCount)
         goto failed;
   c->pool.submit.submit = record_acconfig_submit;
   c->pool.submit.close_fd = record_acconfig_close_fd;
   c->pool.submit.context = c;
   return c;
failed:
   free(c);
   return NULL;
}

const record_android_capture_pool_config_t *record_android_capture_config_pool(
      const record_android_capture_config_t *c)
{
   return c ? &c->pool : NULL;
}

void record_android_capture_config_free(record_android_capture_config_t *c)
{
   free(c);
}
#else
record_android_capture_config_t *record_android_capture_config_new(
      const struct video_device_info *device, unsigned width, unsigned height,
      unsigned count, unsigned mode, bool flip_y)
{
   (void)device;
   (void)width;
   (void)height;
   (void)count;
   (void)mode;
   (void)flip_y;
   return NULL;
}
const record_android_capture_pool_config_t *record_android_capture_config_pool(
      const record_android_capture_config_t *c)
{
   (void)c;
   return NULL;
}
void record_android_capture_config_free(record_android_capture_config_t *c)
{
   (void)c;
}
#endif
