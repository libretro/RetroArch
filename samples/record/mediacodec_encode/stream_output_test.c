/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifdef NDEBUG
#error Output ownership checks require assertions
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../../record/record_stream_mediacodec.h"
struct record_mediacodec_encoder { bool audio; };
struct net_rtmp_client { int unused; };
struct net_rtmp_session { int unused; };
static struct net_rtmp_session session;
static bool credit, live, cancelled, closing, network_fail, connecting;
static unsigned messages, releases, retire_delay;
static const void *borrowed;
static const uint8_t csd[] = {0,0,0,1,103,66,0,30,0,0,0,1,104};
static const uint8_t asc[] = {18,16};
static uint8_t aac[] = {1,2,3,4};
static const uint8_t picture[] = {0,0,0,1,101,1,2};
bool record_mediacodec_encoder_video_config(record_mediacodec_encoder_t *s,
      unsigned index, const void **data, size_t *size)
{
   *data = NULL; *size = 0;
   if (!s || s->audio || index) return false;
   *data = csd; *size = sizeof(csd); return true;
}
bool record_mediacodec_encoder_audio_config(record_mediacodec_encoder_t *s,
      const void **data, size_t *size)
{
   *data = NULL; *size = 0;
   if (!s || !s->audio) return false;
   *data = asc; *size = sizeof(asc); return true;
}
enum net_rtmp_client_state net_rtmp_client_step(net_rtmp_client_t *c,
      uint32_t time, net_rtmp_client_view_t *view)
{
   (void)c; (void)time;
   memset(view, 0, sizeof(*view)); view->session = &session; view->fd = 7;
   view->write = live;
   if (network_fail) return NET_RTMP_CLIENT_FAILED;
   if (connecting) return NET_RTMP_CLIENT_CONNECTING;
   return closing ? NET_RTMP_CLIENT_FINISHED : NET_RTMP_CLIENT_READY;
}
void net_rtmp_client_cancel(net_rtmp_client_t *c)
{
   (void)c; cancelled = true;
}
bool net_rtmp_session_media(net_rtmp_session_t *s, unsigned type,
      uint32_t time, const net_rtmp_span_t *spans, size_t count)
{
   (void)s;
   assert(count == 2 && (type == 8 || type == 9));
   if (live || !credit) return false;
   if (messages >= 2) assert(time == 1000);
   if (type == 8 && messages >= 2) assert(spans[1].data == aac);
   borrowed = spans[1].data;
   live = true; messages++; return true;
}
bool net_rtmp_session_media_release(net_rtmp_session_t *s)
{
   (void)s;
   if (!live) return false;
   if (cancelled && retire_delay) { retire_delay--; return false; }
   if (!cancelled && !credit) return false;
   assert(borrowed); live = false; borrowed = NULL; releases++; return true;
}
bool net_rtmp_session_finish(net_rtmp_session_t *s)
{
   (void)s; assert(!live); closing = true; return true;
}
static void run(bool fail, bool audio_enabled)
{
   record_mediacodec_encoder_t video = {false}, audio = {true};
   struct net_rtmp_client client;
   record_mediacodec_stream_config_t config;
   record_stream_mediacodec_t *stream;
   const record_mediacodec_output_ops_t *ops = record_stream_mediacodec_ops();
   record_mediacodec_packet_t packet, other;
   net_rtmp_client_view_t wait;
   unsigned i;
   memset(&config, 0, sizeof(config)); config.max_packet_size = 1024; config.bit_depth = 8;
   credit = true; live = cancelled = closing = network_fail = false;
   connecting = true; messages = releases = retire_delay = 0;
   stream = record_stream_mediacodec_new(&video, audio_enabled ? &audio : NULL, &client, &config);
   assert(stream && !ops->start(stream));
   assert(ops->track(stream, false, &video));
   assert(!ops->track(stream, false, &video));
   if (audio_enabled) assert(ops->track(stream, true, &audio));
   else assert(!ops->track(stream, true, &audio));
   assert(record_stream_mediacodec_step(stream, 0, &wait) == 0);
   assert(!ops->start(stream)); connecting = false;
   for (i = 0; i < 40 && !ops->start(stream); i++)
      assert(record_stream_mediacodec_step(stream, 0, &wait) >= 0);
   assert(i < 40 && messages == (audio_enabled ? 2u : 1u));
   memset(&packet, 0, sizeof(packet));
   packet.pts_us = 1000000; packet.data = audio_enabled ? aac : picture;
   packet.size = audio_enabled ? sizeof(aac) : sizeof(picture); packet.flags = 1;
   other = packet;
   assert(!ops->write(stream, audio_enabled, &packet));
   assert(!ops->write(stream, audio_enabled, &other));
   for (i = 0; i < 40 && !live; i++)
      assert(record_stream_mediacodec_step(stream, 0, &wait) >= 0);
   assert(live && i < 40);
   credit = false;
   for (i = 0; i < 4; i++)
   {
      assert(record_stream_mediacodec_step(stream, 0, &wait) == 0);
      assert(!ops->write(stream, audio_enabled, &packet)); assert(live);
   }
   if (fail)
   {
      network_fail = true;
      assert(record_stream_mediacodec_step(stream, 0, &wait) == -1);
      retire_delay = 2;
      assert(!ops->retire(stream) && live);
      assert(!record_stream_mediacodec_free(stream) && live);
      assert(ops->retire(stream) && !live && cancelled);
   }
   else
   {
      credit = true;
      assert(record_stream_mediacodec_step(stream, 0, &wait) == 1);
      assert(!live && ops->write(stream, audio_enabled, &packet) == 1);
      assert(ops->end(stream, false, 2000000));
      if (audio_enabled) assert(ops->end(stream, true, 2000000));
      assert(ops->finish(stream) == 0);
      assert(record_stream_mediacodec_step(stream, 0, &wait) == 2);
      assert(ops->finish(stream) == 1);
   }
   assert(record_stream_mediacodec_free(stream));
   assert(messages == releases);
}
int main(void)
{
   run(false, true); run(false, false); run(true, true); run(true, false);
   puts("PASS: real FLV setup and packet conversion, borrowed AAC, network backpressure, closure and delayed cancellation retirement; transport mocked");
   return 0;
}
