/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#define VAAPI_POOL_TEST_MAIN pool_unit_main
#include "pool_test.c"
#undef VAAPI_POOL_TEST_MAIN
#include "../../../gfx/common/vaapi_hevc_decoder.h"
#include <formats/image_yuv_blit.h>
static unsigned decoder_fds, decoder_closes;
static bool decoder_fault;

static VAStatus decoder_export(VADisplay display, VASurfaceID surface,
      uint32_t memory, uint32_t flags, void *out)
{
   VADRMPRIMESurfaceDescriptor *d = (VADRMPRIMESurfaceDescriptor *)out;
   unsigned pitch = po_session.width * (po_session.depth == 10 ? 2 : 1);
   assert(display == po_session.display && surface >= 100 && surface < 132);
   assert(memory == VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2);
   assert(flags
          == (VA_EXPORT_SURFACE_READ_ONLY | VA_EXPORT_SURFACE_SEPARATE_LAYERS));
   assert(!decoder_fds);
   d->fourcc        = po_session.depth == 10 ? VA_FOURCC_P010 : VA_FOURCC_NV12;
   d->width         = po_session.width;
   d->height        = po_session.height;
   d->num_objects   = 1;
   d->objects[0].fd = 7;
   d->objects[0].size                = pitch * po_session.height * 3 / 2;
   d->objects[0].drm_format_modifier = 0;
   d->num_layers                     = 2;
   d->layers[0].drm_format =
         po_session.depth == 10 ? UINT32_C(0x20363152) : UINT32_C(0x20203852);
   d->layers[1].drm_format =
         po_session.depth == 10 ? UINT32_C(0x32335247) : UINT32_C(0x38385247);
   d->layers[0].num_planes = d->layers[1].num_planes = 1;
   d->layers[0].object_index[0] = d->layers[1].object_index[0] = 0;
   d->layers[0].offset[0]                                      = 0;
   d->layers[1].offset[0] = pitch * po_session.height;
   d->layers[0].pitch[0] = d->layers[1].pitch[0] = pitch;
   decoder_fds                                   = 1;
   return VA_STATUS_SUCCESS;
}

static bool decoder_seal(int fd)
{
   assert(fd == 7 && decoder_fds);
   return true;
}

static bool decoder_identity(int fd, uint64_t *device, uint64_t *inode)
{
   assert(fd == 7 && decoder_fds);
   if (decoder_fault)
      return false;
   *device = 3;
   *inode  = 5;
   return true;
}

static void decoder_close_fd(int fd)
{
   assert(fd == 7 && decoder_fds);
   decoder_fds = 0;
   decoder_closes++;
}

static const vaapi_hevc_export_ops_t decoder_exports = {
      decoder_export, decoder_seal, decoder_close_fd, decoder_identity};
static rh265_hw_ops_t decoder_ops;
static video_hw_frame_t decoder_frame, decoder_before;
static rh265_hw_state_t decoder_state;
static video_color_metadata_t decoder_color;

static void decoder_case(
      const sequence_case_t *fixture, bool claimed, bool failed, unsigned yuv)
{
   rh265_video *source = rh265_video_open();
   vaapi_hevc_decoder_t *decoder;
   vaapi_frame_view_t view;
   vaapi_hevc_export_t taken;
   unsigned before_checks;
   decoder_fault = false;
   assert(source
          && !rh265_video_set_extradata(
                source, fixture->picture.hvcc, fixture->picture.hvcc_size));
   assert(!rh265_video_get_access_unit(source, fixture->packets[0].data,
         fixture->packets[0].size, 4, po_slices, 8, &po_au));
   assert(!rh265_video_get_params(
         source, (unsigned)po_au.first.pps_id, &po_params));
   po_setup((unsigned)po_params.bit_depth);
   po_session.width  = (unsigned)po_params.width;
   po_session.height = (unsigned)po_params.height;
   memset(&decoder_ops, 0, sizeof(decoder_ops));
   memset(&decoder_color, 0, sizeof(decoder_color));
   decoder_color.primaries = decoder_color.matrix = 1;
   decoder_color.transfer                         = 13;
   decoder_color.range                            = 2;
   decoder_closes                                 = 0;
   assert(vaapi_hevc_decoder_open(&po_pool, &po_params, &decoder_color, yuv,
         &decoder_exports, &decoder_ops));
   decoder = (vaapi_hevc_decoder_t *)decoder_ops.decoder;
   assert(!vaapi_hevc_decoder_retire(decoder));
   assert(!vaapi_hevc_decoder_open(&po_pool, &po_params, &decoder_color, yuv,
         &decoder_exports, &decoder_ops));
   assert(rh265_hw_state_init(&decoder_state, 32));
   assert(!rh265_hw_prepare(&decoder_state, &po_params, &po_au, &po_picture));
   assert(!decoder_ops.decode(
         decoder, &po_params, &po_au, po_slices, &po_picture));
   assert(rh265_hw_commit(&decoder_state, &po_picture));
   memset(&decoder_frame, 0xa5, sizeof(decoder_frame));
   decoder_before = decoder_frame;
   before_checks  = po_checks;
   assert(decoder_ops.output(decoder, po_picture.slot, &decoder_frame) == 1);
   assert(po_checks == before_checks + 1);
   assert(!memcmp(&decoder_frame, &decoder_before, sizeof(decoder_frame)));
   before_checks            = po_checks;
   po_done[po_picture.slot] = 1;
   assert(vaapi_hevc_decoder_poll(decoder) == 1
          && po_checks == before_checks + 1);
   if (failed)
   {
      decoder_fault = true;
      assert(decoder_ops.output(decoder, po_picture.slot, &decoder_frame) == -1);
      assert(!memcmp(&decoder_frame, &decoder_before, sizeof(decoder_frame)));
      assert(!decoder_fds && decoder_closes == 1 && !po_pool.exports);
      assert(decoder_ops.drain(decoder) == -1);
      assert(vaapi_hevc_decoder_detach(&decoder_ops) == decoder);
      assert(vaapi_hevc_decoder_retire(decoder));
      assert(vaapi_hevc_pool_close(&po_pool) && po_count == po_drops);
      rh265_video_close(source);
      return;
   }
   assert(!decoder_ops.output(decoder, po_picture.slot, &decoder_frame));
   assert(decoder_fds && !decoder_closes && po_pool.exports);
   assert(decoder_frame.yuv == yuv);
   assert(decoder_frame.width
          == (unsigned)(po_params.width - po_params.crop_left
                        - po_params.crop_right));
   assert(decoder_frame.height
          == (unsigned)(po_params.height - po_params.crop_top
                        - po_params.crop_bottom));
   assert(!memcmp(&decoder_frame.color, &decoder_color, sizeof(decoder_color)));
   assert(decoder_ops.output(decoder, po_picture.slot, &decoder_before) == 1);
   assert(!decoder_ops.drain(decoder));
   memset(&taken, 0, sizeof(taken));
   if (claimed)
   {
      assert(vaapi_frame_claim(decoder_frame.owner, decoder_frame.done,
            decoder_frame.done_value, decoder_frame.image, &view));
      assert(vaapi_frame_take(decoder_frame.owner, decoder_frame.done,
            decoder_frame.done_value, &taken));
   }
   assert(vaapi_hevc_decoder_detach(&decoder_ops) == decoder);
   assert(!decoder_ops.decoder && !decoder_ops.decode && !decoder_ops.output
          && !decoder_ops.drain);
   assert(!vaapi_hevc_decoder_retire(decoder) && !decoder_closes);
   video_hw_frame_abandon(&decoder_frame);
   if (claimed)
   {
      assert(!vaapi_hevc_decoder_retire(decoder) && !decoder_closes);
      assert(vaapi_frame_finish(decoder_frame.owner, decoder_frame.done,
            decoder_frame.done_value, &taken));
   }
   if (claimed)
   {
      po_pool.generation[po_picture.slot]++;
      assert(!vaapi_hevc_decoder_retire(decoder));
      assert(!decoder_fds && decoder_closes == 1 && po_pool.exports);
      po_pool.generation[po_picture.slot]--;
   }
   assert(vaapi_hevc_decoder_retire(decoder));
   assert(!decoder_fds && decoder_closes == 1 && !po_pool.exports);
   assert(vaapi_hevc_pool_close(&po_pool) && po_count == po_drops);
   rh265_video_close(source);
}

int main(void)
{
   unsigned i;
   assert(!pool_unit_main());
   for (i = 0; i < sizeof(sequence_cases) / sizeof(sequence_cases[0]); i++)
   {
      decoder_case(&sequence_cases[i], false, false, 0);
      decoder_case(&sequence_cases[i], false, false, IMAGE_YUV_FLAG_FULL_RANGE);
      decoder_case(&sequence_cases[i], true, false, IMAGE_YUV_FLAG_BT709);
      decoder_case(&sequence_cases[i], false, true,
            IMAGE_YUV_FLAG_FULL_RANGE | IMAGE_YUV_FLAG_BT709);
      decoder_case(&sequence_cases[i], false, false,
            IMAGE_YUV_FLAG_FULL_RANGE | IMAGE_YUV_FLAG_BT709);
   }
   assert(!vaapi_hevc_decoder_detach(NULL));
   assert(vaapi_hevc_decoder_retire(NULL));
   puts("PASS: parsed HEVC decode/completion to opaque VA leases, busy output, crop/color, detach and decoder-owner return retirement");
   return 0;
}
