/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifdef NDEBUG
#error VA AV1 picture conformance requires assertions
#endif
#include <assert.h>
#include <stdio.h>
#include "../../../gfx/common/vaapi_av1_picture.h"
static rav1_sequence_t sequence;
static rav1_hw_picture_t picture;
static VADecPictureParameterBufferAV1 native, saved;
static VASurfaceID surfaces[32];
static void setup(void)
{
   unsigned i;
   memset(&sequence, 0, sizeof(sequence));
   memset(&picture, 0, sizeof(picture));
   sequence.operating_point_count = 1;
   sequence.bit_depth = 8;
   sequence.subsampling_x = sequence.subsampling_y = 1;
   sequence.max_width = 192;
   sequence.max_height = 128;
   picture.header.frame_is_intra = 1;
   picture.header.primary_ref_frame = 7;
   picture.setup.size.width = picture.setup.size.upscaled_width = 192;
   picture.setup.size.height = 128;
   picture.setup.size.superres_denom = 8;
   picture.tiles.layout.columns = picture.tiles.layout.rows = 1;
   picture.tiles.mi_col_starts[1] = 48;
   picture.tiles.mi_row_starts[1] = 32;
   for (i = 0; i < 8; i++)
      picture.reference_slots[i] = -1;
   for (i = 0; i < 32; i++)
      surfaces[i] = 100 + i;
   memset(&native, 0xa5, sizeof(native));
   saved = native;
}
static bool build(void)
{
   return vaapi_av1_picture(&native, &sequence, &picture, surfaces, 32, 1000);
}
static void refused(void)
{
   assert(!build());
   assert(!memcmp(&native, &saved, sizeof(native)));
}
static void fields(void)
{
   setup();
   picture.picture.base_q_idx = 177;
   picture.picture.delta_q_y_dc = -12;
   picture.picture.cdef_y_pri[0] = 7;
   picture.picture.cdef_y_sec[0] = 4;
   picture.picture.cdef_uv_pri[0] = 3;
   picture.picture.cdef_uv_sec[0] = 2;
   picture.picture.reference.segment_enabled[2] = 3;
   picture.picture.reference.segment_data[2][0] = -255;
   picture.picture.reference.segment_data[2][1] = 63;
   picture.picture.restoration_type[0] = RAV1_RESTORE_WIENER;
   picture.picture.restoration_type[1] = RAV1_RESTORE_SGRPROJ;
   picture.picture.restoration_size[0] = 128;
   picture.picture.restoration_size[1] = 64;
   picture.picture.restoration_size[2] = 64;
   assert(build());
   assert(native.current_frame == 100);
   assert(native.current_display_picture == 100);
   assert(native.frame_width_minus1 == 191);
   assert(native.frame_height_minus1 == 127);
   assert(native.base_qindex == 177 && native.y_dc_delta_q == -12);
   assert(native.cdef_y_strengths[0] == 31);
   assert(native.cdef_uv_strengths[0] == 14);
   assert(native.seg_info.feature_mask[2] == 3);
   assert(native.seg_info.feature_data[2][0] == -255);
   assert(native.seg_info.feature_data[2][1] == 63);
   assert(native.seg_info.feature_data[2][2] == 0);
   assert(native.loop_restoration_fields.bits.lr_unit_shift == 1);
   assert(native.loop_restoration_fields.bits.lr_uv_shift == 1);
   assert(native.wm[0].wmmat[2] == 65536);
   assert(native.wm[0].wmmat[5] == 65536);
   assert(native.wm[0].wmmat[6] == 0 && native.wm[0].wmmat[7] == 0);
   assert(native.ref_frame_map[0] == VA_INVALID_SURFACE);
}
static void motion(void)
{
   setup();
   picture.motion.global.type[0] = RAV1_GLOBAL_TRANSLATION;
   picture.motion.global.params[0][0] = 512;
   picture.motion.global.params[0][1] = -1024;
   picture.motion.global.params[0][2] = 65536;
   picture.motion.global.params[0][5] = 65536;
   assert(build());
   assert(native.wm[0].wmmat[0] == 512);
   assert(native.wm[0].wmmat[1] == -1024);
   assert(!native.wm[0].invalid);
   saved = native;
   picture.motion.global.params[0][3] = 1;
   refused();
}
static void references(void)
{
   unsigned i;
   setup();
   picture.slot = 8;
   picture.header.frame_type = RAV1_FRAME_INTER;
   picture.header.frame_is_intra = 0;
   for (i = 0; i < 8; i++)
   {
      picture.reference_slots[i] = (int)i;
      picture.reference_info[i].valid = 1;
   }
   for (i = 0; i < 7; i++)
      picture.setup.ref_frame_idx[i] = 7 - i;
   assert(build());
   assert(native.current_frame == 108);
   for (i = 0; i < 8; i++)
      assert(native.ref_frame_map[i] == surfaces[i]);
   for (i = 0; i < 7; i++)
      assert(native.ref_frame_idx[i] == 7 - i);
   saved = native;
   picture.reference_info[7].valid = 0;
   refused();
   picture.reference_info[7].valid = 1;
   picture.setup.ref_frame_idx[0] = 8;
   refused();
   picture.setup.ref_frame_idx[0] = 7;
   surfaces[7] = VA_INVALID_SURFACE;
   refused();
}
static void grain(void)
{
   setup();
   sequence.film_grain_params_present = 1;
   picture.grain.params.apply = 1;
   picture.grain.params.seed = 1234;
   picture.grain.params.scaling_shift = 8;
   picture.grain.params.ar_coeff_shift = 6;
   picture.grain.params.num_points[0] = 2;
   picture.grain.params.point_value[0][0] = 10;
   picture.grain.params.point_value[0][1] = 20;
   assert(build());
   assert(native.current_display_picture == 1000);
   assert(native.film_grain_info.grain_seed == 1234);
   saved = native;
   assert(!vaapi_av1_picture(&native, &sequence, &picture, surfaces, 32, 100));
   assert(!memcmp(&native, &saved, sizeof(native)));
   picture.grain.params.point_value[0][1] = 10;
   refused();
}
static void invalid(void)
{
   setup(); sequence.bit_depth = 12; refused();
   setup(); picture.slot = 32; refused();
   setup(); picture.reference_slots[0] = 0; refused();
   setup(); picture.reference_slots[0] = 32; refused();
   setup(); picture.picture.delta_q_y_dc = -65; refused();
   setup(); picture.picture.loop_filter_level[0] = 64; refused();
   setup(); picture.picture.cdef_y_sec[0] = 3; refused();
   setup(); picture.picture.restoration_type[0] = RAV1_RESTORE_WIENER;
   picture.picture.restoration_size[0] = 96; refused();
   setup(); picture.picture.restoration_type[1] = RAV1_RESTORE_WIENER;
   picture.picture.restoration_size[0] = 128;
   picture.picture.restoration_size[1] = 0x80000040u;
   picture.picture.restoration_size[2] = 0x80000040u;
   refused();
   setup(); picture.picture.reference.segment_enabled[0] = 1;
   picture.picture.reference.segment_data[0][0] = 256; refused();
   setup(); picture.tiles.mi_col_starts[1] = 47; refused();
   setup(); picture.motion.global.type[0] = RAV1_GLOBAL_AFFINE;
   picture.motion.global.params[0][2] = 65536 + 8193; refused();
   setup(); picture.header.frame_type = RAV1_FRAME_INTER;
   picture.header.frame_is_intra = 0; refused();
   setup();
   assert(!vaapi_av1_picture(NULL, &sequence, &picture, surfaces, 32, 1000));
   assert(!vaapi_av1_picture(&native, NULL, &picture, surfaces, 32, 1000));
   assert(!vaapi_av1_picture(&native, &sequence, NULL, surfaces, 32, 1000));
   assert(!vaapi_av1_picture(&native, &sequence, &picture, NULL, 32, 1000));
   assert(!vaapi_av1_picture(&native, &sequence, &picture, surfaces, 8, 1000));
   assert(!memcmp(&native, &saved, sizeof(native)));
}
int main(void)
{
   fields();
   invalid();
   grain();
   references();
   motion();
   puts("PASS: VA AV1 picture fields and unchanged malformed-input refusal; no device execution");
   return 0;
}
