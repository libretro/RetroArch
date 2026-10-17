/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "record_gpu_dispatch_d3d12.h"
#include <stdlib.h>
#include <string.h>
#include <retro_atomic.h>
#if defined(_WIN32) && defined(HAVE_THREADS) && !defined(__WINRT__) && !defined(_XBOX)
#include <windows.h>
struct record_gpu_dispatch_d3d12
{
   nvenc_d3d12_config_t encoder;
   record_gpu_output_config_t output;
   record_gpu_capture_d3d12_t *capture;
   record_gpu_session_d3d12_t *session;
   char *path;
   HANDLE wake, thread;
   retro_atomic_int_t epoch, state, cancelled, video_closed, audio_closed;
   retro_atomic_int_t error, file_error, system_error;
   int completion;
   bool owns_capture;
};

static void record_gpu_dispatch_os_error(record_gpu_dispatch_d3d12_t *j)
{
   DWORD error = GetLastError();
   retro_atomic_cas_int(&j->system_error, 0, (int)(error ? error : ERROR_GEN_FAILURE));
   retro_atomic_store_release_int(&j->cancelled, 1);
}

static void record_gpu_dispatch_notify(record_gpu_dispatch_d3d12_t *j)
{
   if ((retro_atomic_fetch_add_seq_cst_int(&j->epoch, 2) & 1)
         && !SetEvent(j->wake))
      record_gpu_dispatch_os_error(j);
}

static int record_gpu_dispatch_service(record_gpu_dispatch_d3d12_t *j)
{
   enum record_gpu_session_error error;
   uint32_t detail;
   int result;
   bool closed = retro_atomic_load_acquire_int(&j->video_closed)
      && retro_atomic_load_acquire_int(&j->audio_closed);
   if (!j->session)
   {
      if (!closed) return RECORD_GPU_OUTPUT_BUSY;
      if (retro_atomic_load_acquire_int(&j->cancelled))
      {
         j->completion = RECORD_GPU_OUTPUT_ERROR;
         if (!retro_atomic_load_acquire_int(&j->error))
            retro_atomic_store_release_int(&j->error, RECORD_GPU_SESSION_ERROR_CANCELLED);
      }
      if (j->owns_capture && j->capture)
      {
         if (!record_gpu_capture_d3d12_free(j->capture))
            return RECORD_GPU_OUTPUT_BUSY;
         j->capture = NULL;
      }
      return j->completion;
   }
   if (retro_atomic_load_acquire_int(&j->video_closed))
      record_gpu_session_d3d12_video_finish(j->session);
   if (retro_atomic_load_acquire_int(&j->audio_closed))
      record_gpu_session_d3d12_audio_finish(j->session);
   if (retro_atomic_load_acquire_int(&j->cancelled))
      record_gpu_session_d3d12_cancel(j->session);
   result = record_gpu_session_d3d12_step(j->session);
   error = record_gpu_session_d3d12_error(j->session, &detail);
   retro_atomic_store_release_int(&j->file_error, (int)detail);
   retro_atomic_store_release_int(&j->error, (int)error);
   if (error || retro_atomic_load_acquire_int(&j->cancelled))
      retro_atomic_store_release_int(&j->state, RECORD_GPU_DISPATCH_DRAINING);
   if (result == RECORD_GPU_OUTPUT_END || result == RECORD_GPU_OUTPUT_ERROR)
   {
      if (!closed || !record_gpu_session_d3d12_free(j->session))
         return RECORD_GPU_OUTPUT_BUSY;
      j->session = NULL;
      j->completion = result;
      retro_atomic_store_release_int(&j->state, RECORD_GPU_DISPATCH_DRAINING);
      return RECORD_GPU_OUTPUT_PROGRESS;
   }
   return result;
}

static DWORD WINAPI record_gpu_dispatch_run(void *opaque)
{
   record_gpu_dispatch_d3d12_t *j = (record_gpu_dispatch_d3d12_t*)opaque;
   enum record_gpu_session_error error;
   int result;
   j->session = record_gpu_session_d3d12_new(j->capture, &j->encoder, &j->output, j->path);
   error = j->session ? record_gpu_session_d3d12_error(j->session, NULL)
      : RECORD_GPU_SESSION_ERROR_STARTUP;
   retro_atomic_store_release_int(&j->error, (int)error);
   retro_atomic_store_release_int(&j->state, error ? RECORD_GPU_DISPATCH_DRAINING
         : RECORD_GPU_DISPATCH_READY);
   for (;;)
   {
      HANDLE handles[2];
      DWORD timeout, waited;
      result = record_gpu_dispatch_service(j);
      if (result == RECORD_GPU_OUTPUT_END || result == RECORD_GPU_OUTPUT_ERROR)
         break;
      if (result != RECORD_GPU_OUTPUT_BUSY)
         continue;
      if (!ResetEvent(j->wake))
      {
         record_gpu_dispatch_os_error(j);
         /* An unusable owned wake handle cannot safely retire this worker. */
         retro_atomic_store_release_int(&j->state, RECORD_GPU_DISPATCH_DRAINING);
         return 0;
      }
      /* Register before rechecking; notifications then either publish or wake. */
      retro_atomic_fetch_or_int(&j->epoch, 1);
      retro_atomic_thread_fence_seq_cst();
      result = record_gpu_dispatch_service(j);
      if (result != RECORD_GPU_OUTPUT_BUSY)
      {
         retro_atomic_fetch_and_int(&j->epoch, ~1);
         if (result == RECORD_GPU_OUTPUT_END || result == RECORD_GPU_OUTPUT_ERROR)
            break;
         continue;
      }
      handles[0] = j->wake;
      handles[1] = j->session ? record_gpu_session_d3d12_wait_handle(j->session) : NULL;
      timeout = j->session && record_gpu_session_d3d12_retry_needed(j->session) ? 1 : INFINITE;
      if (!j->session && j->owns_capture && j->capture
            && retro_atomic_load_acquire_int(&j->video_closed)
            && retro_atomic_load_acquire_int(&j->audio_closed))
         timeout = 1;
      if (retro_atomic_load_acquire_int(&j->system_error))
      {
         handles[1] = NULL;
         timeout = 1;
      }
      else if (handles[1])
      {
         waited = WaitForSingleObject(handles[1], 0);
         if (waited == WAIT_OBJECT_0 || waited == WAIT_FAILED)
         {
            if (waited == WAIT_FAILED) record_gpu_dispatch_os_error(j);
            handles[1] = NULL;
            timeout = 1;
         }
         else
            timeout = INFINITE;
      }
      waited = WaitForMultipleObjects(handles[1] ? 2 : 1, handles, FALSE, timeout);
      if (waited == WAIT_FAILED) record_gpu_dispatch_os_error(j);
      retro_atomic_fetch_and_int(&j->epoch, ~1);
   }
   retro_atomic_store_release_int(&j->state, result == RECORD_GPU_OUTPUT_END
         && !retro_atomic_load_acquire_int(&j->system_error)
         ? RECORD_GPU_DISPATCH_DONE : RECORD_GPU_DISPATCH_FAILED);
   return 0;
}

static record_gpu_dispatch_d3d12_t *record_gpu_dispatch_new(
      record_gpu_capture_d3d12_t *capture, const nvenc_d3d12_config_t *encoder,
      const record_gpu_output_config_t *output, const char *path, bool owns_capture)
{
   record_gpu_dispatch_d3d12_t *j;
   size_t length;
   if (!capture || !encoder || !output || !path || !*path
         || output->video.headers || output->video.headers_size
         || output->video.ac3_header || output->video.ac3_header_size)
      return NULL;
   length = strlen(path);
   if (length == (size_t)-1) return NULL;
   j = (record_gpu_dispatch_d3d12_t*)calloc(1, sizeof(*j));
   if (!j) return NULL;
   j->encoder = *encoder;
   j->output = *output;
   j->capture = capture;
   j->owns_capture = owns_capture;
   j->completion = RECORD_GPU_OUTPUT_ERROR;
   retro_atomic_int_init(&j->epoch, 0);
   retro_atomic_int_init(&j->state, RECORD_GPU_DISPATCH_STARTING);
   retro_atomic_int_init(&j->cancelled, 0);
   retro_atomic_int_init(&j->video_closed, 0);
   retro_atomic_int_init(&j->audio_closed, output->audio_rate ? 0 : 1);
   retro_atomic_int_init(&j->error, 0);
   retro_atomic_int_init(&j->file_error, 0);
   retro_atomic_int_init(&j->system_error, 0);
   j->path = (char*)malloc(length + 1);
   if (j->path)
   {
      memcpy(j->path, path, length + 1);
      j->wake = CreateEvent(NULL, TRUE, FALSE, NULL);
      if (j->wake)
         j->thread = CreateThread(NULL, 0, record_gpu_dispatch_run, j, 0, NULL);
   }
   if (j->thread) return j;
   if (j->wake) CloseHandle(j->wake);
   free(j->path);
   free(j);
   return NULL;
}

record_gpu_dispatch_d3d12_t *record_gpu_dispatch_d3d12_new(
      record_gpu_capture_d3d12_t *capture, const nvenc_d3d12_config_t *encoder,
      const record_gpu_output_config_t *output, const char *path)
{
   return record_gpu_dispatch_new(capture, encoder, output, path, false);
}

record_gpu_dispatch_d3d12_t *record_gpu_dispatch_d3d12_new_owned(
      record_gpu_capture_d3d12_t *capture, const nvenc_d3d12_config_t *encoder,
      const record_gpu_output_config_t *output, const char *path)
{
   return record_gpu_dispatch_new(capture, encoder, output, path, true);
}

bool record_gpu_dispatch_d3d12_video_reap(record_gpu_dispatch_d3d12_t *j)
{
   if (!j || retro_atomic_load_acquire_int(&j->video_closed)) return false;
   return record_gpu_capture_d3d12_reap(j->capture);
}

int record_gpu_dispatch_d3d12_video(record_gpu_dispatch_d3d12_t *j,
      void *source, unsigned state, unsigned x, unsigned y,
      void *fence, uint64_t value, uint64_t timestamp, uint64_t duration,
      bool keyframe, uint64_t *ready_value)
{
   int status, result;
   if (ready_value) *ready_value = 0;
   if (!j || retro_atomic_load_acquire_int(&j->video_closed)
         || retro_atomic_load_acquire_int(&j->cancelled)) return RECORD_GPU_QUEUE_ERROR;
   status = retro_atomic_load_acquire_int(&j->state);
   if (status == RECORD_GPU_DISPATCH_STARTING) return RECORD_GPU_QUEUE_BUSY;
   if (status != RECORD_GPU_DISPATCH_READY) return RECORD_GPU_QUEUE_ERROR;
   result = record_gpu_capture_d3d12_push(j->capture, source, state, x, y,
         fence, value, timestamp, duration, keyframe, ready_value);
   if (result == RECORD_GPU_QUEUE_ERROR)
      retro_atomic_store_release_int(&j->cancelled, 1);
   if (result != RECORD_GPU_QUEUE_BUSY) record_gpu_dispatch_notify(j);
   return result;
}

int record_gpu_dispatch_d3d12_audio(record_gpu_dispatch_d3d12_t *j,
      const int16_t *samples, size_t frames)
{
   int status, result;
   if (!j || retro_atomic_load_acquire_int(&j->audio_closed)
         || retro_atomic_load_acquire_int(&j->cancelled)) return RECORD_GPU_OUTPUT_ERROR;
   status = retro_atomic_load_acquire_int(&j->state);
   if (status == RECORD_GPU_DISPATCH_STARTING) return RECORD_GPU_OUTPUT_BUSY;
   if (status != RECORD_GPU_DISPATCH_READY) return RECORD_GPU_OUTPUT_ERROR;
   result = record_gpu_session_d3d12_audio(j->session, samples, frames);
   if (result == RECORD_GPU_OUTPUT_ERROR)
      retro_atomic_store_release_int(&j->cancelled, 1);
   if (result != RECORD_GPU_OUTPUT_BUSY) record_gpu_dispatch_notify(j);
   return result;
}

bool record_gpu_dispatch_d3d12_video_finish(record_gpu_dispatch_d3d12_t *j)
{
   if (!j) return false;
   if (retro_atomic_load_acquire_int(&j->video_closed)) return true;
   if (!record_gpu_capture_d3d12_finish(j->capture)) return false;
   retro_atomic_store_release_int(&j->video_closed, 1);
   record_gpu_dispatch_notify(j);
   return true;
}

bool record_gpu_dispatch_d3d12_audio_finish(record_gpu_dispatch_d3d12_t *j)
{
   if (!j) return false;
   retro_atomic_store_release_int(&j->audio_closed, 1);
   record_gpu_dispatch_notify(j);
   return true;
}

void record_gpu_dispatch_d3d12_cancel(record_gpu_dispatch_d3d12_t *j)
{
   if (!j) return;
   retro_atomic_store_release_int(&j->cancelled, 1);
   record_gpu_dispatch_notify(j);
}

enum record_gpu_dispatch_state record_gpu_dispatch_d3d12_status(
      const record_gpu_dispatch_d3d12_t *j, enum record_gpu_session_error *error,
      uint32_t *file_error, uint32_t *system_error)
{
   int status = j ? retro_atomic_load_acquire_int(&j->state) : RECORD_GPU_DISPATCH_FAILED;
   if (error) *error = j ? (enum record_gpu_session_error)retro_atomic_load_acquire_int(&j->error)
      : RECORD_GPU_SESSION_ERROR_STARTUP;
   if (file_error) *file_error = j ? (uint32_t)retro_atomic_load_acquire_int(&j->file_error) : 0;
   if (system_error) *system_error = j ? (uint32_t)retro_atomic_load_acquire_int(&j->system_error) : 0;
   if (j && status < RECORD_GPU_DISPATCH_DRAINING && retro_atomic_load_acquire_int(&j->cancelled))
      status = RECORD_GPU_DISPATCH_DRAINING;
   return (enum record_gpu_dispatch_state)status;
}

bool record_gpu_dispatch_d3d12_free(record_gpu_dispatch_d3d12_t *j)
{
   if (!j) return true;
   if (!retro_atomic_load_acquire_int(&j->video_closed)
         || !retro_atomic_load_acquire_int(&j->audio_closed)) return false;
   if (j->thread)
   {
      if (WaitForSingleObject(j->thread, 0) != WAIT_OBJECT_0 || j->session
            || (j->owns_capture && j->capture))
         return false;
      if (!CloseHandle(j->thread)) return false;
      j->thread = NULL;
   }
   if (!CloseHandle(j->wake)) return false;
   free(j->path);
   free(j);
   return true;
}
#else
record_gpu_dispatch_d3d12_t *record_gpu_dispatch_d3d12_new(
      record_gpu_capture_d3d12_t *c, const nvenc_d3d12_config_t *e,
      const record_gpu_output_config_t *o, const char *p)
{ (void)c; (void)e; (void)o; (void)p; return NULL; }
record_gpu_dispatch_d3d12_t *record_gpu_dispatch_d3d12_new_owned(
      record_gpu_capture_d3d12_t *c, const nvenc_d3d12_config_t *e,
      const record_gpu_output_config_t *o, const char *p)
{ (void)c; (void)e; (void)o; (void)p; return NULL; }
bool record_gpu_dispatch_d3d12_video_reap(record_gpu_dispatch_d3d12_t *j)
{ (void)j; return false; }
int record_gpu_dispatch_d3d12_video(record_gpu_dispatch_d3d12_t *j,
      void *s, unsigned t, unsigned x, unsigned y, void *f, uint64_t v,
      uint64_t p, uint64_t d, bool k, uint64_t *r)
{ (void)j; (void)s; (void)t; (void)x; (void)y; (void)f; (void)v; (void)p; (void)d; (void)k; if (r) *r = 0; return RECORD_GPU_QUEUE_ERROR; }
int record_gpu_dispatch_d3d12_audio(record_gpu_dispatch_d3d12_t *j, const int16_t *s, size_t n)
{ (void)j; (void)s; (void)n; return RECORD_GPU_OUTPUT_ERROR; }
bool record_gpu_dispatch_d3d12_video_finish(record_gpu_dispatch_d3d12_t *j) { (void)j; return false; }
bool record_gpu_dispatch_d3d12_audio_finish(record_gpu_dispatch_d3d12_t *j) { (void)j; return false; }
void record_gpu_dispatch_d3d12_cancel(record_gpu_dispatch_d3d12_t *j) { (void)j; }
enum record_gpu_dispatch_state record_gpu_dispatch_d3d12_status(
      const record_gpu_dispatch_d3d12_t *j, enum record_gpu_session_error *e, uint32_t *f, uint32_t *s)
{ (void)j; if (e) *e = RECORD_GPU_SESSION_ERROR_STARTUP; if (f) *f = 0; if (s) *s = 0; return RECORD_GPU_DISPATCH_FAILED; }
bool record_gpu_dispatch_d3d12_free(record_gpu_dispatch_d3d12_t *j) { return j == NULL; }
#endif
