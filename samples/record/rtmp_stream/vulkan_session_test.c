/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#define STREAM_CONTRACT_HELPERS_ONLY
#include "contract_test.c"
#define main vk_file_session_reference_main
#define cancellation vk_file_session_cancellation
#include "../gpu_av_output/vulkan_session_test.c"
#undef main
#undef cancellation

static record_gpu_session_vulkan_t *stream_session_new(struct net_rtmp_client *client,
      bool audio, bool bad_config)
{
   record_gpu_worker_d3d12_t dummy;
   record_gpu_output_config_t common;
   record_gpu_output_vulkan_config_t output;
   vulkan_encode_h264_open_t encoder;
   static const vulkan_encode_h264_ops_t ops = {{0}, {0}, {0}};
   make_config(client, &dummy, &common);
   memset(&output, 0, sizeof(output));
   memset(&encoder, 0, sizeof(encoder));
   output.video.max_packet_size = bad_config ? 0 : 65536;
   output.stream_client = client;

   if (audio)
   {
      output.audio_rate = common.audio_rate;
      output.audio_layout = common.audio_layout;
      output.audio_kbps = common.audio_kbps;
      output.audio_fifo_frames = common.audio_fifo_frames;
   }
   encoder.slots = 2;
   encoder.session.video.width = encoder.session.video.height = 64;
   encoder.session.video.fps_num = 30000;
   encoder.session.video.fps_den = 1001;
   return record_gpu_session_vulkan_new(&encoder, &ops, &output, "rtmp://no-file/live/test");
}
static void stream_session_normal(bool audio)
{
   struct net_rtmp_client client;
   record_gpu_session_vulkan_t *s;
   int fd;
   bool read, write;
   reset();
   file_open_fail = true;
   s = stream_session_new(&client, audio, false);
   assert(s && record_gpu_session_vulkan_error(s, NULL) == RECORD_VULKAN_SESSION_NONE);
   assert(!record_gpu_session_vulkan_network(s, &fd, &read, &write));
   client.descriptor = 27;
   assert(record_gpu_session_vulkan_step(s) == RECORD_VULKAN_OUTPUT_PROGRESS);
   assert(record_gpu_session_vulkan_network(s, &fd, &read, &write));
   assert(fd == 27 && read && !write);
   publish(record_gpu_session_vulkan_queue(s), 0);
   if (audio) assert(record_gpu_session_vulkan_audio(s, pcm, 1025) == RECORD_VULKAN_OUTPUT_PROGRESS);
   assert(record_gpu_session_vulkan_video_finish(s) && record_gpu_session_vulkan_audio_finish(s));
   assert(terminal(s) == RECORD_VULKAN_OUTPUT_END && opens == frees);
   assert(record_gpu_session_vulkan_audio_frames(s) == (audio ? 1025u : 0u));
   assert(!record_gpu_session_vulkan_network(s, &fd, &read, &write));
   assert(fd == -1 && !read && !write && !file_writes);
   assert(record_gpu_session_vulkan_free(s) && net_rtmp_session_free(client.session));
}
static void stream_session_cancel(bool audio)
{
   struct net_rtmp_client client;
   record_gpu_session_vulkan_t *s;
   unsigned i, old_frees;
   int fd;
   bool read, write;
   reset();
   file_open_fail = true;
   old_frees = frees;
   s = stream_session_new(&client, audio, false);
   assert(s && record_gpu_session_vulkan_error(s, NULL) == RECORD_VULKAN_SESSION_NONE);
   if (audio)
      assert(record_gpu_session_vulkan_audio(s, pcm, 1024) == RECORD_VULKAN_OUTPUT_PROGRESS);
   else
      publish(record_gpu_session_vulkan_queue(s), 0);
   for (i = 0; i < 1000; i++)
   {
      assert(record_gpu_session_vulkan_step(s) != RECORD_VULKAN_OUTPUT_ERROR);
      if (audio ? aac_owner->held : ((vulkan_encode_h264_t*)s->encoder)->locked) break;
   }
   assert(i < 1000);
   client.blocked = true;
   for (i = 0; i < 20 && !client.wire_live; i++)
      assert(record_gpu_session_vulkan_step(s) != RECORD_VULKAN_OUTPUT_ERROR);
   assert(client.wire_live);
   client.delay_discard = true;
   record_gpu_session_vulkan_cancel(s);
   assert(record_gpu_session_vulkan_step(s) == RECORD_VULKAN_OUTPUT_BUSY);
   assert(frees == old_frees && !record_gpu_session_vulkan_free(s));
   if (!audio) release_fail = true;
   assert(record_gpu_session_vulkan_video_finish(s));
   assert(record_gpu_session_vulkan_step(s) == RECORD_VULKAN_OUTPUT_BUSY);
   assert(frees == old_frees);
   release_fail = false;
   assert(record_gpu_session_vulkan_audio_finish(s));
   assert(terminal(s) == RECORD_VULKAN_OUTPUT_ERROR);
   assert(record_gpu_session_vulkan_error(s, NULL) == RECORD_VULKAN_SESSION_CANCELLED);
   assert(opens == frees && !file_writes);
   assert(!record_gpu_session_vulkan_network(s, &fd, &read, &write));
   assert(record_gpu_session_vulkan_free(s) && net_rtmp_session_free(client.session));
}
#ifndef STREAM_PRESSURE_HELPERS_ONLY
int main(void)
{
   struct net_rtmp_client client;
   record_gpu_session_vulkan_t *s;
   unsigned i;
   assert(vk_file_session_reference_main() == 0);
   for (i = 0; i < sizeof(pcm) / sizeof(pcm[0]); i += 2)
   {
      pcm[i] = 1234;
      pcm[i + 1] = -5678;
   }
   stream_session_normal(false);
   stream_session_normal(true);
   stream_session_cancel(false);
   stream_session_cancel(true);
   reset();
   s = stream_session_new(&client, false, true);
   assert(s && record_gpu_session_vulkan_error(s, NULL) == RECORD_VULKAN_SESSION_STARTUP);
   assert(record_gpu_session_vulkan_video_finish(s) && record_gpu_session_vulkan_audio_finish(s));
   assert(terminal(s) == RECORD_VULKAN_OUTPUT_ERROR && opens == frees);
   assert(record_gpu_session_vulkan_free(s));
   net_rtmp_client_cancel(&client);
   assert(net_rtmp_session_free(client.session));
   puts("PASS Vulkan stream session producer closure, network-held video/AAC, cancellation and startup retirement");
   return 0;
}

#endif
