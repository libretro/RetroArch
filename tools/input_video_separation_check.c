/* Video and input code stay out of each other's state.
 *
 * The video drivers used to make the input driver, and some input
 * drivers held the video driver's window or reached into its state. The
 * two are being taken apart (the input layer plan, Phase 2): the
 * frontend starts the input driver, told only what kind of window there
 * is, and neither side reads the other's state.
 *
 * Three things are counted, textually, so that no machine has to compile
 * every platform's files:
 *
 * 1. gfx/ calls input_state_get_ptr().            Must be none.
 * 2. input/ calls video_state_get_ptr().          Must be none.
 * 3. gfx/ names an input driver or a joypad driver (input_x, input_udev,
 *    dinput_joypad, ...), as declared in input/input_driver.h.
 *                                                 Must be none.
 *
 * And a fourth: a video driver's init is handed no slot to put an input
 * driver in - input_driver_t ** appears nowhere under gfx/.
 *
 * All are at none. The two lists below are what was left to move while
 * that was being done, file by file with a count, and they are empty
 * now: a file that is not listed fails, so nothing goes back in.
 *
 * Usage:
 *    tools/check.sh input_video_separation_check [--root DIR] [--selftest]
 *
 * Exit status is 0 when the tree matches, 1 otherwise. */

#include "check_common.h"

struct allowance
{
   const char *rel;
   unsigned long n;
};

/* input/ files that still take the video state (2): none. The last was
 * the Android input driver, which handles the app's lifecycle commands;
 * it asks the video driver by name for what it needs
 * (video_context_surface_create() and its neighbours). */
static const struct allowance video_state_in_input[] = {
   { NULL, 0 }
};

/* gfx/ files that still name an input or a joypad driver (3): none.
 * Every video driver and context leaves the input driver to the
 * frontend, saying what kind of window it made
 * (input_driver_video_window()), and nothing goes back in here. */
static const struct allowance input_drivers_in_gfx[] = {
   { NULL, 0 }
};

static const struct allowance none[] = { { NULL, 0 } };

enum what
{
   W_CALL = 0,
   W_NAMES,
   W_SLOT
};

struct hit
{
   char *rel;
   unsigned long n;
};

struct count
{
   const char *root;
   const char *sub;
   int what;
   const char *symbol;
   const struct ck_set *names;
   struct hit *v;
   size_t n;
};

/* Comments name drivers freely; only code counts. A block comment
 * becomes one space, then a line comment does; an unclosed block
 * comment stays. */
static size_t strip_comments(char *t, size_t n)
{
   size_t i, o;
   for (i = o = 0; i < n; )
   {
      if (t[i] == '/' && i + 1 < n && t[i + 1] == '*')
      {
         size_t j;
         for (j = i + 2; j + 1 < n && !(t[j] == '*' && t[j + 1] == '/'); j++);
         if (j + 1 < n)
         {
            t[o++] = ' ';
            i      = j + 2;
            continue;
         }
      }
      t[o++] = t[i++];
   }
   n = o;
   for (i = o = 0; i < n; )
   {
      if (t[i] == '/' && i + 1 < n && t[i + 1] == '/')
      {
         while (i < n && t[i] != '\n')
            i++;
         t[o++] = ' ';
         continue;
      }
      t[o++] = t[i++];
   }
   t[o] = '\0';
   return o;
}

static const char *skip_space(const char *p, const char *e)
{
   while (p < e && CK_IS_SPACE(*p))
      p++;
   return p;
}

/* \bsymbol\s*\( */
static unsigned long count_call(const char *t, size_t n, const char *sym)
{
   const char *e    = t + n;
   const char *p    = t;
   size_t sl        = strlen(sym);
   unsigned long c  = 0;
   while (p < e)
   {
      if (!CK_IS_WORD(*p) || (p > t && CK_IS_WORD(p[-1])))
      {
         p++;
         continue;
      }
      if ((size_t)(e - p) >= sl && !memcmp(p, sym, sl))
      {
         const char *q = skip_space(p + sl, e);
         if (q < e && *q == '(')
         {
            c++;
            p = q + 1;
            continue;
         }
      }
      while (p < e && CK_IS_WORD(*p))
         p++;
   }
   return c;
}

/* \b(?:name|...)\b: whole words that are one of the names */
static unsigned long count_words(const char *t, size_t n,
      const struct ck_set *names)
{
   const char *e   = t + n;
   const char *p   = t;
   unsigned long c = 0;
   while (p < e)
   {
      const char *w;
      if (!CK_IS_WORD(*p))
      {
         p++;
         continue;
      }
      for (w = p; p < e && CK_IS_WORD(*p); p++);
      if (ck_set_find(names, w, (size_t)(p - w)))
         c++;
   }
   return c;
}

/* \binput_driver_t\s*\*\s*\* */
static unsigned long count_slot(const char *t, size_t n)
{
   static const char sym[] = "input_driver_t";
   const char *e   = t + n;
   const char *p   = t;
   unsigned long c = 0;
   while (p < e)
   {
      const char *q;
      if (!CK_IS_WORD(*p) || (p > t && CK_IS_WORD(p[-1])))
      {
         p++;
         continue;
      }
      if ((size_t)(e - p) >= sizeof(sym) - 1
            && !memcmp(p, sym, sizeof(sym) - 1))
      {
         q = skip_space(p + sizeof(sym) - 1, e);
         if (q < e && *q == '*')
         {
            q = skip_space(q + 1, e);
            if (q < e && *q == '*')
            {
               c++;
               p = q + 1;
               continue;
            }
         }
      }
      while (p < e && CK_IS_WORD(*p))
         p++;
   }
   return c;
}

static void count_file(const char *path, const char *rel, void *ud)
{
   struct count *ct = (struct count*)ud;
   const char *ext  = strrchr(rel, '.');
   unsigned long n  = 0;
   struct ck_sb full = {0};
   size_t len;
   char *text;

   (void)path;
   if (!ext || !(!strcmp(ext, ".c") || !strcmp(ext, ".h")
            || !strcmp(ext, ".m") || !strcmp(ext, ".mm")
            || !strcmp(ext, ".cpp")))
      return;
   if (!(text = ck_read_file(path, &len)))
      return;
   len = strip_comments(text, len);
   if (ct->what == W_CALL)
      n = count_call(text, len, ct->symbol);
   else if (ct->what == W_NAMES)
      n = count_words(text, len, ct->names);
   else
      n = count_slot(text, len);
   free(text);
   if (!n)
      return;
   ct->v = (struct hit*)ck_realloc(ct->v, (ct->n + 1) * sizeof(*ct->v));
   ck_sb_puts(&full, ct->sub);
   ck_sb_putc(&full, '/');
   ck_sb_puts(&full, rel);
   ct->v[ct->n].rel = full.p;
   ct->v[ct->n].n   = n;
   ct->n++;
}

static void count_in(const char *root, const char *sub, int what,
      const char *symbol, const struct ck_set *names, struct count *ct)
{
   struct ck_sb dir = {0};
   memset(ct, 0, sizeof(*ct));
   ct->root   = root;
   ct->sub    = sub;
   ct->what   = what;
   ct->symbol = symbol;
   ct->names  = names;
   ck_sb_puts(&dir, root);
   ck_sb_putc(&dir, '/');
   ck_sb_puts(&dir, sub);
   ck_walk(dir.p, NULL, count_file, ct);
   ck_sb_free(&dir);
}

static unsigned long hit_of(const struct count *ct, const char *rel)
{
   size_t i;
   for (i = 0; i < ct->n; i++)
   {
      if (!strcmp(ct->v[i].rel, rel))
         return ct->v[i].n;
   }
   return 0;
}

static unsigned long allowed_of(const struct allowance *a, const char *rel)
{
   for (; a->rel; a++)
   {
      if (!strcmp(a->rel, rel))
         return a->n;
   }
   return 0;
}

static void compare(const char *what, struct count *ct,
      const struct allowance *allowed, struct ck_strv *errors)
{
   struct ck_strv rels = {0};
   const struct allowance *a;
   size_t i;

   for (i = 0; i < ct->n; i++)
      ck_strv_push(&rels, ck_strdup(ct->v[i].rel));
   for (a = allowed; a->rel; a++)
      ck_strv_push(&rels, ck_strdup(a->rel));
   ck_strv_sort(&rels);
   for (i = 0; i < rels.n; i++)
   {
      unsigned long have, may;
      char tmp[128];
      struct ck_sb m = {0};
      if (i && !strcmp(rels.v[i], rels.v[i - 1]))
         continue;
      have = hit_of(ct, rels.v[i]);
      may  = allowed_of(allowed, rels.v[i]);
      if (have > may)
      {
         ck_sb_puts(&m, what);
         ck_sb_puts(&m, ": ");
         ck_sb_puts(&m, rels.v[i]);
         sprintf(tmp, " has %lu, ", have);
         ck_sb_puts(&m, tmp);
         if (may)
         {
            sprintf(tmp, "%lu listed", may);
            ck_sb_puts(&m, tmp);
         }
         else
            ck_sb_puts(&m, "and is not listed");
         ck_strv_push(errors, m.p);
      }
      else if (have < may)
      {
         ck_sb_puts(&m, what);
         ck_sb_puts(&m, ": ");
         ck_sb_puts(&m, rels.v[i]);
         sprintf(tmp, " has %lu but %lu are listed: bring the", have, may);
         ck_sb_puts(&m, tmp);
         ck_sb_puts(&m, " listing in tools/input_video_separation_check.c"
               " down to match");
         ck_strv_push(errors, m.p);
      }
   }
   ck_strv_free(&rels);
   for (i = 0; i < ct->n; i++)
      free(ct->v[i].rel);
   free(ct->v);
}

/* The input and joypad driver objects input/input_driver.h declares:
 * ^extern\s+input_(?:device_)?driver_t\s+([A-Za-z0-9_]+)\s*;  (re.M) */
static void driver_names(const char *root, struct ck_set *names)
{
   struct ck_sb path = {0};
   const char *p, *e;
   size_t len;
   char *text;

   ck_sb_puts(&path, root);
   ck_sb_puts(&path, "/input/input_driver.h");
   text = ck_read_file(path.p, &len);
   ck_sb_free(&path);
   if (!text)
   {
      fprintf(stderr, "input_video_separation_check: no input/input_driver.h"
            " under %s\n", root);
      exit(1);
   }
   e = text + len;
   for (p = text; p < e; )
   {
      const char *q = p, *w;
      const char *nl = (const char*)memchr(p, '\n', (size_t)(e - p));
      const char *next = nl ? nl + 1 : e;
      if ((size_t)(e - q) > 6 && !memcmp(q, "extern", 6))
      {
         const char *s = q + 6;
         q = skip_space(s, e);
         if (q > s && (size_t)(e - q) >= 6 && !memcmp(q, "input_", 6))
         {
            q += 6;
            if ((size_t)(e - q) >= 7 && !memcmp(q, "device_", 7))
               q += 7;
            if ((size_t)(e - q) >= 8 && !memcmp(q, "driver_t", 8))
            {
               s = q + 8;
               q = skip_space(s, e);
               for (w = q; q < e && CK_IS_WORD(*q); q++);
               if (q > s && q > w)
               {
                  const char *t = skip_space(q, e);
                  if (t < e && *t == ';')
                  {
                     void **slot = ck_set_add(names, w, (size_t)(q - w));
                     *slot = names;
                     /* the next match starts at a line start after it */
                     next = t + 1;
                     while (next < e && next[-1] != '\n')
                        next++;
                  }
               }
            }
         }
      }
      p = next;
   }
   free(text);
}

static int check(const char *root, const struct allowance *vs_in_input,
      const struct allowance *drivers_in_gfx, struct ck_strv *errors)
{
   struct ck_set names = {0};
   struct count ct;
   size_t i;

   driver_names(root, &names);
   if (names.n < 10)
   {
      char tmp[160];
      sprintf(tmp, "input/input_driver.h: only %lu driver objects found;"
            " the declarations this check reads have changed",
            (unsigned long)names.n);
      ck_strv_push(errors, ck_strdup(tmp));
   }
   count_in(root, "gfx", W_CALL, "input_state_get_ptr", NULL, &ct);
   compare("gfx/ calls input_state_get_ptr()", &ct, none, errors);
   count_in(root, "input", W_CALL, "video_state_get_ptr", NULL, &ct);
   compare("input/ calls video_state_get_ptr()", &ct, vs_in_input, errors);
   count_in(root, "gfx", W_NAMES, NULL, &names, &ct);
   compare("gfx/ names an input or joypad driver", &ct, drivers_in_gfx,
         errors);
   count_in(root, "gfx", W_SLOT, NULL, NULL, &ct);
   compare("gfx/ takes a slot for an input driver (input_driver_t **)",
         &ct, none, errors);
   for (i = 0; i < names.cap; i++)
      free(names.keys[i]);
   free(names.keys);
   free(names.vals);
   return (int)errors->n;
}

/* self-test */

static void put(const char *root, const char *rel, const char *text)
{
   struct ck_sb path = {0};
   ck_sb_puts(&path, root);
   ck_sb_putc(&path, '/');
   ck_sb_puts(&path, rel);
   ck_mkdirs_for(path.p);
   ck_write_file(path.p, text);
   ck_sb_free(&path);
}

static int any_has(const char *root, const struct allowance *vs,
      const struct allowance *dg, const char *needle)
{
   struct ck_strv errors = {0};
   size_t i;
   int found = 0;
   check(root, vs, dg, &errors);
   for (i = 0; i < errors.n; i++)
      found |= strstr(errors.v[i], needle) != NULL;
   ck_strv_free(&errors);
   return found;
}

static int selftest(void)
{
   static const struct allowance vs[] = {
      { "input/drivers/a_input.c", 1 }, { NULL, 0 } };
   static const struct allowance dg[] = {
      { "gfx/drivers/listed_gfx.c", 2 }, { NULL, 0 } };
   struct ck_strv failures = {0};
   struct ck_sb decl       = {0};
   char *tmp               = ck_tmpdir();
   size_t i;
   int n;

   for (n = 0; n < 10; n++)
   {
      char line[64];
      sprintf(line, "extern input_driver_t input_n%d;\n", n);
      ck_sb_puts(&decl, line);
   }
   ck_sb_puts(&decl, "extern input_driver_t input_x;\n");
   ck_sb_puts(&decl, "extern input_device_driver_t dinput_joypad;\n");
   put(tmp, "input/input_driver.h", decl.p);
   put(tmp, "gfx/drivers/clean_gfx.c",
         "/* input_x is only named here, in a comment */\nint a;\n");
   put(tmp, "gfx/drivers/listed_gfx.c",
         "void f(void) { g(&input_x); h(&dinput_joypad); }\n");
   put(tmp, "input/drivers/a_input.c",
         "void f(void) { video_state_get_ptr(); }\n");
   {
      struct ck_strv errors = {0};
      if (check(tmp, vs, dg, &errors))
      {
         struct ck_sb m = {0};
         ck_sb_puts(&m, "a tree that matches its listing fails: [");
         for (i = 0; i < errors.n; i++)
         {
            ck_sb_puts(&m, i ? ", '" : "'");
            ck_sb_puts(&m, errors.v[i]);
            ck_sb_putc(&m, '\'');
         }
         ck_sb_putc(&m, ']');
         ck_strv_push(&failures, m.p);
      }
      ck_strv_free(&errors);
   }

   put(tmp, "gfx/drivers/new_gfx.c", "void f(void) { g(&input_x); }\n");
   if (!any_has(tmp, vs, dg, "new_gfx.c"))
      ck_strv_push(&failures,
            ck_strdup("a new file naming an input driver passes"));
   {
      struct ck_sb p = {0};
      ck_sb_puts(&p, tmp);
      ck_sb_puts(&p, "/gfx/drivers/new_gfx.c");
      remove(p.p);
      ck_sb_free(&p);
   }

   put(tmp, "gfx/drivers/listed_gfx.c", "void f(void) { g(&input_x); }\n");
   if (!any_has(tmp, vs, dg, "bring the listing"))
      ck_strv_push(&failures, ck_strdup("a listing higher than the code passes"));
   put(tmp, "gfx/drivers/listed_gfx.c",
         "void f(void) { g(&input_x); h(&dinput_joypad); }\n");

   put(tmp, "gfx/drivers/clean_gfx.c",
         "void f(void) { input_state_get_ptr(); }\n");
   if (!any_has(tmp, vs, dg, "input_state_get_ptr"))
      ck_strv_push(&failures,
            ck_strdup("gfx/ calling input_state_get_ptr() passes"));
   put(tmp, "gfx/drivers/clean_gfx.c", "int a;\n");

   put(tmp, "input/drivers/b_input.c",
         "void f(void) { video_state_get_ptr(); }\n");
   if (!any_has(tmp, vs, dg, "b_input.c"))
      ck_strv_push(&failures,
            ck_strdup("a new input file taking the video state passes"));

   put(tmp, "gfx/drivers/clean_gfx.c",
         "void *f(const video_info_t *v, input_driver_t **i, void **d);\n");
   if (!any_has(tmp, vs, dg, "slot"))
      ck_strv_push(&failures,
            ck_strdup("a video init taking an input driver slot passes"));
   put(tmp, "gfx/drivers/clean_gfx.c", "int a;\n");

   ck_rmtree(tmp);
   free(tmp);
   ck_sb_free(&decl);
   for (i = 0; i < failures.n; i++)
      printf("SELFTEST FAIL: %s\n", failures.v[i]);
   if (!failures.n)
      printf("SELFTEST_OK\n");
   n = failures.n ? 1 : 0;
   ck_strv_free(&failures);
   return n;
}

int main(int argc, char **argv)
{
   struct ck_strv errors = {0};
   const char *root      = getenv("RA_TREE_ROOT");
   unsigned long left_calls = 0, left_files = 0;
   const struct allowance *a;
   size_t i;
   int k;

   if (!root || !*root)
      root = ".";
   for (k = 1; k < argc; k++)
   {
      if (!strcmp(argv[k], "--selftest"))
         return selftest();
   }
   for (k = 1; k < argc; k++)
   {
      if (!strcmp(argv[k], "--root") && k + 1 < argc)
         root = argv[++k];
      else
      {
         fprintf(stderr, "usage: input_video_separation_check [--root DIR]"
               " [--selftest]\n");
         return 2;
      }
   }
   check(root, video_state_in_input, input_drivers_in_gfx, &errors);
   for (i = 0; i < errors.n; i++)
      printf("FAIL %s\n", errors.v[i]);
   if (errors.n)
   {
      printf("FAIL input_video_separation_check: %lu\n",
            (unsigned long)errors.n);
      return 1;
   }
   for (a = video_state_in_input; a->rel; a++)
      left_calls += a->n;
   for (a = input_drivers_in_gfx; a->rel; a++)
      left_files++;
   if (!left_calls && !left_files)
      printf("PASS input_video_separation_check (gfx/ and input/ stay out"
            " of each other's state, and gfx/ names no input driver)\n");
   else
      printf("PASS input_video_separation_check (still listed: %lu call(s)"
            " into the video state from input/, %lu gfx/ file(s) naming an"
            " input driver)\n", left_calls, left_files);
   return 0;
}
