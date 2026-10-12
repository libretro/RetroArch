/* gfx_ctx_driver_t::presentable() contract check.
 *
 * presentable() tells the runloop the context has nothing to present to,
 * so that the pacing happens once, in the runloop, instead of each
 * context sleeping inside its own swap path. The hook is optional and
 * sits last in gfx_ctx_driver_t, so a driver that does not implement it
 * leaves the slot NULL and keeps the old meaning.
 *
 * That shape makes two mistakes easy and silent on every build but one:
 *
 * 1. The function casts `data` to the wrong driver's context type. Each
 *    context driver names its own struct, several are near-identical
 *    (android_ctx_data_t vs android_ctx_data_vk_t), and a file is
 *    compiled by one platform's CI, so the wrong name is a build break
 *    somebody else discovers. This actually happened.
 *
 * 2. The function is written but never wired into the vtable, or a
 *    vtable names a function that does not exist in that file. The
 *    first is silent - the hook simply never runs and the loop spins as
 *    it did before - and is exactly what the feature exists to prevent.
 *
 * So, per file that has one: presentable() must cast to the same type
 * swap_buffers() does, and must be referenced exactly once from a
 * gfx_ctx_driver_t initializer in the same file. Both are textual
 * checks; nothing here needs the file to compile, which is the point,
 * since no machine compiles all of them.
 *
 * Every check reads the file with comments and the contents of string
 * and character literals blanked out: the tree spells an unimplemented
 * vtable slot `NULL, (a comment naming presentable)` - the slot's name,
 * in a comment, inside the initializer. Read as code that is a vtable
 * naming a function nobody defines, which is what the SDL 1.2 GL context
 * was reported as the moment it got a file of its own.
 *
 * A vtable may name a shared implementation - x11_presentable() serves
 * the three X11 contexts from x11_common.c - as long as gfx/ outside
 * gfx/drivers_context defines it; that is looked for under the current
 * directory's gfx/, whatever --root says. Names a vtable gets wrong are
 * reported in name order.
 *
 * Usage:
 *    tools/check.sh ctx_presentable_check [--selftest] [--root DIR]
 *
 * Exit status is 0 when the tree is consistent, 1 otherwise. */

#include "check_common.h"

#define CTX_DIR "gfx/drivers_context"
#define HOOK    "presentable"
#define IS_ID(c) (CK_IS_WORD(c))

static const char *ws(const char *p, const char *e)
{
   while (p < e && CK_IS_SPACE(*p))
      p++;
   return p;
}

/* A string literal, a character literal, a block comment or a line
 * comment - whichever starts first - with every non-space byte of it
 * made a space, so offsets keep. */
static void blank_noncode(char *t, size_t n)
{
   size_t i = 0;
   while (i < n)
   {
      size_t j = 0;
      int hit  = 0;
      if (t[i] == '"' || t[i] == '\'')
      {
         char q = t[i];
         for (j = i + 1; j < n && t[j] != q; j++)
         {
            if (t[j] == '\\')
            {
               if (j + 1 >= n)
               {
                  j = n;
                  break;
               }
               j++;
            }
            else if (t[j] == '\n')
               break;
         }
         hit = j < n && t[j] == q;
         j++;
      }
      else if (t[i] == '/' && i + 1 < n && t[i + 1] == '*')
      {
         for (j = i + 2; j + 1 < n && !(t[j] == '*' && t[j + 1] == '/'); j++);
         hit = j + 1 < n;
         j  += 2;
      }
      else if (t[i] == '/' && i + 1 < n && t[i + 1] == '/')
      {
         for (j = i; j < n && t[j] != '\n'; j++);
         hit = 1;
      }
      if (!hit)
      {
         i++;
         continue;
      }
      for (; i < j; i++)
      {
         if (!CK_IS_SPACE(t[i]))
            t[i] = ' ';
      }
   }
}

/* ^static\s+(bool|void)\s+(name)\s*\(\s*void\s*\*\s*data\s*\)  at p, a
 * line start; returns the end and the name, or NULL. */
static const char *def_at(const char *p, const char *e, const char *type,
      const char **nb, const char **ne)
{
   const char *q;
   size_t tl = strlen(type);
   if (e - p < 7 || memcmp(p, "static", 6))
      return NULL;
   q = ws(p + 6, e);
   if (q == p + 6 || (size_t)(e - q) < tl || memcmp(q, type, tl))
      return NULL;
   p = q + tl;
   q = ws(p, e);
   if (q == p)
      return NULL;
   for (*nb = q; q < e && IS_ID(*q); q++);
   *ne = q;
   if (*ne == *nb)
      return NULL;
   q = ws(q, e);
   if (q >= e || *q != '(')
      return NULL;
   q = ws(q + 1, e);
   if (e - q < 4 || memcmp(q, "void", 4))
      return NULL;
   q = ws(q + 4, e);
   if (q >= e || *q != '*')
      return NULL;
   q = ws(q + 1, e);
   if (e - q < 4 || memcmp(q, "data", 4))
      return NULL;
   q = ws(q + 4, e);
   if (q >= e || *q != ')')
      return NULL;
   return q + 1;
}

/* The braced body of the function whose signature ends at start. */
static void body_of(const char *t, const char *e, const char *start,
      const char **bb, const char **be)
{
   const char *i = (const char*)memchr(start, '{', (size_t)(e - start));
   const char *j;
   int depth = 0;
   *bb = *be = start;
   if (!i)
      return;
   *bb = i;
   for (j = i; j < e; j++)
   {
      if (*j == '{')
         depth++;
      else if (*j == '}' && --depth == 0)
      {
         *be = j + 1;
         return;
      }
   }
   *be = e;
   (void)t;
}

/* \(\s*([A-Za-z_0-9]+_t)\s*\*\s*\)\s*data  - the first in [b, e) */
static int cast_in(const char *b, const char *e, const char **tb,
      const char **te)
{
   const char *p;
   for (p = b; p < e; p++)
   {
      const char *q, *w;
      if (*p != '(')
         continue;
      q = ws(p + 1, e);
      for (w = q; q < e && IS_ID(*q); q++);
      if (q - w < 3 || q[-2] != '_' || q[-1] != 't')
         continue;
      *tb = w;
      *te = q;
      q = ws(q, e);
      if (q >= e || *q != '*')
         continue;
      q = ws(q + 1, e);
      if (q >= e || *q != ')')
         continue;
      q = ws(q + 1, e);
      if (e - q >= 4 && !memcmp(q, "data", 4))
         return 1;
   }
   return 0;
}

static int span_eq(const char *a, const char *ae, const char *b,
      const char *be)
{
   return ae - a == be - b && !memcmp(a, b, (size_t)(ae - a));
}

struct span
{
   const char *b;
   const char *e;
};

/* The type the first  static bool <name>(void *data)  casts data to. */
static int cast_type(const char *t, const char *e, const struct span *name,
      struct span *type)
{
   const char *p = t;
   while (p < e)
   {
      const char *nb, *ne, *end;
      if ((p == t || p[-1] == '\n')
            && (end = def_at(p, e, "bool", &nb, &ne))
            && span_eq(nb, ne, name->b, name->e))
      {
         const char *bb, *be;
         body_of(t, e, end, &bb, &be);
         return cast_in(bb, be, &type->b, &type->e);
      }
      p++;
   }
   return 0;
}

/* The type this file's  static void ...swap_buffers(void *data)  casts
 * data to. */
static int swap_cast_type(const char *t, const char *e, struct span *type)
{
   const char *p;
   for (p = t; p < e; p++)
   {
      const char *nb, *ne, *end;
      if (p != t && p[-1] != '\n')
         continue;
      if (!(end = def_at(p, e, "void", &nb, &ne)) || ne - nb < 12
            || memcmp(ne - 12, "swap_buffers", 12))
         continue;
      {
         const char *bb, *be;
         body_of(t, e, end, &bb, &be);
         return cast_in(bb, be, &type->b, &type->e);
      }
   }
   return 0;
}

static int ends_with_hook(const char *b, const char *e)
{
   return e - b >= 11 && !memcmp(e - 11, HOOK, 11);
}

/* \bbool\s+([A-Za-z_0-9]*presentable)\s*\(  - each name, into set; with
 * want, only whether that name is there */
static int bool_defs(const char *t, const char *e, struct ck_set *set,
      const struct span *want)
{
   const char *p;
   for (p = t; p + 4 <= e; p++)
   {
      const char *q, *w;
      if (memcmp(p, "bool", 4) || (p > t && CK_IS_WORD(p[-1])))
         continue;
      q = ws(p + 4, e);
      if (q == p + 4)
         continue;
      for (w = q; q < e && IS_ID(*q); q++);
      if (want)
      {
         if (!span_eq(w, q, want->b, want->e))
            continue;
      }
      else if (!ends_with_hook(w, q))
         continue;
      q = ws(q, e);
      if (q >= e || *q != '(')
         continue;
      if (want)
         return 1;
      *ck_set_add(set, w, (size_t)(q - w)) = set;
      p = q;
   }
   return 0;
}

static struct ck_set shared;
static int shared_read;

static void shared_file(const char *path, const char *rel, void *ud)
{
   const char *slash = strrchr(path, '/');
   size_t dl         = slash ? (size_t)(slash - path) : 0;
   size_t len;
   char *text;
   (void)rel;
   (void)ud;
   if (dl >= strlen(CTX_DIR)
         && !memcmp(path + dl - strlen(CTX_DIR), CTX_DIR, strlen(CTX_DIR)))
      return;
   if (!(ck_ends_with(path, ".c") || ck_ends_with(path, ".m")
            || ck_ends_with(path, ".h")))
      return;
   if (!(text = ck_read_file(path, &len)))
      return;
   blank_noncode(text, len);
   bool_defs(text, text + len, &shared, NULL);
   free(text);
}

/* Whether some file outside gfx/drivers_context defines name() */
static int declared_elsewhere(const struct span *name)
{
   if (!shared_read)
   {
      shared_read = 1;
      ck_walk("./gfx", NULL, shared_file, NULL);
   }
   return ck_set_find(&shared, name->b, (size_t)(name->e - name->b)) != NULL;
}

static void problem(struct ck_strv *out, const char *path, const char *a,
      const struct span *s1, const char *b, const struct span *s2,
      const char *c)
{
   struct ck_sb m = {0};
   ck_sb_puts(&m, path);
   ck_sb_puts(&m, a);
   ck_sb_add(&m, s1->b, (size_t)(s1->e - s1->b));
   ck_sb_puts(&m, b);
   if (s2)
      ck_sb_add(&m, s2->b, (size_t)(s2->e - s2->b));
   ck_sb_puts(&m, c);
   ck_strv_push(out, m.p);
}

/* Problems with one file's presentable() usage. */
static void check_text(const char *path, const char *src, size_t n,
      struct ck_strv *out)
{
   char *t            = ck_strndup(src, n);
   const char *e      = t + n;
   struct span *def   = NULL;
   size_t ndef        = 0, i;
   struct ck_set wired = {0};
   struct ck_strv wnames = {0};
   struct span swap_t;
   int have_swap;
   const char *p;

   blank_noncode(t, n);
   for (p = t; p < e; p++)
   {
      const char *nb, *ne;
      if ((p == t || p[-1] == '\n') && def_at(p, e, "bool", &nb, &ne)
            && ends_with_hook(nb, ne))
      {
         def = (struct span*)ck_realloc(def, (ndef + 1) * sizeof(*def));
         def[ndef].b = nb;
         def[ndef].e = ne;
         ndef++;
      }
   }
   /* names referenced from a gfx_ctx_driver_t initializer:
    * gfx_ctx_driver_t\s+[A-Za-z_0-9]+\s*=?\s*\{ */
   for (p = t; (p = strstr(p, "gfx_ctx_driver_t")); )
   {
      const char *q = p + 16, *w, *i0, *j;
      int depth = 0;
      p = q;
      q = ws(q, e);
      if (q == p)
         continue;
      for (w = q; q < e && IS_ID(*q); q++);
      if (q == w)
         continue;
      q = ws(q, e);
      if (q < e && *q == '=')
         q = ws(q + 1, e);
      if (q >= e || *q != '{')
         continue;
      i0 = q;
      for (j = i0; j < e; j++)
      {
         if (*j == '{')
            depth++;
         else if (*j == '}' && --depth == 0)
            break;
      }
      if (j >= e)
         j = e - 1;
      /* ([A-Za-z_0-9]*presentable): from each byte, to the last use of
       * the name in its word */
      for (q = i0; q < j; )
      {
         const char *r, *last = NULL, *s;
         if (!IS_ID(*q))
         {
            q++;
            continue;
         }
         for (r = q; r < j && IS_ID(*r); r++);
         for (s = q; s + 11 <= r; s++)
         {
            if (!memcmp(s, HOOK, 11))
               last = s;
         }
         if (!last)
         {
            q = r;
            continue;
         }
         *ck_set_add(&wired, q, (size_t)(last + 11 - q)) = &wired;
         q = last + 11;
      }
      p = i0 + 1;
   }

   for (i = 0; i < ndef; i++)
   {
      if (!ck_set_find(&wired, def[i].b, (size_t)(def[i].e - def[i].b)))
         problem(out, path, ": ", &def[i], "() is defined but never wired"
               " into a gfx_ctx_driver_t - the hook would never run", NULL,
               "");
   }
   for (i = 0; i < wired.cap; i++)
   {
      if (wired.keys[i])
         ck_strv_push(&wnames, ck_strdup(wired.keys[i]));
   }
   ck_strv_sort(&wnames);
   for (i = 0; i < wnames.n; i++)
   {
      struct span w;
      size_t k;
      int is_def = 0;
      w.b = wnames.v[i];
      w.e = w.b + strlen(w.b);
      for (k = 0; k < ndef; k++)
         is_def |= span_eq(def[k].b, def[k].e, w.b, w.e);
      if (is_def)
         continue;
      if (!bool_defs(t, e, NULL, &w) && !declared_elsewhere(&w))
         problem(out, path, ": vtable names ", &w, "(), which is defined"
               " nowhere in the tree", NULL, "");
   }
   have_swap = swap_cast_type(t, e, &swap_t);
   for (i = 0; i < ndef && have_swap; i++)
   {
      struct span ct;
      if (!cast_type(t, e, &def[i], &ct))
         continue;
      if (!span_eq(ct.b, ct.e, swap_t.b, swap_t.e))
      {
         struct ck_sb m = {0};
         ck_sb_puts(&m, path);
         ck_sb_puts(&m, ": ");
         ck_sb_add(&m, def[i].b, (size_t)(def[i].e - def[i].b));
         ck_sb_puts(&m, "() casts data to ");
         ck_sb_add(&m, ct.b, (size_t)(ct.e - ct.b));
         ck_sb_puts(&m, " but swap_buffers() casts it to ");
         ck_sb_add(&m, swap_t.b, (size_t)(swap_t.e - swap_t.b));
         ck_sb_puts(&m, " - one of them has the wrong driver's type");
         ck_strv_push(out, m.p);
      }
   }
   for (i = 0; i < wired.cap; i++)
      free(wired.keys[i]);
   free(wired.keys);
   free(wired.vals);
   ck_strv_free(&wnames);
   free(def);
   free(t);
}

static void ctx_files(const char *root, struct ck_strv *names)
{
   struct ck_sb d = {0};
   ck_sb_puts(&d, root);
   ck_sb_puts(&d, "/" CTX_DIR);
   if (!ck_list_dir(d.p, names))
   {
      fprintf(stderr, "ctx_presentable_check: no %s under %s\n", CTX_DIR,
            root);
      exit(1);
   }
   ck_strv_sort(names);
   ck_sb_free(&d);
}

static char *read_ctx(const char *root, const char *fn, size_t *len)
{
   struct ck_sb p = {0};
   char *text;
   ck_sb_puts(&p, root);
   ck_sb_puts(&p, "/" CTX_DIR "/");
   ck_sb_puts(&p, fn);
   text = ck_is_file(p.p) ? ck_read_file(p.p, len) : NULL;
   ck_sb_free(&p);
   return text;
}

static void check_tree(const char *root, struct ck_strv *problems)
{
   struct ck_strv names = {0};
   size_t i;
   ctx_files(root, &names);
   for (i = 0; i < names.n; i++)
   {
      struct ck_sb rel = {0};
      size_t len;
      char *text;
      if (!(ck_ends_with(names.v[i], ".c") || ck_ends_with(names.v[i], ".m")))
         continue;
      if (!(text = read_ctx(root, names.v[i], &len)))
         continue;
      if (strstr(text, HOOK))
      {
         ck_sb_puts(&rel, CTX_DIR "/");
         ck_sb_puts(&rel, names.v[i]);
         check_text(rel.p, text, len, problems);
         ck_sb_free(&rel);
      }
      free(text);
   }
   ck_strv_free(&names);
}

/* self-test */

static const char good[] =
   "\n"
   "typedef struct { int vk; } my_ctx_data_t;\n"
   "static bool my_presentable(void *data)\n"
   "{\n"
   "   my_ctx_data_t *c = (my_ctx_data_t*)data;\n"
   "   return c && c->vk;\n"
   "}\n"
   "static void my_swap_buffers(void *data)\n"
   "{\n"
   "   my_ctx_data_t *c = (my_ctx_data_t*)data;\n"
   "   (void)c;\n"
   "}\n"
   "const gfx_ctx_driver_t gfx_ctx_my = {\n"
   "   my_swap_buffers,\n"
   "   my_presentable\n"
   "};\n";

/* The slot left NULL, named in a comment the way the tree names every
 * other unimplemented slot. Nothing is defined and nothing is wired. */
static const char null_slot[] =
   "\n"
   "typedef struct { int vk; } my_ctx_data_t;\n"
   "static void my_swap_buffers(void *data)\n"
   "{\n"
   "   my_ctx_data_t *c = (my_ctx_data_t*)data;\n"
   "   (void)c;\n"
   "}\n"
   "const gfx_ctx_driver_t gfx_ctx_my = {\n"
   "   my_swap_buffers,\n"
   "   NULL  /* presentable - this driver has no way to ask */\n"
   "};\n";

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
   const char *labels[6];
   char *texts[6];
   int want[6];
   int i, bad = 0;

   labels[0] = "a correct driver";
   texts[0]  = ck_strdup(good);
   want[0]   = 0;
   labels[1] = "the wrong driver's context type";
   texts[1]  = replace(good, "my_ctx_data_t *c = (my_ctx_data_t*)data;\n"
         "   return", "other_ctx_data_t *c = (other_ctx_data_t*)data;\n"
         "   return");
   want[1]   = 1;
   labels[2] = "defined but not wired";
   texts[2]  = replace(good, "   my_swap_buffers,\n   my_presentable\n",
         "   my_swap_buffers\n");
   want[2]   = 1;
   labels[3] = "a NULL slot named in a comment";
   texts[3]  = ck_strdup(null_slot);
   want[3]   = 0;
   /* A wired hook with a comment beside it that names the one this
    * driver is not using. The slot is right; only the comment mentions
    * the name. */
   labels[4] = "a comment beside a wired hook";
   texts[4]  = replace(good, "   my_presentable\n",
         "   /* not shared_presentable(): this one asks its own window */\n"
         "   my_presentable\n");
   want[4]   = 0;
   /* The ident string is part of the initializer, so a driver named
    * after the hook must not read as a reference to it. */
   labels[5] = "the hook's name inside the ident string";
   texts[5]  = replace(good, "   my_presentable\n",
         "   \"presentable_gl\",\n   my_presentable\n");
   want[5]   = 0;
   for (i = 0; i < 6; i++)
   {
      struct ck_strv out = {0};
      int got;
      check_text("fixture.c", texts[i], strlen(texts[i]), &out);
      got = (int)out.n;
      if (got != want[i])
         bad++;
      printf("   %-38s %d problem(s), want %d  %s\n", labels[i], got, want[i],
            got == want[i] ? "ok" : "FAIL");
      ck_strv_free(&out);
      free(texts[i]);
   }
   if (bad)
   {
      printf("ctx presentable checker: SELFTEST FAILED\n");
      return 1;
   }
   printf("ctx presentable checker: selftest passed\n");
   return 0;
}

int main(int argc, char **argv)
{
   struct ck_strv problems = {0};
   struct ck_strv names    = {0};
   const char *root        = ".";
   unsigned long n         = 0;
   size_t i;
   int a, self = 0;

   for (a = 1; a < argc; a++)
   {
      if (!strcmp(argv[a], "--selftest"))
         self = 1;
      else if (!strcmp(argv[a], "--root") && a + 1 < argc)
         root = argv[++a];
      else
      {
         fprintf(stderr, "usage: ctx_presentable_check [--selftest]"
               " [--root DIR]\n");
         return 2;
      }
   }
   if (self)
      return selftest();
   check_tree(root, &problems);
   for (i = 0; i < problems.n; i++)
      printf("%s\n", problems.v[i]);
   if (problems.n)
   {
      printf("%lu problem(s)\n", (unsigned long)problems.n);
      return 1;
   }
   ctx_files(root, &names);
   for (i = 0; i < names.n; i++)
   {
      size_t len;
      char *text;
      if (!(ck_ends_with(names.v[i], ".c") || ck_ends_with(names.v[i], ".m")))
         continue;
      if (!(text = read_ctx(root, names.v[i], &len)))
         continue;
      blank_noncode(text, len);
      if (strstr(text, HOOK))
         n++;
      free(text);
   }
   printf("ctx presentable: %lu context drivers implement the hook, all "
         "wired once and casting the type their own swap_buffers() does\n", n);
   return 0;
}
