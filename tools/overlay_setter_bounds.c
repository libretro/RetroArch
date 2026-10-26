/* An overlay's per-image setters refuse an image the driver does not
 * hold.
 *
 * video_overlay_interface_t has three setters that take an image index:
 * set_alpha, vertex_geom and tex_geom. The frontend calls them whenever
 * the layout, the opacity or the page changes - not only straight after
 * a load() that worked. With no page loaded the driver's arrays are NULL;
 * after a page with fewer images, or a load that failed half way, the
 * index is off the end. A setter that writes anyway is a NULL write or a
 * write into whatever sits after the page's block.
 *
 * 7661ab06e7 bounded the GL geometry setters and missed set_alpha in all
 * three GL drivers; the D3D, Vulkan and Metal setters had no check at
 * all. No runner can run these drivers, so this reads the source: each
 * setter, or the one helper it maps its buffer through, must compare the
 * index against the page's count before it writes ("index >= count",
 * "image >= gl->overlays", "index < d3d->overlays_size").
 *
 * Covers every driver with an overlay interface (drivers[] below, and
 * metal.m); a driver in gfx/drivers that defines the setters and is not
 * in the table fails, so a new one is read from its first commit.
 *
 * Every driver in gfx/drivers is also read for the alpha set_alpha hands
 * it being packed into a byte unsaturated. The frontend's alpha is the
 * page opacity times a desc's alpha_mod, and packs set alpha_mod above 1
 * to brighten a pressed button: "mod * 0xFF" packed as a byte wraps
 * (1.4 comes out as 0x65, a pressed button drawn dimmer than a released
 * one), and cast straight to u8 it is undefined. The conversion goes
 * through VIDEO_ALPHA_BYTE(), which saturates.
 *
 * D3D10, D3D11 and D3D12 keep a page's sprites in a copy the setters
 * write and the draw uploads once, when something changed: a setter that
 * maps the sprite buffer is a map per image per call - a page load is
 * three of them per image - and writes a buffer the last frame may still
 * be drawing from. Their setters, and the helper they go through, must
 * not map.
 *
 * D3D8 and D3D9 (HLSL) give each overlay image a vertex buffer of its
 * own, filled by the image's draw: that draw locks the buffer only when
 * the quad differs from the one it last wrote (overlay_t::vert_sent), or
 * a page costs a lock per image per frame for quads that never change.
 *
 * Usage: tools/check.sh overlay_setter_bounds
 * Exit status 0 when every setter passes, 1 otherwise. */

#include "check_common.h"

#define COUNT(a) (sizeof(a) / sizeof((a)[0]))

static const char *const drivers[] = {
   "ctr_gfx.c", "d3d8.c", "d3d9cg.c", "d3d9hlsl.c", "d3d10.c", "d3d11.c",
   "d3d12.c", "dispmanx_gfx.c", "drm_gfx.c", "exynos_gfx.c", "gdi_gfx.c",
   "gekko_gfx.c", "gl1.c", "gl2.c", "gl3.c",
   "gx_gfx_libogc.c", "gx2_gfx.c", "gxm_gfx.c", "hub75_gfx.c", "rsx_gfx.c",
   "sdl2_gfx.c", "sdl3_gfx.c", "switch_nx_gfx.c", "vulkan.c"
};

static const char *const store_only[] = { "d3d10.c", "d3d11.c", "d3d12.c" };

/* the overlay draw of the drivers that lock once */
static const char *const lock_once[][2] = {
   { "d3d8.c",     "d3d8_overlay_render" },
   { "d3d9hlsl.c", "d3d9_hlsl_overlay_render" }
};

static const char *dir_path;

static int in_list(const char *const *l, size_t n, const char *s)
{
   size_t i;
   for (i = 0; i < n; i++)
   {
      if (!strcmp(l[i], s))
         return 1;
   }
   return 0;
}

static int word_start(const char *b, const char *p)
{
   return p == b || !CK_IS_WORD(p[-1]);
}

static const char *ws(const char *p, const char *e)
{
   while (p < e && CK_IS_SPACE(*p))
      p++;
   return p;
}

static int ends(const char *b, const char *e, const char *sfx)
{
   size_t n = strlen(sfx);
   return (size_t)(e - b) >= n && !memcmp(e - n, sfx, n);
}

/* /\*.*?\*\/ made one space each, or (keep_lines) blanked line for line */
static size_t strip_comments(char *t, size_t n, int keep_lines)
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
            if (keep_lines)
            {
               for (; i < j + 2; i++)
                  t[o++] = t[i] == '\n' ? '\n' : ' ';
            }
            else
            {
               t[o++] = ' ';
               i      = j + 2;
            }
            continue;
         }
      }
      t[o++] = t[i++];
   }
   t[o] = '\0';
   return o;
}

/* The brace-matched body of the definition whose '(' is at start, or
 * NULL when it is a declaration or a call. */
static int body_of(const char *code, const char *e, const char *start,
      const char **bb, const char **be)
{
   const char *i = start, *j, *k;
   int depth = 0;
   for (;;)
   {
      if (i >= e)
      {
         if (depth)
         {
            fprintf(stderr, "overlay_setter_bounds: a parenthesis does not"
                  " close\n");
            exit(1);
         }
         break;
      }
      if (*i == ')' && !depth)
         break;
      if (*i == '(')
         depth++;
      else if (*i == ')' && --depth == 0)
         break;
      i++;
   }
   for (j = i + 1; j < e && (*j == ' ' || *j == '\t' || *j == '\r'
            || *j == '\n'); j++);
   if (j >= e || *j != '{')
      return 0;
   depth = 0;
   for (k = j; k < e; k++)
   {
      if (*k == '{')
         depth++;
      else if (*k == '}' && --depth == 0)
      {
         *bb = j;
         *be = k + 1;
         return 1;
      }
   }
   (void)code;
   return 0;
}

struct fn
{
   char *name;
   const char *b;
   const char *e;
};

struct fns
{
   struct fn *v;
   size_t n;
};

/* SETTER: \b(\w+_overlay_(?:set_alpha|vertex_geom|tex_geom))\s*\(
 * HELPER: \b(\w+_overlay_sprite(?:_map)?|(?:\w+_)?overlay_at)\s*\( */
static int is_setter(const char *w, const char *r)
{
   return ((r - w > 18 && ends(w, r, "_overlay_set_alpha"))
         || (r - w > 20 && ends(w, r, "_overlay_vertex_geom"))
         || (r - w > 17 && ends(w, r, "_overlay_tex_geom")));
}

static int is_helper(const char *w, const char *r)
{
   return (r - w > 15 && ends(w, r, "_overlay_sprite"))
      || (r - w > 19 && ends(w, r, "_overlay_sprite_map"))
      || (r - w == 10 && !memcmp(w, "overlay_at", 10))
      || (r - w > 11 && ends(w, r, "_overlay_at"));
}

static void functions(const char *code, const char *e, int helpers,
      struct fns *out)
{
   const char *p;
   out->v = NULL;
   out->n = 0;
   for (p = code; p < e; )
   {
      const char *w, *r, *q, *bb, *be;
      size_t k;
      if (!CK_IS_WORD(*p) || !word_start(code, p))
      {
         p++;
         continue;
      }
      for (w = p; p < e && CK_IS_WORD(*p); p++);
      r = p;
      if (!(helpers ? is_helper(w, r) : is_setter(w, r)))
         continue;
      q = ws(r, e);
      if (q >= e || *q != '(')
         continue;
      p = q + 1;
      if (!body_of(code, e, q, &bb, &be))
         continue;
      for (k = 0; k < out->n; k++)
      {
         if (strlen(out->v[k].name) == (size_t)(r - w)
               && !memcmp(out->v[k].name, w, (size_t)(r - w)))
            break;
      }
      if (k == out->n)
      {
         out->v = (struct fn*)ck_realloc(out->v, (out->n + 1) * sizeof(*out->v));
         out->v[k].name = ck_strndup(w, (size_t)(r - w));
         out->n++;
      }
      out->v[k].b = bb;
      out->v[k].e = be;
   }
}

static void free_fns(struct fns *f)
{
   size_t i;
   for (i = 0; i < f->n; i++)
      free(f->v[i].name);
   free(f->v);
}

/* \b(?:index|image)\b\s*(?:>=|<)\s*[\w>.\-]+ */
static int bound(const char *b, const char *e)
{
   const char *p;
   for (p = b; e - p >= 5; p++)
   {
      const char *q;
      if (!word_start(b, p) || (memcmp(p, "index", 5) && memcmp(p, "image", 5))
            || (p + 5 < e && CK_IS_WORD(p[5])))
         continue;
      q = ws(p + 5, e);
      if (e - q >= 2 && q[0] == '>' && q[1] == '=')
         q += 2;
      else if (q < e && *q == '<')
         q++;
      else
         continue;
      q = ws(q, e);
      if (q < e && (CK_IS_WORD(*q) || *q == '>' || *q == '.' || *q == '-'))
         return 1;
   }
   return 0;
}

/* \bNAME\s*\( */
static int calls(const char *b, const char *e, const char *name)
{
   size_t n = strlen(name);
   const char *p;
   for (p = b; (size_t)(e - p) >= n; p++)
   {
      const char *q;
      if (memcmp(p, name, n) || !word_start(b, p))
         continue;
      q = ws(p + n, e);
      if (q < e && *q == '(')
         return 1;
   }
   return 0;
}

/* \b(?:D3D12Map|Map)\s*\( */
static int maps(const char *b, const char *e)
{
   const char *p;
   for (p = b; p < e; p++)
   {
      const char *r, *q;
      if (!CK_IS_WORD(*p) || !word_start(b, p))
         continue;
      for (r = p; r < e && CK_IS_WORD(*r); r++);
      if ((r - p == 8 && !memcmp(p, "D3D12Map", 8))
            || (r - p == 3 && !memcmp(p, "Map", 3)))
      {
         q = ws(r, e);
         if (q < e && *q == '(')
            return 1;
      }
      p = r - 1;
   }
   return 0;
}

static int body_is_blank(const char *b, const char *e)
{
   for (; b < e; b++)
   {
      if (!strchr("{} \t\r\n", *b) || !*b)
         return 0;
   }
   return 1;
}

static void problem(struct ck_strv *out, const char *a, const char *b,
      const char *c)
{
   struct ck_sb m = {0};
   ck_sb_puts(&m, a);
   ck_sb_puts(&m, b ? b : "");
   ck_sb_puts(&m, c ? c : "");
   ck_strv_push(out, m.p);
}

static int cmp_fn(const void *a, const void *b)
{
   return strcmp(((const struct fn*)a)->name, ((const struct fn*)b)->name);
}

static void check_lock_once(const char *fname, const char *code,
      const char *e, struct ck_strv *out)
{
   size_t n = strlen(fname);
   const char *p, *bb = NULL, *be = NULL, *guard;
   long nb = 0;
   int any_lock = 0, early = 0;

   /* \bNAME\s*\(  - each, with a body */
   for (p = code; (size_t)(e - p) >= n; p++)
   {
      const char *q, *b1, *e1;
      if (memcmp(p, fname, n) || !word_start(code, p))
         continue;
      q = ws(p + n, e);
      if (q >= e || *q != '(')
         continue;
      if (body_of(code, e, q, &b1, &e1))
      {
         nb++;
         bb = b1;
         be = e1;
      }
      p = q;
   }
   if (nb != 1)
   {
      char tmp[160];
      sprintf(tmp, "found %ld definitions of the overlay draw - the check is"
            " not reading this driver right", nb);
      ck_strv_push(out, ck_strdup(tmp));
      return;
   }
   {
      static const char g[] = "memcmp(overlay->vert_sent";
      const char *s;
      guard = NULL;
      for (s = bb; (size_t)(be - s) >= sizeof(g) - 1; s++)
      {
         if (!memcmp(s, g, sizeof(g) - 1))
         {
            guard = s;
            break;
         }
      }
   }
   /* (?:_Lock|vertex_buffer_lock)\s*\(  - where each starts */
   for (p = bb; p < be; p++)
   {
      const char *q = NULL;
      if (be - p >= 5 && !memcmp(p, "_Lock", 5))
         q = p + 5;
      else if (be - p >= 18 && !memcmp(p, "vertex_buffer_lock", 18))
         q = p + 18;
      if (!q)
         continue;
      q = ws(q, be);
      if (q >= be || *q != '(')
         continue;
      any_lock = 1;
      if (!guard || p < guard)
         early = 1;
      p = q;
   }
   if (!any_lock)
      ck_strv_push(out, ck_strdup("the overlay draw locks no vertex buffer"
            " - the check is not reading this driver right"));
   else if (early)
      ck_strv_push(out, ck_strdup("the overlay draw locks its vertex buffer"
            " without first comparing the quad with the one it last wrote"
            " (vert_sent)"));
}

static void check_c(const char *name, const char *code, const char *e,
      struct ck_strv *out)
{
   struct fns setters, helpers;
   size_t i, h;

   functions(code, e, 0, &setters);
   functions(code, e, 1, &helpers);
   if (setters.n < 3)
   {
      char tmp[120];
      sprintf(tmp, "found %lu of the 3 setters - the check is not reading"
            " this driver right", (unsigned long)setters.n);
      ck_strv_push(out, ck_strdup(tmp));
   }
   if (setters.n > 1)
      qsort(setters.v, setters.n, sizeof(*setters.v), cmp_fn);
   for (i = 0; i < setters.n; i++)
   {
      const struct fn *f = &setters.v[i];
      int ok = 0;
      /* a driver with nothing to set (a stub) writes nothing */
      if (body_is_blank(f->b, f->e) || bound(f->b, f->e))
         continue;
      for (h = 0; h < helpers.n && !ok; h++)
         ok = calls(f->b, f->e, helpers.v[h].name)
            && bound(helpers.v[h].b, helpers.v[h].e);
      if (!ok)
         problem(out, f->name, "() writes image [index] without comparing"
               " the index to the page's count", NULL);
   }
   if (in_list(store_only, COUNT(store_only), name))
   {
      for (i = 0; i < setters.n; i++)
      {
         const struct fn *f = &setters.v[i];
         int m = maps(f->b, f->e);
         for (h = 0; h < helpers.n && !m; h++)
            m = calls(f->b, f->e, helpers.v[h].name)
               && maps(helpers.v[h].b, helpers.v[h].e);
         if (m)
            problem(out, f->name, "() maps the sprite buffer: a setter writes"
                  " the page's copy, the draw uploads", NULL);
      }
   }
   for (i = 0; i < COUNT(lock_once); i++)
   {
      if (!strcmp(name, lock_once[i][0]))
         check_lock_once(lock_once[i][1], code, e, out);
   }
   free_fns(&setters);
   free_fns(&helpers);
}

/* Objective-C: every update goes through -_getForIndex:, which must
 * check the index against the buffer, and each caller must take NULL for
 * an answer. */
static void check_metal(const char *code, const char *e, struct ck_strv *out)
{
   static const char call[] = "[self _getForIndex:index];";
   const char *p, *mb = NULL, *me = NULL;
   unsigned long users = 0;
   int bad_user = 0;

   /* -\s*\(SpriteVertex\s*\*\)\s*_getForIndex:.*?\n\}  (re.S) */
   for (p = code; p < e && !mb; p++)
   {
      const char *q;
      if (*p != '-')
         continue;
      q = ws(p + 1, e);
      if (e - q < 13 || memcmp(q, "(SpriteVertex", 13))
         continue;
      q = ws(q + 13, e);
      if (e - q < 2 || memcmp(q, "*)", 2))
         continue;
      q = ws(q + 2, e);
      if (e - q < 13 || memcmp(q, "_getForIndex:", 13))
         continue;
      for (q += 13; e - q >= 2 && !(q[0] == '\n' && q[1] == '}'); q++);
      if (e - q < 2)
         continue;
      mb = p;
      me = q + 2;
   }
   {
      int has_len = 0;
      if (mb)
      {
         for (p = mb; me - p >= 12 && !has_len; p++)
            has_len = !memcmp(p, "_vert.length", 12);
      }
      if (!has_len)
         ck_strv_push(out, ck_strdup("-_getForIndex: does not check the index"
               " against the vertex buffer's length"));
   }
   /* \[self _getForIndex:index\];(.*?)\n\}  (re.S) */
   for (p = code; (size_t)(e - p) >= sizeof(call) - 1; )
   {
      const char *q, *u;
      if (memcmp(p, call, sizeof(call) - 1))
      {
         p++;
         continue;
      }
      u = p + sizeof(call) - 1;
      for (q = u; e - q >= 2 && !(q[0] == '\n' && q[1] == '}'); q++);
      if (e - q < 2)
      {
         p++;
         continue;
      }
      users++;
      {
         /* if\s*\(\s*!pv\s*\) */
         const char *s;
         int ok = 0;
         for (s = u; q - s >= 2 && !ok; s++)
         {
            const char *t;
            if (s[0] != 'i' || s[1] != 'f')
               continue;
            t = ws(s + 2, q);
            if (t >= q || *t != '(')
               continue;
            t = ws(t + 1, q);
            if (q - t < 3 || memcmp(t, "!pv", 3))
               continue;
            t = ws(t + 3, q);
            ok = t < q && *t == ')';
         }
         if (!ok)
            bad_user++;
      }
      p = q + 2;
   }
   if (users < 3)
   {
      char tmp[80];
      sprintf(tmp, "found %lu of the 3 callers of -_getForIndex:", users);
      ck_strv_push(out, ck_strdup(tmp));
   }
   while (bad_user--)
      ck_strv_push(out, ck_strdup("a caller of -_getForIndex: writes through"
            " its result without checking it for NULL"));
}

static int at_bound(const char *b, const char *e, const char *p)
{
   int before = p > b && CK_IS_WORD(p[-1]);
   int after  = p < e && CK_IS_WORD(*p);
   return before != after;
}

/* 0x[fF]{2}|255(?:\.0*f?)?  at p: the ends it can take, in the order
 * the expression tries them; returns how many */
static size_t number_ends(const char *p, const char *e, const char **ends_out)
{
   size_t n = 0;
   if (e - p >= 4 && p[0] == '0' && p[1] == 'x' && (p[2] | 0x20) == 'f'
         && (p[3] | 0x20) == 'f')
      ends_out[n++] = p + 4;
   else if (e - p >= 3 && !memcmp(p, "255", 3))
   {
      const char *q = p + 3;
      if (q < e && *q == '.')
      {
         const char *z;
         for (z = q + 1; z < e && *z == '0'; z++);
         for (; z >= q + 1 && n < 60; z--)
         {
            if (z < e && *z == 'f')
               ends_out[n++] = z + 1;
            ends_out[n++] = z;
         }
      }
      ends_out[n++] = q;
   }
   return n;
}

/* UNSATURATED:
 *   \b(?:mod|alpha_mod)\s*\*\s*(?:0x[fF]{2}|255(?:\.0*f?)?)\b
 *  |\b(?:0x[fF]{2}|255(?:\.0*f?)?)\s*\*\s*[\w>.\-\[\]]*\balpha_mod\b
 * at p; the end, or NULL. */
static const char *unsaturated_at(const char *b, const char *e, const char *p)
{
   const char *ends_[64];
   size_t n, i;
   if (!word_start(b, p))
      return NULL;
   {
      const char *q = NULL;
      if (e - p >= 3 && !memcmp(p, "mod", 3))
         q = p + 3;
      if (q)
      {
         const char *s = ws(q, e);
         if (s < e && *s == '*')
         {
            s = ws(s + 1, e);
            n = number_ends(s, e, ends_);
            for (i = 0; i < n; i++)
               if (at_bound(b, e, ends_[i]))
                  return ends_[i];
         }
      }
      q = NULL;
      if (e - p >= 9 && !memcmp(p, "alpha_mod", 9))
         q = p + 9;
      if (q)
      {
         const char *s = ws(q, e);
         if (s < e && *s == '*')
         {
            s = ws(s + 1, e);
            n = number_ends(s, e, ends_);
            for (i = 0; i < n; i++)
               if (at_bound(b, e, ends_[i]))
                  return ends_[i];
         }
      }
   }
   n = number_ends(p, e, ends_);
   for (i = 0; i < n; i++)
   {
      const char *s = ws(ends_[i], e), *r, *c;
      if (s >= e || *s != '*')
         continue;
      s = ws(s + 1, e);
      for (r = s; r < e && (CK_IS_WORD(*r) || *r == '>' || *r == '.'
               || *r == '-' || *r == '[' || *r == ']'); r++);
      for (c = r; c >= s; c--)
      {
         if (e - c >= 9 && !memcmp(c, "alpha_mod", 9)
               && at_bound(b, e, c) && at_bound(b, e, c + 9))
            return c + 9;
      }
   }
   return NULL;
}

static void check_alpha_bytes(struct ck_strv *out)
{
   struct ck_strv names = {0};
   size_t i;
   ck_list_dir(dir_path, &names);
   ck_strv_sort(&names);
   for (i = 0; i < names.n; i++)
   {
      struct ck_sb path = {0};
      const char *p, *e, *counted;
      unsigned long line = 1;
      size_t len;
      char *code;
      if (!(ck_ends_with(names.v[i], ".c") || ck_ends_with(names.v[i], ".m")))
         continue;
      ck_sb_puts(&path, dir_path);
      ck_sb_putc(&path, '/');
      ck_sb_puts(&path, names.v[i]);
      code = ck_is_file(path.p) ? ck_read_file(path.p, &len) : NULL;
      ck_sb_free(&path);
      if (!code)
         continue;
      len     = strip_comments(code, len, 1);
      e       = code + len;
      counted = code;
      for (p = code; p < e; )
      {
         const char *end = unsaturated_at(code, e, p);
         struct ck_sb m = {0};
         char num[32];
         if (!end)
         {
            p++;
            continue;
         }
         for (; counted < p; counted++)
            line += *counted == '\n';
         ck_sb_puts(&m, names.v[i]);
         sprintf(num, ":%lu: ", line);
         ck_sb_puts(&m, num);
         ck_sb_puts(&m, "overlay alpha packed as a byte without"
               " VIDEO_ALPHA_BYTE(): ");
         ck_sb_add(&m, p, (size_t)(end - p));
         ck_strv_push(out, m.p);
         p = end > p ? end : p + 1;
      }
      free(code);
   }
   ck_strv_free(&names);
}

static char *read_driver(const char *name, size_t *len)
{
   struct ck_sb path = {0};
   char *t;
   ck_sb_puts(&path, dir_path);
   ck_sb_putc(&path, '/');
   ck_sb_puts(&path, name);
   t = ck_is_file(path.p) ? ck_read_file(path.p, len) : NULL;
   ck_sb_free(&path);
   return t;
}

int main(int argc, char **argv)
{
   const char *root      = getenv("RA_TREE_ROOT");
   struct ck_strv names  = {0};
   struct ck_strv unsat  = {0};
   struct ck_sb dir      = {0};
   unsigned long failed  = 0, total = 0;
   size_t i, d;

   (void)argv;
   if (argc > 1)
   {
      fprintf(stderr, "usage: overlay_setter_bounds\n");
      return 2;
   }
   if (!root || !*root)
      root = ".";
   ck_sb_puts(&dir, root);
   ck_sb_puts(&dir, "/gfx/drivers");
   dir_path = dir.p;
   if (!ck_list_dir(dir_path, &names))
   {
      fprintf(stderr, "overlay_setter_bounds: no gfx/drivers under %s\n",
            root);
      return 1;
   }
   ck_strv_sort(&names);
   for (i = 0; i < names.n; i++)
   {
      struct fns s;
      size_t len;
      char *code;
      if (!ck_ends_with(names.v[i], ".c")
            || in_list(drivers, COUNT(drivers), names.v[i]))
         continue;
      if (!(code = read_driver(names.v[i], &len)))
         continue;
      len = strip_comments(code, len, 0);
      functions(code, code + len, 0, &s);
      if (s.n)
      {
         printf("[FAIL] %s: defines overlay setters but is not in"
               " drivers[]\n", names.v[i]);
         failed++;
      }
      free_fns(&s);
      free(code);
   }
   check_alpha_bytes(&unsat);
   for (i = 0; i < unsat.n; i++)
      printf("[FAIL] %s\n", unsat.v[i]);
   for (d = 0; d <= COUNT(drivers); d++)
   {
      const char *name = d < COUNT(drivers) ? drivers[d] : "metal.m";
      struct ck_strv problems = {0};
      size_t len;
      char *code = read_driver(name, &len);
      if (!code)
      {
         printf("[FAIL] %s: no such driver\n", name);
         failed++;
         continue;
      }
      len = strip_comments(code, len, 0);
      total++;
      if (d == COUNT(drivers))
         check_metal(code, code + len, &problems);
      else
         check_c(name, code, code + len, &problems);
      if (problems.n)
      {
         failed++;
         for (i = 0; i < problems.n; i++)
            printf("[FAIL] %s: %s\n", name, problems.v[i]);
      }
      else
         printf("[pass] %s\n", name);
      ck_strv_free(&problems);
      free(code);
   }
   if (failed || unsat.n)
   {
      printf("FAIL overlay_setter_bounds (%lu of %lu drivers, %lu unsaturated"
            " alpha bytes)\n", failed, total, (unsigned long)unsat.n);
      return 1;
   }
   printf("PASS overlay_setter_bounds (%lu drivers)\n", total);
   return 0;
}
