/* Test-only synchronization. Hold the SDK inside Init while the video
 * owner presents native frames, queries readiness and closes content.
 * Deadlines prevent a broken worker from hanging the test runner. */
#include <retro_atomic.h>
#include <retro_timers.h>
#include <rthreads/rthreads.h>
#include <rthreads/retro_eventcount.h>

typedef struct async_test_gate
{
   retro_eventcount_t wake;
   retro_atomic_int_t entered;
   retro_atomic_int_t released;
   retro_atomic_int_t timed_out;
} async_test_gate_t;

static bool async_test_wait(retro_eventcount_t *wake, retro_atomic_int_t *flag)
{
   while (!retro_atomic_load_acquire_int(flag))
   {
      int key = retro_eventcount_prepare_wait(wake);
      if (retro_atomic_load_acquire_int(flag))
         retro_eventcount_cancel_wait(wake);
      else if (!retro_eventcount_commit_wait_timeout(wake, key, 5000000))
         return false;
   }
   return true;
}

static void async_test_hold(void *data)
{
   async_test_gate_t *gate = (async_test_gate_t*)data;
   retro_atomic_store_release_int(&gate->entered, 1);
   retro_eventcount_notify(&gate->wake);
   if (!async_test_wait(&gate->wake, &gate->released))
      retro_atomic_store_release_int(&gate->timed_out, 1);
}

static void async_test_release(async_test_gate_t *gate)
{
   retro_atomic_store_release_int(&gate->released, 1);
   retro_eventcount_notify(&gate->wake);
}

static bool async_test_idle(void)
{
   unsigned i;
   /* Test watchdog only, never used by a video/core callback. */
   for (i = 0; i < 5000 && video_upscaler_async_busy(); i++)
      retro_sleep(1);
   return !video_upscaler_async_busy();
}

static bool async_test_borrow_init(void *data)
{
   (void)data;
   return true;
}

static void async_test_borrow_destroy(void *data)
{
   *(bool*)data = true;
}

static video_upscaler_async_t *async_test_parked;
static bool async_test_parked_destroyed;
static unsigned async_test_retire_calls;

static void async_test_retire_parked(void)
{
   async_test_retire_calls++;
   video_upscaler_async_free(async_test_parked);
   async_test_parked = NULL;
}

static bool async_test_park_cache(void)
{
   async_test_parked_destroyed = false;
   async_test_retire_calls = 0;
   async_test_parked = video_upscaler_async_new(&async_test_parked_destroyed,
         async_test_borrow_init, NULL, async_test_borrow_destroy);
   if (!async_test_parked)
      return false;
   video_upscaler_set_cached_retire(async_test_retire_parked);
   return true;
}

static bool async_test_exclusive_borrow(void)
{
   bool destroyed = false;
   bool first, second;
   video_upscaler_async_t *async = video_upscaler_async_new(&destroyed,
         async_test_borrow_init, NULL, async_test_borrow_destroy);
   if (!async)
      return false;
   video_upscaler_async_start(async);
   if (!async_test_idle())
      return false;
   first  = video_upscaler_async_enter(async);
   second = video_upscaler_async_enter(async);
   if (second)
      video_upscaler_async_leave();
   if (first)
      video_upscaler_async_leave();
   video_upscaler_async_free(async);
   return async_test_idle() && destroyed && first && !second;
}
