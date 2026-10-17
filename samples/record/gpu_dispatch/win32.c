/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
/* Test-only pthread implementation; no production Win32 emulation. */
#include "win32/windows.h"
#include <pthread.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
struct test_handle
{
   pthread_t thread;
   DWORD (*run)(void*);
   void *data;
   int is_thread, signaled, manual;
};
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t changed = PTHREAD_COND_INITIALIZER;
static __thread DWORD last_error;
static void *run_thread(void *opaque)
{
   struct test_handle *h = (struct test_handle*)opaque;
   h->run(h->data);
   pthread_mutex_lock(&lock); h->signaled = 1;
   pthread_cond_broadcast(&changed); pthread_mutex_unlock(&lock);
   return NULL;
}
HANDLE CreateThread(void *a, size_t n, DWORD (*run)(void*), void *data, DWORD f, DWORD *id)
{
   struct test_handle *h = (struct test_handle*)calloc(1, sizeof(*h));
   (void)a; (void)n; (void)f; (void)id;
   if (!h) return NULL;
   h->is_thread = 1; h->manual = 1; h->run = run; h->data = data;
   if (pthread_create(&h->thread, NULL, run_thread, h)) { free(h); return NULL; }
   return h;
}
HANDLE CreateEvent(void *a, BOOL manual, BOOL signaled, const char *name)
{
   struct test_handle *h = (struct test_handle*)calloc(1, sizeof(*h));
   (void)a; (void)name;
   if (h) { h->manual = manual; h->signaled = signaled; }
   return h;
}
BOOL SetEvent(HANDLE handle)
{
   struct test_handle *h = (struct test_handle*)handle;
   if (!h) { last_error = 6; return 0; }
   pthread_mutex_lock(&lock); h->signaled = 1;
   pthread_cond_broadcast(&changed); pthread_mutex_unlock(&lock); return 1;
}
BOOL ResetEvent(HANDLE handle)
{
   struct test_handle *h = (struct test_handle*)handle;
   if (!h) { last_error = 6; return 0; }
   pthread_mutex_lock(&lock); h->signaled = 0; pthread_mutex_unlock(&lock); return 1;
}
DWORD WaitForMultipleObjects(DWORD count, const HANDLE *handles, BOOL all, DWORD ms)
{
   struct timespec end;
   unsigned i;
   int error = 0;
   DWORD result = WAIT_TIMEOUT;
   (void)all;
   if (!count || count > 2) { last_error = 87; return WAIT_FAILED; }
   clock_gettime(CLOCK_REALTIME, &end);
   end.tv_sec += ms / 1000; end.tv_nsec += (ms % 1000) * 1000000L;
   if (end.tv_nsec >= 1000000000L) { end.tv_nsec -= 1000000000L; end.tv_sec++; }
   pthread_mutex_lock(&lock);
   for (;;)
   {
      for (i = 0; i < count; i++)
      {
         struct test_handle *h = (struct test_handle*)handles[i];
         if (!h) { last_error = 6; result = WAIT_FAILED; goto done; }
         if (h->signaled)
         {
            if (!h->manual) h->signaled = 0;
            result = WAIT_OBJECT_0 + i; goto done;
         }
      }
      if (!ms || error == ETIMEDOUT) break;
      error = ms == INFINITE ? pthread_cond_wait(&changed, &lock)
         : pthread_cond_timedwait(&changed, &lock, &end);
   }
done:
   pthread_mutex_unlock(&lock); return result;
}
DWORD WaitForSingleObject(HANDLE h, DWORD ms) { return WaitForMultipleObjects(1, &h, 0, ms); }
BOOL CloseHandle(HANDLE handle)
{
   struct test_handle *h = (struct test_handle*)handle;
   if (!h) return 0;
   if (h->is_thread) pthread_join(h->thread, NULL);
   free(h); return 1;
}
DWORD GetLastError(void) { return last_error; }
void SetLastError(DWORD e) { last_error = e; }
DWORD GetCurrentThreadId(void) { return (DWORD)(uintptr_t)pthread_self(); }
void Sleep(DWORD ms) { usleep(ms * 1000); }
