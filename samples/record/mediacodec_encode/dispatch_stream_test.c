/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#define HAVE_NETWORKING
#define HAVE_CRYPTO
#define HAVE_RTMP
#define HAVE_RFLV
#define main file_dispatch_acceptance
#include "dispatch_test.c"
#undef main
#ifndef _WIN32
#include <sys/socket.h>
struct test_dns
{
   retro_atomic_int_t refs, ready;
};
struct net_rtmp_client
{
   struct test_dns *dns;
   int socket[2];
   unsigned retire;
};
static retro_atomic_int_t dns_hold, dns_live, client_live, test_clock;
static void test_dns_drop(struct test_dns *dns)
{
   if (retro_atomic_fetch_add_seq_cst_int(&dns->refs, -1) == 1)
   {
      retro_atomic_fetch_add_int(&dns_live, -1);
      free(dns);
   }
}
static void test_dns_run(void *opaque)
{
   struct test_dns *dns = (struct test_dns*)opaque;
   while (retro_atomic_load_acquire_int(&dns_hold))
      pause_test();
   retro_atomic_store_release_int(&dns->ready, 1);
   test_dns_drop(dns);
}
int crypto_random_bytes(uint8_t *data, size_t size)
{
   memset(data, 42, size);
   return 0;
}
retro_time_t cpu_features_get_time_usec(void)
{
   return retro_atomic_load_acquire_int(&test_clock);
}
net_rtmp_client_t *net_rtmp_client_new_caps(const net_rtmp_client_config_t *config,
      const net_rtmp_codec_caps_t *caps)
{
   net_rtmp_client_t *c = (net_rtmp_client_t*)calloc(1, sizeof(*c));
   assert(c && !strcmp(config->url, "rtmp://localhost/live/test"));
   assert(config->random[0] == 42 && caps->video_count == 1 && caps->audio_count == 1);
   assert(!strcmp(caps->video[0].fourcc, "hvc1"));
   c->dns = (struct test_dns*)calloc(1, sizeof(*c->dns));
   assert(c->dns && !socketpair(AF_UNIX, SOCK_STREAM, 0, c->socket));
   retro_atomic_int_init(&c->dns->refs, 2);
   retro_atomic_int_init(&c->dns->ready, 0);
   retro_atomic_fetch_add_int(&dns_live, 1);
   assert(config->dispatch(test_dns_run, c->dns, config->userdata));
   retro_atomic_fetch_add_int(&client_live, 1);
   return c;
}
enum net_rtmp_client_state net_rtmp_client_step(net_rtmp_client_t *c,
      uint32_t time, net_rtmp_client_view_t *wait)
{
   (void)time;
   memset(wait, 0, sizeof(*wait)); wait->fd = c->socket[0]; wait->read = true;
   return retro_atomic_load_acquire_int(&c->dns->ready)
      ? NET_RTMP_CLIENT_READY : NET_RTMP_CLIENT_RESOLVING;
}
void net_rtmp_client_cancel(net_rtmp_client_t *c)
{
   assert(c);
}
bool net_rtmp_client_free(net_rtmp_client_t *c)
{
   if (++c->retire < 3)
      return false;
   close(c->socket[0]); close(c->socket[1]);
   test_dns_drop(c->dns);
   free(c);
   retro_atomic_fetch_add_int(&client_live, -1);
   return true;
}
record_session_mediacodec_t *record_session_mediacodec_new_stream(JNIEnv *env,
      const mediacodec_encoder_request_t *video, unsigned interval,
      unsigned rate, unsigned channels, unsigned bitrate,
      size_t max_frames, size_t capacity, net_rtmp_client_t *client,
      const record_mediacodec_stream_config_t *config)
{
   assert(client && config->codec == RECORD_MC_STREAM_HEVC);
   return record_session_mediacodec_new(env, video, interval, rate,
         channels, bitrate, max_frames, capacity, "record.mp4");
}
int record_session_mediacodec_network_step(record_session_mediacodec_t *s,
      uint32_t time, net_rtmp_client_view_t *wait)
{
   owner(s); (void)time;
   memset(wait, 0, sizeof(*wait)); wait->fd = -1;
   return 0;
}
static record_dispatch_mediacodec_t *start_stream(void)
{
   mediacodec_encoder_request_t v = {0};
   record_dispatch_mediacodec_stream_config_t config = {0};
   record_dispatch_mediacodec_t *j;
   char mime[] = "video/hevc", url[] = "rtmp://localhost/live/test";
   v.mime = mime; v.width = 320; v.height = 192;
   config.mux.codec = RECORD_MC_STREAM_HEVC;
   config.mux.bit_depth = 8; config.mux.max_packet_size = 1024;
   j = record_dispatch_mediacodec_new_stream(&v, 2, 48000, 2, 128000,
         1024, 65536, url, &config);
   memset(mime, 0, sizeof(mime)); memset(url, 0, sizeof(url));
   return j;
}
int main(void)
{
   record_dispatch_mediacodec_t *j;
   unsigned mode, at;
   assert(file_dispatch_acceptance() == 0);
   retro_atomic_store_release_int(&fault, 0);
   retro_atomic_store_release_int(&env_fault, 0);
   retro_atomic_store_release_int(&startup_fault, 0);
   retro_atomic_store_release_int(&output_fault, 0);
   retro_atomic_int_init(&test_clock, 1000000);
   retro_atomic_int_init(&dns_hold, 0); retro_atomic_int_init(&dns_live, 0);
   retro_atomic_int_init(&client_live, 0);
   for (mode = 0; mode < 3; mode++)
   {
      retro_atomic_store_release_int(&dns_hold, mode != 0);
      j = start_stream(); assert(j);
      for (at = 0; at < 10000 && !retro_atomic_load_acquire_int(&client_live); at++) pause_test();
      assert(at < 10000);
      if (!mode)
         wait_state(j, RECORD_MC_DISPATCH_READY);
      else
      {
         assert(record_dispatch_mediacodec_status(j, NULL, NULL, NULL) == RECORD_MC_DISPATCH_STARTING);
         if (mode == 1) record_dispatch_mediacodec_cancel(j);
         else retro_atomic_store_release_int(&test_clock, 32000000);
         wait_state(j, RECORD_MC_DISPATCH_DRAINING);
      }
      assert(record_dispatch_mediacodec_audio_finish(j, 0));
      assert(record_dispatch_mediacodec_gpu_retired(j, mode ? 0 : 100000));
      assert(record_dispatch_mediacodec_gpu_released(j));
      wait_state(j, mode ? RECORD_MC_DISPATCH_FAILED : RECORD_MC_DISPATCH_DONE);
      collect(j); assert(!retro_atomic_load_acquire_int(&client_live));
      retro_atomic_store_release_int(&dns_hold, 0);
      for (at = 0; at < 10000 && retro_atomic_load_acquire_int(&dns_live); at++) pause_test();
      assert(at < 10000);
      retro_atomic_store_release_int(&test_clock, 1000000);
   }
   puts("PASS: network dispatcher thread, owned URL/config, actual resolver thread and independent wake lifetime, normal closure, cancellation and setup deadline; client/session mocked");
   return 0;
}
#else
int main(void) { return file_dispatch_acceptance(); }
#endif
