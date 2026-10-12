/* A video driver must not delete overlay textures it was lent.
 *
 * video_overlay_interface_t::load_textures hands a driver the overlay
 * pack's own texture handles for one page. They stay the pack's: it
 * deletes them (input_overlay_release_textures), and a handle is usually
 * on several pages. A driver that frees them with its page - as it
 * rightly does for textures it made itself in load() - leaves the pack
 * holding dead handles, and every page that shared them draws as black
 * quads until the pack is reloaded. That was gl2 from df4b5a7e0b: it
 * set GL2_FLAG_OVERLAY_BORROWED and never looked at it, so an Android
 * rotation (a page switch) blacked the overlay out for the session,
 * while gl3, d3d11, d3d12 and vulkan, written the same week, were fine.
 *
 * Nothing in CI can run a GL driver, so this reads the source instead.
 * For every gfx/drivers file that implements load_textures it requires
 * the three things a correct driver does with its "borrowed" marker:
 *
 *   set      where load_textures takes the handles;
 *   tested   negated, in a condition - the guard around the delete;
 *   cleared  so that the next load() page, whose textures ARE the
 *            driver's, is deleted again rather than leaked.
 *
 * The marker is a flag bit (..._OVERLAY_BORROWED), an overlay struct's
 * own "borrowed", or a driver-level overlay_borrowed / overlays_borrowed
 * field.
 *
 * Usage: tools/check.sh overlay_texture_ownership
 * Exit status 0 when every driver passes, 1 otherwise. */

#include "check_common.h"

#define FLAG "OVERLAY_BORROWED"

/* Drivers whose ownership is not expressed as a marker, and why. */
static const char *const exempt[][2] = {
   { "metal.m", "textures are ARC object references; dropping the "
      "page's array releases a reference, never the texture" }
};

/* The marker is cleared by wiping the struct it lives in:
 * memset\s*\(\s*&vk->overlay\s*,\s*0\s*, */
#define CLEARED_BY_FILE "vulkan.c"

static int is_bound_word_end(const char *b, const char *e, const char *p)
{
   return p >= e || !CK_IS_WORD(*p) || (p > b && !CK_IS_WORD(p[-1]));
}

static const char *ws(const char *p, const char *e)
{
   while (p < e && CK_IS_SPACE(*p))
      p++;
   return p;
}

/* (?:\.|->)(?:overlays?_)?borrowed\b  at p; the end, or NULL */
static const char *field_at(const char *b, const char *p, const char *e)
{
   static const char *const mids[] = { "overlays_", "overlay_", "" };
   size_t k;
   if (p < e && *p == '.')
      p++;
   else if (e - p >= 2 && p[0] == '-' && p[1] == '>')
      p += 2;
   else
      return NULL;
   for (k = 0; k < 3; k++)
   {
      size_t n = strlen(mids[k]);
      const char *q = p;
      if ((size_t)(e - q) < n || memcmp(q, mids[k], n))
         continue;
      q += n;
      if (e - q >= 8 && !memcmp(q, "borrowed", 8)
            && is_bound_word_end(b, e, q + 8))
         return q + 8;
   }
   return NULL;
}

/* \w*OVERLAY_BORROWED\b  at p (\b before it is the caller's): the
 * word run from p ends with the flag */
static const char *flag_at(const char *p, const char *e)
{
   const char *r;
   for (r = p; r < e && CK_IS_WORD(*r); r++);
   if (r - p >= 16 && !memcmp(r - 16, FLAG, 16))
      return r;
   return NULL;
}

static int word_start(const char *b, const char *p)
{
   return p == b || !CK_IS_WORD(p[-1]);
}

static int any_marker(const char *b, const char *e)
{
   const char *p;
   for (p = b; p < e; p++)
   {
      if (CK_IS_WORD(*p) && word_start(b, p) && flag_at(p, e))
         return 1;
      if (field_at(b, p, e))
         return 1;
   }
   return 0;
}

/* FIELD\s*=\s*(true|false)\b */
static int field_assigned(const char *b, const char *e, const char *value)
{
   const char *p;
   size_t vl = strlen(value);
   for (p = b; p < e; p++)
   {
      const char *q = field_at(b, p, e);
      if (!q)
         continue;
      q = ws(q, e);
      if (q >= e || *q != '=')
         continue;
      q = ws(q + 1, e);
      if ((size_t)(e - q) >= vl && !memcmp(q, value, vl)
            && is_bound_word_end(b, e, q + vl))
         return 1;
   }
   return 0;
}

static int in_class(int c)
{
   return CK_IS_WORD(c) || c == '>' || c == '.' || c == '-';
}

static int is_set(const char *b, const char *e)
{
   const char *p;
   /* \|=\s*\w*OVERLAY_BORROWED\b */
   for (p = b; p + 1 < e; p++)
   {
      const char *q;
      if (p[0] != '|' || p[1] != '=')
         continue;
      q = ws(p + 2, e);
      if (flag_at(q, e))
         return 1;
   }
   return field_assigned(b, e, "true");
}

static int tested(const char *b, const char *e)
{
   const char *p;
   for (p = b; p < e; p++)
   {
      const char *q, *r, *s;
      if (*p != '!')
         continue;
      q = ws(p + 1, e);
      /* !\s*\(\s*[\w>.\-]+\s*&\s*\w*OVERLAY_BORROWED\s*\) */
      if (q < e && *q == '(')
      {
         r = ws(q + 1, e);
         for (s = r; s < e && in_class(*s); s++);
         if (s > r)
         {
            s = ws(s, e);
            if (s < e && *s == '&')
            {
               const char *f;
               s = ws(s + 1, e);
               if ((f = flag_at(s, e)))
               {
                  f = ws(f, e);
                  if (f < e && *f == ')')
                     return 1;
               }
            }
         }
      }
      /* !\s*[\w>.\-]+FIELD */
      for (s = q; s < e && in_class(*s); s++);
      for (r = s; r > q + 1; )
      {
         r--;
         if (field_at(b, r, e))
            return 1;
      }
   }
   return 0;
}

static int cleared(const char *name, const char *b, const char *e)
{
   const char *p;
   /* &=\s*~\s*\(?\s*\w*OVERLAY_BORROWED\b */
   for (p = b; p + 1 < e; p++)
   {
      const char *q;
      if (p[0] != '&' || p[1] != '=')
         continue;
      q = ws(p + 2, e);
      if (q >= e || *q != '~')
         continue;
      q = ws(q + 1, e);
      if (q < e && *q == '(')
         q++;
      q = ws(q, e);
      if (flag_at(q, e))
         return 1;
   }
   if (field_assigned(b, e, "false"))
      return 1;
   if (!strcmp(name, CLEARED_BY_FILE))
   {
      for (p = b; e - p >= 6; p++)
      {
         const char *q;
         if (memcmp(p, "memset", 6))
            continue;
         q = ws(p + 6, e);
         if (q >= e || *q != '(')
            continue;
         q = ws(q + 1, e);
         if (e - q < 12 || memcmp(q, "&vk->overlay", 12))
            continue;
         q = ws(q + 12, e);
         if (q >= e || *q != ',')
            continue;
         q = ws(q + 1, e);
         if (q >= e || *q != '0')
            continue;
         q = ws(q + 1, e);
         if (q < e && *q == ',')
            return 1;
      }
   }
   return 0;
}

/* /\*.*?\*\/ made one space each */
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
   t[o] = '\0';
   return o;
}

/* \b\w+_overlay_load_textures\s*\( */
static int has_load_textures(const char *b, const char *e)
{
   static const char sfx[] = "_overlay_load_textures";
   const size_t n = sizeof(sfx) - 1;
   const char *p;
   for (p = b; p < e; p++)
   {
      const char *r, *q;
      if (!CK_IS_WORD(*p) || !word_start(b, p))
         continue;
      for (r = p; r < e && CK_IS_WORD(*r); r++);
      if ((size_t)(r - p) > n && !memcmp(r - n, sfx, n))
      {
         q = ws(r, e);
         if (q < e && *q == '(')
            return 1;
      }
      p = r - 1;
   }
   return 0;
}

int main(int argc, char **argv)
{
   const char *root = getenv("RA_TREE_ROOT");
   struct ck_strv names = {0};
   struct ck_sb dir     = {0};
   unsigned long failed = 0, checked = 0;
   size_t i;

   (void)argv;
   if (argc > 1)
   {
      fprintf(stderr, "usage: overlay_texture_ownership\n");
      return 2;
   }
   if (!root || !*root)
      root = ".";
   ck_sb_puts(&dir, root);
   ck_sb_puts(&dir, "/gfx/drivers");
   if (!ck_list_dir(dir.p, &names))
   {
      fprintf(stderr, "overlay_texture_ownership: no gfx/drivers under %s\n",
            root);
      return 1;
   }
   ck_strv_sort(&names);
   for (i = 0; i < names.n; i++)
   {
      struct ck_sb path = {0};
      const char *name  = names.v[i];
      size_t len, k;
      char *src;
      int skip = 0;
      const char *why = NULL;

      if (!(ck_ends_with(name, ".c") || ck_ends_with(name, ".m")
               || ck_ends_with(name, ".cpp")))
         continue;
      ck_sb_puts(&path, dir.p);
      ck_sb_putc(&path, '/');
      ck_sb_puts(&path, name);
      src = ck_is_file(path.p) ? ck_read_file(path.p, &len) : NULL;
      ck_sb_free(&path);
      if (!src)
         continue;
      if (!has_load_textures(src, src + len))
      {
         free(src);
         continue;
      }
      for (k = 0; k < sizeof(exempt) / sizeof(exempt[0]); k++)
      {
         if (!strcmp(name, exempt[k][0]))
         {
            skip = 1;
            why  = exempt[k][1];
         }
      }
      if (skip)
      {
         printf("[skip] %s: %s\n", name, why);
         free(src);
         continue;
      }
      checked++;
      len = strip_comments(src, len);
      if (!any_marker(src, src + len))
      {
         failed++;
         printf("[FAIL] %s: implements load_textures but has no 'borrowed'"
               " marker: its free path cannot tell the pack's textures"
               " from its own\n", name);
      }
      else
      {
         int s = is_set(src, src + len);
         int t = tested(src, src + len);
         int c = cleared(name, src, src + len);
         if (!s)
            printf("[FAIL] %s: the marker is never set in load_textures\n",
                  name);
         if (!t)
            printf("[FAIL] %s: the marker is never tested: the free path"
                  " deletes lent textures along with its own\n", name);
         if (!c)
            printf("[FAIL] %s: the marker is never cleared: after a"
                  " load_textures page, a load() page's own textures would"
                  " leak\n", name);
         if (!s || !t || !c)
            failed++;
         else
            printf("[pass] %s\n", name);
      }
      free(src);
   }
   ck_strv_free(&names);
   ck_sb_free(&dir);
   if (!checked)
   {
      printf("[FAIL] no driver with load_textures found - the check is"
            " looking in the wrong place\n");
      return 1;
   }
   if (failed)
   {
      printf("FAIL overlay_texture_ownership (%lu of %lu drivers)\n",
            failed, checked);
      return 1;
   }
   printf("PASS overlay_texture_ownership (%lu drivers)\n", checked);
   return 0;
}
