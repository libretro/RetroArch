/* A load is asked what it should load.
 *
 * image_texture_load() and its siblings read the struct texture_image
 * they are handed: pix10 is the caller's request for a 10-bit decode,
 * read before the image is filled in. A struct declared on the stack and
 * filled field by field leaves that request as whatever the stack held,
 * and a 16-bit PNG then comes back as packed 10-bit words now and then -
 * taken for 8-bit texels by a caller that never asked, so a shader LUT
 * or a texture gets the wrong colours with nothing to show why.
 *
 * The rule: a local or static struct texture_image passed by address to
 * an image_texture_load* call has its request set first - cleared with
 * memset, initialised to zero at its declaration, or given a pix10 of
 * its own - between the declaration and the call, up to the end of the
 * enclosing function (a line opening with a closing brace). Pointers to
 * images filled elsewhere are not checked; neither is deps/.
 *
 * Usage: tools/check.sh texture_image_init_check [--selftest] */

#include "check_common.h"

static const char *ws(const char *p, const char *e)
{
   while (p < e && CK_IS_SPACE(*p))
      p++;
   return p;
}

static int word_start(const char *b, const char *p)
{
   return p == b || !CK_IS_WORD(p[-1]);
}

/* \bstruct\s+texture_image\s+([A-Za-z_]\w*)\s*(=\s*\{[^;]*\})?\s*;
 * - the first on the line. Returns 1 with the name for one declared
 * without an initialiser, 0 for none or one with. */
static int decl(const char *b, const char *e, const char **nb,
      const char **ne)
{
   const char *p;
   for (p = b; e - p >= 6; p++)
   {
      const char *q, *w, *we, *r;
      if (memcmp(p, "struct", 6) || !word_start(b, p))
         continue;
      q = ws(p + 6, e);
      if (q == p + 6 || e - q < 13 || memcmp(q, "texture_image", 13))
         continue;
      r = q + 13;
      q = ws(r, e);
      if (q == r || q >= e || !CK_IS_IDENT0(*q))
         continue;
      for (w = q; q < e && CK_IS_WORD(*q); q++);
      we = q;
      q  = ws(q, e);
      /* the initialiser: '=' '{' up to a '}' that only space separates
       * from the first ';' after it */
      if (q < e && *q == '=')
      {
         const char *o = ws(q + 1, e);
         if (o < e && *o == '{')
         {
            const char *c = (const char*)memchr(o, ';', (size_t)(e - o));
            if (c)
            {
               while (c > o + 1 && CK_IS_SPACE(c[-1]))
                  c--;
               if (c > o + 1 && c[-1] == '}')
                  return 0;
            }
         }
      }
      if (q < e && *q == ';')
      {
         *nb = w;
         *ne = we;
         return 1;
      }
   }
   return 0;
}

static int name_at(const char *p, const char *e, const char *nb, size_t nl)
{
   return (size_t)(e - p) >= nl && !memcmp(p, nb, nl)
      && (p + nl == e || !CK_IS_WORD(p[nl]));
}

/* \bmemset\s*\(\s*&\s*NAME\b  or  \bNAME\s*\.\s*pix10\s*= */
static int cleared(const char *b, const char *e, const char *nb, size_t nl)
{
   const char *p;
   for (p = b; p < e; p++)
   {
      const char *q;
      if (!word_start(b, p))
         continue;
      if (e - p >= 6 && !memcmp(p, "memset", 6))
      {
         q = ws(p + 6, e);
         if (q < e && *q == '(')
         {
            q = ws(q + 1, e);
            if (q < e && *q == '&')
            {
               q = ws(q + 1, e);
               if (name_at(q, e, nb, nl))
                  return 1;
            }
         }
      }
      if ((size_t)(e - p) >= nl && !memcmp(p, nb, nl))
      {
         q = ws(p + nl, e);
         if (q < e && *q == '.')
         {
            q = ws(q + 1, e);
            if (e - q >= 5 && !memcmp(q, "pix10", 5))
            {
               q = ws(q + 5, e);
               if (q < e && *q == '=')
                  return 1;
            }
         }
      }
   }
   return 0;
}

/* \bimage_texture_load\w*\s*\(\s*&\s*([A-Za-z_]\w*)\b - the first on
 * the line; whether its image is NAME, or -1 when there is none. */
static int load_of(const char *b, const char *e, const char *nb, size_t nl)
{
   const char *p;
   for (p = b; e - p >= 18; p++)
   {
      const char *q, *w;
      if (memcmp(p, "image_texture_load", 18) || !word_start(b, p))
         continue;
      for (q = p + 18; q < e && CK_IS_WORD(*q); q++);
      q = ws(q, e);
      if (q >= e || *q != '(')
         continue;
      q = ws(q + 1, e);
      if (q >= e || *q != '&')
         continue;
      q = ws(q + 1, e);
      if (q >= e || !CK_IS_IDENT0(*q))
         continue;
      for (w = q; q < e && CK_IS_WORD(*q); q++);
      return (size_t)(q - w) == nl && !memcmp(w, nb, nl);
   }
   return -1;
}

/* (line, name) of every load of an image whose request was never set */
static void problems(const char *rel, const char *text, struct ck_strv *out)
{
   const char **lb = NULL, **le = NULL;
   size_t n = 0, i, j;
   const char *p = text;

   for (;;)
   {
      const char *nl = strchr(p, '\n');
      lb = (const char**)ck_realloc(lb, (n + 1) * sizeof(char*));
      le = (const char**)ck_realloc(le, (n + 1) * sizeof(char*));
      lb[n] = p;
      le[n] = nl ? nl : p + strlen(p);
      n++;
      if (!nl)
         break;
      p = nl + 1;
   }
   for (i = 0; i < n; i++)
   {
      const char *nb, *ne;
      size_t nlen;
      if (!decl(lb[i], le[i], &nb, &ne))
         continue;
      nlen = (size_t)(ne - nb);
      for (j = i + 1; j < n; j++)
      {
         int l;
         if (lb[j] < le[j] && *lb[j] == '}')
            break;
         if (cleared(lb[j], le[j], nb, nlen))
            break;
         if ((l = load_of(lb[j], le[j], nb, nlen)) >= 0)
         {
            if (l)
            {
               struct ck_sb m = {0};
               char num[32];
               ck_sb_puts(&m, rel);
               sprintf(num, ":%lu: '", (unsigned long)(j + 1));
               ck_sb_puts(&m, num);
               ck_sb_add(&m, nb, nlen);
               ck_sb_puts(&m, "' reaches image_texture_load with its 10-bit"
                     " request unset (memset it, or set ");
               ck_sb_add(&m, nb, nlen);
               ck_sb_puts(&m, ".pix10)");
               ck_strv_push(out, m.p);
               break;
            }
         }
      }
   }
   free(lb);
   free(le);
}

static int skip_dir(const char *name, const char *rel, void *ud)
{
   (void)ud;
   (void)name;
   return !strcmp(rel, "deps") || !strcmp(rel, ".git");
}

static void scan_file(const char *path, const char *rel, void *ud)
{
   char *text;
   if (!(ck_ends_with(rel, ".c") || ck_ends_with(rel, ".m")
            || ck_ends_with(rel, ".cpp") || ck_ends_with(rel, ".cc")))
      return;
   if (!(text = ck_read_file(path, NULL)))
      return;
   if (strstr(text, "texture_image"))
      problems(rel, text, (struct ck_strv*)ud);
   free(text);
}

static int selftest(void)
{
   static const char *const good[][2] = {
      { "memset", "void f(void)\n{\n   struct texture_" "image img;\n"
         "   memset(&img, 0, sizeof(img));\n"
         "   image_texture_load(&img, path);\n}\n" },
      { "pix10", "void f(void)\n{\n   struct texture_" "image img;\n"
         "   img.pix10 = false;\n"
         "   image_texture_load_buffer(&img, t, b, n);\n}\n" },
      { "initialiser", "void f(void)\n{\n   struct texture_" "image img = {0};\n"
         "   image_texture_load(&img, path);\n}\n" },
      { "other function", "void f(void)\n{\n   struct texture_" "image img;\n}\n"
         "void g(struct texture_image *p)\n{\n"
         "   image_texture_load(p, path);\n}\n" }
   };
   static const char bad[] = "void f(void)\n{\n   struct texture_" "image img;\n"
      "   img.pixels = NULL;\n   img.supports_rgba = true;\n"
      "   image_texture_load(&img, path);\n}\n";
   struct ck_strv out = {0};
   size_t i;
   int ok = 1;

   problems("bad.c", bad, &out);
   if (!out.n)
   {
      printf("selftest: an image loaded with its request unset was not"
            " caught\n");
      ok = 0;
   }
   ck_strv_free(&out);
   for (i = 0; i < sizeof(good) / sizeof(good[0]); i++)
   {
      problems(good[i][0], good[i][1], &out);
      if (out.n)
      {
         printf("selftest: a cleared image (%s) was flagged\n", good[i][0]);
         ok = 0;
      }
      ck_strv_free(&out);
   }
   if (ok)
      printf("selftest: ok\n");
   return ok ? 0 : 1;
}

int main(int argc, char **argv)
{
   struct ck_strv found = {0};
   const char *root     = getenv("RA_TREE_ROOT");
   size_t i;
   int a;

   for (a = 1; a < argc; a++)
   {
      if (!strcmp(argv[a], "--selftest"))
         return selftest();
   }
   if (argc > 1)
   {
      fprintf(stderr, "usage: texture_image_init_check [--selftest]\n");
      return 2;
   }
   if (!root || !*root)
      root = ".";
   ck_walk(root, skip_dir, scan_file, &found);
   for (i = 0; i < found.n; i++)
      printf("%s\n", found.v[i]);
   if (found.n)
   {
      printf("%lu image(s) loaded with the 10-bit request unset\n",
            (unsigned long)found.n);
      return 1;
   }
   printf("texture_image requests: every load is asked\n");
   return 0;
}
