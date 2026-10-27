/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifdef NDEBUG
#error VP9 decoder tests require assertions
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../../gfx/common/mediacodec_vp9_decoder.h"
#include "../../../gfx/common/android_image_ndk.h"
static int native, depth, startup_fail, busy, freed, reader_freed, deleted;
static int release_busy, released, export_busy, eos, pump_status,
      timestamp_extra;
static int pump_calls;
static int64_t queued_tag, rendered_tag, last_pts;
static unsigned expected_eotf;
static android_image_view_t held;
static mediacodec_vp9_decoder_t *retired;
static mediacodec_output_format_t format;
static const uint8_t packet[] = {0x82, 1, 2};

static int acquire(void *reader, void **image, int *fd)
{
   assert(reader == &native);
   if (!queued_tag)
      return 0;
   *image = &native;
   *fd = -1;
   return 1;
}

static bool describe(void *image, android_image_view_t *view)
{
   assert(image == &native);
   view->buffer = &native;
   view->width = view->right = view->height = view->bottom = 64;
   view->timestamp_ns = queued_tag * 1000 + timestamp_extra;
   queued_tag = 0;
   return true;
}

static void delete_image(void *image)
{
   assert(image == &native);
   deleted++;
}

static void delete_reader(void *reader)
{
   assert(reader == &native && (freed || startup_fail));
   reader_freed++;
}

android_image_queue_t *android_image_ndk_new(
      unsigned w, unsigned h, unsigned slots, void **window)
{
   android_image_ops_t ops;
   assert(w == 64 && h == 64 && slots == 10);
   memset(&ops, 0, sizeof(ops));
   ops.acquire = acquire;
   ops.describe = describe;
   ops.delete_image = delete_image;
   ops.delete_reader = delete_reader;
   *window = &native;
   return android_image_queue_new(&native, slots, &ops);
}

mediacodec_session_t *mediacodec_session_new(
      JNIEnv *env, const mediacodec_config_t *config, void *window)
{
   unsigned i;
   assert(env && window == &native && config->width == 64
         && config->height == 64);
   assert(!strcmp(config->mime, "video/x-vnd.on2.vp9")
         && config->profile
               == (expected_eotf         ? 4096
                           : depth == 10 ? 4
                                         : 1)
         && config->max_input == 1024);
   assert(config->color_standard == (expected_eotf ? 6 : 0)
         && config->color_transfer
               == (expected_eotf == 16           ? 6
                           : expected_eotf == 18 ? 7
                                                 : 0)
         && config->color_range == 2);
   for (i = 0; i < 3; i++)
      assert(!config->csd[i] && !config->csd_size[i]);
   last_pts = 0;
   return startup_fail ? NULL : (mediacodec_session_t *)&native;
}

int mediacodec_session_submit(mediacodec_session_t *s, const void *data,
      size_t size, int64_t pts, bool end)
{
   assert(s == (mediacodec_session_t *)&native);
   if (busy)
   {
      busy--;
      return 0;
   }
   if (end)
   {
      assert(!data && !size && !pts);
      eos++;
      return 1;
   }
   assert(data == packet && size == sizeof(packet) && pts == last_pts + 1);
   last_pts = pts;
   return 1;
}

int mediacodec_session_pump(mediacodec_session_t *s, int64_t *pts)
{
   pump_calls++;
   assert(s == (mediacodec_session_t *)&native);
   *pts = 0;
   if (!format.serial)
      return 2;
   if (rendered_tag)
   {
      *pts = rendered_tag;
      rendered_tag = 0;
      return 1;
   }
   return pump_status;
}

const mediacodec_output_format_t *mediacodec_session_format(
      const mediacodec_session_t *s)
{
   assert(s == (mediacodec_session_t *)&native);
   format.serial = 1;
   return &format;
}

void mediacodec_session_free(mediacodec_session_t *s)
{
   assert(s == (mediacodec_session_t *)&native);
   freed++;
}

static int export_image(void *context, const android_image_view_t *image,
      const mediacodec_output_format_t *fmt, const rvp9_hdr *header,
      const video_color_metadata_t *color, video_hw_frame_t *out)
{
   assert(context == &native && fmt->serial && header->bit_depth == depth);
   assert(color && color->transfer == expected_eotf);
   if (export_busy)
   {
      export_busy--;
      return 1;
   }
   held = *image;
   out->image = 1;
   out->width = out->height = 64;
   out->color = *color;
   return 0;
}

static bool release(void *context)
{
   assert(context == &native && freed == 1 && reader_freed == 1);
   if (release_busy)
   {
      release_busy--;
      return false;
   }
   released++;
   return true;
}

static void enqueue(void *context, mediacodec_vp9_decoder_t *decoder)
{
   assert(context == &native && !retired);
   retired = decoder;
}

static void reset(void)
{
   startup_fail = busy = freed = reader_freed = deleted = 0;
   release_busy = released = export_busy = eos = pump_status = timestamp_extra
         = 0;
   queued_tag = rendered_tag = last_pts = 0;
   expected_eotf = 0;
   retired = NULL;
   memset(&held, 0, sizeof(held));
   memset(&format, 0, sizeof(format));
   format.width = format.height = format.right = format.bottom = 64;
}

static void execute(unsigned bits)
{
   static rvp9_hdr header;
   static rvp9_hw_picture_t picture;
   mediacodec_vp9_import_t importer;
   rvp9_hw_ops_t ops;
   video_hw_frame_t frame;
   video_color_metadata_t container;
   unsigned fault;
   depth = (int)bits;
   reset();
   memset(&header, 0, sizeof(header));
   header.w = header.h = 64;
   header.bit_depth = depth;
   header.profile = depth == 10 ? 2 : 0;
   header.subsampling_x = header.subsampling_y = header.show_frame = 1;
   memset(&importer, 0, sizeof(importer));
   importer.context = &native;
   importer.export_image = export_image;
   importer.release = release;
   importer.enqueue = enqueue;
   assert(!mediacodec_vp9_decoder_open(
         (JNIEnv *)&native, &header, 9, 1024, &importer, &ops));
   assert(!ops.decoder && !reader_freed);
   startup_fail = 1;
   assert(!mediacodec_vp9_decoder_open(
         (JNIEnv *)&native, &header, 10, 1024, &importer, &ops));
   assert(!ops.decoder && reader_freed == 1 && !freed && !released && !retired);
   reset();
   assert(mediacodec_vp9_decoder_open(
         (JNIEnv *)&native, &header, 10, 1024, &importer, &ops));
   memset(&picture, 0, sizeof(picture));
   picture.header = header;
   busy = 1;
   assert(ops.decode(ops.decoder, packet, sizeof(packet), &picture) == 1
         && !last_pts);
   assert(!ops.decode(ops.decoder, packet, sizeof(packet), &picture)
         && last_pts == 1);
   assert(ops.decode(ops.decoder, packet, 1025, &picture) < 0 && last_pts == 1);
   picture.header.show_frame = 0;
   picture.slot = 1;
   assert(!ops.decode(ops.decoder, packet, sizeof(packet), &picture)
         && last_pts == 2);
   queued_tag = rendered_tag = 1;
   picture.header.show_frame = picture.header.show_existing_frame = 1;
   assert(!ops.decode(ops.decoder, packet, sizeof(packet), &picture)
         && last_pts == 3);
   assert(!deleted);
   queued_tag = 2;
   assert(ops.output(ops.decoder, 1, &frame) == 1 && deleted == 1);
   queued_tag = 3;
   export_busy = 1;
   assert(ops.output(ops.decoder, 1, &frame) == 1 && held.ticket == NULL
         && deleted == 2);
   assert(!ops.output(ops.decoder, 1, &frame) && held.ticket);
   mediacodec_vp9_decoder_close(&ops);
   assert(!ops.decoder && retired && !freed);
   assert(!mediacodec_vp9_decoder_retire_poll(retired));
   assert(android_image_queue_return(held.ticket, held.generation, -1));
   release_busy = 1;
   assert(!mediacodec_vp9_decoder_retire_poll(retired) && freed == 1
         && reader_freed == 1);
   assert(mediacodec_vp9_decoder_retire_poll(retired) && released == 1);
   reset();
   assert(mediacodec_vp9_decoder_open(
         (JNIEnv *)&native, &header, 10, 1024, &importer, &ops));
   picture.header = header;
   picture.slot = 1;
   assert(!ops.decode(ops.decoder, packet, sizeof(packet), &picture));
   queued_tag = rendered_tag = 1;
   picture.header.show_frame = 0;
   picture.slot = 2;
   assert(!ops.decode(ops.decoder, packet, sizeof(packet), &picture));
   assert(mediacodec_vp9_decoder_finish(ops.decoder, false) == 1 && eos == 1);
   pump_status = 3;
   assert(mediacodec_vp9_decoder_finish(ops.decoder, false) == 1);
   assert(!ops.output(ops.decoder, 1, &frame) && held.timestamp_ns == 1000);
   assert(!mediacodec_vp9_decoder_finish(ops.decoder, false));
   mediacodec_vp9_decoder_close(&ops);
   assert(!mediacodec_vp9_decoder_retire_poll(retired));
   assert(android_image_queue_return(held.ticket, held.generation, -1));
   assert(mediacodec_vp9_decoder_retire_poll(retired));
   reset();
   assert(mediacodec_vp9_decoder_open(
         (JNIEnv *)&native, &header, 10, 1024, &importer, &ops));
   picture.header = header;
   picture.slot = 1;
   assert(!ops.decode(ops.decoder, packet, sizeof(packet), &picture));
   busy = 1;
   assert(mediacodec_vp9_decoder_finish(ops.decoder, true) == 1 && !eos);
   assert(mediacodec_vp9_decoder_finish(ops.decoder, true) == 1 && eos == 1);
   pump_status = 3;
   assert(!mediacodec_vp9_decoder_finish(ops.decoder, true));
   mediacodec_vp9_decoder_close(&ops);
   assert(mediacodec_vp9_decoder_retire_poll(retired));
   reset();
   assert(mediacodec_vp9_decoder_open(
         (JNIEnv *)&native, &header, 10, 1024, &importer, &ops));
   picture.header = header;
   assert(!ops.decode(ops.decoder, packet, sizeof(packet), &picture));
   busy = 1;
   assert(mediacodec_vp9_decoder_drain(ops.decoder) == 1 && !eos);
   assert(!mediacodec_vp9_decoder_drain(ops.decoder) && eos == 1);
   pump_status = 3;
   assert(mediacodec_vp9_decoder_finish(ops.decoder, false) < 0);
   assert(ops.output(ops.decoder, 1, &frame) < 0);
   mediacodec_vp9_decoder_close(&ops);
   assert(mediacodec_vp9_decoder_retire_poll(retired));
   reset();
   assert(mediacodec_vp9_decoder_open(
         (JNIEnv *)&native, &header, 10, 1024, &importer, &ops));
   assert(!ops.decode(ops.decoder, packet, sizeof(packet), &picture));
   rendered_tag = 1;
   assert(!mediacodec_vp9_decoder_drain(ops.decoder));
   pump_status = 3;
   assert(ops.output(ops.decoder, 1, &frame) == 1);
   {
      int calls = pump_calls;
      unsigned i;
      for (i = 0; i < 32; i++)
         assert(ops.output(ops.decoder, 1, &frame) == 1);
      assert(pump_calls == calls);
   }
   queued_tag = 1;
   assert(!ops.output(ops.decoder, 1, &frame));
   mediacodec_vp9_decoder_close(&ops);
   assert(!mediacodec_vp9_decoder_retire_poll(retired));
   assert(android_image_queue_return(held.ticket, held.generation, -1));
   assert(mediacodec_vp9_decoder_retire_poll(retired));
   reset();
   assert(mediacodec_vp9_decoder_open(
         (JNIEnv *)&native, &header, 10, 1024, &importer, &ops));
   assert(!ops.decode(ops.decoder, packet, sizeof(packet), &picture));
   queued_tag = 1;
   timestamp_extra = 1;
   assert(ops.output(ops.decoder, 1, &frame) < 0);
   mediacodec_vp9_decoder_close(&ops);
   assert(mediacodec_vp9_decoder_retire_poll(retired));
   for (fault = 0; fault < 2; fault++)
   {
      reset();
      assert(mediacodec_vp9_decoder_open(
            (JNIEnv *)&native, &header, 10, 1024, &importer, &ops));
      assert(!ops.decode(ops.decoder, packet, sizeof(packet), &picture));
      if (fault)
      {
         format.width = 65;
         format.serial = 0;
      }
      else
         queued_tag = 2;
      assert(ops.output(ops.decoder, 1, &frame) < 0);
      mediacodec_vp9_decoder_close(&ops);
      assert(mediacodec_vp9_decoder_retire_poll(retired));
   }
   for (fault = 16; fault <= 18; fault += 2)
   {
      reset();
      expected_eotf = fault;
      header.color_space = 5;
      memset(&container, 0, sizeof(container));
      container.primaries = container.matrix = 9;
      container.transfer = fault;
      container.range = 1;
      container.max_cll = 1000;
      if (depth == 8)
      {
         assert(!mediacodec_vp9_decoder_open_color((JNIEnv *)&native, &header,
               &container, 10, 1024, &importer, &ops));
         assert(!ops.decoder && !reader_freed && !freed);
         continue;
      }
      assert(mediacodec_vp9_decoder_open_color(
            (JNIEnv *)&native, &header, &container, 10, 1024, &importer, &ops));
      picture.header = header;
      assert(!ops.decode(ops.decoder, packet, sizeof(packet), &picture));
      queued_tag = 1;
      assert(!ops.output(ops.decoder, 1, &frame));
      assert(frame.color.transfer == fault && frame.color.max_cll == 1000);
      mediacodec_vp9_decoder_close(&ops);
      assert(!mediacodec_vp9_decoder_retire_poll(retired));
      assert(android_image_queue_return(held.ticket, held.generation, -1));
      assert(mediacodec_vp9_decoder_retire_poll(retired));
   }
}

int main(void)
{
   execute(8);
   execute(10);
   puts("PASS: VP9 Surface owner startup, hidden/repeat timestamps, BUSY, EOS, "
        "delayed delivery and leased retirement; no Android device execution");
   return 0;
}
