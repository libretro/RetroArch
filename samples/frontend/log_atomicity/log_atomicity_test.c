/* Concurrent log lines arrive whole.
 *
 * Links the shipping RetroArch objects with only main() replaced and
 * hammers RARCH_LOG from two threads into a log file. The generic
 * branch of RARCH_LOG_V formats tag and message into one buffer and
 * writes it with one stdio call, so stdio's per-call lock keeps
 * concurrent lines whole; the shape it replaced - a tag call
 * followed by a body call - left a window where a thread switch
 * interleaved one writer's tag into the other's message.
 *
 * The claims: with two threads each writing distinct fixed-width
 * payloads, every line of the resulting file is exactly a tag
 * followed by one complete payload - no torn, merged, or
 * tag-inside-body lines - and the file carries every line both
 * writers sent. A long-payload lane crosses the stack-buffer
 * boundary so the heap detour ships full lines too.
 *
 * The pre-change shape fails this probabilistically, not
 * deterministically - the tear needs a thread switch inside the
 * two-call window - so the lane's value is the invariant it pins
 * down, and any future regression to split writes has a hammer
 * standing on it.
 *
 * Requires a completed non-Qt build:
 *
 *   ./configure --disable-qt && make
 *   samples/frontend/log_atomicity/build.sh
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <boolean.h>
#include <rthreads/rthreads.h>

#include "../../../verbosity.h"

static unsigned failures = 0;

#define CHECK(cond, ...) \
   do { \
      if (!(cond)) \
      { \
         fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); \
         fprintf(stderr, __VA_ARGS__); \
         fprintf(stderr, "\n"); \
         failures++; \
      } \
   } while (0)

#define LINES_PER_THREAD 4000
#define SHORT_WIDTH      60
/* Wider than RARCH_LOG_V's 1024-byte stack line, so these take the
 * exact-sized heap detour. */
#define LONG_WIDTH       1500

typedef struct writer_arg
{
   char ch;
   unsigned lines;
   unsigned width;
} writer_arg_t;

static void writer(void *data)
{
   writer_arg_t *arg = (writer_arg_t*)data;
   char payload[LONG_WIDTH + 1];
   unsigned i;

   memset(payload, arg->ch, arg->width);
   payload[arg->width] = '\0';

   for (i = 0; i < arg->lines; i++)
      RARCH_LOG("%s\n", payload);
}

/* Every line must be "[INFO] " followed by one run of a single
 * repeated character of an expected width. Returns the payload
 * character, or 0 on a malformed line. */
static char classify_line(const char *line, size_t len,
      unsigned short_w, unsigned long_w)
{
   static const char tag[] = "[INFO] ";
   size_t tag_len          = sizeof(tag) - 1;
   size_t body;
   size_t i;
   char c;

   if (len <= tag_len)
      return 0;
   if (strncmp(line, tag, tag_len) != 0)
      return 0;

   body = len - tag_len;
   if (body != short_w && body != long_w)
      return 0;

   c = line[tag_len];
   for (i = tag_len + 1; i < len; i++)
      if (line[i] != c)
         return 0;
   return c;
}

int main(int argc, char *argv[])
{
   char log_path[512];
   sthread_t *ta;
   sthread_t *tb;
   writer_arg_t a;
   writer_arg_t b;
   writer_arg_t c;
   FILE *f;
   char *line       = NULL;
   size_t line_cap  = 0;
   unsigned count_a = 0;
   unsigned count_b = 0;
   unsigned count_c = 0;
   unsigned bad     = 0;

   (void)argc;
   (void)argv;

   snprintf(log_path, sizeof(log_path),
         "/tmp/log_atomicity_%ld.log", (long)getpid());

   verbosity_enable();
   retro_main_log_file_init(log_path, false);

   /* Two short-payload writers race the per-line path; the third
    * crosses into the heap detour. */
   a.ch = 'A'; a.lines = LINES_PER_THREAD; a.width = SHORT_WIDTH;
   b.ch = 'B'; b.lines = LINES_PER_THREAD; b.width = SHORT_WIDTH;
   c.ch = 'C'; c.lines = LINES_PER_THREAD / 4; c.width = LONG_WIDTH;

   ta = sthread_create(writer, &a);
   tb = sthread_create(writer, &b);
   CHECK(ta && tb, "fixture: thread creation failed");
   if (ta) sthread_join(ta);
   if (tb) sthread_join(tb);

   /* The long lane runs against a fresh short-writer so the heap
    * detour is also raced, not just exercised. */
   a.lines = LINES_PER_THREAD / 4;
   ta = sthread_create(writer, &a);
   tb = sthread_create(writer, &c);
   CHECK(ta && tb, "fixture: second thread pair failed");
   if (ta) sthread_join(ta);
   if (tb) sthread_join(tb);

   /* Flush-policy lane: every line, whatever its tag, is durable the
    * moment the call returns - readable from the file before any
    * deinit. The informational line is the one that matters: it is
    * what a crash log's tail and a live follower both depend on. */
   RARCH_LOG("flush-lane info line\n");
   {
      FILE *ef;
      int   found = 0;
      if ((ef = fopen(log_path, "rb")))
      {
         char   *eline    = NULL;
         size_t  ecap     = 0;
         ssize_t elen;
         while ((elen = getline(&eline, &ecap, ef)) != -1)
            if (strstr(eline, "flush-lane info line"))
               found = 1;
         free(eline);
         fclose(ef);
      }
      CHECK(found,
            "RARCH_LOG line not durable before deinit (flush policy)");
   }

   retro_main_log_file_deinit();

   if (!(f = fopen(log_path, "rb")))
   {
      fprintf(stderr, "FAIL: cannot reopen %s\n", log_path);
      return 1;
   }

   {
      ssize_t n;
      while ((n = getline(&line, &line_cap, f)) > 0)
      {
         /* The flush-policy lane's marker line has its own shape. */
         if (strstr(line, "flush-lane info line"))
            continue;
         size_t len = (size_t)n;
         char which;

         if (len && line[len - 1] == '\n')
            len--;
         if (!len)
            continue;

         which = classify_line(line, len, SHORT_WIDTH, LONG_WIDTH);
         switch (which)
         {
            case 'A': count_a++; break;
            case 'B': count_b++; break;
            case 'C': count_c++; break;
            default:
               if (bad < 3)
                  fprintf(stderr, "torn line (%u bytes): %.90s\n",
                        (unsigned)len, line);
               bad++;
               break;
         }
      }
   }
   free(line);
   fclose(f);
   unlink(log_path);

   CHECK(bad == 0, "%u torn or malformed lines", bad);
   CHECK(count_a == LINES_PER_THREAD + LINES_PER_THREAD / 4,
         "writer A lines: %u, want %u",
         count_a, LINES_PER_THREAD + LINES_PER_THREAD / 4);
   CHECK(count_b == LINES_PER_THREAD,
         "writer B lines: %u, want %u", count_b, LINES_PER_THREAD);
   CHECK(count_c == LINES_PER_THREAD / 4,
         "writer C (heap detour) lines: %u, want %u",
         count_c, LINES_PER_THREAD / 4);

   if (failures)
   {
      fprintf(stderr, "FAILURES (%u)\n", failures);
      return 1;
   }
   printf("log_atomicity: all lanes passed\n");
   return 0;
}
