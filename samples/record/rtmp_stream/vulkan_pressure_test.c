/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#define STREAM_PRESSURE_HELPERS_ONLY
#include "vulkan_session_test.c"

static uint32_t pressure_hash(const net_rtmp_span_t *span)
{
   const uint8_t *bytes = (const uint8_t*)span->data;
   uint32_t value = 2166136261u;
   size_t i;
   for (i = 0; i < span->size; i++)
      value = (value ^ bytes[i]) * 16777619u;
   return value;
}
static void pressure_case(bool audio, bool cancel)
{
   struct net_rtmp_client client;
   record_gpu_session_vulkan_t *s;
   record_gpu_queue_t *q;
   net_rtmp_span_t held, span;
   uint64_t accepted = 0;
   size_t bytes, calls;
   uint32_t hash;
   unsigned i, slot, held_releases;
   assert(!pressure_bytes && !pressure_live);
   reset();
   file_open_fail = true;
   s = stream_session_new(&client, audio, false);
   assert(s && !record_gpu_session_vulkan_error(s, NULL));
   q = record_gpu_session_vulkan_queue(s);
   publish(q, 0);
   if (audio)
   {
      assert(record_gpu_session_vulkan_audio(s, pcm, 1024)
            == RECORD_VULKAN_OUTPUT_PROGRESS);
      accepted = 1024;
   }
   for (i = 0; i < 1000; i++)
   {
      assert(record_gpu_session_vulkan_step(s) != RECORD_VULKAN_OUTPUT_ERROR);
      if (((vulkan_encode_h264_t*)s->encoder)->locked)
         break;
   }
   assert(i < 1000);
   client.blocked = true;
   for (i = 0; i < 100; i++)
      assert(record_gpu_session_vulkan_step(s) != RECORD_VULKAN_OUTPUT_ERROR);
   assert(client.wire_live && net_rtmp_session_get(client.session, &held));
   hash = pressure_hash(&held);
   publish(q, 1);
   assert(record_gpu_queue_acquire(q, &slot) == RECORD_GPU_QUEUE_BUSY);
   if (audio)
   {
      for (i = 0; i < 16; i++)
      {
         int result = record_gpu_session_vulkan_audio(s, pcm, 1024);
         if (result == RECORD_VULKAN_OUTPUT_BUSY)
            break;
         assert(result == RECORD_VULKAN_OUTPUT_PROGRESS);
         accepted += 1024;
      }
      assert(i < 16);
   }
   bytes = pressure_bytes;
   calls = pressure_calls;
   held_releases = releases;
   for (i = 0; i < 10000; i++)
   {
      assert(record_gpu_session_vulkan_step(s) == RECORD_VULKAN_OUTPUT_BUSY);
      assert(record_gpu_queue_acquire(q, &slot) == RECORD_GPU_QUEUE_BUSY);
      if (audio)
         assert(record_gpu_session_vulkan_audio(s, pcm, 1024)
               == RECORD_VULKAN_OUTPUT_BUSY);
      assert(net_rtmp_session_get(client.session, &span));
      assert(span.data == held.data && span.size == held.size);
      assert(pressure_hash(&span) == hash && releases == held_releases);
      assert(pressure_bytes == bytes && pressure_calls == calls);
   }
   assert(record_gpu_session_vulkan_video_finish(s));
   assert(record_gpu_session_vulkan_audio_finish(s));
   if (cancel)
      record_gpu_session_vulkan_cancel(s);
   else
      client.blocked = false;
   assert(terminal(s) == (cancel ? RECORD_VULKAN_OUTPUT_ERROR : RECORD_VULKAN_OUTPUT_END));
   if (!cancel)
      assert(record_gpu_session_vulkan_audio_frames(s) == accepted);
   assert(opens == frees && !file_writes);
   assert(record_gpu_session_vulkan_free(s));
   assert(net_rtmp_session_free(client.session));
   assert(!pressure_bytes && !pressure_live);
   printf("PASS bounded Vulkan stream pressure: audio=%u cancel=%u, 10000 blocked steps, %lu fixed bytes, zero allocations\n",
         (unsigned)audio, (unsigned)cancel, (unsigned long)bytes);
}
int main(void)
{
   unsigned i;
   for (i = 0; i < sizeof(pcm) / sizeof(pcm[0]); i += 2)
   {
      pcm[i] = 1234;
      pcm[i + 1] = -5678;
   }
   pressure_case(false, false);
   pressure_case(true, false);
   pressure_case(false, true);
   pressure_case(true, true);
   return 0;
}
