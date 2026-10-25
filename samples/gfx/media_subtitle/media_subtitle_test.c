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
#include "../../../tasks/task_subtitle.h"

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

static void catalog_discovery(bool threaded)
{
   task_subtitle_catalog_t *catalog;
   char movie[80], name[80], expected[80];
   unsigned i;
   task_queue_init(threaded, NULL);
   sprintf(movie, "subtitle-catalog-%lu.mp4", (unsigned long)sample_getpid());
   catalog = task_subtitle_discover(movie);
   assert(catalog);
   if (!threaded)
   {
      assert(task_subtitle_catalog_status(catalog) == 0);
      assert(!task_subtitle_catalog_count(catalog));
      assert(!task_subtitle_catalog_path(catalog, 0));
   }
   task_subtitle_catalog_release(catalog);
   drain();
   for (i = 0; i < 3; i++)
   {
      FILE *file;
      sprintf(name, "subtitle-catalog-%lu%s",
            (unsigned long)sample_getpid(),
            i == 0 ? ".srt" : i == 1 ? ".en.forced.srt" : ".ja.SRT");
      file = fopen(name, "wb");
      assert(file && !fclose(file));
   }
   catalog = task_subtitle_discover(movie);
   assert(catalog);
   drain();
   assert(task_subtitle_catalog_status(catalog) == 1);
   assert(task_subtitle_catalog_count(catalog) == 3);
   assert(strstr(task_subtitle_catalog_path(catalog, 0), ".en.forced.srt"));
   assert(strstr(task_subtitle_catalog_path(catalog, 1), ".ja.SRT"));
   assert(strstr(task_subtitle_catalog_path(catalog, 2), ".srt"));
   task_subtitle_catalog_release(catalog);
   for (i = 0; i < 3; i++)
   {
      sprintf(name, "subtitle-catalog-%lu%s",
            (unsigned long)sample_getpid(),
            i == 0 ? ".srt" : i == 1 ? ".en.forced.srt" : ".ja.SRT");
      assert(!remove(name));
   }
   sprintf(name, "SUBTITLE-CATALOG-%lu.en.srt", (unsigned long)sample_getpid());
   {
      FILE *file = fopen(name, "wb");
      assert(file && !fclose(file));
   }
   catalog = task_subtitle_discover(movie);
   assert(catalog);
   drain();
   assert(task_subtitle_catalog_status(catalog) == 1);
#if defined(_WIN32)
   assert(task_subtitle_catalog_count(catalog) == 1);
   assert(!strcmp(task_subtitle_catalog_path(catalog, 0), name));
#else
   assert(!task_subtitle_catalog_count(catalog));
#endif
   task_subtitle_catalog_release(catalog);
   assert(!remove(name));
   for (i = 0; i < 70; i++)
   {
      FILE *file;
      sprintf(name, "subtitle-catalog-%lu.%02u.srt",
            (unsigned long)sample_getpid(), 69 - i);
      file = fopen(name, "wb");
      assert(file && !fclose(file));
   }
   sprintf(name, "subtitle-catalog-%lu-other.srt", (unsigned long)sample_getpid());
   {
      FILE *file = fopen(name, "wb");
      assert(file && !fclose(file));
   }
   catalog = task_subtitle_discover(movie);
   assert(catalog);
   drain();
   assert(task_subtitle_catalog_status(catalog) == 1);
   assert(task_subtitle_catalog_count(catalog) == 64);
   for (i = 0; i < 64; i++)
   {
      sprintf(expected, "subtitle-catalog-%lu.%02u.srt",
            (unsigned long)sample_getpid(), i);
      assert(!strcmp(task_subtitle_catalog_path(catalog, i), expected));
   }
   assert(!task_subtitle_catalog_path(catalog, 64));
   task_subtitle_catalog_release(catalog);
   assert(!remove(name));
   for (i = 0; i < 70; i++)
   {
      sprintf(name, "subtitle-catalog-%lu.%02u.srt",
            (unsigned long)sample_getpid(), i);
      assert(!remove(name));
   }
   for (i = 0; i < 128; i++)
   {
      catalog = task_subtitle_discover(movie);
      assert(catalog);
      if (!threaded && (i & 1))
         task_queue_check();
      task_subtitle_catalog_release(catalog);
   }
   drain();
   task_queue_deinit();
   printf("PASS: bounded sorted sidecar discovery and cancellation (%s)\n",
         threaded ? "threaded" : "regular");
}

static void track_selection(bool threaded)
{
   gfx_media_subtitle_t *owner;
   char english[80], japanese[80];
   unsigned i;
   FILE *file;
   sprintf(english, "subtitle-view-%lu.en.srt", (unsigned long)sample_getpid());
   sprintf(japanese, "subtitle-view-%lu.ja.srt", (unsigned long)sample_getpid());
   fixture("1\n00:00:00,000 --> 00:00:03,000\ndefault\n");
   file = fopen(english, "wb");
   assert(file);
   fputs("1\n00:00:00,000 --> 00:00:03,000\nEnglish\n", file);
   assert(!fclose(file));
   file = fopen(japanese, "wb");
   assert(file);
   fputs("1\n00:00:00,000 --> 00:00:03,000\nJapanese\n", file);
   assert(!fclose(file));
   task_queue_init(threaded, NULL);
   owner = gfx_media_subtitle_new();
   assert(owner);
   await(owner, &source_a, 1000000, "default");
   gfx_media_subtitle_next(owner);
   drain();
   gfx_media_subtitle_update(owner, &source_a, media_path, 1000000, 0, true);
   assert(gfx_media_subtitle_view(owner)->off);
   assert(!*gfx_media_subtitle_view(owner)->text);
   assert(gfx_media_subtitle_view(owner)->track_count == 3);
   gfx_media_subtitle_next(owner);
   await(owner, &source_a, 1000000, "English");
   assert(!strcmp(gfx_media_subtitle_view(owner)->track, english));
   gfx_media_subtitle_suspend(owner);
   gfx_media_subtitle_update(owner, NULL, NULL, 0, 0, true);
   assert(!*gfx_media_subtitle_view(owner)->text);
   gfx_media_subtitle_update(owner, &source_b, media_path, 1000000, 0, true);
   assert(!strcmp(gfx_media_subtitle_view(owner)->text, "English"));
   gfx_media_subtitle_next(owner);
   await(owner, &source_b, 1000000, "Japanese");
   assert(!strcmp(gfx_media_subtitle_view(owner)->track, japanese));
   /* Queued choices are collapsed into one final load. */
   for (i = 0; i < 7; i++)
      gfx_media_subtitle_next(owner);
   await(owner, &source_b, 1000000, "English");
   gfx_media_subtitle_next(owner);
   gfx_media_subtitle_update(owner, &source_a, other_path, 1000000, 0, true);
   drain();
   gfx_media_subtitle_update(owner, &source_a, other_path, 1000000, 0, true);
   assert(!*gfx_media_subtitle_view(owner)->text);
   assert(!gfx_media_subtitle_view(owner)->track_count);
   gfx_media_subtitle_free(owner);
   drain();
   task_queue_deinit();
   assert(!remove(english) && !remove(japanese) && !remove(subtitle_path));
   printf("PASS: subtitle choice, Off, context restoration and stale commands (%s)\n",
         threaded ? "threaded" : "regular");
}

static void embedded_selection(const char *path, bool threaded)
{
   gfx_media_subtitle_t *owner;
   int64_t until;
   unsigned choice;
   const char *first = getenv("SUBTITLE_EXPECT_UTF16")
      ? "Z\360\237\230\200" : "first caf\303\251";
   task_queue_init(threaded, NULL);
   owner = gfx_media_subtitle_new();
   assert(owner);
   gfx_media_subtitle_update(owner, &source_a, path, 500000, 0, true);
   for (choice = 0; choice < 4; choice++)
   {
      const gfx_media_subtitle_view_t *view;
      gfx_media_subtitle_next(owner);
      until = cpu_features_get_time_usec() + 10000000;
      do
      {
         task_queue_check();
         gfx_media_subtitle_update(owner, &source_a, path, 500000, 0, true);
         view = gfx_media_subtitle_view(owner);
         if (view->track_count == 2 && (choice == 2 ? view->off
                  : !strcmp(view->text, first)
                  && strstr(view->track, choice == 1 ? "[2]" : "[1]")))
            break;
         retro_sleep(1);
      } while (cpu_features_get_time_usec() < until);
      assert(view->track_count == 2);
      if (choice == 2) assert(view->off && !*view->text);
      else
      {
         assert(!strcmp(view->text, first));
         assert(strstr(view->track, choice == 1 ? "[2]" : "[1]"));
      }
   }
   gfx_media_subtitle_suspend(owner);
   gfx_media_subtitle_update(owner, NULL, NULL, 0, 0, true);
   gfx_media_subtitle_update(owner, &source_b, path, 2500000, 0, true);
   assert(!strcmp(gfx_media_subtitle_view(owner)->text, "second \360\237\230\200"));
   gfx_media_subtitle_free(owner);
   drain();
   task_queue_deinit();
   puts("PASS: embedded track identities, text, Off and context restoration");
}

int main(int argc, char **argv)
{
   if (argc == 2)
   {
      embedded_selection(argv[1], false);
#ifdef HAVE_THREADS
      embedded_selection(argv[1], true);
#endif
      return 0;
   }
   sprintf(media_path, "subtitle-view-%lu.mp4", (unsigned long)sample_getpid());
   sprintf(subtitle_path, "subtitle-view-%lu.srt", (unsigned long)sample_getpid());
   sprintf(other_path, "subtitle-view-%lu-missing.mp4", (unsigned long)sample_getpid());
   catalog_discovery(false);
   lifecycle(false);
   track_selection(false);
#ifdef HAVE_THREADS
   catalog_discovery(true);
   lifecycle(true);
   track_selection(true);
   concurrent_mailbox();
#endif
   return 0;
}
