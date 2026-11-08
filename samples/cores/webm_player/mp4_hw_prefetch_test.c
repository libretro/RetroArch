/* SPDX-License-Identifier: MIT */
#ifndef HAVE_RMP4
#include <stdio.h>
int main(void)
{
   puts("MP4 hardware prefetch: SKIP (MP4 disabled)");
   return 0;
}
#else
#define main vp9_webm_reference_main
#include "webm_vp9_hw_test.c"
#undef main

typedef struct prefetch_mock
{
   unsigned refs, width, height, serial;
} prefetch_mock_t;
static unsigned prefetch_live, prefetch_exports, prefetch_returns;
static unsigned prefetch_decoded, prefetch_taken, prefetch_cpu, prefetch_gpu;
static unsigned prefetch_block, prefetch_ready = 1;

static void prefetch_release(prefetch_mock_t *m)
{
   if (!--m->refs)
   {
      prefetch_live--;
      free(m);
   }
}
static void prefetch_abandon(void *owner, uint64_t done, uint64_t value)
{
   (void)done;
   (void)value;
   prefetch_returns++;
   prefetch_release((prefetch_mock_t *)owner);
}
static int prefetch_decode(void *owner, const uint8_t *packet, size_t size,
      const rvp9_hw_picture_t *picture)
{
   prefetch_mock_t *m = (prefetch_mock_t *)owner;
   CHECK(packet && size && picture->slot >= 0);
   m->serial = ++prefetch_decoded;
   return 0;
}
static int prefetch_output(void *owner, unsigned slot, video_hw_frame_t *frame)
{
   prefetch_mock_t *m = (prefetch_mock_t *)owner;
   CHECK(slot < 10);
   if (prefetch_block)
      return 1;
   m->refs++;
   memset(frame, 0, sizeof(*frame));
   frame->image = m->serial;
   frame->width = m->width;
   frame->height = m->height;
   frame->pixfmt = IMAGE_PIXFMT_NV12;
   frame->api = VIDEO_HW_API_VULKAN;
   frame->owner = m;
   frame->abandon = prefetch_abandon;
   prefetch_exports++;
   return 0;
}
static bool prefetch_open(void *user, const rvp9_hdr *header,
      unsigned slots, rvp9_hw_ops_t *ops)
{
   prefetch_mock_t *m;
   (void)user;
   CHECK(slots == 10 && header->bit_depth == 8);
   m = (prefetch_mock_t *)calloc(1, sizeof(*m));
   if (!m)
      return false;
   m->refs = 1;
   m->width = (unsigned)header->w;
   m->height = (unsigned)header->h;
   prefetch_live++;
   ops->decoder = m;
   ops->decode = prefetch_decode;
   ops->output = prefetch_output;
   return true;
}
static void prefetch_close(rvp9_hw_ops_t *ops)
{
   prefetch_release((prefetch_mock_t *)ops->decoder);
}
static const rvp9_hw_backend_t prefetch_backend = {
      NULL, prefetch_open, prefetch_close, false, NULL, NULL, NULL, NULL};

static bool prefetch_sink(void *user, const video_hw_frame_t *frame)
{
   (void)user;
   if (!frame)
      return prefetch_ready != 0;
   CHECK(frame->image && frame->pixfmt == IMAGE_PIXFMT_NV12);
   prefetch_taken++;
   video_hw_frame_abandon(frame);
   return true;
}
static void prefetch_video(const void *data, unsigned w, unsigned h, size_t pitch)
{
   CHECK(w == 64 && h == 48);
   if (data)
   {
      CHECK(data == media_player.fb && pitch == 64 * 4);
      prefetch_cpu++;
   }
   else
   {
      CHECK(!pitch);
      prefetch_gpu++;
   }
}
static bool prefetch_load(void)
{
   struct retro_game_info game;
   CHECK(!prefetch_live && prefetch_exports == prefetch_returns);
   prefetch_decoded = prefetch_taken = prefetch_cpu = prefetch_gpu = prefetch_block = 0;
   memset(&game, 0, sizeof(game));
   game.path = "fixtures/vp9-prefetch.mp4";
   CHECK(media_set_video_hw_refresh(prefetch_sink, NULL));
   CHECK(retro_load_game(&game));
   CHECK(media_player.mp4vs && !media_player.hw_started);
   CHECK(!media_set_video_hw_refresh(prefetch_sink, NULL));
   return media_player.mp4vs != NULL;
}
static void prefetch_unload(void)
{
   retro_unload_game();
   CHECK(!prefetch_live && prefetch_exports == prefetch_returns);
}

static void prefetch_ahead(void)
{
   unsigned i;
   uint8_t before[40], after[40];
   prefetch_ready = 1;
   if (!prefetch_load()) return;
   media_video_hw_ready();
   for (i = 0; i < 4 && !media_player.vshown; i++) retro_run();
   CHECK(media_player.vshown == 1 && prefetch_taken == 1);
   CHECK(media_player.mp4_prefetched && media_player.hw_frame.image);
   CHECK(prefetch_decoded == 2 && prefetch_exports == 2);
   CHECK(retro_serialize(before, sizeof(before)));
   media_prefetch_mp4(&media_player, media_player.play_ns + media_player.frame_ns / 2);
   CHECK(retro_serialize(after, sizeof(after)));
   CHECK(!memcmp(before, after, sizeof(before)));
   prefetch_unload();
}

static void prefetch_foreground_retry(void)
{
   uint8_t before[40], after[40];
   int duration = 0;
   int64_t target;
   prefetch_ready = 1;
   if (!prefetch_load()) return;
   media_video_hw_ready();
   CHECK(rmp4_video_stream_skip(media_player.mp4vs, &duration) == 1);
   CHECK(rmp4_video_stream_hw_frame(media_player.mp4vs, &media_player.hw_frame));
   media_player.vshown = 1;
   media_player.play_ns = media_player.vpts_ns = (int64_t)duration * 1000000;
   media_present_mp4(&media_player, 0);
   prefetch_block = 1;
   CHECK(rmp4_video_stream_skip(media_player.mp4vs, NULL) == 2);
   CHECK(rmp4_video_stream_wait_reason(media_player.mp4vs) == RMP4_VIDEO_WAIT_DECODER);
   target = rmp4_video_stream_next_end_time_ns(media_player.mp4vs) + 1;
   CHECK(retro_serialize(before, sizeof(before)));
   media_prefetch_mp4(&media_player, target);
   CHECK(!media_player.eof && media_player.mp4_prefetching);
   CHECK(retro_serialize(after, sizeof(after)) && !memcmp(before, after, sizeof(before)));
   prefetch_block = 0;
   media_prefetch_mp4(&media_player, target);
   CHECK(!media_player.eof && media_player.mp4_prefetched);
   CHECK(media_player.mp4_prefetch_count == 2 && media_player.hw_frame.image == 3);
   CHECK(retro_serialize(after, sizeof(after)) && !memcmp(before, after, sizeof(before)));
   prefetch_unload();
}

static void prefetch_replacement(void)
{
   unsigned cpu_before, taken_before;
   int64_t shown;
   prefetch_ready = 0;
   if (!prefetch_load()) return;
   retro_run();
   CHECK(!media_player.hw_video && media_player.fb && prefetch_cpu == 1);
   shown = media_player.vshown;
   media_suspend_video_hw();
   prefetch_ready = 1;
   prefetch_block = 1;
   cpu_before = prefetch_cpu;
   taken_before = prefetch_taken;
   retro_run();
   CHECK(media_player.hw_video && !media_player.eof);
   CHECK(prefetch_cpu == cpu_before + 1 && prefetch_taken == taken_before);
   CHECK(media_player.vshown == shown);
   prefetch_block = 0;
   retro_run();
   CHECK(prefetch_taken > taken_before && !media_player.eof);
   cpu_before = prefetch_cpu;
   prefetch_block = 1;
   retro_run();
   CHECK(prefetch_cpu == cpu_before && !media_player.eof);
   prefetch_unload();
}

static void prefetch_replacement_prime(void)
{
   uint8_t before[40], after[40];
   prefetch_ready = 0;
   if (!prefetch_load()) return;
   retro_run();
   CHECK(!media_player.hw_video && media_player.vshown == 1);
   CHECK(retro_serialize(before, sizeof(before)));
   media_suspend_video_hw();
   prefetch_ready = 1;
   prefetch_block = 1;
   media_video_hw_ready();
   CHECK(media_player.hw_video && !media_player.hw_restart);
   CHECK(media_player.mp4_prefetching && prefetch_decoded == 2);
   CHECK(retro_serialize(after, sizeof(after)));
   CHECK(!memcmp(before, after, sizeof(before)));
   prefetch_unload();
}

int main(void)
{
   retro_set_environment(env);
   retro_set_video_refresh(prefetch_video);
   retro_set_input_poll(poll);
   retro_set_input_state(input);
   retro_set_audio_sample_batch(audio);
   retro_init();
   rvp9_set_hw_backend(&prefetch_backend);
   prefetch_ahead();
   prefetch_foreground_retry();
   prefetch_replacement();
   prefetch_replacement_prime();
   rvp9_set_hw_backend(NULL);
   retro_deinit();
   puts("MP4 asynchronous prefetch, packet retry and CPU-front replacement checks finished");
   return failures ? 1 : 0;
}
#endif
