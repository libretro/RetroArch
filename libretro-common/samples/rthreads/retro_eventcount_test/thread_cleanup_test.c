/* SPDX-License-Identifier: MIT */
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(_WIN32)
#include <windows.h>
#elif defined(RETRO_EVENTCOUNT_FORCE_SEM)
#include <pthread.h>
#include <errno.h>
static int fail_key_store;
static int cleanup_key_store(pthread_key_t key, const void *value)
{
   if (fail_key_store)
   {
      fail_key_store = 0;
      return ENOMEM;
   }
   return pthread_setspecific(key, value);
}
#define pthread_setspecific cleanup_key_store
#endif
#include "../../../rthreads/retro_eventcount.c"
#if defined(RETRO_EVENTCOUNT_FORCE_SEM) && !defined(_WIN32)
#undef pthread_setspecific
#endif
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "thread cleanup line %u\n", (unsigned)__LINE__); return 1; } } while (0)

int main(void)
{
   retro_eventcount_t event;
   unsigned i;
#if defined(_WIN32) && defined(RETRO_EC_ADDR_WIN32)
   typedef BOOL (WINAPI *get_count_t)(HANDLE, PDWORD);
   get_count_t get_count;
   DWORD before, after, event_slot;
   HANDLE cached;
   bool hires;
   int key;
#endif
   retro_eventcount_thread_cleanup();
#if defined(_WIN32) && defined(RETRO_EC_ADDR_WIN32)
   CHECK(_putenv("RETRO_EVENTCOUNT_WIN32=event") == 0);
#endif
   CHECK(retro_eventcount_init(&event));
#if defined(_WIN32) && defined(RETRO_EC_ADDR_WIN32)
   CHECK(!strcmp(retro_eventcount_backend_name(), "win32 event"));
   get_count = (get_count_t)(void (*)(void))GetProcAddress(
         GetModuleHandleA("kernel32.dll"), "GetProcessHandleCount");
   CHECK(get_count);
   CHECK(ec_thread_event());
   if (ec_g.tls_timer != TLS_OUT_OF_INDEXES)
      CHECK(ec_thread_timer());
   retro_eventcount_thread_cleanup();
   CHECK(get_count(GetCurrentProcess(), &before));
   event_slot = ec_g.tls_event;
   hires = ec_g.hires;
   ec_g.tls_event = TLS_OUT_OF_INDEXES;
   ec_g.hires = false;
   key = retro_eventcount_prepare_wait(&event);
   retro_eventcount_commit_wait_timeout(&event, key, 1);
   ec_g.tls_event = event_slot;
   ec_g.hires = hires;
   CHECK(get_count(GetCurrentProcess(), &after) && after == before);
   for (i = 0; i < 128; i++)
   {
      cached = ec_thread_event();
      CHECK(cached && ec_thread_event() == cached);
      if (ec_g.tls_timer != TLS_OUT_OF_INDEXES)
      {
         cached = ec_thread_timer();
         CHECK(cached && ec_thread_timer() == cached);
      }
      retro_eventcount_thread_cleanup();
      retro_eventcount_thread_cleanup();
      CHECK(!TlsGetValue(ec_g.tls_event));
      if (ec_g.tls_timer != TLS_OUT_OF_INDEXES)
         CHECK(!TlsGetValue(ec_g.tls_timer));
      CHECK(get_count(GetCurrentProcess(), &after));
      CHECK(after == before);
   }
#elif defined(RETRO_EC_SEM) && !defined(RETRO_EC_SEM_POOL)
   CHECK(ec_g.ready);
#if defined(RETRO_EVENTCOUNT_FORCE_SEM)
   fail_key_store = 1;
   CHECK(!ec_sem_get());
   CHECK(!pthread_getspecific(ec_g.key));
#endif
   for (i = 0; i < 128; i++)
   {
      ec_sem_t *cached = ec_sem_get();
      CHECK(cached && ec_sem_get() == cached);
      retro_eventcount_thread_cleanup();
      retro_eventcount_thread_cleanup();
      CHECK(!pthread_getspecific(ec_g.key));
   }
#else
   for (i = 0; i < 128; i++)
      retro_eventcount_thread_cleanup();
#endif
   retro_eventcount_free(&event);
   retro_eventcount_thread_cleanup();
   puts("thread_cleanup_test: cached resources released, repeat cleanup and reuse passed");
   return 0;
}
