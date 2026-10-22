/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <features/features_cpu.h>
#include <rthreads/rthreads.h>
#include <formats/rmp4_video.h>
#include <formats/rwebm_video.h>
#include "pipeline_fixture.h"

extern void *videotoolbox_hevc_test_device(void *);
extern void videotoolbox_hevc_test_device_release(void *);
extern int videotoolbox_hevc_test_start(void *, void *, int);
extern void videotoolbox_hevc_test_stop(void *);
extern void videotoolbox_hevc_test_retired(void);
extern void videotoolbox_hevc_test_export(video_hw_frame_t *, unsigned,
      unsigned, const uint32_t [3]);
extern void videotoolbox_hevc_test_movie_reset(void);
extern void videotoolbox_vp9_test_mp4_state(rmp4_video_stream_t *, unsigned [4]);
extern unsigned videotoolbox_vp9_test_session_opens(void);

static int playback_step(rmp4_video_stream_t *mp4,
      rwebm_video_stream_t *webm, int *duration)
{
   if (!webm)
      return rmp4_video_stream_skip(mp4, duration);
   if (rwebm_video_stream_next(webm, duration))
      return 1;
   return rwebm_video_stream_waiting(webm) ? 2 : -1;
}

static void run(const vp9_pipeline_fixture_t *fixture, void *device)
{
   rmp4_video_stream_t *mp4 = NULL;
   rwebm_video_stream_t *webm = NULL;
   video_hw_frame_t frame, held;
   int owner, ret, duration;
   unsigned count = 0, seek;
   unsigned opens = videotoolbox_vp9_test_session_opens();
   int64_t deadline, position;
   printf("VP9 pipeline case: %s depth %u range %u transfer %u hidden/repeat %u\n",
         fixture->webm ? "WebM" : "MP4", fixture->depth,
         fixture->full, fixture->transfer, fixture->hidden_repeat);
   fflush(stdout);
   assert(videotoolbox_hevc_test_start(&owner, device, 1));
   if (fixture->webm)
   {
      webm = rwebm_video_stream_open(fixture->data, fixture->size);
      assert(webm && rwebm_video_stream_set_hw(webm, 1));
   }
   else
   {
      mp4 = rmp4_video_stream_open(fixture->data, fixture->size);
      assert(mp4 && rmp4_video_stream_set_hw(mp4, 1));
   }
   memset(&held, 0, sizeof(held));
   deadline = cpu_features_get_time_usec() + 10000000;
   for (;;)
   {
      ret = playback_step(mp4, webm, &duration);
      assert(cpu_features_get_time_usec() < deadline);
      if (ret == 2)
      {
         sthread_yield();
         continue;
      }
      if (ret < 0)
         break;
      assert(ret == 1 && count < 4 && duration == 250);
      ret = fixture->webm ? rwebm_video_stream_hw_frame(webm, &frame)
         : rmp4_video_stream_hw_frame(mp4, &frame);
      assert(ret == 1 && frame.api == VIDEO_HW_API_METAL
            && frame.width == 64 && frame.height == 64
            && frame.color.transfer == fixture->transfer
            && frame.color.range == (fixture->full ? 2u : 1u));
      if (!count)
         held = frame;
      else
         videotoolbox_hevc_test_export(&frame, fixture->depth,
               fixture->full, fixture->crc[count]);
      count++;
   }
   if (count != 4 || !held.owner)
   {
      fprintf(stderr, "VP9 pipeline stopped: result %d, visible frames %u/4, retained owner %u\n",
            ret, count, held.owner != NULL);
      fflush(stderr);
   }
   assert(count == 4 && held.owner);
   for (seek = 0; seek < 4; seek++)
   {
      unsigned target = seek & 1 ? 3 : 0;
      ret = fixture->webm
         ? rwebm_video_stream_seek_begin_ns(webm, (int64_t)target * 250000000)
         : rmp4_video_stream_seek_begin_ns(mp4, (int64_t)target * 250000000);
      assert(ret);
      deadline = cpu_features_get_time_usec() + 10000000;
      do
      {
         ret = fixture->webm ? rwebm_video_stream_seek_step(webm, &position)
            : rmp4_video_stream_seek_step(mp4, &position);
         assert(ret >= 0 && cpu_features_get_time_usec() < deadline);
         if (!ret)
            sthread_yield();
      } while (!ret);
      assert(position == (int64_t)target * 250000);
      do
      {
         ret = playback_step(mp4, webm, &duration);
         assert(ret >= 0 && cpu_features_get_time_usec() < deadline);
         if (ret == 2)
            sthread_yield();
      } while (ret == 2);
      assert(ret == 1);
      ret = fixture->webm ? rwebm_video_stream_hw_frame(webm, &frame)
         : rmp4_video_stream_hw_frame(mp4, &frame);
      if (ret != 1 || frame.color.transfer != fixture->transfer)
      {
         unsigned state[4] = {0, 0, 0, 0};
         if (mp4) videotoolbox_vp9_test_mp4_state(mp4, state);
         fprintf(stderr, "VP9 seek %u target %u: hardware frame %d, transfer %u expected %u\n",
               seek, target, ret, ret == 1 ? frame.color.transfer : 0,
               fixture->transfer);
         fprintf(stderr, "VP9 test-only MP4 state: hardware %u software %u renderer %u untaken %u\n",
               state[0], state[1], state[2], state[3]);
         fflush(stderr);
      }
      assert(ret == 1 && frame.color.transfer == fixture->transfer);
      videotoolbox_hevc_test_export(&frame, fixture->depth,
            fixture->full, fixture->crc[target]);
   }
   assert(videotoolbox_vp9_test_session_opens() == opens + 1);
   if (webm)
      rwebm_video_stream_close(webm);
   if (mp4)
      rmp4_video_stream_close(mp4);
   videotoolbox_hevc_test_stop(&owner);
   videotoolbox_hevc_test_retired();
   videotoolbox_hevc_test_export(&held, fixture->depth,
         fixture->full, fixture->crc[0]);
   videotoolbox_hevc_test_movie_reset();
   printf("PASS: native VP9 %s %u-bit range %u transfer %u: hidden/repeat %u: Metal decoded-plane CRC, FrameView pixels, repeated seeks and retained export after stream/pool close\n",
         fixture->webm ? "WebM" : "MP4", fixture->depth,
         fixture->full, fixture->transfer, fixture->hidden_repeat);
}

int main(void)
{
#if defined(__APPLE__) && defined(HAVE_THREADS)
   void *device = videotoolbox_hevc_test_device(NULL);
   unsigned i;
   if (!device)
   {
      puts("UNAVAILABLE: VP9 Metal pipeline requires a native Metal device");
      return 77;
   }
   for (i = 0; i < sizeof(pipeline_fixtures) / sizeof(pipeline_fixtures[0]); i++)
      run(&pipeline_fixtures[i], device);
   videotoolbox_hevc_test_device_release(device);
   return 0;
#else
   puts("UNAVAILABLE: native VP9 Metal pipeline requires Apple threads");
   return 77;
#endif
}
