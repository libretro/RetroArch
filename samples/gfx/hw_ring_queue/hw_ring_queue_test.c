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


static void signal_fence(void *data, void *fence) { (void)data; (void)fence; }

static bool gpu_ready = true;
static unsigned polls;
static bool poll_fence(void *data, void *fence, unsigned us)
{
   (void)data; (void)fence;
   CHECK(us == 0, "slot readiness never blocks");
   polls++;
   return gpu_ready;
}

static void run_case(int state, int last, int expected)
{
   static thread_video_t thr;
   video_poke_interface_t poke;
   hw_ring_t *ring;
   unsigned taken = 99;
   int published;
   memset(&thr, 0, sizeof(thr));
   memset(&poke, 0, sizeof(poke));
   created = freed = captures = polls = 0;
   poke.hw_ring_fence_new = new_fence;
   poke.hw_ring_fence_free = free_fence;
   poke.hw_ring_fence_wait = poll_fence;
   poke.hw_ring_capture = capture;
   thr.poke = &poke;
   if (!retro_eventcount_init(&thr.frame.ring) || !hw_ring_setup(&thr, &ring))
      exit(1);
   ring->api = HW_API_GL;
   ring->index = 2;
   ring->gl_queried = true;
   retro_atomic_store_release_int(&ring->last_presented, last);
   thr.frame.slot[0].hw_slot = 0;
   thr.frame.slot[1].hw_slot = 1;
   retro_atomic_store_release_int(&thr.frame.state, state);
   /* Both candidates have GPU work left from an earlier presentation. */
   hw_fence_arm(&ring->slot[0].fence, signal_fence, NULL, NULL, 0);
   hw_fence_arm(&ring->slot[0].fence, signal_fence, NULL, NULL, 0);
   hw_fence_arm(&ring->slot[1].fence, signal_fence, NULL, NULL, 0);
   CHECK(!polls, "superseded disposable fences never wait");
   published = video_thread_hw_publish(&thr, &taken);
   CHECK(!taken, "publication never takes queued slots back");
   CHECK(!retro_atomic_load_acquire_int(&thr.frame.ring.waiters), "publication never parks");
   if (expected < 0)
      CHECK(published == VIDEO_THREAD_HW_BUSY && ring->index == 2 && !captures,
            "busy ring retains the producer's slot without publishing");
   else
      CHECK(published == 2 && ring->index == (unsigned)expected && captures == 1,
            "publication advances only to a completed unqueued slot");
   CHECK(polls <= 2, "bounded readiness checks");
   gpu_ready = true;
   poke.hw_ring_fence_wait = wait_fence;
   video_thread_hw_free(&thr);
   CHECK(!thr.frame.hw_ring && created == freed, "all scripted fences retired");
   retro_eventcount_free(&thr.frame.ring);
}

int main(void)
{
   run_case(VIDEO_THREAD_RING_MAKE(0, 1, false), 0, 1);
   run_case(VIDEO_THREAD_RING_MAKE(1, 0, true), 0, 1);
   run_case(VIDEO_THREAD_RING_MAKE(0, 2, false), -1, -1);
   run_case(VIDEO_THREAD_RING_MAKE(0, 0, false), 1, 0);
   gpu_ready = false;
   run_case(VIDEO_THREAD_RING_MAKE(0, 0, false), -1, -1);
   printf("%s hw_ring_queue_test (%u failures)\n", failures ? "FAIL" : "PASS", failures);
   return failures ? 1 : 0;
}
