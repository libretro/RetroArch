/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#define OWNER_MAIN_NAME owner_contract_main
#include "owner_test.c"
#ifdef VIDEOTOOLBOX_VP9_OWNER
struct metal_frame_cv_pool
{
   int busy, freed;
};
static struct metal_frame_cv_pool pool;
static videotoolbox_vp9_decoder_t *retired;
static rvp9_hw_picture_t picture;

static void abandon(void *image, uint64_t done, uint64_t value)
{
   (void)done;
   (void)value;
   CVPixelBufferRelease((CVPixelBufferRef)image);
}

void metal_frame_cv_pool_free(metal_frame_cv_pool_t *p)
{
   assert(p == &pool && !p->freed);
   p->freed = 1;
}

int metal_frame_cv_pool_export(metal_frame_cv_pool_t *p,
      const metal_frame_view_t *view, void *image, video_hw_frame_t *out)
{
   assert(p == &pool && !p->freed && image);
   assert(view->width == 1920 && view->height == 1080
          && view->pixfmt
                   == (header.bit_depth == 10 ? IMAGE_PIXFMT_P010
                                              : IMAGE_PIXFMT_NV12)
          && view->color.transfer == color.transfer);
   if (p->busy)
      return 1;
   memset(out, 0, sizeof(*out));
   out->owner   = CVPixelBufferRetain((CVPixelBufferRef)image);
   out->abandon = abandon;
   out->color   = view->color;
   out->width   = view->width;
   out->height  = view->height;
   out->pixfmt  = view->pixfmt;
   out->api     = VIDEO_HW_API_METAL;
   return 0;
}

static void retire(void *user, videotoolbox_vp9_decoder_t *d)
{
   assert(user == &wakes && !retired);
   retired = d;
}

#ifndef DECODER_MAIN_NAME
#define DECODER_MAIN_NAME main
#endif
int DECODER_MAIN_NAME(void)
{
   videotoolbox_vp9_owner_t *init = open_owner();
   videotoolbox_vp9_decoder_t *d;
   rvp9_hw_ops_t ops;
   video_hw_frame_t first, second, third;
   CVPixelBufferRef old;
   VTDecompressionSessionRef native;
   close_owner(&init);
   assert(videotoolbox_vp9_decoder_open(
         &header, &color, 1, 10, 64, &pool, NULL, wake, retire, &wakes, &ops));
   d = (videotoolbox_vp9_decoder_t *)ops.decoder;
   memset(&picture, 0, sizeof(picture));
   picture.header            = header;
   picture.header.show_frame = 1;
   picture.slot              = 0;
   assert(!ops.decode(d, (const uint8_t *)"one", 3, &picture));
   assert(!ops.decode(d, (const uint8_t *)"two", 3, &picture));
   assert(videotoolbox_vp9_owner_worker(d->owner) == 1);
   old = d->owner->slots->slot[0].result.image;
   assert(old);
   assert(videotoolbox_vp9_owner_worker(d->owner) == 1);
   pool.busy = 1;
   memset(&first, 0, sizeof(first));
   first.width = 777;
   assert(ops.output(d, 0, &first) == 1 && d->image && d->image != old);
   assert(first.width == 777);
   assert(videotoolbox_vp9_decoder_finish(d, false) == 1);
   pool.busy = 0;
   assert(!ops.output(d, 0, &first));
   assert(first.owner && first.api == VIDEO_HW_API_METAL);
   picture.header.show_existing_frame = 1;
   assert(!ops.decode(d, (const uint8_t *)"old", 3, &picture));
   assert(videotoolbox_vp9_owner_worker(d->owner) == 1);
   assert(!ops.output(d, 0, &second));
   assert(!videotoolbox_vp9_decoder_finish(d, false));
   native = d->owner->session;
   assert(!videotoolbox_vp9_decoder_restart(d));
   picture.header.show_existing_frame = 0;
   assert(!ops.decode(d, (const uint8_t *)"new", 3, &picture));
   assert(videotoolbox_vp9_decoder_restart(d) == 1);
   assert(videotoolbox_vp9_owner_worker(d->owner) == 1);
   assert(!videotoolbox_vp9_decoder_restart(d));
   assert(d->owner->session == native && !pool.freed);
   assert(!ops.decode(d, (const uint8_t *)"key", 3, &picture));
   assert(videotoolbox_vp9_owner_worker(d->owner) == 1);
   assert(!ops.output(d, 0, &third));
   videotoolbox_vp9_decoder_close(&ops);
   assert(!ops.decoder && retired == d && pool.freed);
   assert(videotoolbox_vp9_decoder_retire_poll(&retired) && !retired);
   assert(retro_atomic_load_acquire_int(&objects) == 3);
   video_hw_frame_abandon(&first);
   video_hw_frame_abandon(&second);
   video_hw_frame_abandon(&third);
   assert(!retro_atomic_load_acquire_int(&objects));
   pool.freed         = 0;
   header.profile     = 2;
   header.bit_depth   = 10;
   header.color_space = 5;
   color.transfer     = 16;
   expected_pixel     = 0x78343230;
   assert(videotoolbox_vp9_decoder_open(
         &header, &color, 1, 10, 64, &pool, NULL, wake, retire, &wakes, &ops));
   d                         = (videotoolbox_vp9_decoder_t *)ops.decoder;
   picture.header            = header;
   picture.header.show_frame = 1;
   assert(!ops.decode(d, (const uint8_t *)"pq9", 3, &picture));
   assert(videotoolbox_vp9_owner_worker(d->owner) == 1);
   assert(!ops.output(d, 0, &first));
   assert(first.pixfmt == IMAGE_PIXFMT_P010 && first.color.transfer == 16
          && first.color.primaries == 9 && first.color.matrix == 9);
   videotoolbox_vp9_decoder_close(&ops);
   assert(videotoolbox_vp9_decoder_retire_poll(&retired));
   video_hw_frame_abandon(&first);
   assert(!retro_atomic_load_acquire_int(&objects));
   puts("PASS: VP9 codec adapter, stale display discard, Metal export retry, show-existing and leases across retirement; mock pool only");
   return 0;
}
#endif
