/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifdef NDEBUG
#error AV1 decoder tests require assertions
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../../gfx/common/mediacodec_av1_decoder.h"
#include "../../../gfx/common/android_image_ndk.h"
static const uint8_t *expected_av1c;
static size_t expected_av1c_size;
static int native, depth, startup_fail, busy, freed, reader_freed, deleted;
static int release_busy, released, export_busy, eos, pump_status,
      timestamp_extra;
static int pump_calls;
static unsigned bootstrap_submits;
static int bootstrap_busy;
static int64_t queued_tag, rendered_tag, last_pts;
static unsigned expected_eotf, image_extent;
static android_image_view_t held;
static mediacodec_av1_decoder_t *retired;
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
   view->width = view->height = 64;
   view->right = view->bottom = image_extent;
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
   assert(w == 64 && h == 64 && slots == 9);
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
   assert(!strcmp(config->mime, "video/av01")
         && config->profile
               == (expected_eotf == 16   ? 4096
                           : depth == 10 ? 2
                                         : 1)
         && config->max_input == 1024);
   assert(config->color_standard == (expected_eotf ? 6 : 1)
         && config->color_transfer
               == (expected_eotf == 16           ? 6
                           : expected_eotf == 18 ? 7
                                                 : 3)
         && config->color_range == 2);
   assert(config->csd[0] == expected_av1c
         && config->csd_size[0] == expected_av1c_size);
   for (i = 1; i < 3; i++)
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
   if (data != packet)
   {
      assert(!bootstrap_submits && !pts && data != expected_av1c + 4
            && size == expected_av1c_size - 4
            && !memcmp(data, expected_av1c + 4, size));
      bootstrap_submits++;
      if (bootstrap_busy)
         busy = 1;
      return 1;
   }
   assert(bootstrap_submits == 1);
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
      const mediacodec_output_format_t *fmt, const rav1_sequence_t *sequence,
      const video_color_metadata_t *color, video_hw_frame_t *out)
{
   assert(context == &native && fmt->serial
         && sequence->bit_depth == (unsigned)depth);
   assert(color && color->transfer == (expected_eotf ? expected_eotf : 1));
   assert(fmt->right - fmt->left == image->right - image->left
         && fmt->bottom - fmt->top == image->bottom - image->top);
   if (export_busy)
   {
      export_busy--;
      return 1;
   }
   held = *image;
   out->image = 1;
   out->width = image->right - image->left;
   out->height = image->bottom - image->top;
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

static void enqueue(void *context, mediacodec_av1_decoder_t *decoder)
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
   bootstrap_submits = bootstrap_busy = 0;
   expected_eotf = 0;
   image_extent = 64;
   retired = NULL;
   memset(&held, 0, sizeof(held));
   memset(&format, 0, sizeof(format));
   format.width = format.height = format.right = format.bottom = 64;
}

#include "av1_config_test.h"
static rav1_sequence_t sequence;
static rav1_hw_picture_t picture;
static mediacodec_av1_import_t importer;
static rav1_hw_ops_t ops;
static video_hw_frame_t frame;

static bool open_owner(const video_color_metadata_t *color)
{
   return mediacodec_av1_decoder_open((JNIEnv *)&native, expected_av1c,
         expected_av1c_size, color, 9, 1024, &importer, &ops);
}

static void close_owner(void)
{
   mediacodec_av1_decoder_close(&ops);
   assert(!ops.decoder && retired && !freed);
   assert(mediacodec_av1_decoder_retire_poll(retired));
}

static void execute(unsigned bits)
{
   rav1_config_t record;
   video_color_metadata_t color;
   unsigned fault;
   depth = (int)bits;
   expected_av1c = bits == 8 ? av1c_8_1 : av1c_10_1;
   expected_av1c_size = bits == 8 ? sizeof(av1c_8_1) : sizeof(av1c_10_1);
   assert(rav1_config_parse(&record, expected_av1c, expected_av1c_size));
   assert(rav1_config_sequence(&sequence, &record));
   assert(sequence.max_width == 64 && sequence.max_height == 64);
   memset(&picture, 0, sizeof(picture));
   picture.setup.size.width = picture.setup.size.upscaled_width = 64;
   picture.setup.size.height = 64;
   picture.header.show_frame = 1;
   importer.context = &native;
   importer.export_image = export_image;
   importer.release = release;
   importer.enqueue = enqueue;
   reset();
   assert(!mediacodec_av1_decoder_open((JNIEnv *)&native, expected_av1c,
         expected_av1c_size, NULL, 8, 1024, &importer, &ops));
   assert(!ops.decoder && !reader_freed);
   assert(!mediacodec_av1_decoder_open((JNIEnv *)&native, expected_av1c,
         expected_av1c_size, NULL, 9, 1, &importer, &ops));
   assert(!ops.decoder && !reader_freed);
   {
      uint8_t bad[128];
      size_t n;
      assert(expected_av1c_size <= sizeof(bad));
      for (n = 0; n < expected_av1c_size; n++)
         assert(!mediacodec_av1_decoder_open((JNIEnv *)&native, expected_av1c,
               n, NULL, 9, 1024, &importer, &ops));
      memcpy(bad, expected_av1c, expected_av1c_size);
      bad[0] = 0;
      assert(!mediacodec_av1_decoder_open((JNIEnv *)&native, bad,
            expected_av1c_size, NULL, 9, 1024, &importer, &ops));
      memset(&color, 0, sizeof(color));
      color.range = 2;
      assert(!open_owner(&color) && !reader_freed);
      color.range = 1;
      color.primaries = 9;
      assert(!open_owner(&color) && !reader_freed);
   }
   reset();
   assert(open_owner(NULL));
   bootstrap_busy = 1;
   assert(ops.decode(ops.decoder, packet, sizeof(packet), &sequence, &picture)
               == 1
         && bootstrap_submits == 1 && !last_pts);
   close_owner();
   assert(freed == 1 && reader_freed == 1);
   reset();
   startup_fail = 1;
   assert(!open_owner(NULL));
   assert(reader_freed == 1 && !freed && !retired);
   reset();
   assert(open_owner(NULL));
   busy = 1;
   assert(ops.decode(ops.decoder, packet, sizeof(packet), &sequence, &picture)
               == 1
         && !last_pts);
   bootstrap_busy = 1;
   assert(ops.decode(ops.decoder, packet, sizeof(packet), &sequence, &picture)
               == 1
         && !last_pts && bootstrap_submits == 1);
   assert(!ops.decode(ops.decoder, packet, sizeof(packet), &sequence, &picture)
         && last_pts == 1);
   assert(ops.decode(ops.decoder, packet, 1025, &sequence, &picture) < 0
         && last_pts == 1);
   picture.header.show_frame = 0;
   picture.slot = 1;
   assert(!ops.decode(ops.decoder, packet, sizeof(packet), &sequence, &picture)
         && last_pts == 2);
   queued_tag = rendered_tag = 1;
   picture.header.show_existing = 1;
   picture.grain.params.apply = 1;
   picture.grain.params.seed = 17;
   assert(!ops.decode(ops.decoder, packet, sizeof(packet), &sequence, &picture)
         && last_pts == 3);
   queued_tag = 2;
   assert(ops.output(ops.decoder, 1, &picture.grain.params, &frame) == 1
         && deleted == 1);
   queued_tag = 3;
   export_busy = 1;
   assert(ops.output(ops.decoder, 1, &picture.grain.params, &frame) == 1
         && !held.ticket && deleted == 2);
   assert(!ops.output(ops.decoder, 1, &picture.grain.params, &frame)
         && held.ticket);
   mediacodec_av1_decoder_close(&ops);
   assert(!mediacodec_av1_decoder_retire_poll(retired));
   assert(android_image_queue_return(held.ticket, held.generation, -1));
   release_busy = 1;
   assert(!mediacodec_av1_decoder_retire_poll(retired) && freed == 1
         && reader_freed == 1);
   assert(mediacodec_av1_decoder_retire_poll(retired) && released == 1);
   for (fault = 0; fault < 7; fault++)
   {
      reset();
      assert(open_owner(NULL));
      assert(!ops.decode(
            ops.decoder, packet, sizeof(packet), &sequence, &picture));
      if (fault == 0)
         queued_tag = 2;
      if (fault == 1)
      {
         queued_tag = 1;
         timestamp_extra = 1;
      }
      if (fault == 2)
      {
         format.width = 65;
         format.serial = 0;
      }
      if (fault == 3)
         picture.grain.params.seed++;
      if (fault == 4)
      {
         queued_tag = 1;
         format.right = 63;
         format.serial = 0;
      }
      if (fault < 5)
         assert(ops.output(ops.decoder, 1, &picture.grain.params, &frame) < 0);
      else
      {
         assert(mediacodec_av1_decoder_finish(ops.decoder, fault == 6) == 1
               && eos == 1);
         pump_status = 3;
         if (fault == 5)
            assert(mediacodec_av1_decoder_finish(ops.decoder, false) < 0);
         else
            assert(!mediacodec_av1_decoder_finish(ops.decoder, true));
      }
      close_owner();
   }
   reset();
   assert(open_owner(NULL));
   assert(
         !ops.decode(ops.decoder, packet, sizeof(packet), &sequence, &picture));
   rendered_tag = 1;
   assert(mediacodec_av1_decoder_finish(ops.decoder, false) == 1);
   pump_status = 3;
   assert(ops.output(ops.decoder, 1, &picture.grain.params, &frame) == 1);
   {
      int calls = pump_calls;
      unsigned i;
      for (i = 0; i < 32; i++)
         assert(ops.output(ops.decoder, 1, &picture.grain.params, &frame) == 1);
      assert(pump_calls == calls);
   }
   queued_tag = 1;
   assert(!ops.output(ops.decoder, 1, &picture.grain.params, &frame));
   assert(!mediacodec_av1_decoder_finish(ops.decoder, false));
   mediacodec_av1_decoder_close(&ops);
   assert(!mediacodec_av1_decoder_retire_poll(retired));
   assert(android_image_queue_return(held.ticket, held.generation, -1));
   assert(mediacodec_av1_decoder_retire_poll(retired));
   reset();
   assert(open_owner(NULL));
   picture.setup.size.upscaled_width = picture.setup.size.width = 32;
   picture.setup.size.height = 32;
   assert(
         !ops.decode(ops.decoder, packet, sizeof(packet), &sequence, &picture));
   image_extent = format.right = format.bottom = 32;
   format.serial = 0;
   queued_tag = 1;
   assert(!ops.output(ops.decoder, 1, &picture.grain.params, &frame));
   assert(frame.width == 32 && frame.height == 32);
   mediacodec_av1_decoder_close(&ops);
   assert(!mediacodec_av1_decoder_retire_poll(retired));
   assert(android_image_queue_return(held.ticket, held.generation, -1));
   assert(mediacodec_av1_decoder_retire_poll(retired));
   picture.setup.size.upscaled_width = picture.setup.size.width = 64;
   picture.setup.size.height = 64;
   reset();
   assert(open_owner(NULL));
   assert(
         !ops.decode(ops.decoder, packet, sizeof(packet), &sequence, &picture));
   queued_tag = rendered_tag = 1;
   export_busy = 1;
   assert(ops.output(ops.decoder, 1, &picture.grain.params, &frame) == 1);
   format.right = format.bottom = 32;
   format.serial = 0;
   assert(!ops.output(ops.decoder, 1, &picture.grain.params, &frame));
   assert(frame.width == 64 && frame.height == 64);
   mediacodec_av1_decoder_close(&ops);
   assert(!mediacodec_av1_decoder_retire_poll(retired));
   assert(android_image_queue_return(held.ticket, held.generation, -1));
   assert(mediacodec_av1_decoder_retire_poll(retired));
   for (fault = 16; fault <= 18; fault += 2)
   {
      reset();
      expected_eotf = fault;
      expected_av1c = bits == 8 ? (fault == 16 ? av1c_8_16 : av1c_8_18)
                                : (fault == 16 ? av1c_10_16 : av1c_10_18);
      expected_av1c_size = bits == 8
            ? (fault == 16 ? sizeof(av1c_8_16) : sizeof(av1c_8_18))
            : (fault == 16 ? sizeof(av1c_10_16) : sizeof(av1c_10_18));
      assert(rav1_config_parse(&record, expected_av1c, expected_av1c_size));
      assert(rav1_config_sequence(&sequence, &record));
      memset(&color, 0, sizeof(color));
      color.primaries = color.matrix = 9;
      color.transfer = fault;
      color.range = 1;
      color.max_cll = 1000;
      if (depth == 8)
      {
         assert(!open_owner(&color) && !reader_freed);
         continue;
      }
      assert(open_owner(&color));
      assert(!ops.decode(
            ops.decoder, packet, sizeof(packet), &sequence, &picture));
      queued_tag = 1;
      assert(!ops.output(ops.decoder, 1, &picture.grain.params, &frame));
      assert(frame.color.transfer == fault && frame.color.max_cll == 1000);
      mediacodec_av1_decoder_close(&ops);
      assert(!mediacodec_av1_decoder_retire_poll(retired));
      assert(android_image_queue_return(held.ticket, held.generation, -1));
      assert(mediacodec_av1_decoder_retire_poll(retired));
      reset();
      color.transfer = fault == 16 ? 18 : 16;
      assert(!open_owner(&color) && !reader_freed);
   }
}

int main(void)
{
   execute(8);
   execute(10);
   puts("PASS: AV1 configuration, color/grain snapshots, hidden/repeat "
        "timestamps, BUSY, EOS and leased retirement; no Android device "
        "execution");
   return 0;
}
