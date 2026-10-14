/* The windowed feeder's dead flag, across the two threads that touch it.
 *
 * audio_mixer destroys a voice from the mixer thread as well as the
 * caller's - "Voices are claimed and released from mixer and caller
 * threads", audio_mixer.c - and the borrow's release callback is what
 * sets dead. The feeder task's handler reads it on the task thread.
 * There is no lock on either side and nothing joins between them, so
 * the two are ordered by the field alone.
 *
 * The suite includes the task's translation unit so it can drive that
 * pair directly, the way samples/audio/psp_ring drives the console
 * drivers': the handler is the task's, the release is the mixer's, and
 * both are static. Nothing here reimplements them.
 *
 * Against the volatile bool the flag used to be, ThreadSanitizer reports
 * the read against the write. It is clean as an atomic.
 *
 * A second lane drives the handler's steady state through the stubs: it
 * parks between looks, and not after a lost window or a release. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <rthreads/rthreads.h>
#include <retro_atomic.h>

#include "../../../tasks/task_audio_mixer.c"

#define ROUNDS 2000

extern bool         stub_feed_ok;
extern int64_t      stub_tell;
extern unsigned     stub_removed;
extern unsigned     stub_parks;
extern retro_time_t stub_park_horizon;

/* Once the stream is handed over, the feeder parks after each look: a
 * feed leaves the decoder a full lookahead to eat, and a stopped stream
 * eats nothing.  The park must come back before even a 20 MB/s stream
 * could reach the frontier.  A lost window and a released sound are
 * dealt with at once, without a park. */
static int lane_steady_state_parks(void)
{
   struct audio_mixer_wfeed    w;
   struct audio_mixer_userdata user;
   retro_task_t                task;
   int                         bad = 0;
   unsigned                    parks;

   memset(&w, 0, sizeof(w));
   memset(&user, 0, sizeof(user));
   memset(&task, 0, sizeof(task));
   w.user     = &user;
   w.added    = true;
   w.keep_set = true;
   w.dt       = (struct data_transfer*)&w;   /* stands in for a window */
   retro_atomic_int_init(&w.dead, 0);
   task.state = &w;

   stub_tell    = 4096;
   stub_feed_ok = true;
   stub_parks   = 0;
   task_audio_mixer_handle_wfeed(&task);
   if (stub_parks != 1)
   {
      printf("  FAIL: a fed pass parked %u times, wanted 1\n", stub_parks);
      bad = 1;
   }
   if (     stub_park_horizon <= 0
         || stub_park_horizon > (retro_time_t)(AMIX_WINDOW_LOOKAHEAD / 20))
   {
      printf("  FAIL: a %lld us park outlasts a 20 MB/s stream's lookahead\n",
            (long long)stub_park_horizon);
      bad = 1;
   }

   stub_tell = -1;
   task_audio_mixer_handle_wfeed(&task);
   if (stub_parks != 2)
   {
      printf("  FAIL: a stopped stream's pass did not park\n");
      bad = 1;
   }

   stub_tell    = 4096;
   stub_feed_ok = false;
   stub_removed = 0;
   parks        = stub_parks;
   task_audio_mixer_handle_wfeed(&task);
   if (stub_removed != 1 || stub_parks != parks)
   {
      printf("  FAIL: a lost window removed %u streams and parked %u times\n",
            stub_removed, stub_parks - parks);
      bad = 1;
   }

   task_audio_mixer_wfeed_release(&w);
   task_audio_mixer_handle_wfeed(&task);
   if (     !(task_get_flags(&task) & RETRO_TASK_FLG_FINISHED)
         || stub_parks != parks)
   {
      printf("  FAIL: a released sound did not finish the feeder at once\n");
      bad = 1;
   }

   stub_tell    = -1;
   stub_feed_ok = false;
   if (!bad)
      printf("  ok: the feeder parks between looks, not after a lost window or a release\n");
   return bad;
}

static struct audio_mixer_wfeed  *shared_w;
static retro_atomic_int_t         reader_go;
static retro_atomic_size_t        reads_done;

/* The task thread: the handler, over and over, reading dead each pass. */
static void reader_thread(void *unused)
{
   retro_task_t task;
   (void)unused;
   memset(&task, 0, sizeof(task));
   task.state = shared_w;

   while (retro_atomic_load_acquire_int(&reader_go))
   {
      task_audio_mixer_handle_wfeed(&task);
      retro_atomic_fetch_add_size(&reads_done, 1);
   }
}

int main(void)
{
   struct audio_mixer_wfeed  w;
   struct audio_mixer_userdata user;
   sthread_t *reader;
   unsigned   round;
   int        bad = 0;

   setvbuf(stdout, NULL, _IONBF, 0);
   printf("mixer wfeed: the dead flag across the release and the handler\n");

   for (round = 0; round < ROUNDS; round++)
   {
      memset(&w, 0, sizeof(w));
      memset(&user, 0, sizeof(user));
      w.user = &user;
      w.path = NULL;
      retro_atomic_int_init(&w.dead, 0);
      shared_w = &w;
      retro_atomic_size_init(&reads_done, 0);
      retro_atomic_int_init(&reader_go, 1);

      if (!(reader = sthread_create(reader_thread, NULL)))
      {
         printf("  FAIL: could not start the task thread\n");
         return 1;
      }

      /* Let the handler get in first, then release the sound under it -
       * the mixer thread's write against the task thread's read. */
      while (retro_atomic_load_acquire_size(&reads_done) < 2)
         ;
      task_audio_mixer_wfeed_release(&w);

      retro_atomic_store_release_int(&reader_go, 0);
      sthread_join(reader);

      if (!retro_atomic_load_acquire_int(&w.dead))
      {
         printf("  FAIL: the release did not land (round %u)\n", round);
         bad = 1;
         break;
      }
   }

   if (!bad)
      printf("  ok: %u rounds of the release landing under a running handler\n",
            ROUNDS);
   if (!bad)
      bad = lane_steady_state_parks();
   printf(bad ? "mixer wfeed: FAILED\n" : "mixer wfeed: ok\n");
   return bad;
}
