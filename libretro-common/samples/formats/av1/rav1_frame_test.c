/* SPDX-License-Identifier: MIT */
#include <stdio.h>
#include <stdlib.h>
#include <formats/rav1_frame.h>

static rav1_sequence_t seq;
static rav1_frame_prefix_t prefix, prefix_zero;
static rav1_frame_size_t geometry, size_zero;
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

static void put(uint32_t value, unsigned count)
{
   unsigned i;
   for (i = count; i; i--)
   {
      if ((value >> (i - 1)) & 1)
         data[position / 8] |= 1 << (7 - position % 8);
      position++;
   }
}

static rav1_bits_t reader(size_t size, unsigned start)
{
   rav1_bits_t bits;
   bits.data = data;
   bits.size = size;
   bits.byte = start / 8;
   bits.bit = start % 8;
   bits.failed = 0;
   return bits;
}

static void reset(void)
{
   memset(&seq, 0, sizeof(seq));
   memset(data, 0, sizeof(data));
   position = 3;
   seq.operating_point_count = 1;
   seq.force_screen_content_tools = seq.force_integer_mv = 2;
   seq.frame_width_bits = seq.frame_height_bits = 16;
   seq.max_width = seq.max_height = 65536;
}

static void reject_prefix(size_t size, bool frame_obu)
{
   rav1_bits_t bits = reader(size, 3);
   memset(&prefix, 255, sizeof(prefix));
   CHECK(!rav1_frame_prefix_parse(&prefix, &bits, &seq, 2, 1, frame_obu));
   CHECK(!memcmp(&prefix, &prefix_zero, sizeof(prefix)));
   CHECK(!bits.byte && bits.bit == 3 && !bits.failed);
}

static void test_types(void)
{
   rav1_bits_t bits;
   unsigned type, show, resilient, inferred, intra, i;
   size_t size;
   for (type = 0; type < 4; type++)
      for (show = 0; show < 2; show++)
         for (resilient = 0; resilient < 2; resilient++)
         {
            reset();
            seq.enable_order_hint = 1;
            seq.order_hint_bits = 3;
            seq.frame_id_numbers_present = 1;
            seq.frame_id_length = 8;
            intra = type == RAV1_FRAME_KEY || type == RAV1_FRAME_INTRA_ONLY;
            inferred =
                type == RAV1_FRAME_SWITCH || (type == RAV1_FRAME_KEY && show);
            put(0, 1);
            put(type, 2);
            put(show, 1);
            if (!show)
               put(1, 1);
            if (!inferred)
               put(resilient, 1);
            put(1, 1); /* disable_cdf_update */
            put(1, 1); /* allow_screen_content_tools */
            put(0, 1); /* force_integer_mv, overridden for intra */
            put(0xa5, 8);
            if (type != RAV1_FRAME_SWITCH)
               put(0, 1);
            put(5, 3);
            if (!intra && !inferred && !resilient)
               put(3, 3);
            if (!inferred)
               put(0x82, 8);
            if ((!intra || !inferred) && (inferred || resilient))
               for (i = 0; i < 8; i++)
                  put(i, 3);
            size = (position + 7) / 8;
            bits = reader(size, 3);
            CHECK(rav1_frame_prefix_parse(&prefix, &bits, &seq, 2, 1, true));
            CHECK(prefix.frame_type == type && prefix.show_frame == show &&
                  !prefix.show_existing);
            CHECK(prefix.frame_is_intra == intra &&
                  prefix.showable_frame == (show ? type != 0 : 1));
            CHECK(prefix.error_resilient == (inferred || resilient));
            CHECK(prefix.force_integer_mv == intra &&
                  prefix.allow_screen_content_tools == 1);
            CHECK(prefix.current_frame_id == 0xa5 && prefix.order_hint == 5);
            CHECK(prefix.size_override == (type == RAV1_FRAME_SWITCH));
            CHECK(prefix.primary_ref_frame ==
                  ((!intra && !inferred && !resilient) ? 3u : 7u));
            CHECK(prefix.refresh_frame_flags == (inferred ? 255u : 0x82u));
            CHECK(bits.byte * 8 + bits.bit == position);
            for (i = 0; i < size; i++)
               reject_prefix(i, true);
         }
   reset();
   seq.still_picture = seq.reduced_still_picture_header = 1;
   put(0, 1);
   put(1, 1);
   put(0, 1);
   bits = reader(1, 3);
   CHECK(rav1_frame_prefix_parse(&prefix, &bits, &seq, 0, 0, true));
   CHECK(prefix.frame_type == RAV1_FRAME_KEY && prefix.show_frame &&
         prefix.frame_is_intra);
   CHECK(prefix.refresh_frame_flags == 255 && prefix.primary_ref_frame == 7 &&
         prefix.force_integer_mv == 1);
   CHECK(bits.bit == position);
}

static void test_timing(void)
{
   rav1_bits_t bits;
   unsigned i;
   size_t size;
   reset();
   seq.decoder_model_info_present = 1;
   seq.frame_presentation_time_length = 5;
   seq.buffer_removal_time_length = 32;
   seq.operating_point_count = 32;
   for (i = 0; i < 32; i++)
   {
      seq.operating_points[i].decoder_model_present = 1;
      seq.operating_points[i].idc = i ? 0x100 + i : 0;
   }
   seq.operating_points[31].idc = 0x204;
   put(0, 1);
   put(RAV1_FRAME_INTER, 2);
   put(1, 1);
   put(19, 5);
   put(0, 1); /* error_resilient */
   put(0, 1);
   put(0, 1); /* no screen tools, no integer MV bit */
   put(0, 1);
   put(2, 3);
   put(1, 1);
   put(UINT32_MAX, 32);
   put(0x12345678, 32);
   put(7, 8);
   size = (position + 7) / 8;
   bits = reader(size, 3);
   CHECK(rav1_frame_prefix_parse(&prefix, &bits, &seq, 2, 1, true));
   CHECK(prefix.presentation_time_present && prefix.presentation_time == 19);
   CHECK(prefix.removal_time_present &&
         prefix.removal_time_mask == UINT32_C(0x80000001));
   CHECK(prefix.removal_time[0] == UINT32_MAX &&
         prefix.removal_time[31] == 0x12345678);
   CHECK(prefix.refresh_frame_flags == 7 && prefix.primary_ref_frame == 2);
   CHECK(bits.byte * 8 + bits.bit == position);
   for (i = 0; i < size; i++)
      reject_prefix(i, true);
}

static void test_existing(void)
{
   rav1_bits_t bits;
   reset();
   seq.frame_id_numbers_present = 1;
   seq.frame_id_length = 16;
   seq.decoder_model_info_present = 1;
   seq.frame_presentation_time_length = seq.buffer_removal_time_length = 32;
   put(1, 1);
   put(6, 3);
   put(UINT32_MAX, 32);
   put(65535, 16);
   bits = reader((position + 7) / 8, 3);
   CHECK(rav1_frame_prefix_parse(&prefix, &bits, &seq, 0, 0, false));
   CHECK(prefix.show_existing && prefix.frame_to_show == 6 &&
         prefix.display_frame_id == 65535);
   CHECK(prefix.frame_type == RAV1_FRAME_UNRESOLVED &&
         !prefix.refresh_frame_flags);
   CHECK(prefix.presentation_time_present &&
         prefix.presentation_time == UINT32_MAX);
   CHECK(bits.byte * 8 + bits.bit == position);
   reject_prefix((position + 7) / 8, true);
   seq.still_picture = 1;
   reject_prefix((position + 7) / 8, false);
   seq.still_picture = 0;
   seq.frame_id_length = 17;
   reject_prefix((position + 7) / 8, false);
}

static void test_size(void)
{
   rav1_bits_t bits;
   unsigned denom, i;
   size_t size;
   for (denom = 8; denom <= 16; denom++)
   {
      reset();
      seq.enable_superres = 1;
      put(509, 16);
      put(513, 16);
      put(denom != 8, 1);
      if (denom != 8)
         put(denom - 9, 3);
      put(1, 1);
      put(639, 16);
      put(479, 16);
      size = (position + 7) / 8;
      bits = reader(size, 3);
      CHECK(rav1_frame_size_parse(&geometry, &bits, &seq, true));
      CHECK(geometry.upscaled_width == 510 && geometry.height == 514);
      CHECK(geometry.width == (510 * 8 + denom / 2) / denom &&
            geometry.superres_denom == denom);
      CHECK(geometry.render_width == 640 && geometry.render_height == 480);
      CHECK(geometry.mi_cols == 2 * ((geometry.width + 7) >> 3) &&
            geometry.mi_rows == 130);
      CHECK(bits.byte * 8 + bits.bit == position);
      for (i = 0; i < size; i++)
      {
         bits = reader(i, 3);
         memset(&geometry, 255, sizeof(geometry));
         CHECK(!rav1_frame_size_parse(&geometry, &bits, &seq, true));
         CHECK(!memcmp(&geometry, &size_zero, sizeof(geometry)));
         CHECK(!bits.byte && bits.bit == 3 && !bits.failed);
      }
      seq.max_width = 509;
      bits = reader(size, 3);
      CHECK(!rav1_frame_size_parse(&geometry, &bits, &seq, true));
   }
   reset();
   put(0, 1);
   bits = reader(1, 3);
   CHECK(rav1_frame_size_parse(&geometry, &bits, &seq, false));
   CHECK(geometry.width == 65536 && geometry.height == 65536 &&
         geometry.render_width == 65536);
   CHECK(bits.bit == 4);
}

static int inspect(const char *path)
{
   FILE *file = fopen(path, "rb");
   uint8_t *bytes = NULL;
   long size;
   size_t offset = 0;
   unsigned count = 0;
   int ret = -1, result = 1;
   rav1_obu_t obu;
   bool have_sequence = false;
   if (!file)
      return 1;
   if (fseek(file, 0, SEEK_END) || (size = ftell(file)) <= 0 ||
       size > 32 * 1024 * 1024 || fseek(file, 0, SEEK_SET))
      goto done;
   bytes = (uint8_t *)malloc((size_t)size);
   if (!bytes || fread(bytes, 1, (size_t)size, file) != (size_t)size)
      goto done;
   while ((ret = rav1_obu_next(&obu, bytes, size, &offset, true)) == 1)
   {
      rav1_bits_t bits;
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
          !rav1_frame_prefix_parse(&prefix, &bits, &seq, obu.temporal_id,
                                   obu.spatial_id, obu.type == 6))
         goto done;
      printf("P %u %u %u %u %u %u %u %u %u %u %u %lu\n", count++,
             prefix.show_existing, prefix.frame_type, prefix.show_frame,
             prefix.showable_frame, prefix.error_resilient, prefix.order_hint,
             prefix.primary_ref_frame, prefix.refresh_frame_flags,
             prefix.frame_to_show, prefix.current_frame_id,
             (unsigned long)(bits.byte * 8 + bits.bit +
                             (obu.payload - obu.data) * 8));
      if (!prefix.show_existing && prefix.frame_is_intra)
      {
         if (!rav1_frame_size_parse(&geometry, &bits, &seq,
                                    prefix.size_override != 0))
            goto done;
         printf("S %u %u %u %u %u %u %lu\n", geometry.width, geometry.height,
                geometry.upscaled_width, geometry.render_width,
                geometry.render_height, geometry.superres_denom,
                (unsigned long)(bits.byte * 8 + bits.bit +
                                (obu.payload - obu.data) * 8));
      }
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
   test_types();
   test_timing();
   test_existing();
   test_size();
   printf("AV1 frame prefix and size: %u failures\n", failures);
   return failures ? 1 : 0;
}
