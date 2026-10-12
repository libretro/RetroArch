/* An extern "C" definition in a C++ file takes what its C prototype says.
 *
 * Why this exists: uwp_main.cpp carries the UWP definitions of several
 * win32_common.h functions (win32_check_window, win32_set_video_mode,
 * win32_get_client_rect, ...) inside an extern "C" block, and it does not
 * include win32_common.h. When win32_check_window's size parameters were
 * folded into one word, every C caller was updated and the compiler saw
 * each one - but a definition that never sees its prototype is not
 * checked against it, and the linker matches on the name alone. The UWP
 * build linked a five-parameter body to four-argument callers, and Xbox
 * crashed on boot (bisected to 0b2613d).
 *
 * This reads the tree as text: for each function defined inside an
 * extern "C" region of a .cpp or .mm file, it finds the prototype(s) of
 * that name in the .h files and compares the parameter lists, type by
 * type with the parameter names stripped. A definition whose parameters
 * differ from its prototype's is reported. A definition with no
 * prototype anywhere is not (a C++ file's own C-linkage helper is not
 * ours to judge).
 *
 * Usage:  tools/check.sh extern_c_prototype_check [--selftest] [path ...]
 * Exits non-zero when a mismatch is found. */

#include "check_common.h"

#define MASK '\001'
#define COUNT(a) (sizeof(a) / sizeof((a)[0]))

static const char *const keywords[] = {
   "if", "while", "for", "switch", "return", "sizeof", "defined", "do",
   "else", "case", "typedef", "struct", "union", "enum", "static",
   "extern", "const", "inline", "volatile", "register", "signed",
   "unsigned", "void", "char", "short", "int", "long", "float",
   "double", "__attribute__", "catch", "new", "delete", "throw"
};
static const char *const skip_dirs[] = {
   ".git", "deps", "pkg", "media", "dxsdk", "MESA", "vulkan", "GL",
   "GLES2", "GLES3", "EGL", "KHR", "vita_pib", "libgo2", "libogc",
   "lwbt", "lwip", "libwiikeyboard"
};
static const char *const shader_exts[] = {
   ".cg.h", ".hlsl.h", ".vert.h", ".frag.h", ".glsl.h"
};

/* a parameter list as types only */
struct params
{
   char **v;
   size_t n;
};

struct proto
{
   const char *path;
   struct params params;
   unsigned long line;
};

struct proto_list
{
   struct proto *v;
   size_t n;
};

struct sig
{
   const char *name;
   size_t name_len;
   struct params params;
   unsigned long line;
   char kind;            /* ';' prototype, '{' definition */
};

struct text
{
   char *t;
   size_t len;
   long *close;          /* for each '(' the index of its ')', or -1 */
   const char *line_at;
   unsigned long line;
};

static int in_list(const char *const *list, size_t n, const char *s,
      size_t len)
{
   size_t i;
   for (i = 0; i < n; i++)
   {
      if (strlen(list[i]) == len && !memcmp(list[i], s, len))
         return 1;
   }
   return 0;
}

/* Blanks comments and string/char literals, keeping newlines so line
 * numbers survive. A masked "C" is three MASK bytes. */
static void mask_source(char *t, size_t n)
{
   size_t i = 0;
   while (i < n)
   {
      size_t j;
      char c = t[i];
      if (c == '/' && i + 1 < n && t[i + 1] == '*')
      {
         for (j = i + 2; j + 1 < n && !(t[j] == '*' && t[j + 1] == '/'); j++);
         j = j + 1 < n ? j + 2 : n;
      }
      else if (c == '/' && i + 1 < n && t[i + 1] == '/')
      {
         /* to the end of the line, a line break in it included */
         for (j = i; j < n && t[j] != '\n'; j++)
            t[j] = MASK;
         i = j;
         continue;
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
      {
         if (t[i] != '\n')
            t[i] = MASK;
      }
   }
}

static void match_parens(struct text *tx)
{
   size_t *stack = (size_t*)ck_alloc((tx->len + 1) * sizeof(size_t));
   size_t sp     = 0;
   size_t i;

   tx->close = (long*)ck_alloc((tx->len + 1) * sizeof(long));
   for (i = 0; i < tx->len; i++)
   {
      if (tx->t[i] == '(')
      {
         tx->close[i] = -1;
         stack[sp++]  = i;
      }
      else if (tx->t[i] == ')' && sp)
         tx->close[stack[--sp]] = (long)i;
   }
   free(stack);
}

static unsigned long line_of(struct text *tx, const char *pos)
{
   while (tx->line_at < pos)
   {
      if (*tx->line_at == '\n')
         tx->line++;
      tx->line_at++;
   }
   return tx->line;
}

static void params_free(struct params *p)
{
   size_t i;
   for (i = 0; i < p->n; i++)
      free(p->v[i]);
   free(p->v);
   p->v = NULL;
   p->n = 0;
}

static int params_equal(const struct params *a, const struct params *b)
{
   size_t i;
   if (a->n != b->n)
      return 0;
   for (i = 0; i < a->n; i++)
   {
      if (strcmp(a->v[i], b->v[i]))
         return 0;
   }
   return 1;
}

/* One parameter as a type only: `unsigned *width` -> `unsigned*`,
 * `RECT* rect` -> `RECT*`, `void` -> `void`. */
static char *normalize_param(const char *b, const char *e)
{
   struct ck_sb clean  = {0};
   struct ck_sb out    = {0};
   struct ck_strv toks = {0};
   const char *p;
   size_t i;

   for (p = b; p < e; p++)
   {
      if (*p != MASK)
         ck_sb_putc(&clean, *p);
   }
   ck_sb_puts(&clean, "");
   /* \w+ and * are the tokens; [..] reads as a * (int a[] == int *a) */
   for (p = clean.p; *p; )
   {
      if (*p == '[' && strchr(p, ']'))
      {
         ck_strv_push(&toks, ck_strdup("*"));
         p = strchr(p, ']') + 1;
      }
      else if (*p == '*')
      {
         ck_strv_push(&toks, ck_strdup("*"));
         p++;
      }
      else if (CK_IS_WORD(*p))
      {
         const char *w = p;
         while (CK_IS_WORD(*p))
            p++;
         ck_strv_push(&toks, ck_strndup(w, (size_t)(p - w)));
      }
      else
         p++;
   }
   /* the trailing identifier is the name unless it is the whole type */
   if (toks.n >= 2)
   {
      const char *last = toks.v[toks.n - 1];
      const char *prev = toks.v[toks.n - 2];
      if (     *last != '*'
            && !in_list(keywords, COUNT(keywords), last, strlen(last))
            && !ck_ends_with(last, "_t")
            && strcmp(prev, "struct") && strcmp(prev, "union")
            && strcmp(prev, "enum"))
         free(toks.v[--toks.n]);
   }
   for (i = 0; i < toks.n; i++)
   {
      const char *t = toks.v[i];
      if (!strcmp(t, "const") || !strcmp(t, "volatile")
            || !strcmp(t, "register"))
         continue;
      if (*t != '*' && out.len)
         ck_sb_putc(&out, ' ');
      ck_sb_puts(&out, t);
   }
   ck_sb_puts(&out, "");
   ck_strv_free(&toks);
   ck_sb_free(&clean);
   return out.p;
}

static void normalize_params(const char *b, const char *e, struct params *out)
{
   struct ck_strv items = {0};
   const char *cur;
   const char *p;
   int depth = 0;

   out->v = NULL;
   out->n = 0;
   while (b < e && CK_IS_SPACE(*b))
      b++;
   while (e > b && CK_IS_SPACE(e[-1]))
      e--;
   if (b == e || (e - b == 4 && !memcmp(b, "void", 4)))
      return;
   /* top-level commas; <> count as brackets */
   for (cur = p = b; p < e; p++)
   {
      char ch = *p;
      if (ch == '(' || ch == '[' || ch == '{' || ch == '<')
         depth++;
      else if (ch == ')' || ch == ']' || ch == '}' || ch == '>')
         depth--;
      if (ch == ',' && depth == 0)
      {
         ck_strv_push(&items, normalize_param(cur, p));
         cur = p + 1;
      }
   }
   ck_strv_push(&items, normalize_param(cur, e));
   out->v = items.v;
   out->n = items.n;
}

/* A parameter line that is a preprocessor line: ^\s*# (multiline). */
static int has_directive(const char *b, const char *e)
{
   const char *p = b;
   while (p < e)
   {
      while (p < e && CK_IS_SPACE(*p))
         p++;
      if (p < e && *p == '#')
         return 1;
      while (p < e && *p != '\n')
         p++;
   }
   return 0;
}

static int has_ellipsis(const char *b, const char *e)
{
   for (; e - b >= 3; b++)
   {
      if (b[0] == '.' && b[1] == '.' && b[2] == '.')
         return 1;
   }
   return 0;
}

/* The next prototype or definition in [*pos, hi):
 *
 *   (?<![\w.>:])((?:(?:static|...|[A-Za-z_]\w*)\s+)+)\**\s*(\w+)\s*\(
 *
 * One or more words, each with space after it, then the name and its
 * parenthesis; the name is what follows the last such word and any
 * stars, or that last word itself where the parenthesis follows it.
 * Returns 0 at the end. Fills sig only when the match is a signature
 * (sig->kind set), and always moves *pos past the match. */
static int next_sig(struct text *tx, size_t *pos, size_t hi, struct sig *sig)
{
   const char *t = tx->t;
   size_t p      = *pos;

   sig->kind = 0;
   while (p < hi)
   {
      size_t first, second = 0, last = 0, last_end = 0, chain_end;
      size_t name, name_end, start, o, q;
      size_t words = 0;
      long c;

      if (!CK_IS_WORD(t[p]))
      {
         p++;
         continue;
      }
      first = chain_end = p;
      /* the chain of words that have space after them */
      for (;;)
      {
         size_t w = chain_end;
         size_t s;
         if (w >= hi || !CK_IS_IDENT0(t[w]))
            break;
         while (w < hi && CK_IS_WORD(t[w]))
            w++;
         for (s = w; s < hi && CK_IS_SPACE(t[s]); s++);
         if (s == w)
            break;
         if (words == 1)
            second = chain_end;
         last      = chain_end;
         last_end  = w;
         chain_end = s;
         words++;
      }
      if (!words)
      {
         while (p < hi && CK_IS_WORD(t[p]))
            p++;
         continue;
      }
      /* a word after . > : or inside a longer one starts nothing */
      if (first == 0 || !(CK_IS_WORD(t[first - 1]) || t[first - 1] == '.'
               || t[first - 1] == '>' || t[first - 1] == ':'))
         start = first;
      else if (words >= 2)
      {
         start = second;
         words--;
      }
      else
      {
         p = chain_end;
         continue;
      }
      q = chain_end;
      if (q < hi && t[q] == '(')
      {
         if (words < 2)
         {
            p = chain_end;
            continue;
         }
         name     = last;
         name_end = last_end;
         o        = q;
      }
      else
      {
         while (q < hi && t[q] == '*')
            q++;
         while (q < hi && CK_IS_SPACE(t[q]))
            q++;
         name = q;
         while (q < hi && CK_IS_WORD(t[q]))
            q++;
         name_end = q;
         while (q < hi && CK_IS_SPACE(t[q]))
            q++;
         if (name_end == name || q >= hi || t[q] != '(')
         {
            p = chain_end;
            continue;
         }
         o = q;
      }
      *pos = o + 1;
      if (in_list(keywords, COUNT(keywords), t + name, name_end - name))
         return 1;
      c = tx->close[o];
      if (c < 0 || (size_t)c > hi)
         return 1;
      /* a definition or a prototype, not a call: [\s\x01]*([{;]) */
      for (q = (size_t)c + 1; q < tx->len
            && (CK_IS_SPACE(t[q]) || t[q] == MASK); q++);
      if (q >= tx->len || (t[q] != '{' && t[q] != ';'))
         return 1;
      if (has_directive(t + o + 1, t + c) || has_ellipsis(t + o + 1, t + c))
         return 1;
      sig->name     = t + name;
      sig->name_len = name_end - name;
      sig->kind     = t[q];
      sig->line     = line_of(tx, t + start);
      normalize_params(t + o + 1, t + c, &sig->params);
      return 1;
   }
   *pos = hi;
   return 0;
}

struct state
{
   struct ck_strv paths;
   struct ck_set protos;
   const char *ext_a;
   const char *ext_b;
   unsigned long bad;
};

static int skip_dir(const char *name, const char *rel, void *ud)
{
   (void)rel;
   (void)ud;
   return in_list(skip_dirs, COUNT(skip_dirs), name, strlen(name));
}

static void add_source(const char *path, const char *rel, void *ud)
{
   struct state *st  = (struct state*)ud;
   const char *slash = strrchr(path, '/');
   size_t i;
   (void)rel;

   for (i = 0; i < COUNT(shader_exts); i++)
   {
      if (ck_ends_with(path, shader_exts[i]))
         return;
   }
   /* '_shaders' anywhere in the directory's path */
   if (slash)
   {
      const char *s;
      for (s = path; (s = strstr(s, "_shaders")) && s < slash; s++)
      {
         if (s + 8 <= slash)
            return;
      }
   }
   if (ck_ends_with(path, st->ext_a) || (st->ext_b
            && ck_ends_with(path, st->ext_b)))
      ck_strv_push(&st->paths, ck_strdup(path));
}

/* Files under the roots with one of two extensions, sorted, each once. */
static void sources(struct state *st, char **roots, int nroots,
      const char *ext_a, const char *ext_b)
{
   size_t i, n;
   int r;

   st->ext_a = ext_a;
   st->ext_b = ext_b;
   for (r = 0; r < nroots; r++)
   {
      if (ck_is_file(roots[r]))
      {
         if (ck_ends_with(roots[r], ext_a) || (ext_b
                  && ck_ends_with(roots[r], ext_b)))
            ck_strv_push(&st->paths, ck_strdup(roots[r]));
      }
      else
         ck_walk(roots[r], skip_dir, add_source, st);
   }
   ck_strv_sort(&st->paths);
   for (i = n = 0; i < st->paths.n; i++)
   {
      if (n && !strcmp(st->paths.v[i], st->paths.v[n - 1]))
         free(st->paths.v[i]);
      else
         st->paths.v[n++] = st->paths.v[i];
   }
   st->paths.n = n;
}

static int load(const char *path, struct text *tx)
{
   tx->t = ck_read_file(path, &tx->len);
   if (!tx->t)
      return 0;
   mask_source(tx->t, tx->len);
   match_parens(tx);
   tx->line_at = tx->t;
   tx->line    = 1;
   return 1;
}

static void unload(struct text *tx)
{
   free(tx->t);
   free(tx->close);
}

static void print_params(const struct params *p)
{
   size_t i;
   for (i = 0; i < p->n; i++)
      printf("%s%s", i ? ", " : "", p->v[i]);
}

/* Definitions in one region under C linkage against the prototypes. */
static void check_region(struct state *st, struct text *tx, const char *path,
      size_t lo, size_t hi)
{
   struct sig sig;
   size_t pos = lo;

   /* regions may overlap: lines are counted from the top again */
   tx->line_at = tx->t;
   tx->line    = 1;
   while (next_sig(tx, &pos, hi, &sig))
   {
      void **slot;
      if (!sig.kind)
         continue;
      if (sig.kind == '{'
            && (slot = ck_set_find(&st->protos, sig.name, sig.name_len)))
      {
         const struct proto_list *pl = (const struct proto_list*)*slot;
         size_t i;
         for (i = 0; i < pl->n; i++)
         {
            if (params_equal(&pl->v[i].params, &sig.params))
               continue;
            printf("%s:%lu: %.*s(", path, sig.line, (int)sig.name_len,
                  sig.name);
            print_params(&sig.params);
            printf(") is defined here but %s:%lu declares %.*s(",
                  pl->v[i].path, pl->v[i].line, (int)sig.name_len, sig.name);
            print_params(&pl->v[i].params);
            printf(")\n");
            st->bad++;
         }
      }
      params_free(&sig.params);
   }
}

static unsigned long check(char **roots, int nroots)
{
   struct state st;
   struct ck_strv headers;
   size_t i;

   memset(&st, 0, sizeof(st));
   sources(&st, roots, nroots, ".h", NULL);
   headers = st.paths;
   memset(&st.paths, 0, sizeof(st.paths));
   for (i = 0; i < headers.n; i++)
   {
      struct text tx;
      struct sig sig;
      size_t pos = 0;

      if (!load(headers.v[i], &tx))
         continue;
      while (next_sig(&tx, &pos, tx.len, &sig))
      {
         void **slot;
         struct proto_list *pl;
         if (!sig.kind)
            continue;
         if (sig.kind != ';')
         {
            params_free(&sig.params);
            continue;
         }
         slot = ck_set_add(&st.protos, sig.name, sig.name_len);
         pl   = (struct proto_list*)*slot;
         if (!pl)
         {
            pl    = (struct proto_list*)ck_alloc(sizeof(*pl));
            pl->v = NULL;
            pl->n = 0;
            *slot = pl;
         }
         pl->v = (struct proto*)ck_realloc(pl->v, (pl->n + 1) * sizeof(*pl->v));
         pl->v[pl->n].path   = headers.v[i];
         pl->v[pl->n].line   = sig.line;
         pl->v[pl->n].params = sig.params;
         pl->n++;
      }
      unload(&tx);
   }

   sources(&st, roots, nroots, ".cpp", ".mm");
   for (i = 0; i < st.paths.n; i++)
   {
      struct text tx;
      const char *t;
      size_t p;

      if (!load(st.paths.v[i], &tx))
         continue;
      t = tx.t;
      /* each extern "C" { ... } block, and the single declaration
       * after a bare extern "C":  extern\s+\x01{3}\s*({)? */
      for (p = 0; p + 6 <= tx.len; )
      {
         size_t q, lo, hi;
         if (memcmp(t + p, "extern", 6))
         {
            p++;
            continue;
         }
         for (q = p + 6; q < tx.len && CK_IS_SPACE(t[q]); q++);
         if (q == p + 6 || tx.len - q < 3 || t[q] != MASK
               || t[q + 1] != MASK || t[q + 2] != MASK)
         {
            p++;
            continue;
         }
         for (q += 3; q < tx.len && CK_IS_SPACE(t[q]); q++);
         if (q < tx.len && t[q] == '{')
         {
            int depth = 0;
            lo        = q + 1;
            for (hi = q; hi < tx.len; hi++)
            {
               if (t[hi] == '{')
                  depth++;
               else if (t[hi] == '}' && --depth == 0)
                  break;
            }
         }
         else
         {
            const char *semi, *brace;
            lo    = q;
            semi  = (const char*)memchr(t + lo, ';', tx.len - lo);
            brace = (const char*)memchr(t + lo, '{', tx.len - lo);
            if (!semi)
               hi = brace ? (size_t)(brace - t) + 1 : tx.len;
            else if (brace && brace < semi)
               hi = (size_t)(brace - t) + 1;
            else
               hi = (size_t)(semi - t) + 1;
            if (hi > tx.len)
               hi = tx.len;
         }
         check_region(&st, &tx, st.paths.v[i], lo, hi);
         p = lo;
      }
      unload(&tx);
   }
   printf("%lu extern \"C\" definition(s) disagree with their prototype\n",
         st.bad);
   ck_strv_free(&st.paths);
   return st.bad;
}

static int selftest(void)
{
   static const char hdr[] =
      "void win32_check_window(void *data,\n"
      "      bool *quit,\n"
      "      bool *resize, unsigned *dims);\n"
      "bool win32_get_client_rect(RECT* rect);\n"
      "void helper_no_proto_in_cpp(int x);\n";
   static const char head[] =
      "extern \"C\" {\n"
      "   void win32_check_window(void *data,\n";
   static const char good_dims[] =
      "         bool *quit, bool *resize, unsigned *dims)\n";
   static const char bad_dims[] =
      "         bool *quit, bool *resize, unsigned *width, unsigned *height)\n";
   static const char mid[] =
      "   { *dims = 0; }\n"
      "   bool win32_get_client_rect(";
   static const char tail[] =
      ") { return true; }\n"
      "   /* a C++ file's own helper: no prototype, not judged */\n"
      "   int cpp_only_thing(float a, float b) { return 0; }\n"
      "}\n";
   static const char *const labels[3] = {
      "matching definition",
      "parameter count drift (the 0b2613d Xbox crash)",
      "parameter type drift"
   };
   static const unsigned long want[3] = { 0, 1, 1 };
   int ok = 1;
   int i;

   for (i = 0; i < 3; i++)
   {
      char *d            = ck_tmpdir();
      struct ck_sb path  = {0};
      struct ck_sb body  = {0};
      unsigned long got;

      ck_sb_puts(&body, head);
      ck_sb_puts(&body, i == 1 ? bad_dims : good_dims);
      ck_sb_puts(&body, mid);
      ck_sb_puts(&body, i == 2 ? "unsigned *rect" : "RECT* rect");
      ck_sb_puts(&body, tail);
      ck_sb_puts(&path, d);
      ck_sb_puts(&path, "/x_common.h");
      ck_write_file(path.p, hdr);
      path.len = 0;
      ck_sb_puts(&path, d);
      ck_sb_puts(&path, "/x_main.cpp");
      ck_write_file(path.p, body.p);
      got = check(&d, 1);
      printf("%s: %s\n", got == want[i] ? "ok  " : "FAIL", labels[i]);
      ok &= got == want[i];
      ck_rmtree(d);
      free(d);
      ck_sb_free(&path);
      ck_sb_free(&body);
   }
   return ok ? 0 : 1;
}

int main(int argc, char **argv)
{
   static char dot[] = ".";
   char *here        = dot;

   if (argc > 1 && !strcmp(argv[1], "--selftest"))
      return selftest();
   if (argc > 1)
      return check(argv + 1, argc - 1) ? 1 : 0;
   return check(&here, 1) ? 1 : 0;
}
