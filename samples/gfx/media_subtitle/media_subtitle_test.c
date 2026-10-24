#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <process.h>
#define sample_getpid _getpid
#else
#include <unistd.h>
#define sample_getpid getpid
#endif
#include <features/features_cpu.h>
#include <queues/task_queue.h>
#include <retro_timers.h>
#include <rthreads/rthreads.h>
#include "../../../gfx/gfx_media_subtitle.h"

static char media_path[80], subtitle_path[80], other_path[80];
static gfx_media_subtitle_mailbox_t mailbox;
static gfx_media_subtitle_view_t received;
static int source_a, source_b;

static bool any_task(retro_task_t *task, void *data)
{
   (void)task;
   (void)data;
   return true;
}

static bool busy(void)
{
   task_finder_data_t finder;
   memset(&finder, 0, sizeof(finder));
   finder.func = any_task;
   return task_queue_find(&finder);
}

static void drain(void)
{
   int64_t until = cpu_features_get_time_usec() + 10000000;
   do
   {
      task_queue_check();
      if (!busy()) return;
      retro_sleep(1);
   } while (cpu_features_get_time_usec() < until);
   assert(!busy());
}

static void fixture(const char *text)
{
   FILE *file = fopen(subtitle_path, "wb");
   assert(file && fwrite(text, 1, strlen(text), file) == strlen(text));
   assert(!fclose(file));
}

static void await(gfx_media_subtitle_t *owner, const void *identity,
      int64_t position, const char *expected)
{
   int64_t until = cpu_features_get_time_usec() + 10000000;
   do
   {
      task_queue_check();
      gfx_media_subtitle_update(owner, identity, media_path, position, 0, true);
      if (!strcmp(gfx_media_subtitle_view(owner)->text, expected)) return;
      retro_sleep(1);
   } while (cpu_features_get_time_usec() < until);
   fprintf(stderr, "expected [%s], got [%s]\n", expected,
         gfx_media_subtitle_view(owner)->text);
   assert(0);
}

static void lifecycle(bool threaded)
{
   gfx_media_subtitle_t *owner;
   unsigned i;
   task_queue_init(threaded, NULL);
   fixture("1\n00:00:00,500 --> 00:00:02,000\nfirst \303\251\n\n"
         "2\n00:00:01,000 --> 00:00:01,500\noverlap\n\n"
         "3\n00:00:03,000 --> 00:00:04,000\nlast\n");
   owner = gfx_media_subtitle_new();
   assert(owner);
   gfx_media_subtitle_update(owner, &source_a, media_path, 1200000, 0, true);
   if (!threaded) assert(!*gfx_media_subtitle_view(owner)->text);
   await(owner, &source_a, 1200000, "first \303\251\noverlap");
   assert(!gfx_media_subtitle_view(owner)->truncated);
   assert(!remove(subtitle_path));
   assert(gfx_media_subtitle_update(owner, &source_a, media_path, 3200000, 0, true));
   assert(!strcmp(gfx_media_subtitle_view(owner)->text, "last"));
   assert(!gfx_media_subtitle_update(owner, &source_a, media_path, 3200000, 0, true));
   assert(gfx_media_subtitle_update(owner, &source_a, media_path, 600000, 0, true));
   assert(!strcmp(gfx_media_subtitle_view(owner)->text, "first \303\251"));
   gfx_media_subtitle_update(owner, &source_a, media_path, 1200000, 2000000, true);
   assert(!*gfx_media_subtitle_view(owner)->text);
   gfx_media_subtitle_update(owner, &source_a, media_path, 1200000, -2000000, true);
   assert(!strcmp(gfx_media_subtitle_view(owner)->text, "last"));
   gfx_media_subtitle_publish(&mailbox, gfx_media_subtitle_view(owner));
   assert(gfx_media_subtitle_consume(&mailbox, &received));
   assert(!strcmp(received.text, "last"));
   gfx_media_subtitle_update(owner, &source_b, media_path, 1200000, 0, true);
   assert(!*gfx_media_subtitle_view(owner)->text);
   drain();
   gfx_media_subtitle_update(owner, &source_b, media_path, 1200000, 0, true);
   assert(!*gfx_media_subtitle_view(owner)->text);
   fixture("1\n00:00:00,000 --> 00:00:02,000\nreplacement\n");
   gfx_media_subtitle_update(owner, &source_a, media_path, 1200000, 0, true);
   gfx_media_subtitle_update(owner, &source_b, other_path, 1200000, 0, true);
   drain();
   gfx_media_subtitle_update(owner, &source_b, other_path, 1200000, 0, true);
   assert(!*gfx_media_subtitle_view(owner)->text);
   await(owner, &source_a, 1200000, "replacement");
   gfx_media_subtitle_update(owner, &source_a, media_path, 1200000, 0, false);
   assert(!*gfx_media_subtitle_view(owner)->text);
   for (i = 0; i < 128; i++)
   {
      gfx_media_subtitle_update(owner, &source_a, media_path, 1200000, 0, true);
      if (!threaded && (i & 1)) task_queue_check();
      gfx_media_subtitle_update(owner, NULL, NULL, 0, 0, false);
   }
   gfx_media_subtitle_update(owner, &source_a, media_path, 1200000, 0, true);
   gfx_media_subtitle_free(owner);
   drain();
   gfx_media_subtitle_publish(&mailbox, NULL);
   assert(gfx_media_subtitle_consume(&mailbox, &received) && !*received.text);
   task_queue_deinit();
   assert(!remove(subtitle_path));
   printf("PASS: subtitle source replacement, overlap, seek, delay and retirement (%s)\n",
         threaded ? "threaded" : "regular");
}

#ifdef HAVE_THREADS
static retro_atomic_int_t mailbox_done;
static unsigned mailbox_reads;
static gfx_media_subtitle_view_t mailbox_write, mailbox_read;

static void mailbox_reader(void *data)
{
   (void)data;
   for (;;)
   {
      if (gfx_media_subtitle_consume(&mailbox, &mailbox_read))
      {
         size_t i;
         unsigned char letter = (unsigned char)mailbox_read.text[0];
         assert(letter >= 'a' && letter <= 'z');
         assert(mailbox_read.truncated == ((letter - 'a') & 1));
         for (i = 0; i < sizeof(mailbox_read.text) - 1; i++)
            assert((unsigned char)mailbox_read.text[i] == letter);
         assert(!mailbox_read.text[sizeof(mailbox_read.text) - 1]);
         mailbox_reads++;
      }
      else if (retro_atomic_load_acquire_int(&mailbox_done))
         break;
   }
}

static void concurrent_mailbox(void)
{
   unsigned i;
   sthread_t *reader;
   retro_atomic_int_init(&mailbox_done, 0);
   reader = sthread_create(mailbox_reader, NULL);
   assert(reader);
   for (i = 0; i < 50000; i++)
   {
      unsigned letter = i % 26;
      memset(mailbox_write.text, 'a' + letter, sizeof(mailbox_write.text) - 1);
      mailbox_write.text[sizeof(mailbox_write.text) - 1] = '\0';
      mailbox_write.truncated = (letter & 1) != 0;
      gfx_media_subtitle_publish(&mailbox, &mailbox_write);
   }
   retro_atomic_store_release_int(&mailbox_done, 1);
   sthread_join(reader);
   assert(mailbox_reads);
   gfx_media_subtitle_publish(&mailbox, NULL);
   assert(gfx_media_subtitle_consume(&mailbox, &received) && !*received.text);
   puts("PASS: immutable subtitle text crosses the render mailbox without torn views");
}
#endif

int main(void)
{
   sprintf(media_path, "subtitle-view-%lu.mp4", (unsigned long)sample_getpid());
   sprintf(subtitle_path, "subtitle-view-%lu.srt", (unsigned long)sample_getpid());
   sprintf(other_path, "subtitle-view-%lu-missing.mp4", (unsigned long)sample_getpid());
   lifecycle(false);
#ifdef HAVE_THREADS
   lifecycle(true);
   concurrent_mailbox();
#endif
   return 0;
}
