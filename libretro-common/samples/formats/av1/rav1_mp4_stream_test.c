/* SPDX-License-Identifier: MIT */
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <formats/rav1_sequence.h>
#include <formats/rmp4.h>
#include <formats/rwebm.h>
#include <formats/rmp4_video.h>
#include <formats/rav1_hw.h>
#include <formats/image_yuv_blit.h>

static unsigned failures;
#define CHECK(x)                                                               \
   do                                                                          \
   {                                                                           \
      if (!(x))                                                                \
      {                                                                        \
         fprintf(stderr, "line %u: %s\n", (unsigned)__LINE__, #x);             \
         failures++;                                                           \
      }                                                                        \
   } while (0)
#include "rav1_mp4_fixture.h"

static rav1_hw_backend_t factory;
static unsigned opens, closes, decodes, exports, abandons;
static int decode_busy, output_busy;

static void abandon(void *owner, uint64_t done, uint64_t value)
{
   CHECK(owner == &factory && done == 2 && value);
   abandons++;
}

static int decode(void *owner, const uint8_t *data, size_t size,
                  const rav1_sequence_t *seq, const rav1_hw_picture_t *picture)
{
   CHECK(owner == &factory && seq->max_width == 64);
   CHECK(data >= bytes + media_at && data + size <= bytes + used);
   CHECK(picture->tile_group.data >= data &&
         picture->tile_group.data + picture->tile_group.size <= data + size);
   decodes++;
   return decode_busy;
}

static int output(void *owner, unsigned slot, const rav1_grain_t *grain,
                  video_hw_frame_t *frame)
{
   CHECK(owner == &factory && slot < 9 && !grain->apply);
   if (output_busy)
      return 1;
   frame->image = 100 + ++exports;
   frame->width = frame->height = 64;
   frame->pixfmt = IMAGE_PIXFMT_NV12;
   frame->done = 2;
   frame->done_value = exports;
   frame->owner = &factory;
   frame->abandon = abandon;
   return 0;
}

static bool open_backend(void *user, const rav1_sequence_t *seq, unsigned slots,
                         rav1_hw_ops_t *ops)
{
   CHECK(user == &factory && seq->max_width == 64 && slots == 9);
   ops->decoder = &factory;
   ops->decode = decode;
   ops->output = output;
   opens++;
   return true;
}

static void close_backend(rav1_hw_ops_t *ops)
{
   CHECK(ops->decoder == &factory);
   closes++;
   memset(ops, 0, sizeof(*ops));
}

static void test_stream(void)
{
   rmp4_video_stream_t *s;
   video_hw_frame_t frame;
   unsigned i, w = 0, h = 0, before;
   int duration = 0, frames = 0, loops = 0, ret = 2;
   make_mp4(1);
   CHECK(!rmp4_video_stream_open(bytes, used));
   CHECK(!opens);
   s = rmp4_video_stream_open_hw_avail(
       bytes, used, media_at + sizeof(packet) - 1, NULL, NULL, NULL);
   CHECK(s != NULL);
   if (!s)
      return;
   rmp4_video_stream_get_info(s, &w, &h, &frames, &loops);
   CHECK(w == 64 && h == 64 && frames == 2);
   CHECK(rmp4_video_stream_set_hw(s, 1) && opens == 1);
   CHECK(!rmp4_video_stream_next(s, &duration));
   CHECK(rmp4_video_stream_wait_reason(s) == RMP4_VIDEO_WAIT_INPUT && !decodes);
   rmp4_video_stream_set_avail(s, used);
   decode_busy = 1;
   CHECK(!rmp4_video_stream_next(s, &duration));
   CHECK(rmp4_video_stream_wait_reason(s) == RMP4_VIDEO_WAIT_DECODER);
   CHECK(rmp4_video_stream_consumed(s) <= media_at && decodes == 1);
   decode_busy = 0;
   output_busy = 1;
   CHECK(!rmp4_video_stream_next(s, &duration));
   CHECK(decodes == 2 && !exports);
   output_busy = 0;
   CHECK(rmp4_video_stream_next(s, &duration) != NULL);
   CHECK(duration == 40 && rmp4_video_stream_hw_frame(s, &frame));
   CHECK(frame.image && exports == 1 && decodes == 2);
   CHECK(!rmp4_video_stream_hw_frame(s, &frame));
   for (i = 0; i < 8 && ret == 2; i++)
      ret = rmp4_video_stream_discard(s, &duration);
   CHECK(ret == 1 && duration == 40 && exports == 1 && decodes == 3);
   CHECK(!rmp4_video_stream_hw_frame(s, &frame));
   CHECK(rmp4_video_stream_skip(s, NULL) == -1);
   before = opens;
   rmp4_video_stream_rewind(s);
   CHECK(opens == before + 1 && closes == before);
   decode_busy = 1;
   CHECK(!rmp4_video_stream_next(s, NULL));
   before = opens;
   rmp4_video_stream_rewind(s);
   CHECK(opens == before + 1 && closes == before);
   decode_busy = 0;
   rav1_set_hw_backend(NULL);
   rmp4_video_stream_close(s);
   video_hw_frame_abandon(&frame);
   CHECK(opens == closes && abandons == 1);
   rav1_set_hw_backend(&factory);
}

static void test_color(void)
{
   rmp4_video_stream_t *s;
   video_hw_frame_t frame;
   unsigned i, before = opens;
   const uint32_t *marker = NULL;
   make_mp4(1);
   bytes[color_at + 3] = 16;
   s = rmp4_video_stream_open_hw_avail(bytes, used, used, NULL, NULL, NULL);
   CHECK(s != NULL);
   if (s)
   {
      CHECK(!rmp4_video_stream_set_hw(s, 1));
      rmp4_video_stream_close(s);
   }
   make_mp4(1);
   bytes[color_at + 6] = 128;
   s = rmp4_video_stream_open_hw_avail(bytes, used, used, NULL, NULL, NULL);
   CHECK(s != NULL);
   if (s)
   {
      CHECK(!rmp4_video_stream_set_hw(s, 1));
      rmp4_video_stream_close(s);
   }
   CHECK(opens == before);
   make_mp4(1);
   bytes[color_at + 5] = 1;
   s = rmp4_video_stream_open_hw_avail(bytes, used, used, NULL, NULL, NULL);
   CHECK(s != NULL);
   if (!s)
      return;
   CHECK(rmp4_video_stream_set_hw(s, 1));
   for (i = 0; i < 8 && !marker; i++)
      marker = rmp4_video_stream_next(s, NULL);
   CHECK(marker && rmp4_video_stream_hw_frame(s, &frame));
   if (marker)
   {
      CHECK(frame.yuv & IMAGE_YUV_FLAG_BT709);
      CHECK(frame.color.max_cll == 1024 && frame.color.max_fall == 256
            && frame.color.mastering.present == VIDEO_MASTERING_DISPLAY_COMPLETE
            && frame.color.mastering.chromaticity[0] == 25000
            && frame.color.mastering.luminance_max == 10240000);
      CHECK(frame.color.primaries == 1 && frame.color.matrix == 1
            && frame.color.transfer == 2 && frame.color.range == 1);
      video_hw_frame_abandon(&frame);
   }
   rmp4_video_stream_close(s);
}

static void test_unavailable(void)
{
   rmp4_video_stream_t *s;
   unsigned before = opens;
   make_mp4(1);
   rav1_set_hw_backend(NULL);
   s = rmp4_video_stream_open_hw_avail(bytes, used, used, NULL, NULL, NULL);
   CHECK(s != NULL);
   if (s)
   {
      CHECK(!rmp4_video_stream_set_hw(s, 1));
      CHECK(!rmp4_video_stream_next(s, NULL));
      rmp4_video_stream_close(s);
   }
   rav1_set_hw_backend(&factory);
   bytes[config_at] = 0;
   s = rmp4_video_stream_open_hw_avail(bytes, used, used, NULL, NULL, NULL);
   if (s)
   {
      CHECK(!rmp4_video_stream_set_hw(s, 1));
      rmp4_video_stream_close(s);
   }
   CHECK(opens == before);
}

int main(void)
{
   factory.user = &factory;
   factory.open = open_backend;
   factory.close = close_backend;
   rav1_set_hw_backend(&factory);
   test_stream();
   test_color();
   test_unavailable();
   rav1_set_hw_backend(NULL);
   printf("AV1 MP4 hardware reader: %u failures\n", failures);
   return failures ? 1 : 0;
}
