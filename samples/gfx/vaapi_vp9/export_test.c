/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#define main vp9_pool_contract_suite
#include "pool_test.c"
#undef main
#include "../../../gfx/common/vaapi_vp9_export.h"
#include "../../../gfx/common/vaapi_frame.h"
#include <formats/image.h>
#include <formats/image_yuv_blit.h>
static unsigned fd_live[64], fd_closed, duplicate_fd, release_fail;
static VAStatus export_surface(VADisplay display, VASurfaceID surface,
      uint32_t memory_type, uint32_t flags, void *out)
{
   VADRMPRIMESurfaceDescriptor *descriptor = (VADRMPRIMESurfaceDescriptor *)out;
   unsigned i;
   assert(display == session.display && surface == 100);
   assert(memory_type == VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2);
   assert(flags
         == (VA_EXPORT_SURFACE_READ_ONLY | VA_EXPORT_SURFACE_SEPARATE_LAYERS));
   memset(descriptor, 0, sizeof(*descriptor));
   for (i = 0; i < 4; i++)
      descriptor->objects[i].fd = -1;
   descriptor->fourcc = session.depth == 10 ? VA_FOURCC_P010 : VA_FOURCC_NV12;
   descriptor->width = descriptor->height = 64;
   descriptor->num_objects = descriptor->num_layers = 2;
   descriptor->objects[0].fd = 31;
   descriptor->objects[1].fd = duplicate_fd ? 31 : 32;
   descriptor->objects[0].size = session.depth == 10 ? 8192 : 4096;
   descriptor->objects[1].size = session.depth == 10 ? 4096 : 2048;
   fd_live[31] = 1;
   if (!duplicate_fd)
      fd_live[32] = 1;
   descriptor->layers[0].drm_format = session.depth == 10
         ? VA_FOURCC('R', '1', '6', ' ')
         : VA_FOURCC('R', '8', ' ', ' ');
   descriptor->layers[1].drm_format = session.depth == 10
         ? VA_FOURCC('G', 'R', '3', '2')
         : VA_FOURCC('G', 'R', '8', '8');
   for (i = 0; i < 2; i++)
   {
      descriptor->layers[i].num_planes = 1;
      descriptor->layers[i].object_index[0] = i;
      descriptor->layers[i].pitch[0] = session.depth == 10 ? 128 : 64;
   }
   return VA_STATUS_SUCCESS;
}
static bool cloexec(int fd)
{
   assert(fd >= 0 && fd < 64 && fd_live[fd]);
   return true;
}
static void close_fd(int fd)
{
   assert(fd >= 0 && fd < 64 && fd_live[fd]);
   fd_live[fd] = 0;
   fd_closed++;
}
static bool identity(int fd, uint64_t *device, uint64_t *inode)
{
   assert(fd >= 0 && fd < 64 && fd_live[fd]);
   *device = 7;
   *inode = (uint64_t)fd + 100;
   return true;
}
static bool acquire_ticket(void *owner, unsigned slot, uint64_t *ticket)
{
   return vaapi_vp9_pool_acquire((vaapi_vp9_pool_t *)owner, slot, ticket);
}
static bool current_ticket(void *owner, unsigned slot, uint64_t ticket)
{
   vaapi_vp9_pool_t *native_pool = (vaapi_vp9_pool_t *)owner;
   return native_pool->active && slot < 32 && ticket
         && (native_pool->exports & (UINT32_C(1) << slot))
         && native_pool->generation[slot] == ticket;
}
static bool release_ticket(void *owner, unsigned slot, uint64_t ticket)
{
   if (release_fail)
   {
      release_fail = 0;
      return false;
   }
   return vaapi_vp9_pool_release((vaapi_vp9_pool_t *)owner, slot, ticket);
}
static void ready_picture(void)
{
   init(8);
   select_picture(0, false);
   assert(vaapi_vp9_pool_decode(&pool, packet, sizeof(packet), &picture) == 0);
   done[0] = 1;
   assert(vaapi_vp9_pool_poll(&pool, 0) == 0);
}
#ifndef VAAPI_VP9_EXPORT_TEST_MAIN
#define VAAPI_VP9_EXPORT_TEST_MAIN main
#endif
int VAAPI_VP9_EXPORT_TEST_MAIN(void)
{
   static const vaapi_vp9_export_ops_t exports
         = {export_surface, cloexec, close_fd, identity};
   static const vaapi_export_owner_ops_t ownership
         = {acquire_ticket, current_ticket, release_ticket};
   vaapi_vp9_export_t source, taken;
   vaapi_frame_pool_t *frames;
   vaapi_frame_view_t view;
   video_hw_frame_t frame;
   video_color_metadata_t color;
   unsigned before;
   assert(!vp9_pool_contract_suite());
   memset(&pool, 0, sizeof(pool));
   memset(&source, 0, sizeof(source));
   memset(&taken, 0, sizeof(taken));
   memset(&color, 0, sizeof(color));
   color.primaries = color.matrix = color.range = 1;
   color.transfer = 13;
   ready_picture();
   frames = vaapi_frame_pool_new(1);
   assert(frames);
   assert(vaapi_vp9_export_open(&source, &pool, 0, &exports));
   assert(!source.pool && vaapi_export_owner(&source) == &pool);
   assert(vaapi_export_current(&source));
   assert(vaapi_frame_export(frames, &source, 0, 0, 64, 64,
                IMAGE_YUV_FLAG_BT709, &color, &frame)
         == 0);
   assert(!source.active && pool.exports);
   assert(vaapi_frame_claim(
         frame.owner, frame.done, frame.done_value, frame.image, &view));
   assert(view.source && vaapi_export_owner(view.source) == &pool);
   assert(vaapi_frame_take(frame.owner, frame.done, frame.done_value, &taken));
   taken.native_owner = &session;
   assert(!vaapi_frame_unclaim(
         frame.owner, frame.done, frame.done_value, &taken));
   taken.native_owner = &pool;
   assert(
         vaapi_frame_finish(frame.owner, frame.done, frame.done_value, &taken));
   assert(fd_live[31] && fd_live[32] && pool.exports);
   assert(!vaapi_vp9_pool_close(&pool));
   assert(vaapi_frame_collect(frames) == 1);
   assert(!pool.exports && fd_closed == 2);
   assert(vaapi_vp9_pool_close(&pool));
   vaapi_frame_pool_close(frames);
   assert(vaapi_frame_pool_free(frames));
   ready_picture();
   release_fail = 1;
   assert(vaapi_export_open(&source, &pool, &session, &ownership, 0, &exports));
   assert(!vaapi_vp9_export_close(&source));
   assert(source.active && pool.exports && fd_closed == 4);
   assert(vaapi_vp9_export_close(&source) && !pool.exports && fd_closed == 4);
   duplicate_fd = 1;
   before = fd_closed;
   assert(!vaapi_vp9_export_open(&source, &pool, 0, &exports));
   assert(!source.active && !pool.exports && fd_closed == before + 1);
   assert(!fd_live[31] && !fd_live[32]);
   assert(vaapi_vp9_pool_close(&pool));
   puts("PASS: VP9 DMA-BUF metadata and opaque owner return; mock native "
        "calls");
   return 0;
}
