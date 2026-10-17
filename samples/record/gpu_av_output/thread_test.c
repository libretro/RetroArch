/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "mock_worker.h"
#include "mock_file.h"
#include "../../../record/record_gpu_output_d3d12.c"
#ifdef _WIN32
#include <windows.h>
#define YIELD() Sleep(1)
#else
#include <pthread.h>
#include <unistd.h>
#define YIELD() usleep(1000)
#endif
static const uint8_t headers[] = {0,0,0,1,0x67,0x42,0,0x1e,0x80,0,0,0,1,0x68,0x80};
struct producer { record_gpu_output_d3d12_t *p; retro_atomic_int_t stop; unsigned made; };
#ifdef _WIN32
static DWORD WINAPI produce(void *opaque)
#else
static void *produce(void *opaque)
#endif
{
   struct producer *s = (struct producer*)opaque;
   int16_t *samples = (int16_t*)calloc(1024 * 2, sizeof(*samples));
   unsigned idle = 0;
   if (!samples) abort();
   while (s->made < 40003 && idle < 10000 && !retro_atomic_load_acquire_int(&s->stop))
   {
      size_t n = 1 + (s->made * 17u) % 1024;
      int result;
      if (n > 40003 - s->made) n = 40003 - s->made;
      result = record_gpu_output_d3d12_audio(s->p, samples, n);
      if (result == RECORD_GPU_OUTPUT_ERROR) break;
      if (result == RECORD_GPU_OUTPUT_BUSY) { idle++; YIELD(); }
      else { s->made += (unsigned)n; idle = 0; }
   }
   free(samples); record_gpu_output_d3d12_audio_finish(s->p);
   return 0;
}
int main(void)
{
   record_gpu_output_config_t c;
   record_gpu_worker_d3d12_t w;
   struct producer s;
   unsigned i;
   int result = RECORD_GPU_OUTPUT_BUSY, ok;
#ifdef _WIN32
   HANDLE thread;
#else
   pthread_t thread;
#endif
   memset(&c, 0, sizeof(c)); memset(&w, 0, sizeof(w)); memset(&s, 0, sizeof(s));
   c.video.headers = headers; c.video.headers_size = sizeof(headers);
   c.video.max_packet_size = 65536; c.video.width = c.video.height = 64;
   c.video.timescale = 60; c.video.bit_depth = 8;
   c.audio_rate = 44100; c.audio_layout = 3; c.audio_kbps = 192; c.audio_fifo_frames = 1536;
   w.frames = 4; w.duration = 1; file_delay = 1;
   s.p = record_gpu_output_d3d12_new(&w, &c, "unused"); if (!s.p) return 1;
   retro_atomic_int_init(&s.stop, 0);
#ifdef _WIN32
   thread = CreateThread(NULL, 0, produce, &s, 0, NULL); if (!thread) return 1;
#else
   if (pthread_create(&thread, NULL, produce, &s)) return 1;
#endif
   for (i = 0; i < 1000000; i++)
   {
      result = record_gpu_output_d3d12_step(s.p);
      if (result == RECORD_GPU_OUTPUT_END || result == RECORD_GPU_OUTPUT_ERROR) break;
      if (result == RECORD_GPU_OUTPUT_BUSY) YIELD();
   }
   retro_atomic_store_release_int(&s.stop, 1);
#ifdef _WIN32
   if (WaitForSingleObject(thread, 10000) != WAIT_OBJECT_0) abort();
   CloseHandle(thread);
#else
   pthread_join(thread, NULL);
#endif
   ok = result == RECORD_GPU_OUTPUT_END && s.made == 40003 && s.p->audio_pts == 41472 && w.made == w.frames;
   if (!record_gpu_output_d3d12_free(s.p)) abort();
   puts(ok ? "PASS: concurrent PCM close/drain, whole-batch backpressure and wrapped ring" : "FAIL: PCM concurrency");
   return !ok;
}
