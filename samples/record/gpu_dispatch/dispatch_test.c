/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../../record/record_gpu_dispatch_d3d12.h"
#if defined(_WIN32) && defined(HAVE_THREADS)
#include <windows.h>
#include <retro_atomic.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL %u: %s\n", (unsigned)__LINE__, #x); abort(); } } while (0)
struct record_gpu_capture_d3d12 { retro_atomic_int_t frames, closed; };
struct record_gpu_session_d3d12
{
   record_gpu_capture_d3d12_t *capture;
   retro_atomic_int_t accepted, consumed;
   unsigned seen, retries;
   bool video_closed, audio_closed, cancelled;
};
static HANDLE start, completion, reset_entered, reset_release, wait_entered, wait_release;
static DWORD controller;
static bool fail_start, signaled_busy, completion_busy, fail_event, fail_thread;
static unsigned retries;
static retro_atomic_int_t calls, freed, seen, busy_gate, pause_reset, pause_wait;
static retro_atomic_int_t idle_waits, timed_waits, completion_waits, fail_wait;
static retro_atomic_int_t gpu_ready, capture_frees, capture_attempts, capture_reaps;
static BOOL WINAPI test_reset(HANDLE h)
{
   BOOL result = ResetEvent(h);
   if (retro_atomic_cas_int(&pause_reset, 1, 0))
   {
      SetEvent(reset_entered);
      CHECK(WaitForSingleObject(reset_release, 10000) == WAIT_OBJECT_0);
   }
   return result;
}
static DWORD WINAPI test_wait(DWORD n, const HANDLE *h, BOOL all, DWORD ms)
{
   if (retro_atomic_cas_int(&fail_wait, 1, 0)) { SetLastError(6); return WAIT_FAILED; }
   if (n == 1 && ms == INFINITE) retro_atomic_fetch_add_int(&idle_waits, 1);
   if (ms == 1) retro_atomic_fetch_add_int(&timed_waits, 1);
   if (n == 2) retro_atomic_fetch_add_int(&completion_waits, 1);
   if (retro_atomic_cas_int(&pause_wait, 1, 0))
   {
      SetEvent(wait_entered);
      CHECK(WaitForSingleObject(wait_release, 10000) == WAIT_OBJECT_0);
   }
   return WaitForMultipleObjects(n, h, all, ms);
}
static HANDLE WINAPI test_event(void *a, BOOL m, BOOL s, const char *n)
{ return fail_event ? NULL : CreateEventA(a, m, s, n); }
static HANDLE WINAPI test_thread(void *a, size_t n, DWORD (WINAPI *run)(void*), void *d, DWORD f, DWORD *id)
{ return fail_thread ? NULL : CreateThread(a, n, run, d, f, id); }
#define ResetEvent test_reset
#define WaitForMultipleObjects test_wait
#undef CreateEvent
#define CreateEvent test_event
#define CreateThread test_thread
#include "../../../record/record_gpu_dispatch_d3d12.c"
#undef ResetEvent
#undef WaitForMultipleObjects
#undef CreateEvent
#undef CreateThread
record_gpu_session_d3d12_t *record_gpu_session_d3d12_new(record_gpu_capture_d3d12_t *c,
      const nvenc_d3d12_config_t *e, const record_gpu_output_config_t *o, const char *path)
{
   record_gpu_session_d3d12_t *s;
   CHECK(GetCurrentThreadId() != controller);
   CHECK(WaitForSingleObject(start, 10000) == WAIT_OBJECT_0);
   CHECK(!strcmp(path, "owned.mp4") && e->fps_num == 60 && o->video.max_packet_size == 65536);
   if (fail_start) return NULL;
   s = (record_gpu_session_d3d12_t*)calloc(1, sizeof(*s)); CHECK(s);
   s->capture = c; s->retries = retries; s->audio_closed = !o->audio_rate;
   retro_atomic_int_init(&s->accepted, 0); retro_atomic_int_init(&s->consumed, 0);
   return s;
}
int record_gpu_session_d3d12_step(record_gpu_session_d3d12_t *s)
{
   int n;
   CHECK(GetCurrentThreadId() != controller);
   retro_atomic_fetch_add_int(&calls, 1);
   if (s->retries) { s->retries--; return s->retries ? RECORD_GPU_OUTPUT_BUSY : RECORD_GPU_OUTPUT_PROGRESS; }
   if (retro_atomic_load_acquire_int(&busy_gate)) return RECORD_GPU_OUTPUT_BUSY;
   n = retro_atomic_load_acquire_int(&s->capture->frames);
   if ((unsigned)n != s->seen)
   { s->seen = (unsigned)n; retro_atomic_store_release_int(&seen, n); return RECORD_GPU_OUTPUT_PROGRESS; }
   n = retro_atomic_load_acquire_int(&s->accepted);
   if (n != retro_atomic_load_acquire_int(&s->consumed))
   { retro_atomic_store_release_int(&s->consumed, n); return RECORD_GPU_OUTPUT_PROGRESS; }
   if (s->video_closed && s->audio_closed)
      return s->cancelled ? RECORD_GPU_OUTPUT_ERROR : RECORD_GPU_OUTPUT_END;
   return RECORD_GPU_OUTPUT_BUSY;
}
void *record_gpu_session_d3d12_wait_handle(const record_gpu_session_d3d12_t *s)
{ return (completion_busy && retro_atomic_load_acquire_int(&busy_gate)) || (signaled_busy && s->retries) ? completion : NULL; }
bool record_gpu_session_d3d12_retry_needed(const record_gpu_session_d3d12_t *s)
{ return s->retries || retro_atomic_load_acquire_int(&busy_gate); }
int record_gpu_session_d3d12_audio(record_gpu_session_d3d12_t *s, const int16_t *p, size_t n)
{
   int accepted = retro_atomic_load_acquire_int(&s->accepted);
   CHECK(p && n == 1);
   if (accepted - retro_atomic_load_acquire_int(&s->consumed) >= 4) return RECORD_GPU_OUTPUT_BUSY;
   retro_atomic_store_release_int(&s->accepted, accepted + 1); return RECORD_GPU_OUTPUT_PROGRESS;
}
bool record_gpu_session_d3d12_video_finish(record_gpu_session_d3d12_t *s)
{ CHECK(retro_atomic_load_acquire_int(&s->capture->closed)); s->video_closed = true; return true; }
bool record_gpu_session_d3d12_audio_finish(record_gpu_session_d3d12_t *s)
{ s->audio_closed = true; return true; }
void record_gpu_session_d3d12_cancel(record_gpu_session_d3d12_t *s) { s->cancelled = true; }
enum record_gpu_session_error record_gpu_session_d3d12_error(const record_gpu_session_d3d12_t *s, uint32_t *f)
{ if (f) *f = 0; return s->cancelled ? RECORD_GPU_SESSION_ERROR_CANCELLED : RECORD_GPU_SESSION_ERROR_NONE; }
bool record_gpu_session_d3d12_free(record_gpu_session_d3d12_t *s)
{
   CHECK(GetCurrentThreadId() != controller && s->video_closed && s->audio_closed);
   free(s); retro_atomic_fetch_add_int(&freed, 1); return true;
}
int record_gpu_capture_d3d12_push(record_gpu_capture_d3d12_t *c, void *p, unsigned state,
      unsigned x, unsigned y, void *f, uint64_t v, uint64_t t, uint64_t d, bool k, uint64_t *r)
{
   (void)p; (void)state; (void)x; (void)y; (void)f; (void)v; (void)d; (void)k;
   CHECK(!retro_atomic_load_acquire_int(&c->closed));
   retro_atomic_fetch_add_int(&c->frames, 1); if (r) *r = t + 1; return RECORD_GPU_QUEUE_OK;
}
bool record_gpu_capture_d3d12_finish(record_gpu_capture_d3d12_t *c)
{ retro_atomic_store_release_int(&c->closed, 1); return true; }
bool record_gpu_capture_d3d12_reap(record_gpu_capture_d3d12_t *c)
{
   CHECK(c && !retro_atomic_load_acquire_int(&c->closed));
   retro_atomic_fetch_add_int(&capture_reaps, 1);
   return retro_atomic_load_acquire_int(&gpu_ready) != 0;
}
bool record_gpu_capture_d3d12_free(record_gpu_capture_d3d12_t *c)
{
   CHECK(GetCurrentThreadId() != controller && retro_atomic_load_acquire_int(&c->closed));
   retro_atomic_fetch_add_int(&capture_attempts, 1);
   if (!retro_atomic_load_acquire_int(&gpu_ready)) return false;
   free(c); retro_atomic_fetch_add_int(&capture_frees, 1); return true;
}
static void setup(record_gpu_capture_d3d12_t *c, nvenc_d3d12_config_t *e, record_gpu_output_config_t *o)
{
   controller = GetCurrentThreadId();
   memset(e, 0, sizeof(*e)); memset(o, 0, sizeof(*o));
   e->fps_num = 60; o->video.max_packet_size = 65536; o->audio_rate = 48000;
   retro_atomic_int_init(&c->frames, 0); retro_atomic_int_init(&c->closed, 0);
   retro_atomic_int_init(&calls, 0); retro_atomic_int_init(&freed, 0); retro_atomic_int_init(&seen, 0);
   retro_atomic_int_init(&busy_gate, 0); retro_atomic_int_init(&pause_reset, 0); retro_atomic_int_init(&pause_wait, 0);
   retro_atomic_int_init(&idle_waits, 0); retro_atomic_int_init(&timed_waits, 0); retro_atomic_int_init(&completion_waits, 0); retro_atomic_int_init(&fail_wait, 0);
   retro_atomic_int_init(&gpu_ready, 1); retro_atomic_int_init(&capture_frees, 0);
   retro_atomic_int_init(&capture_attempts, 0); retro_atomic_int_init(&capture_reaps, 0);
   fail_start = signaled_busy = completion_busy = fail_event = fail_thread = false; retries = 0;
   start = CreateEventA(NULL, TRUE, FALSE, NULL); completion = CreateEventA(NULL, TRUE, FALSE, NULL);
   reset_entered = CreateEventA(NULL, TRUE, FALSE, NULL); reset_release = CreateEventA(NULL, TRUE, FALSE, NULL);
   wait_entered = CreateEventA(NULL, TRUE, FALSE, NULL); wait_release = CreateEventA(NULL, TRUE, FALSE, NULL);
   CHECK(start && completion && reset_entered && reset_release && wait_entered && wait_release);
}
static void cleanup(void)
{ CloseHandle(start); CloseHandle(completion); CloseHandle(reset_entered); CloseHandle(reset_release); CloseHandle(wait_entered); CloseHandle(wait_release); }
static void wait_counter(retro_atomic_int_t *p, int n)
{
   unsigned i;
   for (i = 0; i < 10000 && retro_atomic_load_acquire_int(p) < n; i++) Sleep(1);
   CHECK(i < 10000);
}
static void ready(record_gpu_dispatch_d3d12_t *j)
{
   unsigned i;
   SetEvent(start);
   for (i = 0; i < 10000 && record_gpu_dispatch_d3d12_status(j, NULL, NULL, NULL) == RECORD_GPU_DISPATCH_STARTING; i++) Sleep(1);
   CHECK(i < 10000);
}
static void collect(record_gpu_dispatch_d3d12_t *j, enum record_gpu_dispatch_state expected)
{
   unsigned i;
   for (i = 0; i < 10000; i++)
   {
      enum record_gpu_dispatch_state s = record_gpu_dispatch_d3d12_status(j, NULL, NULL, NULL);
      if (s == RECORD_GPU_DISPATCH_DONE || s == RECORD_GPU_DISPATCH_FAILED) break;
      Sleep(1);
   }
   CHECK(i < 10000 && record_gpu_dispatch_d3d12_status(j, NULL, NULL, NULL) == expected);
   for (i = 0; i < 10000 && !record_gpu_dispatch_d3d12_free(j); i++) Sleep(1);
   CHECK(i < 10000);
}
static void lifecycle(unsigned mode)
{
   record_gpu_capture_d3d12_t c;
   nvenc_d3d12_config_t e;
   record_gpu_output_config_t o;
   record_gpu_dispatch_d3d12_t *j;
   char path[] = "owned.mp4";
   int16_t pcm = 1;
   unsigned i;
   setup(&c, &e, &o);
   fail_start = mode == 1;
   if (mode == 2) retro_atomic_store_release_int(&pause_reset, 1);
   if (mode == 3) retro_atomic_store_release_int(&pause_wait, 1);
   if (mode == 4) retries = 20;
   if (mode == 5) { retries = 20; signaled_busy = true; SetEvent(completion); }
   if (mode == 6 || mode == 7) { completion_busy = true; retro_atomic_store_release_int(&busy_gate, 1); }
   if (mode == 7) retro_atomic_store_release_int(&fail_wait, 1);
   j = record_gpu_dispatch_d3d12_new(&c, &e, &o, path); CHECK(j);
   strcpy(path, "destroyed"); e.fps_num = 1; o.video.max_packet_size = 1;
   CHECK(record_gpu_dispatch_d3d12_audio(j, &pcm, 1) == RECORD_GPU_OUTPUT_BUSY);
   CHECK(record_gpu_dispatch_d3d12_video(j, NULL, 0, 0, 0, NULL, 0, 0, 1, true, NULL) == RECORD_GPU_QUEUE_BUSY);
   CHECK(!record_gpu_dispatch_d3d12_free(j));
   if (mode == 1) { record_gpu_dispatch_d3d12_cancel(j); }
   ready(j);
   if (mode == 1)
   {
      CHECK(record_gpu_dispatch_d3d12_status(j, NULL, NULL, NULL) == RECORD_GPU_DISPATCH_DRAINING);
      CHECK(!record_gpu_dispatch_d3d12_free(j));
   }
   else
   {
      if (mode == 2) CHECK(WaitForSingleObject(reset_entered, 10000) == WAIT_OBJECT_0);
      if (mode == 3) CHECK(WaitForSingleObject(wait_entered, 10000) == WAIT_OBJECT_0);
      if (mode == 0)
      {
         int before;
         wait_counter(&idle_waits, 1); before = retro_atomic_load_acquire_int(&calls);
         Sleep(20); CHECK(before == retro_atomic_load_acquire_int(&calls));
      }
      if (mode == 4 || mode == 5) wait_counter(&timed_waits, 5);
      if (mode == 6) wait_counter(&completion_waits, 1);
      if (mode == 7)
      {
         uint32_t os_error = 0;
         for (i = 0; i < 10000 && !os_error; i++)
         {
            record_gpu_dispatch_d3d12_status(j, NULL, NULL, &os_error);
            if (!os_error) Sleep(1);
         }
         CHECK(os_error == 6);
         record_gpu_dispatch_d3d12_cancel(j);
      }
      for (i = 0; i < 1000; i++)
      {
         int result;
         unsigned attempts = 0;
         if (mode == 7) break;
         CHECK(record_gpu_dispatch_d3d12_video(j, NULL, 0, 0, 0, NULL, 0, i, 1, true, NULL) == RECORD_GPU_QUEUE_OK);
         if (mode == 2 && !i) SetEvent(reset_release);
         if (mode == 3 && !i) SetEvent(wait_release);
         if (mode == 6 && !i) { retro_atomic_store_release_int(&busy_gate, 0); SetEvent(completion); }
         do { result = record_gpu_dispatch_d3d12_audio(j, &pcm, 1); if (!result) Sleep(1); } while (!result && ++attempts < 10000);
         CHECK(result == RECORD_GPU_OUTPUT_PROGRESS);
      }
      if (mode == 7) { retro_atomic_store_release_int(&busy_gate, 0); SetEvent(completion); }
   }
   CHECK(record_gpu_dispatch_d3d12_video_finish(j));
   CHECK(record_gpu_dispatch_d3d12_audio_finish(j));
   collect(j, mode == 1 || mode == 7 ? RECORD_GPU_DISPATCH_FAILED : RECORD_GPU_DISPATCH_DONE);
   CHECK(retro_atomic_load_acquire_int(&freed) == (mode == 1 ? 0 : 1));
   cleanup();
}
struct pcm_task { record_gpu_dispatch_d3d12_t *job; unsigned made; };
static DWORD WINAPI concurrent_pcm(void *opaque)
{
   struct pcm_task *p = (struct pcm_task*)opaque;
   int16_t pcm = 123;
   unsigned idle = 0;
   while (p->made < 2000 && idle < 10000)
   {
      int result = record_gpu_dispatch_d3d12_audio(p->job, &pcm, 1);
      if (result == RECORD_GPU_OUTPUT_ERROR) break;
      if (result == RECORD_GPU_OUTPUT_BUSY) { Sleep(1); idle++; }
      else { p->made++; idle = 0; }
   }
   CHECK(record_gpu_dispatch_d3d12_audio_finish(p->job));
   return 0;
}
static void producers(bool cancel)
{
   record_gpu_capture_d3d12_t c;
   nvenc_d3d12_config_t e;
   record_gpu_output_config_t o;
   struct pcm_task pcm;
   HANDLE thread;
   unsigned i;
   setup(&c, &e, &o);
   pcm.job = record_gpu_dispatch_d3d12_new(&c, &e, &o, "owned.mp4"); CHECK(pcm.job);
   pcm.made = 0; ready(pcm.job);
   thread = CreateThread(NULL, 0, concurrent_pcm, &pcm, 0, NULL); CHECK(thread);
   for (i = 0; i < 5000; i++)
   {
      CHECK(record_gpu_dispatch_d3d12_video(pcm.job, NULL, 0, 0, 0, NULL, 0, i, 1, true, NULL) == RECORD_GPU_QUEUE_OK);
      if (cancel && i == 20) { record_gpu_dispatch_d3d12_cancel(pcm.job); break; }
   }
   CHECK(record_gpu_dispatch_d3d12_video_finish(pcm.job));
   CHECK(WaitForSingleObject(thread, 10000) == WAIT_OBJECT_0); CloseHandle(thread);
   if (!cancel) CHECK(pcm.made == 2000);
   collect(pcm.job, cancel ? RECORD_GPU_DISPATCH_FAILED : RECORD_GPU_DISPATCH_DONE);
   CHECK(retro_atomic_load_acquire_int(&freed) == 1); cleanup();
}
static void startup_boundaries(void)
{
   record_gpu_capture_d3d12_t c;
   nvenc_d3d12_config_t e;
   record_gpu_output_config_t o;
   record_gpu_dispatch_d3d12_t *j;
   setup(&c, &e, &o);
   fail_event = true;
   CHECK(!record_gpu_dispatch_d3d12_new(&c, &e, &o, "owned.mp4"));
   CHECK(!record_gpu_dispatch_d3d12_new_owned(&c, &e, &o, "owned.mp4"));
   fail_event = false; fail_thread = true;
   CHECK(!record_gpu_dispatch_d3d12_new(&c, &e, &o, "owned.mp4"));
   CHECK(!record_gpu_dispatch_d3d12_new_owned(&c, &e, &o, "owned.mp4"));
   CHECK(!retro_atomic_load_acquire_int(&capture_attempts));
   CHECK(!retro_atomic_load_acquire_int(&c.closed));
   fail_thread = false;
   j = record_gpu_dispatch_d3d12_new(&c, &e, &o, "owned.mp4"); CHECK(j);
   CHECK(record_gpu_dispatch_d3d12_video_finish(j) && record_gpu_dispatch_d3d12_audio_finish(j));
   SetEvent(start); collect(j, RECORD_GPU_DISPATCH_DONE); cleanup();
   setup(&c, &e, &o); o.audio_rate = 0;
   j = record_gpu_dispatch_d3d12_new(&c, &e, &o, "owned.mp4"); CHECK(j);
   ready(j); CHECK(record_gpu_dispatch_d3d12_video_finish(j));
   collect(j, RECORD_GPU_DISPATCH_DONE); cleanup();
}
static void owned_lifecycle(unsigned mode)
{
   record_gpu_capture_d3d12_t *c = (record_gpu_capture_d3d12_t*)calloc(1, sizeof(*c));
   nvenc_d3d12_config_t e;
   record_gpu_output_config_t o;
   record_gpu_dispatch_d3d12_t *j;
   int count;
   CHECK(c);
   setup(c, &e, &o);
   fail_start = mode == 1 || mode == 4;
   if (!mode) o.audio_rate = 0;
   j = record_gpu_dispatch_d3d12_new_owned(c, &e, &o, "owned.mp4"); CHECK(j);
   CHECK(record_gpu_dispatch_d3d12_video_reap(j));
   retro_atomic_store_release_int(&gpu_ready, 0);
   CHECK(!record_gpu_dispatch_d3d12_video_reap(j));
   if (mode == 1) record_gpu_dispatch_d3d12_cancel(j);
   ready(j);
   if (!fail_start)
      CHECK(record_gpu_dispatch_d3d12_video(j, NULL, 0, 0, 0, NULL, 0, 0, 1, true, NULL) == RECORD_GPU_QUEUE_OK);
   if (mode == 2) record_gpu_dispatch_d3d12_cancel(j);
   CHECK(record_gpu_dispatch_d3d12_video_finish(j));
   count = retro_atomic_load_acquire_int(&capture_reaps);
   CHECK(!record_gpu_dispatch_d3d12_video_reap(j));
   CHECK(count == retro_atomic_load_acquire_int(&capture_reaps));
   if (mode)
   {
      Sleep(10);
      CHECK(!retro_atomic_load_acquire_int(&capture_attempts));
      CHECK(record_gpu_dispatch_d3d12_audio_finish(j));
   }
   wait_counter(&capture_attempts, 5);
   CHECK(record_gpu_dispatch_d3d12_status(j, NULL, NULL, NULL) == RECORD_GPU_DISPATCH_DRAINING);
   CHECK(!retro_atomic_load_acquire_int(&capture_frees));
   CHECK(!record_gpu_dispatch_d3d12_free(j));
   if (mode == 5) record_gpu_dispatch_d3d12_cancel(j);
   retro_atomic_store_release_int(&gpu_ready, 1);
   collect(j, mode == 1 || mode == 2 || mode == 4 || mode == 5 ? RECORD_GPU_DISPATCH_FAILED : RECORD_GPU_DISPATCH_DONE);
   CHECK(retro_atomic_load_acquire_int(&capture_frees) == 1);
   CHECK(retro_atomic_load_acquire_int(&freed) == (fail_start ? 0 : 1));
   cleanup();
}
int main(void)
{
   unsigned i;
   for (i = 0; i < 8; i++) lifecycle(i);
   startup_boundaries(); producers(false); producers(true);
   for (i = 0; i < 6; i++) owned_lifecycle(i);
   puts("PASS: dispatcher startup, idle/completion/retry waits, closure races and retirement");
   return 0;
}
#else
#include "../../../record/record_gpu_dispatch_d3d12.c"
int main(void)
{
   uint64_t ready_value = 123;
   enum record_gpu_session_error error;
   uint32_t file_error = 123, system_error = 123;
   if (record_gpu_dispatch_d3d12_new(NULL, NULL, NULL, NULL) || !record_gpu_dispatch_d3d12_free(NULL)) return 1;
   if (record_gpu_dispatch_d3d12_new_owned(NULL, NULL, NULL, NULL)
         || record_gpu_dispatch_d3d12_video_reap(NULL)) return 1;
   if (record_gpu_dispatch_d3d12_video(NULL, NULL, 0, 0, 0, NULL, 0, 0, 1, false, &ready_value) != RECORD_GPU_QUEUE_ERROR || ready_value) return 1;
   if (record_gpu_dispatch_d3d12_audio(NULL, NULL, 0) != RECORD_GPU_OUTPUT_ERROR
         || record_gpu_dispatch_d3d12_video_finish(NULL) || record_gpu_dispatch_d3d12_audio_finish(NULL)) return 1;
   record_gpu_dispatch_d3d12_cancel(NULL);
   if (record_gpu_dispatch_d3d12_status(NULL, &error, &file_error, &system_error) != RECORD_GPU_DISPATCH_FAILED
         || error != RECORD_GPU_SESSION_ERROR_STARTUP || file_error || system_error) return 1;
   puts("PASS: unsupported/threadless dispatcher is unavailable"); return 0;
}
#endif
