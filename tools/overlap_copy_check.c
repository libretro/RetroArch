/* Overlapping-buffer copy check.
 *
 * Flags calls to copy functions whose source and destination arguments
 * share the same base buffer expression. Two passes:
 *
 * 1. Direct: strlcpy/strlcat/strcpy/strncpy/strcat/strncat/memcpy calls
 *    whose dst and src arguments have the same normalized base
 *    expression (exact alias, or same base at different offsets), and
 *    sprintf/snprintf calls whose destination reappears as a format
 *    argument. memmove is the sanctioned fix and is never flagged.
 *
 * 2. Helper-mediated: function definitions whose body copies from one
 *    pointer parameter to another via the functions above WITHOUT an
 *    alias guard (no `dst == src` / `dst != src` comparison between the
 *    two parameters anywhere in the body), cross-referenced against
 *    every call site passing the same base expression for both.
 *
 * Rationale: strlcpy on macOS resolves to the fortified __strlcpy_chk,
 * which aborts via __chk_fail_overlap on ANY overlap including exact
 * aliasing (bd79aee: EXC_BREAKPOINT typing into the OSK), and
 * overlapping memcpy is undefined everywhere and flagged by ASan
 * (e942ee2: leaderboard widget element shift). The portable
 * strlcpy_retro__ fallback tolerating aliasing on Linux/Windows means
 * these defects ship silently everywhere except macOS; this check is
 * the only thing that notices before a Mac user does.
 *
 * Intentional same-buffer copies (overlap-guarded LZ match runs,
 * disjoint slice copies inside one array) are recorded in
 * tools/overlap_copy_allow.list as `file|func|dstbase|srcbase` entries;
 * see the header of that file. Exact-alias findings are never
 * allowlisted.
 *
 * Usage:
 *   tools/check.sh overlap_copy_check [--root DIR] [--allow FILE]
 *   tools/check.sh overlap_copy_check --emit-allow   (keys of all findings)
 *   tools/check.sh overlap_copy_check --selftest     (fixture check)
 *
 * Exit status: 0 clean, 1 findings (or self-test failure). */

#include "check_common.h"

static const char *const copy_funcs[] = {
   "strlcpy", "strlcat", "strcpy", "strncpy", "strcat", "strncat", "memcpy"
};
static const char *const printf_funcs[] = { "sprintf", "snprintf" };
static const char *const skip_dirs[] = {
   ".git", "deps", "obj-unix", "libogc", "pkg", "media"
};
static const char *const skip_prefixes[] = {
   "gfx/include/", "wii/", "gx/", "libretro-common/samples/",
   "libretro-common/test/"
};
static const char *const exts[] = { ".c", ".h", ".m", ".mm", ".cpp", ".cc" };

#define COUNT(a) (sizeof(a) / sizeof((a)[0]))

struct src_file
{
   char *rel;
   char *text;
   size_t len;
};

struct finding
{
   const char *rel;
   char *func;
   char *dst;
   char *src;
   const char *kind;
   unsigned long line;
};

/* One unguarded param->param copy of a helper. scope is NULL for a
 * tree-wide definition; for one inside a standalone harness
 * (samples/...) it is that directory, and the pair only applies to
 * call sites under it. */
struct helper_pair
{
   char *scope;
   size_t dst_idx;
   size_t src_idx;
};

struct helper
{
   struct helper_pair *pairs;
   size_t n;
};

struct state
{
   struct src_file *files;
   size_t nfiles;
   size_t cap;
   struct finding *found;
   size_t nfound;
   size_t found_cap;
   struct ck_set helpers;
   struct ck_set allow;
   struct ck_strv entries;
   size_t used;
};

/* Newlines before a position, for matches visited in rising order. */
struct line_ctr
{
   const char *at;
   unsigned long line;
};

static unsigned long line_of(struct line_ctr *lc, const char *pos)
{
   while (lc->at < pos)
   {
      if (*lc->at == '\n')
         lc->line++;
      lc->at++;
   }
   return lc->line;
}

static int in_list(const char *const *list, size_t n, const char *s, size_t len)
{
   size_t i;
   for (i = 0; i < n; i++)
   {
      if (strlen(list[i]) == len && !memcmp(list[i], s, len))
         return 1;
   }
   return 0;
}

static void trim(const char **b, const char **e)
{
   while (*b < *e && CK_IS_SPACE((*b)[0]))
      (*b)++;
   while (*e > *b && CK_IS_SPACE((*e)[-1]))
      (*e)--;
}

/* The next call of a copy function in [p, end):
 * \b(strlcpy|...|snprintf)\s*\(   Sets the name and returns the
 * position after the parenthesis, or NULL. */
static const char *next_call(const char *p, const char *end,
      const char **name, size_t *name_len, const char **start)
{
   while (p < end)
   {
      const char *w;
      const char *q;
      if (!CK_IS_WORD(*p))
      {
         p++;
         continue;
      }
      w = p;
      while (p < end && CK_IS_WORD(*p))
         p++;
      /* every name is 6 to 8 letters and starts with s or m */
      if (     (*w != 's' && *w != 'm') || p - w < 6 || p - w > 8
            || (  !in_list(copy_funcs, COUNT(copy_funcs), w, (size_t)(p - w))
               && !in_list(printf_funcs, COUNT(printf_funcs), w,
                  (size_t)(p - w))))
         continue;
      q = p;
      while (q < end && CK_IS_SPACE(*q))
         q++;
      if (q < end && *q == '(')
      {
         *name     = w;
         *name_len = (size_t)(p - w);
         *start    = w;
         return q + 1;
      }
   }
   return NULL;
}

/* Arguments of a call, given the text after '(' and how far to look.
 * Returns 0 when the call does not close within the window. */
static int split_args(const char *p, const char *end, size_t window,
      struct ck_strv *args)
{
   int depth       = 1;
   const char *cur = p;
   if ((size_t)(end - p) > window)
      end = p + window;
   for (; p < end; p++)
   {
      char ch = *p;
      if (ch == '(' || ch == '[' || ch == '{')
         depth++;
      else if (ch == ')' || ch == ']' || ch == '}')
      {
         if (--depth == 0)
         {
            const char *b = cur;
            const char *e = p;
            trim(&b, &e);
            ck_strv_push(args, ck_strndup(b, (size_t)(e - b)));
            return 1;
         }
      }
      if (ch == ',' && depth == 1)
      {
         const char *b = cur;
         const char *e = p;
         trim(&b, &e);
         ck_strv_push(args, ck_strndup(b, (size_t)(e - b)));
         cur = p + 1;
      }
   }
   ck_strv_free(args);
   return 0;
}

/* A leading pointer cast: ^\((?:const\s+)?[\w ]+\*+\s*\)
 * Returns the position after it, or b when there is none. */
static const char *skip_cast(const char *b, const char *e)
{
   int pass;
   if (b >= e || *b != '(')
      return b;
   for (pass = 0; pass < 2; pass++)
   {
      const char *r;
      const char *s;
      const char *p = b + 1;
      if (pass == 0)
      {
         const char *q;
         if (e - p < 6 || memcmp(p, "const", 5) || !CK_IS_SPACE(p[5]))
            continue;
         q = p + 5;
         while (q < e && CK_IS_SPACE(*q))
            q++;
         r = q;
         while (r < e && (CK_IS_WORD(*r) || *r == ' '))
            r++;
         /* [\w ]+ may take a space back from \s+ */
         if (r == q && !(q - (p + 5) >= 2 && q[-1] == ' '))
            continue;
      }
      else
      {
         r = p;
         while (r < e && (CK_IS_WORD(*r) || *r == ' '))
            r++;
         if (r == p)
            continue;
      }
      s = r;
      while (s < e && *s == '*')
         s++;
      if (s == r)
         continue;
      while (s < e && CK_IS_SPACE(*s))
         s++;
      if (s < e && *s == ')')
         return s + 1;
   }
   return b;
}

/* An argument's base buffer expression: casts, parentheses,
 * address-of, a trailing top-level +/- offset and a trailing
 * top-level [index] stripped, so &a[i] and &a[i + 1] come out alike. */
static char *base_expr(const char *s)
{
   const char *b = s;
   const char *e = s + strlen(s);
   const char *p;
   int depth;

   trim(&b, &e);
   b = skip_cast(b, e);
   trim(&b, &e);
   while (b < e && *b == '(' && e[-1] == ')')
   {
      int balanced = 1;
      depth        = 0;
      for (p = b + 1; p < e - 1; p++)
      {
         if (*p == '(')
            depth++;
         else if (*p == ')')
         {
            if (--depth < 0)
            {
               balanced = 0;
               break;
            }
         }
      }
      if (!balanced || depth != 0)
         break;
      b++;
      if (e > b)
         e--;
      trim(&b, &e);
   }
   if (b < e && *b == '&')
   {
      b++;
      trim(&b, &e);
   }
   depth = 0;
   for (p = b; p < e; p++)
   {
      char ch = *p;
      if (ch == '(' || ch == '[')
         depth++;
      else if (ch == ')' || ch == ']')
         depth--;
      else if (depth == 0 && p > b && (ch == '+'
               || (ch == '-' && (p + 1 >= e || p[1] != '>') && p[-1] != '-')))
      {
         e = p;
         trim(&b, &e);
         break;
      }
   }
   depth = 0;
   for (p = b; p < e; p++)
   {
      if (*p == '(')
         depth++;
      else if (*p == ')')
         depth--;
      else if (*p == '[' && depth == 0)
      {
         e = p;
         trim(&b, &e);
         break;
      }
   }
   return ck_strndup(b, (size_t)(e - b));
}

/* Leading identifier of an expression, for parameter matching. */
static char *base_ident(const char *s)
{
   const char *b = s;
   const char *e = s + strlen(s);
   const char *p;

   trim(&b, &e);
   b = skip_cast(b, e);
   trim(&b, &e);
   while (b < e && *b == '(')
   {
      b++;
      trim(&b, &e);
   }
   if (b < e && *b == '&')
   {
      b++;
      trim(&b, &e);
   }
   if (b >= e || !CK_IS_IDENT0(*b))
      return NULL;
   for (p = b; p < e && CK_IS_WORD(*p); p++);
   return ck_strndup(b, (size_t)(p - b));
}

/* A parameter's name: ([A-Za-z_]\w*)\s*(?:\[[^\]]*\])?\s*$ searched
 * in the trimmed declaration. NULL when there is none. */
static char *param_name(const char *b, const char *e)
{
   const char *s;
   trim(&b, &e);
   if (b == e || (e - b == 4 && !memcmp(b, "void", 4)))
      return NULL;
   for (s = b; s < e; s++)
   {
      const char *w;
      const char *t;
      if (!CK_IS_IDENT0(*s))
         continue;
      for (w = s; w < e && CK_IS_WORD(*w); w++);
      for (t = w; t < e && CK_IS_SPACE(*t); t++);
      if (t == e)
         return ck_strndup(s, (size_t)(w - s));
      if (*t == '[')
      {
         const char *u = (const char*)memchr(t + 1, ']', (size_t)(e - t - 1));
         if (u)
         {
            for (u++; u < e && CK_IS_SPACE(*u); u++);
            if (u == e)
               return ck_strndup(s, (size_t)(w - s));
         }
      }
   }
   return NULL;
}

/* Names of a parameter list, NULL where a parameter has none. */
static void param_names(const char *b, const char *e, struct ck_strv *names)
{
   int depth       = 0;
   const char *cur = b;
   const char *p;
   for (p = b; p < e; p++)
   {
      char ch = *p;
      if (ch == '(' || ch == '[' || ch == '{')
         depth++;
      else if (ch == ')' || ch == ']' || ch == '}')
         depth--;
      if (ch == ',' && depth == 0)
      {
         ck_strv_push(names, param_name(cur, p));
         cur = p + 1;
      }
   }
   if (cur < e)
      ck_strv_push(names, param_name(cur, e));
}

static size_t name_index(const struct ck_strv *names, const char *s)
{
   size_t i;
   for (i = 0; i < names->n; i++)
   {
      if (names->v[i] && !strcmp(names->v[i], s))
         return i;
   }
   return names->n;
}

/* x[0] and x[1] passed as the buffers themselves, not their
 * addresses: rows of a two-dimensional array, which do not overlap.
 * ^([A-Za-z_]\w*)\s*\[\s*(\d+)\s*\]$ */
static int row_of(const char *s, const char **name, size_t *name_len,
      const char **num, size_t *num_len)
{
   const char *p = s;
   if (!CK_IS_IDENT0(*p))
      return 0;
   while (CK_IS_WORD(*p))
      p++;
   *name     = s;
   *name_len = (size_t)(p - s);
   while (CK_IS_SPACE(*p))
      p++;
   if (*p++ != '[')
      return 0;
   while (CK_IS_SPACE(*p))
      p++;
   if (!CK_IS_DIGIT(*p))
      return 0;
   while (*p == '0' && CK_IS_DIGIT(p[1]))
      p++;
   *num = p;
   while (CK_IS_DIGIT(*p))
      p++;
   *num_len = (size_t)(p - *num);
   while (CK_IS_SPACE(*p))
      p++;
   return p[0] == ']' && p[1] == '\0';
}

static int distinct_rows(const char *a, const char *b)
{
   const char *an, *bn, *ad, *bd;
   size_t anl, bnl, adl, bdl;
   if (!row_of(a, &an, &anl, &ad, &adl) || !row_of(b, &bn, &bnl, &bd, &bdl))
      return 0;
   return anl == bnl && !memcmp(an, bn, anl)
      && !(adl == bdl && !memcmp(ad, bd, adl));
}

/* s is `word` then a word boundary. */
static int has_word(const char *s, const char *word)
{
   size_t n = strlen(word);
   return !strncmp(s, word, n) && !CK_IS_WORD(s[n]);
}

/* An "argument" that starts with a type is a prototype or definition:
 * ^(const\s|char\b|void\b|unsigned\b|signed\b|size_t\b|uint\d+_t\b|
 *   int\d*_t\b|struct\b) */
static int starts_with_type(const char *s, int direct)
{
   const char *p;
   if (!strncmp(s, "const", 5) && CK_IS_SPACE(s[5]))
      return 1;
   if (     has_word(s, "char") || has_word(s, "void")
         || has_word(s, "unsigned") || has_word(s, "size_t"))
      return 1;
   if (!direct)
      return 0;
   if (has_word(s, "signed") || has_word(s, "struct"))
      return 1;
   if (!strncmp(s, "uint", 4) && CK_IS_DIGIT(s[4]))
   {
      for (p = s + 4; CK_IS_DIGIT(*p); p++);
      if (has_word(p, "_t"))
         return 1;
   }
   if (!strncmp(s, "int", 3))
   {
      for (p = s + 3; CK_IS_DIGIT(*p); p++);
      if (has_word(p, "_t"))
         return 1;
   }
   return 0;
}

static int is_null(const char *s)
{
   return !strcmp(s, "NULL") || !strcmp(s, "0") || !strcmp(s, "nullptr");
}

static void add_finding(struct state *st, const char *rel, unsigned long line,
      const char *func, size_t func_len, const char *dst, const char *src,
      const char *kind)
{
   struct finding *f;
   if (st->nfound == st->found_cap)
   {
      st->found_cap = st->found_cap ? st->found_cap * 2 : 16;
      st->found     = (struct finding*)ck_realloc(st->found,
            st->found_cap * sizeof(*st->found));
   }
   f       = &st->found[st->nfound++];
   f->rel  = rel;
   f->line = line;
   f->func = ck_strndup(func, func_len);
   f->dst  = ck_strdup(dst);
   f->src  = ck_strdup(src);
   f->kind = kind;
}

/* Records a finding's allowlist key; returns 1 when an entry of the
 * allowlist names it (a key holding a '|' can match no entry). */
static int allow_lookup(struct state *st, const char *rel, const char *func,
      size_t func_len, const char *dst, const char *src, int may_allow)
{
   struct ck_sb key = {0};
   char *bd         = base_expr(dst);
   char *bs         = base_expr(src);
   void **slot      = NULL;
   int splits;

   ck_sb_puts(&key, rel);
   ck_sb_putc(&key, '|');
   ck_sb_add(&key, func, func_len);
   ck_sb_putc(&key, '|');
   ck_sb_puts(&key, bd);
   ck_sb_putc(&key, '|');
   ck_sb_puts(&key, bs);
   splits = strchr(rel, '|') || strchr(bd, '|') || strchr(bs, '|');
   if (may_allow && !splits)
      slot = ck_set_find(&st->allow, key.p, key.len);
   ck_strv_push(&st->entries, key.p);
   free(bd);
   free(bs);
   if (!slot)
      return 0;
   if (!*slot)
   {
      *slot = st;
      st->used++;
   }
   return 1;
}

static void direct_findings(struct state *st, const struct src_file *f)
{
   const char *end = f->text + f->len;
   const char *p   = f->text;
   const char *name, *start;
   size_t name_len;
   struct line_ctr lc;

   lc.at   = f->text;
   lc.line = 1;
   while ((p = next_call(p, end, &name, &name_len, &start)))
   {
      struct ck_strv args = {0};
      unsigned long line;
      char *dst;
      size_t i;

      if (!split_args(p, end, 2000, &args))
         continue;
      if (args.n < 2 || starts_with_type(args.v[0], 1))
      {
         ck_strv_free(&args);
         continue;
      }
      line = line_of(&lc, start);
      dst  = base_expr(args.v[0]);
      if (!*dst)
      {
         free(dst);
         ck_strv_free(&args);
         continue;
      }
      if (in_list(printf_funcs, COUNT(printf_funcs), name, name_len))
      {
         /* the arguments after the format */
         for (i = (name_len == 8 ? 3 : 2); i < args.n; i++)
         {
            char *a = base_expr(args.v[i]);
            if (!strcmp(a, dst) && !allow_lookup(st, f->rel, name, name_len,
                     args.v[0], args.v[i], 1))
               add_finding(st, f->rel, line, name, name_len, args.v[0],
                     args.v[i], "dst-reused-as-arg");
            free(a);
         }
      }
      else if (!is_null(dst))
      {
         char *src = base_expr(args.v[1]);
         if (*src && !strcmp(dst, src)
               && !distinct_rows(args.v[0], args.v[1]))
         {
            int exact = !strcmp(args.v[0], args.v[1]);
            if (!allow_lookup(st, f->rel, name, name_len, args.v[0],
                     args.v[1], !exact))
               add_finding(st, f->rel, line, name, name_len, args.v[0],
                     args.v[1], exact ? "exact-alias" : "same-base");
         }
         free(src);
      }
      free(dst);
      ck_strv_free(&args);
   }
}

/* An alias guard: \b(?:D\s*[!=]=\s*S|S\s*[!=]=\s*D)\b in the body. */
static int has_guard(const char *b, const char *e, const char *d,
      const char *s)
{
   size_t dl     = strlen(d);
   size_t sl     = strlen(s);
   const char *p = b;
   while (p < e)
   {
      const char *w;
      const char *q;
      const char *other;
      size_t ol;
      if (!CK_IS_WORD(*p))
      {
         p++;
         continue;
      }
      for (w = p; p < e && CK_IS_WORD(*p); p++);
      if ((size_t)(p - w) == dl && !memcmp(w, d, dl))
      {
         other = s;
         ol    = sl;
      }
      else if ((size_t)(p - w) == sl && !memcmp(w, s, sl))
      {
         other = d;
         ol    = dl;
      }
      else
         continue;
      for (q = p; q < e && CK_IS_SPACE(*q); q++);
      if (e - q < 2 || (*q != '!' && *q != '=') || q[1] != '=')
         continue;
      for (q += 2; q < e && CK_IS_SPACE(*q); q++);
      if ((size_t)(e - q) >= ol && !memcmp(q, other, ol)
            && (q + ol == e || !CK_IS_WORD(q[ol])))
         return 1;
   }
   return 0;
}

static int cmp_scope(const char *a, const char *b)
{
   if (!a || !b)
      return (a != NULL) - (b != NULL);
   return strcmp(a, b);
}

/* Keeps a helper's pairs sorted by (dst, src, scope), unscoped first. */
static void helper_add(struct state *st, const char *name, size_t name_len,
      size_t di, size_t si, const char *scope)
{
   void **slot      = ck_set_add(&st->helpers, name, name_len);
   struct helper *h = (struct helper*)*slot;
   size_t i;

   if (!h)
   {
      h     = (struct helper*)ck_alloc(sizeof(*h));
      h->n  = 0;
      h->pairs = NULL;
      *slot = h;
   }
   for (i = 0; i < h->n; i++)
   {
      const struct helper_pair *hp = &h->pairs[i];
      int c = hp->dst_idx != di ? (hp->dst_idx < di ? -1 : 1)
            : hp->src_idx != si ? (hp->src_idx < si ? -1 : 1)
            : cmp_scope(hp->scope, scope);
      if (c == 0)
         return;
      if (c > 0)
         break;
   }
   h->pairs = (struct helper_pair*)ck_realloc(h->pairs,
         (h->n + 1) * sizeof(*h->pairs));
   memmove(&h->pairs[i + 1], &h->pairs[i], (h->n - i) * sizeof(*h->pairs));
   h->pairs[i].dst_idx = di;
   h->pairs[i].src_idx = si;
   h->pairs[i].scope   = scope ? ck_strdup(scope) : NULL;
   h->n++;
}

/* The unguarded param->param copies in one function body. */
static void scan_body(struct state *st, const struct src_file *f,
      const char *fname, size_t fname_len, const struct ck_strv *pnames,
      const char *b, const char *e)
{
   const char *p = b;
   const char *name, *start;
   size_t name_len;

   while ((p = next_call(p, e, &name, &name_len, &start)))
   {
      struct ck_strv args = {0};
      size_t i, first, last;
      char *d;

      if (!split_args(p, e, 600, &args))
         continue;
      if (args.n < 2)
      {
         ck_strv_free(&args);
         continue;
      }
      if (in_list(printf_funcs, COUNT(printf_funcs), name, name_len))
      {
         first = name_len == 8 ? 3 : 2;
         last  = args.n;
      }
      else
      {
         first = 1;
         last  = 2;
      }
      d = base_ident(args.v[0]);
      for (i = first; d && i < last; i++)
      {
         char *s = base_ident(args.v[i]);
         size_t di, si;
         if (s && strcmp(d, s)
               && (di = name_index(pnames, d)) < pnames->n
               && (si = name_index(pnames, s)) < pnames->n
               && !has_guard(b, e, d, s))
         {
            char *scope = NULL;
            if (ck_starts_with(f->rel, "samples/"))
            {
               const char *slash = strrchr(f->rel, '/');
               scope = ck_strndup(f->rel, (size_t)(slash - f->rel));
            }
            helper_add(st, fname, fname_len, di, si, scope);
            free(scope);
         }
         free(s);
      }
      free(d);
      ck_strv_free(&args);
   }
}

/* Function definitions:
 * (?:^|\n)[A-Za-z_][\w \t\*]*?\b([A-Za-z_]\w*)\s*\(([^;{}()]*)\)\s*\n?\s*\{
 * A line that starts with an identifier; the name is the first later
 * word on it that the rest of the pattern follows. A match ends at its
 * brace and the next one starts at a line break at or after that. */
static void collect_helpers(struct state *st, const struct src_file *f)
{
   const char *text = f->text;
   const char *end  = text + f->len;
   const char *s    = text;
   const char *from = text;

   while (s && s < end)
   {
      const char *nl  = (const char*)memchr(s, '\n', (size_t)(end - s));
      const char *brace = NULL;
      const char *name = NULL, *name_end = NULL;
      const char *par = NULL, *par_end = NULL;
      const char *p;

      if (CK_IS_IDENT0(*s) && (s == text ? from == text : s - 1 >= from))
      {
         /* candidate names: later words of the leading [\w \t*] run */
         for (p = s + 1; p < end && !brace
               && (CK_IS_WORD(*p) || *p == ' ' || *p == '\t' || *p == '*'); p++)
         {
            const char *w, *q, *r;
            if (!CK_IS_IDENT0(*p) || CK_IS_WORD(p[-1]))
               continue;
            for (w = p; w < end && CK_IS_WORD(*w); w++);
            for (q = w; q < end && CK_IS_SPACE(*q); q++);
            if (q < end && *q == '(')
            {
               for (r = q + 1; r < end && *r != ';' && *r != '{'
                     && *r != '}' && *r != '(' && *r != ')'; r++);
               if (r < end && *r == ')')
               {
                  const char *t;
                  for (t = r + 1; t < end && CK_IS_SPACE(*t); t++);
                  if (t < end && *t == '{')
                  {
                     name     = p;
                     name_end = w;
                     par      = q + 1;
                     par_end  = r;
                     brace    = t;
                  }
               }
            }
            p = w - 1;
         }
      }
      if (brace)
      {
         size_t name_len = (size_t)(name_end - name);
         from            = brace + 1;
         if (     !in_list(copy_funcs, COUNT(copy_funcs), name, name_len)
               && !in_list(printf_funcs, COUNT(printf_funcs), name, name_len)
               && !(name_len == 7 && !memcmp(name, "memmove", 7)))
         {
            struct ck_strv pnames = {0};
            size_t i, named       = 0;
            param_names(par, par_end, &pnames);
            for (i = 0; i < pnames.n; i++)
               named += pnames.v[i] != NULL;
            if (named >= 2)
            {
               /* the body, as far as its closing brace */
               int depth     = 1;
               const char *i2 = brace + 1;
               while (i2 < end && depth)
               {
                  if (*i2 == '{')
                     depth++;
                  else if (*i2 == '}')
                     depth--;
                  i2++;
               }
               if (i2 - 1 > brace + 1)
                  scan_body(st, f, name, name_len, &pnames, brace + 1, i2 - 1);
            }
            ck_strv_free(&pnames);
         }
         s = (const char*)memchr(from, '\n', (size_t)(end - from));
         if (s)
            s++;
         continue;
      }
      s = nl ? nl + 1 : NULL;
   }
}

/* Calls of a tracked helper: \b(name|...)\s*\)?\s*\(  The optional
 * parenthesis is the function-pointer spelling (ozone->word_wrap)(...),
 * the exact form of the bd79aee OSK bug. */
static void helper_findings(struct state *st, const struct src_file *f)
{
   const char *end = f->text + f->len;
   const char *p   = f->text;
   struct line_ctr lc;

   lc.at   = f->text;
   lc.line = 1;
   while (p < end)
   {
      const char *w, *q;
      void **slot;
      struct helper *h;
      struct ck_strv args = {0};
      size_t i;

      if (!CK_IS_WORD(*p))
      {
         p++;
         continue;
      }
      for (w = p; p < end && CK_IS_WORD(*p); p++);
      if (!(slot = ck_set_find(&st->helpers, w, (size_t)(p - w))))
         continue;
      for (q = p; q < end && CK_IS_SPACE(*q); q++);
      if (q < end && *q == ')')
         for (q++; q < end && CK_IS_SPACE(*q); q++);
      if (q >= end || *q != '(')
         continue;
      h = (struct helper*)*slot;
      q++;
      if (!split_args(q, end, 1500, &args))
      {
         p = q;
         continue;
      }
      if (!starts_with_type(args.v[0], 0))
      {
         for (i = 0; i < h->n; i++)
         {
            const struct helper_pair *hp = &h->pairs[i];
            const char *a, *b;
            char *ba, *bb;
            if (hp->scope)
            {
               size_t sl = strlen(hp->scope);
               if (strncmp(f->rel, hp->scope, sl)
                     || (f->rel[sl] != '/' && f->rel[sl] != '\0'))
                  continue;
            }
            if (hp->dst_idx >= args.n || hp->src_idx >= args.n)
               continue;
            a  = args.v[hp->dst_idx];
            b  = args.v[hp->src_idx];
            ba = base_expr(a);
            bb = base_expr(b);
            if (*ba && !is_null(ba) && !strcmp(ba, bb)
                  && !allow_lookup(st, f->rel, w, (size_t)(p - w), a, b, 1))
               add_finding(st, f->rel, line_of(&lc, w), w, (size_t)(p - w),
                     a, b, "helper-aliased");
            free(ba);
            free(bb);
         }
      }
      ck_strv_free(&args);
      p = q;
   }
}

static int skip_dir(const char *name, const char *rel, void *ud)
{
   (void)rel;
   (void)ud;
   return in_list(skip_dirs, COUNT(skip_dirs), name, strlen(name));
}

static void add_file(const char *path, const char *rel, void *ud)
{
   struct state *st = (struct state*)ud;
   struct src_file *f;
   size_t i;

   for (i = 0; i < COUNT(exts); i++)
   {
      if (ck_ends_with(rel, exts[i]))
         break;
   }
   if (i == COUNT(exts))
      return;
   for (i = 0; i < COUNT(skip_prefixes); i++)
   {
      if (ck_starts_with(rel, skip_prefixes[i]))
         return;
   }
   if (st->nfiles == st->cap)
   {
      st->cap   = st->cap ? st->cap * 2 : 1024;
      st->files = (struct src_file*)ck_realloc(st->files,
            st->cap * sizeof(*st->files));
   }
   f       = &st->files[st->nfiles++];
   f->rel  = ck_strdup(rel);
   f->text = ck_read_file(path, &f->len);
   if (!f->text)
   {
      f->text = ck_strdup("");
      f->len  = 0;
   }
}

static int cmp_file(const void *a, const void *b)
{
   return strcmp(((const struct src_file*)a)->rel,
         ((const struct src_file*)b)->rel);
}

/* Allowlist entries order as tuples of their '|'-separated fields. */
static int cmp_entry(const void *a, const void *b)
{
   const unsigned char *x = *(const unsigned char * const *)a;
   const unsigned char *y = *(const unsigned char * const *)b;
   int cx, cy;
   while (*x && *x == *y)
   {
      x++;
      y++;
   }
   cx = !*x ? -2 : *x == '|' ? -1 : *x;
   cy = !*y ? -2 : *y == '|' ? -1 : *y;
   return cx - cy;
}

static void load_allow(struct state *st, const char *path)
{
   size_t len;
   char *text;
   char *line;

   if (!path || !*path || !(text = ck_read_file(path, &len)))
      return;
   for (line = text; line; )
   {
      char *nl      = strchr(line, '\n');
      const char *b = line;
      const char *e = nl ? nl : line + strlen(line);
      trim(&b, &e);
      if (b < e && *b != '#')
         ck_set_add(&st->allow, b, (size_t)(e - b));
      line = nl ? nl + 1 : NULL;
   }
   free(text);
}

static int run_check(const char *root, const char *allow_path,
      int emit_allow, struct ck_sb *out, struct ck_sb *err)
{
   struct state st;
   struct src_file *sorted;
   size_t i;
   int rc = 0;

   memset(&st, 0, sizeof(st));
   ck_walk(root, skip_dir, add_file, &st);
   load_allow(&st, allow_path);

   sorted = (struct src_file*)ck_alloc(st.nfiles * sizeof(*sorted));
   if (st.nfiles)
      memcpy(sorted, st.files, st.nfiles * sizeof(*sorted));
   qsort(sorted, st.nfiles, sizeof(*sorted), cmp_file);
   for (i = 0; i < st.nfiles; i++)
      direct_findings(&st, &sorted[i]);
   free(sorted);
   for (i = 0; i < st.nfiles; i++)
      collect_helpers(&st, &st.files[i]);
   if (st.helpers.n)
   {
      for (i = 0; i < st.nfiles; i++)
         helper_findings(&st, &st.files[i]);
   }

   if (emit_allow)
   {
      ck_strv_sort(&st.entries);
      for (i = 0; i < st.entries.n; i++)
      {
         if (i && !strcmp(st.entries.v[i], st.entries.v[i - 1]))
            continue;
         ck_sb_puts(out, st.entries.v[i]);
         ck_sb_putc(out, '\n');
      }
   }
   else
   {
      struct ck_strv stale = {0};
      for (i = 0; i < st.nfound; i++)
      {
         const struct finding *f = &st.found[i];
         ck_sb_puts(out, f->rel);
         ck_sb_putc(out, ':');
         ck_sb_putu(out, f->line);
         ck_sb_puts(out, ": ");
         ck_sb_puts(out, f->func);
         ck_sb_putc(out, '(');
         ck_sb_puts(out, f->dst);
         ck_sb_puts(out, ", ");
         ck_sb_puts(out, f->src);
         ck_sb_puts(out, ")  [");
         ck_sb_puts(out, f->kind);
         ck_sb_puts(out, "]\n");
      }
      for (i = 0; i < st.allow.cap; i++)
      {
         if (st.allow.keys[i] && !st.allow.vals[i])
            ck_strv_push(&stale, ck_strdup(st.allow.keys[i]));
      }
      if (stale.n > 1)
         qsort(stale.v, stale.n, sizeof(char*), cmp_entry);
      for (i = 0; i < stale.n; i++)
      {
         ck_sb_puts(err, "stale allowlist entry: ");
         ck_sb_puts(err, stale.v[i]);
         ck_sb_putc(err, '\n');
      }
      ck_strv_free(&stale);
      if (st.nfound)
      {
         ck_sb_putu(err, (unsigned long)st.nfound);
         ck_sb_puts(err,
               " overlapping-copy finding(s). Overlapping copies are UB\n"
               "and abort under macOS fortified libc; use memmove, a\n"
               "separate staging buffer, or an alias-guarded helper. For\n"
               "audited-intentional same-buffer copies add the printed\n"
               "file|func|dst|src key to tools/overlap_copy_allow.list.\n");
         rc = 1;
      }
      else
      {
         ck_sb_puts(err, "overlap copy check: clean (");
         ck_sb_putu(err, (unsigned long)st.nfiles);
         ck_sb_puts(err, " file(s), ");
         ck_sb_putu(err, (unsigned long)st.helpers.n);
         ck_sb_puts(err, " helper(s) tracked, ");
         ck_sb_putu(err, (unsigned long)st.used);
         ck_sb_puts(err, " allowlisted)\n");
      }
   }

   for (i = 0; i < st.nfiles; i++)
   {
      free(st.files[i].rel);
      free(st.files[i].text);
   }
   free(st.files);
   for (i = 0; i < st.nfound; i++)
   {
      free(st.found[i].func);
      free(st.found[i].dst);
      free(st.found[i].src);
   }
   free(st.found);
   ck_strv_free(&st.entries);
   return rc;
}

/* self-test; a flagged call is spelled in two pieces so that the tree
 * check does not read these fixtures as code */

static const char fixture_bad_exact[] =
   "\n"
   "void f(char *buf, const char *in, unsigned long n)\n"
   "{\n"
   "   str" "lcpy(buf, buf, n);\n"
   "}\n";

static const char fixture_bad_shift[] =
   "\n"
   "void g(struct info *a, int i, int count)\n"
   "{\n"
   "   mem" "cpy(&a[i], &a[i + 1], (count - i) * sizeof(a[i]));\n"
   "}\n";

static const char fixture_ok_rows[] =
   "\n"
   "void g(void)\n"
   "{\n"
   "   char rows[2][16];\n"
   "   memcpy(rows[1], rows[0], sizeof(rows[0]));\n"
   "}\n";

static const char fixture_ok_memmove[] =
   "\n"
   "void g(struct info *a, int i, int count)\n"
   "{\n"
   "   memmove(&a[i], &a[i + 1], (count - i) * sizeof(a[i]));\n"
   "}\n";

/* in two pieces: a C89 string literal holds 509 characters */
static const char fixture_helper_defs[] =
   "\n"
   "unsigned long wrap_like(char *s, unsigned long len, const char *src,\n"
   "      unsigned long src_len)\n"
   "{\n"
   "   if (src_len < 10)\n"
   "      return strlcpy(s, src, len);\n"
   "   return 0;\n"
   "}\n"
   "\n"
   "unsigned long join_guarded(char *s, const char *dir, unsigned long len)\n"
   "{\n"
   "   unsigned long _len = 0;\n"
   "   if (s != dir)\n"
   "      _len = strlcpy(s, dir, len);\n"
   "   return _len;\n"
   "}\n"
   "\n";

static const char fixture_helper_caller[] =
   "void caller(void)\n"
   "{\n"
   "   char msg[64];\n"
   "   char other[64];\n"
   "   wrap_like(msg, sizeof(msg), msg, 3);      /* BAD: helper-aliased */\n"
   "   wrap_like(msg, sizeof(msg), other, 3);    /* ok */\n"
   "   join_guarded(msg, msg, sizeof(msg));      /* ok: guarded helper */\n"
   "}\n";

static const char fixture_sample_stub[] =
   "\n"
   "size_t samp_join(char *out, const char *dir, size_t n)\n"
   "{\n"
   "   return strlcpy(out, dir, n);\n"
   "}\n"
   "void same_dir_caller(char *buf)\n"
   "{\n"
   "   samp_join(buf, buf, 64);\n"
   "}\n";

static const char fixture_tree_caller[] =
   "\n"
   "extern size_t samp_join(char *out, const char *dir, size_t n);\n"
   "void tree_caller(char *buf)\n"
   "{\n"
   "   samp_join(buf, buf, 64);\n"
   "}\n";

static const char fixture_allowed[] =
   "\n"
   "void lz(unsigned char *to, unsigned long have)\n"
   "{\n"
   "   /* deliberate disjoint doubling copy */\n"
   "   mem" "cpy(to + have, to, have);\n"
   "}\n";

/* names and contents alternate in sources; expect and forbid are
 * substrings of the output, each list NULL-terminated */
static int selftest_case(const char *name, const char *const *sources,
      const char *allow_line, const char *const *expect,
      const char *const *forbid)
{
   char *td            = ck_tmpdir();
   struct ck_sb path   = {0};
   struct ck_sb allow  = {0};
   struct ck_sb out    = {0};
   struct ck_sb err    = {0};
   int ok              = 1;
   int want_rc         = 0;
   int rc;
   size_t i;

   for (i = 0; sources[i]; i += 2)
   {
      path.len = 0;
      ck_sb_puts(&path, td);
      ck_sb_putc(&path, '/');
      ck_sb_puts(&path, sources[i]);
      ck_mkdirs_for(path.p);
      ck_write_file(path.p, sources[i + 1]);
   }
   if (allow_line)
   {
      struct ck_sb text = {0};
      ck_sb_puts(&allow, td);
      ck_sb_puts(&allow, "/allow.list");
      ck_sb_puts(&text, allow_line);
      ck_sb_putc(&text, '\n');
      ck_write_file(allow.p, text.p);
      ck_sb_free(&text);
   }
   rc = run_check(td, allow.p, 0, &out, &err);
   ck_sb_puts(&out, err.p ? err.p : "");
   for (i = 0; ok && expect[i]; i++)
   {
      if (strchr(expect[i], '['))
         want_rc = 1;
      if (!strstr(out.p, expect[i]))
      {
         printf("SELFTEST FAIL [%s]: missing '%s' in:\n%s\n", name,
               expect[i], out.p);
         ok = 0;
      }
   }
   for (i = 0; ok && forbid[i]; i++)
   {
      if (strstr(out.p, forbid[i]))
      {
         printf("SELFTEST FAIL [%s]: unexpected '%s' in:\n%s\n", name,
               forbid[i], out.p);
         ok = 0;
      }
   }
   if (ok && rc != want_rc)
   {
      printf("SELFTEST FAIL [%s]: rc=%d expected %d\n", name, rc, want_rc);
      ok = 0;
   }
   ck_rmtree(td);
   free(td);
   ck_sb_free(&path);
   ck_sb_free(&allow);
   ck_sb_free(&out);
   ck_sb_free(&err);
   return ok;
}

static int selftest(void)
{
   static const char *const none[]        = { NULL };
   static const char *const exact[]       = { "[exact-alias]", NULL };
   static const char *const same_base[]   = { "[same-base]", NULL };
   static const char *const clean[]       = { "clean", NULL };
   static const char *const memmove_[]    = { "memmove", NULL };
   static const char *const helper[]      = { "wrap_like", "[helper-aliased]", NULL };
   static const char *const guarded[]     = { "join_guarded", NULL };
   static const char *const stub[]        = {
      "samples/x/stub.c", "[helper-aliased]", NULL };
   static const char *const tree[]        = { "tree.c", NULL };
   static const char *const stale[]       = { "stale allowlist entry", NULL };
   static const char *const s_exact[]     = { "a.c", fixture_bad_exact, NULL };
   static const char *const s_shift[]     = { "a.c", fixture_bad_shift, NULL };
   static const char *const s_rows[]      = { "a.c", fixture_ok_rows, NULL };
   static const char *const s_memmove[]   = { "a.c", fixture_ok_memmove, NULL };
   static char fixture_helper[sizeof(fixture_helper_defs)
      + sizeof(fixture_helper_caller)];
   static const char *s_helper[]          = { "a.c", fixture_helper, NULL };
   static const char *const s_stub[]      = {
      "samples/x/stub.c", fixture_sample_stub,
      "tree.c", fixture_tree_caller, NULL };
   static const char *const s_allowed[]   = { "a.c", fixture_allowed, NULL };
   int ok = 1;

   strcpy(fixture_helper, fixture_helper_defs);
   strcat(fixture_helper, fixture_helper_caller);
   ok &= selftest_case("exact-alias flagged", s_exact, NULL, exact, none);
   ok &= selftest_case("overlapping shift flagged", s_shift, NULL,
         same_base, none);
   ok &= selftest_case("two rows of an array not flagged", s_rows, NULL,
         clean, same_base);
   ok &= selftest_case("memmove not flagged", s_memmove, NULL, clean,
         memmove_);
   ok &= selftest_case("unguarded helper aliased call flagged, guarded not",
         s_helper, NULL, helper, guarded);
   ok &= selftest_case("a sample's stub stays local to its harness",
         s_stub, NULL, stub, tree);
   ok &= selftest_case("allowlist suppresses same-base", s_allowed,
         "a.c|memcpy|to|to", clean, same_base);
   ok &= selftest_case("exact-alias cannot be allowlisted", s_exact,
         "a.c|strlcpy|buf|buf", exact, none);
   ok &= selftest_case("stale allowlist entry reported", s_memmove,
         "gone.c|memcpy|x|x", stale, none);
   printf("selftest: %s\n", ok ? "OK" : "FAILED");
   return ok ? 0 : 1;
}

static int usage(void)
{
   fprintf(stderr, "usage: overlap_copy_check [--root DIR] [--allow FILE]"
         " [--emit-allow] [--selftest]\n");
   return 2;
}

int main(int argc, char **argv)
{
   const char *root   = ".";
   const char *allow  = "tools/overlap_copy_allow.list";
   int emit_allow     = 0;
   int self           = 0;
   struct ck_sb out   = {0};
   struct ck_sb err   = {0};
   int i, rc;

   for (i = 1; i < argc; i++)
   {
      const char *a = argv[i];
      if (!strcmp(a, "--selftest"))
         self = 1;
      else if (!strcmp(a, "--emit-allow"))
         emit_allow = 1;
      else if (!strcmp(a, "--root") && i + 1 < argc)
         root = argv[++i];
      else if (!strncmp(a, "--root=", 7))
         root = a + 7;
      else if (!strcmp(a, "--allow") && i + 1 < argc)
         allow = argv[++i];
      else if (!strncmp(a, "--allow=", 8))
         allow = a + 8;
      else
         return usage();
   }
   if (self)
      return selftest();
   rc = run_check(root, allow, emit_allow, &out, &err);
   if (out.len)
      fwrite(out.p, 1, out.len, stdout);
   fflush(stdout);
   if (err.len)
      fwrite(err.p, 1, err.len, stderr);
   return rc;
}
