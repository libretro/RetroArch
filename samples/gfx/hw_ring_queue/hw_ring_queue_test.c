/* Production GL ring publication with scripted queues and completed fences. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <features/features_cpu.h>
#include <retro_timers.h>
#include "../../../gfx/video_thread_hw.c"

void RARCH_LOG(const char *fmt, ...) { (void)fmt; }
void RARCH_WARN(const char *fmt, ...) { (void)fmt; }
void RARCH_ERR(const char *fmt, ...) { (void)fmt; }
void video_thread_call_on_waiter(void (*fn)(void*), void *data)
{ (void)fn; (void)data; exit(1); }

static unsigned failures, created, freed, captures;
#define CHECK(c, s) do { if (!(c)) { printf("FAIL: %s\n", s); failures++; } } while (0)

static bool new_fence(void *data, void **fence)
{ (void)data; *fence = (void*)(uintptr_t)++created; return true; }
static void free_fence(void *data, void *fence)
{ (void)data; (void)fence; freed++; }
static bool wait_fence(void *data, void *fence, unsigned us)
{ (void)data; (void)fence; (void)us; return true; }
static bool capture(void *data, unsigned slot, const void *context, unsigned format)
{ (void)data; (void)slot; (void)context; (void)format; captures++; return true; }

typedef struct test_run
{
   thread_video_t *thr;
   retro_atomic_int_t done;
   unsigned taken;
   int published;
} test_run_t;

static void publish(void *data)
{
   test_run_t *t = (test_run_t*)data;
   t->published = video_thread_hw_publish(t->thr, &t->taken);
   retro_atomic_store_release_int(&t->done, 1);
}

static void await_flag(retro_atomic_int_t *flag)
{
   retro_time_t start = cpu_features_get_time_usec();
   while (!retro_atomic_load_acquire_int(flag))
   {
      if (cpu_features_get_time_usec() - start > 5000000)
      {
         fprintf(stderr, "FAIL: queue watchdog\n");
         exit(1);
      }
      retro_sleep(1);
   }
}

static void run_case(int state, int last, bool blocked)
{
   static thread_video_t thr;
   video_poke_interface_t poke;
   hw_ring_t *ring;
   test_run_t t;
   sthread_t *thread;
   memset(&thr, 0, sizeof(thr));
   memset(&poke, 0, sizeof(poke));
   memset(&t, 0, sizeof(t));
   created = freed = captures = 0;
   poke.hw_ring_fence_new = new_fence;
   poke.hw_ring_fence_free = free_fence;
   poke.hw_ring_fence_wait = wait_fence;
   poke.hw_ring_capture = capture;
   thr.poke = &poke;
   if (!retro_eventcount_init(&thr.frame.ring) || !hw_ring_setup(&thr, &ring))
      exit(1);
   ring->api = HW_API_GL;
   ring->index = VIDEO_THREAD_HW_RING - 1;
   ring->gl_queried = true;
   retro_atomic_store_release_int(&ring->last_presented, last);
   thr.frame.slot[0].hw_slot = thr.frame.slot[1].hw_slot = -1;
   thr.frame.slot[0].dupe = true;
   retro_atomic_store_release_int(&thr.frame.state, state);
   t.thr = &thr;
   if (!(thread = sthread_create(publish, &t)))
      exit(1);
   if (blocked)
   {
      await_flag(&thr.frame.ring.waiters);
      retro_sleep(80);
      CHECK(!retro_atomic_load_acquire_int(&t.done),
            "duplicate retains GL slot beyond the short wait");
      retro_atomic_store_release_int(&thr.frame.state, VIDEO_THREAD_RING_MAKE(0, 0, false));
      retro_eventcount_notify(&thr.frame.ring);
   }
   await_flag(&t.done);
   sthread_join(thread);
   CHECK(t.published == VIDEO_THREAD_HW_RING - 1 && !t.taken
         && ring->index == 0 && captures == 1, "publication preserves slot rotation");
   CHECK(!retro_atomic_load_acquire_int(&thr.frame.ring.waiters), "waiter retired");
   video_thread_hw_free(&thr);
   CHECK(!thr.frame.hw_ring && created == freed, "all scripted fences retired");
   retro_eventcount_free(&thr.frame.ring);
}

int main(void)
{
   run_case(VIDEO_THREAD_RING_MAKE(0, 1, false), 0, true);
   run_case(VIDEO_THREAD_RING_MAKE(1, 0, true), 0, true);
   run_case(VIDEO_THREAD_RING_MAKE(0, 2, false), 0, true);
   run_case(VIDEO_THREAD_RING_MAKE(1, 2, false), 0, true);
   run_case(VIDEO_THREAD_RING_MAKE(0, 0, false), 0, false);
   run_case(VIDEO_THREAD_RING_MAKE(0, 1, false), 1, false);
   printf("%s hw_ring_queue_test (%u failures)\n", failures ? "FAIL" : "PASS", failures);
   return failures ? 1 : 0;
}
