/* The shipping watcher's spawn function on both thread backends. */
#include <stdio.h>
#include <retro_atomic.h>

typedef struct { void *imm_thread; } xa_t;
typedef xa_t dsound_t;
typedef struct { retro_atomic_int_t reinit_request; } audio_driver_state_t;
static audio_driver_state_t audio;
static void *captured;
static unsigned starts, failures;
static int refuse, token;

static audio_driver_state_t *audio_state_get_ptr(void) { return &audio; }

#ifdef HAVE_THREADS
static void mmdevice_thread(void *data)
{
   retro_atomic_store_release_int((retro_atomic_int_t*)data, 1);
}
static void *sthread_create(void (*entry)(void*), void *data)
{
   (void)entry;
   starts++;
   captured = data;
   return refuse ? NULL : &token;
}
#else
typedef unsigned long DWORD;
static DWORD mmdevice_thread(void *data)
{
   retro_atomic_store_release_int((retro_atomic_int_t*)data, 1);
   return 0;
}
static void *CreateThread(void *attr, size_t stack,
      DWORD (*entry)(void*), void *data, DWORD flags, DWORD *id)
{
   (void)attr; (void)stack; (void)entry; (void)flags; (void)id;
   starts++;
   captured = data;
   return refuse ? NULL : &token;
}
#endif

#include "driver_start.inc"

#define CHECK(c) do { if (!(c)) { \
   fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #c); failures++; } } while (0)

static void test_start(bool (*start)(xa_t*))
{
   xa_t driver;
   starts = 0;
   driver.imm_thread = NULL;
   retro_atomic_int_init(&audio.reinit_request, 0);
   refuse = 1;
   CHECK(!start(&driver));
   CHECK(driver.imm_thread == NULL && starts == 1);
   CHECK(captured == (void*)&audio.reinit_request);
   refuse = 0;
   CHECK(start(&driver));
   CHECK(driver.imm_thread == &token && starts == 2);
   CHECK(captured == (void*)&audio.reinit_request);
   if (captured == (void*)&audio.reinit_request)
   {
      mmdevice_thread(captured);
      CHECK(retro_atomic_load_acquire_int(&audio.reinit_request) == 1);
      CHECK(driver.imm_thread == &token);
   }
   CHECK(start(&driver));
   CHECK(starts == 2 && driver.imm_thread == &token);
}

int main(void)
{
   test_start(xaudio_imm_start_thread);
   test_start(dsound_imm_start_thread);
   puts(failures ? "FAIL mmdevice_notification_test" : "PASS mmdevice_notification_test");
   return failures ? 1 : 0;
}
