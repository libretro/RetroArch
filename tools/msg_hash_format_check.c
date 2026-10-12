/* Translated format string parity check.
 *
 * Usage, from the repository root:
 *     tools/check.sh msg_hash_format_check [intl/msg_hash_xx.h ...]
 *     tools/check.sh msg_hash_format_check --selftest
 *     tools/check.sh msg_hash_format_check --rows FILE
 *
 * Some rows reach snprintf() as the format string, with the arguments
 * supplied at the call site - cheevos.c hands
 * MSG_CHEEVOS_HARDCORE_PAUSED_SYSTEM_NOT_FOR_CORE two char*, connmanctl.c
 * hands MSG_LOCALAP_STARTING two more. The call site cannot know which
 * language is loaded, so a translation whose conversions differ from the
 * English row's changes what snprintf reads off the argument list: a row
 * that gains a %s reads a pointer that was never passed.
 *
 * This compares each translated row's ordered conversion sequence against
 * the English row in intl/msg_hash_us.h and enforces:
 *
 *   * where English has conversions, the translation has the same ones in
 *     the same order;
 *   * no translation uses a positional %n$ conversion - it is POSIX, the
 *     MSVC runtime does not implement it, and the MSVC jobs build the
 *     same headers;
 *   * where a row that is used as a format string has no conversions in
 *     English, the translation has none either. That set is measured:
 *     format_site_keys() walks the sources for printf-family calls whose
 *     format argument resolves to msg_hash_to_str(), directly or through
 *     a local, and the MSG_* namespace is added to it so a site the scan
 *     cannot see still gets the rule. Everything else is displayed rather
 *     than formatted - menu_driver.c strlcpy()s sublabels, help resolves
 *     through msg_hash_get_help_us_enum(), which returns English whatever
 *     the language - so a percent sign in prose stays legal there, as it
 *     must: enforcing it would reject Hungarian's "50%-a" and every other
 *     language whose percent sign takes a suffix.
 *
 * A space flag counts as prose rather than a conversion. C defines that
 * flag for signed conversions only, no English row uses it deliberately,
 * and without the exemption "at 50% of this value" parses as a %o and
 * every faithful translation of it looks like a violation.
 *
 * intl/json2h.py drops a row that fails these rules instead of emitting
 * it, so the runtime falls back to the English row and a bad Crowdin
 * sync degrades to untranslated rather than to a bad vararg read. It asks
 * this program with --rows: FILE holds, per row, a line "KEY ENLEN TRLEN"
 * and then the English and the translated string, ENLEN and TRLEN bytes;
 * every row that may not ship is answered with a line "KEY<tab>WHY".
 * This check is what covers a header edited by hand.
 *
 * The English rows are read from intl/msg_hash_us.h with the
 * settings/settings_def_*.h files it includes expanded, as json2h.py
 * expands them. */

#include "msg_hash_common.h"

#define COUNT(a) (sizeof(a) / sizeof((a)[0]))

/* written in two pieces so that this file is not one of the sites */
#define TO_STR "msg_hash" "_to_str"

struct fmt_arg
{
   const char *name;
   unsigned index;
};

static const struct fmt_arg fmt_args[] = {
   { "snprintf", 2 }, { "vsnprintf", 2 }, { "sprintf", 1 }, { "printf", 0 },
   { "fprintf", 1 }, { "asprintf", 1 }, { "vasprintf", 1 }, { "swprintf", 2 },
   { "rcheevos_log", 0 }, { "runloop_msg_queue_pushf", 0 },
   { "RARCH_LOG", 0 }, { "RARCH_ERR", 0 }, { "RARCH_WARN", 0 },
   { "RARCH_DBG", 0 }, { "RARCH_LOG_OUTPUT", 0 }, { "CHEEVOS_LOG", 0 },
   { "CHEEVOS_ERR", 0 }
};

static const char *const skip_dirs[] = {
   ".git", "deps", "intl", "libretro-common"
};

/* A value of n bytes. */
struct blob
{
   char *p;
   size_t n;
};

static void die_exit(const char *msg)
{
   fprintf(stderr, "%s\n", msg);
   exit(1);
}

/* One conversion at s[i], which is '%':
 * %(?:(\d+)\$)?([-+ #0']*)(\*|\d+)?(?:\.(\*|\d*))?
 *  (hh|h|ll|l|L|j|z|t|q)?([diouxXeEfFgGaAcspn])
 * tried in the order the expression would try it. Returns the end, or
 * 0 when nothing matches. */
static size_t spec_match(const char *s, size_t n, size_t i, int *positional,
      int *space, char *conv)
{
   static const char *const lengths[] = {
      "hh", "h", "ll", "l", "L", "j", "z", "t", "q"
   };
   static const char convs[] = "diouxXeEfFgGaAcspn";
   size_t p = i + 1, d;
   int take;

   for (d = p; d < n && CK_IS_DIGIT(s[d]); d++);
   for (take = (d > p && d < n && s[d] == '$') ? 1 : 0; take >= 0; take--)
   {
      size_t q = take ? d + 1 : p;
      size_t fmax, f;
      for (fmax = q; fmax < n && strchr("-+ #0'", s[fmax]) && s[fmax]; fmax++);
      for (f = fmax + 1; f-- > q; )
      {
         size_t wopt[64];
         size_t nw = 0, wi, r = f;
         if (r < n && s[r] == '*')
            wopt[nw++] = 1;
         else if (r < n && CK_IS_DIGIT(s[r]))
         {
            size_t e;
            for (e = r; e < n && CK_IS_DIGIT(s[e]); e++);
            for (; e > r && nw < 63; e--)
               wopt[nw++] = e - r;
         }
         wopt[nw++] = 0;
         for (wi = 0; wi < nw; wi++)
         {
            size_t popt[66];
            size_t np = 0, pi, t = r + wopt[wi];
            if (t < n && s[t] == '.')
            {
               size_t e;
               if (t + 1 < n && s[t + 1] == '*')
                  popt[np++] = 2;
               for (e = t + 1; e < n && CK_IS_DIGIT(s[e]); e++);
               for (; np < 64; e--)
               {
                  popt[np++] = 1 + (e - (t + 1));
                  if (e == t + 1)
                     break;
               }
            }
            popt[np++] = 0;
            for (pi = 0; pi < np; pi++)
            {
               size_t u = t + popt[pi];
               size_t li;
               for (li = 0; li <= COUNT(lengths); li++)
               {
                  size_t v = u;
                  if (li < COUNT(lengths))
                  {
                     size_t ll = strlen(lengths[li]);
                     if (n - u < ll || memcmp(s + u, lengths[li], ll))
                        continue;
                     v += ll;
                  }
                  if (v < n && s[v] && strchr(convs, s[v]))
                  {
                     *positional = take;
                     *space      = memchr(s + q, ' ', f - q) != NULL;
                     *conv       = s[v];
                     return v + 1;
                  }
               }
            }
         }
      }
   }
   return 0;
}

/* The ordered conversion characters of a string, and whether one of
 * them is positional. */
static void conversions(const char *s, size_t n, struct ck_sb *convs,
      int *positional)
{
   size_t i = 0;
   ck_sb_puts(convs, "");
   *positional = 0;
   for (;;)
   {
      const char *pc = i < n ? (const char*)memchr(s + i, '%', n - i) : NULL;
      size_t end;
      int pos, space;
      char conv;
      if (!pc)
         break;
      i = (size_t)(pc - s);
      if (i + 1 < n && s[i + 1] == '%')
      {
         i += 2;
         continue;
      }
      if (!(end = spec_match(s, n, i, &pos, &space, &conv)))
      {
         i++;
         continue;
      }
      if (space)
      {
         i = end;
         continue;
      }
      if (pos)
         *positional = 1;
      ck_sb_putc(convs, conv);
      i = end;
   }
}

static void put_convs(struct ck_sb *sb, const struct ck_sb *c)
{
   size_t i;
   for (i = 0; i < c->len; i++)
   {
      ck_sb_putc(sb, '%');
      ck_sb_putc(sb, c->p[i]);
   }
}

/* Why this row may not ship, or NULL. formatted holds the keys used as
 * a format string; a key in the MSG_* namespace counts as formatted
 * whether or not it is in there. */
static char *row_error(const char *key, const char *en, size_t en_len,
      const char *tl, size_t tl_len, const struct ck_set *formatted)
{
   struct ck_sb ec  = {0};
   struct ck_sb tc  = {0};
   struct ck_sb why = {0};
   int ep, tp;

   conversions(en, en_len, &ec, &ep);
   conversions(tl, tl_len, &tc, &tp);
   if (tp)
      ck_sb_puts(&why, "uses a positional conversion, which the MSVC "
            "runtime does not implement");
   else if (ec.len)
   {
      if (strcmp(tc.p, ec.p))
      {
         ck_sb_puts(&why, "has conversions ");
         if (tc.len)
            put_convs(&why, &tc);
         else
            ck_sb_puts(&why, "none");
         ck_sb_puts(&why, " where English has ");
         put_convs(&why, &ec);
      }
   }
   else if (tc.len && (ck_starts_with(key, "MSG_")
            || (formatted && formatted->n
               && ck_set_find(formatted, key, strlen(key)))))
   {
      ck_sb_puts(&why, "has conversions ");
      put_convs(&why, &tc);
      ck_sb_puts(&why, " where English has none");
   }
   ck_sb_free(&ec);
   ck_sb_free(&tc);
   return why.p;
}

/* The format-site scan */

static const char *skip_ws(const char *p, const char *e)
{
   return mh_skip_space(p, e);
}

/* msg_hash_to_str\w*\(\s*([A-Z][A-Z0-9_]*)  at p; the key's end, or
 * NULL. */
static const char *to_str_at(const char *p, const char *e,
      const char **key)
{
   size_t n = strlen(TO_STR);
   const char *k;
   if ((size_t)(e - p) < n || memcmp(p, TO_STR, n))
      return NULL;
   for (p += n; p < e && CK_IS_WORD(*p); p++);
   if (p >= e || *p != '(')
      return NULL;
   p = skip_ws(p + 1, e);
   if (p >= e || !(*p >= 'A' && *p <= 'Z'))
      return NULL;
   for (k = p++; p < e && ((*p >= 'A' && *p <= 'Z') || CK_IS_DIGIT(*p)
            || *p == '_'); p++);
   *key = k;
   return p;
}

static void set_add_n(struct ck_set *set, const char *s, size_t n)
{
   void **slot = ck_set_add(set, s, n);
   if (!*slot)
      *slot = set;
}

/* \(?\s*(?:\(\s*const\s+char\s*\*\s*\)\s*)?(\w+)\s*\)?\Z  matched on
 * the whole of [b, e); the name, or 0. */
static int bare_name(const char *b, const char *e, const char **w,
      size_t *wn)
{
   int lp, cast;
   for (lp = 1; lp >= 0; lp--)
   {
      const char *p;
      if (lp && (b >= e || *b != '('))
         continue;
      p = skip_ws(b + lp, e);
      for (cast = 1; cast >= 0; cast--)
      {
         const char *q = p, *r;
         if (cast)
         {
            const char *s;
            if (q >= e || *q != '(')
               continue;
            q = skip_ws(q + 1, e);
            if (!mh_starts(q, e, "const"))
               continue;
            s = q + 5;
            q = skip_ws(s, e);
            if (q == s || !mh_starts(q, e, "char"))
               continue;
            q = skip_ws(q + 4, e);
            if (q >= e || *q != '*')
               continue;
            q = skip_ws(q + 1, e);
            if (q >= e || *q != ')')
               continue;
            q = skip_ws(q + 1, e);
         }
         for (r = q; r < e && CK_IS_WORD(*r); r++);
         if (r == q)
            continue;
         {
            const char *t = skip_ws(r, e);
            if (t == e || (*t == ')' && t + 1 == e))
            {
               *w  = q;
               *wn = (size_t)(r - q);
               return 1;
            }
         }
      }
   }
   return 0;
}

/* The arguments of the call whose '(' is at text[open], as the script
 * before this split them: the first keeps its '(', a string literal is
 * taken whole, and a list that does not close loses its last piece. */
static void call_args(const char *text, size_t len, size_t open,
      struct ck_strv *out)
{
   struct ck_sb cur = {0};
   int depth        = 0;
   size_t i         = open;

   ck_sb_puts(&cur, "");
   while (i < len)
   {
      char c = text[i];
      if (c == '"')
      {
         size_t j = i + 1;
         while (j < len && !(text[j] == '"' && text[j - 1] != '\\'))
            j++;
         ck_sb_add(&cur, text + i, (j < len ? j + 1 : len) - i);
         i = j + 1;
         continue;
      }
      if (c == '(' || c == '[' || c == '{')
         depth++;
      else if (c == ')' || c == ']' || c == '}')
      {
         if (--depth == 0)
         {
            ck_strv_push(out, ck_strndup(cur.p, cur.len));
            ck_sb_free(&cur);
            return;
         }
      }
      if (c == ',' && depth == 1)
      {
         ck_strv_push(out, ck_strndup(cur.p, cur.len));
         cur.len = 0;
         cur.p[0] = '\0';
         i++;
         continue;
      }
      if (depth >= 1)
         ck_sb_putc(&cur, c);
      i++;
   }
   ck_sb_free(&cur);
}

static int fmt_index(const char *w, size_t n)
{
   size_t i;
   for (i = 0; i < COUNT(fmt_args); i++)
   {
      if (strlen(fmt_args[i].name) == n && !memcmp(fmt_args[i].name, w, n))
         return (int)fmt_args[i].index;
   }
   return -1;
}

/* Keys whose string this translation unit uses as a format string. */
static void format_site_text(const char *text, size_t len, struct ck_set *keys)
{
   const char *e = text + len;
   const char *p;
   struct ck_set aliases = {0};
   size_t i;

   /* \b(\w+)\s*=\s*msg_hash_to_str\w*\(\s*([A-Z][A-Z0-9_]*)\s*\) */
   for (p = text; p < e; )
   {
      const char *w, *q, *k, *ke;
      if (!CK_IS_WORD(*p) || (p > text && CK_IS_WORD(p[-1])))
      {
         p++;
         continue;
      }
      for (w = p; p < e && CK_IS_WORD(*p); p++);
      q = skip_ws(p, e);
      if (q >= e || *q != '=')
         continue;
      q = skip_ws(q + 1, e);
      if (!(ke = to_str_at(q, e, &k)))
         continue;
      q = skip_ws(ke, e);
      if (q >= e || *q != ')')
         continue;
      {
         void **slot = ck_set_add(&aliases, w, (size_t)(p - w));
         if (!*slot)
         {
            *slot = ck_alloc(sizeof(struct ck_set));
            memset(*slot, 0, sizeof(struct ck_set));
         }
         set_add_n((struct ck_set*)*slot, k, (size_t)(ke - k));
      }
      p = q + 1;
   }

   /* \b(snprintf|...|CHEEVOS_ERR)\s*\( */
   for (p = text; p < e; )
   {
      const char *w, *q;
      struct ck_strv args = {0};
      int idx;
      if (!CK_IS_WORD(*p) || (p > text && CK_IS_WORD(p[-1])))
      {
         p++;
         continue;
      }
      for (w = p; p < e && CK_IS_WORD(*p); p++);
      if ((idx = fmt_index(w, (size_t)(p - w))) < 0)
         continue;
      q = skip_ws(p, e);
      if (q >= e || *q != '(')
         continue;
      call_args(text, len, (size_t)(q - text), &args);
      p = q + 1;
      if (args.n > (size_t)idx)
      {
         const char *b  = args.v[idx];
         const char *fe = b + strlen(b);
         const char *s, *k, *ke, *bw;
         size_t bn;
         mh_strip(&b, &fe);
         for (s = b; s < fe; )
         {
            if ((ke = to_str_at(s, fe, &k)))
            {
               set_add_n(keys, k, (size_t)(ke - k));
               s = ke;
            }
            else
               s++;
         }
         if (bare_name(b, fe, &bw, &bn))
         {
            void **slot = ck_set_find(&aliases, bw, bn);
            if (slot)
            {
               const struct ck_set *as = (const struct ck_set*)*slot;
               size_t k2;
               for (k2 = 0; k2 < as->cap; k2++)
               {
                  if (as->keys[k2])
                     set_add_n(keys, as->keys[k2], strlen(as->keys[k2]));
               }
            }
         }
      }
      ck_strv_free(&args);
   }
   for (i = 0; i < aliases.cap; i++)
   {
      struct ck_set *as = (struct ck_set*)aliases.vals[i];
      if (as)
      {
         size_t k;
         for (k = 0; k < as->cap; k++)
            free(as->keys[k]);
         free(as->keys);
         free(as->vals);
         free(as);
      }
      free(aliases.keys[i]);
   }
   free(aliases.keys);
   free(aliases.vals);
}

static int skip_dir(const char *name, const char *rel, void *ud)
{
   size_t i;
   (void)rel;
   (void)ud;
   if (name[0] == '.')
      return 1;
   for (i = 0; i < COUNT(skip_dirs); i++)
   {
      if (!strcmp(name, skip_dirs[i]))
         return 1;
   }
   return 0;
}

static void site_file(const char *path, const char *rel, void *ud)
{
   size_t len;
   char *text;
   (void)rel;
   if (!(ck_ends_with(path, ".c") || ck_ends_with(path, ".cpp")
            || ck_ends_with(path, ".m") || ck_ends_with(path, ".mm")))
      return;
   if (!(text = ck_read_file(path, &len)))
      return;
   if (mh_find(text, text + len, TO_STR, 0))
      format_site_text(text, len, (struct ck_set*)ud);
   free(text);
}

/* Keys used as a format string anywhere in the tree.
 *
 * Heuristic in one direction only: a site it misses keeps whatever the
 * MSG_* namespace already gives it, and a site it invents costs at most
 * a rule applied to a row that did not need it. */
static void format_site_keys(const char *root, struct ck_set *keys)
{
   ck_walk(root, skip_dir, site_file, keys);
}

/* The English rows */

static int is_s_type(const char *p, const char *e, const char *w)
{
   return mh_starts(p, e, w);
}

/* S_(BOOL|...|ACTION)(_EX|_LV|_AT|_AT_EX)?(_NS)?(_H)?\s*\(  at p,
 * tried in the order the expression would try it. Returns the end and
 * sets the type and the suffixes, or NULL. */
static const char *settings_row_at(const char *p, const char *e,
      int *action, int *lv, int *ns)
{
   static const char *const types[] = {
      "BOOL", "UINT", "INT", "FLOAT", "STRING_P", "STRING", "DIR",
      "PATH_DS", "PATH", "ACTION"
   };
   static const char *const ex[] = { "_EX", "_LV", "_AT", "_AT_EX", "" };
   size_t t, x;
   int a, b;
   if (!mh_starts(p, e, "S_"))
      return NULL;
   p += 2;
   for (t = 0; t < COUNT(types); t++)
   {
      const char *q;
      if (!is_s_type(p, e, types[t]))
         continue;
      q = p + strlen(types[t]);
      for (x = 0; x < COUNT(ex); x++)
      {
         const char *r;
         if (!mh_starts(q, e, ex[x]))
            continue;
         r = q + strlen(ex[x]);
         for (a = 1; a >= 0; a--)
         {
            const char *s = r;
            if (a)
            {
               if (!mh_starts(s, e, "_NS"))
                  continue;
               s += 3;
            }
            for (b = 1; b >= 0; b--)
            {
               const char *u = s;
               if (b)
               {
                  if (!mh_starts(u, e, "_H"))
                     continue;
                  u += 2;
               }
               u = skip_ws(u, e);
               if (u < e && *u == '(')
               {
                  *action = t == COUNT(types) - 1;
                  *lv     = x == 1;
                  *ns     = a;
                  return u + 1;
               }
            }
         }
      }
   }
   return NULL;
}

static void emit_row(struct ck_strv *out, const char *pfx, const char *token,
      const char *val)
{
   const char *c = strchr(val, '"');
   const char *d = strrchr(val, '"');
   struct ck_sb sb = {0};
   if (!c || d <= c)
      return;
   ck_strv_push(out, ck_strdup("MSG_HASH("));
   ck_sb_puts(&sb, "   ");
   ck_sb_puts(&sb, pfx);
   ck_sb_puts(&sb, token);
   ck_sb_putc(&sb, ',');
   ck_strv_push(out, sb.p);
   sb.p = NULL;
   sb.len = sb.cap = 0;
   ck_sb_puts(&sb, "   \"");
   ck_sb_add(&sb, c + 1, (size_t)(d - c - 1));
   ck_sb_putc(&sb, '"');
   ck_strv_push(out, sb.p);
   ck_strv_push(out, ck_strdup("   )"));
}

/* The MSG_HASH rows a settings_def_*.h file's S_ rows denote, with its
 * guard lines; a guard that names a SETTINGS_DEF pass is dropped with
 * its #endif, as it is always true for the strings consumer. */
static void def_to_rows(const char *text, size_t len, struct ck_strv *out)
{
   const char *e    = text + len;
   size_t i         = 0;
   char *skip_endif = (char*)ck_alloc(len + 1);
   size_t nskip     = 0;

   while (i < len)
   {
      const char *nl  = (const char*)memchr(text + i, '\n', len - i);
      size_t line_end = nl ? (size_t)(nl - text) : len;
      const char *lb  = text + i;
      const char *le  = text + line_end;
      const char *m;
      int action, lv, ns;

      mh_strip(&lb, &le);
      if (lb < le && *lb == '#')
      {
         if (mh_find(lb, le, "SETTINGS_DEF", 0))
         {
            skip_endif[nskip++] = 1;
            i = line_end + 1;
            continue;
         }
         if (mh_starts(lb, le, "#endif") && nskip && skip_endif[nskip - 1])
         {
            nskip--;
            i = line_end + 1;
            continue;
         }
         if (mh_starts(lb, le, "#if"))
            skip_endif[nskip++] = 0;
         else if (mh_starts(lb, le, "#endif") && nskip)
            nskip--;
         ck_strv_push(out, ck_strndup(text + i, line_end - i));
         i = line_end + 1;
         continue;
      }
      if (!(m = settings_row_at(text + i, e, &action, &lv, &ns)))
      {
         i = line_end + 1;
         continue;
      }
      {
         struct ck_strv args = {0};
         struct ck_sb arg    = {0};
         size_t jx           = (size_t)(m - text);
         int depth           = 1;
         int inq             = 0;
         const char *tb, *te;
         char *token;

         ck_sb_puts(&arg, "");
         while (depth)
         {
            char ch;
            if (jx >= len)
               die_exit("msg_hash_format_check: a settings row runs past"
                     " the end of its file");
            ch = text[jx];
            if (inq)
            {
               ck_sb_putc(&arg, ch);
               if (ch == '\\')
               {
                  if (jx + 1 >= len)
                     die_exit("msg_hash_format_check: a settings row runs"
                           " past the end of its file");
                  ck_sb_putc(&arg, text[jx + 1]);
                  jx++;
               }
               else if (ch == '"')
                  inq = 0;
            }
            else if (ch == '"')
            {
               inq = 1;
               ck_sb_putc(&arg, ch);
            }
            else if (ch == '(')
            {
               depth++;
               ck_sb_putc(&arg, ch);
            }
            else if (ch == ')')
            {
               if (--depth)
                  ck_sb_putc(&arg, ch);
            }
            else if (ch == ',' && depth == 1)
            {
               ck_strv_push(&args, ck_strndup(arg.p, arg.len));
               arg.len  = 0;
               arg.p[0] = '\0';
            }
            else
               ck_sb_putc(&arg, ch);
            jx++;
         }
         ck_strv_push(&args, ck_strndup(arg.p, arg.len));
         ck_sb_free(&arg);
         if (!action && args.n < 2)
            die_exit("msg_hash_format_check: a settings row with one"
                  " argument");
         tb = args.v[action ? 0 : 1];
         te = tb + strlen(tb);
         mh_strip(&tb, &te);
         token = ck_strndup(tb, (size_t)(te - tb));
         if (lv)
         {
            /* the value string belongs to the value token's owning def */
            if (!ns)
               emit_row(out, "MENU_ENUM_SUBLABEL_", token,
                     args.v[args.n - 1]);
         }
         else if (ns)
            emit_row(out, "MENU_ENUM_LABEL_VALUE_", token, args.v[args.n - 1]);
         else
         {
            if (args.n < 2)
               die_exit("msg_hash_format_check: a settings row with one"
                     " argument");
            emit_row(out, "MENU_ENUM_LABEL_VALUE_", token, args.v[args.n - 2]);
            emit_row(out, "MENU_ENUM_SUBLABEL_", token, args.v[args.n - 1]);
         }
         free(token);
         ck_strv_free(&args);
         i = jx;
      }
   }
   free(skip_endif);
}

/* #include "((?:\.\./)?settings/settings_def_\w+\.h)"  at the start of
 * the stripped line; the path, or NULL. */
static char *def_include(const char *b, const char *e)
{
   const char *p, *w;
   mh_strip(&b, &e);
   if (!mh_starts(b, e, "#include \""))
      return NULL;
   p = b + 10;
   w = p;
   if (mh_starts(p, e, "../"))
      p += 3;
   if (!mh_starts(p, e, "settings/settings_def_"))
      return NULL;
   p += 22;
   {
      const char *q;
      for (q = p; q < e && CK_IS_WORD(*q); q++);
      if (q == p || !mh_starts(q, e, ".h\""))
         return NULL;
      return ck_strndup(w, (size_t)(q + 2 - w));
   }
}

/* os.path.normpath of a/b */
static char *norm_join(const char *a, const char *b)
{
   struct ck_sb in  = {0};
   struct ck_sb out = {0};
   struct ck_strv parts = {0};
   const char *p;
   size_t i;
   int abs;

   ck_sb_puts(&in, a);
   ck_sb_putc(&in, '/');
   ck_sb_puts(&in, b);
   abs = in.p[0] == '/';
   for (p = in.p; *p; )
   {
      const char *s = p;
      while (*p && *p != '/')
         p++;
      if (p - s == 0 || (p - s == 1 && s[0] == '.'))
         ;
      else if (p - s == 2 && s[0] == '.' && s[1] == '.'
            && parts.n && strcmp(parts.v[parts.n - 1], ".."))
         free(parts.v[--parts.n]);
      else if (!(p - s == 2 && s[0] == '.' && s[1] == '.' && abs))
         ck_strv_push(&parts, ck_strndup(s, (size_t)(p - s)));
      while (*p == '/')
         p++;
   }
   ck_sb_puts(&out, abs ? "/" : "");
   for (i = 0; i < parts.n; i++)
   {
      if (i)
         ck_sb_putc(&out, '/');
      ck_sb_puts(&out, parts.v[i]);
   }
   if (!out.len)
      ck_sb_puts(&out, ".");
   ck_strv_free(&parts);
   ck_sb_free(&in);
   return out.p;
}

/* What parse_message() in json2h.py takes from a MSG_HASH(...) block:
 * the key and the raw text between the first and the last quote, with
 * comments before and after the value taken out. */
static void parse_message(const char *msg, char **key, char **value)
{
   struct ck_sb m = {0};
   long a, b, c, d;
   unsigned long guard = 0;
   long open, comma, start, end, len;
   char *f;

#define FIND(s, from) ((f = strstr(m.p + (from), (s))) ? (long)(f - m.p) : -1L)
   ck_sb_puts(&m, msg);
   a = FIND("/*", 0);
   b = FIND("*/", 0);
   c = FIND("\"", 0);
   while (a >= 0 && b >= 0 && ((a < c && c < b) || b < c))
   {
      struct ck_sb n = {0};
      if (++guard > 1000000)
         die_exit("msg_hash_format_check: parse_message loops");
      ck_sb_add(&n, m.p, (size_t)a);
      ck_sb_puts(&n, m.p + b + 2 <= m.p + m.len ? m.p + b + 2 : "");
      ck_sb_free(&m);
      m = n;
      c = (size_t)a <= m.len ? FIND("\"", a) : -1;
      b = (size_t)a <= m.len ? FIND("*/", a) : -1;
      a = (size_t)a <= m.len ? FIND("/*", a) : -1;
   }
   len   = (long)m.len;
   open  = (f = strchr(m.p, '(')) ? (long)(f - m.p) : -1;
   comma = (f = strchr(m.p, ',')) ? (long)(f - m.p) : -1;
   start = open + 1;
   end   = comma >= 0 ? comma : (len > 0 ? len - 1 : 0);
   {
      const char *kb = m.p + start;
      const char *ke = end > start ? m.p + end : kb;
      mh_strip(&kb, &ke);
      *key = ck_strndup(kb, (size_t)(ke - kb));
   }
#undef FIND
   for (;;)
   {
      long i;
      a = b = d = -1;
      for (i = (long)m.len - 2; i >= 0 && (a < 0 || b < 0); i--)
      {
         if (a < 0 && m.p[i] == '/' && m.p[i + 1] == '*')
            a = i;
         if (b < 0 && m.p[i] == '*' && m.p[i + 1] == '/')
            b = i;
      }
      f = strrchr(m.p, '"');
      d = f ? (long)(f - m.p) : -1;
      if (!(a >= 0 && b >= 0 && ((a < d && d < b) || a > d)))
         break;
      m.len    = (size_t)a;
      m.p[a]   = '\0';
   }
   len   = (long)m.len;
   start = c + 1;
   end   = d >= 0 ? d : (len > 0 ? len - 1 : 0);
   if (start > len)
      start = len;
   *value = end > start ? ck_strndup(m.p + start, (size_t)(end - start))
      : ck_strdup("");
   ck_sb_free(&m);
}

static int key_is_sane(const char *k)
{
   if (!CK_IS_IDENT0(*k))
      return 0;
   for (k++; *k; k++)
   {
      if (!CK_IS_WORD(*k))
         return 0;
   }
   return 1;
}

/* key -> English string, from the expanded intl/msg_hash_us.h. */
static void english_rows(const char *root, struct ck_set *english)
{
   struct ck_sb path     = {0};
   struct ck_sb base     = {0};
   struct ck_strv lines  = {0};
   struct ck_strv kept   = {0};
   size_t len, i;
   int skipping = 0;
   char *text;
   const char *p;

   ck_sb_puts(&base, root);
   ck_sb_puts(&base, "/intl");
   ck_sb_puts(&path, base.p);
   ck_sb_puts(&path, "/msg_hash_us.h");
   if (!(text = ck_read_file(path.p, &len)))
   {
      fprintf(stderr, "msg_hash_format_check: cannot read %s\n", path.p);
      exit(1);
   }
   /* the settings_def_*.h includes expanded */
   for (p = text; ; )
   {
      const char *nl = strchr(p, '\n');
      const char *e  = nl ? nl : p + strlen(p);
      char *inc      = def_include(p, e);
      int done       = 0;
      if (inc)
      {
         char *full = norm_join(base.p, inc);
         size_t dlen;
         char *def;
         if (ck_is_file(full) || ck_is_dir(full))
         {
            if (!(def = ck_read_file(full, &dlen)))
               def = ck_strdup(""), dlen = 0;
            def_to_rows(def, dlen, &lines);
            free(def);
            done = 1;
         }
         free(full);
         free(inc);
      }
      if (!done)
         ck_strv_push(&lines, ck_strndup(p, (size_t)(e - p)));
      if (!nl)
         break;
      p = nl + 1;
   }
   free(text);
   /* #define bodies out, so a macro's MSG_HASH text is never a row */
   for (i = 0; i < lines.n; i++)
   {
      const char *b = lines.v[i];
      const char *e = b + strlen(b);
      size_t k;
      if (skipping)
      {
         skipping = mh_rstrip_ends_with(b, e, '\\');
         continue;
      }
      while (b < e && (k = mh_space_at(b, e)))
         b += k;
      if (mh_starts(b, e, "#define"))
      {
         skipping = mh_rstrip_ends_with(lines.v[i], e, '\\');
         continue;
      }
      ck_strv_push(&kept, ck_strdup(lines.v[i]));
   }
   ck_strv_free(&lines);
   /* the rows */
   for (i = 0; i < kept.n; i++)
   {
      const char *b = kept.v[i];
      const char *e = b + strlen(b);
      mh_strip(&b, &e);
      if (mh_starts(b, e, "#if") || mh_starts(b, e, "#else")
            || mh_starts(b, e, "#endif"))
         continue;
      if (mh_starts(b, e, "#elif"))
         die_exit("packed emitter: #elif not supported");
      if (mh_starts(b, e, "MSG_HASH"))
      {
         struct ck_sb block = {0};
         struct ck_sb val   = {0};
         char *key, *value;
         void **slot;
         struct blob *bl;
         ck_sb_puts(&block, kept.v[i]);
         while (!mh_rstrip_ends_with(kept.v[i], kept.v[i] + strlen(kept.v[i]),
                  ')'))
         {
            if (++i >= kept.n)
               die_exit("msg_hash_format_check: a MSG_HASH row does not"
                     " close");
            ck_sb_putc(&block, '\n');
            ck_sb_puts(&block, kept.v[i]);
         }
         parse_message(block.p, &key, &value);
         if (!key_is_sane(key))
         {
            struct ck_sb m = {0};
            ck_sb_puts(&m, "packed emitter: insane key ");
            ck_repr(&m, key, key + strlen(key));
            die_exit(m.p);
         }
         mh_decode_literal(value, value + strlen(value), &val);
         ck_sb_puts(&val, "");
         slot = ck_set_add(english, key, strlen(key));
         if (*slot)
         {
            free(((struct blob*)*slot)->p);
            free(*slot);
         }
         bl    = (struct blob*)ck_alloc(sizeof(*bl));
         bl->p = val.p;
         bl->n = val.len;
         *slot = bl;
         free(key);
         free(value);
         ck_sb_free(&block);
      }
   }
   ck_strv_free(&kept);
   ck_sb_free(&path);
   ck_sb_free(&base);
}

/* [(key, string)] in blob order, for an already verified header. */
static void packed_rows(const char *text, size_t len, const char *lang,
      struct ck_strv *keys, struct ck_strv *rows, size_t **row_lens)
{
   struct mh_lines sec[4];
   struct mh_lines ml, il, dl;
   unsigned long *sizes;
   struct ck_strv strs = {0};
   size_t *lens        = NULL;
   struct ck_sb cur    = {0};
   size_t i, nm = 0;

   *row_lens = NULL;
   if (!mh_split_sections(text, len, lang, sec))
      die_exit("msg_hash_format_check: generated regions not found");
   mh_scan_guarded(&sec[0], 0, &ml);
   sizes = (unsigned long*)ck_alloc((ml.n + 1) * sizeof(*sizes));
   for (i = 0; i < ml.n; i++)
   {
      char *name;
      if (!mh_member(ml.v[i].b, ml.v[i].e, &name, &sizes[nm]))
         die_exit("msg_hash_format_check: an unverified header");
      free(name);
      nm++;
   }
   mh_scan_guarded(&sec[1], 0, &il);
   ck_sb_puts(&cur, "");
   for (i = 0; i < il.n; i++)
   {
      mh_line_literals(il.v[i].b, il.v[i].e, &cur);
      if (mh_rstrip_ends_with(il.v[i].b, il.v[i].e, ','))
      {
         lens = (size_t*)ck_realloc(lens, (strs.n + 1) * sizeof(*lens));
         lens[strs.n] = cur.len;
         ck_strv_push(&strs, ck_strndup(cur.p, cur.len));
         cur.len  = 0;
         cur.p[0] = '\0';
      }
   }
   for (i = 0; i < nm; i++)
   {
      struct ck_sb data = {0};
      if (i >= strs.n)
         die_exit("msg_hash_format_check: an unverified header");
      ck_sb_add(&data, strs.v[i], lens[i]);
      while (sizes[i] != (unsigned long)lens[i] + 1)
      {
         if (++i >= nm || i >= strs.n)
            die_exit("msg_hash_format_check: an unverified header");
         ck_sb_add(&data, strs.v[i], lens[i]);
      }
      ck_sb_puts(&data, "");
      *row_lens = (size_t*)ck_realloc(*row_lens,
            (rows->n + 1) * sizeof(**row_lens));
      (*row_lens)[rows->n] = data.len;
      ck_strv_push(rows, data.p);
   }
   mh_scan_guarded(&sec[3], 0, &dl);
   for (i = 0; i < dl.n; i++)
   {
      char *id = mh_ident(dl.v[i].b, dl.v[i].e);
      if (!id)
         die_exit("msg_hash_format_check: an unverified header");
      ck_strv_push(keys, id);
   }
   free(sizes);
   free(lens);
   ck_strv_free(&strs);
   ck_sb_free(&cur);
   mh_lines_free(&ml);
   mh_lines_free(&il);
   mh_lines_free(&dl);
   for (i = 0; i < 4; i++)
      mh_lines_free(&sec[i]);
}

static int check_file(const char *path, const struct ck_set *english,
      const struct ck_set *formatted)
{
   struct ck_strv keys = {0};
   struct ck_strv rows = {0};
   size_t *lens        = NULL;
   size_t len, i;
   int errors          = 0;
   char *lang          = mh_lang_of(path);
   char *text          = ck_read_file(path, &len);

   if (!text)
   {
      fprintf(stderr, "msg_hash_format_check: cannot read %s\n", path);
      exit(1);
   }
   if (mh_is_packed(text, len, lang))
   {
      packed_rows(text, len, lang, &keys, &rows, &lens);
      for (i = 0; i < keys.n && i < rows.n; i++)
      {
         void **slot = ck_set_find(english, keys.v[i], strlen(keys.v[i]));
         const struct blob *en;
         char *why;
         if (!slot)
            continue;
         en  = (const struct blob*)*slot;
         why = row_error(keys.v[i], en->p, en->n, rows.v[i], lens[i],
               formatted);
         if (why && *why)
         {
            printf("%s: %s %s\n", path, keys.v[i], why);
            errors++;
         }
         free(why);
      }
   }
   free(lens);
   ck_strv_free(&keys);
   ck_strv_free(&rows);
   free(text);
   free(lang);
   return errors;
}

/* --rows: json2h.py's rows, each answered with KEY<tab>WHY when it may
 * not ship. */
static int check_rows(const char *root, const char *file)
{
   struct ck_set formatted = {0};
   FILE *f                 = fopen(file, "rb");
   char line[512];

   if (!f)
   {
      fprintf(stderr, "msg_hash_format_check: cannot read %s\n", file);
      return 2;
   }
   format_site_keys(root, &formatted);
   while (fgets(line, sizeof(line), f))
   {
      char key[256];
      unsigned long en_len, tl_len;
      char *en, *tl, *why;
      if (sscanf(line, "%255s %lu %lu", key, &en_len, &tl_len) != 3)
      {
         fprintf(stderr, "msg_hash_format_check: a bad row in %s\n", file);
         fclose(f);
         return 2;
      }
      en = (char*)ck_alloc(en_len + 1);
      tl = (char*)ck_alloc(tl_len + 1);
      if (fread(en, 1, en_len, f) != en_len || fread(tl, 1, tl_len, f) != tl_len)
      {
         fprintf(stderr, "msg_hash_format_check: a short row in %s\n", file);
         fclose(f);
         return 2;
      }
      en[en_len] = tl[tl_len] = '\0';
      why = row_error(key, en, en_len, tl, tl_len, &formatted);
      if (why && *why)
         printf("%s\t%s\n", key, why);
      free(why);
      free(en);
      free(tl);
   }
   fclose(f);
   return 0;
}

/* Self-test. The cases are written here rather than taken from intl/,
 * so the result says the same thing whether or not the shipped headers
 * pass. */

struct row_case
{
   const char *label;
   const char *key;
   const char *en;
   const char *tl;
   const char *want;
};

static const struct row_case cases[] = {
   { "matching conversions",
      "MSG_X", "Starting with SSID=%s and key=%s",
      "Uruchamianie z SSID=%s i kluczem=%s", NULL },
   { "a conversion lost in translation",
      "MSG_X", "Starting with SSID=%s and key=%s",
      "Uruchamianie z SSID =% i kluczem=%s", "where English has" },
   { "a conversion invented",
      "MSG_X", "Runahead enabled.", "Wykonywanie %u.",
      "where English has none" },
   { "the same conversions reordered positionally",
      "MSG_X", "No achievements for %s using %s",
      "\344\275\277\347\224\250 %2$s \346\227\240\346\263\225\344\270\272"
      " %1$s", "positional" },
   { "a percent sign in English prose",
      "MENU_ENUM_SUBLABEL_X", "Kept at 50% of this value.",
      "Gehalten bei 50% dieses Wertes.", NULL },
   { "a percent sign taking a suffix",
      "MENU_ENUM_SUBLABEL_X", "Set to 100% for symmetry.",
      "50%-a szimmetria.", NULL },
   { "a conversion invented in a format row outside MSG_",
      "MENU_ENUM_LABEL_VALUE_FMT", "Applying: Default", "Toepassen: %s",
      "where English has none" },
   { "a percent sign in a row that is only displayed",
      "MENU_ENUM_LABEL_VALUE_X", "Applying: Default", "Toepassen: 50%-a",
      NULL },
   { "a doubled percent sign",
      "MSG_X", "Filled to %u%%", "Gevuld tot %u%%", NULL },
   { "prose percent in a formatted row",
      "MSG_X", "Loaded.", "Geladen op 50%-a", "where English has none" },
   { "a width and length modifier",
      "MSG_X", "Address %08X size %zu", "Adres %08X rozmiar %zu", NULL },
   { "a conversion type changed",
      "MSG_X", "Address %08X", "Adres %08s", "where English has" }
};

/* The format-site scan, which decides whether the third rule applies
 * to a row outside the MSG_* namespace. */
struct site_case
{
   const char *label;
   const char *source;
   int want;
};

static const struct site_case site_cases[] = {
   { "a format argument straight from the table",
      "snprintf(b, sizeof(b), " TO_STR "(MENU_ENUM_LABEL_VALUE_A), n);", 1 },
   { "a format argument through a local",
      "const char *fmt = " TO_STR "(MENU_ENUM_LABEL_VALUE_A);\n"
      "snprintf(b, sizeof(b), fmt, n);", 1 },
   { "a table string copied rather than formatted",
      "strlcpy(b, " TO_STR "(MENU_ENUM_LABEL_VALUE_A), sizeof(b));", 0 },
   { "a table string passed as a value",
      "snprintf(b, sizeof(b), \"%s: %u\", "
      TO_STR "(MENU_ENUM_LABEL_VALUE_A), n);", 0 },
   { "a local that is split rather than formatted",
      "const char *fmt = " TO_STR "(MENU_ENUM_LABEL_VALUE_A);\n"
      "snprintf(b, sizeof(b), \"%.*s%u\", (int)(p - fmt), fmt, n);", 0 }
};

static void free_keys(struct ck_set *set)
{
   size_t i;
   for (i = 0; i < set->cap; i++)
      free(set->keys[i]);
   free(set->keys);
   free(set->vals);
   memset(set, 0, sizeof(*set));
}

static int selftest(void)
{
   struct ck_set fmt_keys = {0};
   size_t i;
   int failures = 0;

   set_add_n(&fmt_keys, "MENU_ENUM_LABEL_VALUE_FMT", 25);
   for (i = 0; i < COUNT(site_cases); i++)
   {
      struct ck_set keys = {0};
      int got;
      format_site_text(site_cases[i].source, strlen(site_cases[i].source),
            &keys);
      got = ck_set_find(&keys, "MENU_ENUM_LABEL_VALUE_A", 23) != NULL;
      printf("%-40s %s\n", site_cases[i].label,
            got == site_cases[i].want ? "ok" : "FAILED");
      if (got != site_cases[i].want)
         failures++;
      free_keys(&keys);
   }
   for (i = 0; i < COUNT(cases); i++)
   {
      const struct row_case *c = &cases[i];
      char *got = row_error(c->key, c->en, strlen(c->en), c->tl,
            strlen(c->tl), &fmt_keys);
      int none  = !got || !*got;
      int ok    = c->want ? !none && strstr(got, c->want) != NULL : none;
      printf("%-40s %s\n", c->label, ok ? "ok" : "FAILED");
      if (!ok)
      {
         struct ck_sb r = {0};
         failures++;
         if (none)
            ck_sb_puts(&r, "None");
         else
            ck_repr(&r, got, got + strlen(got));
         printf("    got: %s\n", r.p);
         ck_sb_free(&r);
      }
      free(got);
   }
   free_keys(&fmt_keys);
   return failures ? 1 : 0;
}

int main(int argc, char **argv)
{
   struct ck_set english   = {0};
   struct ck_set formatted = {0};
   struct ck_strv paths    = {0};
   const char *root        = getenv("RA_TREE_ROOT");
   unsigned long failed    = 0, used = 0;
   size_t i;
   int a;

   if (!root || !*root)
      root = ".";
   for (a = 1; a < argc; a++)
   {
      if (!strcmp(argv[a], "--selftest"))
         return selftest();
   }
   if (argc == 3 && !strcmp(argv[1], "--rows"))
      return check_rows(root, argv[2]);
   for (a = 1; a < argc; a++)
   {
      if (!strncmp(argv[a], "--", 2))
      {
         fprintf(stderr, "usage: msg_hash_format_check [--selftest]"
               " [--rows FILE] [intl/msg_hash_xx.h ...]\n");
         return 2;
      }
      ck_strv_push(&paths, ck_strdup(argv[a]));
   }
   english_rows(root, &english);
   format_site_keys(root, &formatted);
   if (!paths.n)
      mh_glob_headers(root, &paths);
   for (i = 0; i < paths.n; i++)
   {
      if (check_file(paths.v[i], &english, &formatted))
         failed++;
   }
   if (failed)
   {
      printf("%lu header(s) hold a row whose conversions do not match "
            "English\n", failed);
      return 1;
   }
   for (i = 0; i < formatted.cap; i++)
   {
      if (formatted.keys[i] && ck_set_find(&english, formatted.keys[i],
               strlen(formatted.keys[i])))
         used++;
   }
   printf("%lu header(s) checked against %lu English rows, %lu of them "
         "used as a format string\n", (unsigned long)paths.n,
         (unsigned long)english.n, used);
   return 0;
}
