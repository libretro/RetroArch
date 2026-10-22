/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#define VAAPI_PRIME_SUBMIT_TEST_MAIN submission_unit_main
#include "prime_submit_test.c"
#undef VAAPI_PRIME_SUBMIT_TEST_MAIN
#include "../../../gfx/common/vaapi_hevc_vulkan_lease.h"
#if defined(__linux__)
#include <pthread.h>
#include <sched.h>
#include <retro_spsc.h>
#endif
static vaapi_hevc_vulkan_lease_t lease;
static vaapi_frame_pool_t *lease_pool;
static video_hw_frame_t lease_frame, lease_before;
static void lease_setup(void)
{
   vaapi_frame_view_t view;
   video_color_metadata_t color;
   assert(!lease_pool && !lease.owner && !lease.returned.active);
   memset(&lease, 0, sizeof(lease)); vf_reset();
   lease_pool = vaapi_frame_pool_new(1); assert(lease_pool);
   memset(&color, 0, sizeof(color));
   color.primaries = color.matrix = 1; color.transfer = 13; color.range = 2;
   assert(!vaapi_frame_export(lease_pool, &ex_frame, 2, 2, 60, 58,
            0, &color, &lease_frame));
   assert(vaapi_frame_claim(lease_frame.owner, lease_frame.done,
            lease_frame.done_value, lease_frame.image, &view));
   vf_export_source = view.source;
   assert(vaapi_hevc_vulkan_frame_prepare(&vf, view.source, (VkPhysicalDevice)(uintptr_t)1,
            vf_device, true, &vf_memory, &vf_ops, vf_format, vf_format_image));
   assert(!vf.source.active && view.source->active && ex_pool.exports == 1);
   assert(vaapi_frame_unclaim(lease_frame.owner, lease_frame.done, lease_frame.done_value, NULL));
   memset(&sub, 0, sizeof(sub)); sub.frame = &vf; sub.device = vf_device; sub.ops = sub_ops;
   sub.queue = (VkQueue)(uintptr_t)7; sub.command = (VkCommandBuffer)(uintptr_t)8;
   sub.fence = (VkFence)(uintptr_t)9; sub.pipeline = (VkPipeline)(uintptr_t)600;
   sub.layout = (VkPipelineLayout)(uintptr_t)601; sub.set = (VkDescriptorSet)(uintptr_t)602;
   sub.sampler = (VkSampler)(uintptr_t)603; sub.family = 2;
   phase = sub_fault = submissions = 0; destination_layout = VK_IMAGE_LAYOUT_UNDEFINED;
   assert(vaapi_hevc_vulkan_lease_bind(&lease, &sub, false));
   assert(!vaapi_hevc_vulkan_lease_bind(&lease, &sub, false));
   lease_before = lease_frame;
}
static int lease_present_frame(const video_hw_frame_t *frame)
{
   return vaapi_hevc_vulkan_lease_present(&lease, frame,
         (VkImage)(uintptr_t)500, (VkImageView)(uintptr_t)501,
         VK_FORMAT_R8G8B8A8_UNORM, 60, 58, destination_layout);
}
static int lease_present(void) { return lease_present_frame(&lease_frame); }
static void lease_cleanup(void)
{
   assert(!lease.owner && !lease.returned.active && !ex_closes);
   assert(vaapi_frame_collect(lease_pool) == 1 && !ex_pool.exports);
   vaapi_frame_pool_close(lease_pool); assert(vaapi_frame_pool_free(lease_pool)); lease_pool = NULL;
   assert(vaapi_hevc_pool_close(&ex_pool));
   if (vf.active)
   {
      vf_cache_released = true;
      assert(vaapi_hevc_vulkan_frame_retire(&vf, &vf_returned) && !vf_returned.active);
   }
   assert(!vf_images && !vf_views && !vf_allocations && !vf_fds);
   memset(&lease, 0, sizeof(lease));
}
static void lease_faults(void)
{
   unsigned fault, calls;
   for (fault = 1; fault <= 5; fault++)
   {
      lease_setup(); sub_fault = fault;
      assert(lease_present() == (fault == 5 ? -2 : -1));
      assert(!ex_closes && !vaapi_frame_collect(lease_pool));
      if (fault == 5)
      {
         assert(lease.owner && vf.pending && vf.broken);
         video_hw_frame_abandon(&lease_frame);
         assert(!vaapi_frame_collect(lease_pool));
         assert(!vaapi_hevc_vulkan_lease_after_device_destroy(&lease, (VkDevice)(uintptr_t)2));
         calls = vf_calls; vf_images = vf_views = vf_allocations = 0;
         assert(vaapi_hevc_vulkan_lease_after_device_destroy(&lease, vf_device) && vf_calls == calls);
      }
      else
      {
         assert(!lease.owner && !vf.source.active && !vf.pending);
         assert(!memcmp(&lease_frame, &lease_before, sizeof(lease_frame)));
         sub_fault = phase = 0;
         assert(lease_present() == 1); vf_fence_result = VK_SUCCESS;
         assert(vaapi_hevc_vulkan_lease_poll(&lease) == 1);
      }
      lease_cleanup();
   }
}
static void lease_hdr(void)
{
   video_color_metadata_t color;
   unsigned transfer, fp16;
   float value;
   for (transfer = 16; transfer <= 18; transfer += 2)
      for (fp16 = 0; fp16 < 2; fp16++)
      {
         lease_setup(); video_hw_frame_abandon(&lease_frame);
         assert(vaapi_frame_collect(lease_pool) == 1); ex_reset();
         assert(vaapi_hevc_export_open(&ex_frame, &ex_pool, 0, &ex_ops));
         memset(&color, 0, sizeof(color)); color.primaries = color.matrix = 9;
         color.transfer = transfer; color.range = 2; color.max_cll = 1000;
         assert(!vaapi_frame_export(lease_pool, &ex_frame, 2, 2, 60, 58, 0, &color, &lease_frame));
         memset(&lease, 0, sizeof(lease));
         assert(vaapi_hevc_vulkan_lease_bind(&lease, &sub, fp16 != 0));
         lease_frame.color.transfer = 13;
         assert(vaapi_hevc_vulkan_lease_present(&lease, &lease_frame,
                  (VkImage)(uintptr_t)500, (VkImageView)(uintptr_t)501,
                  fp16 ? VK_FORMAT_R16G16B16A16_SFLOAT : VK_FORMAT_R8G8B8A8_UNORM,
                  60, 58, VK_IMAGE_LAYOUT_UNDEFINED) == 1);
         memcpy(&value, sub.header+16, sizeof(value)); assert(value == (float)transfer);
         memcpy(&value, sub.header+19, sizeof(value)); assert(fp16 ? value == -1.0f : value != -1.0f);
         vf_fence_result = VK_SUCCESS; assert(vaapi_hevc_vulkan_lease_poll(&lease) == 1);
         lease_cleanup();
      }
}
#if defined(__linux__)
static pthread_t lease_producer;
static retro_spsc_t lease_relay;
static void lease_owner_close(int fd)
{
   assert(pthread_equal(pthread_self(), lease_producer)); ex_close(fd);
}
static bool lease_owner_identity(int fd, uint64_t *device, uint64_t *inode)
{
   assert(pthread_equal(pthread_self(), lease_producer)); return ex_identity(fd, device, inode);
}
static void *lease_renderer(void *unused)
{
   unsigned i;
   video_hw_frame_t packet;
   (void)unused;
   for (i = 0; i < 10000; i++)
   {
      while (retro_spsc_read_avail(&lease_relay) < sizeof(packet)) sched_yield();
      assert(retro_spsc_read(&lease_relay, &packet, sizeof(packet)) == sizeof(packet));
      phase = 0; vf_fence_result = VK_NOT_READY;
      assert(lease_present_frame(&packet) == 1);
      assert(vaapi_hevc_vulkan_lease_poll(&lease) == 0);
      vf_fence_result = VK_SUCCESS;
      assert(vaapi_hevc_vulkan_lease_poll(&lease) == 1 && !lease.owner);
   }
   return NULL;
}
static void lease_threaded(void)
{
   static const vaapi_hevc_export_ops_t ops = { ex_surface, ex_seal, lease_owner_close, lease_owner_identity };
   video_color_metadata_t color;
   pthread_t thread;
   unsigned i;
   ex_depth = 10; ex_mode = 2; lease_setup(); lease_producer = pthread_self();
   video_hw_frame_abandon(&lease_frame); assert(vaapi_frame_collect(lease_pool) == 1);
   assert(retro_spsc_init(&lease_relay, 2*sizeof(lease_frame)));
   memset(&color, 0, sizeof(color));
   color.primaries = color.matrix = 1; color.transfer = 13; color.range = 2;
   assert(!pthread_create(&thread, NULL, lease_renderer, NULL));
   for (i = 0; i < 10000; i++)
   {
      ex_reset(); assert(vaapi_hevc_export_open(&ex_frame, &ex_pool, 0, &ops));
      assert(!vaapi_frame_export(lease_pool, &ex_frame, 2, 2, 60, 58, 0, &color, &lease_frame));
      assert(retro_spsc_write(&lease_relay, &lease_frame, sizeof(lease_frame)) == sizeof(lease_frame));
      while (!vaapi_frame_collect(lease_pool)) sched_yield();
   }
   assert(!pthread_join(thread, NULL)); retro_spsc_free(&lease_relay);
   vaapi_frame_pool_close(lease_pool); assert(vaapi_frame_pool_free(lease_pool)); lease_pool = NULL;
   assert(vaapi_hevc_pool_close(&ex_pool)); vf_cache_released = true;
   assert(vaapi_hevc_vulkan_frame_retire(&vf, &vf_returned) && !vf_returned.active);
   memset(&lease, 0, sizeof(lease));
   puts("PASS: 10000 opaque VA conversion/fence-return handoffs; identity and native close execute only on producer");
}
#endif
int main(void)
{
   unsigned i, calls;
   assert(!submission_unit_main());
   for (ex_depth = 8; ex_depth <= 10; ex_depth += 2)
      for (ex_mode = 0; ex_mode < 3; ex_mode++)
      {
         lease_setup();
         lease_frame.image++; assert(lease_present() == -1); lease_frame = lease_before;
         lease_frame.width--; assert(lease_present() == -1); lease_frame = lease_before;
         lease_frame.pixfmt = IMAGE_PIXFMT_8888; assert(lease_present() == -1); lease_frame = lease_before;
         assert(!phase && !submissions && !lease.owner && !vaapi_frame_collect(lease_pool));
         assert(lease_present() == 1 && vf.pending && lease.owner && phase == 14);
         vaapi_frame_pool_close(lease_pool); assert(!vaapi_frame_pool_free(lease_pool));
         video_hw_frame_abandon(&lease_frame);
         assert(vaapi_hevc_vulkan_lease_poll(&lease) == 0 && !vaapi_frame_collect(lease_pool));
         phase = 0; assert(lease_present() == 0 && !phase);
         vf_fence_result = VK_SUCCESS;
         assert(vaapi_hevc_vulkan_lease_poll(&lease) == 1 && !lease.owner && !vf.source.active && vf.active);
         assert(!ex_closes && vaapi_hevc_vulkan_lease_poll(&lease) == 1);
         assert(lease_present() == -1); lease_cleanup();
      }
   ex_depth = 10; ex_mode = 2; lease_faults();
   lease_hdr();
   for (i = 0; i < 2; i++)
   {
      lease_setup(); assert(lease_present() == 1);
      if (!i)
      {
         vf_fence_result = VK_ERROR_DEVICE_LOST;
         assert(vaapi_hevc_vulkan_lease_poll(&lease) == -1 && lease.owner);
      }
      else
      {
         vf_fence_result = VK_SUCCESS; lease.generation++;
         assert(vaapi_hevc_vulkan_lease_poll(&lease) == -1 && lease.returned.active && !vf.source.active);
      }
      calls = vf_calls; vf_images = vf_views = vf_allocations = 0;
      if (i)
      {
         assert(!vaapi_hevc_vulkan_lease_after_device_destroy(&lease, vf_device));
         assert(lease.owner && lease.returned.active); lease.generation--;
      }
      assert(vaapi_hevc_vulkan_lease_after_device_destroy(&lease, vf_device) && vf_calls == calls);
      lease_cleanup();
   }
#if defined(__linux__)
   lease_threaded();
#endif
   puts("PASS: cold borrowed imports, opaque VA claim/submit/fence return, refusal restore and exact-device error retirement; native returns only on decoder owner");
   return 0;
}
