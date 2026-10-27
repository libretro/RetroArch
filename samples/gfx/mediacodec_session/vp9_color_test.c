/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifdef NDEBUG
#error VP9 color tests require assertions
#endif
#define main hevc_frame_export_test_main
#include "frame_export_test.c"
#undef main

static void colors(unsigned depth)
{
   static rvp9_hdr header;
   video_color_metadata_t container, result;
   mediacodec_output_format_t format;
   memset(&header, 0, sizeof(header));
   header.profile = depth == 10 ? 2 : 0;
   header.bit_depth = (int)depth;
   header.subsampling_x = header.subsampling_y = 1;
   header.color_space = 5;
   memset(&container, 0, sizeof(container));
   container.primaries = container.matrix = 9;
   container.transfer = 16;
   container.range = 1;
   container.max_cll = 4000;
   container.max_fall = 700;
   container.mastering.present = 0x303;
   container.mastering.chromaticity[0] = 15000;
   container.mastering.luminance_max = 20000000;
   assert(mediacodec_vp9_color(&header, &container, &result));
   assert(result.transfer == 16 && result.range == 1 && result.max_cll == 4000
         && result.mastering.chromaticity[0] == 15000
         && result.mastering.present == 0x303);
   assert(mediacodec_vp9_color(&header, &container, &container));
   assert(container.max_fall == 700
         && container.mastering.luminance_max == 20000000);
   assert(mediacodec_vp9_color(&header, NULL, &result));
   assert(result.primaries == 9 && result.matrix == 9 && !result.transfer
         && result.range == 1);
   container.transfer = 18;
   memset(&format, 0, sizeof(format));
   format.color_standard = 6;
   format.color_transfer = 7;
   format.color_range = 2;
   assert(mediacodec_vp9_color(&header, &container, &result));
   assert(mediacodec_color_resolve(&format, &result, &result)
         && result.transfer == 18);
   format.color_transfer = 6;
   assert(!mediacodec_color_resolve(&format, &result, &result)
         && !result.transfer);
   header.color_space = 2;
   assert(!mediacodec_vp9_color(&header, &container, &result)
         && !result.primaries);
   header.color_space = 5;
   container.range = 2;
   assert(!mediacodec_vp9_color(&header, &container, &result) && !result.range);
   memset(&container, 0, sizeof(container));
   header.color_space = 1;
   container.primaries = container.matrix = 5;
   container.transfer = 6;
   assert(mediacodec_vp9_color(&header, &container, &result));
   assert(result.primaries == 5 && result.matrix == 5 && result.transfer == 6);
   assert(mediacodec_vp9_color(&header, NULL, &result));
   assert(!result.primaries && result.matrix == 6 && !result.transfer);
   container.primaries = container.matrix = 9;
   assert(!mediacodec_vp9_color(&header, &container, &result));
   header.color_space = 3;
   assert(mediacodec_vp9_color(&header, NULL, &result) && result.primaries == 6
         && result.matrix == 6);
   header.color_space = 4;
   assert(mediacodec_vp9_color(&header, NULL, &result) && result.primaries == 7
         && result.matrix == 7);
   header.color_space = 0;
   container.transfer = 16;
   assert(mediacodec_vp9_color(&header, &container, &result)
         && result.transfer == 16 && result.matrix == 9);
   header.color_space = 6;
   assert(!mediacodec_vp9_color(&header, NULL, &result));
   header.color_space = 7;
   assert(!mediacodec_vp9_color(&header, NULL, &result));
   header.color_space = 2;
   header.color_range = 2;
   assert(!mediacodec_vp9_color(&header, NULL, &result));
   assert(!mediacodec_vp9_color(NULL, NULL, &result));
   assert(!mediacodec_vp9_color(&header, NULL, NULL));
}

static void input_colors(void)
{
   video_color_metadata_t color;
   mediacodec_config_t config, unchanged;
   memset(&color, 0, sizeof(color));
   memset(&config, 0x55, sizeof(config));
   color.primaries = color.matrix = 9;
   color.transfer = 16;
   color.range = 1;
   assert(mediacodec_color_configure(&color, &config));
   assert(config.color_standard == 6 && config.color_transfer == 6
         && config.color_range == 2 && config.profile == 0x55555555);
   unchanged = config;
   color.primaries = 1;
   assert(!mediacodec_color_configure(&color, &config));
   assert(!memcmp(&config, &unchanged, sizeof(config)));
   color.primaries = 9;
   color.transfer = 13;
   assert(!mediacodec_color_configure(&color, &config));
   color.transfer = 18;
   color.range = 2;
   assert(mediacodec_color_configure(&color, &config));
   assert(config.color_transfer == 7 && config.color_range == 1);
   color.range = 3;
   unchanged = config;
   assert(!mediacodec_color_configure(&color, &config));
   assert(!memcmp(&config, &unchanged, sizeof(config)));
   color.range = 0;
   color.primaries = 0;
   color.transfer = 2;
   assert(mediacodec_color_configure(&color, &config));
   assert(!config.color_standard && !config.color_transfer
         && !config.color_range);
   assert(!mediacodec_color_configure(NULL, &config));
   assert(!mediacodec_color_configure(&color, NULL));
}

static int reader, tokens[2];
static unsigned produced, retired_images;

static int acquire_color(void *native_reader, void **image, int *fd)
{
   assert(native_reader == &reader);
   if (produced == 2)
      return 0;
   *image = &tokens[produced++];
   *fd = -1;
   return 1;
}

static bool describe_color(void *image, android_image_view_t *view)
{
   assert(image == &tokens[0] || image == &tokens[1]);
   view->buffer = image;
   view->width = view->right = view->buffer_width = 64;
   view->height = view->bottom = view->buffer_height = 32;
   return true;
}

static void delete_color(void *image)
{
   assert(image == &tokens[0] || image == &tokens[1]);
   retired_images++;
}

static void close_reader(void *native_reader)
{
   assert(native_reader == &reader);
}

static void leases(void)
{
   static rvp9_hdr header;
   static android_frame_view_t pq, hlg;
   video_color_metadata_t container, refinement;
   mediacodec_output_format_t format;
   android_image_ops_t ops;
   android_image_queue_t *queue;
   mediacodec_frame_exporter_t *exporter;
   android_image_view_t first, second;
   video_hw_frame_t frame, pending;
   memset(&header, 0, sizeof(header));
   header.profile = 2;
   header.bit_depth = 10;
   header.subsampling_x = header.subsampling_y = 1;
   header.color_space = 5;
   memset(&container, 0, sizeof(container));
   container.primaries = container.matrix = 9;
   container.transfer = 16;
   container.range = 1;
   container.max_cll = 4000;
   container.max_fall = 700;
   container.mastering.present = 0x101;
   container.mastering.chromaticity[0] = 15000;
   container.mastering.luminance_max = 20000000;
   memset(&format, 0, sizeof(format));
   format.width = format.right = 64;
   format.height = format.bottom = 32;
   format.serial = 1;
   format.color_standard = 6;
   format.color_transfer = 3;
   format.color_range = 2;
   memset(&ops, 0, sizeof(ops));
   ops.acquire = acquire_color;
   ops.describe = describe_color;
   ops.delete_image = delete_color;
   ops.delete_reader = close_reader;
   queue = android_image_queue_new(&reader, 2, &ops);
   assert(queue);
   exporter = mediacodec_frame_exporter_new(1, IMAGE_PIXFMT_P010);
   assert(exporter);
   assert(android_image_queue_acquire(queue, &first) == 1);
   memset(&frame, 0x55, sizeof(frame));
   assert(mediacodec_vp9_frame_export(
                exporter, &first, &format, &header, &container, &frame)
         == -1);
   assert(frame.width == 0x55555555 && !android_image_queue_poll(queue));
   format.color_transfer = 6;
   header.profile = 0;
   header.bit_depth = 8;
   assert(mediacodec_vp9_frame_export(
                exporter, &first, &format, &header, &container, &frame)
         == -1);
   assert(frame.width == 0x55555555 && !android_image_queue_poll(queue));
   header.profile = 2;
   header.bit_depth = 10;
   assert(!mediacodec_vp9_frame_export(
         exporter, &first, &format, &header, &container, &frame));
   assert(frame.color.transfer == 16 && frame.color.max_cll == 4000
         && frame.pixfmt == IMAGE_PIXFMT_P010);
   assert(android_image_queue_acquire(queue, &second) == 1);
   memset(&pending, 0x55, sizeof(pending));
   assert(mediacodec_vp9_frame_export(
                exporter, &second, &format, &header, &container, &pending)
         == 1);
   assert(pending.width == 0x55555555 && !android_image_queue_poll(queue));
   assert(android_frame_claim(
         frame.owner, frame.done, frame.done_value, frame.image, &pq));
   assert(pq.color.transfer == 16 && pq.color.max_fall == 700
         && pq.color.mastering.chromaticity[0] == 15000);
   memset(&refinement, 0, sizeof(refinement));
   refinement.transfer = 18;
   assert(!android_frame_resolve_color(&pq, &refinement)
         && pq.color.transfer == 16);
   refinement.transfer = 0;
   refinement.max_cll = 1000;
   assert(android_frame_resolve_color(&pq, &refinement));
   assert(pq.color.transfer == 16 && pq.color.max_cll == 1000
         && pq.color.mastering.present == 0x101);
   assert(android_frame_finish(frame.owner, frame.done, frame.done_value, -1));
   assert(android_image_queue_poll(queue) == 1);
   container.transfer = 18;
   format.color_transfer = 7;
   assert(!mediacodec_vp9_frame_export(
         exporter, &second, &format, &header, &container, &frame));
   assert(android_frame_claim(
         frame.owner, frame.done, frame.done_value, frame.image, &hlg));
   assert(hlg.color.transfer == 18 && pq.color.transfer == 16);
   assert(!mediacodec_frame_exporter_free(exporter));
   assert(android_frame_finish(frame.owner, frame.done, frame.done_value, -1));
   assert(mediacodec_frame_exporter_free(exporter));
   assert(android_image_queue_poll(queue) == 1 && retired_images == 2);
   android_image_queue_close(queue);
   assert(android_image_queue_free(queue));
}

static void encoded_transfer(void)
{
   mediacodec_frame_exporter_t *exporter, *wrong;
   mediacodec_output_format_t format;
   android_image_ops_t ops;
   android_image_queue_t *queue;
   android_image_view_t image;
   video_color_metadata_t color;
   video_hw_frame_t frame;
   memset(&format, 0, sizeof(format));
   memset(&ops, 0, sizeof(ops));
   memset(&color, 0, sizeof(color));
   format.width = format.right = 64;
   format.height = format.bottom = 32;
   format.serial = format.color_standard = 1;
   format.color_range = 2;
   color.primaries = color.matrix = color.range = 1;
   ops.acquire = acquire;
   ops.describe = describe;
   ops.delete_image = delete_image;
   ops.delete_reader = delete_reader;
   queue = android_image_queue_new(&native, 1, &ops);
   exporter = mediacodec_frame_exporter_new(1, IMAGE_PIXFMT_NV12);
   wrong = mediacodec_frame_exporter_new(1, IMAGE_PIXFMT_P010);
   assert(queue && exporter && wrong);
   assert(android_image_queue_acquire(queue, &image) == 1);
   memset(&frame, 0x55, sizeof(frame));
   assert(mediacodec_frame_export_color(wrong, &image, &format, &color, &frame) < 0);
   assert(frame.width == 0x55555555 && !android_image_queue_poll(queue));
   assert(mediacodec_frame_exporter_free(wrong));
   color.max_cll = 1000;
   assert(mediacodec_frame_export_color(exporter, &image, &format, &color, &frame) < 0);
   color.max_cll = 0;
   assert(!mediacodec_frame_export_color(exporter, &image, &format, &color, &frame));
   assert(!frame.color.transfer && frame.pixfmt == IMAGE_PIXFMT_NV12);
   video_hw_frame_abandon(&frame);
   assert(android_image_queue_poll(queue) == 1);
   assert(android_image_queue_acquire(queue, &image) == 1);
   color.transfer = 2;
   assert(!mediacodec_frame_export_color(exporter, &image, &format, &color, &frame));
   assert(frame.color.transfer == 2);
   video_hw_frame_abandon(&frame);
   assert(android_image_queue_poll(queue) == 1);
   assert(mediacodec_frame_exporter_free(exporter));
   android_image_queue_close(queue);
   assert(android_image_queue_free(queue));
}

int main(void)
{
   assert(!hevc_frame_export_test_main());
   colors(8);
   colors(10);
   input_colors();
   leases();
   encoded_transfer();
   puts("PASS: VP9 coded/container/native color conflicts, PQ/HLG lease "
        "metadata, mastering/light refinement and retirement; no device "
        "pixels");
   return 0;
}
