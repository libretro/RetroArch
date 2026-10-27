/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#ifdef NDEBUG
#error Session ownership acceptance requires assertions
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#ifndef _WIN32
#include <pthread.h>
#endif
#include "../../../record/record_session_mediacodec.h"
#include "../../../record/record_mediacodec_file.h"
struct record_mediacodec_encoder
{
   int live;
};
struct record_mux_mediacodec
{
   int live;
};
struct record_mediacodec_file
{
   int live;
};
struct record_mediacodec_worker
{
   record_audio_queue_t *pcm;
   int live, closing;
};
struct record_aac_probe
{
   int live;
};
static struct record_mediacodec_encoder video, audio;
static struct record_mux_mediacodec mux;
static struct record_mediacodec_file file;
static struct record_mediacodec_worker worker;
static struct record_aac_probe probe;
static int fault, stops, completes, closes, aborts, finishes, reads_retired;
static int output_error, stop_error, file_error;
static int trim_error, edit_error;
static const int16_t pcm[4] = {1, 2, 3, 4};
static void *session_allocate(size_t count, size_t bytes)
{
   return fault == 1 ? NULL : calloc(count, bytes);
}
#define calloc session_allocate
#include "../../../record/record_session_mediacodec.c"
#undef calloc

record_aac_probe_t *record_aac_probe_new(unsigned rate, unsigned channels)
{
   assert(rate == 48000 && channels == 2 && !probe.live);
   if (fault == 7)
      return NULL;
   probe.live = 1;
   return &probe;
}
void record_aac_probe_free(record_aac_probe_t *p)
{
   if (!p)
      return;
   assert(p == &probe && p->live && !worker.live);
   p->live = 0;
}
bool record_mediacodec_worker_calibrate(record_mediacodec_worker_t *w,
      record_aac_probe_t *p)
{
   assert(w == &worker && w->live && p == &probe && p->live);
   return fault != 8;
}
bool record_mediacodec_worker_audio_edit(const record_mediacodec_worker_t *w,
      uint64_t *skip, uint64_t *frames, unsigned *roll)
{
   assert(w == &worker && w->live && probe.live);
   *skip = 10240;
   *frames = 7;
   *roll = 2;
   return !trim_error;
}
bool record_mediacodec_file_audio_edit(record_mediacodec_file_t *f,
      uint64_t skip, uint64_t frames, unsigned roll, int *error)
{
   assert(f == &file && f->live && !worker.live && !mux.live && !probe.live);
   assert(skip == 10240 && frames == 7 && roll == 2);
   *error = edit_error;
   return !edit_error;
}

record_mediacodec_encoder_t *record_mediacodec_encoder_new(JNIEnv *env,
      const mediacodec_encoder_request_t *request, unsigned interval)
{
   (void)env;
   assert(request && interval == 2 && !video.live);
   if (fault == 2)
      return NULL;
   video.live = 1;
   return &video;
}
record_mediacodec_encoder_t *record_mediacodec_encoder_audio_new(
      unsigned rate, unsigned channels, unsigned bitrate)
{
   assert(rate == 48000 && channels == 2 && bitrate == 128000 && !audio.live);
   if (fault == 3)
      return NULL;
   audio.live = 1;
   return &audio;
}
void *record_mediacodec_encoder_surface(record_mediacodec_encoder_t *e)
{
   assert(e == &video && e->live);
   return e;
}
bool record_mediacodec_encoder_stop(record_mediacodec_encoder_t *e)
{
   assert(e && e->live && !worker.live);
   stops++;
   return !stop_error;
}
void record_mediacodec_encoder_free(record_mediacodec_encoder_t *e)
{
   if (e)
   {
      assert(e->live && !worker.live && !mux.live && !file.live);
      e->live = 0;
   }
}
record_mediacodec_file_t *record_mediacodec_file_new(
      const char *path, int *error)
{
   assert(path && !file.live && error && video.live);
   if (fault == 4)
      return NULL;
   file.live = 1;
   return &file;
}
int record_mediacodec_file_descriptor(const record_mediacodec_file_t *f)
{
   assert(f == &file && f->live);
   return 42;
}
bool record_mediacodec_file_close(
      record_mediacodec_file_t *f, bool complete, int *error)
{
   assert(!worker.live && !mux.live);
   *error = 0;
   if (!f)
      return true;
   assert(f == &file && f->live);
   closes++;
   completes += complete;
   file.live = 0;
   *error = file_error;
   return !file_error;
}
record_mux_mediacodec_t *record_mux_mediacodec_new(int fd, bool has_audio)
{
   assert(fd == 42 && file.live && !mux.live && has_audio == !!audio.live);
   if (fault == 5)
      return NULL;
   mux.live = 1;
   return &mux;
}
void record_mux_mediacodec_free(record_mux_mediacodec_t *m)
{
   assert(!worker.live);
   if (m)
   {
      assert(m == &mux && m->live && file.live);
      m->live = 0;
   }
}
record_mediacodec_worker_t *record_mediacodec_worker_new(
      record_mediacodec_encoder_t *v, record_mediacodec_encoder_t *a,
      record_audio_queue_t *q, record_mux_mediacodec_t *m, unsigned channels,
      unsigned rate)
{
   assert(v == &video && m == &mux && m->live && !worker.live);
   assert((a && q && channels == 2 && rate == 48000)
         || (!a && !q && !channels && !rate));
   if (fault == 6)
      return NULL;
   worker.live = 1;
   worker.closing = 0;
   worker.pcm = q;
   return &worker;
}
bool record_mediacodec_worker_finish(
      record_mediacodec_worker_t *w, int64_t video_end, uint64_t audio_end)
{
   assert(w == &worker && w->live && reads_retired);
   finishes++;
   if (!video_end || (w->pcm && audio_end != 7))
      return false;
   w->closing = 1;
   return true;
}
int record_mediacodec_worker_step(record_mediacodec_worker_t *w)
{
   const int16_t *samples;
   size_t frames;
   uint64_t timestamp;
   int result = RECORD_AUDIO_END;
   assert(w == &worker && w->live);
   if (w->pcm)
      result = record_audio_queue_peek(w->pcm, &samples, &frames, &timestamp);
   if (output_error)
      return RECORD_MC_WORKER_ERROR;
   if (result == RECORD_AUDIO_OK)
   {
      assert(frames == 2 && samples[0] == 1 && samples[3] == 4);
      assert(timestamp == 0 || timestamp == 5);
      assert(record_audio_queue_ack(w->pcm));
      return RECORD_MC_WORKER_PROGRESS;
   }
   return w->closing && result == RECORD_AUDIO_END ? RECORD_MC_WORKER_END
                                                   : RECORD_MC_WORKER_BUSY;
}
void record_mediacodec_worker_abort(record_mediacodec_worker_t *w)
{
   assert(w == &worker && w->live);
   aborts++;
}
bool record_mediacodec_worker_free(record_mediacodec_worker_t *w)
{
   assert(w == &worker && w->live);
   w->live = 0;
   return true;
}
static record_session_mediacodec_t *open_session(bool has_audio)
{
   mediacodec_encoder_request_t request = {0};
   return record_session_mediacodec_new(NULL, &request, 2,
         has_audio ? 48000 : 0, has_audio ? 2 : 0, has_audio ? 128000 : 0,
         has_audio ? 2 : 0, has_audio ? 256 : 0, "record.mp4");
}
static void balanced(void)
{
   assert(
         !video.live && !audio.live && !worker.live && !mux.live && !file.live
         && !probe.live);
}
#ifndef _WIN32
static void *audio_producer(void *data)
{
   record_session_mediacodec_t *s = (record_session_mediacodec_t *)data;
   assert(record_session_mediacodec_audio(s, pcm, 2, 0) == RECORD_AUDIO_OK);
   assert(record_session_mediacodec_audio(s, pcm, 2, 5) == RECORD_AUDIO_OK);
   assert(record_session_mediacodec_audio_finish(s, 7));
   return NULL;
}
static void *gpu_executor(void *data)
{
   record_session_mediacodec_t *s = (record_session_mediacodec_t *)data;
   assert(record_session_mediacodec_gpu_retired(s, 100000));
   assert(record_session_mediacodec_gpu_released(s));
   return NULL;
}
static void concurrent_sessions(void)
{
   pthread_t producer, gpu;
   record_session_mediacodec_t *s;
   int cycle, at, result;
   for (cycle = 0; cycle < 64; cycle++)
   {
      s = open_session(true);
      assert(s);
      assert(!pthread_create(&producer, NULL, audio_producer, s));
      assert(!pthread_create(&gpu, NULL, gpu_executor, s));
      for (at = 0; at < 10; at++)
         assert(record_session_mediacodec_step(s) != RECORD_MC_WORKER_ERROR);
      assert(!pthread_join(producer, NULL));
      assert(!pthread_join(gpu, NULL));
      result = RECORD_MC_WORKER_BUSY;
      for (at = 0; at < 10 && result != RECORD_MC_WORKER_END; at++)
         result = record_session_mediacodec_step(s);
      assert(result == RECORD_MC_WORKER_END);
      assert(record_session_mediacodec_free(s));
      balanced();
   }
}
#endif
int main(void)
{
   mediacodec_encoder_request_t request = {0};
   record_session_mediacodec_t *s;
   int i, before, saved;
   for (fault = 1; fault <= 8; fault++)
   {
      assert(!open_session(true));
      balanced();
   }
   fault = 0;
   assert(!record_session_mediacodec_new(
         NULL, &request, 2, 48000, 2, 128000, 2, 1, "record.mp4"));
   balanced();
   assert(!record_session_mediacodec_new(
         NULL, &request, 2, 0, 2, 0, 0, 0, "record.mp4"));
   balanced();
   s = open_session(true);
   assert(s && record_session_mediacodec_surface(s) == &video);
   assert(!record_session_mediacodec_free(s));
   assert(!record_session_mediacodec_gpu_released(s));
   assert(record_session_mediacodec_audio(s, pcm, 2, 0) == RECORD_AUDIO_OK);
   assert(record_session_mediacodec_audio(s, pcm, 2, 5) == RECORD_AUDIO_OK);
   assert(record_session_mediacodec_step(s) == RECORD_MC_WORKER_PROGRESS);
   assert(record_session_mediacodec_audio_finish(s, 7));
   assert(record_session_mediacodec_audio_finish(s, 7));
   assert(!record_session_mediacodec_audio_finish(s, 8));
   assert(record_session_mediacodec_step(s) == RECORD_MC_WORKER_PROGRESS);
   assert(!finishes);
   reads_retired = 1;
   assert(record_session_mediacodec_gpu_retired(s, 100000));
   assert(record_session_mediacodec_gpu_retired(s, 100000));
   assert(!record_session_mediacodec_gpu_retired(s, 100001));
   assert(record_session_mediacodec_step(s) == RECORD_MC_WORKER_BUSY);
   assert(finishes == 1 && completes == 1 && video.live && audio.live);
   assert(!record_session_mediacodec_free(s));
   assert(record_session_mediacodec_gpu_released(s));
   assert(record_session_mediacodec_step(s) == RECORD_MC_WORKER_END);
   assert(record_session_mediacodec_step(s) == RECORD_MC_WORKER_END);
   assert(record_session_mediacodec_free(s));
   balanced();
   for (i = 0; i < 4; i++)
   {
      s = open_session(true);
      assert(s
            && record_session_mediacodec_audio(s, pcm, 2, 0)
                  == RECORD_AUDIO_OK);
      output_error = i == 0;
      stop_error = i == 2;
      file_error = i == 3 ? 5 : 0;
      if (!output_error)
         record_session_mediacodec_cancel(s);
      before = stops;
      assert(record_session_mediacodec_step(s) == RECORD_MC_WORKER_PROGRESS);
      assert(stops == before + 2 && record_session_mediacodec_cancelled(s));
      assert(record_session_mediacodec_audio(s, pcm, 2, 5)
            == RECORD_AUDIO_ERROR);
      assert(record_session_mediacodec_step(s) == RECORD_MC_WORKER_BUSY);
      assert(video.live && audio.live && !record_session_mediacodec_free(s));
      assert(record_session_mediacodec_audio_finish(s, 7));
      assert(record_session_mediacodec_step(s) == RECORD_MC_WORKER_BUSY);
      assert(record_session_mediacodec_gpu_retired(s, 0));
      assert(record_session_mediacodec_step(s) == RECORD_MC_WORKER_BUSY);
      assert(record_session_mediacodec_gpu_released(s));
      assert(record_session_mediacodec_step(s) == RECORD_MC_WORKER_ERROR);
      assert(record_session_mediacodec_step(s) == RECORD_MC_WORKER_ERROR);
      assert(stops == before + 2);
      assert(record_session_mediacodec_error(s, &saved)
            == (output_error ? RECORD_MC_SESSION_OUTPUT
                             : RECORD_MC_SESSION_CANCELLED));
      assert(saved == file_error);
      assert(record_session_mediacodec_free(s));
      balanced();
      output_error = stop_error = file_error = 0;
   }
   s = open_session(false);
   assert(s && record_session_mediacodec_audio_finish(s, 0));
   assert(!record_session_mediacodec_audio_finish(s, 1));
   assert(record_session_mediacodec_gpu_retired(s, 100000));
   assert(record_session_mediacodec_gpu_released(s));
   file_error = 5;
   assert(record_session_mediacodec_step(s) == RECORD_MC_WORKER_ERROR);
   assert(record_session_mediacodec_error(s, &saved) == RECORD_MC_SESSION_FILE
         && saved == 5);
   assert(record_session_mediacodec_free(s));
   balanced();
   file_error = 0;
   for (i = 0; i < 2; i++)
   {
      s = open_session(true);
      assert(s && record_session_mediacodec_audio_finish(s, 7));
      assert(record_session_mediacodec_gpu_retired(s, 100000));
      assert(record_session_mediacodec_gpu_released(s));
      trim_error = !i;
      edit_error = i ? 5 : 0;
      assert(record_session_mediacodec_step(s) == RECORD_MC_WORKER_ERROR);
      assert(record_session_mediacodec_error(s, &saved)
            == (i ? RECORD_MC_SESSION_FILE : RECORD_MC_SESSION_OUTPUT));
      assert(saved == edit_error);
      assert(record_session_mediacodec_free(s));
      balanced();
      trim_error = edit_error = 0;
   }
#ifndef _WIN32
   concurrent_sessions();
#endif
   assert(record_session_mediacodec_free(NULL));
   puts("PASS: recording session startup rollback, real PCM queue, source "
        "ends, "
        "independent GPU retirement, cancellation and ordered file/codec "
        "release; native owners mocked");
   return 0;
}
