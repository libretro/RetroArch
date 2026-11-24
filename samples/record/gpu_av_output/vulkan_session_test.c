/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "vulkan_mock_file.h"
#include "../../../record/record_gpu_session_vulkan.c"
#include "../../../record/record_gpu_output_vulkan.c"

struct vulkan_encode_h264
{
   vulkan_encode_pool_t pool;
   vulkan_encode_packet_t packet[16];
   unsigned head, count;
   bool locked, closed;
};
static bool open_fail, headers_fail, gpu_ready, release_fail;
/* GPU work held in flight from another thread, waits on it, and polls */
static retro_atomic_int_t gpu_busy, gpu_waits, gpu_polls;
static unsigned opens, frees, drains, submits, releases;
static void (*session_open_observer)(void);
static void (*session_free_observer)(void);
static const uint8_t session_headers[] = {0,0,0,1,0x67,0x42,0,0x1e,0x80,
   0,0,0,1,0x68,0x80};
static const uint8_t session_idr[] = {0,0,0,1,0x65,0x80};
static const uint8_t session_p[] = {0,0,0,1,0x41,0x80};

vulkan_encode_h264_t *vulkan_encode_h264_open(const vulkan_encode_h264_open_t *c,
      const vulkan_encode_h264_ops_t *ops)
{
   vulkan_encode_h264_t *e;
   assert(c && ops);
   if (session_open_observer)
      session_open_observer();
   if (open_fail)
      return NULL;
   e = (vulkan_encode_h264_t*)calloc(1, sizeof(*e));
   assert(e);
   e->pool.count = c->slots;
   opens++;
   return e;
}
const vulkan_encode_pool_t *vulkan_encode_h264_pool(const vulkan_encode_h264_t *e)
{
   return e ? &e->pool : NULL;
}
bool vulkan_encode_h264_headers(vulkan_encode_h264_t *e, void *data,
      size_t capacity, size_t *size)
{
   assert(e && capacity >= sizeof(session_headers));
   if (headers_fail)
      return false;
   memcpy(data, session_headers, sizeof(session_headers));
   *size = sizeof(session_headers);
   return true;
}
int vulkan_encode_h264_submit(vulkan_encode_h264_t *e, unsigned slot,
      uint64_t pts, uint64_t duration, uint64_t ready, bool keyframe)
{
   vulkan_encode_packet_t *p;
   assert(!e->closed && ready && slot < e->pool.count);
   if (e->count == e->pool.count)
      return VULKAN_ENCODE_BUSY;
   p = &e->packet[(e->head + e->count++) % 16];
   p->data = keyframe ? session_idr : session_p;
   p->size = sizeof(session_idr);
   p->timestamp = pts;
   p->duration = duration;
   p->slot = slot;
   p->keyframe = keyframe;
   submits++;
   return VULKAN_ENCODE_OK;
}
int vulkan_encode_h264_poll(vulkan_encode_h264_t *e, vulkan_encode_packet_t *packet)
{
   if (!e->count)
      return e->closed ? VULKAN_ENCODE_END : VULKAN_ENCODE_BUSY;
   retro_atomic_fetch_add_int(&gpu_polls, 1);
   if (!gpu_ready || retro_atomic_load_acquire_int(&gpu_busy))
      return VULKAN_ENCODE_BUSY;
   *packet = e->packet[e->head];
   e->locked = true;
   return VULKAN_ENCODE_OK;
}
bool vulkan_encode_h264_release(vulkan_encode_h264_t *e)
{
   if (release_fail)
      return false;
   assert(e->locked && e->count);
   e->locked = false;
   e->head = (e->head + 1) % 16;
   e->count--;
   releases++;
   return true;
}
int vulkan_encode_h264_finish(vulkan_encode_h264_t *e)
{
   e->closed = true;
   return VULKAN_ENCODE_OK;
}
bool vulkan_encode_h264_drain(vulkan_encode_h264_t *e)
{
   drains++;
   if (!e)
      return true;
   e->closed = true;
   if (e->locked || (e->count && !gpu_ready))
      return false;
   e->count = 0;
   return true;
}
bool vulkan_encode_h264_free(vulkan_encode_h264_t *e)
{
   if (!e)
      return true;
   if (e->count || e->locked)
      return false;
   if (session_free_observer)
      session_free_observer();
   frees++;
   free(e);
   return true;
}
static record_gpu_session_vulkan_t *session_new(bool audio)
{
#ifdef SESSION_HEVC_TEST
   return test_hevc_session_new(audio);
#else
   vulkan_encode_h264_open_t c;
   record_gpu_output_vulkan_config_t output;
   static const vulkan_encode_h264_ops_t ops = { {0}, {0}, {0} };
   memset(&c, 0, sizeof(c));
   memset(&output, 0, sizeof(output));
   c.slots = 2;
   c.session.video.width = c.session.video.height = 64;
   c.session.video.fps_num = 30000;
   c.session.video.fps_den = 1001;
   output.video.max_packet_size = 65536;
   if (audio)
   {
      output.audio_rate = 48000;
      output.audio_layout = 3;
      output.audio_kbps = 192;
      output.audio_fifo_frames = 1536;
   }
   return record_gpu_session_vulkan_new(&c, &ops, &output, "unused");
#endif
}
static void reset(void)
{
   assert(opens == frees);
   open_fail = headers_fail = release_fail = false;
   session_open_observer = NULL;
   session_free_observer = NULL;
   gpu_ready = true;
   retro_atomic_int_init(&gpu_busy, 0);
   retro_atomic_int_init(&gpu_waits, 0);
   retro_atomic_int_init(&gpu_polls, 0);
   file_open_fail = file_push_fail = file_step_fail = file_finish_fail = file_cancel_fail = false;
   file_delay = file_writes = submits = releases = drains = 0;
   file_chunked = false;
}
static void publish(record_gpu_queue_t *q, unsigned frame)
{
   record_gpu_frame_t f;
   assert(record_gpu_queue_acquire(q, &f.slot) == RECORD_GPU_QUEUE_OK);
   f.timestamp = frame * 1001;
   f.duration = 1001;
   f.ready_value = frame + 1;
   f.keyframe = !frame;
   assert(record_gpu_queue_submit(q, &f));
}
static int terminal(record_gpu_session_vulkan_t *s)
{
   unsigned i;
   int result;
   for (i = 0; i < 1000; i++)
   {
      result = record_gpu_session_vulkan_step(s);
      if (result == RECORD_VULKAN_OUTPUT_END || result == RECORD_VULKAN_OUTPUT_ERROR)
         return result;
   }
   assert(!"session did not retire within fixture budget");
   return 0;
}
static uint32_t read32(const uint8_t *p)
{
   return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}
static void initialization(record_gpu_session_vulkan_t *s, bool audio)
{
   const uint8_t *p;
   size_t i, size;
   unsigned tracks = 0, colours = 0;
   assert(record_gpu_session_vulkan_step(s) == RECORD_VULKAN_OUTPUT_PROGRESS);
   p = (const uint8_t*)s->output->file->data;
   size = s->output->file->size;
   assert(p && size > 100 && !memcmp(p + 4, "ftyp", 4));
   for (i = 4; i + 20 <= size; i++)
   {
      if (!memcmp(p + i, "mdhd", 4))
      {
         assert(!p[i + 4] && tracks < (audio ? 2u : 1u));
         assert(read32(p + i + 16) == (tracks ? 48000u : 30000u));
         tracks++;
      }
      if (!memcmp(p + i, "colr", 4))
      {
         static const uint8_t colour[] = {'n','c','l','x',0,1,0,1,0,1,0};
         assert(!memcmp(p + i + 4, colour, sizeof(colour)));
         colours++;
      }
   }
   assert(tracks == (audio ? 2u : 1u) && colours == 1);
}
static void normal(bool audio)
{
   record_gpu_session_vulkan_t *s;
   record_gpu_queue_t *q;
   int16_t samples[4] = {32767,-32768,123,-456};
   unsigned i;
   reset();
   file_delay = 3;
   s = session_new(audio);
   assert(s && record_gpu_session_vulkan_error(s, NULL) == RECORD_VULKAN_SESSION_NONE);
   assert(!record_gpu_session_vulkan_free(s));
   q = record_gpu_session_vulkan_queue(s);
   assert(q && record_gpu_session_vulkan_pool(s)->count == 2);
   initialization(s, audio);
   publish(q, 0);
   publish(q, 1);
   gpu_ready = false;
   for (i = 0; i < 20; i++)
      assert(record_gpu_session_vulkan_step(s) != RECORD_VULKAN_OUTPUT_ERROR);
   assert(submits == 2 && !releases && !drains);
   assert(record_gpu_session_vulkan_retry_needed(s));
   if (audio)
      assert(record_gpu_session_vulkan_audio(s, samples, 2) == RECORD_VULKAN_OUTPUT_PROGRESS);
   assert(record_gpu_session_vulkan_audio_finish(s));
   assert(record_gpu_session_vulkan_video_finish(s));
   assert(!record_gpu_session_vulkan_queue(s) && !record_gpu_session_vulkan_pool(s));
   for (i = 0; i < 10; i++)
      assert(record_gpu_session_vulkan_step(s) != RECORD_VULKAN_OUTPUT_END);
   assert(!record_gpu_session_vulkan_free(s));
   gpu_ready = true;
   assert(terminal(s) == RECORD_VULKAN_OUTPUT_END);
   assert(releases == 2 && opens == frees && file_writes == (audio ? 4u : 3u));
   assert(record_gpu_session_vulkan_video_finish(s));
   assert(!record_gpu_session_vulkan_retry_needed(s));
   assert(record_gpu_session_vulkan_audio_frames(s) == (audio ? 2u : 0u));
   assert(record_gpu_session_vulkan_free(s));
}
static void cancellation(void)
{
   record_gpu_session_vulkan_t *s;
   unsigned i;
   reset();
   s = session_new(true);
   assert(s);
   publish(record_gpu_session_vulkan_queue(s), 0);
   release_fail = true;
   for (i = 0; i < 20 && record_gpu_session_vulkan_error(s, NULL) == RECORD_VULKAN_SESSION_NONE; i++)
      record_gpu_session_vulkan_step(s);
   assert(record_gpu_session_vulkan_error(s, NULL) == RECORD_VULKAN_SESSION_OUTPUT);
   assert(((vulkan_encode_h264_t*)s->encoder)->locked);
   record_gpu_session_vulkan_cancel(s);
   assert(record_gpu_session_vulkan_audio_finish(s));
   for (i = 0; i < 10; i++)
      assert(record_gpu_session_vulkan_step(s) == RECORD_VULKAN_OUTPUT_BUSY);
   assert(!drains && !record_gpu_session_vulkan_retry_needed(s));
   assert(record_gpu_session_vulkan_video_finish(s));
   assert(record_gpu_session_vulkan_audio_finish(s));
   for (i = 0; i < 10; i++)
      assert(record_gpu_session_vulkan_step(s) != RECORD_VULKAN_OUTPUT_ERROR);
   assert(drains && s->encoder && !record_gpu_session_vulkan_free(s));
   release_fail = false;
   assert(terminal(s) == RECORD_VULKAN_OUTPUT_ERROR);
   assert(opens == frees && record_gpu_session_vulkan_free(s));
}
static void failures(void)
{
   record_gpu_session_vulkan_t *s;
   record_gpu_queue_t *q;
   unsigned mode, slot;
   int error;
   for (mode = 0; mode < 3; mode++)
   {
      reset();
      open_fail = mode == 0;
      headers_fail = mode == 1;
      file_open_fail = mode == 2;
      s = session_new(true);
      assert(s && record_gpu_session_vulkan_error(s, NULL) == RECORD_VULKAN_SESSION_STARTUP);
      record_gpu_session_vulkan_cancel(s);
      assert(record_gpu_session_vulkan_video_finish(s));
      assert(record_gpu_session_vulkan_audio_finish(s));
      assert(terminal(s) == RECORD_VULKAN_OUTPUT_ERROR);
      assert(record_gpu_session_vulkan_error(s, NULL) == RECORD_VULKAN_SESSION_STARTUP);
      assert(opens == frees && record_gpu_session_vulkan_free(s));
   }
   reset();
   s = session_new(false);
   assert(s);
   q = record_gpu_session_vulkan_queue(s);
   assert(record_gpu_queue_acquire(q, &slot) == RECORD_GPU_QUEUE_OK);
   record_gpu_session_vulkan_cancel(s);
   assert(!record_gpu_session_vulkan_video_finish(s));
   assert(record_gpu_queue_cancel(q));
   assert(record_gpu_session_vulkan_video_finish(s));
   assert(terminal(s) == RECORD_VULKAN_OUTPUT_ERROR);
   assert(record_gpu_session_vulkan_error(s, NULL) == RECORD_VULKAN_SESSION_CANCELLED);
   assert(record_gpu_session_vulkan_free(s));
   reset();
   s = session_new(false);
   assert(s);
   file_push_fail = true;
   assert(record_gpu_session_vulkan_step(s) == RECORD_VULKAN_OUTPUT_PROGRESS);
   assert(record_gpu_session_vulkan_video_finish(s));
   assert(terminal(s) == RECORD_VULKAN_OUTPUT_ERROR);
   assert(record_gpu_session_vulkan_error(s, &error) == RECORD_VULKAN_SESSION_OUTPUT && error == 112);
   assert(opens == frees && record_gpu_session_vulkan_free(s));
}
#ifdef HAVE_THREADS
#ifdef _WIN32
#include <windows.h>
#define SESSION_YIELD() Sleep(1)
#define SESSION_THREAD_RESULT DWORD WINAPI
#else
#include <pthread.h>
#include <time.h>
static void session_yield(void)
{
   struct timespec delay;
   delay.tv_sec = 0;
   delay.tv_nsec = 1000000;
   nanosleep(&delay, NULL);
}
#define SESSION_YIELD() session_yield()
#define SESSION_THREAD_RESULT void *
#endif
/* Blocks while the GPU work is held, as a fence wait does */
bool vulkan_encode_h264_wait(vulkan_encode_h264_t *e, uint64_t timeout_ns)
{
   unsigned naps = 0;
   (void)timeout_ns;
   if (!e->count || e->locked || !retro_atomic_load_acquire_int(&gpu_busy))
      return false;
   retro_atomic_fetch_add_int(&gpu_waits, 1);
   while (retro_atomic_load_acquire_int(&gpu_busy) && naps++ < 5000)
      SESSION_YIELD();
   return true;
}
struct session_producer
{
   record_gpu_session_vulkan_t *session;
   unsigned made;
};
static SESSION_THREAD_RESULT video_produce(void *opaque)
{
   struct session_producer *p = (struct session_producer*)opaque;
   record_gpu_queue_t *q = record_gpu_session_vulkan_queue(p->session);
   unsigned idle = 0;
   while (p->made < 64 && idle < 10000 && record_gpu_session_vulkan_pool(p->session))
   {
      record_gpu_frame_t frame;
      int result = record_gpu_queue_acquire(q, &frame.slot);
      assert(result != RECORD_GPU_QUEUE_ERROR);
      if (result == RECORD_GPU_QUEUE_BUSY)
      {
         idle++;
         SESSION_YIELD();
         continue;
      }
      if (!record_gpu_session_vulkan_pool(p->session))
      {
         assert(record_gpu_queue_cancel(q));
         break;
      }
      frame.timestamp = p->made * 1001;
      frame.duration = 1001;
      frame.ready_value = p->made + 1;
      frame.keyframe = !p->made;
      assert(record_gpu_queue_submit(q, &frame));
      p->made++;
      idle = 0;
   }
   assert(record_gpu_session_vulkan_video_finish(p->session));
   return 0;
}
static SESSION_THREAD_RESULT audio_produce(void *opaque)
{
   struct session_producer *p = (struct session_producer*)opaque;
   int16_t *samples = (int16_t*)calloc(2048, sizeof(*samples));
   unsigned idle = 0;
   assert(samples);
   while (p->made < 40003 && idle < 10000)
   {
      size_t frames = 40003 - p->made;
      int result;
      if (frames > 1024)
         frames = 1024;
      result = record_gpu_session_vulkan_audio(p->session, samples, frames);
      if (result == RECORD_VULKAN_OUTPUT_ERROR)
         break;
      if (result == RECORD_VULKAN_OUTPUT_BUSY)
      {
         idle++;
         SESSION_YIELD();
      }
      else
      {
         p->made += (unsigned)frames;
         idle = 0;
      }
   }
   free(samples);
   assert(record_gpu_session_vulkan_audio_finish(p->session));
   return 0;
}
static void concurrent(bool cancel)
{
   struct session_producer video, audio;
   unsigned i;
   int result = RECORD_VULKAN_OUTPUT_BUSY;
#ifdef _WIN32
   HANDLE threads[2];
#else
   pthread_t threads[2];
#endif
   reset();
   file_delay = 1;
   video.session = audio.session = session_new(true);
   assert(video.session);
   video.made = audio.made = 0;
#ifdef _WIN32
   threads[0] = CreateThread(NULL, 0, video_produce, &video, 0, NULL);
   threads[1] = CreateThread(NULL, 0, audio_produce, &audio, 0, NULL);
   assert(threads[0] && threads[1]);
#else
   assert(!pthread_create(&threads[0], NULL, video_produce, &video));
   assert(!pthread_create(&threads[1], NULL, audio_produce, &audio));
#endif
   for (i = 0; i < 1000000; i++)
   {
      if (cancel && submits >= 3)
         record_gpu_session_vulkan_cancel(video.session);
      result = record_gpu_session_vulkan_step(video.session);
      if (result == RECORD_VULKAN_OUTPUT_END || result == RECORD_VULKAN_OUTPUT_ERROR)
         break;
      if (result == RECORD_VULKAN_OUTPUT_BUSY)
         SESSION_YIELD();
   }
   assert(i < 1000000);
#ifdef _WIN32
   assert(WaitForMultipleObjects(2, threads, TRUE, 10000) == WAIT_OBJECT_0);
   assert(CloseHandle(threads[0]) && CloseHandle(threads[1]));
#else
   assert(!pthread_join(threads[0], NULL) && !pthread_join(threads[1], NULL));
#endif
   assert(result == (cancel ? RECORD_VULKAN_OUTPUT_ERROR : RECORD_VULKAN_OUTPUT_END));
   if (!cancel)
      assert(video.made == 64 && audio.made == 40003 && submits == releases && submits == 64);
   assert(opens == frees && record_gpu_session_vulkan_free(video.session));
   puts("Vulkan session: independent video/PCM producers, I/O owner and closure PASS");
}
#else
bool vulkan_encode_h264_wait(vulkan_encode_h264_t *e, uint64_t timeout_ns)
{
   (void)e;
   (void)timeout_ns;
   return false;
}
#endif
int main(void)
{
   normal(false);
   normal(true);
   cancellation();
   failures();
#ifdef HAVE_THREADS
   concurrent(false);
   concurrent(true);
#endif
   puts("Vulkan recording session: queue/GPU/file leases, producer close, cancellation and startup retirement PASS");
   return 0;
}
