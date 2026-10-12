/* The GL drivers hand the threaded wrapper's hardware ring a fence taken
 * ahead of the buffer swap. A fence taken after it completes only when a
 * driver that queues swaps releases that swap, which holds a hardware
 * rendered core to the display's rate in fast-forward.
 *
 * Fails if a driver's frame function swaps before taking the fence, or if
 * its hand-over (hw_ring_fence_signal) stops using the one taken.
 *
 * The thread that waits on the fence does not delete it either: the video
 * thread made it and can still be in the swap that follows it, and a
 * driver has aborted on the delete. Fails if hw_ring_fence_wait deletes.
 *
 * Usage, from the repository root:
 *    tools/check.sh gl_ring_fence_check [--selftest] */

#include "check_common.h"

static const char *const drivers[][2] = {
   { "gfx/drivers/gl2.c", "gl2" },
   { "gfx/drivers/gl3.c", "gl3" }
};

/* A function's text: from the line  ^static \w+ name\(  to the next
 * "\n}\n", or NULL. */
static char *body(const char *src, const char *prefix, const char *suffix)
{
   struct ck_sb name = {0};
   const char *s     = src;
   char *out         = NULL;

   ck_sb_puts(&name, prefix);
   ck_sb_puts(&name, suffix);
   ck_sb_putc(&name, '(');
   for (; *s; )
   {
      const char *p = s;
      const char *nl = strchr(s, '\n');
      if (!strncmp(p, "static ", 7))
      {
         const char *w;
         for (w = p += 7; CK_IS_WORD(*p); p++);
         if (p > w && *p == ' ' && !strncmp(p + 1, name.p, name.len))
         {
            const char *end = strstr(s, "\n}\n");
            if (end)
               out = ck_strndup(s, (size_t)(end - s));
            break;
         }
      }
      if (!nl)
         break;
      s = nl + 1;
   }
   ck_sb_free(&name);
   return out;
}

static long pos_of(const char *hay, const char *needle)
{
   const char *p = strstr(hay, needle);
   return p ? (long)(p - hay) : -1;
}

static void err(struct ck_strv *errs, const char *a, const char *b,
      const char *c, const char *d)
{
   struct ck_sb m = {0};
   ck_sb_puts(&m, a);
   ck_sb_puts(&m, b);
   ck_sb_puts(&m, c ? c : "");
   ck_sb_puts(&m, d ? d : "");
   ck_strv_push(errs, m.p);
}

static void check(const char *src, const char *prefix, struct ck_strv *errs)
{
   char *frame  = body(src, prefix, "_frame");
   char *signal = body(src, prefix, "_hw_ring_fence_signal");
   char *wait;

   if (!frame || !signal)
   {
      struct ck_sb m = {0};
      ck_sb_puts(&m, prefix);
      ck_sb_puts(&m, "_frame or ");
      ck_sb_puts(&m, prefix);
      ck_sb_puts(&m, "_hw_ring_fence_signal not found");
      ck_strv_push(errs, m.p);
      free(frame);
      free(signal);
      return;
   }
   {
      struct ck_sb drawn_call = {0};
      long swap, drawn;
      ck_sb_puts(&drawn_call, prefix);
      ck_sb_puts(&drawn_call, "_hw_ring_drawn(gl);");
      swap  = pos_of(frame, "swap_buffers(");
      drawn = pos_of(frame, drawn_call.p);
      if (swap < 0)
         err(errs, prefix, "_frame: no swap_buffers call", NULL, NULL);
      else if (drawn < 0 || drawn > swap)
      {
         struct ck_sb m = {0};
         ck_sb_puts(&m, prefix);
         ck_sb_puts(&m, "_frame: swaps before ");
         ck_sb_puts(&m, prefix);
         ck_sb_puts(&m, "_hw_ring_drawn(gl)");
         ck_strv_push(errs, m.p);
      }
      ck_sb_free(&drawn_call);
   }
   if (!strstr(signal, "hw_ring_done_sync"))
      err(errs, prefix, "_hw_ring_fence_signal: does not hand over"
            " hw_ring_done_sync", NULL, NULL);
   wait = body(src, prefix, "_hw_ring_fence_wait");
   if (wait && strstr(wait, "glDeleteSync"))
      err(errs, prefix, "_hw_ring_fence_wait: deletes the video thread's"
            " sync", NULL, NULL);
   free(wait);
   free(frame);
   free(signal);
}

static char *replace(const char *text, const char *from, const char *to)
{
   struct ck_sb out = {0};
   size_t n         = strlen(from);
   const char *p    = text;
   const char *q;
   ck_sb_puts(&out, "");
   while ((q = strstr(p, from)))
   {
      ck_sb_add(&out, p, (size_t)(q - p));
      ck_sb_puts(&out, to);
      p = q + n;
   }
   ck_sb_puts(&out, p);
   return out.p;
}

static size_t count(const char *src)
{
   struct ck_strv errs = {0};
   size_t n;
   check(src, "glx", &errs);
   n = errs.n;
   ck_strv_free(&errs);
   return n;
}

static int selftest(void)
{
   static const char good[] =
      "static bool glx_frame(void *d)\n{\n   glx_hw_ring_drawn(gl);\n"
      "   gl->ctx_driver->swap_buffers(gl->ctx_data);\n   return true;\n}\n"
      "static void glx_hw_ring_fence_signal(void *d, void *f)\n{\n"
      "   f->sync = gl->hw_ring_done_sync;\n}\n";
   static const char wait_fn[] =
      "static bool glx_hw_ring_fence_wait(void *d, void *f, unsigned t)\n{\n"
      "   glDeleteSync(f->sync);\n   return true;\n}\n";
   struct ck_sb deletes = {0};
   char *tmp    = replace(good, "   glx_hw_ring_drawn(gl);\n", "");
   char *late   = replace(tmp, "   return true;",
         "   glx_hw_ring_drawn(gl);\n   return true;");
   char *unused = replace(good, "gl->hw_ring_done_sync", "glFenceSync(0, 0)");
   int ok;

   ck_sb_puts(&deletes, good);
   ck_sb_puts(&deletes, wait_fn);
   ok = count(good) == 0 && count(late) == 1 && count(unused) == 1
      && count(deletes.p) == 1;
   free(tmp);
   free(late);
   free(unused);
   ck_sb_free(&deletes);
   if (!ok)
   {
      printf("gl_ring_fence_check: selftest FAILED\n");
      return 1;
   }
   printf("gl_ring_fence_check: selftest ok\n");
   return 0;
}

int main(int argc, char **argv)
{
   struct ck_strv errs = {0};
   size_t d, i;
   int a;

   for (a = 1; a < argc; a++)
   {
      if (!strcmp(argv[a], "--selftest"))
         return selftest();
   }
   if (argc > 1)
   {
      fprintf(stderr, "usage: gl_ring_fence_check [--selftest]\n");
      return 2;
   }
   for (d = 0; d < sizeof(drivers) / sizeof(drivers[0]); d++)
   {
      struct ck_strv found = {0};
      char *src = ck_read_file(drivers[d][0], NULL);
      if (!src)
      {
         fprintf(stderr, "gl_ring_fence_check: cannot read %s\n",
               drivers[d][0]);
         return 1;
      }
      check(src, drivers[d][1], &found);
      for (i = 0; i < found.n; i++)
      {
         struct ck_sb m = {0};
         ck_sb_puts(&m, drivers[d][0]);
         ck_sb_puts(&m, ": ");
         ck_sb_puts(&m, found.v[i]);
         ck_strv_push(&errs, m.p);
      }
      ck_strv_free(&found);
      free(src);
   }
   for (i = 0; i < errs.n; i++)
      printf("FAIL %s\n", errs.v[i]);
   if (!errs.n)
      printf("gl_ring_fence_check: ok\n");
   return errs.n ? 1 : 0;
}
