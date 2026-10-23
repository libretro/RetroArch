/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifdef NDEBUG
#error Native VideoToolbox acceptance requires assertions
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <formats/rmp4_video.h>
#include <formats/rwebm_video.h>
#include <features/features_cpu.h>
#include <rthreads/rthreads.h>

static uint8_t *load(const char *path, size_t *size)
{
   FILE *file = fopen(path, "rb");
   long length;
   uint8_t *data;
   assert(file && !fseek(file, 0, SEEK_END));
   length = ftell(file);
   assert(length > 0 && length <= 16 * 1024 * 1024);
   assert(!fseek(file, 0, SEEK_SET));
   *size = (size_t)length;
   data = (uint8_t *)malloc(*size);
   assert(data && fread(data, 1, *size, file) == *size && !fclose(file));
   return data;
}

static uint32_t hashes[4];
static void references(const uint8_t *raw, size_t size, unsigned depth)
{
   unsigned bytes = depth == 10 ? 2 : 1, frame;
   size_t frame_size = 128u * 96u * 3u / 2u * bytes;
   assert(size == frame_size * 4);
   for (frame = 0; frame < 4; frame++)
   {
      size_t i;
      uint32_t hash = 2166136261u;
      for (i = frame * frame_size; i < (frame + 1) * frame_size; i += bytes)
      {
         unsigned value = raw[i];
         if (bytes == 2)
            value |= (unsigned)raw[i + 1] << 8;
         assert(value < (1u << depth));
         hash = (hash ^ (value & 255)) * 16777619u;
         if (bytes == 2)
            hash = (hash ^ (value >> 8)) * 16777619u;
      }
      hashes[frame] = hash;
   }
}

#if defined(__APPLE__) && defined(HAVE_THREADS)
extern void *videotoolbox_hevc_test_device(void *);
extern void videotoolbox_hevc_test_device_release(void *);
extern int videotoolbox_hevc_test_start(void *, void *, int);
extern void videotoolbox_hevc_test_stop(void *);
extern void videotoolbox_hevc_test_retired(void);
extern void videotoolbox_hevc_test_movie_reset(void);
extern void videotoolbox_av1_test_export(
      video_hw_frame_t *, unsigned, unsigned, uint32_t);
extern unsigned videotoolbox_av1_test_session_opens(void);

static int step(
      rmp4_video_stream_t *mp4, rwebm_video_stream_t *webm, int *duration)
{
   if (mp4)
      return rmp4_video_stream_skip(mp4, duration);
   if (rwebm_video_stream_next(webm, duration))
      return 1;
   return rwebm_video_stream_waiting(webm) ? 2 : -1;
}

static void take(rmp4_video_stream_t *mp4, rwebm_video_stream_t *webm,
      video_hw_frame_t *frame)
{
   int result = mp4 ? rmp4_video_stream_hw_frame(mp4, frame)
                    : rwebm_video_stream_hw_frame(webm, frame);
   assert(result == 1 && frame->api == VIDEO_HW_API_METAL && frame->width == 128
         && frame->height == 96);
}

static int execute(const uint8_t *data, size_t size, unsigned webm,
      unsigned depth, unsigned position)
{
   void *device = videotoolbox_hevc_test_device(NULL);
   unsigned cycle;
   int renderer;
   if (!device)
   {
      puts("UNAVAILABLE: AV1 container playback requires a Metal device");
      return 77;
   }
   for (cycle = 0; cycle < 2; cycle++)
   {
      rmp4_video_stream_t *mp4 = NULL;
      rwebm_video_stream_t *stream = NULL;
      video_hw_frame_t frame, held;
      unsigned count = 0, seek;
      unsigned opens = videotoolbox_av1_test_session_opens();
      int result, duration;
      int64_t deadline, reached;
      assert(videotoolbox_hevc_test_start(&renderer, device, 1));
      if (webm)
      {
         stream = rwebm_video_stream_open(data, size);
         assert(stream && rwebm_video_stream_set_hw(stream, 1));
      }
      else
      {
         mp4 = rmp4_video_stream_open(data, size);
         assert(mp4 && rmp4_video_stream_set_hw(mp4, 1));
      }
      memset(&held, 0, sizeof(held));
      deadline = cpu_features_get_time_usec() + 10000000;
      for (;;)
      {
         result = step(mp4, stream, &duration);
         assert(cpu_features_get_time_usec() < deadline);
         if (result == 2)
         {
            sthread_yield();
            continue;
         }
         if (result < 0)
            break;
         assert(result == 1 && count < 4 && duration == 250);
         take(mp4, stream, &frame);
         if (!count)
            held = frame;
         else
            videotoolbox_av1_test_export(
                  &frame, depth, position, hashes[count]);
         count++;
      }
      assert(count == 4 && held.owner);
      for (seek = 0; seek < 8; seek++)
      {
         unsigned target = seek & 1 ? 3 : 0;
         int64_t ns = (int64_t)target * 250000000;
         assert(mp4 ? rmp4_video_stream_seek_begin_ns(mp4, ns)
                    : rwebm_video_stream_seek_begin_ns(stream, ns));
         deadline = cpu_features_get_time_usec() + 10000000;
         do
         {
            result = mp4 ? rmp4_video_stream_seek_step(mp4, &reached)
                         : rwebm_video_stream_seek_step(stream, &reached);
            assert(result >= 0 && cpu_features_get_time_usec() < deadline);
            if (!result)
               sthread_yield();
         } while (!result);
         assert(reached == (int64_t)target * 250000);
         do
         {
            result = step(mp4, stream, &duration);
            assert(result >= 0 && cpu_features_get_time_usec() < deadline);
            if (result == 2)
               sthread_yield();
         } while (result == 2);
         assert(result == 1);
         take(mp4, stream, &frame);
         videotoolbox_av1_test_export(&frame, depth, position, hashes[target]);
      }
      assert(videotoolbox_av1_test_session_opens() == opens + 1);
      if (mp4)
         rmp4_video_stream_close(mp4);
      else
         rwebm_video_stream_close(stream);
      videotoolbox_hevc_test_stop(&renderer);
      videotoolbox_hevc_test_retired();
      videotoolbox_av1_test_export(&held, depth, position, hashes[0]);
      videotoolbox_hevc_test_movie_reset();
   }
   videotoolbox_hevc_test_device_release(device);
   puts("PASS: native AV1 container Metal pixels, repeated seeks, reopen "
        "and retained pool exports");
   return 0;
}
#endif

int main(int argc, char **argv)
{
   uint8_t *data, *raw;
   size_t size, raw_size;
   unsigned depth, position, grain, webm;
   int result = 77;
   puts("MODE: native-container");
   assert(argc == 7);
   depth = (unsigned)atoi(argv[4]);
   position = (unsigned)atoi(argv[5]);
   grain = (unsigned)atoi(argv[6]);
   assert((depth == 8 || depth == 10) && position <= 2 && grain <= 1);
   data = load(argv[2], &size);
   raw = load(argv[3], &raw_size);
   references(raw, raw_size, depth);
   webm = size >= 4 && data[0] == 0x1a && data[1] == 0x45 && data[2] == 0xdf
         && data[3] == 0xa3;
   assert(webm || (size >= 12 && !memcmp(data + 4, "ftyp", 4)));
#if defined(__APPLE__) && defined(HAVE_THREADS)
   result = execute(data, size, webm, depth, position);
#else
   puts("UNAVAILABLE: native AV1 container playback requires Apple threads");
#endif
   free(raw);
   free(data);
   return result;
}
