/* Every sample keeps its objects in its own directory.
 *
 * Dry-runs each sample Makefile (make -n -B) and fails on any object
 * written outside the sample, where the next sample to link that source
 * would take it whatever flags it was built with.
 *
 * The dry runs are started several at a time, one a processor, and read
 * in order.
 *
 *   tools/check.sh sample_objdir_check [DIR...]
 *   tools/check.sh sample_objdir_check --selftest */

#include "check_common.h"

#ifndef _WIN32
#include <sys/wait.h>
#endif

#define COUNT(a) (sizeof(a) / sizeof((a)[0]))
#define MAX_JOBS 16

static const char *const roots[]   = { "samples", "libretro-common/samples" };
/* the default goal, all and check */
static const char *const targets[] = { "", " all", " check" };

struct state
{
   struct ck_strv dirs;
   const char *base;
};

static int is_space(int c)
{
   return CK_IS_SPACE(c) || (c >= 0x1c && c <= 0x1f);
}

/* A command line split as a POSIX shell would split it; where a quote
 * or an escape is left open, at white space. */
static void split_line(const char *b, const char *e, struct ck_strv *argv)
{
   struct ck_sb tok = {0};
   const char *p;
   int in_tok = 0;
   int open   = 0;
   char quote = 0;

   for (p = b; p < e; p++)
   {
      char c = *p;
      if (quote)
      {
         if (c == quote)
            quote = 0;
         else if (c == '\\' && quote == '"')
         {
            if (++p == e)
            {
               open = 1;
               break;
            }
            if (*p != '\\' && *p != '"')
               ck_sb_putc(&tok, '\\');
            ck_sb_putc(&tok, *p);
         }
         else
            ck_sb_putc(&tok, c);
      }
      else if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
      {
         if (in_tok)
         {
            ck_sb_puts(&tok, "");
            ck_strv_push(argv, ck_strdup(tok.p));
            tok.len = 0;
            in_tok  = 0;
         }
      }
      else
      {
         in_tok = 1;
         if (c == '\\')
         {
            if (++p == e)
            {
               open = 1;
               break;
            }
            ck_sb_putc(&tok, *p);
         }
         else if (c == '\'' || c == '"')
            quote = c;
         else
            ck_sb_putc(&tok, c);
      }
   }
   if (open || quote)
   {
      /* left open: every run of white space separates */
      ck_strv_free(argv);
      for (p = b; p < e; )
      {
         const char *w;
         while (p < e && is_space((unsigned char)*p))
            p++;
         for (w = p; p < e && !is_space((unsigned char)*p); p++);
         if (p > w)
            ck_strv_push(argv, ck_strndup(w, (size_t)(p - w)));
      }
   }
   else if (in_tok)
   {
      ck_sb_puts(&tok, "");
      ck_strv_push(argv, ck_strdup(tok.p));
   }
   ck_sb_free(&tok);
}

/* A path with its symbolic links followed as far as it exists, and
 * the rest of it normalised. */
static char *real_path(const char *path, int depth)
{
   struct ck_sb res = {0};
   struct ck_sb abs = {0};
   const char *p;

   if (path[0] != '/'
#ifdef _WIN32
         && !(path[0] && path[1] == ':')
#endif
      )
   {
      char cwd[4096];
#ifdef _WIN32
      if (!_getcwd(cwd, sizeof(cwd)))
#else
      if (!getcwd(cwd, sizeof(cwd)))
#endif
         ck_die("cannot read the current directory");
      ck_sb_puts(&abs, cwd);
      ck_sb_putc(&abs, '/');
   }
   ck_sb_puts(&abs, path);
   ck_sb_puts(&res, "");
   for (p = abs.p; *p; )
   {
      const char *c = p;
      size_t n, keep;
      while (*p && *p != '/')
         p++;
      n = (size_t)(p - c);
      while (*p == '/')
         p++;
      if (!n || (n == 1 && c[0] == '.'))
         continue;
      if (n == 2 && c[0] == '.' && c[1] == '.')
      {
         while (res.len && res.p[res.len - 1] != '/')
            res.len--;
         if (res.len)
            res.len--;
         res.p[res.len] = '\0';
         continue;
      }
      keep = res.len;
      ck_sb_putc(&res, '/');
      ck_sb_add(&res, c, n);
#ifndef _WIN32
      if (depth < 40 && ck_is_link(res.p))
      {
         char link[4096];
         long got = (long)readlink(res.p, link, sizeof(link) - 1);
         if (got > 0)
         {
            struct ck_sb to = {0};
            char *target;
            link[got] = '\0';
            if (link[0] != '/')
            {
               ck_sb_add(&to, res.p, keep);
               ck_sb_putc(&to, '/');
            }
            ck_sb_puts(&to, link);
            target  = real_path(to.p, depth + 1);
            res.len = 0;
            ck_sb_puts(&res, target);
            free(target);
            ck_sb_free(&to);
         }
      }
#else
      (void)depth;
      (void)keep;
#endif
   }
   if (!res.len)
      ck_sb_puts(&res, "/");
   ck_sb_free(&abs);
   return res.p;
}

static int outside(const char *sample_dir, const char *path)
{
   struct ck_sb joined = {0};
   char *base, *full;
   size_t n;
   int out;

   if (path[0] != '/')
   {
      ck_sb_puts(&joined, sample_dir);
      ck_sb_putc(&joined, '/');
   }
   ck_sb_puts(&joined, path);
   base = real_path(sample_dir, 0);
   full = real_path(joined.p, 0);
   n    = strlen(base);
   out  = strncmp(base, full, n) || (full[n] && full[n] != '/' && n > 1);
   free(base);
   free(full);
   ck_sb_free(&joined);
   return out;
}

/* Objects in a dry run's output that land outside sample_dir: what a
 * compile line writes with -o, where it ends in .o or .obj. */
static void stray(const char *sample_dir, const char *text,
      struct ck_strv *bad)
{
   const char *line = text;
   while (*line)
   {
      struct ck_strv argv = {0};
      const char *e       = line;
      size_t i, k;

      while (*e && *e != '\n' && *e != '\r' && *e != '\v' && *e != '\f'
            && !(*e >= 0x1c && *e <= 0x1e))
         e++;
      split_line(line, e, &argv);
      for (i = 0; i < argv.n; i++)
      {
         const char *a = argv.v[i];
         const char *o = NULL;
         if (!strcmp(a, "-o") && i + 1 < argv.n)
            o = argv.v[++i];
         else if (!strncmp(a, "-o", 2) && a[2] && a[2] != 'n')
            o = a + 2;
         if (!o || !(ck_ends_with(o, ".o") || ck_ends_with(o, ".obj"))
               || !outside(sample_dir, o))
            continue;
         for (k = 0; k < bad->n && strcmp(bad->v[k], o); k++);
         if (k == bad->n)
            ck_strv_push(bad, ck_strdup(o));
      }
      ck_strv_free(&argv);
      line = *e ? e + 1 : e;
   }
}

static void quote_arg(struct ck_sb *sb, const char *s)
{
#ifdef _WIN32
   ck_sb_putc(sb, '"');
   ck_sb_puts(sb, s);
   ck_sb_putc(sb, '"');
#else
   ck_sb_putc(sb, '\'');
   for (; *s; s++)
   {
      if (*s == '\'')
         ck_sb_puts(sb, "'\\''");
      else
         ck_sb_putc(sb, *s);
   }
   ck_sb_putc(sb, '\'');
#endif
}

static int processors(void)
{
   long n = 2;
#ifdef _WIN32
   SYSTEM_INFO si;
   GetSystemInfo(&si);
   n = (long)si.dwNumberOfProcessors;
#elif defined(_SC_NPROCESSORS_ONLN)
   n = sysconf(_SC_NPROCESSORS_ONLN);
#endif
   return n < 2 ? 2 : n > MAX_JOBS ? MAX_JOBS : (int)n;
}

/* What the dry runs of every directory print, in ok[] whether any of
 * a directory's three could be run. Each writes to a file of its own,
 * so none waits on a reader. */
static void dry_runs(const struct ck_strv *dirs, char **text, int *ok)
{
   char *tmp        = ck_tmpdir();
   size_t total     = dirs->n * COUNT(targets);
   int jobs         = processors();
   struct ck_sb cmd = {0};
   FILE *running[MAX_JOBS];
   size_t at, i;
#ifdef _WIN32
   const char *limit = "";
   const char *sink  = "2>NUL";
#else
   const char *sink  = "2>/dev/null";
   const char *limit = system("command -v timeout >/dev/null 2>&1") == 0
      ? "timeout 120 " : "";
#endif

   for (i = 0; i < dirs->n; i++)
   {
      text[i] = NULL;
      ok[i]   = 0;
   }
   for (at = 0; at < total; at += (size_t)jobs)
   {
      size_t n = total - at < (size_t)jobs ? total - at : (size_t)jobs;
      for (i = 0; i < n; i++)
      {
         char num[32];
         sprintf(num, "/%lu.out", (unsigned long)(at + i));
         cmd.len = 0;
         ck_sb_puts(&cmd, limit);
         ck_sb_puts(&cmd, "make -n -B -C ");
         quote_arg(&cmd, dirs->v[(at + i) / COUNT(targets)]);
         ck_sb_puts(&cmd, targets[(at + i) % COUNT(targets)]);
         ck_sb_puts(&cmd, " > ");
         ck_sb_puts(&cmd, tmp);
         ck_sb_puts(&cmd, num);
         ck_sb_putc(&cmd, ' ');
         ck_sb_puts(&cmd, sink);
#ifdef _WIN32
         running[i] = _popen(cmd.p, "r");
#else
         running[i] = popen(cmd.p, "r");
#endif
      }
      for (i = 0; i < n; i++)
      {
         size_t d = (at + i) / COUNT(targets);
         int status;
         char num[32];
         if (!running[i])
            continue;
#ifdef _WIN32
         status = _pclose(running[i]);
#else
         status = pclose(running[i]);
         status = WIFEXITED(status) ? WEXITSTATUS(status) : 1;
#endif
         if (status == 0)
         {
            struct ck_sb all = {0};
            char *out;
            sprintf(num, "/%lu.out", (unsigned long)(at + i));
            cmd.len = 0;
            ck_sb_puts(&cmd, tmp);
            ck_sb_puts(&cmd, num);
            out = ck_read_file(cmd.p, NULL);
            ck_sb_puts(&all, text[d] ? text[d] : "");
            ck_sb_puts(&all, out ? out : "");
            free(out);
            free(text[d]);
            text[d] = all.p;
            ok[d]   = 1;
         }
      }
   }
   ck_rmtree(tmp);
   free(tmp);
   ck_sb_free(&cmd);
}

/* Sample directories; the Makefile at a root runs the samples through
 * $(MAKE), which make -n would run for real, and builds nothing of its
 * own. */
static void add_makefile(const char *path, const char *rel, void *ud)
{
   struct state *st  = (struct state*)ud;
   const char *slash = strrchr(path, '/');
   (void)rel;
   if (!slash || strcmp(slash + 1, "Makefile")
         || ((size_t)(slash - path) == strlen(st->base)
            && !strncmp(path, st->base, strlen(st->base))))
      return;
   ck_strv_push(&st->dirs, ck_strndup(path, (size_t)(slash - path)));
}

static int selftest(void)
{
   static const char d[] = "samples/x/y";
   static const char *const cases[][2] = {
      { "cc -c -o a.o a.c", NULL },
      { "cc -c -oobj/a.o a.c", NULL },
      { "cc -c -o obj/__/__/__/f/b.o ../../../f/b.c", NULL },
      { "cc -c -o ../../../f/b.o ../../../f/b.c", "../../../f/b.o" },
      { "cc -c -o../../../f/b.o ../../../f/b.c", "../../../f/b.o" },
      { "cc -o test a.o ../../../f/b.o", NULL },
      { "x86_64-w64-mingw32-gcc -c -o ../z.obj z.c", "../z.obj" }
   };
   size_t i;
   int fails = 0;

   for (i = 0; i < COUNT(cases); i++)
   {
      struct ck_strv got = {0};
      const char *want   = cases[i][1];
      stray(d, cases[i][0], &got);
      if (want ? got.n != 1 || strcmp(got.v[0], want) : got.n != 0)
      {
         printf("selftest: '%s' gave %s%s%s, want %s%s%s\n", cases[i][0],
               got.n ? "['" : "[", got.n ? got.v[0] : "", got.n ? "']" : "]",
               want ? "['" : "[", want ? want : "", want ? "']" : "]");
         fails++;
      }
      ck_strv_free(&got);
   }
   printf("selftest: %lu case(s), %d failure(s)\n",
         (unsigned long)COUNT(cases), fails);
   return fails ? 1 : 0;
}

int main(int argc, char **argv)
{
   struct state st;
   char **text;
   int *ok;
   size_t i;
   int bad = 0;

   if (argc == 2 && !strcmp(argv[1], "--selftest"))
      return selftest();
   memset(&st, 0, sizeof(st));
   if (argc > 1)
   {
      int a;
      for (a = 1; a < argc; a++)
         ck_strv_push(&st.dirs, ck_strdup(argv[a]));
   }
   else
   {
      for (i = 0; i < COUNT(roots); i++)
      {
         struct ck_sb base = {0};
         ck_sb_puts(&base, "./");
         ck_sb_puts(&base, roots[i]);
         st.base = base.p;
         ck_walk(base.p, NULL, add_makefile, &st);
         ck_sb_free(&base);
      }
      ck_strv_sort(&st.dirs);
   }
   text = (char**)ck_alloc((st.dirs.n + 1) * sizeof(char*));
   ok   = (int*)ck_alloc((st.dirs.n + 1) * sizeof(int));
   dry_runs(&st.dirs, text, ok);
   for (i = 0; i < st.dirs.n; i++)
   {
      struct ck_strv objs = {0};
      if (!ok[i])
      {
         bad++;
         printf("%s: make cannot dry-run it, so it cannot be checked\n",
               st.dirs.v[i]);
         continue;
      }
      stray(st.dirs.v[i], text[i], &objs);
      if (objs.n)
      {
         bad++;
         printf("%s: %lu object(s) outside the sample, e.g. %s\n",
               st.dirs.v[i], (unsigned long)objs.n, objs.v[0]);
      }
      ck_strv_free(&objs);
      free(text[i]);
   }
   printf("%d of %lu sample(s) build objects outside their directory\n",
         bad, (unsigned long)st.dirs.n);
   return bad ? 1 : 0;
}
