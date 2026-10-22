/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#define VULKAN_ENTRY_TEST_MAIN entry_fault_suite
#include "vulkan_entry_test.c"
#include "../../../gfx/common/vulkan_android_cache.h"
static int owned_pool_destroyed, destinations_destroyed;
static void destroy_destination(VkDevice device, void *destination)
{
   assert(device && destination == &native && !entry_live);
   destinations_destroyed++;
}
static VKAPI_ATTR void VKAPI_CALL destroy_owned_pool(VkDevice device,
      VkCommandPool pool, const VkAllocationCallbacks *allocator)
{
   assert(device && pool && !allocator && !entry_live);
   owned_pool_destroyed++;
}
int main(void)
{
   android_image_ops_t image_ops;
   android_image_queue_t *q;
   android_frame_pool_t *pool;
   android_frame_view_t view;
   video_hw_frame_t frame;
   vulkan_android_cache_t *cache;
   int count, other_buffer;
   bool prepare_needed;
   assert(!entry_fault_suite());
   submit_fail = export_fail = end_fail = 0;
   entry_fail = entry_calls = 0;
   fence_status = VK_SUCCESS;
   memset(&image_ops, 0, sizeof(image_ops));
   image_ops.acquire = acquire;
   image_ops.describe = describe;
   image_ops.delete_image = del_image;
   image_ops.delete_reader = del_reader;
   q = android_image_queue_new(&native, 1, &image_ops);
   pool = android_frame_pool_new(1);
   assert(q && pool);
   memset(&view, 0, sizeof(view));
   view.format.serial = 1;
   view.pixfmt = IMAGE_PIXFMT_NV12;
   view.color.primaries = view.color.matrix = view.color.range = 1;
   view.color.transfer = 13;
   entry_fixture_config.destroy_owned_pool = destroy_owned_pool;
   cache = vulkan_android_cache_new(&entry_fixture_config, 1, false);
   assert(cache && !entry_live);
   assert(android_image_queue_acquire(q, &view.image) == 1);
   assert(!android_frame_export(pool, &view, &frame));
   count = entry_calls;
   assert(!vulkan_android_cache_present(cache, &frame, H(VkImage, 9),
         H(VkImageView, 9), VK_IMAGE_LAYOUT_UNDEFINED, &prepare_needed));
   assert(prepare_needed && entry_calls == count && !entry_live);
   assert(vulkan_android_cache_prepare(cache, &frame) == 1 && entry_live);
   count = entry_calls;
   assert(vulkan_android_cache_prepare(cache, &frame) == 1
         && entry_calls == count);
   assert(vulkan_android_cache_present(cache, &frame, H(VkImage, 9),
                H(VkImageView, 9), VK_IMAGE_LAYOUT_UNDEFINED, &prepare_needed)
         == 1);
   assert(!prepare_needed && entry_calls == count);
   assert(android_image_queue_poll(q) == 1);
   assert(android_image_queue_acquire(q, &view.image) == 1);
   view.image.buffer = &other_buffer;
   assert(!android_frame_export(pool, &view, &frame));
   assert(vulkan_android_cache_prepare(cache, &frame) == 0
         && entry_calls == count);
   fence_status = VK_SUCCESS;
   assert(vulkan_android_cache_prepare(cache, &frame) == 1
         && entry_calls == count + 17);
   count = entry_calls;
   assert(vulkan_android_cache_present(cache, &frame, H(VkImage, 9),
                H(VkImageView, 9), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                &prepare_needed)
         == 1);
   assert(entry_calls == count);
   vulkan_android_cache_close(cache);
   assert(!vulkan_android_cache_free(cache) && entry_live);
   fence_status = VK_SUCCESS;
   assert(vulkan_android_cache_free(cache) && !entry_live);
   assert(android_image_queue_poll(q) == 1 && owned_pool_destroyed == 1);
   entry_fixture_config.destroy_deferred_destination = destroy_destination;
   entry_fixture_config.deferred_destination = &native;
   cache = vulkan_android_cache_new(&entry_fixture_config, 1, false);
   assert(cache);
   assert(android_image_queue_acquire(q, &view.image) == 1);
   assert(!android_frame_export(pool, &view, &frame));
   assert(vulkan_android_cache_prepare(cache, &frame) == 1);
   assert(vulkan_android_cache_present(cache, &frame, H(VkImage, 9),
                H(VkImageView, 9), VK_IMAGE_LAYOUT_UNDEFINED, NULL) == 1);
   vulkan_android_cache_close(cache);
   assert(!vulkan_android_cache_free(cache));
   vulkan_android_cache_defer(cache);
   vulkan_android_cache_device_collect(H(VkDevice, 99));
   vulkan_android_cache_device_collect(entry_fixture_config.device);
   assert(entry_live && !destinations_destroyed && owned_pool_destroyed == 1);
   fence_status = VK_SUCCESS;
   vulkan_android_cache_device_collect(entry_fixture_config.device);
   assert(!entry_live && destinations_destroyed == 1 && owned_pool_destroyed == 2);
   assert(!vulkan_android_cache_device_pending(entry_fixture_config.device));
   vulkan_android_cache_device_collect(entry_fixture_config.device);
   assert(destinations_destroyed == 1 && android_image_queue_poll(q) == 1);
   /* A failed queue submission may have touched GPU input. Closing must keep
    * the lease until the renderer proves that its device has been destroyed. */
   cache = vulkan_android_cache_new(&entry_fixture_config, 1, false);
   assert(cache);
   assert(android_image_queue_acquire(q, &view.image) == 1);
   assert(!android_frame_export(pool, &view, &frame));
   assert(vulkan_android_cache_prepare(cache, &frame) == 1 && entry_live);
   count = entry_calls;
   submit_fail = 1;
   assert(vulkan_android_cache_present(cache, &frame, H(VkImage, 9),
                H(VkImageView, 9), VK_IMAGE_LAYOUT_UNDEFINED, &prepare_needed)
         == -2);
   video_hw_frame_abandon(&frame);
   vulkan_android_cache_close(cache);
   assert(!vulkan_android_cache_free(cache) && !android_image_queue_poll(q));
   /* Before device destruction, destroy native children but keep the external
    * image lease. Wrong-device and repeated notifications remain harmless. */
   assert(!vulkan_android_cache_after_device_destroy(cache));
   vulkan_android_cache_defer(cache);
   assert(!vulkan_android_cache_free(cache));
   vulkan_android_cache_defer(cache); /* Idempotent ownership transfer. */
   assert(vulkan_android_cache_device_pending(entry_fixture_config.device));
   vulkan_android_cache_device_destroyed(H(VkDevice, 99));
   assert(vulkan_android_cache_device_pending(entry_fixture_config.device));
   assert(!android_image_queue_poll(q));
   vulkan_android_cache_device_destroyed(entry_fixture_config.device);
   assert(vulkan_android_cache_device_pending(entry_fixture_config.device));
   vulkan_android_cache_device_collect(entry_fixture_config.device);
   assert(entry_live && destinations_destroyed == 1);
   vulkan_android_cache_device_destroying(H(VkDevice, 99));
   assert(entry_live && !android_image_queue_poll(q));
   vulkan_android_cache_device_destroying(entry_fixture_config.device);
   vulkan_android_cache_device_destroying(entry_fixture_config.device);
   assert(!entry_live && owned_pool_destroyed == 3
         && !android_image_queue_poll(q));
   vulkan_android_cache_device_destroyed(entry_fixture_config.device);
   assert(!vulkan_android_cache_device_pending(entry_fixture_config.device));
   vulkan_android_cache_device_destroyed(entry_fixture_config.device);
   assert(android_image_queue_poll(q) == 1 && !entry_live
         && entry_calls == count && owned_pool_destroyed == 3);
   submit_fail = 0;
   android_frame_pool_close(pool);
   assert(android_frame_pool_free(pool));
   android_image_queue_close(q);
   assert(android_image_queue_free(q));
   puts("PASS: bounded Android Vulkan cache, allocation-free miss/hit, pending "
        "eviction refusal and deferred close");
   return 0;
}
