/* The Switch audren driver's renderer ownership, against a scripted
 * renderer.
 *
 * libnx's audrv is not thread-safe, and write_avail() is the one call
 * the frontend makes from a thread other than the writer's: rate
 * control's fill, every frame, while the audio thread wrapper writes.
 * The driver keeps the renderer to the writer's thread and publishes
 * the room for the others, so no lock is taken by either side.
 *
 *   - wrapper shape: a writer thread makes blocking writes while the
 *     main thread reads write_avail() as fast as it can. Every
 *     renderer call comes from the writer (audren_mock.c counts any
 *     other), every room read is within the buffers, and everything
 *     written plays. Under TSan, two threads in the renderer at once
 *     is also a reported race on the mock's wave buffer list.
 *   - one thread, non-blocking: the frontend writing for itself, as
 *     without the wrapper. A full driver plays out, and write_avail()
 *     on that thread sees the room at once - it still updates the
 *     renderer there - and the writes go on taking audio.
 *   - stop and start from the writer: the voice stops and resumes, and
 *     a write after start plays. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <stdatomic.h>

#include <boolean.h>

#include "../../../audio/audio_driver.h"
#include "audren_mock.h"

extern audio_driver_t audio_switch_libnx_audren;

static unsigned failures = 0;

#define CHECK(cond, msg, a, b) \
   do { \
      if (!(cond)) \
      { \
         printf("FAIL %s:%d: %s (%ld, %ld)\n", __FILE__, __LINE__, \
               msg, (long)(a), (long)(b)); \
         failures++; \
      } \
   } while (0)

#define WRITES      200
#define WRITE_BYTES 960         /* 240 int16 stereo frames */

static int16_t pcm[WRITE_BYTES / 2];
static void *drv;
static size_t total;            /* wave buffers together, in bytes */
static _Atomic int writer_done;
static size_t written;

static void sleep_us(unsigned us)
{
   struct timespec ts = { 0, (long)us * 1000 };
   nanosleep(&ts, NULL);
}

static void *writer_main(void *arg)
{
   unsigned i;
   (void)arg;
   audren_mock_set_owner(pthread_self());
   audio_switch_libnx_audren.set_nonblock_state(drv, false);
   audio_switch_libnx_audren.start(drv, false);
   for (i = 0; i < WRITES; i++)
   {
      ssize_t w;
      audio_switch_libnx_audren.wait_writable(drv, WRITE_BYTES);
      w = audio_switch_libnx_audren.write(drv, pcm, WRITE_BYTES);
      if (w > 0)
         written += (size_t)w;
   }
   atomic_store(&writer_done, 1);
   return NULL;
}

static void lane_wrapper_shape(void)
{
   pthread_t writer;
   unsigned rate = 48000, reads = 0, over = 0, waited = 0;
   size_t   last = 0;

   drv   = audio_switch_libnx_audren.init(NULL, 48000, 64, &rate);
   CHECK(drv != NULL, "init", 0, 0);
   if (!drv)
      return;
   total   = audio_switch_libnx_audren.buffer_size(drv);
   written = 0;
   atomic_store(&writer_done, 0);
   audren_mock_start(2000);
   pthread_create(&writer, NULL, writer_main, NULL);
   while (!atomic_load(&writer_done))
   {
      size_t room = audio_switch_libnx_audren.write_avail(drv);
      if (room > total)
         over++;
      last = room;
      reads++;
   }
   pthread_join(writer, NULL);
   /* Everything written plays out: the writer's thread is the one
    * allowed to update, so it brings the states in. */
   audren_mock_set_owner(pthread_self());
   while (     audren_mock_played_bytes() < written
         && waited++ < 2000)
   {
      audio_switch_libnx_audren.wait_writable(drv, total / 2);
      sleep_us(1000);
   }
   audren_mock_stop();
   printf("   wrapper shape: %u writes, %u write_avail reads from another "
         "thread (last %u of %u), %u renderer calls from it\n",
         WRITES, reads, (unsigned)last, (unsigned)total,
         audren_mock_foreign_calls());
   CHECK(audren_mock_foreign_calls() == 0,
         "a renderer call came from a thread other than the writer's",
         audren_mock_foreign_calls(), 0);
   CHECK(over == 0, "write_avail() reported more than the buffers", over, total);
   CHECK(written == (size_t)WRITES * WRITE_BYTES,
         "a blocking write came back short", written,
         (size_t)WRITES * WRITE_BYTES);
   CHECK(audren_mock_played_bytes() >= written - total,
         "what was written did not play", audren_mock_played_bytes(), written);
   audio_switch_libnx_audren.free(drv);
}

static void lane_one_thread_nonblock(void)
{
   unsigned rate = 48000, i, stalls = 0;
   size_t   took = 0, room_full, room_later;

   drv = audio_switch_libnx_audren.init(NULL, 48000, 64, &rate);
   CHECK(drv != NULL, "init", 0, 0);
   if (!drv)
      return;
   total = audio_switch_libnx_audren.buffer_size(drv);
   audren_mock_set_owner(pthread_self());
   audio_switch_libnx_audren.set_nonblock_state(drv, true);
   audio_switch_libnx_audren.start(drv, false);
   audren_mock_start(2000);

   /* Full: non-blocking writes until one takes nothing. */
   for (i = 0; i < 1000; i++)
      if (audio_switch_libnx_audren.write(drv, pcm, WRITE_BYTES) <= 0)
         break;
   room_full = audio_switch_libnx_audren.write_avail(drv);
   /* The renderer plays a buffer every 2 ms. */
   sleep_us(20000);
   room_later = audio_switch_libnx_audren.write_avail(drv);
   printf("   one thread, non-blocking: room %u when full, %u after 20 ms\n",
         (unsigned)room_full, (unsigned)room_later);
   CHECK(room_later > room_full,
         "write_avail() on the writer's thread did not see the renderer play",
         room_later, room_full);

   /* And the writes go on: a frame's worth at a time for a second. */
   for (i = 0; i < 500; i++)
   {
      ssize_t w = audio_switch_libnx_audren.write(drv, pcm, WRITE_BYTES);
      if (w > 0)
         took += (size_t)w;
      else
         stalls++;
      sleep_us(2000);
   }
   audren_mock_stop();
   printf("   one thread, non-blocking: took %u bytes over 500 writes, "
         "%u refused\n", (unsigned)took, stalls);
   CHECK(took >= (size_t)250 * WRITE_BYTES,
         "non-blocking writes stopped taking audio", took,
         (size_t)250 * WRITE_BYTES);
   CHECK(audren_mock_foreign_calls() == 0, "renderer call off the writer's thread",
         audren_mock_foreign_calls(), 0);
   audio_switch_libnx_audren.free(drv);
}

static void lane_stop_start(void)
{
   unsigned rate = 48000;
   size_t   before;

   drv = audio_switch_libnx_audren.init(NULL, 48000, 64, &rate);
   CHECK(drv != NULL, "init", 0, 0);
   if (!drv)
      return;
   audren_mock_set_owner(pthread_self());
   audio_switch_libnx_audren.set_nonblock_state(drv, false);
   audren_mock_start(2000);
   CHECK(audio_switch_libnx_audren.start(drv, false), "start", 0, 0);
   CHECK(audio_switch_libnx_audren.write(drv, pcm, WRITE_BYTES) == WRITE_BYTES,
         "write after start", 0, 0);
   CHECK(audio_switch_libnx_audren.stop(drv), "stop", 0, 0);
   sleep_us(20000);
   audio_switch_libnx_audren.write_avail(drv);
   before = audren_mock_played_bytes();
   sleep_us(20000);
   audio_switch_libnx_audren.write_avail(drv);
   CHECK(audren_mock_played_bytes() == before, "the voice played while stopped",
         audren_mock_played_bytes(), before);
   CHECK(audio_switch_libnx_audren.start(drv, false), "start after stop", 0, 0);
   CHECK(audio_switch_libnx_audren.write(drv, pcm, WRITE_BYTES) == WRITE_BYTES,
         "write after restart", 0, 0);
   audren_mock_stop();
   printf("   stop and start: the voice paused and came back\n");
   audio_switch_libnx_audren.free(drv);
}

int main(void)
{
   unsigned i;
   for (i = 0; i < WRITE_BYTES / 2; i++)
      pcm[i] = (int16_t)(i * 37);
   printf("switch audren:\n");
   lane_wrapper_shape();
   lane_one_thread_nonblock();
   lane_stop_start();
   if (failures)
   {
      printf("%u failure(s)\n", failures);
      return 1;
   }
   printf("switch audren: the renderer stays on the writer's thread, no lock\n");
   return 0;
}
