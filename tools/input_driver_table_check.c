/* An input driver's table has no more entries than the structure has
 * members.
 *
 * Every input driver fills in an input_driver_t by position:
 *
 *     input_driver_t input_uwp = {
 *        uwp_input_init,
 *        ...
 *        uwp_keys_down,
 *        uwp_bind_mouse_buttons
 *     };
 *
 * When a member is added to struct input_driver (input/input_driver.h)
 * and a driver names it, the driver's table and the structure have to
 * land together. They did not, once: the UWP, udev and web drivers named
 * bind_mouse_buttons a commit before the structure had it. gcc and clang
 * say "excess elements in struct initializer" and carry on; MSVC says
 * C2078, too many initializers, and the UWP build stops - on a runner
 * most contributors do not have, for a file no Linux job compiles.
 *
 * This is that check at source level, for every driver whatever SDK it
 * needs: the entries of each `input_driver_t NAME = { ... };` are
 * counted, down each side of any #if / #else in the table, and none may
 * come to more than the structure's members. Fewer is fine - the members
 * a table leaves out are NULL.
 *
 *     tools/check.sh input_driver_table_check [--root DIR] [--selftest] */

#include "check_common.h"

#define HEADER "input/input_driver.h"
#define DIR_   "input/drivers"

static int is_strip_space(int c)
{
   return CK_IS_SPACE(c) || (c >= 0x1c && c <= 0x1f);
}

/* Block comments made spaces with their line breaks kept, then line
 * comments taken out. */
static size_t strip_comments(char *t, size_t n)
{
   size_t i, o;
   for (i = 0; i + 1 < n; i++)
   {
      size_t j;
      if (t[i] != '/' || t[i + 1] != '*')
         continue;
      for (j = i + 2; j + 1 < n && !(t[j] == '*' && t[j + 1] == '/'); j++);
      if (j + 1 >= n)
         break;
      for (; i < j + 2; i++)
      {
         if (t[i] != '\n')
            t[i] = ' ';
      }
      i--;
   }
   for (i = o = 0; i < n; )
   {
      if (t[i] == '/' && i + 1 < n && t[i + 1] == '/')
      {
         while (i < n && t[i] != '\n')
            i++;
      }
      else
         t[o++] = t[i++];
   }
   t[o] = '\0';
   return o;
}

static char *read_stripped(const char *path, size_t *len)
{
   char *t = ck_read_file(path, len);
   if (t)
      *len = strip_comments(t, *len);
   return t;
}

/* The members of struct input_driver, or 0. */
static long members(const char *root)
{
   struct ck_sb path = {0};
   const char *p, *e, *b;
   size_t len;
   long count = 0;
   int depth;
   char *t;

   ck_sb_puts(&path, root);
   ck_sb_puts(&path, "/" HEADER);
   t = read_stripped(path.p, &len);
   ck_sb_free(&path);
   if (!t)
   {
      fprintf(stderr, "input_driver_table_check: cannot read %s\n", HEADER);
      exit(1);
   }
   e = t + len;
   /* ^struct input_driver\s*\{  (re.M) */
   for (p = t, b = NULL; p < e && !b; p++)
   {
      const char *q;
      if ((p != t && p[-1] != '\n') || (size_t)(e - p) < 19
            || memcmp(p, "struct input_driver", 19))
         continue;
      for (q = p + 19; q < e && CK_IS_SPACE(*q); q++);
      if (q < e && *q == '{')
         b = q + 1;
   }
   if (!b)
   {
      free(t);
      return 0;
   }
   depth = 1;
   for (p = b; depth && p < e; p++)
      depth += *p == '{' ? 1 : *p == '}' ? -1 : 0;
   /* a member is a declaration ending in ';' at the structure's own
    * depth */
   e     = p - 1;
   depth = 0;
   for (p = b; p < e; p++)
   {
      if (*p == '(' || *p == '{')
         depth++;
      else if (*p == ')' || *p == '}')
         depth--;
      else if (*p == ';' && depth == 0)
         count++;
   }
   free(t);
   return count;
}

struct lines
{
   const char **b;
   const char **e;
   size_t n;
};

static int starts(const struct lines *l, size_t i, const char *s)
{
   const char *b = l->b[i], *e = l->e[i];
   size_t n      = strlen(s);
   while (b < e && is_strip_space((unsigned char)*b))
      b++;
   return (size_t)(e - b) >= n && !memcmp(b, s, n);
}

/* Entries of plain text: the pieces between commas that are not blank */
static long entries(const struct ck_sb *text)
{
   const char *p = text->p, *e = text->p + text->len;
   long n        = 0;
   while (p <= e)
   {
      const char *c = (const char*)memchr(p, ',', (size_t)(e - p));
      const char *s = c ? c : e;
      const char *q;
      for (q = p; q < s && is_strip_space((unsigned char)*q); q++);
      n += q < s;
      if (!c)
         break;
      p = c + 1;
   }
   return n;
}

static void die_short(void)
{
   fprintf(stderr, "input_driver_table_check: an #if in a table does not"
         " end\n");
   exit(1);
}

/* (fewest, most) entries over every way through the #if branches */
static size_t block(const struct lines *l, size_t i, long *lo_out,
      long *hi_out, int depth)
{
   struct ck_sb text = {0};
   long lo = 0, hi = 0;

   if (depth > 1000)
      die_short();
   ck_sb_puts(&text, "");
   while (i < l->n)
   {
      if (starts(l, i, "#if"))
      {
         long blo, bhi, mn, mx;
         int saw_else = 0;
         i  = block(l, i + 1, &blo, &bhi, depth + 1);
         mn = blo;
         mx = bhi;
         for (;;)
         {
            if (i >= l->n)
               die_short();
            if (!starts(l, i, "#elif") && !starts(l, i, "#else"))
               break;
            saw_else |= starts(l, i, "#else");
            i = block(l, i + 1, &blo, &bhi, depth + 1);
            if (blo < mn)
               mn = blo;
            if (bhi > mx)
               mx = bhi;
         }
         if (!saw_else)
         {
            if (mn > 0)
               mn = 0;
            if (mx < 0)
               mx = 0;
         }
         lo += mn;
         hi += mx;
         i++;
         continue;
      }
      if (starts(l, i, "#elif") || starts(l, i, "#else")
            || starts(l, i, "#endif"))
         break;
      if (!starts(l, i, "#"))
      {
         ck_sb_putc(&text, ' ');
         ck_sb_add(&text, l->b[i], (size_t)(l->e[i] - l->b[i]));
      }
      i++;
      /* entries seen so far in plain text are counted when it ends */
      if (i >= l->n || starts(l, i, "#"))
      {
         long n = entries(&text);
         lo        += n;
         hi        += n;
         text.len   = 0;
         text.p[0]  = '\0';
      }
   }
   ck_sb_free(&text);
   *lo_out = lo;
   *hi_out = hi;
   return i;
}

struct table
{
   char *rel;
   char *name;
   unsigned long line;
   long lo;
   long hi;
};

struct tables
{
   struct table *v;
   size_t n;
};

static void find_tables(const char *root, struct tables *out)
{
   struct ck_strv names = {0};
   struct ck_sb base    = {0};
   size_t f;

   out->v = NULL;
   out->n = 0;
   ck_sb_puts(&base, root);
   ck_sb_puts(&base, "/" DIR_);
   if (!ck_is_dir(base.p))
   {
      ck_sb_free(&base);
      return;
   }
   ck_list_dir(base.p, &names);
   ck_strv_sort(&names);
   for (f = 0; f < names.n; f++)
   {
      struct ck_sb path = {0};
      const char *t, *e, *p;
      unsigned long line = 1;
      const char *counted;
      size_t len;
      char *text;

      if (!(ck_ends_with(names.v[f], ".c") || ck_ends_with(names.v[f], ".m")
               || ck_ends_with(names.v[f], ".mm")
               || ck_ends_with(names.v[f], ".cpp")))
         continue;
      ck_sb_puts(&path, base.p);
      ck_sb_putc(&path, '/');
      ck_sb_puts(&path, names.v[f]);
      text = ck_is_file(path.p) ? read_stripped(path.p, &len) : NULL;
      ck_sb_free(&path);
      if (!text)
         continue;
      t       = text;
      e       = t + len;
      counted = t;
      /* ^input_driver_t\s+(\w+)\s*=\s*\{  (re.M) */
      for (p = t; p < e; )
      {
         const char *q, *w, *we, *end;
         if ((p != t && p[-1] != '\n') || (size_t)(e - p) < 14
               || memcmp(p, "input_driver_t", 14))
         {
            p++;
            continue;
         }
         for (q = p + 14; q < e && CK_IS_SPACE(*q); q++);
         for (w = q; q < e && CK_IS_WORD(*q); q++);
         we = q;
         if (w == p + 14 || we == w)
         {
            p++;
            continue;
         }
         for (; q < e && CK_IS_SPACE(*q); q++);
         if (q >= e || *q != '=')
         {
            p++;
            continue;
         }
         for (q++; q < e && CK_IS_SPACE(*q); q++);
         if (q >= e || *q != '{')
         {
            p++;
            continue;
         }
         q++;
         for (; counted < p; counted++)
            line += *counted == '\n';
         end = strstr(q, "};");
         if (end)
         {
            struct lines l;
            const char *s = q;
            long lo, hi;
            l.b = NULL;
            l.e = NULL;
            l.n = 0;
            for (;;)
            {
               const char *nl = (const char*)memchr(s, '\n',
                     (size_t)(end - s));
               l.b = (const char**)ck_realloc(l.b, (l.n + 1) * sizeof(char*));
               l.e = (const char**)ck_realloc(l.e, (l.n + 1) * sizeof(char*));
               l.b[l.n] = s;
               l.e[l.n] = nl ? nl : end;
               l.n++;
               if (!nl)
                  break;
               s = nl + 1;
            }
            block(&l, 0, &lo, &hi, 0);
            free(l.b);
            free(l.e);
            out->v = (struct table*)ck_realloc(out->v,
                  (out->n + 1) * sizeof(*out->v));
            {
               struct ck_sb rel = {0};
               ck_sb_puts(&rel, DIR_ "/");
               ck_sb_puts(&rel, names.v[f]);
               out->v[out->n].rel = rel.p;
            }
            out->v[out->n].name = ck_strndup(w, (size_t)(we - w));
            out->v[out->n].line = line;
            out->v[out->n].lo   = lo;
            out->v[out->n].hi   = hi;
            out->n++;
         }
         p = q;
      }
      free(text);
   }
   ck_strv_free(&names);
   ck_sb_free(&base);
}

static int run(const char *root, int quiet)
{
   struct tables found;
   long n = members(root);
   long most = 0;
   size_t i;
   int bad = 0;

   if (!n)
   {
      if (!quiet)
         printf("error: struct input_driver was not found in %s; nothing"
               " was checked\n", HEADER);
      return 2;
   }
   find_tables(root, &found);
   if (!found.n)
   {
      if (!quiet)
         printf("error: no input_driver_t table was found; nothing was"
               " checked\n");
      return 2;
   }
   for (i = 0; i < found.n; i++)
   {
      const struct table *t = &found.v[i];
      if (t->hi > most || !i)
         most = t->hi;
      if (t->hi > n)
      {
         if (!quiet)
            printf("%s:%lu: %s has %ld entries, and struct input_driver has"
                  " %ld members.\n"
                  "  MSVC stops at this (C2078, too many initializers). The"
                  " member a new entry\n"
                  "  is for has to be in %s in the same commit.\n",
                  t->rel, t->line, t->name, t->hi, n, HEADER);
         bad++;
      }
   }
   for (i = 0; i < found.n; i++)
   {
      free(found.v[i].rel);
      free(found.v[i].name);
   }
   free(found.v);
   if (bad)
   {
      if (!quiet)
         printf("FAIL input_driver_table_check: %d table(s)\n", bad);
      return 1;
   }
   if (!quiet)
      printf("PASS input_driver_table_check (%lu tables, none past the"
            " structure's %ld members; the fullest has %ld)\n",
            (unsigned long)i, n, most);
   return 0;
}

static int selftest(void)
{
   static const char *const bodies[] = {
      "   a, b, c, \"t\"",
      "   a, b",
      "   a, b, c, \"t\", e",
      "   a,\n#ifdef X\n   b,\n#else\n   NULL, /* b, c */\n#endif\n   c, \"t\"",
      "   a, b, c,\n#ifdef X\n   \"t\", e\n#else\n   \"t\"\n#endif",
      "   a, b, c, \"t\"\n#ifdef X\n   , e\n#endif"
   };
   static const int want[] = { 0, 0, 1, 0, 1, 1 };
   static const char *const what[] = {
      "as many entries as members",
      "fewer entries than members",
      "one entry too many",
      "an #if with as many either way",
      "too many down one side of an #if",
      "too many when an #if with no #else is taken"
   };
   struct ck_sb p = {0};
   char *root     = ck_tmpdir();
   size_t i;
   int bad = 0;
   long m;

   ck_sb_puts(&p, root);
   ck_sb_puts(&p, "/" HEADER);
   ck_mkdirs_for(p.p);
   ck_write_file(p.p, "struct input_driver\n{\n   void *(*init)(const char"
         " *j);\n   /* one; two */\n   void (*poll)(void *data);\n"
         "   int16_t (*state)(void *data,\n         unsigned port);\n"
         "   const char *ident;\n};\n");
   p.len = 0;
   ck_sb_puts(&p, root);
   ck_sb_puts(&p, "/" DIR_ "/t_input.c");
   ck_mkdirs_for(p.p);
   for (i = 0; i < sizeof(want) / sizeof(want[0]); i++)
   {
      struct ck_sb body = {0};
      int got;
      ck_sb_puts(&body, "input_driver_t input_t = {\n");
      ck_sb_puts(&body, bodies[i]);
      ck_sb_puts(&body, "\n};\n");
      ck_write_file(p.p, body.p);
      ck_sb_free(&body);
      got = run(root, 1);
      if (got != want[i])
      {
         printf("selftest: %s: wanted %d, got %d\n", what[i], want[i], got);
         bad++;
      }
   }
   if ((m = members(root)) != 4)
   {
      printf("selftest: the structure's members were counted as %ld, not 4\n",
            m);
      bad++;
   }
   ck_rmtree(root);
   free(root);
   ck_sb_free(&p);
   if (bad)
   {
      printf("FAIL input_driver_table_check --selftest\n");
      return 1;
   }
   printf("PASS input_driver_table_check --selftest (%lu cases)\n",
         (unsigned long)(sizeof(want) / sizeof(want[0]) + 1));
   return 0;
}

int main(int argc, char **argv)
{
   const char *root = getenv("RA_TREE_ROOT");
   int self         = 0;
   int i;

   if (!root || !*root)
      root = ".";
   for (i = 1; i < argc; i++)
   {
      if (!strcmp(argv[i], "--selftest"))
         self = 1;
      else if (!strcmp(argv[i], "--root") && i + 1 < argc)
         root = argv[++i];
      else if (!strncmp(argv[i], "--root=", 7))
         root = argv[i] + 7;
      else
      {
         fprintf(stderr, "usage: input_driver_table_check [--root DIR]"
               " [--selftest]\n");
         return 2;
      }
   }
   return self ? selftest() : run(root, 0);
}
