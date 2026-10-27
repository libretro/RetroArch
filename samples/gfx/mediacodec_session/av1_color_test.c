/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#define main hevc_frame_export_test_main
#include "frame_export_test.c"
#undef main

static void colors(unsigned depth, unsigned transfer)
{
   static rav1_sequence_t sequence;
   video_color_metadata_t color, resolved;
   mediacodec_output_format_t format;
   mediacodec_frame_exporter_t *exporter, *wrong;
   android_image_queue_t *queue;
   android_image_ops_t ops;
   android_image_view_t first, second;
   android_frame_view_t claim;
   video_hw_frame_t frame, pending;
   memset(&sequence, 0, sizeof(sequence));
   sequence.bit_depth = depth;
   sequence.subsampling_x = sequence.subsampling_y = 1;
   sequence.color_primaries = sequence.matrix_coefficients
         = depth == 10 ? 9 : 1;
   sequence.transfer_characteristics = transfer;
   sequence.film_grain_params_present = 1;
   memset(&color, 0, sizeof(color));
   color.primaries = sequence.color_primaries;
   color.matrix = sequence.matrix_coefficients;
   color.transfer = sequence.transfer_characteristics;
   color.range = 1;
   color.max_cll = 4000;
   color.mastering.present = 0x100;
   color.mastering.luminance_max = 20000000;
   assert(mediacodec_av1_color(&sequence, &color, &resolved));
   assert(resolved.transfer == color.transfer && resolved.max_cll == 4000);
   sequence.color_primaries = sequence.transfer_characteristics
         = sequence.matrix_coefficients = 2;
   assert(mediacodec_av1_color(&sequence, &color, &resolved));
   assert(resolved.transfer == color.transfer
         && resolved.primaries == color.primaries);
   assert(mediacodec_av1_color(&sequence, &color, &color)
         && color.max_cll == 4000);
   sequence.color_primaries = color.primaries;
   sequence.transfer_characteristics = color.transfer;
   sequence.matrix_coefficients = color.matrix;
   color.range = 2;
   assert(!mediacodec_av1_color(&sequence, &color, &resolved)
         && !resolved.range);
   color.range = 1;
   sequence.transfer_characteristics = 0;
   assert(!mediacodec_av1_color(&sequence, &color, &resolved));
   sequence.transfer_characteristics = color.transfer;
   memset(&format, 0, sizeof(format));
   format.width = format.right = 64;
   format.height = format.bottom = 32;
   format.serial = 1;
   format.color_standard = depth == 10 ? 6 : 1;
   format.color_transfer = transfer == 18 ? 7 : transfer == 16 ? 6 : 3;
   format.color_range = 2;
   memset(&ops, 0, sizeof(ops));
   ops.acquire = acquire;
   ops.describe = describe;
   ops.delete_image = delete_image;
   ops.delete_reader = delete_reader;
   queue = android_image_queue_new(&native, 2, &ops);
   exporter = mediacodec_frame_exporter_new(
         1, depth == 10 ? IMAGE_PIXFMT_P010 : IMAGE_PIXFMT_NV12);
   wrong = mediacodec_frame_exporter_new(
         1, depth == 10 ? IMAGE_PIXFMT_NV12 : IMAGE_PIXFMT_P010);
   assert(queue && exporter && wrong);
   assert(android_image_queue_acquire(queue, &first) == 1);
   memset(&frame, 0x55, sizeof(frame));
   assert(mediacodec_av1_frame_export(
                wrong, &first, &format, &sequence, &color, &frame)
         < 0);
   assert(frame.width == 0x55555555 && !android_image_queue_poll(queue));
   assert(mediacodec_frame_exporter_free(wrong));
   format.color_transfer = depth == 10 ? 3 : 6;
   assert(mediacodec_av1_frame_export(
                exporter, &first, &format, &sequence, &color, &frame)
         < 0);
   assert(frame.width == 0x55555555 && !android_image_queue_poll(queue));
   format.color_transfer = transfer == 18 ? 7 : transfer == 16 ? 6 : 3;
   assert(!mediacodec_av1_frame_export(
         exporter, &first, &format, &sequence, &color, &frame));
   assert(
         frame.color.transfer == color.transfer && frame.color.max_cll == 4000);
   assert(android_image_queue_acquire(queue, &second) == 1);
   memset(&pending, 0x55, sizeof(pending));
   assert(mediacodec_av1_frame_export(
                exporter, &second, &format, &sequence, &color, &pending)
         == 1);
   assert(pending.width == 0x55555555);
   assert(android_frame_claim(
         frame.owner, frame.done, frame.done_value, frame.image, &claim));
   assert(
         claim.color.transfer == color.transfer && claim.color.max_cll == 4000);
   assert(android_frame_finish(frame.owner, frame.done, frame.done_value, -1));
   assert(android_image_queue_poll(queue) == 1);
   color.max_cll = 1000;
   assert(!mediacodec_av1_frame_export(
         exporter, &second, &format, &sequence, &color, &pending));
   assert(claim.color.max_cll == 4000 && pending.color.max_cll == 1000);
   assert(!mediacodec_frame_exporter_free(exporter));
   video_hw_frame_abandon(&pending);
   assert(android_image_queue_poll(queue) == 1);
   assert(mediacodec_frame_exporter_free(exporter));
   android_image_queue_close(queue);
   assert(android_image_queue_free(queue));
}

static void extended_color(void)
{
   video_color_metadata_t coded, resolved;
   mediacodec_output_format_t format;
   static const int invalid[] = { -1, 63, 113, 65536 };
   unsigned i;
   memset(&coded, 0, sizeof(coded));
   memset(&format, 0, sizeof(format));
   coded.primaries = coded.transfer = 2;
   coded.matrix = coded.range = 1;
   format.color_standard = 75;
   format.color_transfer = 3;
   format.color_range = 2;
   assert(mediacodec_color_resolve(&format, &coded, &resolved));
   assert(resolved.primaries == 6 && resolved.matrix == 1
         && resolved.transfer == 6 && resolved.range == 1);
   coded.primaries = 1;
   assert(!mediacodec_color_resolve(&format, &coded, &resolved));
   coded.primaries = 2;
   coded.matrix = 6;
   assert(!mediacodec_color_resolve(&format, &coded, &resolved));
   coded.matrix = 1;
   format.color_standard = 3;
   assert(mediacodec_color_resolve(&format, &coded, &resolved));
   assert(resolved.primaries == 5 && resolved.matrix == 1);
   coded.primaries = 6;
   assert(!mediacodec_color_resolve(&format, &coded, &resolved));
   coded.primaries = 2;
   coded.matrix = 7;
   format.color_standard = 5;
   assert(mediacodec_color_resolve(&format, &coded, &resolved));
   assert(resolved.primaries == 6 && resolved.matrix == 7);
   coded.matrix = 6;
   assert(!mediacodec_color_resolve(&format, &coded, &resolved));
   coded.matrix = 1;
   for (i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++)
   {
      format.color_standard = invalid[i];
      assert(!mediacodec_color_resolve(&format, &coded, &resolved));
   }
}

int main(void)
{
   extended_color();
   colors(8, 1);
   colors(10, 16);
   colors(10, 18);
   puts("PASS: AV1 CICP intersection, native-format and depth refusal, "
        "NV12/P010 BUSY and independent color leases");
   return 0;
}
