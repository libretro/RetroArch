/* The video processor core's audio capture handle, between the
 * frontend's audio thread and the main thread's (re)configuration.
 *
 * audio_callback() runs on the frontend's audio thread and reads from
 * the ALSA capture handle; open_audio_device() and close_audio_device()
 * run on the main thread when a device is chosen, swapped or the game
 * unloaded, and RetroArch ignores SET_AUDIO_CALLBACK(NULL), so the
 * callback keeps running through all of it. A reader thread calls the
 * callback back to back while the main thread opens and closes the
 * device 200 times:
 *
 *   - no snd_pcm_readi() is made on a handle that has been closed
 *     (snd_pcm_open, snd_pcm_readi and snd_pcm_close are wrapped: the
 *     read is a stand-in that takes a capture period's time, and must
 *     name the handle open now, which a close marks closed before it
 *     goes - a later open may well return the same address);
 *   - the callback takes no lock: pthread_mutex_lock is wrapped and
 *     counted on the reader thread;
 *   - capture happens: the callback reads while the device is open,
 *     and hands each read to the frontend.
 *
 * Under TSan, a handle read without the ordering the use word gives
 * is a reported race. Includes the core for its statics; the device is
 * ALSA's "null" PCM. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <stdatomic.h>

#include "../../../cores/libretro-video-processor/video_processor_v4l2.c"

static unsigned failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { printf("FAIL: "); printf(__VA_ARGS__); printf("\n"); failures++; } } while (0)

/* ---- the wrapped ALSA calls ---- */

/* The handle open now, and whether its close has begun. */
static void *_Atomic live;
static _Atomic int   live_closed = 1;
static _Atomic unsigned reads_after_close;
static _Atomic unsigned reads;

int __real_snd_pcm_open(snd_pcm_t **pcm, const char *name,
      snd_pcm_stream_t stream, int mode);
int __wrap_snd_pcm_open(snd_pcm_t **pcm, const char *name,
      snd_pcm_stream_t stream, int mode)
{
   int rc = __real_snd_pcm_open(pcm, name, stream, mode);
   if (rc == 0)
   {
      atomic_store(&live, (void*)*pcm);
      atomic_store(&live_closed, 0);
   }
   return rc;
}

int __real_snd_pcm_close(snd_pcm_t *pcm);
int __wrap_snd_pcm_close(snd_pcm_t *pcm)
{
   if (atomic_load(&live) == (void*)pcm)
      atomic_store(&live_closed, 1);
   return __real_snd_pcm_close(pcm);
}

snd_pcm_sframes_t __wrap_snd_pcm_readi(snd_pcm_t *pcm, void *buf,
      snd_pcm_uframes_t frames)
{
   struct timespec ts = { 0, 200000 };
   if (atomic_load(&live) != (void*)pcm || atomic_load(&live_closed))
      atomic_fetch_add(&reads_after_close, 1);
   /* A capture period's wait, then a period of silence. The handle has
    * to stay open across the wait too: a close that lands in the middle
    * of a read is the one the use word is for. */
   nanosleep(&ts, NULL);
   if (atomic_load(&live) != (void*)pcm || atomic_load(&live_closed))
      atomic_fetch_add(&reads_after_close, 1);
   memset(buf, 0, frames * 4);
   atomic_fetch_add(&reads, 1);
   return (snd_pcm_sframes_t)frames;
}

static _Thread_local int on_reader;
static _Atomic unsigned reader_locks;
int __real_pthread_mutex_lock(pthread_mutex_t *m);
int __wrap_pthread_mutex_lock(pthread_mutex_t *m)
{
   if (on_reader)
      atomic_fetch_add(&reader_locks, 1);
   return __real_pthread_mutex_lock(m);
}

/* ---- the frontend's side ---- */

static _Atomic size_t frames_handed;
static size_t batch(const int16_t *data, size_t frames)
{
   (void)data;
   atomic_fetch_add(&frames_handed, frames);
   return frames;
}

static _Atomic int quit;
static int saved_stdout = -1;
static void *reader_main(void *arg)
{
   (void)arg;
   on_reader = 1;
   while (!atomic_load(&quit))
      audio_callback();
   return NULL;
}

int main(void)
{
   pthread_t reader;
   unsigned i, opened = 0;

   audio_sample_batch_cb = batch;
   printf("video processor audio capture:\n");
   fflush(stdout);
   /* The core announces every open on stdout. */
   {
      int devnull = open("/dev/null", O_WRONLY);
      saved_stdout = dup(1);
      if (devnull >= 0)
      {
         dup2(devnull, 1);
         close(devnull);
      }
   }
   pthread_create(&reader, NULL, reader_main, NULL);
   for (i = 0; i < 200; i++)
   {
      /* Open long enough for the callback, which sleeps 10 ms while no
       * device is open, to come round and be reading when the close
       * lands. */
      struct timespec open_for = { 0, 3000000L + (long)(i % 5) * 2000000L };
      struct timespec shut_for = { 0, 1000000L };
      if (open_audio_device("null") == 0)
         opened++;
      nanosleep(&open_for, NULL);
      close_audio_device();
      nanosleep(&shut_for, NULL);
   }
   atomic_store(&quit, 1);
   pthread_join(reader, NULL);
   fflush(stdout);
   if (saved_stdout >= 0)
      dup2(saved_stdout, 1);

   printf("   %u opens, %u reads, %u frames handed on, %u reads on a "
         "closed handle, %u locks taken by the callback\n",
         opened, atomic_load(&reads), (unsigned)atomic_load(&frames_handed),
         atomic_load(&reads_after_close), atomic_load(&reader_locks));
   CHECK(opened == 200, "the null capture device opened %u of 200 times", opened);
   CHECK(atomic_load(&reads_after_close) == 0,
         "%u reads were made on a closed handle", atomic_load(&reads_after_close));
   CHECK(atomic_load(&reader_locks) == 0,
         "the callback took %u locks", atomic_load(&reader_locks));
   CHECK(atomic_load(&reads) > 0, "the callback never read while the device was open");
   CHECK(atomic_load(&frames_handed) > 0, "no capture reached the frontend");
   if (failures)
   {
      printf("%u failure(s)\n", failures);
      return 1;
   }
   printf("video processor audio capture: no read on a closed handle, no lock\n");
   return 0;
}
