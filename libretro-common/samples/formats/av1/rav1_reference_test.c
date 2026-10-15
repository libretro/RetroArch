/* SPDX-License-Identifier: MIT */
#include <stdio.h>
#include <stdlib.h>
#include <formats/rav1_motion.h>
#include <formats/rav1_tile_info.h>
#include <formats/rav1_picture.h>

static rav1_sequence_t seq;
static rav1_frame_prefix_t frame;
static rav1_reference_t refs[8], saved[8];
static rav1_frame_setup_t setup, zero;
static rav1_tile_info_t tiles;
static rav1_picture_t picture;
static rav1_motion_t motion;
static rav1_global_motion_t globals[8];
static rav1_picture_reference_t pictures[8];
static uint8_t data[128];
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
         data[position / 8] |= 1u << (7 - position % 8);
      position++;
   }
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
   unsigned i;
   memset(&seq, 0, sizeof(seq));
   memset(&frame, 0, sizeof(frame));
   memset(refs, 0, sizeof(refs));
   memset(data, 0, sizeof(data));
   position = 3;
   seq.frame_width_bits = seq.frame_height_bits = 10;
   seq.max_width = 1024;
   seq.max_height = 768;
   frame.frame_type = RAV1_FRAME_INTER;
   frame.primary_ref_frame = 7;
   for (i = 0; i < 8; i++)
   {
      refs[i].valid = 1;
      refs[i].size.upscaled_width = 320 + 8 * i;
      refs[i].size.height = 240 + 8 * i;
      refs[i].size.render_width = 640;
      refs[i].size.render_height = 480;
      refs[i].order_hint = i;
      refs[i].frame_id = 31;
   }
}

static void reject(size_t size)
{
   rav1_bits_t bits = reader(size);
   memcpy(saved, refs, sizeof(refs));
   memset(&setup, 255, sizeof(setup));
   CHECK(!rav1_frame_setup_parse(&setup, &bits, &seq, &frame, refs));
   CHECK(!memcmp(&setup, &zero, sizeof(setup)));
   CHECK(!memcmp(saved, refs, sizeof(refs)));
   CHECK(!bits.byte && bits.bit == 3 && !bits.failed);
}

static void test_explicit(void)
{
   rav1_bits_t bits;
   unsigned i, denom, found, filter;
   size_t size;
   for (found = 0; found < 8; found++)
      for (denom = 8; denom <= 16; denom++)
         for (filter = 0; filter < 5; filter++)
         {
            reset();
            seq.enable_superres = seq.enable_ref_frame_mvs = 1;
            frame.size_override = 1;
            for (i = 0; i < 7; i++)
               put(i, 3);
            for (i = 0; i < 7; i++)
            {
               put(i == found, 1);
               if (i == found)
                  break;
            }
            if (found == 7)
            {
               put(510, 10);
               put(512, 10);
            }
            put(denom != 8, 1);
            if (denom != 8)
               put(denom - 9, 3);
            if (found == 7)
               put(0, 1);
            put(1, 1);
            put(filter == 4, 1);
            if (filter != 4)
               put(filter, 2);
            put(1, 1);
            put(1, 1);
            put(0, 1);
            size = (position + 7) / 8;
            bits = reader(size);
            CHECK(rav1_frame_setup_parse(&setup, &bits, &seq, &frame, refs));
            CHECK(setup.size.upscaled_width ==
                  (found == 7 ? 511 : 320 + 8 * found));
            CHECK(setup.size.height == (found == 7 ? 513 : 240 + 8 * found));
            CHECK(setup.size.superres_denom == denom);
            CHECK(setup.size.width ==
                  (setup.size.upscaled_width * 8 + denom / 2) / denom);
            CHECK(setup.size.render_width == (found == 7 ? 511u : 640u));
            CHECK(setup.interpolation_filter == filter &&
                  setup.allow_high_precision_mv);
            CHECK(setup.motion_mode_switchable && setup.use_ref_frame_mvs);
            CHECK(!setup.disable_frame_end_update_cdf &&
                  setup.valid_mask == 255);
            CHECK(bits.byte * 8 + bits.bit == position);
            for (i = 0; i < size; i++)
               reject(i);
         }
}

static void test_short(void)
{
   static const unsigned expected[7] = {0, 7, 3, 1, 4, 5, 6};
   unsigned indices[7], i;
   rav1_bits_t bits;
   reset();
   seq.enable_order_hint = 1;
   seq.order_hint_bits = 4;
   frame.order_hint = 4;
   refs[7].order_hint = 3;
   CHECK(rav1_reference_select(indices, refs, 4, 4, 0, 1));
   CHECK(!memcmp(indices, expected, sizeof(indices)));
   put(1, 1);
   put(0, 3);
   put(1, 3);
   put(0, 1); /* render size */
   put(0, 1); /* high precision */
   put(1, 1); /* switchable filter */
   put(0, 1); /* motion mode */
   put(1, 1); /* frame-end CDF */
   bits = reader((position + 7) / 8);
   CHECK(rav1_frame_setup_parse(&setup, &bits, &seq, &frame, refs));
   CHECK(!memcmp(setup.ref_frame_idx, expected, sizeof(expected)));
   CHECK(setup.sign_bias[4] == 0 && setup.sign_bias[5] == 1 &&
         setup.sign_bias[6] == 1);
   refs[0].order_hint = 4;
   reject((position + 7) / 8);
   for (i = 0; i < 8; i++)
      refs[i].order_hint = 15;
   CHECK(rav1_reference_select(indices, refs, 0, 4, 0, 0));
   CHECK(indices[1] == 7 && indices[2] == 6 && indices[4] == 5);
   CHECK(indices[5] == 4 && indices[6] == 3);
   for (i = 0; i < 8; i++)
      refs[i].order_hint = 1;
   refs[0].order_hint = 15;
   CHECK(rav1_reference_select(indices, refs, 0, 4, 0, 0));
   CHECK(indices[0] == 0 && indices[1] == 0 && indices[2] == 0);
   CHECK(indices[3] == 0 && indices[4] == 1 && indices[5] == 2 &&
         indices[6] == 7);
   CHECK(rav1_relative_dist(0, 15, 4) == 1);
   CHECK(rav1_relative_dist(15, 0, 4) == -1);
   CHECK(rav1_relative_dist(8, 0, 4) == -8);
   CHECK(!rav1_reference_select(indices, refs, 0, 0, 0, 0));
   CHECK(!rav1_reference_select(indices, refs, 0, 9, 0, 0));
   CHECK(!rav1_reference_select(indices, refs, 0, 4, 8, 0));
}

static void test_ids_and_gates(void)
{
   rav1_bits_t bits;
   unsigned i;
   reset();
   seq.frame_id_numbers_present = 1;
   seq.frame_id_length = 5;
   seq.delta_frame_id_length = 3;
   frame.current_frame_id = 1;
   frame.force_integer_mv = frame.error_resilient = frame.disable_cdf_update =
       1;
   seq.enable_ref_frame_mvs = 1;
   for (i = 0; i < 7; i++)
   {
      put(i, 3);
      put(1, 3); /* ID wraps from 1 to 31 */
   }
   put(0, 1);
   put(1, 1);
   put(0, 1);
   bits = reader((position + 7) / 8);
   CHECK(rav1_frame_setup_parse(&setup, &bits, &seq, &frame, refs));
   CHECK(setup.disable_frame_end_update_cdf && !setup.allow_high_precision_mv);
   CHECK(!setup.use_ref_frame_mvs && bits.byte * 8 + bits.bit == position);
   refs[7].frame_id = 25;
   bits = reader((position + 7) / 8);
   CHECK(rav1_frame_setup_parse(&setup, &bits, &seq, &frame, refs));
   CHECK(setup.valid_mask == 255); /* distance equals the eight-ID window */
   refs[7].frame_id = 24;
   bits = reader((position + 7) / 8);
   CHECK(rav1_frame_setup_parse(&setup, &bits, &seq, &frame, refs));
   CHECK(setup.valid_mask == 127);
   refs[0].frame_id = 30;
   reject((position + 7) / 8);
   refs[0].frame_id = 20;
   reject((position + 7) / 8);
   refs[0].frame_id = 31;
   refs[0].valid = 0;
   reject((position + 7) / 8);
   refs[0].valid = 1;
   frame.ref_order_hints_present = 1;
   reject((position + 7) / 8);
   reset();
   frame.frame_is_intra = frame.show_frame = 1;
   frame.frame_type = RAV1_FRAME_KEY;
   frame.allow_screen_content_tools = 1;
   seq.reduced_still_picture_header = 1;
   put(0, 1);
   put(1, 1);
   bits = reader(1);
   CHECK(rav1_frame_setup_parse(&setup, &bits, &seq, &frame, refs));
   CHECK(setup.allow_intrabc && setup.disable_frame_end_update_cdf &&
         !setup.valid_mask);
   CHECK(bits.byte * 8 + bits.bit == position);
   frame.show_existing = 1;
   reject(1);
}

/* The fixture harness assumes successful decode of its encoder-made frames.
 * Production code must defer these metadata updates until decode completes. */
static int inspect(const char *path)
{
   FILE *file = fopen(path, "rb");
   uint8_t *bytes = NULL;
   size_t offset = 0, base, start;
   long size;
   unsigned i, count = 0;
   int ret = -1, result = 1;
   bool have_sequence = false;
   rav1_obu_t obu;
   if (!file)
      return 1;
   if (fseek(file, 0, SEEK_END) || (size = ftell(file)) <= 0 ||
       size > 32 * 1024 * 1024 || fseek(file, 0, SEEK_SET))
      goto done;
   bytes = (uint8_t *)malloc((size_t)size);
   if (!bytes || fread(bytes, 1, (size_t)size, file) != (size_t)size)
      goto done;
   memset(refs, 0, sizeof(refs));
   while ((ret = rav1_obu_next(&obu, bytes, size, &offset, true)) == 1)
   {
      rav1_bits_t bits;
      const rav1_picture_reference_t *primary = NULL;
      if (obu.type == 1)
      {
         if (!rav1_sequence_parse(&seq, obu.payload, obu.payload_size))
            goto done;
         have_sequence = true;
      }
      if (obu.type != 3 && obu.type != 6)
         continue;
      bits.data = obu.payload;
      bits.size = obu.payload_size;
      bits.byte = 0;
      bits.bit = bits.failed = 0;
      if (!have_sequence ||
          !rav1_frame_prefix_parse(&frame, &bits, &seq, obu.temporal_id,
                                   obu.spatial_id, obu.type == 6))
         goto done;
      if (frame.show_existing)
         continue;
      base = (size_t)(obu.payload - obu.data) * 8;
      if (!rav1_frame_setup_parse(&setup, &bits, &seq, &frame, refs))
         goto done;
      printf("H %u %u %u %u %u %u %lu\n", setup.short_signaling,
             setup.allow_intrabc, setup.allow_high_precision_mv,
             setup.interpolation_filter, setup.motion_mode_switchable,
             setup.use_ref_frame_mvs,
             (unsigned long)(bits.byte * 8 + bits.bit + base));
      printf("S %u %u %u %u %u %u %u\n", setup.size.width, setup.size.height,
             setup.size.upscaled_width, setup.size.render_width,
             setup.size.render_height, setup.size.superres_denom,
             setup.disable_frame_end_update_cdf);
      if (!frame.frame_is_intra)
      {
         printf("I");
         for (i = 0; i < 7; i++)
            printf(" %u", setup.ref_frame_idx[i]);
         printf("\n");
      }
      if (!rav1_tile_info_parse(&tiles, &bits, setup.size.width,
                                setup.size.height,
                                seq.use_128x128_superblock != 0))
         goto done;
      if (frame.primary_ref_frame != 7)
         primary = &pictures[setup.ref_frame_idx[frame.primary_ref_frame]];
      start = bits.byte * 8 + bits.bit + base;
      if (!rav1_picture_parse(&picture, &bits, &seq, &frame, &setup.size,
                              setup.allow_intrabc != 0, primary))
         goto done;
      printf("P %lu %lu %u %u %d %d\n", (unsigned long)start,
             (unsigned long)(bits.byte * 8 + bits.bit + base),
             picture.base_q_idx, picture.tx_mode,
             picture.reference.loop_filter_ref_deltas[0],
             picture.reference.loop_filter_ref_deltas[4]);
      printf("M");
      for (i = 0; i < 8; i++)
         printf(" %d", picture.reference.loop_filter_ref_deltas[i]);
      for (i = 0; i < 2; i++)
         printf(" %d", picture.reference.loop_filter_mode_deltas[i]);
      for (i = 0; i < 8; i++)
      {
         unsigned j;
         printf(" %u", picture.reference.segment_enabled[i]);
         for (j = 0; j < 8; j++)
            printf(" %d", picture.reference.segment_data[i][j]);
      }
      printf("\n");
      if (!rav1_motion_parse(
              &motion, &bits, &seq, &frame, &setup, refs,
              frame.primary_ref_frame == 7
                  ? NULL
                  : &globals[setup.ref_frame_idx[frame.primary_ref_frame]]))
         goto done;
      printf("T %u %u %u %u %lu\n", motion.reference_select,
             motion.skip_mode_present, motion.allow_warped_motion,
             motion.reduced_tx_set,
             (unsigned long)(bits.byte * 8 + bits.bit + base));
      for (i = 0; i < 7; i++)
      {
         unsigned j;
         printf("G %u", motion.global.type[i]);
         for (j = 0; j < 6; j++)
            printf(" %d", (int)motion.global.params[i][j]);
         printf("\n");
      }
      for (i = 0; i < 8; i++)
      {
         refs[i].valid = (setup.valid_mask >> i) & 1;
         if ((frame.refresh_frame_flags >> i) & 1)
         {
            refs[i].valid = 1;
            refs[i].order_hint = frame.order_hint;
            refs[i].frame_id = frame.current_frame_id;
            refs[i].size = setup.size;
            pictures[i] = picture.reference;
            globals[i] = motion.global;
         }
      }
      count++;
   }
   result = ret == 0 && count ? 0 : 1;
done:
   free(bytes);
   fclose(file);
   return result;
}

int main(int argc, char **argv)
{
   if (argc == 2)
      return inspect(argv[1]);
   test_explicit();
   test_short();
   test_ids_and_gates();
   printf("AV1 references and frame setup: %u failures\n", failures);
   return failures ? 1 : 0;
}
