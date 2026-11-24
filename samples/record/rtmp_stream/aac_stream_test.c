/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#define STREAM_CONTRACT_HELPERS_ONLY
#define net_rtmp_client_step aac_stream_reference_step
#include "contract_test.c"
#undef net_rtmp_client_step
#include <math.h>

static uint8_t aac_stream_wire[65536];
static size_t aac_stream_wire_size;

enum net_rtmp_client_state net_rtmp_client_step(net_rtmp_client_t *c,
      uint32_t time, net_rtmp_client_view_t *view)
{
   enum net_rtmp_client_state state = aac_stream_reference_step(c, time, view);
   if (!c->blocked && !c->cancelled)
   {
      assert(output_size <= sizeof(aac_stream_wire) - aac_stream_wire_size);
      memcpy(aac_stream_wire + aac_stream_wire_size, output, output_size);
      aac_stream_wire_size += output_size;
   }
   return state;
}

int main(int argc, char **argv)
{
   struct net_rtmp_client client;
   record_gpu_worker_d3d12_t worker;
   record_gpu_output_config_t config;
   record_stream_d3d12_t *stream;
   record_stream_wait_t wait;
   unsigned i, hold, sent = 0;
   int result = RECORD_GPU_OUTPUT_BUSY;
   bool closed = false, blocked = false;
   make_config(&client, &worker, &config);
   worker.frames = 12;
   stream = record_stream_d3d12_new(&worker, &config, &client);
   assert(stream && !aac_owner);
   for (i = 0; i < 10000 && result != RECORD_GPU_OUTPUT_END; i++)
   {
      if (sent < 8365)
      {
         unsigned frame, count = 8365 - sent;
         int accepted;
         if (count > 2048)
            count = 2048;
         for (frame = 0; frame < count; frame++)
         {
            unsigned at = sent + frame;
            pcm[frame * 2] = at < 500 ? 0 : (int16_t)(12000 * sin(6.283185307179586 * 440 * at / 44100));
            pcm[frame * 2 + 1] = at < 500 ? 0 : (int16_t)(12000 * sin(6.283185307179586 * 880 * at / 44100));
         }
         accepted = record_stream_d3d12_audio(stream, pcm, count);
         assert(accepted == RECORD_GPU_OUTPUT_PROGRESS || accepted == RECORD_GPU_OUTPUT_BUSY);
         if (accepted == RECORD_GPU_OUTPUT_PROGRESS)
            sent += count;
      }
      if (sent == 8365 && !closed)
      {
         assert(record_stream_d3d12_audio_finish(stream));
         closed = true;
      }
      result = record_stream_d3d12_step(stream, 0, &wait);
      assert(result != RECORD_GPU_OUTPUT_ERROR);
      if (!blocked && worker.held)
      {
         unsigned released = worker.releases;
         client.blocked = true;
         for (hold = 0; hold < 20; hold++)
            assert(record_stream_d3d12_step(stream, 0, &wait) != RECORD_GPU_OUTPUT_ERROR);
         assert(worker.held && worker.releases == released && client.wire_live);
         client.blocked = false;
         blocked = true;
      }
   }
   assert(result == RECORD_GPU_OUTPUT_END && worker.releases == 12 && blocked);
   assert(record_stream_gpu_audio_frames(stream) == 8365);
   assert(record_stream_d3d12_free(stream) && net_rtmp_session_free(client.session));
   assert(!aac_owner);
   if (argc > 1)
   {
      FILE *out = fopen(argv[1], "wb");
      assert(out && fwrite(aac_stream_wire, 1, aac_stream_wire_size, out) == aac_stream_wire_size);
      assert(!fclose(out));
   }
   make_config(&client, &worker, &config);
   worker.frames = 0;
   stream = record_stream_d3d12_new(&worker, &config, &client);
   assert(stream);
   assert(record_stream_d3d12_audio(stream, pcm, 1) == RECORD_GPU_OUTPUT_PROGRESS);
   for (i = 0; i < 10; i++)
      assert(record_stream_d3d12_step(stream, 0, &wait) != RECORD_GPU_OUTPUT_ERROR);
   record_stream_d3d12_abort(stream);
   assert(record_stream_d3d12_step(stream, 0, &wait) == RECORD_GPU_OUTPUT_ERROR);
   assert(record_stream_d3d12_audio_finish(stream));
   assert(record_stream_d3d12_free(stream) && net_rtmp_session_free(client.session));
   puts("PASS builtin AAC through shared GPU stream: bounded PCM, network-held video, partial drain and cancellation");
   return 0;
}
