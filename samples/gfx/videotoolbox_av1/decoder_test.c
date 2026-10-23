/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#define OWNER_MAIN_NAME owner_contract_main
#include "owner_test.c"
#include "../../../gfx/common/videotoolbox_av1_decoder.h"
#ifdef VIDEOTOOLBOX_AV1_OWNER
struct metal_frame_cv_pool
{
   int busy, freed;
};
static struct metal_frame_cv_pool pool;
static videotoolbox_av1_decoder_t *retired;
static rav1_hw_picture_t picture;
static unsigned expected_width, expected_height;

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
   assert(view->width == expected_width && view->height == expected_height
         && view->pixfmt
               == (header.bit_depth == 10 ? IMAGE_PIXFMT_P010
                                          : IMAGE_PIXFMT_NV12)
         && view->color.transfer == color.transfer);
   if (p->busy)
      return 1;
   memset(out, 0, sizeof(*out));
   out->owner = CVPixelBufferRetain((CVPixelBufferRef)image);
   out->abandon = abandon;
   out->color = view->color;
   out->width = view->width;
   out->height = view->height;
   out->pixfmt = view->pixfmt;
   out->api = VIDEO_HW_API_METAL;
   return 0;
}

static void retire(void *user, videotoolbox_av1_decoder_t *d)
{
   assert(user == &wakes && !retired);
   retired = d;
}

#ifndef DECODER_MAIN_NAME
#define DECODER_MAIN_NAME main
#endif
int DECODER_MAIN_NAME(void)
{
   static uint8_t bytes[128];
   static const uint8_t frame[] = {0x32, 1, 0x80};
   static const uint8_t repeat[] = {0x1a, 1, 0x80};
   rav1_obu_t sequence;
   rav1_hw_ops_t ops;
   rav1_grain_t grain;
   size_t offset = 0;
   videotoolbox_av1_decoder_t *d;
   video_hw_frame_t first, second;
   VTDecompressionSessionRef native;
   bytes[0] = 0x0a;
   bytes[1] = (uint8_t)sizeof(av1_sequence_8_grain);
   memcpy(bytes + 2, av1_sequence_8_grain, sizeof(av1_sequence_8_grain));
   assert(rav1_obu_next(&sequence, bytes, sizeof(av1_sequence_8_grain) + 2,
                &offset, false)
         == 1);
   assert(
         rav1_sequence_parse(&header, sequence.payload, sequence.payload_size));
   memset(&color, 0, sizeof(color));
   color.primaries = color.transfer = color.matrix = color.range = 1;
   expected_pixel = kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange;
   assert(videotoolbox_av1_decoder_open(&header, &color, &sequence, 10, 64,
         &pool, NULL, wake, retire, &wakes, &ops));
   d = (videotoolbox_av1_decoder_t *)ops.decoder;
   memset(&picture, 0, sizeof(picture));
   picture.slot = 0;
   picture.header.show_frame = 1;
   picture.setup.size.upscaled_width = header.max_width;
   picture.setup.size.height = header.max_height;
   picture.grain.params.apply = 1;
   picture.grain.params.seed = 55;
   expected_width = header.max_width;
   expected_height = header.max_height;
   assert(!ops.decode(d, frame, sizeof(frame), &header, &picture));
   grain = picture.grain.params;
   picture.grain.params.seed++;
   assert(videotoolbox_av1_owner_worker(d->owner) == 1);
   memset(&first, 0, sizeof(first));
   first.width = 777;
   assert(ops.output(d, 0, &picture.grain.params, &first) < 0
         && first.width == 777);
   pool.busy = 1;
   assert(ops.output(d, 0, &grain, &first) == 1 && d->image
         && first.width == 777);
   assert(videotoolbox_av1_decoder_finish(d, false) == 1);
   pool.busy = 0;
   assert(!ops.output(d, 0, &grain, &first));
   assert(first.owner && first.width == expected_width);
   picture.header.show_frame = 0;
   picture.header.show_existing = 1;
   picture.grain.params = grain;
   picture.setup.size.upscaled_width /= 2;
   picture.setup.size.height /= 2;
   expected_width = picture.setup.size.upscaled_width;
   expected_height = picture.setup.size.height;
   assert(!ops.decode(d, repeat, sizeof(repeat), &header, &picture));
   assert(videotoolbox_av1_owner_worker(d->owner) == 1);
   assert(!ops.output(d, 0, &grain, &second));
   assert(first.width != second.width && first.height != second.height);
   native = d->owner->session;
   assert(!videotoolbox_av1_decoder_restart(d));
   assert(d->owner->session == native);
   header.max_width++;
   assert(ops.decode(d, repeat, sizeof(repeat), &header, &picture) < 0);
   header.max_width--;
   videotoolbox_av1_decoder_close(&ops);
   assert(!ops.decoder && retired == d && pool.freed);
   assert(videotoolbox_av1_decoder_retire_poll(&retired) && !retired);
   assert(retro_atomic_load_acquire_int(&objects) == 2);
   video_hw_frame_abandon(&first);
   video_hw_frame_abandon(&second);
   assert(!retro_atomic_load_acquire_int(&objects));
   puts("PASS: AV1 decoder metadata snapshots, export retry, repeats and "
        "retained mock leases");
   return 0;
}
#endif
