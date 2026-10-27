/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../../gfx/common/mediacodec_hevc_decoder.h"
#include "../../../gfx/common/android_image_ndk.h"
static int native, submitted, busy, eos, freed, reader_freed, released,
      startup_fail;
static int timestamp_extra, release_busy, release_calls, pump_status;
static int pump_calls;
static int64_t rendered_tag;
static int64_t queued_tag;
static android_image_view_t held;
static mediacodec_hevc_decoder_t *retired;
static mediacodec_output_format_t format;
static uint8_t vps[] = {64, 1, 128}, sps[] = {66, 1, 128}, pps[] = {68, 1, 128};
int rh265_video_get_parameter_sets(
      const rh265_video *v, unsigned id, rh265_nal_t sets[3])
{
   assert(v && !id);
   memset(sets, 0, 3 * sizeof(*sets));
   sets[0].data = vps;
   sets[1].data = sps;
   sets[2].data = pps;
   sets[0].size = sets[1].size = sets[2].size = 3;
   return 0;
}
int rh265_video_get_color(
      const rh265_video *v, unsigned id, rh265_video_color_t *color)
{
   assert(v && !id);
   memset(color, 0, sizeof(*color));
   return 0;
}
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
   view->width = view->right = 64;
   view->height = view->bottom = 64;
   view->timestamp_ns = queued_tag * 1000 + timestamp_extra;
   queued_tag = 0;
   return true;
}
static void delete_image(void *image)
{
   assert(image == &native);
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
   assert(w == 64 && h == 64);
   memset(&ops, 0, sizeof(ops));
   ops.acquire = acquire;
   ops.describe = describe;
   ops.delete_image = delete_image;
   ops.delete_reader = delete_reader;
   *window = &native;
   return android_image_queue_new(&native, slots, &ops);
}
mediacodec_session_t *mediacodec_session_new(
      JNIEnv *env, const mediacodec_config_t *c, void *window)
{
   assert(env && window == &native && c->profile == 1 && c->csd_size[0] == 21);
   assert(!memcmp(c->csd[0], "\0\0\0\1\100\1", 6));
   return startup_fail ? NULL : (mediacodec_session_t *)&native;
}
int mediacodec_session_submit(mediacodec_session_t *s, const void *data,
      size_t bytes, int64_t pts, bool end)
{
   assert(s == (mediacodec_session_t *)&native);
   if (busy)
   {
      busy--;
      return 0;
   }
   if (end)
   {
      assert(!data && !bytes);
      eos++;
      return 1;
   }
   assert(bytes == 7 && !memcmp(data, "\0\0\0\1\46\1\200", 7));
   submitted++;
   assert(pts > 0);
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
static int export_image(void *ctx, const android_image_view_t *image,
      const mediacodec_output_format_t *fmt, const rh265_video_color_t *color,
      video_hw_frame_t *out)
{
   assert(ctx == &native && fmt->serial && color);
   held = *image;
   out->image = 1;
   out->width = out->height = 64;
   return 0;
}
static bool release(void *ctx)
{
   assert(ctx == &native && freed && reader_freed);
   release_calls++;
   if (release_busy)
   {
      release_busy--;
      return false;
   }
   released++;
   return true;
}
static void enqueue(void *ctx, mediacodec_hevc_decoder_t *d)
{
   assert(ctx == &native && !retired);
   retired = d;
}
int main(void)
{
   static rh265_video_params_t params;
   static rh265_access_unit_t au;
   rh265_hw_ops_t ops;
   rh265_hw_picture_t picture;
   rh265_nal_t slice;
   mediacodec_hevc_import_t importer;
   video_hw_frame_t frame;
   uint8_t bytes[] = {38, 1, 128};
   memset(&importer, 0, sizeof(importer));
   importer.context = &native;
   importer.export_image = export_image;
   importer.enqueue = enqueue;
   importer.release = release;
   params.width = params.height = 64;
   params.bit_depth = 8;
   params.chroma_format_idc = 1;
   format.width = format.height = format.right = format.bottom = 64;
   memset(&picture, 0, sizeof(picture));
   picture.serial = 1;
   picture.output = 1;
   memset(&slice, 0, sizeof(slice));
   slice.data = bytes;
   slice.size = 3;
   au.num_slices = 1;
   au.coded_bytes = 3;
   au.first.nal_type = 19;
   startup_fail = 1;
   assert(!mediacodec_hevc_decoder_open((JNIEnv *)&native,
         (rh265_video *)&native, &params, 4, 1024, &importer, &ops));
   assert(!ops.decoder && !released && !freed && reader_freed == 1);
   startup_fail = 0;
   reader_freed = 0;
   assert(mediacodec_hevc_decoder_open((JNIEnv *)&native,
         (rh265_video *)&native, &params, 4, 1024, &importer, &ops));
   busy = 1;
   assert(ops.decode(ops.decoder, &params, &au, &slice, &picture) == 1
         && !submitted);
   assert(!ops.decode(ops.decoder, &params, &au, &slice, &picture)
         && submitted == 1);
   picture.serial = 2;
   picture.slot = 1;
   picture.keep_outputs = 1;
   assert(!ops.decode(ops.decoder, &params, &au, &slice, &picture)
         && submitted == 2);
   /* Surface delivery can arrive in display order rather than submission order.
    */
   queued_tag = 2;
   assert(ops.output(ops.decoder, 0, &frame) == 1);
   queued_tag = 1;
   assert(!ops.output(ops.decoder, 0, &frame));
   /* No-output transactions cancel prior display images. Their late Surface
    * deliveries must be returned, not consume reader capacity or be exported.
    */
   picture.serial = 3;
   picture.slot = 2;
   picture.keep_outputs = 0;
   picture.output = 0;
   assert(!ops.decode(ops.decoder, &params, &au, &slice, &picture));
   queued_tag = 3;
   assert(ops.output(ops.decoder, 1, &frame) == 1);
   queued_tag = 2;
   assert(ops.output(ops.decoder, 1, &frame) == 1);
   busy = 1;
   assert(ops.drain(ops.decoder) == 1 && !eos);
   assert(!ops.drain(ops.decoder) && eos == 1);
   assert(!ops.drain(ops.decoder) && eos == 1);
   assert(ops.decode(ops.decoder, &params, &au, &slice, &picture) == -1);
   mediacodec_hevc_decoder_close(&ops);
   assert(!ops.decoder && retired);
   assert(!mediacodec_hevc_decoder_retire_poll(retired) && !freed
         && !reader_freed);
   assert(android_image_queue_return(held.ticket, held.generation, -1));
   release_busy = 2;
   assert(!mediacodec_hevc_decoder_retire_poll(retired));
   assert(freed == 1 && reader_freed == 1 && !released && release_calls == 1);
   assert(!mediacodec_hevc_decoder_retire_poll(retired));
   assert(freed == 1 && reader_freed == 1 && !released && release_calls == 2);
   assert(mediacodec_hevc_decoder_retire_poll(retired));
   assert(release_calls == 3);
   assert(freed == 1 && reader_freed == 1 && released == 1);
   /* A malformed synthetic timestamp must fail without exporting or leaking. */
   retired = NULL;
   format.serial = 0;
   freed = reader_freed = released = 0;
   assert(mediacodec_hevc_decoder_open((JNIEnv *)&native,
         (rh265_video *)&native, &params, 4, 1024, &importer, &ops));
   picture.serial = 1;
   picture.slot = 0;
   picture.output = 1;
   assert(!ops.decode(ops.decoder, &params, &au, &slice, &picture));
   queued_tag = 1;
   timestamp_extra = 1;
   assert(ops.output(ops.decoder, 0, &frame) == -1);
   assert(ops.output(ops.decoder, 0, &frame) == -1);
   mediacodec_hevc_decoder_close(&ops);
   assert(mediacodec_hevc_decoder_retire_poll(retired));
   assert(freed == 1 && reader_freed == 1 && released == 1);
   /* EOS proves an expected codec output was never rendered. */
   retired = NULL;
   format.serial = 0;
   freed = reader_freed = released = 0;
   timestamp_extra = 0;
   eos = 0;
   assert(mediacodec_hevc_decoder_open((JNIEnv *)&native,
         (rh265_video *)&native, &params, 4, 1024, &importer, &ops));
   assert(!ops.decode(ops.decoder, &params, &au, &slice, &picture));
   assert(!ops.drain(ops.decoder) && eos == 1);
   pump_status = 3;
   assert(ops.output(ops.decoder, 0, &frame) == -1);
   assert(ops.output(ops.decoder, 0, &frame) == -1);
   mediacodec_hevc_decoder_close(&ops);
   assert(mediacodec_hevc_decoder_retire_poll(retired));
   /* A rendered Surface image may still arrive after codec EOS. */
   retired = NULL;
   format.serial = 0;
   freed = reader_freed = released = 0;
   pump_status = 0;
   eos = 0;
   assert(mediacodec_hevc_decoder_open((JNIEnv *)&native,
         (rh265_video *)&native, &params, 4, 1024, &importer, &ops));
   assert(!ops.decode(ops.decoder, &params, &au, &slice, &picture));
   rendered_tag = 1;
   assert(!ops.drain(ops.decoder) && eos == 1);
   pump_status = 3;
   assert(ops.output(ops.decoder, 0, &frame) == 1);
   {
      int calls = pump_calls;
      unsigned i;
      for (i = 0; i < 32; i++)
         assert(ops.output(ops.decoder, 0, &frame) == 1);
      assert(pump_calls == calls);
   }
   queued_tag = 1;
   assert(!ops.output(ops.decoder, 0, &frame));
   mediacodec_hevc_decoder_close(&ops);
   assert(!mediacodec_hevc_decoder_retire_poll(retired));
   assert(android_image_queue_return(held.ticket, held.generation, -1));
   assert(mediacodec_hevc_decoder_retire_poll(retired));
   puts("PASS: HEVC Surface owner startup faults, BUSY, reorder, missing EOS "
        "output, delayed Surface delivery and leased retirement");
   return 0;
}
