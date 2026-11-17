/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif
#include "record_gpu_dispatch_vulkan_backend.h"
#include <retro_atomic.h>
#include <stdlib.h>
#include <string.h>
#if defined(HAVE_THREADS) && defined(RETRO_ATOMIC_LOCK_FREE) \
      && (defined(__linux__) || (defined(_WIN32) && !defined(__WINRT__) && !defined(_XBOX)))
#include <rthreads/retro_eventcount.h>
#ifdef _WIN32
#if defined(HAVE_MF_AUDIO) && defined(HAVE_RTMP) && defined(HAVE_RFLV) && defined(HAVE_CRYPTO)
#define RECORD_VKD_STREAM
#include <net/net_rtmp_client.h>
#include <net/net_socket.h>
#include <crypto/kdf.h>
#endif
#include <windows.h>
#else
#include <pthread.h>
#endif
struct record_gpu_dispatch_vulkan
{
   record_gpu_dispatch_vulkan_config_t config;
   record_gpu_dispatch_vulkan_start_t start;
   vulkan_encode_capture_ops_t capture_ops;
   record_gpu_output_vulkan_config_t output;
   uint64_t last_end, last_ready, audio_frames;
   record_gpu_session_vulkan_t *session;
   record_gpu_queue_t *queue;
   const vulkan_encode_pool_t *pool;
   vulkan_encode_capture_t *capture;
   char *path;
   retro_eventcount_t wake;
   retro_atomic_int_t state, done, video_closed, audio_closed, cancelled, error, file_error;
   bool capture_enabled;
#ifdef RECORD_VKD_STREAM
   net_rtmp_client_t *client;
   net_rtmp_client_view_t connect_view;
   WSAEVENT network_event;
   HANDLE stream_wake;
   long network_mask;
   int network_fd;
   DWORD network_started;
   unsigned network_phase;
   uint8_t random[1528];
   retro_atomic_int_t epoch;
   bool streaming, legacy, bootstrapping;
#endif
#ifdef _WIN32
   HANDLE thread;
#endif
};
static void record_vkd_error(record_gpu_dispatch_vulkan_t *j,
      enum record_gpu_session_vulkan_error error, int file_error)
{
   retro_atomic_cas_int(&j->error, 0, (int)error);
   retro_atomic_cas_int(&j->file_error, 0, file_error);
   retro_atomic_store_release_int(&j->state, RECORD_VKD_DRAINING);
}
static enum record_gpu_session_vulkan_error record_vkd_start(record_gpu_dispatch_vulkan_t *j)
{
   enum record_gpu_session_vulkan_error error;
   j->session = j->start(&j->config, &j->output, j->path);
   error = record_gpu_session_vulkan_error(j->session, NULL);
   if (!error && j->capture_enabled
         && !retro_atomic_load_acquire_int(&j->cancelled)
         && !retro_atomic_load_acquire_int(&j->video_closed))
   {
      j->capture = vulkan_encode_capture_new_compiled(
            record_gpu_session_vulkan_pool(j->session), &j->capture_ops, NULL, 0);
      if (!j->capture)
      {
         error = RECORD_VULKAN_SESSION_STARTUP;
         retro_atomic_store_release_int(&j->cancelled, 1);
      }
   }
   if (!error)
   {
      j->pool = record_gpu_session_vulkan_pool(j->session);
      j->queue = record_gpu_session_vulkan_queue(j->session);
   }
   return error;
}
#ifdef RECORD_VKD_STREAM
static void record_vkd_os_error(record_gpu_dispatch_vulkan_t *j)
{
   DWORD error = GetLastError();
   record_vkd_error(j, RECORD_VULKAN_SESSION_OUTPUT,
         (int)(error ? error : ERROR_GEN_FAILURE));
   retro_atomic_store_release_int(&j->cancelled, 1);
}
/* Only setup and final drain have deadlines; live capture uses readiness. */
static DWORD record_vkd_deadline(record_gpu_dispatch_vulkan_t *j, DWORD now)
{
   unsigned phase = j->bootstrapping ? 1 :
      (j->session && retro_atomic_load_acquire_int(&j->video_closed)
       && retro_atomic_load_acquire_int(&j->audio_closed) ? 2 : 0);
   DWORD elapsed, limit = phase == 1 ? 30000 : 10000;
   if (!j->streaming || retro_atomic_load_acquire_int(&j->cancelled))
      phase = 0;
   if (phase != j->network_phase)
   {
      j->network_phase = phase;
      j->network_started = now;
   }
   if (!phase)
      return INFINITE;
   elapsed = now - j->network_started;
   if (elapsed < limit)
      return limit - elapsed;
   SetLastError(ERROR_TIMEOUT);
   record_vkd_os_error(j);
   return 0;
}

struct record_vkd_resolve
{
   void (*work)(void*);
   void *job;
   HANDLE wake;
};

static DWORD WINAPI record_vkd_resolve_run(void *opaque)
{
   struct record_vkd_resolve *r = (struct record_vkd_resolve*)opaque;
   r->work(r->job);
   SetEvent(r->wake);
   CloseHandle(r->wake);
   free(r);
   return 0;
}

static bool record_vkd_resolve(void (*work)(void*), void *job, void *userdata)
{
   record_gpu_dispatch_vulkan_t *j = (record_gpu_dispatch_vulkan_t*)userdata;
   struct record_vkd_resolve *r;
   HANDLE thread;
   r = (struct record_vkd_resolve*)calloc(1, sizeof(*r));
   if (!r)
      return false;
   r->work = work;
   r->job = job;
   /* The lookup may outlive its canceled recording job. */
   if (!DuplicateHandle(GetCurrentProcess(), j->stream_wake, GetCurrentProcess(),
            &r->wake, 0, FALSE, DUPLICATE_SAME_ACCESS))
   {
      free(r);
      return false;
   }
   thread = CreateThread(NULL, 0, record_vkd_resolve_run, r, 0, NULL);
   if (!thread)
   {
      CloseHandle(r->wake);
      free(r);
      return false;
   }
   if (!CloseHandle(thread))
      record_vkd_os_error(j);
   return true;
}

static int record_vkd_bootstrap(record_gpu_dispatch_vulkan_t *j)
{
   net_rtmp_client_config_t config;
   net_rtmp_codec_capability_t video = {NET_RTMP_CODEC_ENCODE, "avc1"};
   net_rtmp_codec_capability_t audio = {NET_RTMP_CODEC_ENCODE, "mp4a"};
   net_rtmp_codec_caps_t caps;
   enum net_rtmp_client_state state;
   enum record_gpu_session_vulkan_error error;
   if (retro_atomic_load_acquire_int(&j->cancelled)
         || (retro_atomic_load_acquire_int(&j->video_closed)
            && retro_atomic_load_acquire_int(&j->audio_closed)))
      goto failed;
   if (!j->client)
   {
      if (crypto_random_bytes(j->random, sizeof(j->random)) != 0)
         goto failed;
      config.url = j->path;
      config.random = j->random;
      config.dispatch = record_vkd_resolve;
      config.userdata = j;
      config.max_reply = 65536;
      config.time = 0;
      config.streams = 8;
      config.legacy_commands = j->legacy;
      if (j->output.video.codec == RECORD_MP4_HEVC)
         memcpy(video.fourcc, "hvc1", 5);
      else if (j->output.video.codec != RECORD_MP4_H264)
         goto failed;
      caps.video = &video;
      caps.audio = &audio;
      caps.video_count = 1;
      caps.audio_count = j->output.audio_rate ? 1 : 0;
      j->client = net_rtmp_client_new_caps(&config, &caps);
      if (!j->client)
         goto failed;
   }
   state = net_rtmp_client_step(j->client, 0, &j->connect_view);
   if (state == NET_RTMP_CLIENT_FAILED)
      goto failed;
   if (state != NET_RTMP_CLIENT_READY)
      return j->connect_view.again ? RECORD_VULKAN_OUTPUT_PROGRESS : RECORD_VULKAN_OUTPUT_BUSY;
   j->output.stream_client = j->client;
   error = record_vkd_start(j);
   j->bootstrapping = false;
   if (error)
      record_vkd_error(j, error, 0);
   else if (!retro_atomic_load_acquire_int(&j->cancelled))
      retro_atomic_store_release_int(&j->state, RECORD_VKD_READY);
   return RECORD_VULKAN_OUTPUT_PROGRESS;
failed:
   if (j->client)
      net_rtmp_client_cancel(j->client);
   j->bootstrapping = false;
   retro_atomic_cas_int(&j->error, 0, RECORD_VULKAN_SESSION_STARTUP);
   retro_atomic_store_release_int(&j->state, RECORD_VKD_DRAINING);
   return RECORD_VULKAN_OUTPUT_PROGRESS;
}

static int record_vkd_network(record_gpu_dispatch_vulkan_t *j, HANDLE *handle)
{
   WSANETWORKEVENTS events;
   int fd = -1;
   long mask;
   bool read = false, write = false;
   *handle = NULL;
   if (!j->streaming || !j->network_event
         || retro_atomic_load_acquire_int(&j->cancelled))
      return 0;
   if (j->bootstrapping)
   {
      fd = j->connect_view.fd;
      read = j->connect_view.read;
      write = j->connect_view.write;
   }
   else if (j->session)
      record_gpu_session_vulkan_network(j->session, &fd, &read, &write);
   if (fd < 0)
   {
      j->network_fd = -1;
      j->network_mask = 0;
      if (!ResetEvent(j->network_event))
         goto failed;
      return 0;
   }
   mask = FD_CLOSE | (read ? FD_READ : 0) | (write ? FD_WRITE : 0)
      | (j->bootstrapping ? FD_CONNECT : 0);
   if (fd != j->network_fd || mask != j->network_mask)
   {
      if (WSAEventSelect((SOCKET)fd, j->network_event, mask))
         goto socket_failed;
      j->network_fd = fd;
      j->network_mask = mask;
   }
   if (WSAEnumNetworkEvents((SOCKET)fd, j->network_event, &events))
      goto socket_failed;
   *handle = j->network_event;
   return events.lNetworkEvents ? 1 : 0;
socket_failed:
   SetLastError((DWORD)WSAGetLastError());
failed:
   record_vkd_os_error(j);
   return -1;
}
#endif

static int record_vkd_service(record_gpu_dispatch_vulkan_t *j)
{
   enum record_gpu_session_vulkan_error error;
   bool closed = retro_atomic_load_acquire_int(&j->video_closed)
      && retro_atomic_load_acquire_int(&j->audio_closed);
   int result, file_error;
#ifdef RECORD_VKD_STREAM
   if (j->bootstrapping)
      return record_vkd_bootstrap(j);
#endif
   if (!j->session)
   {
#ifdef RECORD_VKD_STREAM
      if (j->client)
      {
         net_rtmp_client_cancel(j->client);
         if (!net_rtmp_client_free(j->client))
            return RECORD_VULKAN_OUTPUT_BUSY;
         j->client = NULL;
         j->output.stream_client = NULL;
         j->connect_view.fd = -1;
      }
#endif
      return closed ? (retro_atomic_load_acquire_int(&j->error)
            ? RECORD_VULKAN_OUTPUT_ERROR : RECORD_VULKAN_OUTPUT_END)
         : RECORD_VULKAN_OUTPUT_BUSY;
   }
   if (retro_atomic_load_acquire_int(&j->cancelled))
      record_gpu_session_vulkan_cancel(j->session);
   if (retro_atomic_load_acquire_int(&j->video_closed))
   {
      /* The producer's closure proves every capture command has retired. */
      vulkan_encode_capture_free(j->capture);
      j->capture = NULL;
      record_gpu_session_vulkan_video_finish(j->session);
   }
   if (retro_atomic_load_acquire_int(&j->audio_closed))
      record_gpu_session_vulkan_audio_finish(j->session);
   if (closed)
      retro_atomic_store_release_int(&j->state, RECORD_VKD_DRAINING);
   result = record_gpu_session_vulkan_step(j->session);
   error = record_gpu_session_vulkan_error(j->session, &file_error);
   if (error)
      record_vkd_error(j, error, file_error);
   if (result == RECORD_VULKAN_OUTPUT_END || result == RECORD_VULKAN_OUTPUT_ERROR)
   {
      j->audio_frames = record_gpu_session_vulkan_audio_frames(j->session);
      if (!record_gpu_session_vulkan_free(j->session))
         return RECORD_VULKAN_OUTPUT_BUSY;
      j->session = NULL;
      return RECORD_VULKAN_OUTPUT_PROGRESS;
   }
   return result;
}
#ifdef RECORD_VKD_STREAM
static bool record_vkd_stream_run(record_gpu_dispatch_vulkan_t *j)
{
   int result;
   if (!network_init() || (j->network_event = WSACreateEvent()) == WSA_INVALID_EVENT)
      record_vkd_os_error(j);
   else
      j->bootstrapping = true;
   for (;;)
   {
      HANDLE handles[2], network;
      DWORD timeout, remaining, count = 1;
      int activity;
      record_vkd_deadline(j, GetTickCount());
      result = record_vkd_service(j);
      if (result == RECORD_VULKAN_OUTPUT_END || result == RECORD_VULKAN_OUTPUT_ERROR)
         break;
      if (result != RECORD_VULKAN_OUTPUT_BUSY)
         continue;
      if (!ResetEvent(j->stream_wake))
      {
         record_vkd_os_error(j);
         return false;
      }
      retro_atomic_fetch_or_int(&j->epoch, 1);
      retro_atomic_thread_fence_seq_cst();
      result = record_vkd_service(j);
      if (result != RECORD_VULKAN_OUTPUT_BUSY)
      {
         retro_atomic_fetch_and_int(&j->epoch, ~1);
         if (result == RECORD_VULKAN_OUTPUT_END || result == RECORD_VULKAN_OUTPUT_ERROR)
            break;
         continue;
      }
      handles[0] = j->stream_wake;
      timeout = j->session && record_gpu_session_vulkan_retry_needed(j->session) ? 1 : INFINITE;
      activity = record_vkd_network(j, &network);
      if (activity > 0)
      {
         retro_atomic_fetch_and_int(&j->epoch, ~1);
         continue;
      }
      if (activity < 0 || (j->client && !j->session && !j->bootstrapping))
         timeout = 1;
      else if (network)
         handles[count++] = network;
      remaining = record_vkd_deadline(j, GetTickCount());
      if (remaining < timeout)
         timeout = remaining;
      if (WaitForMultipleObjects(count, handles, FALSE, timeout) == WAIT_FAILED)
         record_vkd_os_error(j);
      retro_atomic_fetch_and_int(&j->epoch, ~1);
   }
   if (j->network_event)
   {
      if (!WSACloseEvent(j->network_event))
         record_vkd_os_error(j);
      j->network_event = NULL;
   }
   return true;
}
#endif
static void record_vkd_notify(record_gpu_dispatch_vulkan_t *j)
{
#ifdef RECORD_VKD_STREAM
   if (j->streaming)
   {
      if ((retro_atomic_fetch_add_seq_cst_int(&j->epoch, 2) & 1)
            && !SetEvent(j->stream_wake))
         record_vkd_os_error(j);
      return;
   }
#endif
   retro_eventcount_notify(&j->wake);
}

#ifdef _WIN32
static DWORD WINAPI record_vkd_run(void *opaque)
#else
static void *record_vkd_run(void *opaque)
#endif
{
   record_gpu_dispatch_vulkan_t *j = (record_gpu_dispatch_vulkan_t*)opaque;
   enum record_gpu_session_vulkan_error error;
   int result, key;
#ifdef RECORD_VKD_STREAM
   if (j->streaming)
   {
      if (!record_vkd_stream_run(j))
         return 0;
      goto finished;
   }
#endif
   if (retro_atomic_load_acquire_int(&j->cancelled))
      error = RECORD_VULKAN_SESSION_CANCELLED;
   else
   {
      error = record_vkd_start(j);
   }
   if (error)
      record_vkd_error(j, error, 0);
   else
   {
      retro_atomic_store_release_int(&j->state, RECORD_VKD_READY);
   }
   for (;;)
   {
      result = record_vkd_service(j);
      if (result == RECORD_VULKAN_OUTPUT_END || result == RECORD_VULKAN_OUTPUT_ERROR)
         break;
      if (result != RECORD_VULKAN_OUTPUT_BUSY)
         continue;
      key = retro_eventcount_prepare_wait(&j->wake);
      result = record_vkd_service(j);
      if (result != RECORD_VULKAN_OUTPUT_BUSY)
      {
         retro_eventcount_cancel_wait(&j->wake);
         if (result == RECORD_VULKAN_OUTPUT_END || result == RECORD_VULKAN_OUTPUT_ERROR)
            break;
         continue;
      }
      if (j->session && record_gpu_session_vulkan_retry_needed(j->session))
         retro_eventcount_commit_wait_timeout(&j->wake, key, 1000);
      else
         retro_eventcount_commit_wait(&j->wake, key);
   }
#ifdef RECORD_VKD_STREAM
finished:
#endif
   if (retro_atomic_load_acquire_int(&j->cancelled))
      retro_atomic_cas_int(&j->error, 0, RECORD_VULKAN_SESSION_CANCELLED);
   retro_eventcount_thread_cleanup();
   retro_atomic_store_release_int(&j->state,
         retro_atomic_load_acquire_int(&j->error) ? RECORD_VKD_FAILED : RECORD_VKD_DONE);
   retro_atomic_store_release_int(&j->done, 1);
   return 0;
}

record_gpu_dispatch_vulkan_t *record_gpu_dispatch_vulkan_new(
      const vulkan_encode_h264_open_t *encoder, const vulkan_encode_h264_ops_t *ops,
      const record_gpu_output_vulkan_config_t *output, const char *path)
{
   return record_gpu_dispatch_vulkan_new_capture(encoder, ops, output, path, NULL);
}

static record_gpu_session_vulkan_t *record_vkd_h264_start(
      const record_gpu_dispatch_vulkan_config_t *config,
      const record_gpu_output_vulkan_config_t *output, const char *path)
{
   return record_gpu_session_vulkan_new(&config->h264.encoder,
         &config->h264.ops, output, path);
}

record_gpu_dispatch_vulkan_t *record_gpu_dispatch_vulkan_new_capture(
      const vulkan_encode_h264_open_t *encoder, const vulkan_encode_h264_ops_t *ops,
      const record_gpu_output_vulkan_config_t *output, const char *path,
      const vulkan_encode_capture_ops_t *capture_ops)
{
   record_gpu_dispatch_vulkan_config_t config;
   if (!encoder || !ops)
      return NULL;
   config.h264.encoder = *encoder;
   config.h264.ops = *ops;
   return record_gpu_dispatch_vulkan_new_backend(&config, record_vkd_h264_start,
         output, path, capture_ops);
}

record_gpu_dispatch_vulkan_t *record_gpu_dispatch_vulkan_new_stream(
      const vulkan_encode_h264_open_t *encoder, const vulkan_encode_h264_ops_t *ops,
      const record_gpu_output_vulkan_config_t *output, const char *url,
      const vulkan_encode_capture_ops_t *capture_ops, bool legacy)
{
   record_gpu_dispatch_vulkan_config_t config;
   if (!encoder || !ops || !output || output->video.codec != RECORD_MP4_H264)
      return NULL;
   config.h264.encoder = *encoder;
   config.h264.ops = *ops;
   return record_gpu_dispatch_vulkan_new_backend_stream(&config, record_vkd_h264_start,
         output, url, capture_ops, legacy);
}

static record_gpu_dispatch_vulkan_t *record_vkd_new(
      const record_gpu_dispatch_vulkan_config_t *config,
      record_gpu_dispatch_vulkan_start_t start,
      const record_gpu_output_vulkan_config_t *output, const char *path,
      const vulkan_encode_capture_ops_t *capture_ops, bool streaming, bool legacy)
{
   record_gpu_dispatch_vulkan_t *j;
   size_t length;
#ifndef _WIN32
   pthread_attr_t attributes;
   pthread_t thread;
   int result;
#endif
#ifndef RECORD_VKD_STREAM
   (void)legacy;
   if (streaming)
      return NULL;
#endif
   if (!config || !start || !output || !path || !*path || output->stream_client
         || (streaming && output->video.codec != RECORD_MP4_H264
            && output->video.codec != RECORD_MP4_HEVC)
         || output->video.headers || output->video.headers_size
         || output->video.ac3_header || output->video.ac3_header_size)
      return NULL;
   length = strlen(path);
   if (length == (size_t)-1)
      return NULL;
   j = (record_gpu_dispatch_vulkan_t*)calloc(1, sizeof(*j));
   if (!j)
      return NULL;
   j->path = (char*)malloc(length + 1);
   if (!j->path)
      goto failed;
   memcpy(j->path, path, length + 1);
   j->config = *config;
   j->start = start;
   j->output = *output;
#ifdef RECORD_VKD_STREAM
   j->streaming = streaming;
   j->legacy = legacy;
   j->network_fd = j->connect_view.fd = -1;
   retro_atomic_int_init(&j->epoch, 0);
   if (streaming && !(j->stream_wake = CreateEvent(NULL, TRUE, FALSE, NULL)))
      goto failed;
#endif
   if (capture_ops)
   {
      j->capture_ops = *capture_ops;
      j->capture_enabled = true;
   }
   retro_atomic_int_init(&j->state, RECORD_VKD_STARTING);
   retro_atomic_int_init(&j->done, 0);
   retro_atomic_int_init(&j->video_closed, 0);
   retro_atomic_int_init(&j->audio_closed, output->audio_rate ? 0 : 1);
   retro_atomic_int_init(&j->cancelled, 0);
   retro_atomic_int_init(&j->error, 0);
   retro_atomic_int_init(&j->file_error, 0);
   /* A producer wake must never acquire the fallback mutex. */
   if (!retro_eventcount_init(&j->wake) || j->wake.cond)
      goto failed;
#ifdef _WIN32
   j->thread = CreateThread(NULL, 0, record_vkd_run, j, 0, NULL);
   if (!j->thread)
      goto failed;
#else
   if (pthread_attr_init(&attributes))
      goto failed;
   result = pthread_attr_setdetachstate(&attributes, PTHREAD_CREATE_DETACHED);
   if (!result)
      result = pthread_create(&thread, &attributes, record_vkd_run, j);
   pthread_attr_destroy(&attributes);
   if (result)
      goto failed;
#endif
   return j;
failed:
#ifdef RECORD_VKD_STREAM
   if (j->stream_wake)
      CloseHandle(j->stream_wake);
#endif
   retro_eventcount_free(&j->wake);
   free(j->path);
   free(j);
   return NULL;
}

record_gpu_dispatch_vulkan_t *record_gpu_dispatch_vulkan_new_backend(
      const record_gpu_dispatch_vulkan_config_t *config,
      record_gpu_dispatch_vulkan_start_t start,
      const record_gpu_output_vulkan_config_t *output, const char *path,
      const vulkan_encode_capture_ops_t *ops)
{
   return record_vkd_new(config, start, output, path, ops, false, false);
}
record_gpu_dispatch_vulkan_t *record_gpu_dispatch_vulkan_new_backend_stream(
      const record_gpu_dispatch_vulkan_config_t *config,
      record_gpu_dispatch_vulkan_start_t start,
      const record_gpu_output_vulkan_config_t *output, const char *path,
      const vulkan_encode_capture_ops_t *ops, bool legacy)
{
   return record_vkd_new(config, start, output, path, ops, true, legacy);
}

const vulkan_encode_pool_t *record_gpu_dispatch_vulkan_pool(const record_gpu_dispatch_vulkan_t *j)
{
   return j && !retro_atomic_load_acquire_int(&j->video_closed)
      && !retro_atomic_load_acquire_int(&j->cancelled)
      && retro_atomic_load_acquire_int(&j->state) == RECORD_VKD_READY ? j->pool : NULL;
}
vulkan_encode_capture_t *record_gpu_dispatch_vulkan_capture(const record_gpu_dispatch_vulkan_t *j)
{
   return record_gpu_dispatch_vulkan_pool(j) ? j->capture : NULL;
}
int record_gpu_dispatch_vulkan_acquire(record_gpu_dispatch_vulkan_t *j, unsigned *slot)
{
   int state;
   if (!j || retro_atomic_load_acquire_int(&j->video_closed)
         || retro_atomic_load_acquire_int(&j->cancelled))
      return RECORD_GPU_QUEUE_ERROR;
   state = retro_atomic_load_acquire_int(&j->state);
   if (state != RECORD_VKD_READY)
      return state == RECORD_VKD_STARTING ? RECORD_GPU_QUEUE_BUSY : RECORD_GPU_QUEUE_ERROR;
   return record_gpu_queue_acquire(j->queue, slot);
}
bool record_gpu_dispatch_vulkan_submit(record_gpu_dispatch_vulkan_t *j, const record_gpu_frame_t *frame)
{
   if (!j || !frame || !frame->duration || frame->duration > 0xffffffffu
         || frame->timestamp > (uint64_t)-1 - frame->duration
         || frame->timestamp < j->last_end || frame->ready_value <= j->last_ready
         || retro_atomic_load_acquire_int(&j->video_closed)
         || retro_atomic_load_acquire_int(&j->cancelled)
         || retro_atomic_load_acquire_int(&j->state) != RECORD_VKD_READY
         || !record_gpu_queue_submit(j->queue, frame))
      return false;
   j->last_end = frame->timestamp + frame->duration;
   j->last_ready = frame->ready_value;
   record_vkd_notify(j);
   return true;
}
bool record_gpu_dispatch_vulkan_cancel_slot(record_gpu_dispatch_vulkan_t *j)
{
   return j && !retro_atomic_load_acquire_int(&j->video_closed)
      && retro_atomic_load_acquire_int(&j->state) != RECORD_VKD_STARTING
      && j->queue && record_gpu_queue_cancel(j->queue);
}
int record_gpu_dispatch_vulkan_audio(record_gpu_dispatch_vulkan_t *j,
      const int16_t *samples, size_t frames)
{
   int state, result;
   if (!j || retro_atomic_load_acquire_int(&j->audio_closed)
         || retro_atomic_load_acquire_int(&j->cancelled))
      return RECORD_VULKAN_OUTPUT_ERROR;
   state = retro_atomic_load_acquire_int(&j->state);
   if (state != RECORD_VKD_READY)
      return state == RECORD_VKD_STARTING ? RECORD_VULKAN_OUTPUT_BUSY : RECORD_VULKAN_OUTPUT_ERROR;
   result = record_gpu_session_vulkan_audio(j->session, samples, frames);
   if (result == RECORD_VULKAN_OUTPUT_ERROR)
   {
      record_vkd_error(j, RECORD_VULKAN_SESSION_OUTPUT, 0);
      retro_atomic_store_release_int(&j->cancelled, 1);
   }
   if (result != RECORD_VULKAN_OUTPUT_BUSY)
      record_vkd_notify(j);
   return result;
}
bool record_gpu_dispatch_vulkan_video_finish(record_gpu_dispatch_vulkan_t *j)
{
   if (!j)
      return false;
   if (retro_atomic_load_acquire_int(&j->video_closed))
      return true;
   if (retro_atomic_load_acquire_int(&j->state) != RECORD_VKD_STARTING
         && j->queue && !record_gpu_queue_finish(j->queue))
      return false;
   retro_atomic_store_release_int(&j->video_closed, 1);
   record_vkd_notify(j);
   return true;
}
bool record_gpu_dispatch_vulkan_audio_finish(record_gpu_dispatch_vulkan_t *j)
{
   if (!j)
      return false;
   retro_atomic_store_release_int(&j->audio_closed, 1);
   record_vkd_notify(j);
   return true;
}
void record_gpu_dispatch_vulkan_cancel(record_gpu_dispatch_vulkan_t *j)
{
   if (!j)
      return;
   retro_atomic_store_release_int(&j->cancelled, 1);
   record_vkd_notify(j);
}
enum record_gpu_dispatch_vulkan_state record_gpu_dispatch_vulkan_status(
      const record_gpu_dispatch_vulkan_t *j,
      enum record_gpu_session_vulkan_error *error, int *file_error)
{
   int state = j ? retro_atomic_load_acquire_int(&j->state) : RECORD_VKD_FAILED;
   if (error)
      *error = j ? (enum record_gpu_session_vulkan_error)retro_atomic_load_acquire_int(&j->error)
         : RECORD_VULKAN_SESSION_STARTUP;
   if (file_error)
      *file_error = j ? retro_atomic_load_acquire_int(&j->file_error) : 0;
   if (j && state < RECORD_VKD_DRAINING && retro_atomic_load_acquire_int(&j->cancelled))
      state = RECORD_VKD_DRAINING;
   return (enum record_gpu_dispatch_vulkan_state)state;
}
bool record_gpu_dispatch_vulkan_audio_frames(const record_gpu_dispatch_vulkan_t *j,
      uint64_t *frames)
{
   if (!j || !frames || retro_atomic_load_acquire_int(&j->state) < RECORD_VKD_DONE)
      return false;
   *frames = j->audio_frames;
   return true;
}
bool record_gpu_dispatch_vulkan_free(record_gpu_dispatch_vulkan_t *j)
{
   if (!j)
      return true;
   if (!retro_atomic_load_acquire_int(&j->done)
         || !retro_atomic_load_acquire_int(&j->video_closed)
         || !retro_atomic_load_acquire_int(&j->audio_closed))
      return false;
#ifdef _WIN32
   if (WaitForSingleObject(j->thread, 0) != WAIT_OBJECT_0 || !CloseHandle(j->thread))
      return false;
#endif
#ifdef RECORD_VKD_STREAM
   if (j->stream_wake && !CloseHandle(j->stream_wake))
      return false;
#endif
   retro_eventcount_free(&j->wake);
   free(j->path);
   free(j);
   return true;
}
#else
record_gpu_dispatch_vulkan_t *record_gpu_dispatch_vulkan_new_backend(
      const record_gpu_dispatch_vulkan_config_t *c,
      record_gpu_dispatch_vulkan_start_t start,
      const record_gpu_output_vulkan_config_t *o, const char *p,
      const vulkan_encode_capture_ops_t *ops)
{ (void)c; (void)start; (void)o; (void)p; (void)ops; return NULL; }
record_gpu_dispatch_vulkan_t *record_gpu_dispatch_vulkan_new_stream(
      const vulkan_encode_h264_open_t *e, const vulkan_encode_h264_ops_t *o,
      const record_gpu_output_vulkan_config_t *c, const char *url,
      const vulkan_encode_capture_ops_t *ops, bool legacy)
{ (void)e; (void)o; (void)c; (void)url; (void)ops; (void)legacy; return NULL; }

record_gpu_dispatch_vulkan_t *record_gpu_dispatch_vulkan_new_backend_stream(
      const record_gpu_dispatch_vulkan_config_t *c,
      record_gpu_dispatch_vulkan_start_t start,
      const record_gpu_output_vulkan_config_t *o, const char *p,
      const vulkan_encode_capture_ops_t *ops, bool legacy)
{ (void)c; (void)start; (void)o; (void)p; (void)ops; (void)legacy; return NULL; }

bool record_gpu_dispatch_vulkan_audio_frames(const record_gpu_dispatch_vulkan_t *j,
      uint64_t *frames)
{ (void)j; (void)frames; return false; }
record_gpu_dispatch_vulkan_t *record_gpu_dispatch_vulkan_new_capture(
      const vulkan_encode_h264_open_t *e, const vulkan_encode_h264_ops_t *ops,
      const record_gpu_output_vulkan_config_t *o, const char *p,
      const vulkan_encode_capture_ops_t *capture_ops)
{ (void)e; (void)ops; (void)o; (void)p; (void)capture_ops; return NULL; }
vulkan_encode_capture_t *record_gpu_dispatch_vulkan_capture(const record_gpu_dispatch_vulkan_t *j)
{ (void)j; return NULL; }
record_gpu_dispatch_vulkan_t *record_gpu_dispatch_vulkan_new(
      const vulkan_encode_h264_open_t *e, const vulkan_encode_h264_ops_t *ops,
      const record_gpu_output_vulkan_config_t *o, const char *p)
{ (void)e; (void)ops; (void)o; (void)p; return NULL; }
const vulkan_encode_pool_t *record_gpu_dispatch_vulkan_pool(const record_gpu_dispatch_vulkan_t *j)
{ (void)j; return NULL; }
int record_gpu_dispatch_vulkan_acquire(record_gpu_dispatch_vulkan_t *j, unsigned *slot)
{ (void)j; (void)slot; return RECORD_GPU_QUEUE_ERROR; }
bool record_gpu_dispatch_vulkan_submit(record_gpu_dispatch_vulkan_t *j, const record_gpu_frame_t *f)
{ (void)j; (void)f; return false; }
bool record_gpu_dispatch_vulkan_cancel_slot(record_gpu_dispatch_vulkan_t *j)
{ (void)j; return false; }
int record_gpu_dispatch_vulkan_audio(record_gpu_dispatch_vulkan_t *j, const int16_t *p, size_t n)
{ (void)j; (void)p; (void)n; return RECORD_VULKAN_OUTPUT_ERROR; }
bool record_gpu_dispatch_vulkan_video_finish(record_gpu_dispatch_vulkan_t *j)
{ (void)j; return false; }
bool record_gpu_dispatch_vulkan_audio_finish(record_gpu_dispatch_vulkan_t *j)
{ (void)j; return false; }
void record_gpu_dispatch_vulkan_cancel(record_gpu_dispatch_vulkan_t *j) { (void)j; }
enum record_gpu_dispatch_vulkan_state record_gpu_dispatch_vulkan_status(
      const record_gpu_dispatch_vulkan_t *j, enum record_gpu_session_vulkan_error *e, int *f)
{ (void)j; if (e) *e = RECORD_VULKAN_SESSION_STARTUP; if (f) *f = 0; return RECORD_VKD_FAILED; }
bool record_gpu_dispatch_vulkan_free(record_gpu_dispatch_vulkan_t *j) { return j == NULL; }
#endif
