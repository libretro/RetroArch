/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "../d3d11_video_decode/d3d11_video_decode_test_data.h"

static int h264_hash(rh264_video *v, uint32_t *hash, unsigned *count)
{
   unsigned plane;
   uint32_t value = UINT32_C(2166136261);
   if (*count >= 32) return 0;
   for (plane = 0; plane < 3; plane++)
   {
      int stride, width, height, x, y;
      const uint8_t *data = rh264_video_plane(v, (int)plane, &stride, &width, &height);
      if (!data) return 0;
      value = (value ^ (unsigned)width) * UINT32_C(16777619);
      value = (value ^ (unsigned)height) * UINT32_C(16777619);
      for (y = 0; y < height; y++)
         for (x = 0; x < width; x++) value = (value ^ data[y * stride + x]) * UINT32_C(16777619);
   }
   hash[(*count)++] = value;
   return 1;
}

static int h264_decode_fixture(const uint8_t *data, size_t size, int hardware,
      uint32_t *hash, unsigned *count)
{
   rmp4_t *mp4 = rmp4_open_memory(data, size);
   rh264_video *v = rh264_video_open();
   rmp4_packet packet;
   int selected = -1, i, ret, ok = 0;
   unsigned tries;
   *count = 0;
   if (!mp4 || !v) goto done;
   if (hardware) rh264_video_set_hw_readback(v, 1);
   for (i = 0; i < rmp4_num_tracks(mp4); i++)
   {
      const rmp4_track *track = rmp4_get_track(mp4, i);
      if (track->codec != RMP4_CODEC_H264) continue;
      if (rh264_video_set_extradata(v, track->codec_private, track->codec_private_size)) goto done;
      selected = i; break;
   }
   if (selected < 0) goto done;
   while ((ret = rmp4_read_packet(mp4, &packet)) == 1)
   {
      if (packet.track != selected) continue;
      tries = 0;
      do
      {
         ret = rh264_video_decode(v, packet.data, packet.size);
         if (ret == 2) vkDeviceWaitIdle(vvd.device);
      } while (ret == 2 && ++tries < 1024);
      if (ret < 0 || ret == 2 || (ret == 1 && !h264_hash(v, hash, count))) goto done;
   }
   if (ret < 0) goto done;
   while ((ret = rh264_video_drain(v)) >= 0)
      if (!h264_hash(v, hash, count)) goto done;
   ok = *count && (!hardware || rh264_video_is_hw(v));
done:
   vkDeviceWaitIdle(vvd.device);
   rh264_video_close(v); rmp4_close(mp4);
   return ok;
}

static int h264_regression(void)
{
   static const struct { const char *name; const uint8_t *data; size_t size; } streams[] = {
#define STREAM(n) { #n, stream_##n, sizeof(stream_##n) }
      STREAM(baseline), STREAM(main_bpyr), STREAM(high_slices), STREAM(high_jvt), STREAM(high_cqm), STREAM(cropped)
#undef STREAM
   };
   unsigned i, cpu_count, gpu_count;
   uint32_t cpu[32], gpu[32];
   int ok = 1;
   for (i = 0; i < sizeof(streams) / sizeof(streams[0]); i++)
   {
      int pass = h264_decode_fixture(streams[i].data, streams[i].size, 0, cpu, &cpu_count)
         && h264_decode_fixture(streams[i].data, streams[i].size, 1, gpu, &gpu_count)
         && cpu_count == gpu_count && !memcmp(cpu, gpu, cpu_count * sizeof(*cpu));
      printf("Vulkan H.264 regression %s: %s\n", streams[i].name, pass ? "PASS" : "FAIL");
      if (!pass) ok = 0;
   }
   return ok;
}
