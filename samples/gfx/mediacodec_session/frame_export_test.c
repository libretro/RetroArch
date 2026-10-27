/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <formats/image.h>
#include "../../../gfx/common/mediacodec_frame_export.h"
#include "../../../gfx/common/mediacodec_color.h"
static int native, deleted;
static int acquire(void *reader, void **image, int *fd)
{
   assert(reader == &native);
   *image = &native;
   *fd = -1;
   return 1;
}
static bool describe(void *image, android_image_view_t *view)
{
   assert(image == &native);
   view->buffer = &native;
   view->width = view->right = view->buffer_width = 64;
   view->height = view->bottom = view->buffer_height = 32;
   return true;
}
static void delete_image(void *image)
{
   assert(image == &native);
   deleted++;
}
static void delete_reader(void *reader)
{
   assert(reader == &native);
}
int main(void)
{
   mediacodec_output_format_t format;
   video_color_metadata_t color, out;
   rh265_video_color_t coded;
   mediacodec_frame_exporter_t *exporter;
   android_image_queue_t *queue;
   android_image_ops_t ops;
   android_image_view_t image;
   android_frame_view_t view;
   video_hw_frame_t frame;
   unsigned i;
   memset(&format, 0, sizeof(format));
   memset(&color, 0, sizeof(color));
   format.color_standard = 6;
   format.color_transfer = 6;
   format.color_range = 2;
   assert(mediacodec_color_resolve(&format, NULL, &out));
   assert(out.primaries == 9 && out.matrix == 9 && out.transfer == 16
         && out.range == 1);
   color = out;
   color.max_cll = 1000;
   assert(mediacodec_color_resolve(&format, &color, &color)
         && color.max_cll == 1000);
   format.color_transfer = 7;
   assert(!mediacodec_color_resolve(&format, &color, &out) && !out.transfer);
   format.color_transfer = 3;
   assert(!mediacodec_color_resolve(&format, &color, &out));
   memset(&color, 0, sizeof(color));
   format.color_standard = 1;
   for (i = 0; i < 4; i++)
   {
      static const unsigned transfers[] = {1, 6, 14, 15};
      color.transfer = transfers[i];
      assert(mediacodec_color_resolve(&format, &color, &out));
      assert(out.transfer == transfers[i]);
   }
   color.transfer = 13;
   assert(!mediacodec_color_resolve(&format, &color, &out));
   color.transfer = 2;
   format.color_standard = 2;
   color.matrix = 6;
   color.primaries = 5;
   assert(mediacodec_color_resolve(&format, &color, &out) && out.matrix == 6
         && out.transfer == 6);
   format.color_standard = 4;
   assert(!mediacodec_color_resolve(&format, &color, &out));
   format.color_standard = 113;
   assert(!mediacodec_color_resolve(&format, NULL, &out));
   memset(&format, 0, sizeof(format));
   assert(mediacodec_color_resolve(&format, NULL, &out));
   assert(!out.range && !out.transfer);
   memset(&ops, 0, sizeof(ops));
   ops.acquire = acquire;
   ops.describe = describe;
   ops.delete_image = delete_image;
   ops.delete_reader = delete_reader;
   queue = android_image_queue_new(&native, 1, &ops);
   assert(queue);
   assert(android_image_queue_acquire(queue, &image) == 1);
   exporter = mediacodec_frame_exporter_new(1, IMAGE_PIXFMT_P010);
   assert(exporter);
   memset(&coded, 0, sizeof(coded));
   coded.description_present = coded.video_signal_present = 1;
   coded.primaries = coded.matrix = 9;
   coded.transfer = 16;
   format.serial = 1;
   format.color_standard = 6;
   format.color_transfer = 3;
   format.color_range = 2;
   memset(&frame, 0x55, sizeof(frame));
   assert(
         mediacodec_hevc_frame_export(exporter, &image, &format, &coded, &frame)
               == -1
         && frame.width == 0x55555555);
   assert(!android_image_queue_poll(queue));
   format.color_transfer = 6;
   assert(!mediacodec_hevc_frame_export(
         exporter, &image, &format, &coded, &frame));
   assert(frame.color.transfer == 16 && frame.color.range == 1
         && frame.pixfmt == IMAGE_PIXFMT_P010);
   assert(android_frame_claim(
         frame.owner, frame.done, frame.done_value, frame.image, &view));
   assert(view.format.color_transfer == 6 && view.color.matrix == 9);
   assert(!mediacodec_frame_exporter_free(exporter));
   assert(android_frame_finish(frame.owner, frame.done, frame.done_value, -1));
   assert(mediacodec_frame_exporter_free(exporter));
   assert(android_image_queue_poll(queue) == 1);
   android_image_queue_close(queue);
   assert(android_image_queue_free(queue) && deleted == 1);
   puts("PASS: native/coded color conflict refusal and HEVC-to-renderer ticket "
        "export/retirement");
   return 0;
}
