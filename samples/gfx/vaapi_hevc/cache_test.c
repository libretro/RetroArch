/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#define VAAPI_PRIME_SUBMIT_TEST_MAIN submission_unit_main
#include "prime_submit_test.c"
#undef VAAPI_PRIME_SUBMIT_TEST_MAIN
#define main resources_unit_main
#include "resources_test.c"
#undef main
#include "../../../gfx/common/vaapi_hevc_vulkan_cache.c"

static vaapi_hevc_vulkan_cache_t *cache;
static vaapi_frame_pool_t *carrier;
static video_hw_frame_t picture;
static unsigned destination_destroyed;

static VKAPI_ATTR VkResult VKAPI_CALL cache_status(VkDevice d, VkFence f)
{
   assert(d == device && (uintptr_t)f == 3000);
   vf_fence_calls++;
   return vf_fence_result;
}

static VKAPI_ATTR void VKAPI_CALL cache_barrier(VkCommandBuffer command,
      VkPipelineStageFlags src, VkPipelineStageFlags dst,
      VkDependencyFlags dependency, uint32_t memories,
      const VkMemoryBarrier *memory, uint32_t buffers,
      const VkBufferMemoryBarrier *buffer, uint32_t n,
      const VkImageMemoryBarrier *images)
{
   assert(command == sub.command);
   if (phase == 3 || phase == 10)
   {
      vf_acquire = phase == 3;
      vf_stage   = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
      vf_barrier((VkCommandBuffer)(uintptr_t)8, src, dst, dependency, memories,
            memory, buffers, buffer, n, images);
      phase++;
   }
   else
      sub_barrier(command, src, dst, dependency, memories, memory, buffers,
            buffer, n, images);
}

static VkResult cache_submit(void *context, VkQueue queue, uint32_t n,
      const VkSubmitInfo *info, VkFence fence)
{
   vf = *sub.frame;
   return sub_submit(context, queue, n, info, fence);
}

static void cache_destination(VkDevice d, void *context)
{
   assert(d == device && context == &destination_destroyed);
   assert(!live && !fences && !vf_images && !vf_views && !vf_allocations);
   assert(ex_pool.exports == 1 && !ex_closes);
   destination_destroyed++;
}

static void cache_publish(void)
{
   video_color_metadata_t color;
   vaapi_frame_view_t view;
   memset(&color, 0, sizeof(color));
   color.primaries = color.matrix = 1;
   color.transfer                 = 13;
   color.range                    = 2;
   assert(!vaapi_frame_export(
         carrier, &ex_frame, 2, 2, 60, 58, 0, &color, &picture));
   assert(vaapi_frame_claim(picture.owner, picture.done, picture.done_value,
         picture.image, &view));
   vf_export_source = view.source;
   assert(vaapi_frame_unclaim(
         picture.owner, picture.done, picture.done_value, NULL));
}

static void cache_setup(bool fp16)
{
   vaapi_hevc_vulkan_cache_config_t config;
   memset(&vf, 0, sizeof(vf));
   vf_reset();
   memset(&config, 0, sizeof(config));
   config.gpu    = (VkPhysicalDevice)(uintptr_t)1;
   config.device = device              = vf_device;
   config.queue                        = (VkQueue)(uintptr_t)7;
   config.family                       = 2;
   config.prime_device                 = true;
   config.memory                       = &vf_memory;
   config.image                        = vf_ops;
   config.submit                       = sub_ops;
   config.submit.fence_status          = cache_status;
   config.submit.submit                = cache_submit;
   config.submit.barrier               = cache_barrier;
   config.resources                    = ops;
   config.get_format                   = vf_format;
   config.get_image                    = vf_format_image;
   config.destroy_deferred_destination = cache_destination;
   config.deferred_destination         = &destination_destroyed;
   calls = next_fence = destroyed = fault = 0;
   count                                  = 1;
   destination_destroyed                  = 0;
   cache = vaapi_hevc_vulkan_cache_new(&config, count, fp16);
   assert(cache && calls == 10 && live && fences);
   assert(!vaapi_hevc_vulkan_cache_free(cache));
   carrier = vaapi_frame_pool_new(1);
   assert(carrier);
   cache_publish();
   phase = sub_fault = submissions = 0;
   destination_layout              = VK_IMAGE_LAYOUT_UNDEFINED;
}

static int cache_present(bool *prepare)
{
   vaapi_hevc_vulkan_cache_entry_t *entry = &cache->entries[0];
   sub                                    = entry->submission;
   vf                                     = entry->frame;
   return vaapi_hevc_vulkan_cache_present(cache, &picture,
         (VkImage)(uintptr_t)500, (VkImageView)(uintptr_t)501,
         cache->resources.format, 60, 58, destination_layout, prepare);
}

static void cache_collect(void)
{
   assert(vaapi_frame_collect(carrier) == 1 && !ex_pool.exports);
   vaapi_frame_pool_close(carrier);
   assert(vaapi_frame_pool_free(carrier));
   carrier = NULL;
   assert(vaapi_hevc_pool_close(&ex_pool));
   assert(!vf_images && !vf_views && !vf_allocations && !vf_fds);
   assert(!live && !fences);
   memset(&vf, 0, sizeof(vf));
}

static void cache_reuse(void)
{
   unsigned n, native_calls, resource_calls;
   bool prepare = false;
   cache_setup(false);
   assert(!cache_present(&prepare) && prepare && !phase && !vf_calls);
   assert(vaapi_hevc_vulkan_cache_prepare(cache, &picture) == 1);
   assert(cache->entries[0].occupied && !cache->entries[0].frame.source.active);
   native_calls   = vf_calls;
   resource_calls = calls;
   for (n = 0; n < 64; n++)
   {
      assert(vaapi_hevc_vulkan_cache_prepare(cache, &picture) == 1);
      assert(vf_calls == native_calls && calls == resource_calls);
      phase = 0;
      assert(cache_present(&prepare) == 1 && !prepare && phase == 14);
      memset(&vf, 0, sizeof(vf));
      assert(vaapi_hevc_vulkan_cache_poll(cache) == 0);
      assert(!vaapi_frame_collect(carrier));
      vf_fence_result = VK_SUCCESS;
      assert(vaapi_hevc_vulkan_cache_poll(cache) == 1);
      assert(!ex_closes && vf_calls == native_calls && calls == resource_calls);
      assert(vaapi_frame_collect(carrier) == 1);
      assert(!ex_pool.exports);
      if (n < 63)
      {
         ex_pool.ready = 1;
         ex_pool.generation[0]++;
         ex_closes = 0;
         assert(vaapi_hevc_export_open(&ex_frame, &ex_pool, 0, &ex_ops));
         cache_publish();
         vf_fence_result    = VK_NOT_READY;
         destination_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      }
   }
   vaapi_hevc_vulkan_cache_close(cache);
   vf_cache_released = true;
   assert(vaapi_hevc_vulkan_cache_free(cache));
   assert(!destination_destroyed && !live && !fences && !vf_images);
   vaapi_frame_pool_close(carrier);
   assert(vaapi_frame_pool_free(carrier));
   carrier = NULL;
   assert(vaapi_hevc_pool_close(&ex_pool));
}

static void cache_faults(void)
{
   unsigned f, native_calls;
   bool prepare;
   for (f = 1; f <= 7; f++)
   {
      cache_setup(false);
      assert(vaapi_hevc_vulkan_cache_prepare(cache, &picture) == 1);
      sub_fault = f < 6 ? f : 0;
      assert(cache_present(&prepare) == (f < 5 ? -1 : f == 5 ? -2 : 1));
      memset(&vf, 0, sizeof(vf));
      assert(!vaapi_frame_collect(carrier));
      vaapi_hevc_vulkan_cache_close(cache);
      if (f < 5)
      {
         assert(vaapi_hevc_vulkan_cache_free(cache));
         video_hw_frame_abandon(&picture);
      }
      else
      {
         if (f >= 6)
         {
            vf_fence_result = VK_ERROR_DEVICE_LOST;
            assert(vaapi_hevc_vulkan_cache_poll(cache) == -1);
         }
         assert(!vaapi_hevc_vulkan_cache_free(cache));
         vaapi_hevc_vulkan_cache_defer(cache);
         vaapi_hevc_vulkan_cache_defer(cache);
         assert(vaapi_hevc_vulkan_cache_device_pending(device));
         vaapi_hevc_vulkan_cache_device_destroyed(device);
         assert(vaapi_hevc_vulkan_cache_device_pending(device) && !ex_closes);
         vaapi_hevc_vulkan_cache_device_destroying((VkDevice)(uintptr_t)2);
         assert(!destination_destroyed);
         vaapi_hevc_vulkan_cache_device_destroying(device);
         assert(destination_destroyed == 1 && !ex_closes);
         native_calls = vf_calls;
         vaapi_hevc_vulkan_cache_device_destroying(device);
         vaapi_hevc_vulkan_cache_device_destroyed((VkDevice)(uintptr_t)2);
         assert(vaapi_hevc_vulkan_cache_device_pending(device));
         if (f == 7)
         {
            cache->entries[0].lease.generation++;
            vaapi_hevc_vulkan_cache_device_destroyed(device);
            assert(vaapi_hevc_vulkan_cache_device_pending(device) && !ex_closes
                   && native_calls == vf_calls);
            cache->entries[0].lease.generation--;
         }
         vaapi_hevc_vulkan_cache_device_destroyed(device);
         assert(!vaapi_hevc_vulkan_cache_device_pending(device)
                && native_calls == vf_calls && !ex_closes);
      }
      cache_collect();
   }
}

static void cache_creation_faults(void)
{
   vaapi_hevc_vulkan_cache_config_t config;
   unsigned n, f;
   cache_setup(false);
   config        = cache->config;
   config.memory = &vf_memory;
   vaapi_hevc_vulkan_cache_close(cache);
   assert(vaapi_hevc_vulkan_cache_free(cache));
   for (n = 1; n <= 32; n += 31)
   {
      count = n;
      for (f = 1; f <= 9 + n; f++)
      {
         calls = next_fence = destroyed = 0;
         fault                          = f;
         assert(!vaapi_hevc_vulkan_cache_new(&config, count, false));
         assert(!live && !fences && ex_pool.exports == 1 && !ex_closes);
      }
      fault = calls = next_fence = 0;
      cache = vaapi_hevc_vulkan_cache_new(&config, count, false);
      assert(cache && !vaapi_hevc_vulkan_cache_free(cache));
      vaapi_hevc_vulkan_cache_close(cache);
      assert(vaapi_hevc_vulkan_cache_free(cache) && !live && !fences);
   }
   assert(!vaapi_hevc_vulkan_cache_new(NULL, 1, false));
   assert(!vaapi_hevc_vulkan_cache_new(&config, 0, false));
   assert(!vaapi_hevc_vulkan_cache_new(&config, 33, false));
   config.prime_device = false;
   assert(!vaapi_hevc_vulkan_cache_new(&config, 1, false));
   video_hw_frame_abandon(&picture);
   cache_collect();
}

static void cache_evict(void)
{
   unsigned native_calls;
   bool prepare;
   cache_setup(true);
   assert(vaapi_hevc_vulkan_cache_prepare(cache, &picture) == 1);
   native_calls = vf_calls;
   assert(cache_present(&prepare) == 1);
   memset(&vf, 0, sizeof(vf));
   vf_fence_result = VK_SUCCESS;
   assert(vaapi_hevc_vulkan_cache_poll(cache) == 1);
   assert(vaapi_frame_collect(carrier) == 1);
   ex_pool.ready = 1;
   ex_pool.generation[0]++;
   ex_closes = 0;
   assert(vaapi_hevc_export_open(&ex_frame, &ex_pool, 0, &ex_ops));
   ex_frame.object_inode[0]++;
   cache_publish();
   phase = 0;
   assert(!cache_present(&prepare) && prepare && !phase
          && vf_calls == native_calls);
   assert(vaapi_hevc_vulkan_cache_prepare(cache, &picture) == 1
          && vf_calls > native_calls);
   assert(cache_present(&prepare) == 1);
   memset(&vf, 0, sizeof(vf));
   assert(vaapi_hevc_vulkan_cache_poll(cache) == 1);
   vaapi_hevc_vulkan_cache_close(cache);
   assert(vaapi_hevc_vulkan_cache_free(cache));
   cache_collect();
}

#ifndef VAAPI_CACHE_TEST_MAIN
#define VAAPI_CACHE_TEST_MAIN main
#endif
int VAAPI_CACHE_TEST_MAIN(void)
{
   unsigned depth, mode;
   assert(!resources_unit_main());
   for (depth = 8; depth <= 10; depth += 2)
      for (mode = 0; mode < 3; mode++)
      {
         ex_depth = depth;
         ex_mode  = mode;
         cache_reuse();
         cache_faults();
         cache_creation_faults();
         cache_evict();
      }
   puts("PASS: persistent VA cache reuse, paused-tick returns, pre-submit restore, uncertain retention and exact-device deferred teardown");
   return 0;
}
