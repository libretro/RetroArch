/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include "record_session_mediacodec.h"
#include "record_mediacodec_file.h"
#include <retro_atomic.h>
#include <stdlib.h>

struct record_session_mediacodec
{
   uint64_t audio_end;
   int64_t video_end;
   record_mediacodec_encoder_t *video, *audio;
   record_audio_queue_t *pcm;
   record_mux_mediacodec_t *mux;
   record_mediacodec_worker_t *worker;
   record_aac_probe_t *probe;
   record_mediacodec_file_t *file;
   retro_atomic_int_t audio_closed, gpu_done, gpu_released, cancelled;
   retro_atomic_int_t error, file_error;
   bool finishing, draining, retired;
};

static void record_mcs_fail(record_session_mediacodec_t *s, int error)
{
   if (!retro_atomic_load_acquire_int(&s->error))
      retro_atomic_store_release_int(&s->error, error);
}

record_session_mediacodec_t *record_session_mediacodec_new(JNIEnv *env,
      const mediacodec_encoder_request_t *video, unsigned interval,
      unsigned rate, unsigned channels, unsigned bitrate,
      size_t max_audio_frames, size_t audio_capacity, const char *path)
{
   record_session_mediacodec_t *s;
   int error = 0;
   bool audio
         = rate || channels || bitrate || max_audio_frames || audio_capacity;
   if (!video || !path || !*path
         || (audio
               && (!rate || !channels || channels > 2 || !bitrate
                     || !max_audio_frames || !audio_capacity)))
      return NULL;
   s = (record_session_mediacodec_t *)calloc(1, sizeof(*s));
   if (!s)
      return NULL;
   retro_atomic_int_init(&s->audio_closed, !audio);
   retro_atomic_int_init(&s->gpu_done, 0);
   retro_atomic_int_init(&s->gpu_released, 0);
   retro_atomic_int_init(&s->cancelled, 0);
   retro_atomic_int_init(&s->error, 0);
   retro_atomic_int_init(&s->file_error, 0);
   s->video = record_mediacodec_encoder_new(env, video, interval);
   if (!s->video)
      goto failed;
   if (audio)
   {
      s->audio = record_mediacodec_encoder_audio_new(rate, channels, bitrate);
      s->pcm = record_audio_queue_new(
            channels, max_audio_frames, audio_capacity);
      s->probe = record_aac_probe_new(rate, channels);
      if (!s->audio || !s->pcm || !s->probe)
         goto failed;
   }
   s->file = record_mediacodec_file_new(path, &error);
   if (!s->file)
      goto failed;
   s->mux = record_mux_mediacodec_new(
         record_mediacodec_file_descriptor(s->file), audio);
   if (!s->mux)
      goto failed;
   s->worker = record_mediacodec_worker_new(
         s->video, s->audio, s->pcm, s->mux, channels, rate);
   if (s->worker && (!audio
         || record_mediacodec_worker_calibrate(s->worker, s->probe)))
      return s;
failed:
   if (s->worker)
      record_mediacodec_worker_free(s->worker);
   record_aac_probe_free(s->probe);
   record_mux_mediacodec_free(s->mux);
   record_mediacodec_file_close(s->file, false, &error);
   record_audio_queue_free(s->pcm);
   record_mediacodec_encoder_free(s->audio);
   record_mediacodec_encoder_free(s->video);
   free(s);
   return NULL;
}

void *record_session_mediacodec_surface(record_session_mediacodec_t *s)
{
   return s ? record_mediacodec_encoder_surface(s->video) : NULL;
}

bool record_session_mediacodec_cancelled(const record_session_mediacodec_t *s)
{
   return !s || retro_atomic_load_acquire_int(&s->cancelled)
         || retro_atomic_load_acquire_int(&s->error);
}

void record_session_mediacodec_cancel(record_session_mediacodec_t *s)
{
   if (s)
      retro_atomic_store_release_int(&s->cancelled, 1);
}

int record_session_mediacodec_audio(record_session_mediacodec_t *s,
      const int16_t *samples, size_t frames, uint64_t timestamp)
{
   if (!s || retro_atomic_load_acquire_int(&s->audio_closed)
         || record_session_mediacodec_cancelled(s))
      return RECORD_AUDIO_ERROR;
   return record_audio_queue_push(s->pcm, samples, frames, timestamp);
}

bool record_session_mediacodec_audio_finish(
      record_session_mediacodec_t *s, uint64_t end_sample)
{
   if (!s)
      return false;
   if (retro_atomic_load_acquire_int(&s->audio_closed))
      return s->audio_end == end_sample;
   if (!record_audio_queue_finish(s->pcm))
      return false;
   s->audio_end = end_sample;
   retro_atomic_store_release_int(&s->audio_closed, 1);
   return true;
}

bool record_session_mediacodec_gpu_retired(
      record_session_mediacodec_t *s, int64_t end_us)
{
   if (!s || end_us < 0)
      return false;
   if (retro_atomic_load_acquire_int(&s->gpu_done))
      return s->video_end == end_us;
   s->video_end = end_us;
   retro_atomic_store_release_int(&s->gpu_done, 1);
   return true;
}

bool record_session_mediacodec_gpu_released(record_session_mediacodec_t *s)
{
   if (!s || !retro_atomic_load_acquire_int(&s->gpu_done))
      return false;
   retro_atomic_store_release_int(&s->gpu_released, 1);
   return true;
}

static bool record_mcs_close_output(
      record_session_mediacodec_t *s, bool complete)
{
   int error = 0;
   uint64_t skip = 0, frames = 0;
   unsigned roll = 0;
   bool edit = complete && s->probe;
   if (s->worker)
   {
      if (edit && !record_mediacodec_worker_audio_edit(s->worker,
                &skip, &frames, &roll))
      {
         record_mcs_fail(s, RECORD_MC_SESSION_OUTPUT);
         complete = false;
      }
      if (!complete)
         record_mediacodec_worker_abort(s->worker);
      if (!record_mediacodec_worker_free(s->worker))
         return false;
      s->worker = NULL;
   }
   record_aac_probe_free(s->probe);
   s->probe = NULL;
   if (!complete)
   {
      if (!record_mediacodec_encoder_stop(s->video))
         record_mcs_fail(s, RECORD_MC_SESSION_OUTPUT);
      if (s->audio && !record_mediacodec_encoder_stop(s->audio))
         record_mcs_fail(s, RECORD_MC_SESSION_OUTPUT);
   }
   record_mux_mediacodec_free(s->mux);
   s->mux = NULL;
   if (complete && edit && !record_mediacodec_file_audio_edit(s->file,
             skip, frames, roll, &error))
   {
      retro_atomic_store_release_int(&s->file_error, error);
      record_mcs_fail(s, RECORD_MC_SESSION_FILE);
      complete = false;
   }
   if (!record_mediacodec_file_close(s->file, complete, &error))
   {
      retro_atomic_store_release_int(&s->file_error, error);
      record_mcs_fail(s, RECORD_MC_SESSION_FILE);
   }
   s->file = NULL;
   s->draining = true;
   return true;
}

int record_session_mediacodec_step(record_session_mediacodec_t *s)
{
   const int16_t *samples;
   size_t frames;
   uint64_t timestamp;
   int result;
   bool closed;
   if (!s)
      return RECORD_MC_WORKER_ERROR;
   if (s->retired)
      return retro_atomic_load_acquire_int(&s->error) ? RECORD_MC_WORKER_ERROR
                                                      : RECORD_MC_WORKER_END;
   if (retro_atomic_load_acquire_int(&s->cancelled))
      record_mcs_fail(s, RECORD_MC_SESSION_CANCELLED);
   closed = retro_atomic_load_acquire_int(&s->audio_closed)
         && retro_atomic_load_acquire_int(&s->gpu_done);
   if (!s->draining && !retro_atomic_load_acquire_int(&s->error))
   {
      if (closed && !s->finishing)
      {
         if (!record_mediacodec_worker_finish(
                   s->worker, s->video_end, s->audio_end))
            record_mcs_fail(s, RECORD_MC_SESSION_OUTPUT);
         else
            s->finishing = true;
      }
      if (!retro_atomic_load_acquire_int(&s->error))
      {
         result = record_mediacodec_worker_step(s->worker);
         if (result == RECORD_MC_WORKER_END)
         {
            if (!record_mcs_close_output(s, true))
               record_mcs_fail(s, RECORD_MC_SESSION_OUTPUT);
         }
         else if (result == RECORD_MC_WORKER_ERROR)
            record_mcs_fail(s, RECORD_MC_SESSION_OUTPUT);
         else
            return result;
      }
   }
   if (!s->draining && !record_mcs_close_output(s, false))
      return RECORD_MC_WORKER_BUSY;
   if (s->pcm)
   {
      result = record_audio_queue_peek(s->pcm, &samples, &frames, &timestamp);
      if (result == RECORD_AUDIO_OK)
      {
         if (!record_audio_queue_ack(s->pcm))
            record_mcs_fail(s, RECORD_MC_SESSION_OUTPUT);
         return RECORD_MC_WORKER_PROGRESS;
      }
      if (result != RECORD_AUDIO_END)
         return RECORD_MC_WORKER_BUSY;
   }
   if (!closed || !retro_atomic_load_acquire_int(&s->gpu_released))
      return RECORD_MC_WORKER_BUSY;
   if (!record_audio_queue_free(s->pcm))
      return RECORD_MC_WORKER_BUSY;
   s->pcm = NULL;
   record_mediacodec_encoder_free(s->audio);
   record_mediacodec_encoder_free(s->video);
   s->audio = s->video = NULL;
   s->retired = true;
   return retro_atomic_load_acquire_int(&s->error) ? RECORD_MC_WORKER_ERROR
                                                   : RECORD_MC_WORKER_END;
}

enum record_session_mediacodec_error record_session_mediacodec_error(
      const record_session_mediacodec_t *s, int *file_error)
{
   if (file_error)
      *file_error = s ? retro_atomic_load_acquire_int(&s->file_error) : 0;
   return s
         ? (enum record_session_mediacodec_error)retro_atomic_load_acquire_int(
                 &s->error)
         : RECORD_MC_SESSION_OUTPUT;
}

bool record_session_mediacodec_free(record_session_mediacodec_t *s)
{
   if (s && !s->retired)
      return false;
   free(s);
   return true;
}
