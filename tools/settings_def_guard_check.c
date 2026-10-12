/* A setting's label exists wherever its translations name it.
 *
 * The rule this holds
 * -------------------
 * A translation file under intl/ names a label for every string it has,
 * and is compiled in every build. Where a setting belongs to a feature,
 * intl/json2h.py copies the guard of its row in settings/settings_def_*.h
 * into the translation file, so the name is only there when the label is.
 *
 * It makes one exception: a guard line that mentions a SETTINGS_DEF_*
 * pass is dropped whole, because such a line is meant to be the same for
 * every build once the pass is known - "HAVE_X or the strings pass",
 * "not the configuration pass".
 *
 * A line that mentions a pass and still depends on a feature breaks that:
 *
 *     #if defined(HAVE_X) && !defined(SETTINGS_DEF_CONFIG_PASS)
 *
 * json2h.py drops it, the translations name the label in every build,
 * and the label only exists with HAVE_X. Nothing fails until the first
 * translation of the string arrives from Crowdin; then every build
 * without the feature stops compiling. That happened in October 2026
 * with the MCP server's token and the SMB client's Kerberos rows.
 *
 * What is checked
 * ---------------
 * For every guard line that mentions a pass, in a settings/settings_def_*.h
 * that msg_hash.h takes its labels from: with the passes set as msg_hash.h
 * sets them (the label list) and as intl/msg_hash_us.h sets them (the
 * strings), the line has to come out the same whatever the feature macros
 * are - within the builds that compile the row's region of msg_hash.h at
 * all.
 *
 * A line that fails names the feature it still depends on. The usual
 * repair is "defined(HAVE_X) || defined(SETTINGS_DEF_STRINGS_PASS)", as
 * the rows around it have.
 *
 * A guard is read as defined(X), a bare macro, a whole number, !, &&, ||
 * and parentheses; anything else is reported as a guard that cannot be
 * read.
 *
 * Usage:  tools/check.sh settings_def_guard_check [--root DIR] [--selftest] */

#include "check_common.h"

#define NPASSES 3
static const char *const passes[NPASSES] = {
   "SETTINGS_DEF_STRINGS_PASS", "SETTINGS_DEF_ENUM_PASS",
   "SETTINGS_DEF_CONFIG_PASS"
};
/* the two consumers that decide whether a label and its string exist:
 * msg_hash.h and intl/msg_hash_us.h, and what each sets the passes to */
static const int consumers[2][NPASSES] = { { 1, 1, 0 }, { 1, 0, 0 } };

/* more names on one line than this would need tables past 8 MiB */
#define MAX_NAMES 26

enum tok
{
   T_NAME, T_TRUE, T_FALSE, T_AND, T_OR, T_NOT, T_LP, T_RP, T_END
};

/* a parsed condition, in postfix */
struct cond
{
   int *ops;          /* T_NAME, T_TRUE, T_FALSE, T_AND, T_OR, T_NOT */
   int *args;         /* for T_NAME: index into names */
   size_t n;
   size_t cap;
   struct ck_strv names;
};

struct lexer
{
   const char *p;
   struct cond *c;
   int tok;
   int name;
   int bad;
};

static int mentions_pass(const char *s)
{
   int i;
   for (i = 0; i < NPASSES; i++)
   {
      if (strstr(s, passes[i]))
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

static const char *skip_space(const char *p)
{
   while (CK_IS_SPACE(*p))
      p++;
   return p;
}

/* The condition of a #if/#ifdef/#ifndef/#elif line as text, comments
 * removed, or NULL. */
static char *condition(const char *line)
{
   const char *b      = line;
   const char *e      = line + strlen(line);
   struct ck_sb sb    = {0};
   const char *p, *w;
   char *res;

   trim(&b, &e);
   if (b == e || *b != '#')
      return NULL;
   for (p = b + 1; p < e && CK_IS_SPACE(*p); p++);
   if (e - p >= 5 && (!strncmp(p, "ifdef", 5) || !strncmp(p, "ifndef", 6)))
   {
      int neg = p[2] == 'n';
      p      += neg ? 6 : 5;
      if (p < e && CK_IS_SPACE(*p))
      {
         for (; p < e && CK_IS_SPACE(*p); p++);
         for (w = p; w < e && CK_IS_WORD(*w); w++);
         if (w > p)
         {
            ck_sb_puts(&sb, neg ? "!defined(" : "defined(");
            ck_sb_add(&sb, p, (size_t)(w - p));
            ck_sb_putc(&sb, ')');
            return sb.p;
         }
      }
      return NULL;
   }
   if (e - p >= 2 && !strncmp(p, "el", 2))
      p += 2;
   if (e - p < 3 || strncmp(p, "if", 2) || !CK_IS_SPACE(p[2]))
      return NULL;
   for (p += 2; p < e && CK_IS_SPACE(*p); p++);
   while (p < e)
   {
      if (p[0] == '/' && e - p >= 2 && p[1] == '*')
      {
         const char *q;
         for (q = p + 2; e - q >= 2 && !(q[0] == '*' && q[1] == '/'); q++);
         if (e - q >= 2)
         {
            p = q + 2;
            continue;
         }
      }
      ck_sb_putc(&sb, *p++);
   }
   ck_sb_puts(&sb, "");
   b = sb.p;
   e = sb.p + sb.len;
   trim(&b, &e);
   res = ck_strndup(b, (size_t)(e - b));
   ck_sb_free(&sb);
   return res;
}

static int cond_name(struct cond *c, const char *s, size_t n)
{
   size_t i;
   for (i = 0; i < c->names.n; i++)
   {
      if (strlen(c->names.v[i]) == n && !memcmp(c->names.v[i], s, n))
         return (int)i;
   }
   ck_strv_push(&c->names, ck_strndup(s, n));
   return (int)(c->names.n - 1);
}

static void cond_emit(struct cond *c, int op, int arg)
{
   if (c->n == c->cap)
   {
      c->cap  = c->cap ? c->cap * 2 : 16;
      c->ops  = (int*)ck_realloc(c->ops, c->cap * sizeof(int));
      c->args = (int*)ck_realloc(c->args, c->cap * sizeof(int));
   }
   c->ops[c->n]    = op;
   c->args[c->n++] = arg;
}

static void cond_free(struct cond *c)
{
   free(c->ops);
   free(c->args);
   ck_strv_free(&c->names);
   memset(c, 0, sizeof(*c));
}

/* defined(X) and defined X are found wherever the letters stand, as
 * the scripts before this did; the words and, or, not are operators. */
static void next_tok(struct lexer *lx)
{
   const char *p = skip_space(lx->p);
   const char *w;

   if (!*p)
      lx->tok = T_END;
   else if (*p == '(' || *p == ')')
   {
      lx->tok = *p++ == '(' ? T_LP : T_RP;
   }
   else if (p[0] == '&' && p[1] == '&')
   {
      lx->tok = T_AND;
      p      += 2;
   }
   else if (p[0] == '|' && p[1] == '|')
   {
      lx->tok = T_OR;
      p      += 2;
   }
   else if (p[0] == '!' && p[1] != '=')
   {
      lx->tok = T_NOT;
      p++;
   }
   else if (CK_IS_WORD(*p))
   {
      const char *d, *q, *r;
      for (w = p; CK_IS_WORD(*w); w++);
      /* the first "defined" in this word that an operand follows */
      for (d = p; (d = strstr(d, "defined")) && d < w; d++)
      {
         if (d + 7 != w)
            continue;
         q = skip_space(w);
         if (*q == '(')
         {
            q = skip_space(q + 1);
            for (r = q; CK_IS_WORD(*r); r++);
            if (r > q && *skip_space(r) == ')')
            {
               if (d > p)
                  break;
               lx->tok  = T_NAME;
               lx->name = cond_name(lx->c, q, (size_t)(r - q));
               lx->p    = skip_space(r) + 1;
               return;
            }
         }
         if (q > w && CK_IS_WORD(*q))
         {
            if (d > p)
               break;
            for (r = q; CK_IS_WORD(*r); r++);
            lx->tok  = T_NAME;
            lx->name = cond_name(lx->c, q, (size_t)(r - q));
            lx->p    = r;
            return;
         }
      }
      /* what stands before such a "defined" is a word of its own */
      if (d && d < w && d > p)
         w = d;
      if (CK_IS_DIGIT(*p))
      {
         int any = 0;
         q       = p;
         if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X') && w - p > 2)
         {
            for (q = p + 2; q < w && (CK_IS_DIGIT(*q)
                     || (*q >= 'a' && *q <= 'f')
                     || (*q >= 'A' && *q <= 'F')); q++)
               any |= *q != '0';
         }
         else if (p[0] != '0' || strspn(p, "0") == (size_t)(w - p))
         {
            for (; q < w && CK_IS_DIGIT(*q); q++)
               any |= *q != '0';
         }
         if (q != w)
            lx->bad = 1;
         lx->tok = any ? T_TRUE : T_FALSE;
      }
      else if (w - p == 3 && !strncmp(p, "and", 3))
         lx->tok = T_AND;
      else if (w - p == 2 && !strncmp(p, "or", 2))
         lx->tok = T_OR;
      else if (w - p == 3 && !strncmp(p, "not", 3))
         lx->tok = T_NOT;
      else if (w - p == 1 && *p == 'V')
         lx->tok = T_TRUE;
      else
      {
         lx->tok  = T_NAME;
         lx->name = cond_name(lx->c, p, (size_t)(w - p));
      }
      p = w;
   }
   else
   {
      lx->bad = 1;
      lx->tok = T_END;
   }
   lx->p = p;
}

static void parse_or(struct lexer *lx, int depth);

static void parse_not(struct lexer *lx, int depth)
{
   if (lx->bad)
      return;
   if (lx->tok == T_NOT)
   {
      next_tok(lx);
      if (depth > 200)
         lx->bad = 1;
      else
         parse_not(lx, depth + 1);
      cond_emit(lx->c, T_NOT, 0);
   }
   else if (lx->tok == T_LP)
   {
      next_tok(lx);
      if (lx->tok == T_RP)
         cond_emit(lx->c, T_FALSE, 0);
      else if (depth > 200)
         lx->bad = 1;
      else
         parse_or(lx, depth + 1);
      if (lx->tok != T_RP)
         lx->bad = 1;
      next_tok(lx);
      if (lx->tok == T_LP)
         lx->bad = 1;
   }
   else if (lx->tok == T_NAME || lx->tok == T_TRUE || lx->tok == T_FALSE)
   {
      cond_emit(lx->c, lx->tok, lx->name);
      next_tok(lx);
      if (lx->tok == T_LP)
         lx->bad = 1;
   }
   else
      lx->bad = 1;
}

static void parse_and(struct lexer *lx, int depth)
{
   parse_not(lx, depth);
   while (!lx->bad && lx->tok == T_AND)
   {
      next_tok(lx);
      parse_not(lx, depth);
      cond_emit(lx->c, T_AND, 0);
   }
}

static void parse_or(struct lexer *lx, int depth)
{
   parse_and(lx, depth);
   while (!lx->bad && lx->tok == T_OR)
   {
      next_tok(lx);
      parse_and(lx, depth);
      cond_emit(lx->c, T_OR, 0);
   }
}

/* Returns 0 for a guard that cannot be read. */
static int compile_condition(const char *text, struct cond *c)
{
   struct lexer lx;
   memset(c, 0, sizeof(*c));
   lx.p    = text;
   lx.c    = c;
   lx.bad  = 0;
   lx.name = 0;
   next_tok(&lx);
   parse_or(&lx, 0);
   if (lx.bad || lx.tok != T_END)
   {
      cond_free(c);
      return 0;
   }
   return 1;
}

/* One 32-assignment word of a condition's truth table. var[] maps the
 * condition's names to a bit of the assignment, or -1 - value for a
 * constant. Assignment bit i < 5 varies within the word. */
static unsigned long cond_word(const struct cond *c, const int *var,
      unsigned long word, unsigned long *stack)
{
   static const unsigned long low[5] = {
      0xaaaaaaaaUL, 0xccccccccUL, 0xf0f0f0f0UL, 0xff00ff00UL, 0xffff0000UL
   };
   size_t i, sp = 0;
   for (i = 0; i < c->n; i++)
   {
      switch (c->ops[i])
      {
         case T_NAME:
            {
               int v = var[c->args[i]];
               stack[sp++] = v < 0 ? (v == -2 ? 0xffffffffUL : 0)
                  : v < 5 ? low[v]
                  : ((word >> (v - 5)) & 1) ? 0xffffffffUL : 0;
            }
            break;
         case T_TRUE:
            stack[sp++] = 0xffffffffUL;
            break;
         case T_FALSE:
            stack[sp++] = 0;
            break;
         case T_NOT:
            stack[sp - 1] = ~stack[sp - 1] & 0xffffffffUL;
            break;
         case T_AND:
            sp--;
            stack[sp - 1] &= stack[sp];
            break;
         default:
            sp--;
            stack[sp - 1] |= stack[sp];
            break;
      }
   }
   return stack[0];
}

static int index_of(const struct ck_strv *sv, const char *s)
{
   size_t i;
   for (i = 0; i < sv->n; i++)
   {
      if (!strcmp(sv->v[i], s))
         return (int)i;
   }
   return -1;
}

static int *map_vars(const struct cond *c, const struct ck_strv *every,
      const int *pass_val)
{
   int *var = (int*)ck_alloc((c->names.n + 1) * sizeof(int));
   size_t i;
   for (i = 0; i < c->names.n; i++)
   {
      int k, p = -1;
      for (k = 0; k < NPASSES; k++)
      {
         if (!strcmp(c->names.v[i], passes[k]))
            p = k;
      }
      var[i] = p >= 0 ? -1 - pass_val[p] : index_of(every, c->names.v[i]);
   }
   return var;
}

/* Names of the features a pass-mentioning guard still depends on,
 * sorted. Returns 0 when the guard, or one around its file's include
 * in msg_hash.h, cannot be read. regions holds each include's guards,
 * the lists separated by NULL. */
static int check_line(const char *text, char **regions, size_t nregions,
      struct ck_strv *depends)
{
   struct cond fn;
   struct ck_strv features = {0};
   size_t i, r, at = 0;
   int ok = 1;

   if (!compile_condition(text, &fn))
      return 0;
   for (i = 0; i < fn.names.n; i++)
   {
      int k, is_pass = 0;
      for (k = 0; k < NPASSES; k++)
         is_pass |= !strcmp(fn.names.v[i], passes[k]);
      if (!is_pass)
         ck_strv_push(&features, ck_strdup(fn.names.v[i]));
   }
   ck_strv_sort(&features);

   for (r = 0; ok && features.n && r < nregions; r++)
   {
      struct cond *reg     = NULL;
      size_t nreg          = 0;
      struct ck_strv every = {0};
      size_t depth         = fn.n;
      int consumer;

      for (i = 0; i < features.n; i++)
         ck_strv_push(&every, ck_strdup(features.v[i]));
      for (; regions[at]; at++)
      {
         size_t k;
         if (!ok || mentions_pass(regions[at]))
            continue;
         reg = (struct cond*)ck_realloc(reg, (nreg + 1) * sizeof(*reg));
         if (!compile_condition(regions[at], &reg[nreg]))
         {
            ok = 0;
            continue;
         }
         for (k = 0; k < reg[nreg].names.n; k++)
         {
            if (index_of(&every, reg[nreg].names.v[k]) < 0)
               ck_strv_push(&every, ck_strdup(reg[nreg].names.v[k]));
         }
         if (reg[nreg].n > depth)
            depth = reg[nreg].n;
         nreg++;
      }
      at++;
      if (ok && every.n > MAX_NAMES)
      {
         fprintf(stderr, "settings_def_guard_check: %lu macros decide"
               " one guard, more than %d:\n    %s\n",
               (unsigned long)every.n, MAX_NAMES, text);
         exit(2);
      }
      for (consumer = 0; ok && consumer < 2; consumer++)
      {
         size_t words          = every.n > 5 ? (size_t)1 << (every.n - 5) : 1;
         unsigned long live    = every.n >= 5 ? 0xffffffffUL
            : (1UL << (1UL << every.n)) - 1;
         unsigned long *stack  = (unsigned long*)ck_alloc(
               (depth + 1) * sizeof(unsigned long));
         unsigned long *f      = (unsigned long*)ck_alloc(
               words * sizeof(unsigned long));
         unsigned long *in     = (unsigned long*)ck_alloc(
               words * sizeof(unsigned long));
         int *fvar             = map_vars(&fn, &every, consumers[consumer]);
         size_t w, k;

         for (w = 0; w < words; w++)
         {
            f[w]  = cond_word(&fn, fvar, (unsigned long)w, stack);
            in[w] = live;
         }
         for (k = 0; k < nreg; k++)
         {
            int *rvar = map_vars(&reg[k], &every, consumers[consumer]);
            for (w = 0; w < words; w++)
               in[w] &= cond_word(&reg[k], rvar, (unsigned long)w, stack);
            free(rvar);
         }
         /* a feature matters if switching it alone, within the builds
          * that compile the region, changes the outcome */
         for (k = 0; k < features.n; k++)
         {
            static const unsigned long lo[5] = {
               0x55555555UL, 0x33333333UL, 0x0f0f0f0fUL, 0x00ff00ffUL,
               0x0000ffffUL
            };
            int bit    = index_of(&every, features.v[k]);
            int found  = 0;
            if (index_of(depends, features.v[k]) >= 0)
               continue;
            if (bit < 5)
            {
               int sh = 1 << bit;
               for (w = 0; !found && w < words; w++)
                  found = (in[w] & (in[w] >> sh) & (f[w] ^ (f[w] >> sh))
                        & lo[bit]) != 0;
            }
            else
            {
               size_t step = (size_t)1 << (bit - 5);
               for (w = 0; !found && w < words; w++)
                  found = !(w & step) && (in[w] & in[w | step]
                        & (f[w] ^ f[w | step])) != 0;
            }
            if (found)
               ck_strv_push(depends, ck_strdup(features.v[k]));
         }
         free(stack);
         free(f);
         free(in);
         free(fvar);
      }
      for (i = 0; i < nreg; i++)
         cond_free(&reg[i]);
      free(reg);
      ck_strv_free(&every);
   }
   ck_strv_sort(depends);
   ck_strv_free(&features);
   cond_free(&fn);
   return ok;
}

/* a preprocessor line: # then the directive */
static const char *directive(const char *s, const char *name)
{
   size_t n = strlen(name);
   if (*s != '#')
      return NULL;
   s = skip_space(s + 1);
   return strncmp(s, name, n) ? NULL : s + n;
}

/* An #include "settings/..." line of msg_hash.h: what follows the
 * directive, and the guard lines around it, outermost first. */
struct include_site
{
   char *what;
   struct ck_strv guards;
};

struct include_sites
{
   struct include_site *v;
   size_t n;
};

static void find_includes(const char *text, struct include_sites *out)
{
   struct ck_strv stack = {0};
   const char *line     = text;

   while (line)
   {
      const char *nl = strchr(line, '\n');
      const char *s  = line;
      const char *e  = nl ? nl : line + strlen(line);
      const char *t;
      char *own      = NULL;

      trim(&s, &e);
      if (s < e && *s == '#')
         s = own = ck_strndup(s, (size_t)(e - s));
      line = nl ? nl + 1 : NULL;
      if (!own)
         continue;
      if (directive(s, "if"))
         ck_strv_push(&stack, condition(s));
      else if (directive(s, "else"))
      {
         if (stack.n)
         {
            char *top = stack.v[stack.n - 1];
            if (top && *top)
            {
               struct ck_sb neg = {0};
               ck_sb_puts(&neg, "!(");
               ck_sb_puts(&neg, top);
               ck_sb_putc(&neg, ')');
               stack.v[stack.n - 1] = neg.p;
            }
            else
               stack.v[stack.n - 1] = NULL;
            free(top);
         }
      }
      else if (directive(s, "elif"))
      {
         if (stack.n)
         {
            free(stack.v[stack.n - 1]);
            stack.v[stack.n - 1] = condition(s);
         }
      }
      else if (directive(s, "endif"))
      {
         if (stack.n)
            free(stack.v[--stack.n]);
      }
      else if ((t = directive(s, "include")) && CK_IS_SPACE(*t)
            && ck_starts_with(skip_space(t), "\"settings/"))
      {
         struct include_site *site;
         size_t i;
         out->v = (struct include_site*)ck_realloc(out->v,
               (out->n + 1) * sizeof(*out->v));
         site   = &out->v[out->n++];
         memset(site, 0, sizeof(*site));
         site->what = ck_strdup(skip_space(t));
         for (i = 0; i < stack.n; i++)
         {
            if (stack.v[i] && *stack.v[i])
               ck_strv_push(&site->guards, ck_strdup(stack.v[i]));
         }
      }
      free(own);
   }
   ck_strv_free(&stack);
}

/* The guards around each include of one def file: a NULL-terminated
 * list an include, in out; returns the count. */
static size_t region_guards(const struct include_sites *sites,
      const char *def_name, struct ck_strv *out)
{
   struct ck_sb inc = {0};
   size_t found     = 0;
   size_t n, i;

   ck_sb_puts(&inc, "\"settings/");
   ck_sb_puts(&inc, def_name);
   ck_sb_putc(&inc, '"');
   for (n = 0; n < sites->n; n++)
   {
      if (!ck_starts_with(sites->v[n].what, inc.p))
         continue;
      for (i = 0; i < sites->v[n].guards.n; i++)
         ck_strv_push(out, ck_strdup(sites->v[n].guards.v[i]));
      ck_strv_push(out, NULL);
      found++;
   }
   ck_sb_free(&inc);
   return found;
}

static int check_file(const char *root, const char *name,
      const struct include_sites *msg_hash, unsigned long *lines)
{
   struct ck_sb path      = {0};
   struct ck_strv regions = {0};
   size_t nregions        = 0;
   int have_regions       = 0;
   unsigned long n        = 0;
   int bad                = 0;
   char *text;
   char *line;

   ck_sb_puts(&path, root);
   ck_sb_puts(&path, "/settings/");
   ck_sb_puts(&path, name);
   text = ck_read_file(path.p, NULL);
   ck_sb_free(&path);
   if (!text)
      return 0;
   for (line = text; line && *line; )
   {
      char *nl             = strchr(line, '\n');
      struct ck_strv deps  = {0};
      const char *b, *e, *p;
      char *cond;
      char *shown;

      if (nl)
         *nl = '\0';
      n++;
      b    = line;
      e    = line + strlen(line);
      p    = skip_space(line);
      line = nl ? nl + 1 : NULL;
      if (*p != '#')
         continue;
      p = skip_space(p + 1);
      if (!strncmp(p, "el", 2))
         p += 2;
      if (strncmp(p, "if", 2) || !mentions_pass(b) || !(cond = condition(b)))
         continue;
      (*lines)++;
      if (!have_regions)
      {
         nregions     = region_guards(msg_hash, name, &regions);
         have_regions = 1;
      }
      /* msg_hash.h does not take its labels from this file: they are
       * written out there, and this guard does not decide whether
       * they exist */
      if (!nregions)
      {
         free(cond);
         continue;
      }
      trim(&b, &e);
      shown = ck_strndup(b, (size_t)(e - b));
      if (!check_line(cond, regions.v, nregions, &deps))
      {
         printf("settings/%s:%lu: cannot read this guard: %s\n", name, n,
               shown);
         bad++;
      }
      else if (deps.n)
      {
         size_t i;
         printf("settings/%s:%lu: this guard mentions a pass and still"
               " depends on ", name, n);
         for (i = 0; i < deps.n; i++)
            printf("%s%s", i ? ", " : "", deps.v[i]);
         printf(":\n    %s\n", shown);
         printf("  intl/json2h.py drops such a line, so the translations"
               " name the\n"
               "  label in builds that do not have it. Make the label"
               " exist in the\n");
         printf("  strings pass too: defined(%s) ||"
               " defined(SETTINGS_DEF_STRINGS_PASS).\n", deps.v[0]);
         bad++;
      }
      ck_strv_free(&deps);
      free(shown);
      free(cond);
   }
   free(text);
   ck_strv_free(&regions);
   return bad;
}

static int run(const char *root)
{
   struct ck_sb path    = {0};
   struct ck_strv names = {0};
   struct ck_strv defs  = {0};
   struct include_sites hash;
   unsigned long lines  = 0;
   int bad              = 0;
   char *msg_hash;
   size_t i;

   ck_sb_puts(&path, root);
   ck_sb_puts(&path, "/msg_hash.h");
   msg_hash = ck_read_file(path.p, NULL);
   if (!msg_hash)
   {
      printf("settings_def_guard_check: no msg_hash.h under %s\n", root);
      return 2;
   }
   hash.v = NULL;
   hash.n = 0;
   find_includes(msg_hash, &hash);
   free(msg_hash);
   path.len = 0;
   ck_sb_puts(&path, root);
   ck_sb_puts(&path, "/settings");
   ck_list_dir(path.p, &names);
   for (i = 0; i < names.n; i++)
   {
      if (     ck_starts_with(names.v[i], "settings_def_")
            && ck_ends_with(names.v[i], ".h")
            && strlen(names.v[i]) >= strlen("settings_def_.h"))
         ck_strv_push(&defs, ck_strdup(names.v[i]));
   }
   ck_strv_sort(&defs);
   for (i = 0; i < defs.n; i++)
      bad += check_file(root, defs.v[i], &hash, &lines);
   ck_strv_free(&names);
   ck_strv_free(&defs);
   for (i = 0; i < hash.n; i++)
   {
      free(hash.v[i].what);
      ck_strv_free(&hash.v[i].guards);
   }
   free(hash.v);
   ck_sb_free(&path);
   if (bad)
   {
      printf("FAIL settings_def_guard_check: %d guard(s)\n", bad);
      return 1;
   }
   printf("PASS settings_def_guard_check (%lu pass-mentioning guards in "
         "settings/settings_def_*.h come out the same in every build)\n",
         lines);
   return 0;
}

struct self_case
{
   const char *guard;
   const char *region;   /* one guard around the include, or NULL */
   const char *want;     /* features, as a list is printed */
};

static int selftest(void)
{
   static const char smb_region[] =
      "defined(HAVE_SMBCLIENT) || defined(HAVE_NFSCLIENT)";
   static const char smb_right[] =
      "(defined(HAVE_SMBCLIENT) || (defined(SETTINGS_DEF_STRINGS_PASS)"
      " && defined(HAVE_NFSCLIENT))) && !defined(SETTINGS_DEF_CONFIG_PASS)";
   static const struct self_case cases[] = {
      { "defined(HAVE_MCP) && !defined(SETTINGS_DEF_CONFIG_PASS)", NULL,
         "['HAVE_MCP']" },
      { "(defined(HAVE_MCP) || defined(SETTINGS_DEF_STRINGS_PASS))"
         " && !defined(SETTINGS_DEF_CONFIG_PASS)", NULL, "[]" },
      { "defined(HAVE_MCP) || defined(SETTINGS_DEF_STRINGS_PASS)", NULL,
         "[]" },
      { "!defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU))", NULL,
         "[]" },
      { "!defined(SETTINGS_DEF_CONFIG_PASS) || ((TARGET_OS_IOS))", NULL,
         "[]" },
      { "defined(HAVE_EGL) && !defined(SETTINGS_DEF_STRINGS_PASS)"
         " && !defined(SETTINGS_DEF_CONFIG_PASS)"
         " && !defined(SETTINGS_DEF_ENUM_PASS)", NULL, "[]" },
      /* the SMB rows: wrong on their own, right with either client */
      { "defined(HAVE_SMBCLIENT) && !defined(SETTINGS_DEF_CONFIG_PASS)",
         smb_region, "['HAVE_SMBCLIENT']" },
      { smb_right, smb_region, "[]" },
      /* and that last form is wrong outside such a region */
      { smb_right, NULL, "['HAVE_NFSCLIENT', 'HAVE_SMBCLIENT']" }
   };
   struct cond c;
   size_t i, k;
   int bad = 0;

   for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
   {
      struct ck_strv deps = {0};
      struct ck_sb got    = {0};
      char *regions[2];
      regions[0] = (char*)cases[i].region;
      regions[1] = NULL;
      if (!check_line(cases[i].guard, regions, 1, &deps))
         ck_sb_puts(&got, "a guard that cannot be read");
      else
      {
         ck_sb_putc(&got, '[');
         for (k = 0; k < deps.n; k++)
         {
            ck_sb_puts(&got, k ? ", '" : "'");
            ck_sb_puts(&got, deps.v[k]);
            ck_sb_putc(&got, '\'');
         }
         ck_sb_putc(&got, ']');
      }
      if (strcmp(got.p, cases[i].want))
      {
         printf("selftest: %s\n   wanted %s, got %s\n", cases[i].guard,
               cases[i].want, got.p);
         bad++;
      }
      ck_strv_free(&deps);
      ck_sb_free(&got);
   }
   if (compile_condition(
            "_MSC_VER < 1900 && defined(SETTINGS_DEF_CONFIG_PASS)", &c))
   {
      printf("selftest: a guard that cannot be read was read\n");
      cond_free(&c);
      bad++;
   }
   if (bad)
   {
      printf("FAIL settings_def_guard_check --selftest\n");
      return 1;
   }
   printf("PASS settings_def_guard_check --selftest (%lu guards, right"
         " and wrong)\n", (unsigned long)(sizeof(cases) / sizeof(cases[0])));
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
         fprintf(stderr, "usage: settings_def_guard_check [--root DIR]"
               " [--selftest]\n");
         return 2;
      }
   }
   return self ? selftest() : run(root);
}
