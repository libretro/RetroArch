/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mock_file.h"
#include "../../../record/record_gpu_session_d3d12.h"
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL %u: %s\n", (unsigned)__LINE__, #x); return 1; } } while (0)
static const uint8_t sequence[] = {0,0,0,1,0x67,0x42,0,0x1e,0x80,0,0,0,1,0x68,0x80};
static bool open_fault, headers_fault, free_blocked, removed, poll_error;
static unsigned opened, freed;
struct nvenc_d3d12
{
   nvenc_d3d12_packet_t packets[16];
   void *fence;
   uint64_t value;
   unsigned read, count, submits, polls, releases, finishes;
   int submit_fault, poll_fault, release_fault, finish_fault;
   bool done, locked, eos;
};

int nvenc_d3d12_submit(nvenc_d3d12_t *e, unsigned slot, uint64_t timestamp,
      uint64_t duration, void *fence, uint64_t value, bool keyframe)
{
   nvenc_d3d12_packet_t *p;
   e->submits++;
   if (e->submit_fault)
   {
      int fault = e->submit_fault;
      e->submit_fault = 0;
      return fault;
   }
   if (e->eos || !duration || e->count == 16)
      return NVENC_D3D12_ERROR;
   p = &e->packets[(e->read + e->count++) % 16];
   p->data = "\0\0\0\1\x65\x88"; p->size = 6;
   p->slot = slot; p->timestamp = timestamp;
   p->duration = duration; p->keyframe = keyframe;
   e->fence = fence; e->value = value;
   return NVENC_D3D12_OK;
}
int nvenc_d3d12_poll(nvenc_d3d12_t *e, nvenc_d3d12_packet_t *packet)
{
   e->polls++;
   if (poll_error) return NVENC_D3D12_ERROR;
   if (e->poll_fault)
   {
      int fault = e->poll_fault;
      e->poll_fault = 0;
      return fault;
   }
   if (e->locked)
      return NVENC_D3D12_ERROR;
   if (!e->count)
      return e->eos && e->done ? NVENC_D3D12_END : NVENC_D3D12_BUSY;
   if (!e->done)
      return NVENC_D3D12_BUSY;
   *packet = e->packets[e->read]; e->locked = true;
   return NVENC_D3D12_OK;
}
int nvenc_d3d12_release(nvenc_d3d12_t *e)
{
   e->releases++;
   if (e->release_fault)
   {
      e->release_fault = 0;
      return NVENC_D3D12_ERROR;
   }
   if (!e->locked)
      return NVENC_D3D12_ERROR;
   e->count--; e->read = (e->read + 1) % 16; e->locked = false;
   return NVENC_D3D12_OK;
}
void *nvenc_d3d12_wait_handle(const nvenc_d3d12_t *e)
{ return e && !e->locked && (e->count || e->eos) ? (void*)e : NULL; }
int nvenc_d3d12_finish(nvenc_d3d12_t *e)
{
   e->finishes++;
   if (e->finish_fault)
   {
      int fault = e->finish_fault;
      e->finish_fault = 0;
      return fault;
   }
   e->eos = true;
   e->done = true;
   return NVENC_D3D12_OK;
}


struct record_gpu_capture_d3d12 { record_gpu_queue_t *queue; };
bool record_gpu_capture_d3d12_bind(record_gpu_capture_d3d12_t *c, nvenc_d3d12_config_t *n)
{ n->slots = 2; n->width = n->height = 64; n->format = NVENC_D3D12_BGRA8; return c != NULL; }
record_gpu_queue_t *record_gpu_capture_d3d12_queue(record_gpu_capture_d3d12_t *c)
{ return c->queue; }
void *record_gpu_capture_d3d12_fence(record_gpu_capture_d3d12_t *c)
{ return c; }
bool record_gpu_capture_d3d12_finish(record_gpu_capture_d3d12_t *c)
{ return record_gpu_queue_finish(c->queue); }
nvenc_d3d12_t *nvenc_d3d12_open(const nvenc_d3d12_config_t *c)
{
   nvenc_d3d12_t *e;
   (void)c;
   if (open_fault) return NULL;
   e = (nvenc_d3d12_t*)calloc(1, sizeof(*e));
   if (e) { e->done = true; opened++; }
   return e;
}
bool nvenc_d3d12_headers(nvenc_d3d12_t *e, void *data, size_t capacity, size_t *size)
{
   (void)e;
   if (headers_fault || capacity < sizeof(sequence)) return false;
   memcpy(data, sequence, sizeof(sequence)); *size = sizeof(sequence); return true;
}
bool nvenc_d3d12_free(nvenc_d3d12_t *e)
{
   if (!e) return true;
   if (free_blocked || (!removed && (e->count || e->locked || (e->eos && !e->done)))) return false;
   free(e); freed++; return true;
}
static void setup(record_gpu_output_config_t *o, nvenc_d3d12_config_t *e)
{
   memset(o, 0, sizeof(*o)); memset(e, 0, sizeof(*e));
   o->video.timescale = 60; o->video.max_packet_size = 65536;
   o->audio_rate = 48000; o->audio_kbps = 192; o->audio_layout = 3; o->audio_fifo_frames = 1536;
   e->fps_num = 60; e->fps_den = 1;
   open_fault = headers_fault = free_blocked = removed = poll_error = false;
   file_delay = file_writes = 0;
   file_open_fail = file_push_fail = file_step_fail = file_finish_fail = file_cancel_fail = false;
   opened = freed = 0;
}
static bool publish(record_gpu_capture_d3d12_t *c, unsigned timestamp)
{
   record_gpu_frame_t f;
   if (record_gpu_queue_acquire(c->queue, &f.slot) != RECORD_GPU_QUEUE_OK) return false;
   f.timestamp = timestamp; f.duration = 1; f.ready_value = timestamp + 1; f.keyframe = true;
   return record_gpu_queue_submit(c->queue, &f);
}
static int drain(record_gpu_session_d3d12_t *s)
{
   unsigned i;
   int result = RECORD_GPU_OUTPUT_BUSY;
   for (i = 0; i < 1000; i++)
   {
      result = record_gpu_session_d3d12_step(s);
      if (result == RECORD_GPU_OUTPUT_END || result == RECORD_GPU_OUTPUT_ERROR) return result;
   }
   return result;
}
static int normal(void)
{
   record_gpu_output_config_t o;
   nvenc_d3d12_config_t e;
   record_gpu_capture_d3d12_t c;
   record_gpu_session_d3d12_t *s;
   int16_t pcm[2] = {123,-456};
   unsigned slot;
   setup(&o, &e); c.queue = record_gpu_queue_new(2); CHECK(c.queue);
   CHECK(!record_gpu_session_d3d12_new(NULL, &e, &o, "unused"));
   o.video.color_primaries = 9;
   CHECK(!record_gpu_session_d3d12_new(&c, &e, &o, "unused"));
   o.video.color_primaries = 0; o.video.transfer_characteristics = 16;
   CHECK(!record_gpu_session_d3d12_new(&c, &e, &o, "unused"));
   o.video.transfer_characteristics = 0; o.video.matrix_coefficients = 9;
   CHECK(!record_gpu_session_d3d12_new(&c, &e, &o, "unused"));
   o.video.matrix_coefficients = 0; o.video.full_range = true;
   CHECK(!record_gpu_session_d3d12_new(&c, &e, &o, "unused"));
   o.video.full_range = false;
   s = record_gpu_session_d3d12_new(&c, &e, &o, "unused"); CHECK(s);
   CHECK(!record_gpu_session_d3d12_free(s));
   CHECK(record_gpu_session_d3d12_error(s, NULL) == RECORD_GPU_SESSION_ERROR_NONE);
   CHECK(record_gpu_queue_acquire(c.queue, &slot) == RECORD_GPU_QUEUE_OK);
   CHECK(!record_gpu_session_d3d12_video_finish(s));
   CHECK(record_gpu_queue_cancel(c.queue));
   CHECK(publish(&c, 0) && publish(&c, 1));
   CHECK(record_gpu_session_d3d12_audio(s, pcm, 1) == RECORD_GPU_OUTPUT_PROGRESS);
   CHECK(record_gpu_session_d3d12_video_finish(s) && record_gpu_session_d3d12_video_finish(s));
   CHECK(record_gpu_session_d3d12_audio_finish(s) && record_gpu_session_d3d12_audio_finish(s));
   CHECK(record_gpu_session_d3d12_audio(s, pcm, 1) == RECORD_GPU_OUTPUT_ERROR);
   CHECK(drain(s) == RECORD_GPU_OUTPUT_END && opened == 1 && freed == 1);
   CHECK(record_gpu_session_d3d12_step(s) == RECORD_GPU_OUTPUT_END);
   CHECK(!record_gpu_session_d3d12_retry_needed(s) && !record_gpu_session_d3d12_wait_handle(s));
   CHECK(record_gpu_session_d3d12_free(s) && record_gpu_queue_free(c.queue));
   return 0;
}
static int cancel(void)
{
   record_gpu_output_config_t o;
   nvenc_d3d12_config_t e;
   record_gpu_capture_d3d12_t c;
   record_gpu_session_d3d12_t *s;
   unsigned i;
   setup(&o, &e); c.queue = record_gpu_queue_new(2); CHECK(c.queue);
   file_delay = 3;
   s = record_gpu_session_d3d12_new(&c, &e, &o, "unused"); CHECK(s);
   CHECK(publish(&c, 0));
   CHECK(record_gpu_session_d3d12_step(s) == RECORD_GPU_OUTPUT_PROGRESS);
   record_gpu_session_d3d12_cancel(s);
   file_cancel_fail = true;
   CHECK(record_gpu_session_d3d12_step(s) == RECORD_GPU_OUTPUT_BUSY);
   CHECK(record_gpu_session_d3d12_wait_handle(s) && record_gpu_session_d3d12_retry_needed(s));
   CHECK(record_gpu_session_d3d12_error(s, NULL) == RECORD_GPU_SESSION_ERROR_CANCELLED);
   file_cancel_fail = false;
   for (i = 0; i < 8; i++) CHECK(record_gpu_session_d3d12_step(s) == RECORD_GPU_OUTPUT_BUSY);
   CHECK(!record_gpu_session_d3d12_retry_needed(s) && !freed);
   CHECK(record_gpu_session_d3d12_audio_finish(s));
   CHECK(record_gpu_session_d3d12_step(s) == RECORD_GPU_OUTPUT_BUSY && !record_gpu_session_d3d12_free(s));
   CHECK(record_gpu_session_d3d12_video_finish(s));
   CHECK(drain(s) == RECORD_GPU_OUTPUT_ERROR && opened == freed);
   CHECK(record_gpu_session_d3d12_free(s) && record_gpu_queue_free(c.queue));
   return 0;
}
static int failures(void)
{
   record_gpu_output_config_t o;
   nvenc_d3d12_config_t e;
   record_gpu_capture_d3d12_t c;
   record_gpu_session_d3d12_t *s;
   unsigned mode, i;
   uint32_t detail;
   for (mode = 0; mode < 6; mode++)
   {
      setup(&o, &e); c.queue = record_gpu_queue_new(2); CHECK(c.queue);
      open_fault = mode == 0; headers_fault = mode == 1; file_open_fail = mode == 2;
      s = record_gpu_session_d3d12_new(&c, &e, &o, "unused"); CHECK(s);
      if (mode < 3) CHECK(record_gpu_session_d3d12_error(s, NULL) == RECORD_GPU_SESSION_ERROR_STARTUP);
      if (mode == 3) file_step_fail = true;
      if (mode >= 4)
      {
         CHECK(publish(&c, 0));
         if (mode == 5)
         {
            CHECK(record_gpu_session_d3d12_step(s) == RECORD_GPU_OUTPUT_PROGRESS);
            CHECK(record_gpu_session_d3d12_step(s) == RECORD_GPU_OUTPUT_PROGRESS);
            CHECK(record_gpu_session_d3d12_step(s) == RECORD_GPU_OUTPUT_PROGRESS);
         }
         poll_error = true;
      }
      CHECK(record_gpu_session_d3d12_audio_finish(s) && record_gpu_session_d3d12_video_finish(s));
      free_blocked = true;
      if (opened)
      {
         for (i = 0; i < 40; i++) CHECK(record_gpu_session_d3d12_step(s) >= RECORD_GPU_OUTPUT_BUSY);
         CHECK(!record_gpu_session_d3d12_free(s) && freed == 0 && record_gpu_session_d3d12_retry_needed(s));
      }
      if (mode == 3)
      {
         CHECK(record_gpu_session_d3d12_error(s, NULL) == RECORD_GPU_SESSION_ERROR_OUTPUT);
         record_gpu_session_d3d12_cancel(s);
         CHECK(record_gpu_session_d3d12_step(s) == RECORD_GPU_OUTPUT_BUSY);
         CHECK(record_gpu_session_d3d12_error(s, NULL) == RECORD_GPU_SESSION_ERROR_OUTPUT);
      }
      free_blocked = false; removed = true;
      CHECK(drain(s) == RECORD_GPU_OUTPUT_ERROR && opened == freed);
      CHECK(record_gpu_session_d3d12_error(s, &detail) != RECORD_GPU_SESSION_ERROR_NONE);
      if (mode == 3) CHECK(detail != 0);
      CHECK(record_gpu_session_d3d12_free(s) && record_gpu_queue_free(c.queue));
   }
   return 0;
}
static int video_only(void)
{
   record_gpu_output_config_t o;
   nvenc_d3d12_config_t e;
   record_gpu_capture_d3d12_t c;
   record_gpu_session_d3d12_t *s;
   int16_t pcm[2] = {0,0};
   setup(&o, &e); c.queue = record_gpu_queue_new(2); CHECK(c.queue);
   o.audio_rate = o.audio_layout = o.audio_kbps = 0; o.audio_fifo_frames = 0;
   s = record_gpu_session_d3d12_new(&c, &e, &o, "unused"); CHECK(s);
   CHECK(record_gpu_session_d3d12_audio(s, pcm, 1) == RECORD_GPU_OUTPUT_ERROR);
   CHECK(publish(&c, 0) && record_gpu_session_d3d12_video_finish(s));
   CHECK(drain(s) == RECORD_GPU_OUTPUT_END && opened == freed);
   CHECK(record_gpu_session_d3d12_free(s) && record_gpu_queue_free(c.queue));
   return 0;
}
#ifdef HAVE_THREADS
#include <retro_atomic.h>
#ifdef _WIN32
#include <windows.h>
#define SESSION_YIELD() Sleep(1)
#else
#include <pthread.h>
#include <unistd.h>
#define SESSION_YIELD() usleep(1000)
#endif
struct session_producer
{
   record_gpu_session_d3d12_t *session;
   retro_atomic_int_t stop;
   unsigned made;
};
#ifdef _WIN32
static DWORD WINAPI produce_pcm(void *opaque)
#else
static void *produce_pcm(void *opaque)
#endif
{
   struct session_producer *p = (struct session_producer*)opaque;
   int16_t *pcm = (int16_t*)calloc(1024 * 2, sizeof(*pcm));
   unsigned idle = 0;
   if (!pcm) abort();
   while (p->made < 40003 && idle < 10000 && !retro_atomic_load_acquire_int(&p->stop))
   {
      size_t n = 1 + p->made % 1024;
      int result;
      if (n > 40003 - p->made) n = 40003 - p->made;
      result = record_gpu_session_d3d12_audio(p->session, pcm, n);
      if (result == RECORD_GPU_OUTPUT_ERROR) break;
      if (result == RECORD_GPU_OUTPUT_BUSY) { idle++; SESSION_YIELD(); }
      else { p->made += (unsigned)n; idle = 0; }
   }
   free(pcm);
   record_gpu_session_d3d12_audio_finish(p->session);
   return 0;
}
static int concurrent(void)
{
   record_gpu_output_config_t o;
   nvenc_d3d12_config_t e;
   record_gpu_capture_d3d12_t c;
   struct session_producer p;
   unsigned i;
   int result = RECORD_GPU_OUTPUT_BUSY;
#ifdef _WIN32
   HANDLE thread;
#else
   pthread_t thread;
#endif
   setup(&o, &e); c.queue = record_gpu_queue_new(2); CHECK(c.queue);
   p.session = record_gpu_session_d3d12_new(&c, &e, &o, "unused"); CHECK(p.session);
   p.made = 0;
   retro_atomic_int_init(&p.stop, 0);
   CHECK(publish(&c, 0) && record_gpu_session_d3d12_video_finish(p.session));
#ifdef _WIN32
   thread = CreateThread(NULL, 0, produce_pcm, &p, 0, NULL); CHECK(thread);
#else
   CHECK(!pthread_create(&thread, NULL, produce_pcm, &p));
#endif
   for (i = 0; i < 1000000; i++)
   {
      result = record_gpu_session_d3d12_step(p.session);
      if (result == RECORD_GPU_OUTPUT_END || result == RECORD_GPU_OUTPUT_ERROR) break;
      if (result == RECORD_GPU_OUTPUT_BUSY) SESSION_YIELD();
   }
   retro_atomic_store_release_int(&p.stop, 1);
#ifdef _WIN32
   if (WaitForSingleObject(thread, 10000) != WAIT_OBJECT_0) abort();
   CloseHandle(thread);
#else
   pthread_join(thread, NULL);
#endif
   CHECK(result == RECORD_GPU_OUTPUT_END && p.made == 40003 && opened == freed);
   CHECK(record_gpu_session_d3d12_free(p.session) && record_gpu_queue_free(c.queue));
   return 0;
}
#endif
int main(void)
{
   CHECK(!record_gpu_session_d3d12_wait_handle(NULL));
   CHECK(!record_gpu_session_d3d12_retry_needed(NULL));
   CHECK(record_gpu_session_d3d12_free(NULL));
   if (normal() || video_only() || cancel() || failures()) return 1;
#ifdef HAVE_THREADS
   if (concurrent()) return 1;
#endif
   puts("PASS: recording session close, cancellation, partial startup and ordered retirement");
   return 0;
}
