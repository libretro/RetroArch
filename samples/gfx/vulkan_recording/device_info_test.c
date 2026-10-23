/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <retro_atomic.h>
#include "gfx/video_driver.h"
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan_core.h>
#ifdef TEST_ANDROID
#define ANDROID 1
#endif
/* ENUMS */
struct context
{
   VkInstance instance;
   VkPhysicalDevice gpu;
   VkDevice device;
   VkQueue queue;
   void *queue_lock;
   struct
   {
      unsigned major_id, minor_id;
      bool valid;
   } drm_identity;
   unsigned graphics_queue_index, flags;
};
typedef struct vk
{
   struct context *context;
   uint32_t device_support;
   retro_atomic_int_t texture_format_support;
   struct
   {
      VkPipeline yuv420_to_rgba8888;
   } pipelines;
} vk_t;
static void VKAPI_CALL dummy(void)
{
}
static PFN_vkGetInstanceProcAddr vulkan_symbol_wrapper_instance_proc_addr(void)
{
   return (PFN_vkGetInstanceProcAddr)dummy;
}
/* FUNCTIONS */
static struct context context;
static vk_t vk;
static unsigned expected_cap;
static void setup(unsigned flags)
{
   memset(&context, 0, sizeof(context));
   memset(&vk, 0, sizeof(vk));
   context.instance = (VkInstance)(uintptr_t)10;
   context.gpu = (VkPhysicalDevice)(uintptr_t)1;
   context.device = (VkDevice)(uintptr_t)2;
   context.queue = (VkQueue)(uintptr_t)3;
   context.queue_lock = (void *)(uintptr_t)4;
   context.graphics_queue_index = 5;
   context.drm_identity.major_id = 226;
   context.drm_identity.minor_id = 128;
   context.drm_identity.valid = true;
   context.flags = flags;
   vk.context = &context;
   retro_atomic_int_init(
         &vk.texture_format_support, VULKAN_TEXTURE_SUPPORT_YUV420);
   vulkan_note_device(&vk);
   expected_cap = 0;
#if defined(TEST_ANDROID) && defined(HAVE_THREADS)
   if (flags & VK_CTX_FLAG_ANDROID_IMAGES)
      expected_cap = VIDEO_DEVICE_CAP_ANDROID_CAPTURE_INTEROP;
#endif
}
static void check(void)
{
   video_device_info_t out;
   assert(vulkan_get_device(&vk, &out));
   assert(out.api == VIDEO_DEVICE_VULKAN
         && out.instance == (void *)context.instance);
   assert(out.physical_device == (void *)context.gpu
         && out.device == (void *)context.device);
   assert(out.queue == (void *)context.queue
         && out.queue_lock == context.queue_lock);
   assert(out.queue_family == 5 && out.drm_render_major == 226
         && out.drm_render_minor == 128);
   assert(out.drm_render_valid && out.get_instance_proc_addr);
   assert(out.recording_caps == expected_cap);
   assert(!out.import_formats
         || out.import_formats == (IMAGE_PIXFMT_NV12 | IMAGE_PIXFMT_P010));
}
#ifdef DEVICE_INFO_THREADS
#include <pthread.h>
static retro_atomic_int_t started;
static void *writer(void *unused)
{
   unsigned i;
   (void)unused;
   while (!retro_atomic_load_acquire_int(&started))
   {
   }
   for (i = 0; i < 50000; i++)
   {
      context.flags = VK_CTX_FLAG_TIMELINE_SEMAPHORE
            | VK_CTX_FLAG_ANDROID_IMAGES
            | ((i & 1) ? VK_CTX_FLAG_HDR_ENABLE : 0);
      vk.pipelines.yuv420_to_rgba8888
            = (i & 1) ? (VkPipeline)(uintptr_t)77 : VK_NULL_HANDLE;
      retro_atomic_store_release_int(&vk.texture_format_support,
            (i & 1) ? VULKAN_TEXTURE_SUPPORT_YUV420 : 0);
   }
   return NULL;
}
static void concurrent(void)
{
   pthread_t thread;
   unsigned i;
   setup(VK_CTX_FLAG_TIMELINE_SEMAPHORE | VK_CTX_FLAG_ANDROID_IMAGES);
   retro_atomic_int_init(&started, 0);
   assert(!pthread_create(&thread, NULL, writer, NULL));
   retro_atomic_store_release_int(&started, 1);
   for (i = 0; i < 50000; i++)
      check();
   assert(!pthread_join(thread, NULL));
}
#endif
int main(void)
{
   unsigned flags;
   video_device_info_t out;
   for (flags = 0; flags < 4; flags++)
   {
      setup(((flags & 1) ? VK_CTX_FLAG_TIMELINE_SEMAPHORE : 0)
            | ((flags & 2) ? VK_CTX_FLAG_ANDROID_IMAGES : 0));
      check();
      context.flags = 0;
      vk.pipelines.yuv420_to_rgba8888 = VK_NULL_HANDLE;
      check();
      assert(vulkan_get_device(&vk, &out));
      assert(out.import_formats
            == ((flags & 1) ? (IMAGE_PIXFMT_NV12 | IMAGE_PIXFMT_P010) : 0));
      retro_atomic_store_release_int(&vk.texture_format_support, 0);
      assert(vulkan_get_device(&vk, &out) && !out.import_formats);
   }
   assert(!vulkan_get_device(NULL, &out));
   assert(!vulkan_get_device(&vk, NULL));
   context.device = VK_NULL_HANDLE;
   assert(!vulkan_get_device(&vk, &out));
   vk.context = NULL;
   assert(!vulkan_get_device(&vk, &out));
#ifdef DEVICE_INFO_THREADS
   concurrent();
#endif
   puts("PASS shipping Vulkan enabled-device snapshot and atomic import "
        "publication");
   return 0;
}
