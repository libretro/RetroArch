/* SPDX-License-Identifier: MIT */
#include <stdio.h>
#include <stdlib.h>
#include "../../../gfx/common/vaapi_av1.h"
#if RARCH_VAAPI_AV1_PARAMS_AVAILABLE
static rav1_sequence_t seq;
static rav1_hw_picture_t picture;
static rav1_hw_session_t session;
static vaapi_av1_params_t params, before;
static VASliceParameterBufferAV1 slices[4096], saved[4096];
static VASurfaceID surfaces[32];
static unsigned failures, frames, models[4], invalid_models;
static const uint8_t bytes[] = {0x32, 8, 0, 0, 0, 0, 0, 0, 0, 0};
#define CHECK(x)                                                               \
   do                                                                          \
   {                                                                           \
      if (!(x))                                                                \
      {                                                                        \
         fprintf(stderr, "line %u: %s\n", (unsigned)__LINE__, #x);             \
         failures++;                                                           \
      }                                                                        \
   } while (0)
static void reset(void)
{
   unsigned i;
   memset(&seq, 0, sizeof(seq));
   memset(&picture, 0, sizeof(picture));
   seq.operating_point_count = 1;
   seq.bit_depth = 8;
   seq.subsampling_x = seq.subsampling_y = 1;
   seq.max_width = 192;
   seq.max_height = 128;
   seq.frame_width_bits = 8;
   seq.frame_height_bits = 7;
   picture.header.frame_is_intra = picture.header.show_frame = 1;
   picture.header.primary_ref_frame = 7;
   picture.header.refresh_frame_flags = 255;
   picture.setup.size.width = picture.setup.size.upscaled_width =
      picture.setup.size.render_width = 192;
   picture.setup.size.height = picture.setup.size.render_height = 128;
   picture.setup.size.mi_cols = 48;
   picture.setup.size.mi_rows = 32;
   picture.setup.size.superres_denom = 8;
   picture.tiles.layout.columns = picture.tiles.layout.rows = 1;
   picture.tiles.mi_col_starts[1] = 48;
   picture.tiles.mi_row_starts[1] = 32;
   picture.picture.cdef_damping = 3;
   for (i = 0; i < 32; i++)
      surfaces[i] = 100 + i;
   for (i = 0; i < 8; i++)
      picture.reference_slots[i] = -1;
   for (i = 0; i < 7; i++)
      picture.motion.global.params[i][2] = picture.motion.global.params[i][5] =
         65536;
   CHECK(rav1_tile_group_init(&picture.tile_group, bytes + 3, sizeof(bytes) - 3,
                              &picture.tiles.layout, true));
}
static void reject(void)
{
   memset(&params, 0xa5, sizeof(params));
   memset(slices, 0x5a, sizeof(slices));
   before = params;
   memcpy(saved, slices, sizeof(saved));
   CHECK(!vaapi_av1_picture(&params, slices, 4096, bytes, sizeof(bytes), &seq,
                            &picture, surfaces, 32));
   CHECK(!memcmp(&params, &before, sizeof(params)));
   CHECK(!memcmp(slices, saved, sizeof(slices)));
}
static void grain_reset(void)
{
   rav1_grain_t *g;
   unsigned plane, i;
   reset();
   seq.film_grain_params_present = 1;
   g = &picture.grain.params;
   g->apply = g->overlap = g->clip_to_restricted_range = 1;
   g->seed = 65535;
   g->scaling_shift = 11;
   g->ar_coeff_lag = 3;
   g->ar_coeff_shift = 9;
   g->scale_shift = 3;
   for (plane = 0; plane < 3; plane++)
   {
      g->num_points[plane] = plane ? 10 : 14;
      for (i = 0; i < 14; i++)
      {
         g->point_value[plane][i] = (uint8_t)(i * 17);
         g->point_scaling[plane][i] = (uint8_t)(255 - i);
      }
      for (i = 0; i < 25; i++)
         g->ar_coeffs[plane][i] = (int8_t)(i % 2 ? 127 : -128);
   }
   for (plane = 0; plane < 2; plane++)
   {
      g->chroma_mult[plane] = 255;
      g->chroma_luma_mult[plane] = 254;
      g->chroma_offset[plane] = 511;
   }
}
static void grain_reject(VASurfaceID display)
{
   memset(&params, 0xa5, sizeof(params));
   memset(slices, 0x5a, sizeof(slices));
   before = params;
   memcpy(saved, slices, sizeof(saved));
   CHECK(!vaapi_av1_picture_display(&params, slices, 4096, bytes,
         sizeof(bytes), &seq, &picture, surfaces, 32, display));
   CHECK(!memcmp(&params, &before, sizeof(params)));
   CHECK(!memcmp(slices, saved, sizeof(slices)));
}
static void test_grain(void)
{
   VAFilmGrainStructAV1 *v = &params.picture.film_grain_info;
   rav1_grain_t *g = &picture.grain.params;
   unsigned i, plane;
   grain_reset();
   CHECK(vaapi_av1_picture_display(&params, slices, 4096, bytes,
         sizeof(bytes), &seq, &picture, surfaces, 32, 999));
   CHECK(params.target == 100 && params.picture.current_frame == 100 &&
         params.picture.current_display_picture == 999);
   CHECK(v->film_grain_info_fields.value == 0xffd && v->grain_seed == 65535);
   CHECK(v->num_y_points == 14 && v->num_cb_points == 10 &&
         v->num_cr_points == 10);
   CHECK(v->point_y_value[13] == 221 && v->point_y_scaling[13] == 242 &&
         v->point_cb_value[9] == 153 && v->point_cr_scaling[9] == 246);
   CHECK(v->ar_coeffs_y[23] == 127 && v->ar_coeffs_cb[24] == -128 &&
         v->ar_coeffs_cr[24] == -128);
   CHECK(v->cb_mult == 255 && v->cr_mult == 255 && v->cb_luma_mult == 254 &&
         v->cr_luma_mult == 254 && v->cb_offset == 511 && v->cr_offset == 511);
   for (i = 0; i < VA_PADDING_LOW; i++)
      CHECK(!v->va_reserved[i]);
   reject();
   grain_reject(VA_INVALID_SURFACE);
   for (i = 0; i < 32; i++)
      grain_reject(surfaces[i]);
   grain_reset(); seq.film_grain_params_present = 0; grain_reject(999);
   grain_reset(); picture.header.show_frame = 0; grain_reject(999);
   grain_reset(); picture.header.show_frame = 0;
   picture.header.showable_frame = 1;
   CHECK(vaapi_av1_picture_display(&params, slices, 4096, bytes,
         sizeof(bytes), &seq, &picture, surfaces, 32, 999));
   grain_reset(); g->apply = 2; grain_reject(999);
   grain_reset(); g->seed = 65536; grain_reject(999);
   grain_reset(); g->scaling_shift = 7; grain_reject(999);
   grain_reset(); g->scaling_shift = 12; grain_reject(999);
   grain_reset(); g->ar_coeff_lag = 4; grain_reject(999);
   grain_reset(); g->ar_coeff_shift = 5; grain_reject(999);
   grain_reset(); g->ar_coeff_shift = 10; grain_reject(999);
   grain_reset(); g->scale_shift = 4; grain_reject(999);
   grain_reset(); g->overlap = 2; grain_reject(999);
   grain_reset(); g->clip_to_restricted_range = 2; grain_reject(999);
   grain_reset(); g->chroma_scaling_from_luma = 2; grain_reject(999);
   grain_reset(); g->chroma_scaling_from_luma = 1; grain_reject(999);
   grain_reset(); g->num_points[0] = 0; grain_reject(999);
   grain_reset(); g->num_points[1] = 0; grain_reject(999);
   grain_reset(); g->num_points[2] = 0; grain_reject(999);
   for (plane = 0; plane < 3; plane++)
   {
      grain_reset(); g->num_points[plane] = plane ? 11 : 15;
      grain_reject(999);
      grain_reset(); g->point_value[plane][1] = g->point_value[plane][0];
      grain_reject(999);
      grain_reset(); g->point_value[plane][1] = 0;
      g->point_value[plane][0] = 1; grain_reject(999);
   }
   for (plane = 0; plane < 2; plane++)
   {
      grain_reset(); g->chroma_mult[plane] = 256; grain_reject(999);
      grain_reset(); g->chroma_luma_mult[plane] = 256; grain_reject(999);
      grain_reset(); g->chroma_offset[plane] = 512; grain_reject(999);
   }
   grain_reset();
   g->num_points[1] = g->num_points[2] = 0;
   g->num_points[0] = 1;
   g->chroma_scaling_from_luma = 1;
   g->ar_coeff_lag = 0;
   CHECK(vaapi_av1_picture_display(&params, slices, 4096, bytes,
         sizeof(bytes), &seq, &picture, surfaces, 32, 999));
   CHECK(!v->ar_coeffs_y[0] && v->ar_coeffs_cb[0] == -128 &&
         v->ar_coeffs_cr[0] == -128 && !v->ar_coeffs_cb[1] &&
         !v->point_y_value[1] && !v->point_cb_scaling[0] && !v->cb_mult);
   g->num_points[0] = 0;
   CHECK(vaapi_av1_picture_display(&params, slices, 4096, bytes,
         sizeof(bytes), &seq, &picture, surfaces, 32, 999));
   CHECK(!v->ar_coeffs_cb[0]);
   g->apply = 0;
   grain_reject(999);
   CHECK(vaapi_av1_picture(&params, slices, 4096, bytes, sizeof(bytes), &seq,
         &picture, surfaces, 32));
   for (i = 0; i < sizeof(*v); i++)
      CHECK(!((const uint8_t *)v)[i]);
}
static void manual(void)
{
   static const uint8_t tiled[] = {0x32, 8,    0,    0,    1,
                                   0xaa, 0xbb, 0xcc, 0xdd, 0xee};
   reset();
   CHECK(vaapi_av1_picture(&params, slices, 4096, bytes, sizeof(bytes), &seq,
                           &picture, surfaces, 32));
   CHECK(params.target == 100 && params.picture.current_frame == 100 &&
         params.tile_count == 1);
   CHECK(params.picture.current_display_picture == VA_INVALID_SURFACE);
   CHECK(params.picture.frame_width_minus1 == 191 &&
         params.picture.frame_height_minus1 == 127);
   CHECK(slices[0].slice_data_size == 7 && slices[0].slice_data_offset == 3);
   reset();
   picture.grain.params.apply = 1;
   reject();
   reset();
   picture.motion.global.type[0] = RAV1_GLOBAL_AFFINE;
   reject();
   reset();
   picture.picture.delta_q_y_dc = -65;
   reject();
   reset();
   picture.picture.reference.segment_data[0][0] = 256;
   reject();
   reset();
   picture.picture.cdef_y_sec[0] = 3;
   reject();
   reset();
   surfaces[1] = surfaces[0];
   reject();
   reset();
   picture.setup.size.width--;
   reject();
   reset();
   seq.bit_depth = 12;
   reject();
   reset();
   picture.header.show_existing = 1;
   reject();
   reset();
   before = params;
   CHECK(!vaapi_av1_picture(&params, (VASliceParameterBufferAV1 *)&params, 1,
                            bytes, sizeof(bytes), &seq, &picture, surfaces,
                            32));
   CHECK(!memcmp(&params, &before, sizeof(params)));
   reset();
   picture.tiles.layout.columns = 2;
   picture.tiles.layout.columns_log2 = picture.tiles.layout.size_bytes = 1;
   picture.tiles.mi_col_starts[1] = 32;
   picture.tiles.mi_col_starts[2] = 48;
   CHECK(rav1_tile_group_init(&picture.tile_group, tiled + 3, sizeof(tiled) - 3,
                              &picture.tiles.layout, true));
   CHECK(vaapi_av1_picture(&params, slices, 4096, tiled, sizeof(tiled), &seq,
                           &picture, surfaces, 32));
   CHECK(params.tile_count == 2 && slices[0].slice_data_offset == 5 &&
         slices[0].slice_data_size == 2);
   CHECK(slices[1].slice_data_offset == 7 && slices[1].slice_data_size == 3);
   CHECK(params.picture.width_in_sbs_minus_1[0] == 1 &&
         !params.picture.width_in_sbs_minus_1[1]);
   seq.bit_depth = 10;
   CHECK(vaapi_av1_picture(&params, slices, 4096, tiled, sizeof(tiled), &seq,
                           &picture, surfaces, 32));
   CHECK(params.picture.bit_depth_idx == 1);
}
static void test_inter(void)
{
   unsigned i;
   reset();
   seq.bit_depth = 10;
   seq.color_range = seq.enable_cdef = seq.enable_restoration = 1;
   picture.header.frame_type = RAV1_FRAME_INTER;
   picture.header.frame_is_intra = 0;
   picture.header.primary_ref_frame = 0;
   picture.setup.valid_mask = 255;
   for (i = 0; i < 8; i++)
   {
      picture.reference_slots[i] = i + 1;
      picture.reference_info[i].valid = 1;
   }
   for (i = 0; i < 7; i++)
      picture.setup.ref_frame_idx[i] = i;
   picture.picture.segmentation_enabled = 1;
   picture.picture.reference.segment_enabled[0] = 255;
   picture.picture.reference.segment_data[0][0] = -255;
   picture.picture.reference.segment_data[0][1] = 63;
   picture.picture.reference.segment_data[0][5] = 7;
   picture.picture.reference.loop_filter_ref_deltas[0] = -64;
   picture.picture.reference.loop_filter_mode_deltas[1] = 63;
   picture.picture.delta_q_y_dc = -64;
   picture.picture.delta_q_u_ac = 63;
   picture.picture.using_qmatrix = 1;
   picture.picture.qm_u = 15;
   picture.picture.cdef_bits = 3;
   picture.picture.cdef_damping = 6;
   picture.picture.cdef_y_pri[0] = 15;
   picture.picture.cdef_y_sec[0] = 4;
   picture.picture.restoration_type[0] = RAV1_RESTORE_WIENER;
   picture.picture.restoration_type[1] = RAV1_RESTORE_SGRPROJ;
   picture.picture.restoration_type[2] = RAV1_RESTORE_SWITCHABLE;
   picture.picture.restoration_size[0] = 256;
   picture.picture.restoration_size[1] = picture.picture.restoration_size[2] =
      128;
   picture.motion.global.type[0] = RAV1_GLOBAL_TRANSLATION;
   picture.motion.global.params[0][0] = 8192;
   CHECK(vaapi_av1_picture(&params, slices, 4096, bytes, sizeof(bytes), &seq,
                           &picture, surfaces, 32));
   CHECK(params.picture.ref_frame_map[0] == 101 &&
         params.picture.ref_frame_map[7] == 108);
   CHECK(params.picture.ref_frame_idx[6] == 6);
   CHECK(params.picture.seq_info_fields.fields.color_range);
   CHECK(params.picture.seg_info.feature_mask[0] == 255 &&
         params.picture.seg_info.feature_data[0][0] == -255 &&
         params.picture.seg_info.feature_data[0][5] == 7);
   CHECK(params.picture.ref_deltas[0] == -64 &&
         params.picture.mode_deltas[1] == 63);
   CHECK(params.picture.y_dc_delta_q == -64 &&
         params.picture.u_ac_delta_q == 63);
   CHECK(params.picture.qmatrix_fields.bits.using_qmatrix &&
         params.picture.qmatrix_fields.bits.qm_u == 15);
   CHECK(params.picture.cdef_y_strengths[0] == 63 &&
         params.picture.cdef_damping_minus_3 == 3);
   CHECK(params.picture.loop_restoration_fields.bits.lr_unit_shift == 2 &&
         params.picture.loop_restoration_fields.bits.lr_uv_shift == 1 &&
         params.picture.loop_restoration_fields.bits.crframe_restoration_type ==
            3);
   CHECK(params.picture.wm[0].wmtype == VAAV1TransformationTranslation &&
         params.picture.wm[0].wmmat[0] == 8192);
   picture.setup.ref_frame_idx[0] = 8;
   reject();
}

static void test_bounds(void)
{
   unsigned i;
   for (i = 0; i < 30; i++)
   {
      reset();
      switch (i)
      {
      case 0:
         seq.profile = 1;
         break;
      case 1:
         seq.monochrome = 1;
         break;
      case 2:
         seq.subsampling_x = 0;
         break;
      case 3:
         seq.operating_point_count = 2;
         break;
      case 4:
         seq.operating_points[0].idc = 1;
         break;
      case 5:
         seq.operating_points[0].level = 24;
         break;
      case 6:
         seq.frame_width_bits = 0;
         break;
      case 7:
         seq.max_width = 65537;
         break;
      case 8:
         seq.enable_cdef = 2;
         break;
      case 9:
         picture.header.show_frame = 2;
         break;
      case 10:
         picture.header.order_hint = 256;
         break;
      case 11:
         picture.header.primary_ref_frame = 8;
         break;
      case 12:
         picture.header.refresh_frame_flags = 1;
         break;
      case 13:
         picture.reference_slots[0] = -2;
         break;
      case 14:
         picture.reference_slots[0] = 32;
         break;
      case 15:
         picture.reference_slots[0] = 0;
         break;
      case 16:
         picture.setup.size.mi_cols--;
         break;
      case 17:
         picture.setup.size.render_width--;
         break;
      case 18:
         picture.tile_group.offset = 1;
         break;
      case 19:
         picture.tile_group.next = 1;
         break;
      case 20:
         picture.tiles.mi_col_starts[1]++;
         break;
      case 21:
         picture.picture.delta_q_res = 4;
         break;
      case 22:
         picture.picture.loop_filter_level[0] = 64;
         break;
      case 23:
         picture.picture.reference.loop_filter_ref_deltas[0] = -65;
         break;
      case 24:
         picture.picture.reference.segment_data[0][5] = -1;
         break;
      case 25:
         picture.picture.qm_y = 16;
         break;
      case 26:
         picture.picture.cdef_damping = 7;
         break;
      case 27:
         picture.motion.global.params[0][2] = 65535;
         break;
      case 28:
         surfaces[0] = VA_INVALID_SURFACE;
         break;
      case 29:
         picture.picture.restoration_type[0] = 1;
         break;
      }
      reject();
   }
}

static void test_global(void)
{
   unsigned i;
   reset();
   picture.header.frame_type = RAV1_FRAME_INTER;
   picture.header.frame_is_intra = 0;
   picture.header.primary_ref_frame = 0;
   picture.setup.valid_mask = 255;
   for (i = 0; i < 8; i++)
   {
      picture.reference_slots[i] = i + 1;
      picture.reference_info[i].valid = 1;
   }
   for (i = 0; i < 7; i++) picture.setup.ref_frame_idx[i] = i;
   picture.motion.global.type[0] = RAV1_GLOBAL_ROTZOOM;
   picture.motion.global.params[0][0] = 1024;
   picture.motion.global.params[0][1] = -2048;
   picture.motion.global.params[0][2] = picture.motion.global.params[0][5] = 65538;
   picture.motion.global.params[0][3] = 2;
   picture.motion.global.params[0][4] = -2;
   CHECK(vaapi_av1_picture(&params, slices, 4096, bytes, sizeof(bytes), &seq,
         &picture, surfaces, 32));
   CHECK(params.picture.wm[0].wmtype == VAAV1TransformationRotzoom
         && !params.picture.wm[0].invalid);
   for (i = 0; i < 6; i++)
      CHECK(params.picture.wm[0].wmmat[i] == picture.motion.global.params[0][i]);
   picture.motion.global.params[0][5] = 65536;
   reject();
   picture.motion.global.type[0] = RAV1_GLOBAL_AFFINE;
   picture.motion.global.params[0][2] = 73728;
   picture.motion.global.params[0][3] = 8192;
   picture.motion.global.params[0][4] = -8192;
   picture.motion.global.params[0][5] = 57344;
   CHECK(vaapi_av1_picture(&params, slices, 4096, bytes, sizeof(bytes), &seq,
         &picture, surfaces, 32));
   CHECK(params.picture.wm[0].wmtype == VAAV1TransformationAffine
         && params.picture.wm[0].invalid == 1);
   picture.motion.global.params[0][0] = picture.motion.global.params[0][1] = 0;
   picture.motion.global.params[0][2] = 57604;
   picture.motion.global.params[0][3] = -3794;
   picture.motion.global.params[0][4] = 7902;
   picture.motion.global.params[0][5] = 57624;
   CHECK(vaapi_av1_picture(&params, slices, 4096, bytes, sizeof(bytes), &seq,
         &picture, surfaces, 32));
   CHECK(!params.picture.wm[0].invalid);
   picture.motion.global.params[0][2]++;
   reject();
   picture.motion.global.params[0][2] = 65536;
   picture.motion.global.params[0][4] = -8194;
   reject();
   picture.motion.global.params[0][4] = 0;
   picture.motion.global.params[0][0] = 1025;
   reject();
}

static uint32_t global_seed;
static uint32_t global_word(void)
{
   global_seed = global_seed * UINT32_C(1664525) + UINT32_C(1013904223);
   return global_seed;
}
static void global_corpus(void)
{
   int32_t m[6];
   uint32_t hash = UINT32_C(2166136261);
   unsigned i, type, invalid, bad = 0;
   global_seed = UINT32_C(0x734a11);
   for (i = 0; i < 200000; i++)
   {
      type = 2 + (global_word() & 1);
      m[0] = ((int)(global_word() % 8193) - 4096) * 1024;
      m[1] = ((int)(global_word() % 8193) - 4096) * 1024;
      m[2] = 57344 + (int)(global_word() % 8193) * 2;
      m[3] = -8192 + (int)(global_word() % 8193) * 2;
      m[4] = -8192 + (int)(global_word() % 8193) * 2;
      m[5] = 57344 + (int)(global_word() % 8193) * 2;
      if (type == RAV1_GLOBAL_ROTZOOM)
      {
         m[4] = -m[3];
         m[5] = m[2];
      }
      CHECK(vaapi_av1_global_valid(type, m));
      invalid = vaapi_av1_global_invalid(m);
      CHECK(invalid <= 1);
      bad += invalid;
      hash = (hash ^ (invalid + type * 2)) * UINT32_C(16777619);
   }
   /* Frozen normative table oracle, including signed rounding and clipping. */
   CHECK(bad == 32519);
   CHECK(hash == UINT32_C(0x4169ab20));
}

static int map(void *owner, const uint8_t *packet, size_t size,
               const rav1_sequence_t *s, const rav1_hw_picture_t *p)
{
   unsigned i;
   (void)owner;
   if (!vaapi_av1_picture(&params, slices, 4096, packet, size, s, p, surfaces,
                          32))
      return -1;
   CHECK(params.target == surfaces[p->slot]);
   CHECK(params.tile_count == p->tiles.layout.rows * p->tiles.layout.columns);
   for (i = 0; i < params.tile_count; i++)
      CHECK(slices[i].slice_data_size && slices[i].slice_data_offset < size &&
            slices[i].slice_data_size <= size - slices[i].slice_data_offset);
   for (i = 0; i < 7; i++)
   {
      models[p->motion.global.type[i]]++;
      invalid_models += params.picture.wm[i].invalid;
   }
   frames++;
   return 0;
}
static int output(void *owner, unsigned slot, const rav1_grain_t *grain,
                  video_hw_frame_t *frame)
{
   (void)owner;
   (void)slot;
   (void)grain;
   (void)frame;
   return -1;
}
static int inspect(const char *path)
{
   FILE *file = fopen(path, "rb");
   uint8_t *data;
   long length;
   size_t offset = 0;
   rav1_obu_t obu;
   rav1_hw_ops_t ops;
   int ret, initialized = 0;
   unsigned i;
   if (!file)
      return 1;
   CHECK(!fseek(file, 0, SEEK_END));
   length = ftell(file);
   CHECK(length > 0 && length < 64 * 1024 * 1024);
   if (length <= 0 || length >= 64 * 1024 * 1024)
   {
      fclose(file);
      return 1;
   }
   CHECK(!fseek(file, 0, SEEK_SET));
   data = (uint8_t *)malloc((size_t)length);
   CHECK(data != NULL);
   if (!data)
   {
      fclose(file);
      return 1;
   }
   CHECK(fread(data, 1, (size_t)length, file) == (size_t)length);
   fclose(file);
   for (i = 0; i < 32; i++)
      surfaces[i] = 100 + i;
   memset(&ops, 0, sizeof(ops));
   ops.decode = map;
   ops.output = output;
   while ((ret = rav1_obu_next(&obu, data, (size_t)length, &offset, false)) ==
          1)
   {
      if (obu.type == 1)
      {
         CHECK(rav1_sequence_parse(&seq, obu.payload, obu.payload_size));
         if (!initialized)
         {
            CHECK(rav1_hw_session_init(&session, &seq, &ops, 9));
            initialized = 1;
         }
      }
      if (obu.type == 3 || obu.type == 6)
      {
         if (!initialized ||
             rav1_hw_session_submit(&session, obu.data, obu.size))
         {
            CHECK(0);
            break;
         }
         rav1_hw_session_discard_output(&session);
      }
   }
   CHECK(ret == 0 && frames);
   free(data);
   printf("AV1 global identity=%u translation=%u rotzoom=%u affine=%u invalid=%u\n",
         models[0], models[1], models[2], models[3], invalid_models);
   printf("AV1 VA typed parsed pictures=%u failures=%u\n", frames, failures);
   return failures ? 1 : 0;
}
int main(int argc, char **argv)
{
   if (argc == 2)
      return inspect(argv[1]);
   manual();
   test_grain();
   test_inter();
   test_global();
   global_corpus();
   test_bounds();
   printf("AV1 VA parameters: %u failures\n", failures);
   return failures ? 1 : 0;
}
#else
int main(void)
{
   puts("AV1 VA parameters: SKIP (SDK lacks AV1)");
   return 0;
}
#endif
