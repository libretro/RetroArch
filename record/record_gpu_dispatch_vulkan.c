/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif
#include "record_gpu_dispatch_vulkan.h"
#include <retro_atomic.h>
#include <stdlib.h>
#include <string.h>
#if defined(HAVE_THREADS) && defined(RETRO_ATOMIC_LOCK_FREE) \
      && (defined(__linux__) || (defined(_WIN32) && !defined(__WINRT__) && !defined(_XBOX)))
#include <rthreads/retro_eventcount.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif
struct record_gpu_dispatch_vulkan
{
   vulkan_encode_h264_open_t encoder;
   vulkan_encode_h264_ops_t ops;
   record_gpu_output_vulkan_config_t output;
   uint64_t last_end, last_ready;
   record_gpu_session_vulkan_t *session;
   record_gpu_queue_t *queue;
   const vulkan_encode_pool_t *pool;
   char *path;
   retro_eventcount_t wake;
   retro_atomic_int_t state, done, video_closed, audio_closed, cancelled, error, file_error;
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
static int record_vkd_service(record_gpu_dispatch_vulkan_t *j)
{
   enum record_gpu_session_vulkan_error error;
   bool closed = retro_atomic_load_acquire_int(&j->video_closed)
      && retro_atomic_load_acquire_int(&j->audio_closed);
   int result, file_error;
   if (!j->session)
      return closed ? RECORD_VULKAN_OUTPUT_ERROR : RECORD_VULKAN_OUTPUT_BUSY;
   if (retro_atomic_load_acquire_int(&j->cancelled))
      record_gpu_session_vulkan_cancel(j->session);
   if (retro_atomic_load_acquire_int(&j->video_closed))
      record_gpu_session_vulkan_video_finish(j->session);
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
      if (!record_gpu_session_vulkan_free(j->session))
         return RECORD_VULKAN_OUTPUT_BUSY;
      j->session = NULL;
   }
   return result;
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
   if (retro_atomic_load_acquire_int(&j->cancelled))
      error = RECORD_VULKAN_SESSION_CANCELLED;
   else
   {
      j->session = record_gpu_session_vulkan_new(&j->encoder, &j->ops, &j->output, j->path);
      error = record_gpu_session_vulkan_error(j->session, NULL);
   }
   if (error)
      record_vkd_error(j, error, 0);
   else
   {
      j->pool = record_gpu_session_vulkan_pool(j->session);
      j->queue = record_gpu_session_vulkan_queue(j->session);
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
   record_gpu_dispatch_vulkan_t *j;
   size_t length;
#ifndef _WIN32
   pthread_attr_t attributes;
   pthread_t thread;
   int result;
#endif
   if (!encoder || !ops || !output || !path || !*path
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
   j->encoder = *encoder;
   j->ops = *ops;
   j->output = *output;
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
   retro_eventcount_free(&j->wake);
   free(j->path);
   free(j);
   return NULL;
}

const vulkan_encode_pool_t *record_gpu_dispatch_vulkan_pool(const record_gpu_dispatch_vulkan_t *j)
{
   return j && !retro_atomic_load_acquire_int(&j->video_closed)
      && !retro_atomic_load_acquire_int(&j->cancelled)
      && retro_atomic_load_acquire_int(&j->state) == RECORD_VKD_READY ? j->pool : NULL;
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
   retro_eventcount_notify(&j->wake);
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
      retro_eventcount_notify(&j->wake);
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
   retro_eventcount_notify(&j->wake);
   return true;
}
bool record_gpu_dispatch_vulkan_audio_finish(record_gpu_dispatch_vulkan_t *j)
{
   if (!j)
      return false;
   retro_atomic_store_release_int(&j->audio_closed, 1);
   retro_eventcount_notify(&j->wake);
   return true;
}
void record_gpu_dispatch_vulkan_cancel(record_gpu_dispatch_vulkan_t *j)
{
   if (!j)
      return;
   retro_atomic_store_release_int(&j->cancelled, 1);
   retro_eventcount_notify(&j->wake);
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
   retro_eventcount_free(&j->wake);
   free(j->path);
   free(j);
   return true;
}
#else
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
