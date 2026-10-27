/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif
#include "record_dispatch_mediacodec.h"
#include <retro_atomic.h>
#include <stdlib.h>
#include <string.h>
#if defined(HAVE_THREADS) && defined(RETRO_ATOMIC_LOCK_FREE)                   \
      && (defined(__ANDROID__) || defined(RECORD_MEDIACODEC_DISPATCH_TEST))
#include <jni/rjni_env.h>
#include <pthread.h>
#include <sys/eventfd.h>
#include <poll.h>
#include <unistd.h>
#include <errno.h>

struct record_dispatch_mediacodec
{
   mediacodec_encoder_request_t video;
   uint64_t audio_end;
   int64_t video_end;
   size_t max_frames, capacity;
   char *path, *mime;
   record_session_mediacodec_t *session;
   void *window;
   pthread_t thread;
   retro_atomic_int_t state, done, epoch, cancelled, audio_closed;
   retro_atomic_int_t gpu_done, gpu_released, gpu_owner, error, file_error,
         system_error;
   unsigned interval, rate, channels, bitrate;
   int wake;
   bool joinable;
};

static void record_mcd_system(record_dispatch_mediacodec_t *j, int error)
{
   retro_atomic_cas_int(&j->system_error, 0, error ? error : EIO);
   retro_atomic_store_release_int(&j->cancelled, 1);
}

void record_dispatch_mediacodec_notify(record_dispatch_mediacodec_t *j)
{
   uint64_t one = 1;
   if (j && (retro_atomic_fetch_add_seq_cst_int(&j->epoch, 2) & 1)
         && write(j->wake, &one, sizeof(one)) != (ssize_t)sizeof(one)
         && errno != EAGAIN)
      record_mcd_system(j, errno);
}

static int record_mcd_service(record_dispatch_mediacodec_t *j)
{
   int result, detail, error;
   bool audio_closed = retro_atomic_load_acquire_int(&j->audio_closed);
   bool gpu_done = retro_atomic_load_acquire_int(&j->gpu_done);
   bool gpu_released = retro_atomic_load_acquire_int(&j->gpu_released);
   if (!j->session)
      return audio_closed && gpu_done && gpu_released ? RECORD_MC_WORKER_ERROR
                                                      : RECORD_MC_WORKER_BUSY;
   if ((audio_closed
             && !record_session_mediacodec_audio_finish(
                   j->session, j->audio_end))
         || (gpu_done
               && !record_session_mediacodec_gpu_retired(
                     j->session, j->video_end))
         || (gpu_released
               && !record_session_mediacodec_gpu_released(j->session)))
      retro_atomic_store_release_int(&j->cancelled, 1);
   if (retro_atomic_load_acquire_int(&j->cancelled))
      record_session_mediacodec_cancel(j->session);
   result = record_session_mediacodec_step(j->session);
   error = (int)record_session_mediacodec_error(j->session, &detail);
   if (error)
      retro_atomic_cas_int(&j->error, 0, error);
   retro_atomic_store_release_int(&j->file_error, detail);
   if (error || retro_atomic_load_acquire_int(&j->cancelled))
      retro_atomic_store_release_int(&j->state, RECORD_MC_DISPATCH_DRAINING);
   if (result == RECORD_MC_WORKER_END || result == RECORD_MC_WORKER_ERROR)
   {
      if (!audio_closed || !gpu_done || !gpu_released
            || !record_session_mediacodec_free(j->session))
         return RECORD_MC_WORKER_BUSY;
      j->session = NULL;
   }
   return result;
}

static void *record_mcd_run(void *opaque)
{
   record_dispatch_mediacodec_t *j = (record_dispatch_mediacodec_t *)opaque;
   JNIEnv *env = rjni_env();
   struct pollfd descriptor;
   uint64_t value;
   int result, waited;
   if (env)
      j->session = record_session_mediacodec_new(env, &j->video, j->interval,
            j->rate, j->channels, j->bitrate, j->max_frames, j->capacity,
            j->path);
   if (j->session)
      j->window = record_session_mediacodec_surface(j->session);
   if (!j->session || !j->window)
   {
      retro_atomic_store_release_int(&j->error, RECORD_MC_SESSION_OUTPUT);
      retro_atomic_store_release_int(&j->cancelled, 1);
   }
   retro_atomic_store_release_int(&j->state,
         record_dispatch_mediacodec_cancelled(j) ? RECORD_MC_DISPATCH_DRAINING
                                                 : RECORD_MC_DISPATCH_READY);
   descriptor.fd = j->wake;
   descriptor.events = POLLIN;
   for (;;)
   {
      result = record_mcd_service(j);
      if (result == RECORD_MC_WORKER_END || result == RECORD_MC_WORKER_ERROR)
         break;
      if (result != RECORD_MC_WORKER_BUSY)
         continue;
      retro_atomic_fetch_add_seq_cst_int(&j->epoch, 1);
      result = record_mcd_service(j);
      if (result != RECORD_MC_WORKER_BUSY)
      {
         retro_atomic_fetch_and_int(&j->epoch, ~1);
         if (result == RECORD_MC_WORKER_END || result == RECORD_MC_WORKER_ERROR)
            break;
         continue;
      }
      descriptor.revents = 0;
      if (retro_atomic_load_acquire_int(&j->system_error))
         descriptor.fd = -1;
      waited = poll(&descriptor, 1, j->session ? 1 : 100);
      retro_atomic_fetch_and_int(&j->epoch, ~1);
      if (waited < 0 && errno != EINTR)
         record_mcd_system(j, errno);
      else if (descriptor.revents & (POLLERR | POLLHUP | POLLNVAL))
         record_mcd_system(j, EIO);
      else if (descriptor.revents & POLLIN)
      {
         if (read(j->wake, &value, sizeof(value)) != (ssize_t)sizeof(value)
               && errno != EAGAIN && errno != EINTR)
            record_mcd_system(j, errno);
      }
   }
   retro_atomic_store_release_int(&j->state,
         result == RECORD_MC_WORKER_END
                     && !retro_atomic_load_acquire_int(&j->system_error)
               ? RECORD_MC_DISPATCH_DONE
               : RECORD_MC_DISPATCH_FAILED);
   retro_atomic_store_release_int(&j->done, 1);
   return NULL;
}

record_dispatch_mediacodec_t *record_dispatch_mediacodec_new(
      const mediacodec_encoder_request_t *video, unsigned interval,
      unsigned rate, unsigned channels, unsigned bitrate,
      size_t max_audio_frames, size_t audio_capacity, const char *path)
{
   record_dispatch_mediacodec_t *j;
   size_t path_length, mime_length;
   if (!video || !video->mime || !path || !*path)
      return NULL;
   for (path_length = 0; path_length < 32768 && path[path_length];
         path_length++)
   {
   }
   for (mime_length = 0; mime_length < 128 && video->mime[mime_length];
         mime_length++)
   {
   }
   if (path_length == 32768 || !mime_length || mime_length == 128)
      return NULL;
   j = (record_dispatch_mediacodec_t *)calloc(1, sizeof(*j));
   if (!j)
      return NULL;
   j->wake = -1;
   j->path = (char *)malloc(path_length + 1);
   j->mime = (char *)malloc(mime_length + 1);
   if (!j->path || !j->mime)
      goto failed;
   memcpy(j->path, path, path_length + 1);
   memcpy(j->mime, video->mime, mime_length + 1);
   j->video = *video;
   j->video.mime = j->mime;
   j->interval = interval;
   j->rate = rate;
   j->channels = channels;
   j->bitrate = bitrate;
   j->max_frames = max_audio_frames;
   j->capacity = audio_capacity;
   retro_atomic_int_init(&j->state, RECORD_MC_DISPATCH_STARTING);
   retro_atomic_int_init(&j->done, 0);
   retro_atomic_int_init(&j->epoch, 0);
   retro_atomic_int_init(&j->cancelled, 0);
   retro_atomic_int_init(&j->audio_closed,
         !(rate || channels || bitrate || max_audio_frames || audio_capacity));
   retro_atomic_int_init(&j->gpu_done, 0);
   retro_atomic_int_init(&j->gpu_released, 0);
   retro_atomic_int_init(&j->gpu_owner, 0);
   retro_atomic_int_init(&j->error, 0);
   retro_atomic_int_init(&j->file_error, 0);
   retro_atomic_int_init(&j->system_error, 0);
   j->wake = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
   if (j->wake < 0 || pthread_create(&j->thread, NULL, record_mcd_run, j))
      goto failed;
   j->joinable = pthread_detach(j->thread) != 0;
   return j;
failed:
   if (j->wake >= 0)
      close(j->wake);
   free(j->path);
   free(j->mime);
   free(j);
   return NULL;
}

void *record_dispatch_mediacodec_surface(record_dispatch_mediacodec_t *j)
{
   return j
               && retro_atomic_load_acquire_int(&j->state)
                     == RECORD_MC_DISPATCH_READY
         ? j->window
         : NULL;
}
bool record_dispatch_mediacodec_video_format(
      const record_dispatch_mediacodec_t *j, unsigned *width, unsigned *height,
      unsigned *color)
{
   if (!j || !width || !height || !color)
      return false;
   *width = j->video.width;
   *height = j->video.height;
   *color = j->video.color;
   return true;
}
bool record_dispatch_mediacodec_gpu_claim(record_dispatch_mediacodec_t *j)
{
   if (!j || !retro_atomic_cas_int(&j->gpu_owner, 0, 1))
      return false;
   if (retro_atomic_load_acquire_int(&j->gpu_done))
   {
      retro_atomic_store_release_int(&j->gpu_owner, 0);
      return false;
   }
   return true;
}
void record_dispatch_mediacodec_gpu_release_claim(
      record_dispatch_mediacodec_t *j)
{
   if (j)
      retro_atomic_store_release_int(&j->gpu_owner, 0);
}

int record_dispatch_mediacodec_audio(record_dispatch_mediacodec_t *j,
      const int16_t *samples, size_t frames, uint64_t timestamp)
{
   int state, result;
   if (!j || retro_atomic_load_acquire_int(&j->audio_closed)
         || record_dispatch_mediacodec_cancelled(j))
      return RECORD_AUDIO_ERROR;
   state = retro_atomic_load_acquire_int(&j->state);
   if (state != RECORD_MC_DISPATCH_READY)
      return state == RECORD_MC_DISPATCH_STARTING ? RECORD_AUDIO_BUSY
                                                  : RECORD_AUDIO_ERROR;
   result = record_session_mediacodec_audio(
         j->session, samples, frames, timestamp);
   if (result == RECORD_AUDIO_OK)
      record_dispatch_mediacodec_notify(j);
   return result;
}

bool record_dispatch_mediacodec_audio_finish(
      record_dispatch_mediacodec_t *j, uint64_t end_sample)
{
   if (!j)
      return false;
   if (retro_atomic_load_acquire_int(&j->audio_closed))
      return j->audio_end == end_sample;
   j->audio_end = end_sample;
   retro_atomic_store_release_int(&j->audio_closed, 1);
   record_dispatch_mediacodec_notify(j);
   return true;
}

bool record_dispatch_mediacodec_gpu_retired(
      record_dispatch_mediacodec_t *j, int64_t end_us)
{
   if (!j || end_us < 0)
      return false;
   if (retro_atomic_load_acquire_int(&j->gpu_done))
      return j->video_end == end_us;
   j->video_end = end_us;
   retro_atomic_store_release_int(&j->gpu_done, 1);
   record_dispatch_mediacodec_notify(j);
   return true;
}

bool record_dispatch_mediacodec_gpu_released(record_dispatch_mediacodec_t *j)
{
   if (!j || !retro_atomic_load_acquire_int(&j->gpu_done))
      return false;
   retro_atomic_store_release_int(&j->gpu_released, 1);
   record_dispatch_mediacodec_notify(j);
   return true;
}

void record_dispatch_mediacodec_cancel(record_dispatch_mediacodec_t *j)
{
   if (j)
   {
      retro_atomic_store_release_int(&j->cancelled, 1);
      record_dispatch_mediacodec_notify(j);
   }
}

bool record_dispatch_mediacodec_cancelled(const record_dispatch_mediacodec_t *j)
{
   return !j || retro_atomic_load_acquire_int(&j->cancelled)
         || retro_atomic_load_acquire_int(&j->error)
         || retro_atomic_load_acquire_int(&j->system_error);
}

enum record_dispatch_mediacodec_state record_dispatch_mediacodec_status(
      const record_dispatch_mediacodec_t *j, int *error, int *file_error,
      int *system_error)
{
   if (error)
      *error = j ? retro_atomic_load_acquire_int(&j->error)
                 : RECORD_MC_SESSION_OUTPUT;
   if (file_error)
      *file_error = j ? retro_atomic_load_acquire_int(&j->file_error) : 0;
   if (system_error)
      *system_error = j ? retro_atomic_load_acquire_int(&j->system_error) : 0;
   return j
         ? (enum record_dispatch_mediacodec_state)retro_atomic_load_acquire_int(
                 &j->state)
         : RECORD_MC_DISPATCH_FAILED;
}

bool record_dispatch_mediacodec_free(record_dispatch_mediacodec_t *j)
{
   if (!j)
      return true;
   if (retro_atomic_load_acquire_int(&j->gpu_owner)
         || !retro_atomic_load_acquire_int(&j->done))
      return false;
   if (j->joinable && pthread_join(j->thread, NULL))
      return false;
   close(j->wake);
   free(j->path);
   free(j->mime);
   free(j);
   return true;
}
#else
bool record_dispatch_mediacodec_gpu_claim(record_dispatch_mediacodec_t *j)
{
   (void)j;
   return false;
}
void record_dispatch_mediacodec_gpu_release_claim(
      record_dispatch_mediacodec_t *j)
{
   (void)j;
}
bool record_dispatch_mediacodec_video_format(
      const record_dispatch_mediacodec_t *j, unsigned *width, unsigned *height,
      unsigned *color)
{
   (void)j;
   (void)width;
   (void)height;
   (void)color;
   return false;
}
record_dispatch_mediacodec_t *record_dispatch_mediacodec_new(
      const mediacodec_encoder_request_t *video, unsigned interval,
      unsigned rate, unsigned channels, unsigned bitrate,
      size_t max_audio_frames, size_t audio_capacity, const char *path)
{
   (void)video;
   (void)interval;
   (void)rate;
   (void)channels;
   (void)bitrate;
   (void)max_audio_frames;
   (void)audio_capacity;
   (void)path;
   return NULL;
}
void *record_dispatch_mediacodec_surface(record_dispatch_mediacodec_t *j)
{
   (void)j;
   return NULL;
}
int record_dispatch_mediacodec_audio(record_dispatch_mediacodec_t *j,
      const int16_t *samples, size_t frames, uint64_t timestamp)
{
   (void)j;
   (void)samples;
   (void)frames;
   (void)timestamp;
   return RECORD_AUDIO_ERROR;
}
bool record_dispatch_mediacodec_audio_finish(
      record_dispatch_mediacodec_t *j, uint64_t end)
{
   (void)j;
   (void)end;
   return false;
}
bool record_dispatch_mediacodec_gpu_retired(
      record_dispatch_mediacodec_t *j, int64_t end)
{
   (void)j;
   (void)end;
   return false;
}
bool record_dispatch_mediacodec_gpu_released(record_dispatch_mediacodec_t *j)
{
   (void)j;
   return false;
}
void record_dispatch_mediacodec_notify(record_dispatch_mediacodec_t *j)
{
   (void)j;
}
void record_dispatch_mediacodec_cancel(record_dispatch_mediacodec_t *j)
{
   (void)j;
}
bool record_dispatch_mediacodec_cancelled(const record_dispatch_mediacodec_t *j)
{
   (void)j;
   return true;
}
enum record_dispatch_mediacodec_state record_dispatch_mediacodec_status(
      const record_dispatch_mediacodec_t *j, int *error, int *file_error,
      int *system_error)
{
   (void)j;
   if (error)
      *error = RECORD_MC_SESSION_OUTPUT;
   if (file_error)
      *file_error = 0;
   if (system_error)
      *system_error = 0;
   return RECORD_MC_DISPATCH_FAILED;
}
bool record_dispatch_mediacodec_free(record_dispatch_mediacodec_t *j)
{
   return j == NULL;
}
#endif
