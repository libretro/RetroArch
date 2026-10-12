/* Find readers left behind by a struct member that a change removed.
 *
 * Why this exists: when a member goes away, the compiler finds every
 * reader it compiles. The readers it does not compile are the ones a
 * Linux box never sees - the Objective-C drivers above all, plus
 * anything behind a platform #ifdef - and each of those costs a CI
 * round trip to discover, one lane at a time.
 *
 * window_position_x and window_position_y went that way. Packing them
 * into one word updated the Win32 and SDL3 users, because those are
 * what a grep of the .c and .h files turns up; ui_cocoa.m kept reading
 * both and broke the macOS job and both PowerPC cross jobs at once.
 *
 * tools/packed_field_check.py does not cover this: it knows the six
 * axis names a size or origin pair is spelled with, and
 * window_position_x is not one of them. The question here is not what
 * a member is called, it is whether anything still reaches for one the
 * change took away.
 *
 * So this reads the change rather than the tree's vocabulary. It takes
 * the struct members removed by a commit range, drops the ones still
 * declared somewhere (a member that merely moved, or a name another
 * struct also uses), and reports every remaining reader it can find, in
 * every language the tree is written in.
 *
 * Usage:
 *   tools/check.sh removed_member_check [--range A..B] [paths ...]
 *   tools/check.sh removed_member_check --selftest
 *
 * --range defaults to origin/master..HEAD, which is the set a patch is
 * about to deliver. Run from the top of the tree: the range is read
 * with git there and the paths default to ".". Exits non-zero when a
 * reader is left behind. */

#include "check_common.h"

static const char *const suffixes[] = {
   ".c", ".h", ".m", ".mm", ".cpp", ".hpp", ".inc", NULL
};

/* Vendored trees: their members are not ours to remove, and their
 * readers are not ours to fix. */
static const char *const prune[] = {
   ".git", "deps", "media", "wii/libogc", "pkg/apple/WebServer", NULL
};

/* Names too common to reason about from a diff alone, and never the
 * whole story when one goes: a struct keeping its own `data` or `size`
 * says nothing about anyone else's. */
static const char *const noise[] = {
   "data", "size", "len", "ptr", "buf", "flags", "type", "count",
   "idx", "index", "next", "prev", "id", "name", "val", "value", NULL
};

#define IS_BL(c) ((c) == ' ' || (c) == '\t')

static const char *blanks(const char *p, const char *e)
{
   while (p < e && IS_BL(*p))
      p++;
   return p;
}

static const char *word_end(const char *p, const char *e)
{
   while (p < e && CK_IS_WORD(*p))
      p++;
   return p;
}

static const char *lit(const char *p, const char *e, const char *s)
{
   size_t n = strlen(s);
   return (size_t)(e - p) >= n && !memcmp(p, s, n) ? p + n : NULL;
}

/* (?:WORDS)[ \t]+  repeated: where each repetition ends, up to max */
static size_t quals(const char *p, const char *e,
      const char *const *words, const char **ends, size_t max)
{
   size_t n = 0;
   while (n < max)
   {
      const char *q = NULL;
      size_t k;
      for (k = 0; words[k] && !q; k++)
      {
         const char *r = lit(p, e, words[k]);
         if (r && r < e && IS_BL(*r))
            q = blanks(r, e);
      }
      if (!q)
         break;
      ends[n++] = p = q;
   }
   return n;
}

/* Where the type alternatives of DECL can end, starting at p, in the
 * order a backtracking match tries them. */
static size_t type_ends(const char *p, const char *e, const char **out)
{
   static const char *const tagged[] = { "struct", "union", "enum" };
   static const char *const sign[]   = { "unsigned", "signed" };
   static const char *const ints[]   = { "char", "short", "int", "long" };
   static const char *const plain[]  = {
      "void", "char", "int", "float", "double", "bool", "size_t", "ssize_t"
   };
   static const char *const widths[] = {
      "8", "16", "32", "64", "ptr", "max"
   };
   size_t n = 0, k;
   const char *q, *r;

   /* (?:struct|union|enum)[ \t]+\w+ */
   for (k = 0; k < 3; k++)
   {
      if ((q = lit(p, e, tagged[k])) && q < e && IS_BL(*q))
      {
         q = blanks(q, e);
         if (q < e && CK_IS_WORD(*q))
            out[n++] = word_end(q, e);
      }
   }
   /* (?:unsigned|signed)(?:[ \t]+(?:char|short|int|long))* */
   for (k = 0; k < 2; k++)
   {
      const char *chain[64];
      size_t c = 0;
      if (!(q = lit(p, e, sign[k])))
         continue;
      chain[c++] = q;
      for (;;)
      {
         size_t j;
         const char *s = blanks(q, e);
         if (s == q || c >= 64)
            break;
         for (j = 0, r = NULL; j < 4 && !r; j++)
            r = lit(s, e, ints[j]);
         if (!r)
            break;
         chain[c++] = q = r;
      }
      while (c)
         out[n++] = chain[--c];
   }
   /* long(?:[ \t]+long)?(?:[ \t]+int)? */
   if ((q = lit(p, e, "long")))
   {
      const char *l2 = NULL, *s;
      s = blanks(q, e);
      if (s > q)
         l2 = lit(s, e, "long");
      if (l2)
      {
         s = blanks(l2, e);
         if (s > l2 && (r = lit(s, e, "int")))
            out[n++] = r;
         out[n++] = l2;
      }
      s = blanks(q, e);
      if (s > q && (r = lit(s, e, "int")))
         out[n++] = r;
      out[n++] = q;
   }
   /* short(?:[ \t]+int)? */
   if ((q = lit(p, e, "short")))
   {
      const char *s = blanks(q, e);
      if (s > q && (r = lit(s, e, "int")))
         out[n++] = r;
      out[n++] = q;
   }
   for (k = 0; k < 8; k++)
      if ((q = lit(p, e, plain[k])))
         out[n++] = q;
   /* u?int(?:8|16|32|64|ptr|max)_t - without the u only where there
    * is none, as "int" cannot then start at p */
   r = p < e && *p == 'u' ? p + 1 : p;
   if ((q = lit(r, e, "int")))
   {
      for (k = 0; k < 6; k++)
      {
         const char *w = lit(q, e, widths[k]);
         if (w && (w = lit(w, e, "_t")))
            out[n++] = w;
      }
   }
   /* \w+ */
   if (p < e && CK_IS_WORD(*p))
      out[n++] = word_end(p, e);
   return n;
}

/* The rest of DECL from the end of the type: the separator, the name,
 * an array bound, the semicolon and a trailing comment. */
static int decl_tail(const char *p, const char *e, const char **nb,
      const char **ne)
{
   const char *q = blanks(p, e), *s;
   int sep       = q > p;
   s = q;
   while (q < e && *q == '*')
      q++;
   if (q > s)
      sep = 1;
   if (!sep)
      return 0;
   q = blanks(q, e);
   if (q >= e || !CK_IS_IDENT0(*q))
      return 0;
   *nb = q;
   *ne = q = word_end(q, e);
   q = blanks(q, e);
   if (q < e && *q == '[')
   {
      for (s = q + 1; s < e && *s != ']' && *s != ';'; s++);
      if (s < e && *s == ']')
         q = blanks(s + 1, e);
   }
   if (q >= e || *q != ';')
      return 0;
   q = blanks(q + 1, e);
   return q == e || lit(q, e, "/*") || lit(q, e, "//");
}

/* DECL.match(line): the member name a line declares */
static int decl_match(const char *b, const char *e, const char **nb,
      const char **ne)
{
   static const char *const qw[] = { "const", "volatile", "static", NULL };
   const char *starts[257];
   size_t nq, i;
   const char *p = blanks(b, e);

   starts[0] = p;
   nq        = quals(p, e, qw, starts + 1, 256);
   for (i = nq + 1; i-- > 0; )
   {
      const char *ends[64];
      size_t nt = type_ends(starts[i], e, ends), t;
      for (t = 0; t < nt; t++)
         if (decl_tail(ends[t], e, nb, ne))
            return 1;
   }
   return 0;
}

static int is_ident(const char *b, const char *e)
{
   const char *p;
   if (b >= e || !CK_IS_IDENT0(*b))
      return 0;
   for (p = b; p < e; p++)
      if (!CK_IS_WORD(*p))
         return 0;
   return 1;
}

/* every name a line declares, read as liberally as is safe */
static void declared_names(const char *b, const char *e,
      struct ck_set *out)
{
   static const char *const qw[] = {
      "const", "volatile", "static", "extern", "struct", "union", "enum",
      "unsigned", "signed", NULL
   };
   const char *starts[257];
   const char *nb, *ne;
   size_t nq, i;
   const char *p;

   if (decl_match(b, e, &nb, &ne))
   {
      ck_set_add(out, nb, (size_t)(ne - nb));
      return;
   }
   /* DECL_LIST: quals* \w+[ \t]+(?P<names>[^;()=]+); */
   p         = blanks(b, e);
   starts[0] = p;
   nq        = quals(p, e, qw, starts + 1, 256);
   for (i = nq + 1; i-- > 0; )
   {
      const char *q = starts[i], *w, *s, *xb;
      struct ck_sb part = {0};
      if (q >= e || !CK_IS_WORD(*q))
         continue;
      w = word_end(q, e);
      s = blanks(w, e);
      if (s == w)
         continue;
      for (xb = s; s < e && !strchr(";()=", *s); s++);
      if (s >= e || *s != ';')
         continue;
      if (s == xb && xb - w < 2)
         continue;
      /* the names, split on ',', bounds dropped, ' \t*' stripped */
      for (p = xb; p <= s; p++)
      {
         if (p == s || *p == ',')
         {
            const char *pb = part.p, *pe = part.p + part.len;
            while (pb < pe && (IS_BL(*pb) || *pb == '*'))
               pb++;
            while (pe > pb && (IS_BL(pe[-1]) || pe[-1] == '*'))
               pe--;
            if (part.p && is_ident(pb, pe))
               ck_set_add(out, pb, (size_t)(pe - pb));
            part.len = 0;
            continue;
         }
         if (*p == '[')
         {
            const char *c;
            for (c = p + 1; c < s && *c != ']' && *c != ','; c++);
            if (c < s && *c == ']')
            {
               p = c;
               continue;
            }
         }
         ck_sb_putc(&part, *p);
      }
      ck_sb_free(&part);
      return;
   }
}

static int in_list(const char *const *list, const char *b, size_t n)
{
   size_t k;
   for (k = 0; list[k]; k++)
      if (strlen(list[k]) == n && !memcmp(list[k], b, n))
         return 1;
   return 0;
}

/* Members a diff takes away and does not put back under the same name,
 * sorted. A rename shows up as one of each, so the added side is
 * subtracted; a member that merely moves file is caught by the
 * declared-somewhere pass instead. */
static void removed_members(const char *diff, struct ck_strv *out)
{
   struct ck_set gone = {0}, kept = {0};
   const char *p = diff;
   size_t i;

   for (;;)
   {
      const char *nl = strchr(p, '\n');
      const char *e  = nl ? nl : p + strlen(p);
      const char *nb, *ne;
      if (!(lit(p, e, "---") || lit(p, e, "+++"))
            && p < e && (*p == '-' || *p == '+')
            && decl_match(p + 1, e, &nb, &ne))
         ck_set_add(*p == '-' ? &gone : &kept, nb, (size_t)(ne - nb));
      if (!nl)
         break;
      p = nl + 1;
   }
   for (i = 0; i < gone.cap; i++)
   {
      const char *k = gone.keys[i];
      if (k && !ck_set_find(&kept, k, strlen(k))
            && !in_list(noise, k, strlen(k)))
         ck_strv_push(out, ck_strdup(k));
   }
   ck_strv_sort(out);
   ck_set_free(&gone);
   ck_set_free(&kept);
}

/* Blank out comments and string literals so a member named in prose or
 * in a log line is not mistaken for a reader. In place. */
static void strip(char *t, size_t n)
{
   size_t i = 0, j;
   while (i < n)
   {
      char c = t[i];
      if (c == '/' && i + 1 < n && t[i + 1] == '*')
      {
         for (j = i + 2; j + 1 < n && !(t[j] == '*' && t[j + 1] == '/'); j++);
         j = j + 1 < n ? j + 2 : n;
      }
      else if (c == '/' && i + 1 < n && t[i + 1] == '/')
      {
         for (j = i; j < n && t[j] != '\n'; j++);
      }
      else if (c == '"' || c == '\'')
      {
         for (j = i + 1; j < n && t[j] != c; )
            j += t[j] == '\\' ? 2 : 1;
         j = j + 1 < n ? j + 1 : n;
      }
      else
      {
         i++;
         continue;
      }
      for (; i < j; i++)
         if (t[i] != '\n')
            t[i] = ' ';
   }
}

struct hit
{
   const char *path;
   unsigned long line;
   char *name;
};

struct hits
{
   struct hit *v;
   size_t n;
};

/* Every read of one of names through . or ->, in files where the name
 * is not declared as a member any more. */
static void scan(const struct ck_strv *names, const struct ck_strv *paths,
      char **texts, size_t *lens, struct hits *out)
{
   struct ck_set declared = {0}, orphans = {0};
   size_t i, f, no = 0;

   for (f = 0; f < paths->n; f++)
   {
      const char *p = texts[f], *end = texts[f] + lens[f];
      for (;;)
      {
         const char *nl = (const char*)memchr(p, '\n', (size_t)(end - p));
         const char *e  = nl ? nl : end;
         declared_names(p, e, &declared);
         if (!nl)
            break;
         p = nl + 1;
      }
   }
   for (i = 0; i < names->n; i++)
      if (!ck_set_find(&declared, names->v[i], strlen(names->v[i])))
      {
         ck_set_add(&orphans, names->v[i], strlen(names->v[i]));
         no++;
      }
   ck_set_free(&declared);
   if (!no)
      return;
   for (f = 0; f < paths->n; f++)
   {
      char *t = ck_strndup(texts[f], lens[f]);
      size_t n = lens[f];
      unsigned long line = 1;
      size_t p, counted = 0;
      strip(t, n);
      for (p = 0; p < n; )
      {
         size_t q = 0;
         if (t[p] == '.')
            q = p + 1;
         else if (t[p] == '-' && p + 1 < n && t[p + 1] == '>')
            q = p + 2;
         if (q)
         {
            size_t w;
            while (q < n && CK_IS_SPACE(t[q]))
               q++;
            for (w = q; w < n && CK_IS_WORD(t[w]); w++);
            if (w > q && ck_set_find(&orphans, t + q, w - q))
            {
               for (; counted < p; counted++)
                  if (t[counted] == '\n')
                     line++;
               out->v = (struct hit*)ck_realloc(out->v,
                     (out->n + 1) * sizeof(*out->v));
               out->v[out->n].path = paths->v[f];
               out->v[out->n].line = line;
               out->v[out->n].name = ck_strndup(t + q, w - q);
               out->n++;
               p = w;
               continue;
            }
         }
         p++;
      }
      free(t);
   }
   ck_set_free(&orphans);
}

static void hits_free(struct hits *h)
{
   size_t i;
   for (i = 0; i < h->n; i++)
      free(h->v[i].name);
   free(h->v);
   h->v = NULL;
   h->n = 0;
}

/* ---- self-test ----------------------------------------------------- */

struct fixture
{
   const char *what;
   const char *diff;
   const char *files[4]; /* name, text, name, text */
   unsigned want;
};

static const struct fixture fixtures[] = {
   { "a member removed with its readers is clean",
     "-   unsigned window_position_x;\n"
     "-   unsigned window_position_y;\n"
     "+   unsigned window_position_pos;\n",
     { "ui.m", "p = s->uints.window_position_pos;\n", NULL, NULL },
     0 },
   { "a reader left in a language no lane here compiles is caught",
     "-   unsigned window_position_x;\n"
     "-   unsigned window_position_y;\n"
     "+   unsigned window_position_pos;\n",
     { "ui.m", "r.x = s->uints.window_position_x;\n"
               "r.y = s->uints.window_position_y;\n", NULL, NULL },
     2 },
   { "a member that only moved is not reported",
     "-   unsigned pos_width;\n",
     { "a.h", "struct s {\n   unsigned pos_width;\n};\n",
       "b.c", "v = g->pos_width;\n" },
     0 },
   { "a name another struct still declares is not reported",
     "-   int scissor_x;\n",
     { "a.h", "struct other {\n   int scissor_x;\n};\n",
       "b.c", "v = o->scissor_x;\n" },
     0 },
   { "a renamed member is not reported against its new name",
     "-   unsigned tex_w;\n"
     "+   unsigned tex_dims;\n",
     { "a.c", "v = t->tex_dims;\n", NULL, NULL },
     0 },
   { "the name in a comment or a log string is not a reader",
     "-   unsigned atlas_width;\n",
     { "a.c", "/* atlas_width used to say this */\n"
              "RARCH_LOG(\"x.atlas_width\\n\");\n", NULL, NULL },
     0 },
   { "a member declared only as a bare word is still found",
     "-   unsigned camera_height;\n",
     { "a.mm", "x = self->camera_height;\n", NULL, NULL },
     1 },
   { "a common name is left alone",
     "-   void *data;\n",
     { "a.c", "v = q->data;\n", NULL, NULL },
     0 },
   { "a name declared in a declarator list still counts as declared",
     "-   int next_row;\n",
     { "a.c", "int seq_pos, break_pos, row, next_row, tick;\n"
              "v = replay->next_row;\n", NULL, NULL },
     0 }
};

static int selftest(void)
{
   size_t i, nf = sizeof(fixtures) / sizeof(fixtures[0]);
   unsigned bad = 0;
   for (i = 0; i < nf; i++)
   {
      const struct fixture *fx = &fixtures[i];
      struct ck_strv names = {0}, paths = {0};
      struct hits h        = {0};
      char *texts[2];
      size_t lens[2];
      size_t k;
      const char *mark = "ok  ";

      removed_members(fx->diff, &names);
      for (k = 0; k < 2 && fx->files[2 * k]; k++)
      {
         ck_strv_push(&paths, ck_strdup(fx->files[2 * k]));
         texts[k] = (char*)fx->files[2 * k + 1];
         lens[k]  = strlen(texts[k]);
      }
      scan(&names, &paths, texts, lens, &h);
      if (h.n != fx->want)
      {
         mark = "FAIL";
         bad++;
      }
      printf("%s %s (%lu reader(s), wanted %u)\n", mark, fx->what,
            (unsigned long)h.n, fx->want);
      hits_free(&h);
      ck_strv_free(&names);
      ck_strv_free(&paths);
   }
   printf("%lu fixture(s), %u failed\n", (unsigned long)nf, bad);
   return bad ? 1 : 0;
}

/* ---- the tree ------------------------------------------------------ */

struct walk_ctx
{
   const char *root;
   struct ck_strv *paths;
};

static int skip_dir(const char *name, const char *rel, void *ud)
{
   const struct walk_ctx *w = (const struct walk_ctx*)ud;
   struct ck_sb full = {0};
   const char *s;
   size_t k;
   int skip = 0;

   /* ('%s/%s' % (relpath(dir), name)).lstrip('./') */
   ck_sb_puts(&full, w->root);
   ck_sb_putc(&full, '/');
   ck_sb_puts(&full, rel);
   for (s = full.p; *s == '.' || *s == '/'; s++);
   for (k = 0; prune[k] && !skip; k++)
      skip = !strcmp(s, prune[k]) || !strcmp(name, prune[k]);
   ck_sb_free(&full);
   return skip;
}

static void on_file(const char *path, const char *rel, void *ud)
{
   const struct walk_ctx *w = (const struct walk_ctx*)ud;
   size_t k;
   (void)rel;
   for (k = 0; suffixes[k]; k++)
      if (ck_ends_with(path, suffixes[k]))
      {
         ck_strv_push(w->paths, ck_strdup(path));
         return;
      }
}

static void normalise_newlines(char *t, size_t *n)
{
   size_t i, o;
   for (i = o = 0; i < *n; i++)
   {
      if (t[i] == '\r')
      {
         t[o++] = '\n';
         if (i + 1 < *n && t[i + 1] == '\n')
            i++;
      }
      else
         t[o++] = t[i];
   }
   t[o] = '\0';
   *n   = o;
}

int main(int argc, char **argv)
{
   const char *rng = "origin/master..HEAD";
   struct ck_strv roots = {0}, names = {0}, paths = {0}, files = {0};
   struct ck_sb cmd = {0}, repr = {0};
   struct hits h    = {0};
   char **texts;
   size_t *lens;
   size_t dlen, i, k;
   char *diff;
   int a, st;

   for (a = 1; a < argc; a++)
      if (!strcmp(argv[a], "--selftest"))
         return selftest();
   for (a = 1; a < argc; a++)
   {
      if (!strcmp(argv[a], "--range"))
      {
         if (++a >= argc)
         {
            fprintf(stderr, "removed_member_check: --range needs A..B\n");
            return 2;
         }
         rng = argv[a];
      }
      else if (!strncmp(argv[a], "--range=", 8))
         rng = argv[a] + 8;
      else
         ck_strv_push(&roots, ck_strdup(argv[a]));
   }

   /* The pruned trees are left out of the diff as they are out of the
    * scan: a vendored library's members are its own API, read through
    * its own headers, and deleting the library removes nothing
    * RetroArch declares. */
   ck_sb_puts(&cmd, "git diff --unified=0 ");
   ck_sb_shq(&cmd, rng);
   ck_sb_puts(&cmd, " -- .");
   ck_sb_puts(&repr, "['git', 'diff', '--unified=0', ");
   ck_repr(&repr, rng, rng + strlen(rng));
   ck_sb_puts(&repr, ", '--', '.'");
   for (k = 0; prune[k]; k++)
   {
      if (!strcmp(prune[k], ".git"))
         continue;
      ck_sb_puts(&cmd, " ':(exclude)");
      ck_sb_puts(&cmd, prune[k]);
      ck_sb_puts(&cmd, "'");
      ck_sb_puts(&repr, ", ':(exclude)");
      ck_sb_puts(&repr, prune[k]);
      ck_sb_puts(&repr, "'");
   }
   ck_sb_puts(&repr, "]");
#ifdef _WIN32
   ck_sb_puts(&cmd, " 2>NUL");
#else
   ck_sb_puts(&cmd, " 2>/dev/null");
#endif
   diff = ck_capture(cmd.p, &dlen, &st);
   ck_sb_free(&cmd);
   if (st != 0)
   {
      if (st < 0)
         printf("cannot read %s: cannot run git\n", rng);
      else
         printf("cannot read %s: Command '%s' returned non-zero exit"
               " status %d.\n", rng, repr.p, st);
      return 2;
   }
   ck_sb_free(&repr);
   normalise_newlines(diff, &dlen);
   removed_members(diff, &names);
   free(diff);
   if (!names.n)
   {
      printf("%s removes no struct member\n", rng);
      return 0;
   }

   if (!roots.n)
      ck_strv_push(&roots, ck_strdup("."));
   for (i = 0; i < roots.n; i++)
   {
      struct walk_ctx w;
      struct ck_sb r = {0};
      if (ck_is_file(roots.v[i]))
      {
         ck_strv_push(&files, ck_strdup(roots.v[i]));
         continue;
      }
      ck_sb_puts(&r, roots.v[i]);
      while (r.len > 1 && r.p[r.len - 1] == '/')
         r.p[--r.len] = '\0';
      w.root  = r.p;
      w.paths = &files;
      ck_walk(r.p, skip_dir, on_file, &w);
      ck_sb_free(&r);
   }
   ck_strv_sort(&files);
   texts = (char**)ck_alloc((files.n + 1) * sizeof(char*));
   lens  = (size_t*)ck_alloc((files.n + 1) * sizeof(size_t));
   for (i = 0, k = 0; i < files.n; i++)
   {
      if (i && !strcmp(files.v[i], files.v[i - 1]))
         continue;
      if (!(texts[k] = ck_read_file(files.v[i], &lens[k])))
         continue;
      ck_strv_push(&paths, ck_strdup(files.v[i]));
      k++;
   }

   scan(&names, &paths, texts, lens, &h);
   for (i = 0; i < h.n; i++)
      printf("%s:%lu: %s - %s no longer exists\n", h.v[i].path,
            h.v[i].line, h.v[i].name, h.v[i].name);
   printf("%s removes %lu member(s), %lu reader(s) left behind\n", rng,
         (unsigned long)names.n, (unsigned long)h.n);
   a = h.n ? 1 : 0;
   for (i = 0; i < paths.n; i++)
      free(texts[i]);
   free(texts);
   free(lens);
   hits_free(&h);
   ck_strv_free(&names);
   ck_strv_free(&paths);
   ck_strv_free(&files);
   ck_strv_free(&roots);
   return a;
}
