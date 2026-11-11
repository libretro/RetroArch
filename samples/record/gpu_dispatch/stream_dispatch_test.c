/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <net/net_socket.h>
#include <net/net_rtmp_client.h>
#include <crypto/kdf.h>
#define main file_dispatch_main
#include "dispatch_test.c"
#undef main

struct net_rtmp_client { bool cancelled; };
static HANDLE resolve_entered, resolve_release, resolve_done;
static retro_atomic_int_t resolved, client_frees;
static void resolve_work(void *opaque)
{
   (void)opaque;
   CHECK(SetEvent(resolve_entered));
   CHECK(WaitForSingleObject(resolve_release, 10000) == WAIT_OBJECT_0);
   retro_atomic_store_release_int(&resolved, 1);
   CHECK(SetEvent(resolve_done));
}
bool network_init(void)
{
   WSADATA data;
   return WSAStartup(MAKEWORD(2, 2), &data) == 0;
}
int crypto_random_bytes(uint8_t *p, size_t n)
{
   memset(p, 42, n);
   return 0;
}
net_rtmp_client_t *net_rtmp_client_new_caps(const net_rtmp_client_config_t *c,
      const net_rtmp_codec_caps_t *caps)
{
   net_rtmp_client_t *client = (net_rtmp_client_t*)calloc(1, sizeof(*client));
   CHECK(GetCurrentThreadId() != controller);
   CHECK(c->random && c->max_reply == 65536);
   CHECK(caps && caps->video_count == 1 && caps->audio_count == 1);
   CHECK(!strcmp(caps->video[0].fourcc, "avc1") && caps->video[0].flags == NET_RTMP_CODEC_ENCODE);
   CHECK(!strcmp(caps->audio[0].fourcc, "mp4a") && caps->audio[0].flags == NET_RTMP_CODEC_ENCODE);
   CHECK(c->dispatch(resolve_work, NULL, c->userdata));
   return client;
}
enum net_rtmp_client_state net_rtmp_client_step(net_rtmp_client_t *c,
      uint32_t time, net_rtmp_client_view_t *view)
{
   (void)time;
   memset(view, 0, sizeof(*view));
   view->fd = -1;
   return c->cancelled ? NET_RTMP_CLIENT_FAILED :
      retro_atomic_load_acquire_int(&resolved) ? NET_RTMP_CLIENT_READY : NET_RTMP_CLIENT_RESOLVING;
}
void net_rtmp_client_cancel(net_rtmp_client_t *c) { c->cancelled = true; }
bool net_rtmp_client_free(net_rtmp_client_t *c)
{
   CHECK(c->cancelled);
   free(c);
   retro_atomic_fetch_add_int(&client_frees, 1);
   return true;
}
bool record_gpu_session_d3d12_network(const record_gpu_session_d3d12_t *s,
      int *fd, bool *read, bool *write)
{
   (void)s;
   *fd = -1; *read = *write = false;
   return true;
}
static void stream_start(bool cancel_lookup)
{
   nvenc_d3d12_config_t e;
   record_gpu_output_config_t o;
   record_gpu_capture_d3d12_t *c = (record_gpu_capture_d3d12_t*)calloc(1, sizeof(*c));
   record_gpu_dispatch_d3d12_t *j;
   int16_t pcm[2] = {0, 0};
   setup(c, &e, &o);
   resolve_entered = CreateEventA(NULL, TRUE, FALSE, NULL);
   resolve_release = CreateEventA(NULL, TRUE, FALSE, NULL);
   resolve_done = CreateEventA(NULL, TRUE, FALSE, NULL);
   CHECK(resolve_entered && resolve_release && resolve_done);
   retro_atomic_int_init(&resolved, 0);
   retro_atomic_int_init(&client_frees, 0);
   j = record_gpu_dispatch_d3d12_new_stream_owned(c, &e, &o, "owned.mp4", false);
   CHECK(j);
   CHECK(WaitForSingleObject(resolve_entered, 10000) == WAIT_OBJECT_0);
   CHECK(record_gpu_dispatch_d3d12_status(j, NULL, NULL, NULL) == RECORD_GPU_DISPATCH_STARTING);
   CHECK(record_gpu_dispatch_d3d12_audio(j, pcm, 1) == RECORD_GPU_OUTPUT_BUSY);
   if (!cancel_lookup)
   {
      CHECK(SetEvent(resolve_release));
      ready(j);
      CHECK(record_gpu_dispatch_d3d12_status(j, NULL, NULL, NULL) == RECORD_GPU_DISPATCH_READY);
   }
   else
      record_gpu_dispatch_d3d12_cancel(j);
   CHECK(record_gpu_dispatch_d3d12_video_finish(j));
   CHECK(record_gpu_dispatch_d3d12_audio_finish(j));
   collect(j, cancel_lookup ? RECORD_GPU_DISPATCH_FAILED : RECORD_GPU_DISPATCH_DONE);
   CHECK(retro_atomic_load_acquire_int(&client_frees) == 1);
   /* The resolver resumes after the canceled job and its wake handle retire. */
   CHECK(SetEvent(resolve_release));
   CHECK(WaitForSingleObject(resolve_done, 10000) == WAIT_OBJECT_0);
   CloseHandle(resolve_entered); CloseHandle(resolve_release); CloseHandle(resolve_done);
   cleanup();
}
static void deadlines(void)
{
   record_gpu_dispatch_d3d12_t j;
   memset(&j, 0, sizeof(j));
   j.streaming = j.bootstrapping = true;
   CHECK(record_gpu_dispatch_deadline(&j, 0xfffffff0UL) == 30000);
   CHECK(record_gpu_dispatch_deadline(&j, 0x10) == 29968);
   CHECK(record_gpu_dispatch_deadline(&j, 0x7520) == 0);
   CHECK(retro_atomic_load_acquire_int(&j.cancelled));
   CHECK(retro_atomic_load_acquire_int(&j.system_error) == ERROR_TIMEOUT);
   CHECK(record_gpu_dispatch_deadline(&j, 0x7521) == INFINITE);
   memset(&j, 0, sizeof(j));
   j.streaming = true;
   CHECK(record_gpu_dispatch_deadline(&j, 100) == INFINITE);
   j.session = (record_gpu_session_d3d12_t*)&j;
   retro_atomic_store_release_int(&j.video_closed, 1);
   retro_atomic_store_release_int(&j.audio_closed, 1);
   CHECK(record_gpu_dispatch_deadline(&j, 100) == 10000);
   CHECK(record_gpu_dispatch_deadline(&j, 10100) == 0);
}
static void socket_readiness(void)
{
   struct sockaddr_in address;
   int length = sizeof(address);
   SOCKET listener, peer, channel;
   HANDLE event;
   char byte;
   record_gpu_dispatch_d3d12_t *j;
   CHECK(network_init());
   listener = socket(AF_INET, SOCK_STREAM, 0);
   CHECK(listener != INVALID_SOCKET);
   memset(&address, 0, sizeof(address));
   address.sin_family = AF_INET;
   address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
   CHECK(!bind(listener, (struct sockaddr*)&address, sizeof(address)));
   CHECK(!getsockname(listener, (struct sockaddr*)&address, &length));
   CHECK(!listen(listener, 1));
   peer = socket(AF_INET, SOCK_STREAM, 0);
   CHECK(peer != INVALID_SOCKET);
   CHECK(!connect(peer, (struct sockaddr*)&address, sizeof(address)));
   channel = accept(listener, NULL, NULL);
   CHECK(channel != INVALID_SOCKET);
   closesocket(listener);
   j = (record_gpu_dispatch_d3d12_t*)calloc(1, sizeof(*j));
   CHECK(j);
   j->streaming = j->bootstrapping = true;
   j->network_event = WSACreateEvent();
   j->network_fd = -1;
   j->connect_view.fd = (int)channel;
   j->connect_view.read = true;
   CHECK(record_gpu_dispatch_network(j, &event) == 0 && event);
   CHECK(send(peer, "x", 1, 0) == 1);
   CHECK(WaitForSingleObject(event, 10000) == WAIT_OBJECT_0);
   CHECK(record_gpu_dispatch_network(j, &event) == 1);
   CHECK(recv(channel, &byte, 1, 0) == 1 && byte == 'x');
   CHECK(record_gpu_dispatch_network(j, &event) == 0);
   closesocket(peer);
   CHECK(WaitForSingleObject(event, 10000) == WAIT_OBJECT_0);
   CHECK(record_gpu_dispatch_network(j, &event) == 1);
   closesocket(channel);
   j->connect_view.fd = -1;
   CHECK(record_gpu_dispatch_network(j, &event) == 0 && !event);
   CHECK(WaitForSingleObject(j->network_event, 0) == WAIT_TIMEOUT);
   WSACloseEvent(j->network_event);
   free(j);
}
int main(void)
{
   deadlines();
   socket_readiness();
   stream_start(false);
   stream_start(true);
   puts("PASS: streaming startup admission, resolver wake/lifetime and bounded setup/drain");
   return 0;
}
