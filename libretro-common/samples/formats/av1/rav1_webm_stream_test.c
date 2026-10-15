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
#include "rav1_webm_fixture.h"
#include <formats/rwebm_video.h>

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

static void test_stream(int inband)
{
   rwebm_video_stream_t *s;
   video_hw_frame_t frame = {0};
   unsigned before = opens, w = 0, h = 0;
   int duration = 0, frames = 0, loops = 0;
   make_webm(inband, 0, 0, 0);
   CHECK(!rwebm_video_stream_open(bytes, used));
   s = rwebm_video_stream_open_hw_avail(bytes, used, 8, &frames);
   CHECK(!s && frames);
   s = rwebm_video_stream_open_hw_avail(bytes, used, used, NULL);
   CHECK(s != NULL);
   if (!s)
      return;
   rwebm_video_stream_get_info(s, &w, &h, &frames, &loops);
   CHECK(w == 64 && h == 64 && frames == 2);
   CHECK(rwebm_video_stream_set_hw(s, 1) && opens == before + 1);
   rwebm_video_stream_set_avail(s, media_at);
   CHECK(!rwebm_video_stream_next(s, NULL) && rwebm_video_stream_waiting(s));
   rwebm_video_stream_set_avail(s, used);
   decode_busy = 1;
   CHECK(!rwebm_video_stream_next(s, NULL) && rwebm_video_stream_waiting(s));
   CHECK(rwebm_video_stream_consumed(s) <= media_at);
   rwebm_video_stream_set_avail(s, media_at);
   decode_busy = 0;
   CHECK(!rwebm_video_stream_next(s, NULL) && rwebm_video_stream_waiting(s));
   rwebm_video_stream_set_avail(s, used);
   output_busy = 1;
   CHECK(!rwebm_video_stream_next(s, NULL) && rwebm_video_stream_waiting(s));
   output_busy = 0;
   CHECK(rwebm_video_stream_next(s, &duration) != NULL);
   CHECK(duration == 40 && rwebm_video_stream_hw_frame(s, &frame));
   CHECK(!rwebm_video_stream_hw_frame(s, &frame));
   before = exports;
   CHECK(rwebm_video_stream_skip(s, &duration) == 1 && duration == 40);
   CHECK(exports == before && !rwebm_video_stream_hw_frame(s, &frame));
   CHECK(!rwebm_video_stream_next(s, NULL) && !rwebm_video_stream_waiting(s));
   rwebm_video_stream_rewind(s);
   decode_busy = 1;
   CHECK(!rwebm_video_stream_next(s, NULL));
   rwebm_video_stream_rewind(s);
   decode_busy = 0;
   CHECK(rwebm_video_stream_next(s, NULL) != NULL);
   before = abandons;
   rwebm_video_stream_close(s);
   CHECK(abandons == before + 1);
   video_hw_frame_abandon(&frame);
   CHECK(opens == closes);
}

static void test_config(int inband, unsigned matrix, unsigned transfer,
                        unsigned range, int allowed)
{
   rwebm_video_stream_t *s;
   video_hw_frame_t frame = {0};
   make_webm(inband, matrix, transfer, range);
   s = rwebm_video_stream_open_hw_avail(bytes, used, used, NULL);
   CHECK(s != NULL);
   if (!s)
      return;
   CHECK(rwebm_video_stream_set_hw(s, 1) == allowed);
   if (allowed)
   {
      CHECK(rwebm_video_stream_next(s, NULL) != NULL);
      CHECK(rwebm_video_stream_hw_frame(s, &frame));
      CHECK((frame.yuv & IMAGE_YUV_FLAG_BT709) != 0);
      video_hw_frame_abandon(&frame);
   }
   rwebm_video_stream_close(s);
}

int main(void)
{
   rwebm_video_stream_t *s;
   factory.user = &factory;
   factory.open = open_backend;
   factory.close = close_backend;
   rav1_set_hw_backend(&factory);
   test_stream(0);
   test_stream(1);
   test_config(0, 1, 0, 1, 1);
   test_config(1, 0, 16, 0, 0);
   test_config(0, 0, 18, 0, 0);
   test_config(1, 9, 0, 0, 0);
   test_config(0, 0, 0, 2, 0);
   test_config(1, 0, 0, 3, 0);
   make_webm(1, 0, 0, 0);
   bytes[config_at + 2] ^= 0x40;
   s = rwebm_video_stream_open_hw_avail(bytes, used, used, NULL);
   CHECK(s && !rwebm_video_stream_set_hw(s, 1));
   rwebm_video_stream_close(s);
   rav1_set_hw_backend(NULL);
   make_webm(0, 0, 0, 0);
   s = rwebm_video_stream_open_hw_avail(bytes, used, used, NULL);
   CHECK(s && !rwebm_video_stream_set_hw(s, 1));
   CHECK(!rwebm_video_stream_next(s, NULL));
   rwebm_video_stream_close(s);
   CHECK(opens == closes && exports == abandons);
   printf("AV1 WebM hardware reader: %u failures\n", failures);
   return failures ? 1 : 0;
}
