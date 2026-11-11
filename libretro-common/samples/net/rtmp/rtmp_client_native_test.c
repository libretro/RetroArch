/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: MIT */
#include <net/net_rtmp_client.h>
static net_rtmp_client_t *active_client;
static int client_transport_step(net_rtmp_socket_t *transport, uint32_t time);
#define main original_socket_main
#define net_rtmp_socket_step client_transport_step
#include "rtmp_socket_native_test.c"
#undef net_rtmp_socket_step
#undef main

static int client_transport_step(net_rtmp_socket_t *transport, uint32_t time)
{
   net_rtmp_client_view_t view;
   enum net_rtmp_client_state state = net_rtmp_client_step(active_client, time, &view);
   (void)transport;
   return state == NET_RTMP_CLIENT_FAILED ? NET_RTMP_SOCKET_ERROR
      : view.again ? NET_RTMP_SOCKET_PROGRESS : NET_RTMP_SOCKET_IDLE;
}

int main(int argc, char **argv)
{
   net_rtmp_client_config_t config;
   net_rtmp_client_view_t view;
   net_rtmp_codec_capability_t video = {NET_RTMP_CODEC_ENCODE, "avc1"};
   net_rtmp_codec_capability_t audio = {NET_RTMP_CODEC_ENCODE, "mp4a"};
   net_rtmp_codec_caps_t caps;
   enum net_rtmp_client_state state = NET_RTMP_CLIENT_RESOLVING, previous_state;
   sthread_t *resolver_thread = NULL;
   retro_time_t deadline;
   char *url;
   size_t length;
   int fd;
   unsigned i;
#ifdef HAVE_SSL
   const char *ca = getenv("RTMPS_CA");
   char *pem = NULL;
   int64_t pem_size = 0;
   assert(ca && filestream_read_file(ca, (void**)&pem, &pem_size));
   assert(pem && pem_size > 0);
   ssl_socket_retro_set_trust_pem(pem, (size_t)pem_size);
#endif
   assert((argc == 5 || argc == 6) && network_init());
   length = strlen(argv[1]);
   url = (char*)malloc(length + 6);
   assert(url);
   memcpy(url, argv[1], length);
   memcpy(url + length, "/test", 6);
   memset(&config, 0, sizeof(config));
   for (i = 0; i < sizeof(random_bytes); i++)
      random_bytes[i] = (uint8_t)(i * 13 + 1);
   config.url = url;
   config.random = random_bytes;
   config.dispatch = resolve_dispatch;
   config.userdata = &resolver_thread;
   config.max_reply = 65536;
   config.streams = 8;
   config.legacy_commands = !strcmp(argv[3], "legacy");
   caps.video = &video;
   caps.audio = &audio;
   caps.video_count = caps.audio_count = 1;
   if (getenv("RTMP_TEST_AV1"))
      memcpy(video.fourcc, "av01", 4);
   active_client = getenv("RTMP_TEST_CAPS") ? net_rtmp_client_new_caps(&config, &caps)
      : net_rtmp_client_new(&config);
   memset(&video, 0, sizeof(video));
   memset(&audio, 0, sizeof(audio));
   memset(&caps, 0, sizeof(caps));
   free(url);
   assert(active_client);
   deadline = cpu_features_get_time_usec() + 10000000;
   do
   {
      assert(cpu_features_get_time_usec() < deadline);
      previous_state = state;
      state = net_rtmp_client_step(active_client, 0, &view);
      if (state == NET_RTMP_CLIENT_FAILED)
      {
         fprintf(stderr, "Client setup failed in phase %d\n", (int)previous_state);
         assert(previous_state == NET_RTMP_CLIENT_RESOLVING
               || previous_state == NET_RTMP_CLIENT_CONNECTING);
         assert(net_rtmp_client_free(active_client));
         sthread_join(resolver_thread);
#ifdef HAVE_SSL
         ssl_socket_retro_set_trust_pem(NULL, 0);
         free(pem);
#endif
         return 77;
      }
      if (!view.again)
      {
         if (view.fd >= 0)
            socket_wait(view.fd, &view.read, &view.write, 10);
         else
            retro_sleep(1);
      }
   } while (state != NET_RTMP_CLIENT_READY);
#ifdef HAVE_SSL
   ssl_socket_retro_set_trust_pem(NULL, 0);
   free(pem);
#endif
   assert(view.session && view.fd >= 0);
   fd = view.fd;
   assert(net_rtmp_session_chunk_size(view.session, 4096));
   send_media(view.session, NULL, fd, argv[4], argc == 6);
   assert(net_rtmp_session_finish(view.session));
   deadline = cpu_features_get_time_usec() + 10000000;
   do
   {
      assert(cpu_features_get_time_usec() < deadline);
      state = net_rtmp_client_step(active_client, 0, &view);
      assert(state != NET_RTMP_CLIENT_FAILED);
      if (!view.again && view.fd >= 0)
         socket_wait(view.fd, &view.read, &view.write, 10);
   } while (state != NET_RTMP_CLIENT_FINISHED);
   assert(net_rtmp_session_status(view.session, NULL, NULL) == NET_RTMP_SESSION_FINISHED);
   assert(net_rtmp_client_free(active_client));
   sthread_join(resolver_thread);
   puts("PASS owned URL/resolver/TCP/TLS RTMP client and normal finish");
   return 0;
}
