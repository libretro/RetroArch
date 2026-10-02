/* The ffmpeg core's wait for a finished video slot, and the decode
 * thread ending under it.
 *
 * The decode thread ends at the end of the file. The main thread, at
 * the end of a clip, can be waiting for the tail slot to be finished
 * when it does - a slot the thread will now never claim. The wait has
 * to end when the thread does, rather than hold retro_run() forever.
 *
 * The core is included rather than linked, for its statics: the video
 * buffer, the context, and the decode thread's exit routine are driven
 * directly, with no media involved. */

#include <stdio.h>
#include <time.h>

#include "../../../cores/libretro-ffmpeg/ffmpeg_core.c"

static unsigned failures;
#define CHECK(cond, ...) \
   do { if (!(cond)) { printf("FAIL line %d: ", __LINE__); \
        printf(__VA_ARGS__); printf("\n"); failures++; } } while (0)

static retro_atomic_int_t waited_out;
static bool               wait_result;

static void waiter(void *unused)
{
   (void)unused;
   wait_result = video_buffer_wait_for_finished_slot(VIDEO_BUFFER_STR,
         &g_ctx.decode_thread_dead);
   retro_atomic_store_release_int(&waited_out, 1);
}

static void sleep_ms(long ms)
{
   struct timespec d;
   d.tv_sec  = ms / 1000;
   d.tv_nsec = (ms % 1000) * 1000000L;
   nanosleep(&d, NULL);
}

int main(void)
{
   sthread_t *t;
   int i;

   FIFO_LOCK_STR    = slock_new();
   FIFO_COND_STR    = scond_new();
   VIDEO_BUFFER_STR = video_buffer_create(4, 64 * 64 * 4, 64, 64);
   if (!FIFO_LOCK_STR || !FIFO_COND_STR || !VIDEO_BUFFER_STR)
   {
      printf("FAIL: fixture\n");
      return 1;
   }
   DECODE_THREAD_DEAD_SET(0);

   /* The main thread waits on a tail slot nobody has claimed. */
   retro_atomic_store_release_int(&waited_out, 0);
   t = sthread_create(waiter, NULL);
   sleep_ms(50);
   CHECK(!retro_atomic_load_acquire_int(&waited_out),
         "the wait returned with the thread alive and no slot finished");

   /* The decode thread reaches the end of the file. */
   decode_thread_mark_dead();
   for (i = 0; i < 1000 && !retro_atomic_load_acquire_int(&waited_out); i++)
      sleep_ms(1);
   CHECK(retro_atomic_load_acquire_int(&waited_out),
         "the wait for a finished slot outlived the decode thread");
   if (!retro_atomic_load_acquire_int(&waited_out))
   {
      printf("%u failure(s)\n", failures);
      return 1;   /* the waiter is stuck; do not join it */
   }
   sthread_join(t);
   CHECK(!wait_result, "the wait reported a slot finished that was not");

   /* A thread already gone is answered without waiting at all. */
   retro_atomic_store_release_int(&waited_out, 0);
   waiter(NULL);
   CHECK(retro_atomic_load_acquire_int(&waited_out) && !wait_result,
         "a wait after the thread ended did not return at once");

   video_buffer_destroy(VIDEO_BUFFER_STR);
   scond_free(FIFO_COND_STR);
   slock_free(FIFO_LOCK_STR);

   if (failures)
   {
      printf("%u failure(s)\n", failures);
      return 1;
   }
   printf("[pass] ffmpeg_finished_wait_test\n");
   return 0;
}
