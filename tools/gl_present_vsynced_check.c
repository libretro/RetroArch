/* The GL drivers tell the threaded presenter whether the swap just made
 * waited for the vertical blank (get_last_present_wait), from a flag they
 * keep beside the swap interval. An interval set without the flag leaves
 * the presenter subtracting a wait that was not made, or counting one
 * that was as drawing time.
 *
 * Fails if a function in a driver sets the context's swap interval and
 * does not set the flag after it.
 *
 * Usage, from the repository root:
 *    tools/check.sh gl_present_vsynced_check [--selftest] */

#include "check_common.h"

static const char *const drivers[][2] = {
   { "gfx/drivers/gl2.c", "gl2_present_vsynced" },
   { "gfx/drivers/gl3.c", "gl->present_vsynced" }
};

#define CALL "ctx_driver->swap_interval("

static const char *find_in(const char *b, const char *e, const char *s)
{
   size_t n = strlen(s);
   const char *p;
   for (p = b; (size_t)(e - p) >= n; p++)
   {
      if (*p == *s && !memcmp(p, s, n))
         return p;
   }
   return NULL;
}

/* The last  ^\w[^\n;]*?\b(\w+)\(  of a chunk (re.M): on a line that
 * starts with a word character, the first later word that a '(' follows
 * directly, with no ';' or line break before it. */
static int last_name(const char *b, const char *e, const char **nb,
      const char **ne)
{
   const char *s = b;
   int found     = 0;
   while (s < e)
   {
      const char *nl   = (const char*)memchr(s, '\n', (size_t)(e - s));
      const char *next = nl ? nl + 1 : e;
      if (CK_IS_WORD(*s))
      {
         const char *p;
         for (p = s + 1; p < e && *p != '\n' && *p != ';'; p++)
         {
            const char *r;
            if (!CK_IS_WORD(*p) || CK_IS_WORD(p[-1]))
               continue;
            for (r = p; r < e && CK_IS_WORD(*r); r++);
            if (r < e && *r == '(')
            {
               *nb   = p;
               *ne   = r;
               found = 1;
               for (next = r + 1; next < e && next[-1] != '\n'; next++);
               break;
            }
         }
      }
      s = next;
   }
   return found;
}

/* The functions of src split at lines that are a lone "}" (^\}\n). */
static void check(const char *src, size_t len, const char *flag,
      struct ck_strv *errs)
{
   const char *e     = src + len;
   const char *chunk = src;
   struct ck_sb set  = {0};

   ck_sb_puts(&set, flag);
   ck_sb_puts(&set, " =");
   while (chunk <= e)
   {
      const char *ce = chunk;
      const char *call = NULL, *p;
      /* the next "}\n" at a line start */
      for (;;)
      {
         const char *q = find_in(ce, e, "}\n");
         if (!q)
         {
            ce = e;
            break;
         }
         if (q == src || q[-1] == '\n')
         {
            ce = q;
            break;
         }
         ce = q + 1;
      }
      for (p = chunk; (p = find_in(p, ce, CALL)); p++)
         call = p;
      if (call && !find_in(call, ce, set.p))
      {
         struct ck_sb m = {0};
         const char *nb, *ne;
         if (last_name(chunk, ce, &nb, &ne))
            ck_sb_add(&m, nb, (size_t)(ne - nb));
         else
            ck_sb_puts(&m, "?");
         ck_sb_puts(&m, ": sets the swap interval without ");
         ck_sb_puts(&m, flag);
         ck_strv_push(errs, m.p);
      }
      if (ce == e)
         break;
      chunk = ce + 2;
   }
   ck_sb_free(&set);
}

static int check_text(const char *src, const char *flag, struct ck_strv *errs)
{
   check(src, strlen(src), flag, errs);
   return (int)errs->n;
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

static int selftest(void)
{
   static const char good[] =
      "static void glx_set_nonblock_state(void *d, bool state)\n{\n"
      "   if (gl->ctx_driver->swap_interval)\n"
      "      gl->ctx_driver->swap_interval(gl->ctx_data, interval);\n"
      "   glx_vsynced = (interval != 0);\n}\n";
   static const char other[] = "static int glx_other(void)\n{\n   return 0;\n}\n";
   struct ck_strv errs = {0};
   struct ck_sb t      = {0};
   char *unset, *early, *tmp;
   int ok = 1;

   unset = replace(good, "   glx_vsynced = (interval != 0);\n", "");
   tmp   = replace(good, "   glx_vsynced = (interval != 0);\n", "");
   early = replace(tmp, "   if (gl->ctx_driver->swap_interval)\n",
         "   glx_vsynced = (interval != 0);\n"
         "   if (gl->ctx_driver->swap_interval)\n");

   ck_sb_puts(&t, good);
   ck_sb_puts(&t, other);
   ok &= check_text(t.p, "glx_vsynced", &errs) == 0;
   ck_strv_free(&errs);
   t.len = 0;
   ck_sb_puts(&t, other);
   ck_sb_puts(&t, unset);
   ok &= check_text(t.p, "glx_vsynced", &errs) == 1 && !strcmp(errs.v[0],
         "glx_set_nonblock_state: sets the swap interval without glx_vsynced");
   ck_strv_free(&errs);
   ok &= check_text(early, "glx_vsynced", &errs) == 1;
   ck_strv_free(&errs);
   free(unset);
   free(tmp);
   free(early);
   ck_sb_free(&t);
   if (!ok)
   {
      printf("gl_present_vsynced_check: selftest FAILED\n");
      return 1;
   }
   printf("gl_present_vsynced_check: selftest ok\n");
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
      fprintf(stderr, "usage: gl_present_vsynced_check [--selftest]\n");
      return 2;
   }
   for (d = 0; d < sizeof(drivers) / sizeof(drivers[0]); d++)
   {
      struct ck_strv found = {0};
      size_t len;
      char *src = ck_read_file(drivers[d][0], &len);
      if (!src)
      {
         fprintf(stderr, "gl_present_vsynced_check: cannot read %s\n",
               drivers[d][0]);
         return 1;
      }
      check(src, len, drivers[d][1], &found);
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
      printf("gl_present_vsynced_check: ok\n");
   return errs.n ? 1 : 0;
}
