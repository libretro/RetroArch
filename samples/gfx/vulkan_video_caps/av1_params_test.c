/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include <stdio.h>
#include <stdlib.h>
#include "../../../gfx/common/vulkan_av1_params.h"
#include "av1_session_data.h"

static vulkan_av1_sequence_t sequence_params;
static vulkan_av1_picture_t params, empty;
static rav1_sequence_t av1_sequence;
static rav1_hw_picture_t picture;
static rav1_hw_session_t av1_session;
static unsigned failures, frames, verbose;
static const uint8_t bytes[] = {0x32, 8, 0, 0, 0, 0, 0, 0, 0, 0};

#define CHECK(c)                                                               \
   do                                                                          \
   {                                                                           \
      if (!(c))                                                                \
      {                                                                        \
         fprintf(stderr, "line %u: %s\n", (unsigned)__LINE__, #c);             \
         failures++;                                                           \
      }                                                                        \
   } while (0)

static void reset_params(void)
{
   unsigned i;
   memset(&av1_sequence, 0, sizeof(av1_sequence));
   memset(&picture, 0, sizeof(picture));
   av1_sequence.operating_point_count = 1;
   av1_sequence.bit_depth = 8;
   av1_sequence.subsampling_x = av1_sequence.subsampling_y = 1;
   av1_sequence.max_width = 192;
   av1_sequence.max_height = 128;
   av1_sequence.frame_width_bits = 8;
   av1_sequence.frame_height_bits = 7;
   picture.header.frame_is_intra = 1;
   picture.header.primary_ref_frame = 7;
   picture.header.refresh_frame_flags = 255;
   picture.setup.size.width = picture.setup.size.upscaled_width = 192;
   picture.setup.size.height = 128;
   picture.setup.size.render_width = 192;
   picture.setup.size.render_height = 128;
   picture.setup.size.superres_denom = 8;
   picture.tiles.layout.columns = picture.tiles.layout.rows = 1;
   picture.tiles.mi_col_starts[1] = 48;
   picture.tiles.mi_row_starts[1] = 32;
   picture.picture.cdef_damping = 3;
   for (i = 0; i < 8; i++)
      picture.reference_slots[i] = -1;
   CHECK(rav1_tile_group_init(&picture.tile_group, bytes + 3, sizeof(bytes) - 3,
                              &picture.tiles.layout, true));
}

static void test_sequence(void)
{
   unsigned i;
   reset_params();
   av1_sequence.bit_depth = 10;
   av1_sequence.enable_order_hint = 1;
   av1_sequence.order_hint_bits = 8;
   av1_sequence.frame_id_numbers_present = 1;
   av1_sequence.delta_frame_id_length = 4;
   av1_sequence.frame_id_length = 9;
   av1_sequence.force_integer_mv = av1_sequence.force_screen_content_tools = 2;
   av1_sequence.timing_info_present = av1_sequence.equal_picture_interval = 1;
   av1_sequence.num_units_in_display_tick = 1001;
   av1_sequence.time_scale = 24000;
   av1_sequence.num_ticks_per_picture_minus_one = 2;
   CHECK(vulkan_av1_sequence(&sequence_params, &av1_sequence));
   CHECK(sequence_params.header.pColorConfig == &sequence_params.color);
   CHECK(sequence_params.header.pTimingInfo == &sequence_params.timing);
   CHECK(sequence_params.header.max_frame_width_minus_1 == 191);
   CHECK(sequence_params.header.max_frame_height_minus_1 == 127);
   CHECK(sequence_params.header.delta_frame_id_length_minus_2 == 2);
   CHECK(sequence_params.header.additional_frame_id_length_minus_1 == 4);
   CHECK(sequence_params.header.order_hint_bits_minus_1 == 7);
   CHECK(sequence_params.header.seq_force_integer_mv == 2);
   CHECK(sequence_params.color.BitDepth == 10 &&
         sequence_params.color.subsampling_x == 1);
   CHECK(sequence_params.timing.time_scale == 24000 &&
         sequence_params.timing.num_ticks_per_picture_minus_1 == 2);
   av1_sequence.frame_id_length = 4;
   CHECK(!vulkan_av1_sequence(&sequence_params, &av1_sequence));
   reset_params();
   av1_sequence.max_width = av1_sequence.max_height = 65536;
   av1_sequence.frame_width_bits = av1_sequence.frame_height_bits = 16;
   CHECK(vulkan_av1_sequence(&sequence_params, &av1_sequence));
   CHECK(sequence_params.header.max_frame_width_minus_1 == 65535);
   av1_sequence.bit_depth = 12;
   CHECK(!vulkan_av1_sequence(&sequence_params, &av1_sequence));
   reset_params();
   av1_sequence.operating_points[0].idc = 1;
   CHECK(!vulkan_av1_sequence(&sequence_params, &av1_sequence));
   for (i = 0;
        i < sizeof(av1_sequence_fixtures) / sizeof(av1_sequence_fixtures[0]);
        i++)
   {
      const struct av1_sequence_fixture *f = &av1_sequence_fixtures[i];
      CHECK(rav1_sequence_parse(&av1_sequence, f->data, f->size));
      CHECK(vulkan_av1_sequence(&sequence_params, &av1_sequence));
      CHECK(sequence_params.color.BitDepth == f->depth);
      CHECK(sequence_params.header.flags.film_grain_params_present == f->grain);
      CHECK(sequence_params.header.flags.reduced_still_picture_header ==
            (i % 3 == 1));
   }
}

static void test_tiles(void)
{
   static const uint8_t tiled[] = {0x32, 8,    0,    0,    1,
                                   0xaa, 0xbb, 0xcc, 0xdd, 0xee};
   reset_params();
   picture.tiles.layout.columns = 2;
   picture.tiles.layout.columns_log2 = picture.tiles.layout.size_bytes = 1;
   picture.tiles.mi_col_starts[1] = 32;
   picture.tiles.mi_col_starts[2] = 48;
   CHECK(rav1_tile_group_init(&picture.tile_group, tiled + 3, sizeof(tiled) - 3,
                              &picture.tiles.layout, true));
   CHECK(vulkan_av1_picture(&params, tiled, sizeof(tiled), &av1_sequence,
                            &picture));
   CHECK(params.info.tileCount == 2 && params.offsets[0] == 5 &&
         params.offsets[1] == 7);
   CHECK(params.sizes[0] == 2 && params.sizes[1] == 3);
   CHECK(params.widths[0] == 1 && params.widths[1] == 0);
   av1_sequence.use_128x128_superblock = 1;
   CHECK(vulkan_av1_picture(&params, tiled, sizeof(tiled), &av1_sequence,
                            &picture));
   CHECK(!params.widths[0] && !params.widths[1] && !params.heights[0]);
   picture.tile_group.size = 3;
   CHECK(!vulkan_av1_picture(&params, tiled, sizeof(tiled), &av1_sequence,
                             &picture));
   CHECK(!memcmp(&params, &empty, sizeof(params)));
   picture.tile_group.size = sizeof(tiled) - 3;
   picture.tile_group.next = 1;
   CHECK(!vulkan_av1_picture(&params, tiled, sizeof(tiled), &av1_sequence,
                             &picture));
}

static void test_unsized(void)
{
   static const uint8_t tiled[] = {0x30, 0, 0, 1, 0xaa, 0xbb, 0xcc, 0xdd, 0xee};
   static const uint8_t extended[] = {
      0x34, 0, 0, 0, 1, 0xaa, 0xbb, 0xcc, 0xdd, 0xee
   };
   reset_params();
   picture.tiles.layout.columns = 2;
   picture.tiles.layout.columns_log2 = picture.tiles.layout.size_bytes = 1;
   picture.tiles.mi_col_starts[1] = 32;
   picture.tiles.mi_col_starts[2] = 48;
   CHECK(rav1_tile_group_init(&picture.tile_group, tiled + 2, sizeof(tiled) - 2,
         &picture.tiles.layout, true));
   CHECK(vulkan_av1_picture(&params, tiled, sizeof(tiled), &av1_sequence,
         &picture));
   CHECK(params.info.tileCount == 2 && params.offsets[0] == 4 &&
         params.offsets[1] == 6 && params.sizes[0] == 2 && params.sizes[1] == 3);
   CHECK(rav1_tile_group_init(&picture.tile_group, extended + 3,
         sizeof(extended) - 3, &picture.tiles.layout, true));
   CHECK(vulkan_av1_picture(&params, extended, sizeof(extended), &av1_sequence,
         &picture));
   CHECK(params.offsets[0] == 5 && params.offsets[1] == 7);
   CHECK(!vulkan_av1_picture(&params, extended, sizeof(extended) - 1,
         &av1_sequence, &picture));
   CHECK(!memcmp(&params, &empty, sizeof(params)));
}

static void test_picture(void)
{
   unsigned i;
   reset_params();
   CHECK(vulkan_av1_picture(&params, bytes, sizeof(bytes), &av1_sequence,
                            &picture));
   CHECK(params.info.pStdPictureInfo == &params.header &&
         params.info.tileCount == 1);
   CHECK(params.offsets[0] == 3 && params.sizes[0] == 7);
   CHECK(params.tiles.pMiColStarts[1] == 48 &&
         params.tiles.pMiRowStarts[1] == 32);
   CHECK(params.tiles.pWidthInSbsMinus1[0] == 2 &&
         params.tiles.pHeightInSbsMinus1[0] == 1);
   CHECK(params.header.primary_ref_frame == 7 &&
         params.header.refresh_frame_flags == 255);
   picture.picture.cdef_y_sec[0] = 4;
   picture.picture.cdef_uv_sec[0] = 2;
   picture.picture.restoration_type[0] = RAV1_RESTORE_WIENER;
   picture.picture.restoration_type[2] = RAV1_RESTORE_SGRPROJ;
   picture.picture.restoration_size[0] = 256;
   picture.picture.restoration_size[1] = picture.picture.restoration_size[2] =
       128;
   picture.picture.diff_uv_delta = 1;
   picture.picture.loop_filter_update_ref_delta = 0x81;
   picture.picture.loop_filter_update_mode_delta = 2;
   picture.picture.reference.segment_enabled[3] = 0x85;
   picture.picture.reference.segment_data[3][0] = -255;
   picture.picture.reference.loop_filter_ref_deltas[7] = -63;
   picture.motion.global.type[6] = RAV1_GLOBAL_AFFINE;
   picture.motion.global.params[6][0] = -12345;
   CHECK(vulkan_av1_picture(&params, bytes, sizeof(bytes), &av1_sequence,
                            &picture));
   CHECK(params.cdef.cdef_y_sec_strength[0] == 3 &&
         params.cdef.cdef_uv_sec_strength[0] == 2);
   CHECK(params.restoration.LoopRestorationSize[0] == 3 &&
         params.restoration.LoopRestorationSize[2] == 2);
   CHECK(params.header.flags.UsesLr && params.header.flags.usesChromaLr);
   CHECK(params.quant.flags.diff_uv_delta &&
         params.filter.update_ref_delta == 0x81 &&
         params.filter.update_mode_delta == 2);
   CHECK(params.segments.FeatureEnabled[3] == 0x85 &&
         params.segments.FeatureData[3][0] == -255);
   CHECK(params.filter.loop_filter_ref_deltas[7] == -63);
   CHECK(params.global.GmType[7] == RAV1_GLOBAL_AFFINE &&
         params.global.gm_params[7][0] == -12345);
   picture.header.frame_is_intra = 0;
   picture.header.frame_type = RAV1_FRAME_INTER;
   av1_sequence.frame_id_numbers_present = 1;
   picture.reference_slots[5] = 9;
   picture.reference_info[5].valid = 1;
   picture.reference_info[5].frame_id = 99;
   for (i = 0; i < 7; i++)
   {
      picture.setup.ref_frame_idx[i] = 5;
      picture.setup.sign_bias[i] = i & 1;
      picture.order_hints[i + 1] = (uint8_t)(i + 10);
   }
   CHECK(vulkan_av1_picture(&params, bytes, sizeof(bytes), &av1_sequence,
                            &picture));
   CHECK(params.reference.RefFrameSignBias == 0x54);
   for (i = 0; i < 7; i++)
      CHECK(params.info.referenceNameSlotIndices[i] == 9 &&
            params.header.expectedFrameId[i] == 99 &&
            params.reference.SavedOrderHints[i + 1] == i + 10);
   picture.reference_slots[5] = picture.slot;
   CHECK(!vulkan_av1_picture(&params, bytes, sizeof(bytes), &av1_sequence,
                             &picture));
   CHECK(!memcmp(&params, &empty, sizeof(params)));
   reset_params();
   picture.tile_group.data = bytes + sizeof(bytes);
   CHECK(!vulkan_av1_picture(&params, bytes, sizeof(bytes), &av1_sequence,
                             &picture));
   reset_params();
   picture.tiles.mi_col_starts[1] = 47;
   CHECK(!vulkan_av1_picture(&params, bytes, sizeof(bytes), &av1_sequence,
                             &picture));
   CHECK(!memcmp(&params, &empty, sizeof(params)));
   reset_params();
   picture.tiles.layout.columns = 65;
   CHECK(!vulkan_av1_picture(&params, bytes, sizeof(bytes), &av1_sequence,
                             &picture));
   reset_params();
   picture.setup.size.width = 128;
   picture.setup.size.superres_denom = 12;
   picture.setup.size.render_width = 320;
   picture.tiles.mi_col_starts[1] = 32;
   CHECK(vulkan_av1_picture(&params, bytes, sizeof(bytes), &av1_sequence,
                            &picture));
   CHECK(params.header.flags.use_superres && params.header.coded_denom == 3 &&
         params.header.flags.render_and_frame_size_different);
}

static void test_grain(void)
{
   reset_params();
   av1_sequence.film_grain_params_present = 1;
   picture.grain.params.apply = 1;
   picture.grain.params.seed = 65535;
   picture.grain.params.scaling_shift = 9;
   picture.grain.params.ar_coeff_shift = 7;
   picture.grain.params.num_points[0] = 14;
   picture.grain.params.num_points[1] = picture.grain.params.num_points[2] = 10;
   picture.grain.params.point_value[0][13] = 250;
   picture.grain.params.ar_coeffs[0][0] = -128;
   picture.grain.params.ar_coeffs[0][1] = 127;
   picture.grain.params.ar_coeffs[1][24] = -1;
   picture.grain.params.chroma_offset[1] = 511;
   CHECK(vulkan_av1_picture(&params, bytes, sizeof(bytes), &av1_sequence,
                            &picture));
   CHECK(params.header.flags.apply_grain &&
         params.header.pFilmGrain == &params.grain);
   CHECK(params.grain.grain_seed == 65535 &&
         params.grain.grain_scaling_minus_8 == 1);
   CHECK(params.grain.ar_coeff_shift_minus_6 == 1 &&
         params.grain.point_y_value[13] == 250);
   CHECK((uint8_t)params.grain.ar_coeffs_y_plus_128[0] == 0 &&
         (uint8_t)params.grain.ar_coeffs_y_plus_128[1] == 255);
   CHECK((uint8_t)params.grain.ar_coeffs_cb_plus_128[24] == 127 &&
         params.grain.cr_offset == 511);
   picture.grain.params.num_points[1] = 11;
   CHECK(!vulkan_av1_picture(&params, bytes, sizeof(bytes), &av1_sequence,
                             &picture));
   CHECK(!memcmp(&params, &empty, sizeof(params)));
}

static int map_picture(void *owner, const uint8_t *packet, size_t size,
                       const rav1_sequence_t *seq, const rav1_hw_picture_t *p)
{
   unsigned i;
   (void)owner;
   if (!vulkan_av1_sequence(&sequence_params, seq) ||
       !vulkan_av1_picture(&params, packet, size, seq, p))
      return -1;
   if (verbose)
   {
      printf("AP %u %u %u %u %u %u %u %u %u %u",
             (unsigned)params.header.frame_type, params.header.OrderHint,
             params.header.primary_ref_frame, params.header.refresh_frame_flags,
             params.quant.flags.diff_uv_delta, params.filter.update_ref_delta,
             params.filter.update_mode_delta, params.cdef.cdef_damping_minus_3,
             params.cdef.cdef_bits, params.header.coded_denom);
      for (i = 0; i < 8; i++)
         printf(" %u %u", params.cdef.cdef_y_sec_strength[i],
                params.cdef.cdef_uv_sec_strength[i]);
      for (i = 0; i < 3; i++)
         printf(" %u", params.restoration.LoopRestorationSize[i]);
      printf(" %u %u\n", params.header.flags.apply_grain,
             params.grain.grain_seed);
      for (i = 0; i < params.info.tileCount; i++)
         printf("AT %u %u %u\n", i, params.offsets[i], params.sizes[i]);
   }
   frames++;
   return 0;
}

static int unused_output(void *owner, unsigned slot, const rav1_grain_t *grain,
                         video_hw_frame_t *frame)
{
   (void)owner;
   (void)slot;
   (void)grain;
   (void)frame;
   return -1;
}

static int inspect_params(const char *path)
{
   FILE *file = fopen(path, "rb");
   uint8_t *data = NULL;
   size_t offset = 0;
   long size;
   rav1_obu_t obu;
   rav1_hw_ops_t ops;
   int ret = -1, result = 1;
   bool initialized = false;
   if (!file)
      return 1;
   if (fseek(file, 0, SEEK_END) || (size = ftell(file)) <= 0 ||
       size > 32 * 1024 * 1024 || fseek(file, 0, SEEK_SET))
      goto done;
   data = (uint8_t *)malloc((size_t)size);
   if (!data || fread(data, 1, (size_t)size, file) != (size_t)size)
      goto done;
   ops.decoder = NULL;
   ops.decode = map_picture;
   ops.output = unused_output;
   verbose = 1;
   frames = 0;
   while ((ret = rav1_obu_next(&obu, data, size, &offset, false)) == 1)
   {
      if (obu.type == 1)
      {
         if (!rav1_sequence_parse(&av1_sequence, obu.payload, obu.payload_size))
            goto done;
         if (!initialized)
         {
            if (!rav1_hw_session_init(&av1_session, &av1_sequence, &ops, 9))
               goto done;
            initialized = true;
         }
         else if (memcmp(&av1_sequence, &av1_session.sequence,
                         sizeof(av1_sequence)))
            goto done;
      }
      if (obu.type != 3 && obu.type != 6)
         continue;
      if (!initialized ||
          rav1_hw_session_submit(&av1_session, obu.data, obu.size))
         goto done;
      rav1_hw_session_discard_output(&av1_session);
   }
   if (!ret && frames)
      result = 0;
done:
   free(data);
   fclose(file);
   return result;
}

#ifndef AV1_PARAMS_NO_MAIN
int main(int argc, char **argv)
{
   if (argc == 2)
      return inspect_params(argv[1]);
   test_sequence();
   test_tiles();
   test_unsized();
   test_picture();
   test_grain();
   printf("AV1 Vulkan parameters: %u failures\n", failures);
   return failures ? 1 : 0;
}
#endif
