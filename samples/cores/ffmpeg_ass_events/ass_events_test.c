#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../../cores/libretro-ffmpeg/ffmpeg_core.c"

#define LINES   20000
#define SEEKS   64

static ASS_Track track;
static retro_atomic_int_t inside;
static uintptr_t renderer;
static unsigned flushes, chunks, next_line, last_line, misordered, overlaps;
static retro_atomic_int_t strays;
static retro_atomic_int_t pushed;

/* libass is not thread-safe: every call is the renderer's, one at a
 * time. A call from elsewhere is counted and goes no further. */
static bool fixture_enter(void)
{
   if (sthread_get_current_thread_id() != renderer)
   {
      retro_atomic_fetch_add_int(&strays, 1);
      return false;
   }
   if (retro_atomic_fetch_add_int(&inside, 1) != 0)
      overlaps++;
   return true;
}

static void fixture_leave(void)
{
   retro_atomic_fetch_add_int(&inside, -1);
}

void ass_flush_events(ASS_Track *t)
{
   if (!fixture_enter())
      return;
   if (t == &track)
      flushes++;
   fixture_leave();
}

void ass_process_chunk(ASS_Track *t, const char *data, int size,
      long long timecode, long long duration)
{
   unsigned n = 0;
   if (!fixture_enter())
      return;
   if (     t != &track
         || (int)strlen(data) != size
         || sscanf(data, "Dialogue: %u", &n) != 1
         || n < next_line
         || timecode != (long long)n
         || duration != (long long)n * 2)
      misordered++;
   next_line = n + 1;
   last_line = n;
   chunks++;
   fixture_leave();
}

static bool direct;

/* Decode thread: a line per subtitle, and a flush now and then as a
 * seek does. With @direct, the flush is made in place. */
static void decoder(void *data)
{
   unsigned i;
   char line[64];
   (void)data;
   for (i = 0; i < LINES; i++)
   {
      snprintf(line, sizeof(line), "Dialogue: %u,0,Default", i);
      ass_change_push(&track, line, i, (long long)i * 2);
      if (i % (LINES / SEEKS) == 0)
      {
         if (direct)
            ass_flush_events(&track);
         else
            ass_change_push(&track, NULL, 0, 0);
      }
      retro_atomic_fetch_add_int(&pushed, 1);
   }
}

static void run(bool in_place)
{
   sthread_t *thread;
   direct    = in_place;
   flushes   = chunks = next_line = last_line = misordered = overlaps = 0;
   retro_atomic_int_init(&strays, 0);
   retro_atomic_int_init(&pushed, 0);
   renderer  = sthread_get_current_thread_id();
   thread    = sthread_create(decoder, NULL);
   if (!thread)
   {
      printf("FAIL: no thread\n");
      exit(1);
   }
   while (retro_atomic_load_acquire_int(&pushed) < LINES)
      ass_changes_run(true);
   sthread_join(thread);
   ass_changes_run(true);
}

int main(void)
{
   unsigned failures = 0;
#if !defined(RETRO_ATOMIC_HAS_PTR)
   g_ctx.ass_lock = slock_new();
#endif
   retro_atomic_int_init(&inside, 0);

   ASS_TRACK_STR[2] = &track;

   /* lines come in order, the newest last; superseded ones are dropped */
   run(false);
   printf("hand-off: %u of %u lines, newest %u, %u flushes, %u misordered, "
         "%u off-thread, %u overlapping\n",
         chunks, LINES, last_line, flushes, misordered,
         (unsigned)retro_atomic_load_acquire_int(&strays), overlaps);
   if (     !chunks || last_line != LINES - 1 || flushes < chunks
         || misordered || retro_atomic_load_acquire_int(&strays) || overlaps)
      failures++;

   /* never rendered: one change held, the newest */
   {
      unsigned i;
      char line[64];
      flushes = chunks = next_line = 0;
      for (i = 0; i < 1000; i++)
      {
         snprintf(line, sizeof(line), "Dialogue: %u", i);
         ass_change_push(&track, line, i, (long long)i * 2);
      }
      ass_changes_run(true);
      printf("unrendered: %u applied, newest %u\n", chunks, last_line);
      if (chunks != 1 || flushes != 1 || last_line != 999 || misordered)
         failures++;
   }

   /* negative: a flush made on the decode thread is caught */
   run(true);
   printf("in place: %u off-thread\n",
         (unsigned)retro_atomic_load_acquire_int(&strays));
   if (!retro_atomic_load_acquire_int(&strays))
      failures++;

   /* what was never rendered is freed */
   ass_change_push(&track, "Dialogue: 0", 0, 0);
   ass_changes_run(false);
   chunks = 0;
   ass_changes_run(true);
   if (chunks)
      failures++;

   printf("%s\n", failures ? "FAIL" : "PASS");
   return failures ? 1 : 0;
}
