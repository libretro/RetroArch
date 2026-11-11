/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "../../../record/record_stream_d3d12.h"
#include "../../../record/record_mf_audio.h"
#include "../gpu_av_output/mock_worker.h"
#include <stdlib.h>
#define main session_reference_main
#include "../../../libretro-common/samples/net/rtmp/rtmp_session_test.c"
#undef main

struct net_rtmp_client
{
   net_rtmp_session_t *session;
   bool blocked, wire_live, delay_discard, cancelled;
};
struct record_mf_audio
{
   uint64_t frames;
   int16_t samples[2048];
   uint8_t asc[2], bytes[8];
   size_t pending;
   unsigned rate, channels;
   bool input, held, finishing, ended, cancelled;
};
static struct net_rtmp_client *client_owner;
static struct record_mf_audio *aac_owner;
static unsigned submitted, returned, submit_busy;
static int16_t pcm[4096];

enum net_rtmp_client_state net_rtmp_client_step(net_rtmp_client_t *c,
      uint32_t time, net_rtmp_client_view_t *view)
{
   net_rtmp_span_t span;
   (void)time;
   view->session = c->session;
   view->fd = -1;
   view->read = true;
   view->write = view->again = false;
   if (c->cancelled)
      return NET_RTMP_CLIENT_FAILED;
   if (c->blocked)
   {
      c->wire_live = net_rtmp_session_get(c->session, &span);
      return NET_RTMP_CLIENT_READY;
   }
   output_size = 0;
   pump(c->session);
   c->wire_live = false;
   return net_rtmp_session_status(c->session, NULL, NULL) == NET_RTMP_SESSION_FINISHED
      ? NET_RTMP_CLIENT_FINISHED : NET_RTMP_CLIENT_READY;
}
void net_rtmp_client_cancel(net_rtmp_client_t *c)
{
   c->cancelled = true;
   net_rtmp_session_cancel(c->session);
   if (c->delay_discard)
   {
      c->delay_discard = false;
      return;
   }
   assert(net_rtmp_session_discard(c->session));
   c->wire_live = false;
}

record_mf_audio_t *record_mf_audio_new(unsigned rate, unsigned channels, unsigned bitrate)
{
   record_mf_audio_t *e;
   unsigned index = rate == 44100 ? 4 : 3;
   assert((rate == 44100 || rate == 48000) && channels == 2 && bitrate == 128000);
   e = (record_mf_audio_t*)calloc(1, sizeof(*e));
   assert(e);
   e->rate = rate;
   e->channels = channels;
   e->asc[0] = (uint8_t)(16 | (index >> 1));
   e->asc[1] = (uint8_t)((index << 7) | (channels << 3));
   aac_owner = e;
   return e;
}
bool record_mf_audio_config(record_mf_audio_t *e, const void **data, size_t *size)
{
   *data = e->asc;
   *size = sizeof(e->asc);
   return true;
}
int record_mf_audio_input(record_mf_audio_t *e, int16_t **samples, size_t *frames)
{
   assert(!e->held && !e->pending && !e->finishing && !e->cancelled);
   e->input = true;
   *samples = e->samples;
   *frames = 1024;
   return RECORD_MF_AUDIO_PROGRESS;
}
int record_mf_audio_submit(record_mf_audio_t *e, size_t frames, uint64_t index)
{
   size_t i;
   assert(e->input && !e->pending && !e->held && index == e->frames && frames <= 1024);
   for (i = 0; i < frames; i++)
      assert(e->samples[2 * i] == 1234 && e->samples[2 * i + 1] == -5678);
   if (submit_busy)
   {
      submit_busy--;
      return RECORD_MF_AUDIO_BUSY;
   }
   e->input = false;
   e->pending = frames;
   submitted += (unsigned)frames;
   return RECORD_MF_AUDIO_PROGRESS;
}
int record_mf_audio_pump(record_mf_audio_t *e, record_mf_audio_packet_t *packet)
{
   assert(!e->cancelled);
   if (e->pending)
   {
      e->held = true;
      packet->data = e->bytes;
      packet->size = sizeof(e->bytes);
      packet->timestamp = (int64_t)(e->frames * 10000000 / e->rate);
      packet->duration = (int64_t)(e->pending * 10000000 / e->rate);
      return RECORD_MF_AUDIO_PACKET;
   }
   if (e->finishing)
   {
      e->ended = true;
      return RECORD_MF_AUDIO_END;
   }
   return RECORD_MF_AUDIO_BUSY;
}
bool record_mf_audio_release(record_mf_audio_t *e)
{
   assert(e->held && !client_owner->wire_live);
   e->frames += e->pending;
   e->held = false;
   e->pending = 0;
   returned++;
   return true;
}
int record_mf_audio_finish(record_mf_audio_t *e)
{
   assert(!e->input && !e->held && !e->pending);
   e->finishing = true;
   return RECORD_MF_AUDIO_PROGRESS;
}
bool record_mf_audio_cancel(record_mf_audio_t *e)
{
   assert(!e->held);
   e->input = false;
   e->pending = 0;
   e->cancelled = true;
   return true;
}
bool record_mf_audio_free(record_mf_audio_t *e)
{
   if (!e)
      return true;
   assert(!e->input && !e->held && (e->ended || e->cancelled));
   aac_owner = NULL;
   free(e);
   return true;
}

static void make_config(struct net_rtmp_client *c,
      record_gpu_worker_d3d12_t *w, record_gpu_output_config_t *config)
{
   static const uint8_t sets[] = { 0, 0, 0, 1, 103, 66, 0, 30, 0, 0, 1, 104 };
   memset(c, 0, sizeof(*c));
   memset(w, 0, sizeof(*w));
   memset(config, 0, sizeof(*config));
   c->session = ready();
   client_owner = c;
   w->frames = 4;
   w->duration = 1;
   config->video.headers = sets;
   config->video.headers_size = sizeof(sets);
   config->video.max_packet_size = 65536;
   config->video.timescale = 60;
   config->video.bit_depth = 8;
   config->audio_rate = 44100;
   config->audio_layout = 3;
   config->audio_kbps = 128;
   config->audio_fifo_frames = 2048;
   submitted = returned = submit_busy = 0;
}

#ifndef STREAM_CONTRACT_HELPERS_ONLY
static void hevc_admission(void)
{
   static const uint8_t sets[] = {
      0,0,0,1,64,1,12,1,255,255,1,96,0,0,3,0,144,0,0,3,0,0,3,0,93,151,2,64,
      0,0,0,1,66,1,1,1,96,0,0,3,0,144,0,0,3,0,0,3,0,93,160,10,8,12,22,89,
      116,164,33,25,23,84,48,22,160,32,32,32,128,0,0,3,0,128,0,0,30,4,
      0,0,0,1,68,1,193,247,192,204,144
   };
   struct net_rtmp_client client;
   record_gpu_worker_d3d12_t worker;
   record_gpu_output_config_t config;
   record_stream_d3d12_t *stream;
   record_stream_wait_t wait;
   unsigned i;
   int result;
   make_config(&client, &worker, &config);
   worker.frames = 0;
   config.video.codec = RECORD_MP4_HEVC;
   config.video.headers = sets;
   config.video.headers_size = sizeof(sets);
   config.audio_rate = config.audio_layout = config.audio_kbps = 0;
   config.audio_fifo_frames = 0;
   config.video.bit_depth = 10;
   assert(!record_stream_d3d12_new(&worker, &config, &client));
   config.video.bit_depth = 8;
   config.stream_color.transfer = 256;
   assert(!record_stream_d3d12_new(&worker, &config, &client));
   config.stream_color.transfer = 0;
   config.video.max_packet_size = 0xfffffa;
   assert(!record_stream_d3d12_new(&worker, &config, &client));
   config.video.max_packet_size = 0xfffff7;
   stream = record_stream_d3d12_new(&worker, &config, &client);
   assert(stream && record_stream_d3d12_audio_finish(stream));
   result = RECORD_GPU_OUTPUT_PROGRESS;
   for (i = 0; i < 1000 && result != RECORD_GPU_OUTPUT_END; i++)
   {
      result = record_stream_d3d12_step(stream, 0, &wait);
      assert(result != RECORD_GPU_OUTPUT_ERROR);
   }
   assert(result == RECORD_GPU_OUTPUT_END && record_stream_d3d12_free(stream));
   assert(net_rtmp_session_free(client.session));
   puts("PASS HEVC native header admission, depth/color/packet bounds and video-only retirement");
}

static record_stream_d3d12_t *make_stream(struct net_rtmp_client *c,
      record_gpu_worker_d3d12_t *w)
{
   record_gpu_output_config_t config;
   make_config(c, w, &config);
   return record_stream_d3d12_new(w, &config, c);
}

int main(void)
{
   struct net_rtmp_client client;
   record_gpu_worker_d3d12_t worker;
   record_stream_d3d12_t *stream;
   record_stream_wait_t wait;
   unsigned i;
   int result;
   hevc_admission();
   for (i = 0; i < sizeof(pcm) / sizeof(pcm[0]); i += 2)
   {
      pcm[i] = 1234;
      pcm[i + 1] = -5678;
   }
   stream = make_stream(&client, &worker);
   assert(stream && !record_stream_d3d12_free(stream));
   assert(record_stream_d3d12_audio(stream, pcm, 2048) == RECORD_GPU_OUTPUT_PROGRESS);
   assert(record_stream_d3d12_audio(stream, pcm, 1) == RECORD_GPU_OUTPUT_BUSY);
   assert(record_stream_d3d12_audio_finish(stream));
   for (i = 0; i < 1000 && !worker.held; i++)
      assert(record_stream_d3d12_step(stream, 0, &wait) == RECORD_GPU_OUTPUT_PROGRESS);
   assert(worker.held && !worker.releases);
   client.blocked = true;
   for (i = 0; i < 20; i++)
      assert(record_stream_d3d12_step(stream, 0, &wait) != RECORD_GPU_OUTPUT_ERROR);
   assert(client.wire_live && worker.held && !worker.releases);
   client.blocked = false;
   submit_busy = 1;
   result = RECORD_GPU_OUTPUT_BUSY;
   for (i = 0; i < 10000 && result != RECORD_GPU_OUTPUT_END; i++)
   {
      result = record_stream_d3d12_step(stream, 0, &wait);
      assert(result != RECORD_GPU_OUTPUT_ERROR);
   }
   assert(result == RECORD_GPU_OUTPUT_END && worker.releases == 4);
   assert(submitted == 2048 && returned == 2 && !submit_busy);
   assert(record_stream_d3d12_free(stream) && !aac_owner);
   assert(net_rtmp_session_free(client.session));
   stream = make_stream(&client, &worker);
   assert(stream && record_stream_d3d12_audio(stream, pcm, 1025) == RECORD_GPU_OUTPUT_PROGRESS);
   assert(record_stream_d3d12_audio_finish(stream));
   result = RECORD_GPU_OUTPUT_BUSY;
   for (i = 0; i < 10000 && result != RECORD_GPU_OUTPUT_END; i++)
   {
      result = record_stream_d3d12_step(stream, 0, &wait);
      assert(result != RECORD_GPU_OUTPUT_ERROR);
   }
   assert(result == RECORD_GPU_OUTPUT_END && submitted == 1025 && returned == 2);
   assert(record_stream_d3d12_free(stream) && net_rtmp_session_free(client.session));
   stream = make_stream(&client, &worker);
   worker.frames = 0;
   assert(stream && record_stream_d3d12_audio(stream, pcm, 1024) == RECORD_GPU_OUTPUT_PROGRESS);
   for (i = 0; i < 1000 && !aac_owner->held; i++)
      assert(record_stream_d3d12_step(stream, 0, &wait) != RECORD_GPU_OUTPUT_ERROR);
   assert(aac_owner->held);
   client.blocked = true;
   for (i = 0; i < 10; i++)
      assert(record_stream_d3d12_step(stream, 0, &wait) != RECORD_GPU_OUTPUT_ERROR);
   assert(client.wire_live && aac_owner->held && !returned);
   client.delay_discard = true;
   record_stream_d3d12_abort(stream);
   assert(record_stream_d3d12_step(stream, 0, &wait) == RECORD_GPU_OUTPUT_BUSY);
   assert(aac_owner->held && !returned && !record_stream_d3d12_free(stream));
   assert(record_stream_d3d12_step(stream, 0, &wait) == RECORD_GPU_OUTPUT_ERROR);
   assert(!aac_owner->held && returned == 1 && !record_stream_d3d12_free(stream));
   assert(record_stream_d3d12_audio_finish(stream));
   assert(record_stream_d3d12_free(stream) && net_rtmp_session_free(client.session));
   stream = make_stream(&client, &worker);
   worker.frames = 0;
   assert(stream && record_stream_d3d12_audio(stream, pcm, 1) == RECORD_GPU_OUTPUT_PROGRESS);
   for (i = 0; i < 1000 && !aac_owner->input; i++)
      assert(record_stream_d3d12_step(stream, 0, &wait) != RECORD_GPU_OUTPUT_ERROR);
   assert(aac_owner->input);
   record_stream_d3d12_abort(stream);
   assert(record_stream_d3d12_step(stream, 0, &wait) == RECORD_GPU_OUTPUT_ERROR);
   assert(!aac_owner->input && record_stream_d3d12_audio_finish(stream));
   assert(record_stream_d3d12_free(stream) && net_rtmp_session_free(client.session));
   puts("PASS stream video/AAC lease retention, direct PCM input, partial drain and cancellation");
   return 0;
}
#endif
