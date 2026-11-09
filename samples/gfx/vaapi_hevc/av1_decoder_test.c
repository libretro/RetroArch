/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#define main av1_pool_unit_main
#include "av1_pool_test.c"
#undef main
#include "../../../gfx/common/vaapi_av1_decoder.h"
#if RARCH_VAAPI_AV1_PARAMS_AVAILABLE
#include <formats/image_yuv_blit.h>

static unsigned fds[128], fd_surface[128], allocated, closed_fds;
static bool identity_fault;

static VAStatus export_surface(VADisplay display, VASurfaceID surface,
      uint32_t memory, uint32_t flags, void *output)
{
   VADRMPRIMESurfaceDescriptor *d = (VADRMPRIMESurfaceDescriptor *)output;
   unsigned pitch = session.width * (session.depth == 10 ? 2 : 1);
   unsigned fd = 7 + allocated++;
   assert(fd < 128 && !fds[fd] && display == session.display);
   assert(surface >= 1 && surface <= session.slots);
   assert(memory == VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2);
   assert(flags == (VA_EXPORT_SURFACE_READ_ONLY | VA_EXPORT_SURFACE_SEPARATE_LAYERS));
   fds[fd] = 1;
   fd_surface[fd] = surface;
   d->fourcc = session.depth == 10 ? VA_FOURCC_P010 : VA_FOURCC_NV12;
   d->width = session.width;
   d->height = session.height;
   d->num_objects = 1;
   d->objects[0].fd = (int)fd;
   d->objects[0].size = pitch * session.height * 3 / 2;
   d->objects[0].drm_format_modifier = 0;
   d->num_layers = 2;
   d->layers[0].drm_format = session.depth == 10
         ? UINT32_C(0x20363152) : UINT32_C(0x20203852);
   d->layers[1].drm_format = session.depth == 10
         ? UINT32_C(0x32335247) : UINT32_C(0x38385247);
   d->layers[0].num_planes = d->layers[1].num_planes = 1;
   d->layers[0].object_index[0] = d->layers[1].object_index[0] = 0;
   d->layers[0].offset[0] = 0;
   d->layers[1].offset[0] = pitch * session.height;
   d->layers[0].pitch[0] = d->layers[1].pitch[0] = pitch;
   return VA_STATUS_SUCCESS;
}

static bool seal_fd(int fd)
{
   assert(fd >= 7 && fd < 128 && fds[fd]);
   return true;
}

static bool identify_fd(int fd, uint64_t *device, uint64_t *inode)
{
   assert(fd >= 7 && fd < 128 && fds[fd]);
   if (identity_fault)
      return false;
   *device = 3;
   *inode = 1000 + fd_surface[fd];
   return true;
}

static void close_fd(int fd)
{
   assert(fd >= 7 && fd < 128 && fds[fd]);
   fds[fd] = 0;
   closed_fds++;
}

static const vaapi_hevc_export_ops_t exports = {
      export_surface, seal_fd, close_fd, identify_fd};
static rav1_hw_ops_t decoder_ops;
static rav1_grain_t grain;
static video_hw_frame_t frames[16], unchanged;

static vaapi_av1_decoder_t *open_decoder(unsigned depth)
{
   setup(depth, 12);
   memset(&decoder_ops, 0, sizeof(decoder_ops));
   memset(frames, 0, sizeof(frames));
   memset(fds, 0, sizeof(fds));
   allocated = closed_fds = 0;
   identity_fault = false;
   sequence.color_primaries = sequence.transfer_characteristics = sequence.matrix_coefficients = 1;
   sequence.chroma_sample_position = 2;
   assert(vaapi_av1_decoder_open(&pool, &sequence, NULL,
         &exports, &decoder_ops));
   return (vaapi_av1_decoder_t *)decoder_ops.decoder;
}

static int submit(void)
{
   return decoder_ops.decode(decoder_ops.decoder, packet, sizeof(packet), &sequence, &picture);
}

static void lifecycle(unsigned depth)
{
   vaapi_av1_decoder_t *decoder = open_decoder(depth);
   vaapi_frame_view_t view;
   vaapi_hevc_export_t taken;
   unsigned before;
   completion[0] = VA_STATUS_ERROR_TIMEDOUT;
   assert(!submit());
   memset(&unchanged, 0xa5, sizeof(unchanged));
   memcpy(&frames[0], &unchanged, sizeof(unchanged));
   assert(decoder_ops.output(decoder, 0, &grain, &frames[0]) == 1);
   assert(!memcmp(&frames[0], &unchanged, sizeof(unchanged)) && !allocated);
   completion[0] = VA_STATUS_SUCCESS;
   assert(!decoder_ops.output(decoder, 0, &grain, &frames[0]));
   assert(!decoder_ops.output(decoder, 0, &grain, &frames[1]));
   assert(pool.exports[0] == 2 && allocated == 2);
   assert(frames[0].api == VIDEO_HW_API_VULKAN && frames[0].width == 192);
   assert(frames[0].height == 128 && frames[0].yuv == IMAGE_YUV_FLAG_BT709);
   assert(frames[0].color.matrix == 1 && frames[0].color.range == 1);
   assert(vaapi_frame_claim(frames[0].owner, frames[0].done,
         frames[0].done_value, frames[0].image, &view));
   assert(view.source->native_owner == &pool && view.source->slot == 0
         && view.chroma_position == 2 && view.color.matrix == 1);
   memset(&taken, 0, sizeof(taken));
   assert(vaapi_frame_take(frames[0].owner, frames[0].done,
         frames[0].done_value, &taken));
   next(1, 0);
   picture.setup.size.width = picture.setup.size.upscaled_width =
      picture.setup.size.render_width = 128;
   picture.setup.size.mi_cols = picture.tiles.mi_col_starts[1] = 32;
   assert(!submit() && !decoder_ops.output(decoder, 1, &grain, &frames[2]));
   assert(frames[2].width == 128 && frames[2].height == 128
         && frames[2].color.matrix == 1 && frames[2].color.range == 1);
   assert(frames[1].color.matrix == 1 && frames[1].color.range == 1);
   video_hw_frame_abandon(&frames[1]);
   assert(!closed_fds && pool.exports[0] == 2);
   assert(vaapi_av1_decoder_poll(decoder) == 1);
   assert(closed_fds == 1 && pool.exports[0] == 1);
   next(0, 1);

   assert(!submit() && pool.mapping[0] == 2);
   assert(!decoder_ops.output(decoder, 0, &grain, &frames[3]));
   assert(frames[3].width == 192 && frames[3].color.matrix == 1 && frames[3].color.range == 1);
   assert(taken.slot == 0 && vaapi_export_current(&taken));
   assert(vaapi_av1_decoder_detach(&decoder_ops) == decoder);
   assert(!decoder_ops.decoder && !decoder_ops.decode && !decoder_ops.output);
   assert(!vaapi_av1_decoder_retire(decoder));
   video_hw_frame_abandon(&frames[2]);
   video_hw_frame_abandon(&frames[3]);
   assert(!vaapi_av1_decoder_retire(decoder) && closed_fds == 3);
   video_hw_frame_abandon(&frames[0]);
   before = closed_fds;
   assert(!vaapi_av1_decoder_retire(decoder) && closed_fds == before);
   assert(vaapi_frame_finish(frames[0].owner, frames[0].done,
         frames[0].done_value, &taken));
   assert(vaapi_av1_decoder_retire(decoder));
   assert(closed_fds == allocated && !vaapi_av1_pool_held(&pool));
   assert(vaapi_av1_pool_close(&pool));
}

static void busy(void)
{
   vaapi_av1_decoder_t *decoder = open_decoder(8);
   unsigned i, before;
   assert(!submit());
   for (i = 0; i < 12; i++)
      assert(!decoder_ops.output(decoder, 0, &grain, &frames[i]));
   before = allocated;
   memset(&frames[12], 0xa5, sizeof(frames[12]));
   memcpy(&unchanged, &frames[12], sizeof(unchanged));
   assert(decoder_ops.output(decoder, 0, &grain, &frames[12]) == 1);
   assert(allocated == before + 1 && pool.exports[0] == 13);
   assert(!memcmp(&frames[12], &unchanged, sizeof(unchanged)));
   video_hw_frame_abandon(&frames[0]);
   assert(!decoder_ops.output(decoder, 0, &grain, &frames[12]) && allocated == before + 1);
   assert(pool.exports[0] == 12 && closed_fds == 1);
   assert(vaapi_av1_decoder_detach(&decoder_ops) == decoder);
   for (i = 1; i <= 12; i++) video_hw_frame_abandon(&frames[i]);
   assert(vaapi_av1_decoder_retire(decoder));
   assert(allocated == closed_fds && vaapi_av1_pool_close(&pool));
}

static void failed_export(void)
{
   vaapi_av1_decoder_t *decoder = open_decoder(8);
   assert(!submit());
   identity_fault = true;
   memset(&frames[0], 0xa5, sizeof(frames[0]));
   memcpy(&unchanged, &frames[0], sizeof(unchanged));
   assert(decoder_ops.output(decoder, 0, &grain, &frames[0]) < 0);
   assert(!memcmp(&frames[0], &unchanged, sizeof(unchanged)));
   assert(allocated == 1 && closed_fds == 1 && !pool.exports[0]);
   assert(vaapi_av1_decoder_detach(&decoder_ops) == decoder);
   assert(vaapi_av1_decoder_retire(decoder) && vaapi_av1_pool_close(&pool));
}

static void admission(void)
{
   video_color_metadata_t color;
   vaapi_av1_decoder_t *decoder;
   unsigned before;
   setup(8, 12);
   memset(&decoder_ops, 0, sizeof(decoder_ops));
   memset(&color, 0, sizeof(color));
   sequence.color_primaries = sequence.transfer_characteristics = sequence.matrix_coefficients = 1;
   color.matrix = 9;
   assert(!vaapi_av1_decoder_open(&pool, &sequence, &color,
         &exports, &decoder_ops) && !decoder_ops.decoder);
   color.matrix = 1;
   assert(vaapi_av1_decoder_open(&pool, &sequence, &color,
         &exports, &decoder_ops));
   decoder = (vaapi_av1_decoder_t *)decoder_ops.decoder;
   before = calls;
   sequence.matrix_coefficients = 6;
   assert(submit() < 0 && calls == before);
   assert(vaapi_av1_decoder_detach(&decoder_ops) == decoder);
   assert(vaapi_av1_decoder_retire(decoder) && vaapi_av1_pool_close(&pool));
}

static void explicit_position(void)
{
   vaapi_av1_decoder_t *decoder;
   vaapi_frame_view_t view;
   identity_fault = false;
   setup(8, 12);
   memset(&decoder_ops, 0, sizeof(decoder_ops));
   sequence.color_primaries = sequence.transfer_characteristics = sequence.matrix_coefficients = 1;
   sequence.chroma_sample_position = 3;
   assert(!vaapi_av1_decoder_open(&pool, &sequence, NULL, &exports, &decoder_ops));
   sequence.chroma_sample_position = 1;
   sequence.film_grain_params_present = 1;
   assert(!vaapi_av1_decoder_open(&pool, &sequence, NULL, &exports, &decoder_ops));
   sequence.film_grain_params_present = 0;
   assert(vaapi_av1_decoder_open(&pool, &sequence, NULL, &exports, &decoder_ops));
   decoder = (vaapi_av1_decoder_t *)decoder_ops.decoder;
   assert(!submit() && !decoder_ops.output(decoder, 0, &grain, &frames[0]));
   assert(vaapi_frame_claim(frames[0].owner, frames[0].done,
         frames[0].done_value, frames[0].image, &view));
   assert(view.chroma_position == 1);
   assert(vaapi_frame_unclaim(frames[0].owner, frames[0].done,
         frames[0].done_value, NULL));
   video_hw_frame_abandon(&frames[0]);
   assert(vaapi_av1_decoder_detach(&decoder_ops) == decoder);
   assert(vaapi_av1_decoder_retire(decoder) && vaapi_av1_pool_close(&pool));
}

static void metadata(void)
{
   video_color_metadata_t color;
   vaapi_av1_decoder_t *decoder;
   vaapi_frame_view_t view;
   unsigned before;
   setup(10, 12);
   memset(&decoder_ops, 0, sizeof(decoder_ops));
   memset(&color, 0, sizeof(color));
   sequence.color_primaries = sequence.matrix_coefficients = 9;
   sequence.transfer_characteristics = 16;
   sequence.color_range = 1;
   sequence.chroma_sample_position = 1;
   color.primaries = color.matrix = 9;
   color.transfer = 16;
   color.range = 2;
   color.max_cll = 1200;
   color.max_fall = 400;
   color.mastering.present = 0x100;
   color.mastering.luminance_max = 10000000;
   assert(vaapi_av1_decoder_open(&pool, &sequence, &color, &exports, &decoder_ops));
   decoder = (vaapi_av1_decoder_t *)decoder_ops.decoder;
   memset(&color, 0, sizeof(color));
   allocated = closed_fds = 0;
   identity_fault = false;
   assert(!submit());
   memset(&frames[0], 0xa5, sizeof(frames[0]));
   unchanged = frames[0];
   before = allocated;
   grain.apply = 1;
   assert(decoder_ops.output(decoder, 0, &grain, &frames[0]) < 0);
   assert(allocated == before && !memcmp(&unchanged, &frames[0], sizeof(unchanged)));
   grain.apply = 0;
   assert(!decoder_ops.output(decoder, 0, &grain, &frames[0]));
   assert(frames[0].color.primaries == 9 && frames[0].color.transfer == 16
         && frames[0].color.matrix == 9 && frames[0].color.range == 2
         && frames[0].color.max_cll == 1200 && frames[0].color.max_fall == 400
         && frames[0].color.mastering.luminance_max == 10000000);
   assert(vaapi_frame_claim(frames[0].owner, frames[0].done,
         frames[0].done_value, frames[0].image, &view));
   assert(view.chroma_position == 1 && view.color.max_cll == 1200);
   assert(vaapi_frame_unclaim(frames[0].owner, frames[0].done, frames[0].done_value, NULL));
   video_hw_frame_abandon(&frames[0]);
   assert(vaapi_av1_decoder_detach(&decoder_ops) == decoder);
   assert(vaapi_av1_decoder_retire(decoder));
   assert(vaapi_av1_pool_close(&pool) && allocated == closed_fds);
}

static rav1_grain_t grain_original, grain_request;
static void grain_lifecycle(unsigned depth)
{
   vaapi_av1_decoder_t *decoder;
   vaapi_frame_view_t view;
   vaapi_hevc_export_t taken;
   unsigned before, i;
   grain_setup(depth, 21);
   sequence.color_primaries = sequence.transfer_characteristics =
      sequence.matrix_coefficients = 1;
   sequence.chroma_sample_position = 2;
   picture.grain.params.seed = 1234;
   picture.grain.params.ar_coeff_lag = 3;
   picture.grain.params.num_points[0] = 2;
   picture.grain.params.point_value[0][1] = 255;
   picture.grain.params.point_scaling[0][0] = 64;
   picture.grain.params.point_scaling[0][1] = 128;
   for (i = 1; i < 3; i++)
   {
      picture.grain.params.num_points[i] = 1;
      picture.grain.params.point_scaling[i][0] = 80;
      picture.grain.params.ar_coeffs[i][24] = -128;
      picture.grain.params.chroma_mult[i - 1] = 20;
      picture.grain.params.chroma_luma_mult[i - 1] = 30;
      picture.grain.params.chroma_offset[i - 1] = 100;
   }
   grain_original = picture.grain.params;
   memset(&decoder_ops, 0, sizeof(decoder_ops));
   memset(frames, 0, sizeof(frames));
   memset(fds, 0, sizeof(fds));
   allocated = closed_fds = 0;
   identity_fault = false;
   assert(vaapi_av1_decoder_open(&pool, &sequence, NULL, &exports, &decoder_ops));
   decoder = (vaapi_av1_decoder_t *)decoder_ops.decoder;
   completion[1] = VA_STATUS_ERROR_TIMEDOUT;
   assert(!submit());
   memset(&unchanged, 0xa5, sizeof(unchanged));
   memcpy(&frames[0], &unchanged, sizeof(unchanged));
   before = native_syncs;
#define GRAIN_BAD(change) \
   do { \
      grain_request = grain_original; \
      change; \
      assert(decoder_ops.output(decoder, 0, &grain_request, &frames[0]) < 0); \
      assert(!memcmp(&frames[0], &unchanged, sizeof(unchanged)) && !allocated); \
      assert(native_syncs == before); \
   } while (0)
   GRAIN_BAD(grain_request.seed++);
   GRAIN_BAD(grain_request.apply = 0);
   GRAIN_BAD(grain_request.scaling_shift++);
   GRAIN_BAD(grain_request.ar_coeff_lag--);
   GRAIN_BAD(grain_request.ar_coeff_shift++);
   GRAIN_BAD(grain_request.scale_shift++);
   GRAIN_BAD(grain_request.overlap = 1);
   GRAIN_BAD(grain_request.clip_to_restricted_range = 1);
   GRAIN_BAD(grain_request.point_value[0][1]--);
   GRAIN_BAD(grain_request.point_scaling[1][0]++);
   GRAIN_BAD(grain_request.ar_coeffs[0][23]++);
   GRAIN_BAD(grain_request.ar_coeffs[1][24]++);
   GRAIN_BAD(grain_request.chroma_mult[0]++);
   GRAIN_BAD(grain_request.chroma_luma_mult[1]++);
   GRAIN_BAD(grain_request.chroma_offset[1]++);
#undef GRAIN_BAD
   assert(decoder_ops.output(decoder, 0, &grain_original, &frames[0]) == 1);
   assert(!allocated && !memcmp(&frames[0], &unchanged, sizeof(unchanged)));
   completion[1] = VA_STATUS_SUCCESS;
   assert(!decoder_ops.output(decoder, 0, &grain_original, &frames[0]));
   assert(!decoder_ops.output(decoder, 0, &grain_original, &frames[1]));
   assert(pool.exports[1] == 2 && allocated == 2);
   assert(fd_surface[7] == 2 && fd_surface[8] == 2);
   assert(vaapi_frame_claim(frames[0].owner, frames[0].done,
         frames[0].done_value, frames[0].image, &view));
   assert(view.source->slot == 1 && view.chroma_position == 2);
   memset(&taken, 0, sizeof(taken));
   assert(vaapi_frame_take(frames[0].owner, frames[0].done,
         frames[0].done_value, &taken));
   picture.grain.params.seed = 9999;
   grain_request = grain_original;
   grain_request.point_value[2][13] = 42;
   grain_request.ar_coeffs[0][24] = 127;
   assert(!decoder_ops.output(decoder, 0, &grain_request, &frames[2]));
   video_hw_frame_abandon(&frames[2]);
   assert(vaapi_av1_decoder_poll(decoder) >= 0);
   grain_next(1, 0);
   grain_request = picture.grain.params;
   assert(!submit() && !decoder_ops.output(decoder, 1, &grain_request, &frames[3]));
   assert(pool.mapping[1] == 2 && pool.outputs[2] == 3);
   grain_next(0, 1);
   assert(!submit() && !decoder_ops.output(decoder, 0, &grain_request, &frames[4]));
   assert(pool.mapping[0] == 0 && pool.outputs[0] == 4);
   assert(vaapi_export_current(&taken) && taken.slot == 1);
   assert(decoder_ops.output(decoder, 0, &grain_original, &frames[5]) < 0);
   assert(vaapi_av1_decoder_detach(&decoder_ops) == decoder);
   assert(!vaapi_av1_decoder_retire(decoder));
   video_hw_frame_abandon(&frames[1]);
   video_hw_frame_abandon(&frames[3]);
   video_hw_frame_abandon(&frames[4]);
   video_hw_frame_abandon(&frames[0]);
   assert(!vaapi_av1_decoder_retire(decoder));
   assert(vaapi_frame_finish(frames[0].owner, frames[0].done,
         frames[0].done_value, &taken));
   assert(vaapi_av1_decoder_retire(decoder));
   assert(allocated == closed_fds && vaapi_av1_pool_close(&pool));
}
int main(void)
{
   grain_lifecycle(8);
   grain_lifecycle(10);
   lifecycle(8);
   lifecycle(10);
   busy();
   failed_export();
   admission();
   explicit_position();
   metadata();
   assert(!vaapi_av1_decoder_detach(NULL));
   assert(vaapi_av1_decoder_retire(NULL));
   puts("PASS: AV1 opaque exports, repeated leases, color snapshots and renderer return retirement");
   return 0;
}

#else
int main(void)
{
   puts("SKIP: VA AV1 parameter SDK unavailable");
   return 0;
}
#endif
