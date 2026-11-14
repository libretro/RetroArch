/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifdef NDEBUG
#error Receiver acceptance requires assertions
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <features/features_cpu.h>
#include <net/net_compat.h>
#include <net/net_socket.h>
#include <net/net_socket_ssl.h>
#include <rthreads/rthreads.h>
#include <streams/file_stream.h>
#include <retro_miscellaneous.h>
#include <retro_timers.h>
#include "../../../record/record_stream_mediacodec.h"
struct record_mediacodec_encoder
{
   void *data;
   size_t bytes;
};
struct receiver_resolver { void (*work)(void*); void *job; };
static void resolver_run(void *opaque)
{
   struct receiver_resolver *r = (struct receiver_resolver*)opaque;
   r->work(r->job); free(r);
}
static bool resolve_dispatch(void (*work)(void*), void *job, void *context)
{
   sthread_t **thread = (sthread_t**)context;
   struct receiver_resolver *r;
   assert(!*thread);
   r = (struct receiver_resolver*)malloc(sizeof(*r));
   if (!r) return false;
   r->work = work; r->job = job;
   *thread = sthread_create(resolver_run, r);
   if (*thread) return true;
   free(r); return false;
}
bool record_mediacodec_encoder_video_config(record_mediacodec_encoder_t *s,
      unsigned index, const void **data, size_t *bytes)
{
   *data = index ? NULL : s->data; *bytes = index ? 0 : s->bytes;
   return *data && *bytes;
}
bool record_mediacodec_encoder_audio_config(record_mediacodec_encoder_t *s,
      const void **data, size_t *bytes)
{
   *data = s->data; *bytes = s->bytes; return *data && *bytes;
}
static unsigned word(const uint8_t *p)
{
   return p[0] | (unsigned)p[1] << 8 | (unsigned)p[2] << 16 | (unsigned)p[3] << 24;
}
static void load_config(FILE *file, record_mediacodec_encoder_t *s)
{
   uint8_t bytes[4];
   assert(fread(bytes, 1, sizeof(bytes), file) == sizeof(bytes));
   s->bytes = word(bytes); assert(s->bytes <= 65535);
   s->data = s->bytes ? malloc(s->bytes) : NULL;
   assert(!s->bytes || (s->data && fread(s->data, 1, s->bytes, file) == s->bytes));
}
int main(int argc, char **argv)
{
   static uint8_t random[1528];
   record_mediacodec_encoder_t video = {0}, audio = {0};
   record_mediacodec_stream_config_t mux;
   net_rtmp_client_config_t config;
   net_rtmp_client_view_t wait;
   record_stream_mediacodec_t *stream;
   net_rtmp_client_t *client;
   const record_mediacodec_output_ops_t *ops = record_stream_mediacodec_ops();
   record_mediacodec_packet_t packet;
   sthread_t *resolver = NULL;
   retro_time_t deadline;
   FILE *file;
   uint8_t header[20], *payload = NULL;
   uint64_t timestamp;
   unsigned i, count[2] = {0,0}, track = 0;
   int result, accepted, started;
   bool eof = false, closing = false;
   enum net_rtmp_client_state state = NET_RTMP_CLIENT_RESOLVING, previous;
   char *pem = NULL;
   int64_t pem_size = 0;
   const char *ca = getenv("RTMPS_CA");
   assert(argc == 4 && network_init());
   if (ca)
   {
      assert(filestream_read_file(ca, (void**)&pem, &pem_size) && pem_size > 0);
      ssl_socket_retro_set_trust_pem(pem, (size_t)pem_size);
   }
   file = fopen(argv[2], "rb"); assert(file);
   load_config(file, &video); load_config(file, &audio);
   for (i = 0; i < sizeof(random); i++) random[i] = (uint8_t)(i * 13 + 1);
   memset(&config, 0, sizeof(config));
   config.url = argv[1]; config.random = random;
   config.dispatch = resolve_dispatch; config.userdata = &resolver;
   config.max_reply = 65536; config.streams = 8;
   config.legacy_commands = !strcmp(argv[3], "legacy");
   client = net_rtmp_client_new(&config); assert(client);
   deadline = cpu_features_get_time_usec() + 10000000;
   do
   {
      assert(cpu_features_get_time_usec() < deadline);
      previous = state;
      state = net_rtmp_client_step(client, 0, &wait);
      if (state == NET_RTMP_CLIENT_FAILED)
      {
         assert(previous == NET_RTMP_CLIENT_RESOLVING || previous == NET_RTMP_CLIENT_CONNECTING);
         net_rtmp_client_cancel(client); assert(net_rtmp_client_free(client));
         if (resolver) sthread_join(resolver);
         fclose(file); free(video.data); free(audio.data);
         ssl_socket_retro_set_trust_pem(NULL, 0); free(pem);
         return 78;
      }
      if (!wait.again && state != NET_RTMP_CLIENT_READY)
      {
         if (wait.fd >= 0) socket_wait(wait.fd, &wait.read, &wait.write, 1);
         else retro_sleep(1);
      }
   } while (state != NET_RTMP_CLIENT_READY);
   memset(&mux, 0, sizeof(mux)); mux.bit_depth = 8; mux.max_packet_size = 0xfffff7;
   stream = record_stream_mediacodec_new(&video, audio.bytes ? &audio : NULL, client, &mux);
   assert(stream && ops->track(stream, false, &video));
   if (audio.bytes) assert(ops->track(stream, true, &audio));
   deadline = cpu_features_get_time_usec() + 20000000;
   for (;;)
   {
      assert(cpu_features_get_time_usec() < deadline);
      result = record_stream_mediacodec_step(stream, 0, &wait);
      assert(result >= 0);
      started = ops->start(stream); assert(started >= 0);
      if (started && !closing)
      {
         if (!payload && !eof)
         {
            size_t size = fread(header, 1, sizeof(header), file);
            assert(!size || size == sizeof(header)); eof = !size;
            if (!eof)
            {
               track = word(header); assert(track < 2 && (!track || audio.bytes));
               packet.flags = word(header + 4);
               timestamp = word(header + 8) | (uint64_t)word(header + 12) << 32;
               assert(timestamp < ((uint64_t)1 << 63)); packet.pts_us = (int64_t)timestamp;
               packet.size = word(header + 16); assert(packet.size && packet.size < 1048576);
               payload = (uint8_t*)malloc(packet.size); assert(payload);
               assert(fread(payload, 1, packet.size, file) == packet.size); packet.data = payload;
            }
         }
         if (payload)
         {
            accepted = ops->write(stream, track != 0, &packet); assert(accepted >= 0);
            if (accepted) { count[track]++; free(payload); payload = NULL; }
         }
         else if (eof)
         {
            assert(ops->end(stream, false, 1000000));
            if (audio.bytes) assert(ops->end(stream, true, 1000000));
            closing = true;
         }
      }
      if (closing)
      {
         accepted = ops->finish(stream); assert(accepted >= 0);
         if (accepted) break;
      }
      if (!result && !wait.again)
      {
         if (wait.fd >= 0) socket_wait(wait.fd, &wait.read, &wait.write, 1);
         else retro_sleep(1);
      }
   }
   assert(record_stream_mediacodec_free(stream));
   assert(net_rtmp_client_free(client));
   if (resolver) sthread_join(resolver);
   fclose(file); free(video.data); free(audio.data);
   ssl_socket_retro_set_trust_pem(NULL, 0); free(pem);
   printf("PASS video=%u audio=%u native_mediacodec=0\n", count[0], count[1]);
   return 0;
}
