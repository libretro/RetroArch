/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#define VAAPI_PRIME_FRAME_TEST_MAIN frame_unit_main
#include "prime_frame_test.c"
#undef VAAPI_PRIME_FRAME_TEST_MAIN
#include "../../../gfx/common/vaapi_frame.h"
#include <retro_atomic.h>
#include <formats/image.h>
#include <formats/image_yuv_blit.h>
#if defined(__linux__)
#include <pthread.h>
#include <sched.h>
#include <retro_spsc.h>
static pthread_t producer;
#endif
static vaapi_frame_pool_t *opaque;
static video_hw_frame_t current_frame, old_frame;
static vaapi_hevc_export_t owned, other;
static unsigned native_calls;

static void owner_close(int fd)
{
#if defined(__linux__)
   assert(pthread_equal(pthread_self(), producer));
#endif
   native_calls++;
   ex_close(fd);
}

static bool owner_identity(int fd, uint64_t *device, uint64_t *inode)
{
#if defined(__linux__)
   assert(pthread_equal(pthread_self(), producer));
#endif
   return ex_identity(fd, device, inode);
}

static const vaapi_hevc_export_ops_t owner_ops = {
      ex_surface, ex_seal, owner_close, owner_identity};

static void produce(void)
{
   ex_reset();
   assert(vaapi_hevc_export_open(&ex_frame, &ex_pool, 0, &owner_ops));
}

static int publish(video_hw_frame_t *out)
{
   video_color_metadata_t color;
   memset(&color, 0, sizeof(color));
   color.primaries = color.matrix = 1;
   color.transfer                 = 13;
   color.range                    = 1;
   return vaapi_frame_export(
         opaque, &ex_frame, 2, 2, 60, 60, IMAGE_YUV_FLAG_BT709, &color, out);
}

static bool claim(vaapi_frame_view_t *view)
{
   return vaapi_frame_claim(current_frame.owner, current_frame.done,
         current_frame.done_value, current_frame.image, view);
}

static void portable_cases(void)
{
   vaapi_frame_view_t view;
   video_hw_frame_t before, untouched;
   unsigned calls;
   int fd;
   opaque = vaapi_frame_pool_new(1);
   assert(opaque && !vaapi_frame_pool_new(0) && !vaapi_frame_pool_new(33));
   produce();
   assert(!publish(&current_frame));
   old_frame = current_frame;
   assert(!ex_frame.active && current_frame.api == VIDEO_HW_API_VULKAN
          && current_frame.width == 60 && current_frame.height == 60
          && current_frame.crop_x == 2
          && current_frame.yuv == IMAGE_YUV_FLAG_BT709);
   assert(vaapi_frame_callback(current_frame.abandon)
          && !vaapi_frame_callback(NULL));
   video_hw_frame_abandon(&current_frame);
   assert(!ex_closes && ex_pool.exports == 1);
   assert(vaapi_frame_collect(opaque) == 1 && !ex_pool.exports
          && !vaapi_frame_collect(opaque));
   produce();
   memset(&untouched, 0x55, sizeof(untouched));
   before = untouched;
   assert(!publish(&current_frame));
   video_hw_frame_abandon(&old_frame);
   assert(!vaapi_frame_collect(opaque));
   assert(!vaapi_frame_claim(current_frame.owner, current_frame.done,
         current_frame.done_value + 1, current_frame.image, &view));
   assert(!vaapi_frame_claim(current_frame.owner, current_frame.done,
         current_frame.done_value, current_frame.image + 1, &view));
   assert(claim(&view) && view.source->active && view.width == 60
          && view.height == 60 && view.color.range == 1);
   assert(view.pixfmt
          == (ex_depth == 10 ? IMAGE_PIXFMT_P010 : IMAGE_PIXFMT_NV12));
   assert(!vaapi_frame_unclaim(current_frame.owner, current_frame.done,
         current_frame.done_value + 1, NULL));
   video_hw_frame_abandon(&current_frame);
   assert(!vaapi_frame_collect(opaque) && !ex_closes);
   assert(vaapi_frame_take(current_frame.owner, current_frame.done,
                current_frame.done_value, &owned)
          && owned.active);
   fd                             = owned.descriptor.objects[0].fd;
   owned.descriptor.objects[0].fd = -1;
   assert(vaapi_frame_export(opaque, &owned, 2, 2, 60, 60, 0, NULL, &untouched)
                == -1
          && owned.active);
   owned.descriptor.objects[0].fd = fd;
   assert(vaapi_frame_export(opaque, &owned, 2, 2, 60, 60, 0, NULL, &untouched)
                == 1
          && !memcmp(&before, &untouched, sizeof(before)) && owned.active);
   assert(!vaapi_frame_take(current_frame.owner, current_frame.done,
         current_frame.done_value, &other));
   assert(!vaapi_frame_unclaim(current_frame.owner, current_frame.done,
         current_frame.done_value, NULL));
   assert(!vaapi_frame_finish(current_frame.owner, current_frame.done,
         current_frame.done_value, NULL));
   assert(vaapi_frame_unclaim(current_frame.owner, current_frame.done,
                current_frame.done_value, &owned)
          && !owned.active);
   assert(claim(&view)
          && vaapi_frame_take(current_frame.owner, current_frame.done,
                current_frame.done_value, &owned));
   owned.ticket++;
   assert(!vaapi_frame_finish(current_frame.owner, current_frame.done,
                current_frame.done_value, &owned)
          && owned.active);
   owned.ticket--;
   vaapi_frame_pool_close(opaque);
   assert(!vaapi_frame_pool_free(opaque));
   assert(vaapi_frame_export(opaque, &owned, 2, 2, 60, 60, 0, NULL, &untouched)
          == -1);
   assert(!memcmp(&before, &untouched, sizeof(before)) && owned.active);
   assert(vaapi_frame_finish(current_frame.owner, current_frame.done,
                current_frame.done_value, &owned)
          && !owned.active);
   assert(!vaapi_frame_pool_free(opaque) && !ex_closes && ex_pool.exports == 1);
   /* A refused native ticket release remains owned and is retried. */
   ex_pool.generation[0]++;
   assert(!vaapi_frame_collect(opaque) && ex_pool.exports == 1);
   calls = ex_closes;
   ex_pool.generation[0]--;
   assert(vaapi_frame_collect(opaque) == 1 && ex_closes == calls
          && !ex_pool.exports);
   assert(vaapi_frame_pool_free(opaque) && vaapi_hevc_pool_close(&ex_pool));
   opaque = NULL;
}

static void imported_case(void)
{
   vaapi_frame_view_t view;
   vf_reset();
   ex_frame.ops.close = owner_close;
   opaque             = vaapi_frame_pool_new(1);
   assert(opaque && !publish(&current_frame));
   assert(claim(&view)
          && vaapi_frame_take(current_frame.owner, current_frame.done,
                current_frame.done_value, &owned));
   vf_export_source = &owned;
   assert(vaapi_hevc_vulkan_frame_create(&vf, &owned,
                (VkPhysicalDevice)(uintptr_t)1, vf_device, true, &vf_memory,
                &vf_ops, vf_format, vf_format_image)
          && !owned.active);
   assert(vaapi_hevc_vulkan_frame_track(&vf, (VkFence)(uintptr_t)9));
   video_hw_frame_abandon(&current_frame);
   assert(!vaapi_frame_collect(opaque)
          && !vaapi_hevc_vulkan_frame_retire(&vf, &owned)
          && ex_pool.exports == 1 && !ex_closes);
   assert(vaapi_hevc_vulkan_frame_poll(&vf, vf_status) == 0);
   vf_fence_result = VK_SUCCESS;
   assert(vaapi_hevc_vulkan_frame_poll(&vf, vf_status) == 1
          && vaapi_hevc_vulkan_frame_retire(&vf, &owned));
   assert(vaapi_frame_finish(current_frame.owner, current_frame.done,
                current_frame.done_value, &owned)
          && !owned.active && !ex_closes);
   assert(vaapi_frame_collect(opaque) == 1 && !vf.active && !vf_images
          && !vf_allocations && !ex_pool.exports);
   vaapi_frame_pool_close(opaque);
   assert(vaapi_frame_pool_free(opaque) && vaapi_hevc_pool_close(&ex_pool));
   opaque = NULL;
}
#if defined(__linux__)
static retro_spsc_t relay;

static void *renderer(void *unused)
{
   unsigned i;
   vaapi_frame_view_t view;
   vaapi_hevc_export_t source;
   video_hw_frame_t packet;
   (void)unused;
   memset(&source, 0, sizeof(source));
   for (i = 0; i < 10000; i++)
   {
      while (retro_spsc_read_avail(&relay) < sizeof(packet))
         sched_yield();
      assert(
            retro_spsc_read(&relay, &packet, sizeof(packet)) == sizeof(packet));
      assert(vaapi_frame_claim(packet.owner, packet.done, packet.done_value,
                   packet.image, &view)
             && view.width == 60 && view.source->active);
      assert(vaapi_frame_take(
            packet.owner, packet.done, packet.done_value, &source));
      video_hw_frame_abandon(&packet);
      assert(vaapi_frame_finish(
                   packet.owner, packet.done, packet.done_value, &source)
             && !source.active);
   }
   return NULL;
}

static void threaded_case(void)
{
   pthread_t thread;
   unsigned i;
   ex_depth = 10;
   ex_mode  = 0;
   opaque   = vaapi_frame_pool_new(1);
   assert(opaque);
   assert(retro_spsc_init(&relay, 2 * sizeof(current_frame)));
   assert(!pthread_create(&thread, NULL, renderer, NULL));
   for (i = 0; i < 10000; i++)
   {
      produce();
      assert(!publish(&current_frame));
      assert(retro_spsc_write(&relay, &current_frame, sizeof(current_frame))
             == sizeof(current_frame));
      while (!vaapi_frame_collect(opaque))
         sched_yield();
      assert(!ex_pool.exports);
   }
   assert(!pthread_join(thread, NULL));
   retro_spsc_free(&relay);
   vaapi_frame_pool_close(opaque);
   assert(vaapi_frame_pool_free(opaque) && vaapi_hevc_pool_close(&ex_pool));
   opaque = NULL;
   puts("PASS: 10000 producer/renderer VA export handoffs; native close always on producer");
}
#endif
int main(void)
{
#if defined(__linux__)
   producer = pthread_self();
#endif
   assert(!frame_unit_main());
   for (ex_depth = 8; ex_depth <= 10; ex_depth += 2)
      for (ex_mode = 0; ex_mode < 3; ex_mode++)
      {
         portable_cases();
         imported_case();
      }
#if defined(__linux__)
   threaded_case();
#endif
   assert(native_calls);
   puts("PASS: opaque VA ABI carrier, stale/claimed abandonment, take/restore, delayed GPU retirement and owner-only native ticket return");
   return 0;
}
