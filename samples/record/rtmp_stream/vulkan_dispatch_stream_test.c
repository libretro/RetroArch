/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include "../../../record/record_gpu_dispatch_vulkan.c"

struct net_rtmp_client { bool cancelled; };
struct record_gpu_session_vulkan
{
   struct net_rtmp_client *client;
   retro_atomic_int_t audio_closed, video_closed, cancelled;
};
static DWORD caller;
static HANDLE resolve_gate, resolve_done;
static retro_atomic_int_t resolved, client_created, client_freed, starts, stopped;
static bool fail_start, fail_connect, fail_random, expected_legacy;
static unsigned expected_codec, expected_audio;
static vulkan_encode_pool_t pool;
static record_gpu_queue_t *queue;
static SOCKET network_socket = INVALID_SOCKET, network_peer = INVALID_SOCKET;
static void not_caller(void) { assert(GetCurrentThreadId() != caller); }
static void resolve_work(void *opaque)
{
   assert(opaque == &resolved);
   assert(WaitForSingleObject(resolve_gate, 10000) == WAIT_OBJECT_0);
   retro_atomic_store_release_int(&resolved, 1);
   assert(SetEvent(resolve_done));
}
bool network_init(void) { return true; }
int crypto_random_bytes(uint8_t *data, size_t size)
{
   not_caller();
   assert(size == 1528);
   memset(data, 7, size);
   return fail_random ? -1 : 0;
}
net_rtmp_client_t *net_rtmp_client_new_caps(const net_rtmp_client_config_t *c,
      const net_rtmp_codec_caps_t *caps)
{
   struct net_rtmp_client *client;
   not_caller();
   assert(c->legacy_commands == expected_legacy && c->max_reply == 65536);
   assert(c->random[0] == 7 && c->streams == 8);
   assert(!strcmp(c->url, "rtmps://localhost/live/test"));
   assert(caps->video_count == 1 && caps->audio_count == expected_audio);
   assert(!strcmp(caps->video[0].fourcc, expected_codec == RECORD_MP4_HEVC ? "hvc1" : "avc1"));
   if (expected_audio) assert(!strcmp(caps->audio[0].fourcc, "mp4a"));
   client = (struct net_rtmp_client*)calloc(1, sizeof(*client));
   assert(client && c->dispatch(resolve_work, &resolved, c->userdata));
   retro_atomic_store_release_int(&client_created, 1);
   return client;
}
enum net_rtmp_client_state net_rtmp_client_step(net_rtmp_client_t *client,
      uint32_t time, net_rtmp_client_view_t *view)
{
   not_caller();
   (void)client; (void)time;
   memset(view, 0, sizeof(*view)); view->fd = -1;
   if (!retro_atomic_load_acquire_int(&resolved)) return NET_RTMP_CLIENT_RESOLVING;
   return fail_connect ? NET_RTMP_CLIENT_FAILED : NET_RTMP_CLIENT_READY;
}
void net_rtmp_client_cancel(net_rtmp_client_t *client)
{ not_caller(); client->cancelled = true; }
bool net_rtmp_client_free(net_rtmp_client_t *client)
{
   not_caller(); assert(client->cancelled);
   free(client); retro_atomic_store_release_int(&client_freed, 1); return true;
}
record_gpu_session_vulkan_t *record_gpu_session_vulkan_new(
      const vulkan_encode_h264_open_t *e, const vulkan_encode_h264_ops_t *ops,
      const record_gpu_output_vulkan_config_t *output, const char *path)
{
   record_gpu_session_vulkan_t *s;
   (void)e; (void)ops; (void)path;
   not_caller(); assert(output->stream_client);
   retro_atomic_fetch_add_seq_cst_int(&starts, 1);
   if (fail_start) return NULL;
   s = (record_gpu_session_vulkan_t*)calloc(1, sizeof(*s)); assert(s);
   s->client = output->stream_client;
   retro_atomic_int_init(&s->video_closed, 0);
   retro_atomic_int_init(&s->audio_closed, output->audio_rate ? 0 : 1);
   retro_atomic_int_init(&s->cancelled, 0);
   return s;
}
enum record_gpu_session_vulkan_error record_gpu_session_vulkan_error(
      const record_gpu_session_vulkan_t *s, int *detail)
{
   if (detail) *detail = 0;
   return s ? RECORD_VULKAN_SESSION_NONE : RECORD_VULKAN_SESSION_STARTUP;
}
const vulkan_encode_pool_t *record_gpu_session_vulkan_pool(const record_gpu_session_vulkan_t *s)
{ assert(s); return &pool; }
record_gpu_queue_t *record_gpu_session_vulkan_queue(const record_gpu_session_vulkan_t *s)
{ assert(s); return queue; }
int record_gpu_session_vulkan_audio(record_gpu_session_vulkan_t *s, const int16_t *pcm, size_t frames)
{ assert(s && pcm && frames); return RECORD_VULKAN_OUTPUT_PROGRESS; }
bool record_gpu_session_vulkan_audio_finish(record_gpu_session_vulkan_t *s)
{ retro_atomic_store_release_int(&s->audio_closed, 1); return true; }
bool record_gpu_session_vulkan_video_finish(record_gpu_session_vulkan_t *s)
{ retro_atomic_store_release_int(&s->video_closed, 1); return true; }
void record_gpu_session_vulkan_cancel(record_gpu_session_vulkan_t *s)
{ retro_atomic_store_release_int(&s->cancelled, 1); }
int record_gpu_session_vulkan_step(record_gpu_session_vulkan_t *s)
{
   char byte;
   not_caller(); assert(!s->client->cancelled);
   if (network_socket != INVALID_SOCKET && !retro_atomic_load_acquire_int(&stopped)
         && !retro_atomic_load_acquire_int(&s->cancelled))
   {
      if (recv(network_socket, &byte, 1, 0) != 1) return RECORD_VULKAN_OUTPUT_BUSY;
      retro_atomic_store_release_int(&stopped, 1);
   }
   if (!retro_atomic_load_acquire_int(&s->audio_closed)
         || !retro_atomic_load_acquire_int(&s->video_closed)) return RECORD_VULKAN_OUTPUT_BUSY;
   return retro_atomic_load_acquire_int(&s->cancelled) ? RECORD_VULKAN_OUTPUT_ERROR : RECORD_VULKAN_OUTPUT_END;
}
bool record_gpu_session_vulkan_retry_needed(const record_gpu_session_vulkan_t *s)
{ (void)s; return false; }
bool record_gpu_session_vulkan_free(record_gpu_session_vulkan_t *s)
{ not_caller(); assert(!s->client->cancelled); free(s); return true; }
uint64_t record_gpu_session_vulkan_audio_frames(const record_gpu_session_vulkan_t *s)
{ assert(s); return 735; }
bool record_gpu_session_vulkan_network(const record_gpu_session_vulkan_t *s,
      int *fd, bool *read, bool *write)
{
   assert(s); *write = false;
   *read = network_socket != INVALID_SOCKET && !retro_atomic_load_acquire_int(&stopped);
   *fd = *read ? (int)network_socket : -1; return *read;
}
vulkan_encode_capture_t *vulkan_encode_capture_new_compiled(const vulkan_encode_pool_t *p,
      const vulkan_encode_capture_ops_t *ops, char *error, size_t capacity)
{ (void)p; (void)ops; (void)error; (void)capacity; assert(0); return NULL; }
void vulkan_encode_capture_free(vulkan_encode_capture_t *capture) { assert(!capture); }
static void await_flag(retro_atomic_int_t *flag)
{
   unsigned n;
   for (n = 0; !retro_atomic_load_acquire_int(flag); n++) { assert(n < 10000); Sleep(1); }
}
static void retire(record_gpu_dispatch_vulkan_t *j)
{
   unsigned n;
   for (n = 0; !record_gpu_dispatch_vulkan_free(j); n++) { assert(n < 10000); Sleep(1); }
}
static void run_case(unsigned mode)
{
   record_gpu_dispatch_vulkan_config_t config;
   record_gpu_output_vulkan_config_t output;
   record_gpu_dispatch_vulkan_t *j;
   enum record_gpu_dispatch_vulkan_state state;
   enum record_gpu_session_vulkan_error error;
   unsigned n;
   uint64_t frames;
   memset(&config, 0, sizeof(config)); memset(&output, 0, sizeof(output));
   expected_codec = mode & 1 ? RECORD_MP4_HEVC : RECORD_MP4_H264;
   expected_audio = mode & 2 ? 1 : 0; expected_legacy = mode & 1 ? true : false;
   output.video.codec = expected_codec; output.audio_rate = expected_audio ? 44100 : 0;
   fail_random = mode == 6; fail_connect = mode == 5; fail_start = mode == 4;
   retro_atomic_int_init(&resolved, 0); retro_atomic_int_init(&client_created, 0);
   retro_atomic_int_init(&client_freed, 0); retro_atomic_int_init(&starts, 0);
   retro_atomic_int_init(&stopped, 0);
   assert(ResetEvent(resolve_gate) && ResetEvent(resolve_done));
   j = record_gpu_dispatch_vulkan_new_backend_stream(&config, record_vkd_h264_start,
         &output, "rtmps://localhost/live/test", NULL, expected_legacy);
   assert(j && !record_gpu_dispatch_vulkan_free(j));
   if (mode != 6)
   {
      await_flag(&client_created);
      assert(!retro_atomic_load_acquire_int(&starts));
      assert(record_gpu_dispatch_vulkan_status(j, NULL, NULL) == RECORD_VKD_STARTING);
   }
   if (mode == 3)
   {
      record_gpu_dispatch_vulkan_cancel(j);
      assert(record_gpu_dispatch_vulkan_video_finish(j) && record_gpu_dispatch_vulkan_audio_finish(j));
      retire(j); assert(!retro_atomic_load_acquire_int(&resolved));
      assert(retro_atomic_load_acquire_int(&client_freed));
      assert(SetEvent(resolve_gate));
      assert(WaitForSingleObject(resolve_done, 10000) == WAIT_OBJECT_0);
      return;
   }
   assert(SetEvent(resolve_gate));
   for (n = 0; ; n++)
   {
      state = record_gpu_dispatch_vulkan_status(j, &error, NULL);
      if (state != RECORD_VKD_STARTING) break;
      assert(n < 10000); Sleep(1);
   }
   if (mode < 3 || mode == 7 || mode == 8)
   {
      assert(state == RECORD_VKD_READY && !error);
      assert(record_gpu_dispatch_vulkan_pool(j) == &pool);
   }
   else assert(state == RECORD_VKD_DRAINING && error);
   if (mode == 8)
   {
      record_gpu_dispatch_vulkan_cancel(j);
      Sleep(5);
      assert(!retro_atomic_load_acquire_int(&client_freed));
      assert(!record_gpu_dispatch_vulkan_free(j));
   }
   assert(record_gpu_dispatch_vulkan_video_finish(j) && record_gpu_dispatch_vulkan_audio_finish(j));
   if (mode == 7)
   {
      Sleep(5);
      assert(!record_gpu_dispatch_vulkan_audio_frames(j, &frames));
      assert(send(network_peer, "x", 1, 0) == 1);
   }
   for (n = 0; !record_gpu_dispatch_vulkan_audio_frames(j, &frames); n++)
   { assert(n < 10000); Sleep(1); }
   assert(frames == (mode < 3 || mode == 7 || mode == 8 ? 735u : 0u));
   retire(j);
   assert(retro_atomic_load_acquire_int(&client_freed) == (mode == 6 ? 0 : 1));
}
int main(void)
{
   WSADATA data;
   unsigned mode;
   caller = GetCurrentThreadId(); assert(!WSAStartup(MAKEWORD(2,2), &data));
   resolve_gate = CreateEvent(NULL, TRUE, FALSE, NULL);
   resolve_done = CreateEvent(NULL, TRUE, FALSE, NULL); assert(resolve_gate && resolve_done);
   queue = record_gpu_queue_new(2); assert(queue);
   for (mode = 0; mode < 7; mode++) run_case(mode);
   run_case(8);
   {
      SOCKET listener;
      struct sockaddr_in address;
      int length = sizeof(address);
      u_long nonblocking = 1;
      listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); assert(listener != INVALID_SOCKET);
      memset(&address, 0, sizeof(address)); address.sin_family = AF_INET;
      address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
      assert(!bind(listener, (struct sockaddr*)&address, sizeof(address)));
      assert(!getsockname(listener, (struct sockaddr*)&address, &length) && !listen(listener, 1));
      network_peer = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); assert(network_peer != INVALID_SOCKET);
      assert(!connect(network_peer, (struct sockaddr*)&address, sizeof(address)));
      network_socket = accept(listener, NULL, NULL); assert(network_socket != INVALID_SOCKET);
      assert(!ioctlsocket(network_socket, FIONBIO, &nonblocking)); closesocket(listener);
      run_case(7);
      assert(retro_atomic_load_acquire_int(&stopped));
      closesocket(network_socket); closesocket(network_peer); network_socket = INVALID_SOCKET;
   }
   {
      record_gpu_dispatch_vulkan_t *j = (record_gpu_dispatch_vulkan_t*)calloc(1, sizeof(*j));
      assert(j); j->streaming = j->bootstrapping = true;
      retro_atomic_int_init(&j->cancelled, 0); retro_atomic_int_init(&j->error, 0);
      retro_atomic_int_init(&j->file_error, 0); retro_atomic_int_init(&j->state, RECORD_VKD_STARTING);
      assert(record_vkd_deadline(j, 0xfffffff0u) == 30000);
      assert(record_vkd_deadline(j, 0x10u) == 29968);
      assert(!record_vkd_deadline(j, 30000));
      assert(retro_atomic_load_acquire_int(&j->cancelled));
      assert(retro_atomic_load_acquire_int(&j->file_error) == ERROR_TIMEOUT);
      retro_atomic_store_release_int(&j->cancelled, 0);
      j->bootstrapping = false; j->network_phase = 0;
      j->session = (record_gpu_session_vulkan_t*)(uintptr_t)1;
      retro_atomic_int_init(&j->video_closed, 0); retro_atomic_int_init(&j->audio_closed, 0);
      assert(record_vkd_deadline(j, 100) == INFINITE);
      assert(record_vkd_deadline(j, 1000000) == INFINITE);
      retro_atomic_store_release_int(&j->video_closed, 1);
      retro_atomic_store_release_int(&j->audio_closed, 1);
      assert(record_vkd_deadline(j, 1000000) == 10000);
      assert(!record_vkd_deadline(j, 1010000));
      free(j);
   }
   assert(record_gpu_queue_free(queue));
   assert(CloseHandle(resolve_gate) && CloseHandle(resolve_done)); WSACleanup();
   puts("PASS Vulkan stream worker capabilities, async resolver, cancellation lifetime and startup failure");
   return 0;
}
