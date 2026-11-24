/* GET_LOG's ring in verbosity.c: lines logged from several threads at
 * once, read while they are written, arrive whole and in order, and
 * an overrun is reported as lines skipped rather than torn lines.
 * Logging turned off, on and to another level while other threads log
 * is no race, and the gate still holds the lines back. A line of any
 * length comes back as written, cut at the slot's 255 bytes, from a
 * slot an older, longer line left bytes in. The log file opened and
 * closed while other threads log is never written after it is closed,
 * and holds only whole lines. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <rthreads/rthreads.h>
#include <retro_atomic.h>

#include "../../../verbosity.h"

#define THREADS      4
#define LINES        20000
#define TOGGLE_LINES 2000
#define SINK_TURNS   300
#define SINK_FILE    "log_ring_test.log"

static unsigned failures;
static retro_atomic_int_t done;

#define CHECK(test) do { if (!(test)) { \
   fprintf(stderr, "FAIL line %u: %s\n", (unsigned)__LINE__, #test); \
   failures++; } } while (0)

static void writer(void *data)
{
   unsigned t = (unsigned)(size_t)data;
   unsigned i;
   for (i = 0; i < LINES; i++)
      RARCH_LOG("w%u %06u ........................................\n", t, i);
   retro_atomic_fetch_add_int(&done, 1);
}

static void toggled_writer(void *data)
{
   unsigned i;
   (void)data;
   for (i = 0; i < TOGGLE_LINES; i++)
      RARCH_LOG("t ........................................\n");
   retro_atomic_fetch_add_int(&done, 1);
}

/* Logs until the main thread is done opening and closing the file */
static retro_atomic_int_t sink_stop;
static void sink_writer(void *data)
{
   unsigned t = (unsigned)(size_t)data;
   unsigned i = 0;
   while (!retro_atomic_load_acquire_int(&sink_stop))
      RARCH_LOG("w%u %06u ........................................\n",
            t, i++ % LINES);
   retro_atomic_fetch_add_int(&done, 1);
}

/* A line as the writers write it, after the tag RARCH_LOG adds */
static bool line_whole(const char *s, const char *e)
{
   const char *w = strstr(s, "] w");
   unsigned t, i;
   if (!w || w > e)
      return false;
   if (sscanf(w + 3, "%u %6u", &t, &i) != 2 || t >= THREADS || i >= LINES)
      return false;
   return (size_t)(e - w) == strlen("] w0 000000 ") + 40;
}

static unsigned drain(size_t *since, char *buf, size_t len, unsigned *skipped)
{
   size_t first, next;
   size_t n     = verbosity_ring_read(*since, buf, len, &first, &next);
   unsigned got = 0;
   char *s      = buf;
   CHECK(next >= first && first >= *since);
   *skipped    += (unsigned)(first - *since);
   while (s < buf + n)
   {
      char *e = strchr(s, '\n');
      CHECK(e != NULL);
      if (!e)
         break;
      CHECK(line_whole(s, e));
      got++;
      s = e + 1;
   }
   *since = next;
   return got;
}

int main(void)
{
   sthread_t *th[THREADS];
   char      *buf = (char*)malloc(1 << 16);
   size_t     since = 0, first, next;
   unsigned   got = 0, skipped = 0, i;

   verbosity_enable();
   CHECK(verbosity_ring_read(0, buf, 1 << 16, &first, &next) == 0);
   RARCH_LOG("before the ring is armed\n");
   CHECK(verbosity_ring_read(0, buf, 1 << 16, &first, &next) == 0);
   verbosity_ring_arm();

   for (i = 0; i < THREADS; i++)
      th[i] = sthread_create(writer, (void*)(size_t)i);
   while (retro_atomic_load_acquire_int(&done) < THREADS)
      got += drain(&since, buf, 1 << 16, &skipped);
   for (i = 0; i < THREADS; i++)
      sthread_join(th[i]);
   got += drain(&since, buf, 1 << 16, &skipped);

   /* every line was read once or counted as skipped; a slot being
    * written when read is skipped too, so allow none lost beyond that */
   CHECK(since == (size_t)THREADS * LINES);
   CHECK(got + skipped <= (unsigned)(THREADS * LINES));
   CHECK(got > 0);

   /* a short buffer stops at a whole line, and NEXT resumes there */
   RARCH_LOG("w0 000001 ........................................\n");
   RARCH_LOG("w0 000002 ........................................\n");
   {
      size_t n = verbosity_ring_read(since, buf, 80, &first, &next);
      CHECK(n > 0 && n < 80 && next == since + 1);
      n = verbosity_ring_read(next, buf, 1 << 16, &first, &next);
      CHECK(n > 0 && next == since + 2);
   }

   retro_atomic_store_release_int(&done, 0);
   for (i = 0; i < THREADS; i++)
      th[i] = sthread_create(toggled_writer, NULL);
   do
   {
      verbosity_disable();
      verbosity_set_log_level(2);
      verbosity_set_log_level(1);
      verbosity_enable();
   } while (retro_atomic_load_acquire_int(&done) < THREADS);
   for (i = 0; i < THREADS; i++)
      sthread_join(th[i]);

   /* off, or below the level, logs nothing; back on logs again */
   {
      size_t head;
      verbosity_ring_read((size_t)-1, buf, 1 << 16, &first, &head);
      verbosity_disable();
      CHECK(*verbosity_get_ptr() == verbosity_is_enabled());
      RARCH_LOG("while off\n");
      verbosity_enable();
      CHECK(*verbosity_get_ptr() == verbosity_is_enabled());
      verbosity_set_log_level(2);
      RARCH_LOG("below the level\n");
      verbosity_set_log_level(1);
      verbosity_ring_read((size_t)-1, buf, 1 << 16, &first, &next);
      CHECK(next == head);
      RARCH_LOG("back on\n");
      verbosity_ring_read((size_t)-1, buf, 1 << 16, &first, &next);
      CHECK(next == head + 1);
   }

   /* lengths around the word size and the slot's 255 bytes, each in a
    * slot a 255-byte line of other bytes was in before */
   {
      static const unsigned lens[] = { 0, 1, 7, 8, 9, 15, 16, 17, 100,
         228, 229, 230, 231, 232, 233, 240, 250, 300, 1200 };
      static char body[1300], dirt[300], prefix[256];
      size_t head, n, p, k, j;
      memset(body, 'x', sizeof(body));
      memset(dirt, 'z', sizeof(dirt));
      for (i = 0; i < 256; i++)
         RARCH_LOG("%.*s\n", (int)sizeof(dirt), dirt);
      verbosity_ring_read((size_t)-1, buf, 1 << 16, &first, &head);
      /* the tag RARCH_LOG puts ahead of a line */
      RARCH_LOG("\n");
      n = verbosity_ring_read(head, buf, 1 << 16, &first, &next);
      CHECK(n >= 1 && n < sizeof(prefix) && next == head + 1);
      p = n ? n - 1 : 0;
      memcpy(prefix, buf, p);
      head = next;
      for (k = 0; k < sizeof(lens) / sizeof(lens[0]); k++)
      {
         size_t full = p + lens[k] + 1;
         unsigned bad = 0;
         RARCH_LOG("%.*s\n", (int)lens[k], body);
         n = verbosity_ring_read(head, buf, 1 << 16, &first, &next);
         CHECK(next == head + 1);
         head = next;
         CHECK(n == (full > 255 ? 256 : full));
         CHECK(n && buf[n - 1] == '\n');
         CHECK(n > p && !memcmp(buf, prefix, p));
         for (j = p; j + 1 < n; j++)
            bad += buf[j] != 'x';
         if (bad)
            fprintf(stderr, "length %u: %u bytes not the line's\n",
                  lens[k], bad);
         CHECK(!bad);
      }
      /* the sizing call GET_LOG's MCP arm makes: nothing written */
      CHECK(verbosity_ring_read(head - 1, NULL, 0, &first, &next) == 0);
      CHECK(next == head - 1);
   }

   /* the log file opened and closed under four logging threads */
   {
      FILE *fp;
      char  line[512];
      unsigned lines = 0, torn = 0;
      remove(SINK_FILE);
      retro_atomic_store_release_int(&done, 0);
      retro_atomic_store_release_int(&sink_stop, 0);
      for (i = 0; i < THREADS; i++)
         th[i] = sthread_create(sink_writer, (void*)(size_t)i);
      for (i = 0; i < SINK_TURNS; i++)
      {
         size_t at, now;
         retro_main_log_file_init(SINK_FILE, true);
         /* close it only with lines landing in it: the writers move
          * the ring's head */
         verbosity_ring_read((size_t)-1, NULL, 0, &first, &at);
         do
         {
            sthread_yield();
            verbosity_ring_read((size_t)-1, NULL, 0, &first, &now);
         } while (now < at + THREADS);
         retro_main_log_file_deinit();
      }
      retro_atomic_store_release_int(&sink_stop, 1);
      for (i = 0; i < THREADS; i++)
         sthread_join(th[i]);
      /* nothing logs now: a file still held at its close is closed */
      retro_main_log_file_init(NULL, false);
      retro_main_log_file_deinit();
      CHECK((fp = fopen(SINK_FILE, "rb")) != NULL);
      if (fp)
      {
         while (fgets(line, sizeof(line), fp))
         {
            char *e = strchr(line, '\n');
            lines++;
            if (!e || !line_whole(line, e))
               torn++;
         }
         fclose(fp);
      }
      CHECK(lines > 0);
      CHECK(torn == 0);
      CHECK(remove(SINK_FILE) == 0);
   }

   free(buf);
   printf("log_ring: %s (%u read, %u skipped)\n",
         failures ? "FAIL" : "PASS", got, skipped);
   return failures ? 1 : 0;
}
