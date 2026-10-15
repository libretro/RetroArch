/* SPDX-License-Identifier: MIT */
#include <stdio.h>
#include <stdlib.h>
#include <formats/rav1_picture.h>
#include <formats/rav1_tile_info.h>

static rav1_sequence_t seq;
static rav1_frame_prefix_t frame;
static rav1_frame_size_t geometry;
static rav1_tile_info_t tile_info;
static rav1_picture_t picture, zero;
static rav1_picture_reference_t primary, saved_primary;
static uint8_t data[512];
static unsigned position, failures;

#define CHECK(x)                                                               \
   do                                                                          \
   {                                                                           \
      if (!(x))                                                                \
      {                                                                        \
         fprintf(stderr, "line %u: %s\n", (unsigned)__LINE__, #x);             \
         failures++;                                                           \
      }                                                                        \
   } while (0)

static void put(unsigned value, unsigned count)
{
   unsigned i;
   for (i = count; i; i--)
   {
      if ((value >> (i - 1)) & 1)
         data[position / 8] |= 1 << (7 - position % 8);
      position++;
   }
}

static void signed_value(int value, unsigned count)
{
   put((unsigned)value & ((1u << count) - 1), count);
}

static void quant(unsigned base)
{
   put(base, 8);
   put(0, 1);
   if (!seq.monochrome)
   {
      if (seq.separate_uv_delta_q)
         put(0, 1);
      put(0, 2);
   }
   put(0, 1);
}

static rav1_bits_t reader(size_t size)
{
   rav1_bits_t bits;
   bits.data = data;
   bits.size = size;
   bits.byte = 0;
   bits.bit = 3;
   bits.failed = 0;
   return bits;
}

static void reset(void)
{
   memset(data, 0, sizeof(data));
   memset(&seq, 0, sizeof(seq));
   memset(&frame, 0, sizeof(frame));
   memset(&geometry, 0, sizeof(geometry));
   seq.separate_uv_delta_q = seq.subsampling_x = seq.subsampling_y = 1;
   seq.enable_cdef = seq.enable_restoration = 1;
   frame.primary_ref_frame = 7;
   geometry.width = geometry.upscaled_width = 512;
   geometry.height = 512;
   position = 3;
}

static void test_full(void)
{
   rav1_bits_t bits;
   unsigned i, j, enabled;
   int value;
   size_t size;
   reset();
   put(100, 8);
   put(1, 1);
   signed_value(-64, 7);
   put(1, 1);
   put(1, 1);
   signed_value(63, 7);
   put(1, 1);
   signed_value(-1, 7);
   put(1, 1);
   signed_value(1, 7);
   put(1, 1);
   signed_value(0, 7);
   put(1, 1);
   put(2, 4);
   put(3, 4);
   put(4, 4);
   put(1, 1);
   for (i = 0; i < 8; i++)
      for (j = 0; j < 8; j++)
      {
         enabled =
             (i == 0 && (j < 2 || j >= 5)) || (i == 7 && (j == 0 || j == 4));
         put(enabled, 1);
         if (!enabled)
            continue;
         value = i == 0 ? (j == 0   ? -256
                           : j == 1 ? -64
                                    : 7)
                        : (j == 0 ? 255 : 63);
         if (j < 5)
            signed_value(value, j ? 7 : 9);
         else if (j == 5)
            put(value, 3);
      }
   put(1, 1);
   put(3, 2);
   put(1, 1);
   put(2, 2);
   put(1, 1);
   for (i = 0; i < 4; i++)
      put((i + 1) * 10, 6);
   put(7, 3);
   put(3, 2);
   for (i = 0; i < 8; i++)
   {
      put(1, 1);
      signed_value((int)i - 4, 7);
   }
   put(1, 1);
   signed_value(-64, 7);
   put(1, 1);
   signed_value(63, 7);
   put(3, 2);
   put(3, 2);
   for (i = 0; i < 8; i++)
   {
      put(i, 4);
      put(i % 4, 2);
      put(15 - i, 4);
      put(3 - i % 4, 2);
   }
   put(1, 2);
   put(2, 2);
   put(3, 2);
   put(1, 1);
   put(1, 1);
   put(1, 1);
   put(1, 1);
   size = (position + 7) / 8;
   bits = reader(size);
   CHECK(rav1_picture_parse(&picture, &bits, &seq, &frame, &geometry, false,
                            NULL));
   CHECK(picture.base_q_idx == 100 && picture.delta_q_y_dc == -64);
   CHECK(picture.delta_q_u_dc == 63 && picture.delta_q_u_ac == -1 &&
         picture.delta_q_v_dc == 1 && !picture.delta_q_v_ac);
   CHECK(picture.diff_uv_delta && picture.loop_filter_update_ref_delta == 255 &&
         picture.loop_filter_update_mode_delta == 3);
   CHECK(picture.using_qmatrix && picture.qm_y == 2 && picture.qm_u == 3 &&
         picture.qm_v == 4);
   CHECK(picture.reference.segment_enabled[0] == 227 &&
         picture.reference.segment_enabled[7] == 17);
   CHECK(picture.reference.segment_data[0][0] == -255 &&
         picture.reference.segment_data[0][1] == -63);
   CHECK(picture.last_active_segment == 7 && picture.segment_id_pre_skip);
   CHECK(picture.segmentation_update_map && picture.segmentation_update_data &&
         !picture.segmentation_temporal_update);
   CHECK(picture.delta_q_present && picture.delta_q_res == 3 &&
         picture.delta_lf_present && picture.delta_lf_res == 2 &&
         picture.delta_lf_multi);
   CHECK(!picture.coded_lossless && !picture.all_lossless &&
         !picture.lossless_mask);
   for (i = 0; i < 4; i++)
      CHECK(picture.loop_filter_level[i] == (i + 1) * 10);
   CHECK(picture.reference.loop_filter_ref_deltas[0] == -4 &&
         picture.reference.loop_filter_ref_deltas[7] == 3);
   CHECK(picture.reference.loop_filter_mode_deltas[0] == -64 &&
         picture.reference.loop_filter_mode_deltas[1] == 63);
   CHECK(picture.cdef_damping == 6 && picture.cdef_bits == 3 &&
         picture.cdef_y_sec[3] == 4 && picture.cdef_uv_sec[0] == 4);
   CHECK(picture.restoration_type[0] == RAV1_RESTORE_SWITCHABLE &&
         picture.restoration_type[1] == RAV1_RESTORE_WIENER &&
         picture.restoration_type[2] == RAV1_RESTORE_SGRPROJ);
   CHECK(picture.restoration_size[0] == 256 &&
         picture.restoration_size[1] == 128 &&
         picture.restoration_size[2] == 128);
   CHECK(picture.tx_mode == 2 && bits.byte * 8 + bits.bit == position);
   primary = picture.reference;
   saved_primary = primary;
   for (i = 0; i < size; i++)
   {
      bits = reader(i);
      CHECK(!rav1_picture_parse(&picture, &bits, &seq, &frame, &geometry, false,
                                NULL));
      CHECK(!memcmp(&picture, &zero, sizeof(picture)));
      CHECK(!bits.byte && bits.bit == 3 && !bits.failed);
   }
}

static void test_inherited(void)
{
   rav1_bits_t bits;
   reset();
   seq.monochrome = 1;
   seq.enable_cdef = seq.enable_restoration = 0;
   frame.primary_ref_frame = 3;
   quant(0);
   put(1, 1);
   put(1, 1);
   put(1, 1);
   put(0, 1);
   put(0, 12);
   put(0, 3);
   put(1, 1);
   put(0, 1);
   put(0, 1);
   bits = reader((position + 7) / 8);
   CHECK(rav1_picture_parse(&picture, &bits, &seq, &frame, &geometry, false,
                            &primary));
   CHECK(picture.lossless_mask == 127 && !picture.coded_lossless &&
         !picture.all_lossless);
   CHECK(!picture.segmentation_update_data &&
         picture.segmentation_temporal_update &&
         picture.last_active_segment == 7);
   CHECK(!picture.diff_uv_delta && !picture.loop_filter_update_ref_delta &&
         !picture.loop_filter_update_mode_delta);
   CHECK(!memcmp(&picture.reference, &primary, sizeof(primary)));
   CHECK(!memcmp(&primary, &saved_primary, sizeof(primary)));
   CHECK(picture.tx_mode == 1 && bits.byte * 8 + bits.bit == position);
   bits = reader((position + 7) / 8);
   CHECK(!rav1_picture_parse(&picture, &bits, &seq, &frame, &geometry, false,
                             NULL));
   CHECK(!memcmp(&picture, &zero, sizeof(picture)));
   reset();
   frame.primary_ref_frame = 0;
   quant(0);
   put(0, 1);
   bits = reader((position + 7) / 8);
   CHECK(rav1_picture_parse(&picture, &bits, &seq, &frame, &geometry, false,
                            &primary));
   CHECK(picture.coded_lossless && picture.all_lossless &&
         picture.lossless_mask == 255);
   CHECK(!picture.reference.segment_enabled[0] &&
         !picture.reference.segment_data[7][0]);
   CHECK(picture.reference.loop_filter_ref_deltas[0] == 1 &&
         !picture.reference.loop_filter_mode_deltas[0]);
   CHECK(!rav1_picture_parse(&picture, &bits, &seq, &frame, &geometry, false,
                             &picture.reference));
   CHECK(picture.coded_lossless && picture.all_lossless);
}

static void test_gates(void)
{
   rav1_bits_t bits;
   reset();
   quant(1);
   put(0, 1);
   put(1, 1);
   put(1, 2);
   put(0, 1);
   bits = reader((position + 7) / 8);
   CHECK(rav1_picture_parse(&picture, &bits, &seq, &frame, &geometry, true,
                            NULL));
   CHECK(picture.delta_q_present && !picture.delta_lf_present &&
         !picture.loop_filter_level[0]);
   CHECK(picture.tx_mode == 1 && picture.cdef_damping == 3 &&
         !picture.restoration_type[0]);
   CHECK(bits.byte * 8 + bits.bit == position);
   reset();
   geometry.width = 256;
   seq.use_128x128_superblock = 1;
   quant(0);
   put(0, 1);
   put(0, 2);
   put(1, 2);
   put(0, 2);
   put(0, 1);
   put(1, 1);
   bits = reader((position + 7) / 8);
   CHECK(rav1_picture_parse(&picture, &bits, &seq, &frame, &geometry, false,
                            NULL));
   CHECK(picture.coded_lossless && !picture.all_lossless && !picture.tx_mode);
   CHECK(picture.restoration_type[1] == RAV1_RESTORE_SWITCHABLE &&
         picture.restoration_size[0] == 128 &&
         picture.restoration_size[1] == 64);
   CHECK(bits.byte * 8 + bits.bit == position);
}

static uint8_t *load_file(const char *path, size_t *length)
{
   FILE *file = fopen(path, "rb");
   uint8_t *bytes = NULL;
   long size;
   *length = 0;
   if (!file)
      return NULL;
   if (!fseek(file, 0, SEEK_END) && (size = ftell(file)) > 0 &&
       size <= 32 * 1024 * 1024 && !fseek(file, 0, SEEK_SET))
   {
      bytes = (uint8_t *)malloc((size_t)size);
      if (bytes && fread(bytes, 1, (size_t)size, file) == (size_t)size)
         *length = (size_t)size;
      else
      {
         free(bytes);
         bytes = NULL;
      }
   }
   fclose(file);
   return bytes;
}

static void test_sparse_updates(void)
{
   rav1_bits_t bits;
   unsigned i;
   reset();
   seq.monochrome = 1;
   seq.enable_cdef = seq.enable_restoration = 0;
   quant(50);
   put(0, 2);
   put(1, 6);
   put(2, 6);
   put(0, 3);
   put(3, 2);
   for (i = 0; i < 8; i++)
   {
      put(i == 0 || i == 7, 1);
      if (i == 0 || i == 7)
         signed_value(i == 0 ? 1 : -2, 7);
   }
   put(0, 1);
   put(1, 1);
   signed_value(-3, 7);
   put(0, 1);
   bits = reader((position + 7) / 8);
   CHECK(rav1_picture_parse(&picture, &bits, &seq, &frame, &geometry, false,
                            NULL));
   CHECK(picture.loop_filter_update_ref_delta == 0x81 &&
         picture.loop_filter_update_mode_delta == 2);
   CHECK(picture.reference.loop_filter_ref_deltas[0] == 1 &&
         picture.reference.loop_filter_ref_deltas[7] == -2 &&
         picture.reference.loop_filter_ref_deltas[4] == -1);
   CHECK(!picture.reference.loop_filter_mode_deltas[0] &&
         picture.reference.loop_filter_mode_deltas[1] == -3);
   CHECK(bits.byte * 8 + bits.bit == position);
}

static void dump_picture(void)
{
   unsigned i, j;
   printf("Q %u %d %d %d %d %d %u %u %u %u\n", picture.base_q_idx,
          picture.delta_q_y_dc, picture.delta_q_u_dc, picture.delta_q_u_ac,
          picture.delta_q_v_dc, picture.delta_q_v_ac, picture.using_qmatrix,
          picture.qm_y, picture.qm_u, picture.qm_v);
   printf("S %u %u %u %u %u %u\n", picture.segmentation_enabled,
          picture.segmentation_update_map, picture.segmentation_temporal_update,
          picture.segmentation_update_data, picture.last_active_segment,
          picture.segment_id_pre_skip);
   for (i = 0; i < 8; i++)
   {
      printf("G %u %u", i, picture.reference.segment_enabled[i]);
      for (j = 0; j < 8; j++)
         printf(" %d", picture.reference.segment_data[i][j]);
      printf("\n");
   }
   printf("D %u %u %u %u %u %u %u %u\n", picture.delta_q_present,
          picture.delta_q_res, picture.delta_lf_present, picture.delta_lf_res,
          picture.delta_lf_multi, picture.lossless_mask, picture.coded_lossless,
          picture.all_lossless);
   printf("L");
   for (i = 0; i < 4; i++)
      printf(" %u", picture.loop_filter_level[i]);
   printf(" %u %u %u", picture.loop_filter_sharpness,
          picture.loop_filter_delta_enabled, picture.loop_filter_delta_update);
   for (i = 0; i < 8; i++)
      printf(" %d", picture.reference.loop_filter_ref_deltas[i]);
   for (i = 0; i < 2; i++)
      printf(" %d", picture.reference.loop_filter_mode_deltas[i]);
   printf("\nC %u %u", picture.cdef_damping, picture.cdef_bits);
   for (i = 0; i < (1u << picture.cdef_bits); i++)
      printf(" %u %u %u %u", picture.cdef_y_pri[i], picture.cdef_y_sec[i],
             picture.cdef_uv_pri[i], picture.cdef_uv_sec[i]);
   printf("\nR");
   for (i = 0; i < 3; i++)
      printf(" %u %u", picture.restoration_type[i],
             picture.restoration_size[i]);
   printf("\nT %u\n", picture.tx_mode);
}

static int inspect(const char *seq_path, const char *frame_path)
{
   size_t seq_size, frame_size, offset = 0, start;
   uint8_t *seq_bytes = load_file(seq_path, &seq_size);
   uint8_t *frame_bytes = load_file(frame_path, &frame_size);
   rav1_obu_t obu;
   rav1_bits_t bits;
   bool intrabc = false;
   int result = 1;
   if (!seq_bytes || !frame_bytes ||
       !rav1_sequence_parse(&seq, seq_bytes, seq_size) ||
       rav1_obu_next(&obu, frame_bytes, frame_size, &offset, false) != 1 ||
       obu.type != 6 || offset != frame_size)
      goto done;
   bits.data = obu.payload;
   bits.size = obu.payload_size;
   bits.byte = 0;
   bits.bit = bits.failed = 0;
   if (!rav1_frame_prefix_parse(&frame, &bits, &seq, obu.temporal_id,
                                obu.spatial_id, true) ||
       !frame.frame_is_intra ||
       !rav1_frame_size_parse(&geometry, &bits, &seq, frame.size_override != 0))
      goto done;
   if (frame.allow_screen_content_tools &&
       geometry.width == geometry.upscaled_width)
      intrabc = rav1_bits_read(&bits, 1) != 0;
   if (!seq.reduced_still_picture_header && !frame.disable_cdf_update)
      rav1_bits_read(&bits, 1);
   if (!rav1_tile_info_parse(&tile_info, &bits, geometry.width, geometry.height,
                             seq.use_128x128_superblock != 0))
      goto done;
   start = bits.byte * 8 + bits.bit;
   if (!rav1_picture_parse(&picture, &bits, &seq, &frame, &geometry, intrabc,
                           NULL))
      goto done;
   printf("B %lu %lu\n", (unsigned long)(start + (obu.payload - obu.data) * 8),
          (unsigned long)(bits.byte * 8 + bits.bit +
                          (obu.payload - obu.data) * 8));
   dump_picture();
   result = 0;
done:
   free(seq_bytes);
   free(frame_bytes);
   return result;
}

int main(int argc, char **argv)
{
   if (argc == 3)
      return inspect(argv[1], argv[2]);
   test_full();
   test_inherited();
   test_gates();
   test_sparse_updates();
   printf("AV1 picture tools: %u failures\n", failures);
   return failures ? 1 : 0;
}
