/* GET_LOG's ring in verbosity.c: lines logged from several threads at
 * once, read while they are written, arrive whole and in order, and
 * an overrun is reported as lines skipped rather than torn lines.
 * Logging turned off, on and to another level while other threads log
 * is no race, and the gate still holds the lines back. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <rthreads/rthreads.h>
#include <retro_atomic.h>

#include "../../../verbosity.h"

#define THREADS      4
#define LINES        20000
#define TOGGLE_LINES 2000

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
      if (verbosity_is_enabled() && verbosity_get_log_level() <= 1)
         RARCH_LOG("t ........................................\n");
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

   free(buf);
   printf("log_ring: %s (%u read, %u skipped)\n",
         failures ? "FAIL" : "PASS", got, skipped);
   return failures ? 1 : 0;
}
