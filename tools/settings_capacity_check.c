/* Checks that every settings list in configuration.c has room for what
 * it is given.
 *
 * configuration.c builds its lists of settings - booleans, integers,
 * strings, paths and so on - in populate_settings_*() functions, each
 * into an array of a fixed size, SETTINGS_*_COUNT_MAX. Adding a setting
 * is a line in one of those functions, or a row in a table one of them
 * includes; nothing made anyone raise the size, and one setting too many
 * wrote past the end of the array. That happened: two per-port string
 * settings (the keyboard and the mouse a port is pinned to, sixteen of
 * each) overflowed the string list, and RetroArch died of heap
 * corruption at startup.
 *
 * The lists now refuse what they have no room for, and say so. This
 * finds it before anything is run. configuration.c is preprocessed, as
 * tools/settings_check.py does it and for the same build profiles, so
 * that every table and macro is expanded; then, for each list, the
 * settings its function adds are counted - one for each place a
 * setting's name is stored, times the loop's count where that is inside
 * a loop over the users - and compared with the size the list is
 * allocated with.
 *
 * Usage: tools/check.sh settings_capacity_check
 * Exits 1 when a list is given more than it holds. Needs gcc (or
 * $SETTINGS_CC) on PATH for the preprocessing. */

#include "check_common.h"

static const char *const kinds[] = {
   "array", "path", "bool", "float", "uint", "size", "int"
};

/* Keep in step with COMMON, PROFILES and INCLUDES in
 * tools/settings_check.py. */
static const char *const common[] = {
   "RARCH_INTERNAL", "HAVE_MENU", "HAVE_XMB", "HAVE_OZONE", "HAVE_RGUI",
   "HAVE_MATERIALUI", "HAVE_NETWORKING", "HAVE_NETPLAY", "HAVE_NETWORK_CMD",
   "HAVE_NETWORKGAMEPAD", "HAVE_COMMAND", "HAVE_CHEEVOS", "HAVE_OVERLAY",
   "HAVE_TRANSLATE", "HAVE_LANGEXTRA", "HAVE_MICROPHONE", "HAVE_CLOUDSYNC",
   "HAVE_SMBCLIENT", "HAVE_IMAGEVIEWER", "HAVE_CRTSWITCHRES",
   "HAVE_ACCESSIBILITY", "HAVE_GFX_WIDGETS", "HAVE_THREADS",
   "HAVE_AUDIOMIXER", "HAVE_REWIND", "HAVE_RUNAHEAD", "HAVE_SCREENSHOTS",
   "HAVE_BLUETOOTH", "HAVE_WIFI", "HAVE_BSV_MOVIE", "HAVE_CONFIGFILE",
   "HAVE_PATCH", "HAVE_CHEATS", "HAVE_ZLIB", "HAVE_COMPRESSION",
   "HAVE_UPDATE_CORES", "HAVE_LIBRETRODB", "HAVE_DYNAMIC",
   "HAVE_SHADERPIPELINE", "HAVE_GLSL", "HAVE_SLANG", "HAVE_OPENGL",
   "HAVE_VIDEO_LAYOUT", "HAVE_STB_FONT", "HAVE_CC_RESAMPLER", NULL
};

/* sorted by name, the order they are reported in */
static const char *const profiles[][4] = {
   { "android", "ANDROID", "RARCH_MOBILE", "HAVE_VULKAN" },
   { "console", "RARCH_CONSOLE", NULL, NULL },
   { "desktop", "HAVE_VULKAN", "HAVE_QT", "HAVE_DISCORD" }
};

static const char *const includes[] = {
   ".", "libretro-common/include", "deps", "deps/stb",
   "deps/rcheevos/include", NULL
};

static const char *ws(const char *p, const char *e)
{
   while (p < e && CK_IS_SPACE(*p))
      p++;
   return p;
}

static const char *lit(const char *p, const char *e, const char *s)
{
   size_t n = strlen(s);
   return (size_t)(e - p) >= n && !memcmp(p, s, n) ? p + n : NULL;
}

/* tmp\s*\[\s*count\s*\]\s*\.\s*ident\s*=  at p; its end or NULL */
static const char *add_at(const char *p, const char *e)
{
   static const char *const parts[] = {
      "tmp", "[", "count", "]", ".", "ident", "="
   };
   size_t k;
   for (k = 0; k < 7; k++)
   {
      if (k)
         p = ws(p, e);
      if (!(p = lit(p, e, parts[k])))
         return NULL;
   }
   return p;
}

static unsigned long count_add(const char *p, const char *e)
{
   unsigned long n = 0;
   while (p < e)
   {
      const char *q = add_at(p, e);
      if (q)
      {
         n++;
         p = q;
      }
      else
         p++;
   }
   return n;
}

/* \bfor\s*\([^;]*;[^;<]*<\s*\(?\s*(\d+)\s*\)?\s*;[^)]*\)  at p, with b
 * the start of the text for \b; the match end, or NULL */
static const char *loop_at(const char *b, const char *p, const char *e,
      unsigned long *count)
{
   const char *q;
   if (p > b && CK_IS_WORD(p[-1]))
      return NULL;
   if (!(q = lit(p, e, "for")))
      return NULL;
   q = ws(q, e);
   if (q >= e || *q != '(')
      return NULL;
   for (q++; q < e && *q != ';'; q++);
   if (q >= e)
      return NULL;
   for (q++; q < e && *q != ';' && *q != '<'; q++);
   if (q >= e || *q != '<')
      return NULL;
   q = ws(q + 1, e);
   if (q < e && *q == '(')
   {
      const char *r = ws(q + 1, e);
      if (r < e && CK_IS_DIGIT(*r))
         q = r;
   }
   if (q >= e || !CK_IS_DIGIT(*q))
      return NULL;
   for (*count = 0; q < e && CK_IS_DIGIT(*q); q++)
      *count = *count * 10 + (unsigned long)(*q - '0');
   q = ws(q, e);
   if (q < e && *q == ')')
      q = ws(q + 1, e);
   if (q >= e || *q != ';')
      return NULL;
   for (q++; q < e && *q != ')'; q++);
   return q < e ? q + 1 : NULL;
}

static unsigned long count_adds(const char *b, const char *e)
{
   unsigned long total = 0;
   const char *pos = b;
   for (;;)
   {
      const char *ls = NULL, *le = NULL, *p, *i, *j, *ib, *ie;
      unsigned long n = 0;
      for (p = pos; p < e; p++)
      {
         if ((le = loop_at(b, p, e, &n)))
         {
            ls = p;
            break;
         }
      }
      total += count_add(pos, ls ? ls : e);
      if (!ls)
         return total;
      /* the loop's body: a block, or one statement */
      for (i = le; i < e && CK_IS_SPACE(*i); i++);
      if (i >= e)
         ck_die("settings_capacity_check: a loop with no body");
      if (*i == '{')
      {
         int depth = 0;
         for (j = i; ; j++)
         {
            if (j >= e)
               ck_die("settings_capacity_check: an unclosed loop body");
            if (*j == '{')
               depth++;
            else if (*j == '}' && !--depth)
               break;
         }
         ib  = i + 1;
         ie  = j;
         pos = j + 1;
      }
      else
      {
         for (j = i; j < e && *j != ';'; j++);
         if (j >= e)
            ck_die("settings_capacity_check: an unterminated loop body");
         ib  = i;
         ie  = j + 1;
         pos = j + 1;
      }
      total += n * count_adds(ib, ie);
   }
}

/* [0-9+*() ]+ evaluated as Python would: +, * and ** over integers,
 * with brackets */
static const char *ev_sum(const char *p, const char *e, unsigned long *v,
      int *ok);

static const char *ev_atom(const char *p, const char *e, unsigned long *v,
      int *ok)
{
   while (p < e && *p == ' ')
      p++;
   if (p < e && *p == '+') /* unary */
      return ev_atom(p + 1, e, v, ok);
   if (p < e && *p == '(')
   {
      p = ev_sum(p + 1, e, v, ok);
      while (p < e && *p == ' ')
         p++;
      if (p < e && *p == ')')
      {
         for (p++; p < e && *p == ' '; p++);
         return p;
      }
      *ok = 0;
      return p;
   }
   if (p >= e || !CK_IS_DIGIT(*p))
   {
      *ok = 0;
      return p;
   }
   for (*v = 0; p < e && CK_IS_DIGIT(*p); p++)
      *v = *v * 10 + (unsigned long)(*p - '0');
   while (p < e && *p == ' ')
      p++;
   return p;
}

/* atom ** power, right to left */
static const char *ev_pow(const char *p, const char *e, unsigned long *v,
      int *ok)
{
   p = ev_atom(p, e, v, ok);
   if (*ok && e - p >= 2 && p[0] == '*' && p[1] == '*')
   {
      unsigned long r = 0, b = *v;
      p = ev_pow(p + 2, e, &r, ok);
      for (*v = 1; r; r--)
         *v *= b;
   }
   return p;
}

static const char *ev_prod(const char *p, const char *e, unsigned long *v,
      int *ok)
{
   p = ev_pow(p, e, v, ok);
   while (*ok && p < e && *p == '*')
   {
      unsigned long r = 0;
      p   = ev_pow(p + 1, e, &r, ok);
      *v *= r;
   }
   return p;
}

static const char *ev_sum(const char *p, const char *e, unsigned long *v,
      int *ok)
{
   p = ev_prod(p, e, v, ok);
   while (*ok && p < e && *p == '+')
   {
      unsigned long r = 0;
      p   = ev_prod(p + 1, e, &r, ok);
      *v += r;
   }
   return p;
}

static char *find_in(const char *b, const char *e, const char *s)
{
   size_t n = strlen(s);
   const char *p;
   for (p = b; (size_t)(e - p) >= n; p++)
      if (!memcmp(p, s, n))
         return (char*)p;
   return NULL;
}

/* calloc\s*\(([^,]+),\s*sizeof\s*\(\s*struct\s+config_KIND_setting
 * \s*\)\s*\)  - the first; its size expression */
static int alloc_expr(const char *b, const char *e, const char *kind,
      const char **xb, const char **xe)
{
   const char *p;
   char want[64];
   sprintf(want, "config_%s_setting", kind);
   for (p = b; p < e; p++)
   {
      const char *q, *c, *r;
      if (!(q = lit(p, e, "calloc")))
         continue;
      q = ws(q, e);
      if (q >= e || *q != '(')
         continue;
      q++;
      for (c = q; c < e && *c != ','; c++);
      if (c == q || c >= e)
         continue;
      r = ws(c + 1, e);
      if (!(r = lit(r, e, "sizeof")))
         continue;
      r = ws(r, e);
      if (r >= e || *r != '(')
         continue;
      r = ws(r + 1, e);
      if (!(r = lit(r, e, "struct")))
         continue;
      if (r >= e || !CK_IS_SPACE(*r))
         continue;
      r = ws(r, e);
      if (!(r = lit(r, e, want)))
         continue;
      r = ws(r, e);
      if (r >= e || *r != ')')
         continue;
      r = ws(r + 1, e);
      if (r >= e || *r != ')')
         continue;
      *xb = q;
      *xe = c;
      return 1;
   }
   return 0;
}

/* The rows of the config_KIND_rows[] table the def files generate, less
 * the empty one it ends in; 0 when there is none */
static unsigned long table_rows(const char *b, const char *e,
      const char *kind)
{
   char want[64];
   const char *p;
   unsigned long n = 0;
   int depth = 0;
   sprintf(want, "config_%s_rows[] = {", kind);
   if (!(p = find_in(b, e, want)))
      return 0;
   for (p += strlen(want); p < e; p++)
   {
      if (*p == '{' && !depth++)
         n++;
      else if (*p == '}' && !depth--)
         break;
   }
   return n ? n - 1 : 0;
}

static char *preprocess(const char *root, int profile, size_t *len)
{
   struct ck_sb cmd = {0}, path = {0};
   const char *cc = getenv("SETTINGS_CC");
   char *text;
   size_t k;
   int st;

   ck_sb_puts(&cmd, cc && *cc ? cc : "gcc");
   ck_sb_puts(&cmd, " -std=gnu99");
   for (k = 0; common[k]; k++)
   {
      ck_sb_puts(&cmd, " -D");
      ck_sb_puts(&cmd, common[k]);
   }
   for (k = 1; k < 4 && profiles[profile][k]; k++)
   {
      ck_sb_puts(&cmd, " -D");
      ck_sb_puts(&cmd, profiles[profile][k]);
   }
   for (k = 0; includes[k]; k++)
   {
      path.len = 0;
      ck_sb_puts(&path, root);
      ck_sb_putc(&path, '/');
      ck_sb_puts(&path, includes[k]);
      ck_sb_puts(&cmd, " -I ");
      ck_sb_shq(&cmd, path.p);
   }
   path.len = 0;
   ck_sb_puts(&path, root);
   ck_sb_puts(&path, "/configuration.c");
   ck_sb_puts(&cmd, " -E -P ");
   ck_sb_shq(&cmd, path.p);
   text = ck_capture(cmd.p, len, &st);
   ck_sb_free(&cmd);
   ck_sb_free(&path);
   if (st != 0)
   {
      fprintf(stderr, "preprocessing configuration.c failed\n");
      exit(1);
   }
   return text;
}

int main(int argc, char **argv)
{
   const char *root = getenv("RA_TREE_ROOT");
   unsigned long bad = 0;
   int pr;

   (void)argv;
   if (argc > 1)
   {
      fprintf(stderr, "usage: settings_capacity_check\n");
      return 2;
   }
   if (!root || !*root)
      root = ".";
   for (pr = 0; pr < 3; pr++)
   {
      const char *name = profiles[pr][0];
      size_t len, k;
      char *text = preprocess(root, pr, &len);
      const char *te = text + len;

      for (k = 0; k < sizeof(kinds) / sizeof(kinds[0]); k++)
      {
         const char *kind = kinds[k];
         char head[64];
         const char *start, *end = NULL, *xb, *xe, *s;
         unsigned long cap = 0, need;
         int ok = 1;

         sprintf(head, "*populate_settings_%s(", kind);
         start = find_in(text, te, head);
         if (start)
            end = find_in(start, te, "*size = count;");
         if (!start || !end || !alloc_expr(start, end, kind, &xb, &xe))
         {
            printf("FAIL [%s] %s list: populate_settings_%s() or its"
                  " allocation not found\n", name, kind, kind);
            bad++;
            continue;
         }
         while (xb < xe && CK_IS_SPACE(*xb))
            xb++;
         while (xe > xb && CK_IS_SPACE(xe[-1]))
            xe--;
         for (s = xb; s < xe; s++)
            if (!(CK_IS_DIGIT(*s) || *s == '+' || *s == '*' || *s == '('
                     || *s == ')' || *s == ' '))
               ok = 0;
         if (ok && xb < xe)
         {
            s  = ev_sum(xb, xe, &cap, &ok);
            ok = ok && s == xe;
         }
         else
            ok = 0;
         if (!ok)
         {
            printf("FAIL [%s] %s list: cannot read its size: %.*s\n",
                  name, kind, (int)(xe - xb), xb);
            bad++;
            continue;
         }
         need = count_adds(start, end);
         sprintf(head, "config_%s_rows_add(", kind);
         if (find_in(start, end, head))
            need += table_rows(text, te, kind);
         if (need > cap)
         {
            char up[16];
            size_t u;
            for (u = 0; kind[u]; u++)
               up[u] = (char)(kind[u] - 'a' + 'A');
            up[u] = '\0';
            printf("FAIL [%s] %-5s list: given %lu settings, holds %lu:"
                  " raise SETTINGS_%s_COUNT_MAX in configuration.c\n",
                  name, kind, need, cap, up);
            bad++;
         }
         else
            printf("ok   [%s] %-5s list: given %3lu settings, holds %3lu\n",
                  name, kind, need, cap);
      }
      free(text);
   }
   if (bad)
   {
      printf("FAIL settings_capacity_check: %lu\n", bad);
      return 1;
   }
   printf("PASS settings_capacity_check\n");
   return 0;
}
