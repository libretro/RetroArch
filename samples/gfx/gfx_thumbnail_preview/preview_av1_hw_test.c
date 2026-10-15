/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <formats/image.h>
#include <formats/video_player.h>
#include <formats/rav1_hw.h>
#include <features/features_cpu.h>
#include "../../../gfx/gfx_anim_preview.h"
#include "../../../libretro-common/samples/formats/av1/rav1_webm_fixture.h"

static const char *fixture_path;
static unsigned failures, opens, closes, exports, abandons, shown;
static int busy, threaded;
static gfx_anim_preview_t *preview;
static video_hw_frame_t held;
static uint64_t offered;
static rav1_hw_backend_t factory;
#define CHECK(x)                                                               \
   do                                                                          \
   {                                                                           \
      if (!(x))                                                                \
      {                                                                        \
         fprintf(stderr, "line %u: %s\n", (unsigned)__LINE__, #x);             \
         failures++;                                                           \
      }                                                                        \
   } while (0)

static void abandon(void *owner, uint64_t done, uint64_t value)
{
   CHECK(owner == &factory && done == 2 && value);
   abandons++;
}

static int decode(void *owner, const uint8_t *data, size_t size,
                  const rav1_sequence_t *seq, const rav1_hw_picture_t *picture)
{
   CHECK(owner == &factory && seq->max_width == 64);
   CHECK(data >= preview->base && data + size <= preview->base + preview->len);
   CHECK(picture->tile_group.data >= data);
   return 0;
}

static int output(void *owner, unsigned slot, const rav1_grain_t *grain,
                  video_hw_frame_t *frame)
{
   CHECK(owner == &factory && slot < 9 && !grain->apply);
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
}

static int start(void *user, unsigned w, unsigned h, unsigned slots)
{
   (void)user;
   (void)w;
   (void)h;
   (void)slots;
   CHECK(0);
   return 0;
}

static int start_hw(void *user, unsigned w, unsigned h, unsigned jobs)
{
   (void)user;
   CHECK(w == 64 && h == 64 && (jobs == 1 || jobs == 2));
   threaded = jobs == 2;
   return 0;
}

static bool feed(void *user)
{
   (void)user;
   return gfx_anim_preview_feed(preview);
}

static enum video_player_present present(void *user,
                                         const video_player_frame_t *frame)
{
   (void)user;
   CHECK(frame->hw && !frame->pixels);
   if (!frame->hw)
      return VIDEO_PLAYER_PRESENT_FAILED;
   if (busy)
   {
      offered = frame->hw->image;
      busy = 0;
      return VIDEO_PLAYER_PRESENT_BUSY;
   }
   /* Inline playback abandons BUSY exports; worker playback retries them. */
   CHECK(!offered ||
         (threaded ? frame->hw->image == offered : frame->hw->image > offered));
   offered = 0;
   video_hw_frame_abandon(&held);
   held = *frame->hw;
   shown++;
   return VIDEO_PLAYER_PRESENT_DONE;
}

static void playback(unsigned flags, int loops)
{
   video_player_sink_t sink;
   video_player_t *player;
   int state = VIDEO_PLAYER_PLAYING;
   int64_t deadline;
   shown = 0;
   busy = 1;
   offered = 0;
   preview = gfx_anim_preview_open_hw(fixture_path, 0);
   CHECK(preview != NULL);
   if (!preview)
      return;
   memset(&sink, 0, sizeof(sink));
   sink.start = start;
   sink.start_hw = start_hw;
   sink.present = present;
   sink.feed = feed;
   player =
       video_player_new(preview->stream, preview->type, loops, &sink, flags);
   CHECK(player != NULL);
   deadline = cpu_features_get_time_usec() + 5000000;
   while (state != VIDEO_PLAYER_ENDED &&
          cpu_features_get_time_usec() < deadline)
      state = video_player_poll(player, cpu_features_get_time_usec());
   CHECK(state == VIDEO_PLAYER_ENDED &&
         shown == (unsigned)(2 * loops - (threaded ? 0 : 1)));
   video_player_free(player);
   video_player_worker_deinit();
   gfx_anim_preview_close(preview);
   preview = NULL;
   CHECK(held.image != 0 && opens == closes);
   video_hw_frame_abandon(&held);
   memset(&held, 0, sizeof(held));
}

static void test_container(unsigned container)
{
   FILE *file;
   void *stream;
   int more = 0;
   unsigned before = opens;
   enum image_type_enum type = container ? IMAGE_TYPE_WEBM : IMAGE_TYPE_MP4;
   fixture_path =
       container ? "av1-preview-fixture.webm" : "av1-preview-fixture.mp4";
   if (container)
      make_webm(container == 2, 0, 0, 0);
   else
      make_mp4(1);
   file = fopen(fixture_path, "wb");
   if (!file)
   {
      CHECK(0);
      return;
   }
   CHECK(fwrite(bytes, 1, used, file) == used);
   fclose(file);
   CHECK(!image_transfer_anim_stream_new_avail(bytes, used, used, type, &more,
                                               NULL, NULL) &&
         !more);
   CHECK(!gfx_anim_preview_open_hw(fixture_path, 0));
   factory.user = &factory;
   factory.open = open_backend;
   factory.close = close_backend;
   rav1_set_hw_backend(&factory);
   CHECK(!gfx_anim_preview_open(fixture_path, 0) && opens == before);
   stream = image_transfer_anim_stream_new_hw_avail(bytes, used, 8, type, &more,
                                                    NULL, NULL);
   CHECK(!stream && more && opens == before);
   image_transfer_anim_stream_free(stream, type);
   playback(VIDEO_PLAYER_INLINE, 1);
   playback(0, 1);
   playback(VIDEO_PLAYER_INLINE, 2);
   playback(0, 2);
   rav1_set_hw_backend(NULL);
   CHECK(exports == abandons && opens == closes);
   remove(fixture_path);
}

int main(void)
{
   test_container(0);
   test_container(1);
   test_container(2);
   printf("AV1 preview hardware: %u failures\n", failures);
   return failures ? 1 : 0;
}
