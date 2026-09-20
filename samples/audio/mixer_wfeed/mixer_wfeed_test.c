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
 * the read against the write. It is clean as an atomic. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <rthreads/rthreads.h>
#include <retro_atomic.h>

#include "../../../tasks/task_audio_mixer.c"

#define ROUNDS 2000

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
   printf(bad ? "mixer wfeed: FAILED\n" : "mixer wfeed: ok\n");
   return bad;
}
