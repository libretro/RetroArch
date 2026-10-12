/* The video thread reads its snapshot.
 *
 * The main thread builds a video_frame_info_t and hands it across the
 * thread boundary; the video driver works from that. A driver function
 * that reaches back into config_get_ptr() instead is reading settings
 * the main thread may be writing at that moment - which is a data race,
 * and TSan found one in the null driver's present_last() rather than
 * anyone noticing it by reading.
 *
 * This looks for the class instead of waiting for the next one. It seeds
 * from the entry points the video worker calls - frame(), present_last()
 * and their neighbours - walks the calls each one makes inside its own
 * translation unit, and reports live-state access anywhere it reaches.
 *
 * What it cannot see: a call through a function pointer, and a callee in
 * another file. So a clean run means "no path this can follow", not "no
 * path exists". It is a floor, not a proof, and it is still worth having
 * - every site it names is a real one.
 *
 * Usage: tools/check.sh video_thread_state_check */

#include "check_common.h"

#define COUNT(a) (sizeof(a) / sizeof((a)[0]))

/* The frontend state a driver must not read from the video thread: the
 * settings the main thread writes, and the video singleton whose fields
 * it updates around a frame. */
static const char *const live[] = {
   "config_get_ptr", "video_state_get_ptr", "runloop_state_get_ptr"
};

/* Entry points the video worker runs: a driver names its own functions
 * after them by convention (gl2_gfx_frame, d3d11_gfx_present_last), and
 * the wrapper calls them with nothing but the snapshot - names that end
 * in _frame or _present_last.
 *
 * Only the asynchronous path. A poke the wrapper marshals -
 * set_menu_texture_frame, a readback, a viewport query - runs on the
 * video thread while the main thread waits inside the command, so it
 * reads settings nobody is writing. frame() and present_last() are the
 * two the video thread runs on its own. set_texture_frame and
 * set_menu_texture_frame end in "frame" and are marshalled pokes, not
 * the asynchronous path; video_driver_frame and video_driver_cached_frame
 * are the frontend side, which runs on the main thread and is where the
 * snapshot comes from: names with set_..texture_frame in them, or that
 * start with video_driver_, are no seeds. */

/* The drivers are the ones behind the wrapper; the wrapper itself and
 * the frontend-side files run on the main thread by definition. */
#define ROOT_DIR "gfx/drivers"
static const char *const extra[] = { "gfx/video_driver.c" };

/* Reached from a seed by name, and safe: build_info is the snapshot's
 * producer, which the main thread calls before the hand-off - reading
 * the settings is its whole job. */
static const char *const exempt[] = { "video_driver_build_info" };

/* Known, with the reason. A driver for a platform that has no video
 * thread cannot race its own frame(), so its reads are latent rather
 * than live: they become findings the moment that platform runs
 * threaded, and the entry is what has to be removed then. gl1's is a
 * null check - it takes the pointer to test it and reads the paper
 * white from its own state - which this cannot see. */
static const char *const allowed[][2] = {
   { "gfx/drivers/ctr_gfx.c",    "3DS: no video thread" },
   { "gfx/drivers/d3d8.c",       "the D3D8 path predates threaded video" },
   { "gfx/drivers/d3d9cg.c",     "the D3D9 Cg path predates threaded video" },
   { "gfx/drivers/exynos_gfx.c", "Exynos: single-threaded KMS frontend" },
   { "gfx/drivers/omap_gfx.c",   "OMAP: single-threaded KMS frontend" },
   { "gfx/drivers/ps2_gfx.c",    "PS2: no video thread" },
   { "gfx/drivers/gl1.c",        "null check, not a read of the settings" }
};

static int is_seed(const char *f)
{
   size_t n = strlen(f);
   const char *s;
   if (     !(n >= 6 && !strcmp(f + n - 6, "_frame"))
         && !(n >= 13 && !strcmp(f + n - 13, "_present_last")))
      return 0;
   if (!strncmp(f, "video_driver_", 13))
      return 0;
   /* set_[a-z_]*texture_frame */
   for (s = f; (s = strstr(s, "set_")); s++)
   {
      const char *q;
      for (q = s + 4; ; q++)
      {
         if (!strncmp(q, "texture_frame", 13))
            return 0;
         if (!((*q >= 'a' && *q <= 'z') || *q == '_'))
            break;
      }
   }
   return 1;
}

struct func
{
   char *name;
   char *body;
};

struct funcs
{
   struct func *v;
   size_t n;
   struct ck_set index;
};

static long count_char(const char *b, const char *e, char c)
{
   long n = 0;
   for (; b < e; b++)
      n += *b == c;
   return n;
}

/* ^(?:static\s+)?(?:INLINE\s+|inline\s+)?[A-Za-z_][\w \*]*?\b([a-z_]\w*)\s*\(
 * on one line; the name, or 0. */
static int def_name(const char *b, const char *e, const char **nb,
      const char **ne)
{
   int st, il;
   for (st = 1; st >= 0; st--)
   {
      const char *p = b;
      if (st)
      {
         const char *q;
         if (e - p < 6 || memcmp(p, "static", 6))
            continue;
         for (q = p + 6; q < e && CK_IS_SPACE(*q); q++);
         if (q == p + 6)
            continue;
         p = q;
      }
      for (il = 2; il >= 0; il--)
      {
         const char *q = p, *r;
         if (il)
         {
            const char *kw = il == 2 ? "INLINE" : "inline";
            if (e - q < 6 || memcmp(q, kw, 6))
               continue;
            for (r = q + 6; r < e && CK_IS_SPACE(*r); r++);
            if (r == q + 6)
               continue;
            q = r;
         }
         if (q >= e || !CK_IS_IDENT0(*q))
            continue;
         for (r = q + 1; r < e; r++)
         {
            if (((*r >= 'a' && *r <= 'z') || *r == '_') && !CK_IS_WORD(r[-1]))
            {
               const char *w, *s;
               for (w = r; w < e && CK_IS_WORD(*w); w++);
               for (s = w; s < e && CK_IS_SPACE(*s); s++);
               if (s < e && *s == '(')
               {
                  *nb = r;
                  *ne = w;
                  return 1;
               }
            }
            if (!(CK_IS_WORD(*r) || *r == ' ' || *r == '*'))
               break;
         }
      }
   }
   return 0;
}

/* Every function defined in text, as name -> body. A signature may run
 * over several lines (gl2_frame does), so the name is taken from the
 * line that opens the parameter list and the body from the brace that
 * follows the line closing it. A later definition of a name replaces
 * the earlier one. */
static void functions(char *text, struct funcs *out)
{
   const char **lb = NULL, **le = NULL;
   size_t n = 0, i;
   char *p = text;

   out->v = NULL;
   out->n = 0;
   memset(&out->index, 0, sizeof(out->index));
   for (;;)
   {
      char *nl = strchr(p, '\n');
      lb = (const char**)ck_realloc(lb, (n + 1) * sizeof(char*));
      le = (const char**)ck_realloc(le, (n + 1) * sizeof(char*));
      lb[n] = p;
      le[n] = nl ? nl : p + strlen(p);
      n++;
      if (!nl)
         break;
      p = nl + 1;
   }
   i = 0;
   while (i < n)
   {
      const char *nb, *ne, *tb, *te;
      long depth = 0;
      size_t j = i, k, nbody = 0;
      if (!def_name(lb[i], le[i], &nb, &ne))
      {
         i++;
         continue;
      }
      /* walk to the line that closes the parameter list */
      while (j < n && j < i + 12)
      {
         depth += count_char(lb[j], le[j], '(') - count_char(lb[j], le[j], ')');
         if (depth <= 0)
            break;
         j++;
      }
      if (j >= n || depth > 0)
      {
         i++;
         continue;
      }
      /* what follows the last ')' on that line */
      for (tb = le[j]; tb > lb[j] && tb[-1] != ')'; tb--);
      te = le[j];
      while (tb < te && (CK_IS_SPACE(*tb) || (*tb >= 0x1c && *tb <= 0x1f)))
         tb++;
      while (te > tb && (CK_IS_SPACE(te[-1])
               || (te[-1] >= 0x1c && te[-1] <= 0x1f)))
         te--;
      k = j;
      if (!(te - tb == 1 && *tb == '{'))
      {
         if (k + 1 >= n || *lb[k + 1] != '{')
         {
            i = j + 1;
            continue;
         }
         k = j + 1;
      }
      depth = 0;
      {
         size_t first = k;
         while (k < n)
         {
            depth += count_char(lb[k], le[k], '{')
               - count_char(lb[k], le[k], '}');
            nbody++;
            if (depth <= 0 && nbody > 1)
               break;
            k++;
         }
         if (nbody > 1)
         {
            size_t last = k < n ? k : n - 1;
            size_t nlen = (size_t)(ne - nb);
            char *body  = ck_strndup(lb[first],
                  (size_t)(le[last] - lb[first]));
            size_t f;
            for (f = 0; f < out->n; f++)
            {
               if (strlen(out->v[f].name) == nlen
                     && !memcmp(out->v[f].name, nb, nlen))
                  break;
            }
            if (f < out->n)
            {
               free(out->v[f].body);
               out->v[f].body = body;
            }
            else
            {
               out->v = (struct func*)ck_realloc(out->v,
                     (out->n + 1) * sizeof(*out->v));
               out->v[out->n].name = ck_strndup(nb, nlen);
               out->v[out->n].body = body;
               out->n++;
            }
            i = k;
         }
      }
      i++;
   }
   for (i = 0; i < out->n; i++)
      *ck_set_add(&out->index, out->v[i].name, strlen(out->v[i].name))
         = &out->v[i];
   free(lb);
   free(le);
}

static struct func *find(const struct funcs *fs, const char *name, size_t n)
{
   void **slot = ck_set_find(&fs->index, name, n);
   return slot ? (struct func*)*slot : NULL;
}

/* \bNAME\s*\( */
static int calls(const char *body, const char *name)
{
   size_t n = strlen(name);
   const char *p;
   for (p = body; (p = strstr(p, name)); p++)
   {
      const char *q;
      if (p > body && CK_IS_WORD(p[-1]))
         continue;
      for (q = p + n; CK_IS_SPACE(*q); q++);
      if (*q == '(')
         return 1;
   }
   return 0;
}

static int cmp_str(const void *a, const void *b)
{
   return strcmp(*(char * const *)a, *(char * const *)b);
}

int main(int argc, char **argv)
{
   const char *root = getenv("RA_TREE_ROOT");
   struct ck_strv paths    = {0};
   struct ck_strv findings = {0};
   struct ck_strv names    = {0};
   struct ck_sb dir        = {0};
   unsigned long scanned   = 0;
   size_t i, k;
   int stale_any = 0, bad = 0;

   (void)argv;
   if (argc > 1)
   {
      fprintf(stderr, "usage: video_thread_state_check\n");
      return 2;
   }
   if (!root || !*root)
      root = ".";
   for (i = 0; i < COUNT(extra); i++)
      ck_strv_push(&paths, ck_strdup(extra[i]));
   ck_sb_puts(&dir, root);
   ck_sb_puts(&dir, "/" ROOT_DIR);
   if (!ck_list_dir(dir.p, &names))
   {
      fprintf(stderr, "video_thread_state_check: no %s under %s\n",
            ROOT_DIR, root);
      return 1;
   }
   ck_strv_sort(&names);
   for (i = 0; i < names.n; i++)
   {
      if (ck_ends_with(names.v[i], ".c") || ck_ends_with(names.v[i], ".m"))
      {
         struct ck_sb p = {0};
         ck_sb_puts(&p, ROOT_DIR "/");
         ck_sb_puts(&p, names.v[i]);
         ck_strv_push(&paths, p.p);
      }
   }
   for (i = 0; i < paths.n; i++)
   {
      struct ck_sb full = {0};
      struct funcs fs;
      struct ck_set seen = {0};
      struct ck_strv todo = {0};
      char *text;

      ck_sb_puts(&full, root);
      ck_sb_putc(&full, '/');
      ck_sb_puts(&full, paths.v[i]);
      text = ck_read_file(full.p, NULL);
      ck_sb_free(&full);
      if (!text)
      {
         fprintf(stderr, "video_thread_state_check: cannot read %s\n",
               paths.v[i]);
         return 1;
      }
      functions(text, &fs);
      if (!fs.n)
      {
         free(text);
         continue;
      }
      scanned++;
      for (k = 0; k < fs.n; k++)
      {
         if (is_seed(fs.v[k].name))
            ck_strv_push(&todo, ck_strdup(fs.v[k].name));
      }
      while (todo.n)
      {
         char *fn = todo.v[--todo.n];
         struct func *f;
         size_t e;
         int is_exempt = 0;
         const char *p;
         if (ck_set_find(&seen, fn, strlen(fn)))
         {
            free(fn);
            continue;
         }
         *ck_set_add(&seen, fn, strlen(fn)) = &seen;
         f = find(&fs, fn, strlen(fn));
         for (e = 0; e < COUNT(exempt); e++)
            is_exempt |= !strcmp(fn, exempt[e]);
         if (!f || !f->body[0] || is_exempt)
         {
            free(fn);
            continue;
         }
         for (e = 0; e < COUNT(live); e++)
         {
            if (calls(f->body, live[e]))
            {
               struct ck_sb key = {0};
               ck_sb_puts(&key, paths.v[i]);
               ck_sb_putc(&key, '\001');
               ck_sb_puts(&key, fn);
               ck_sb_putc(&key, '\001');
               ck_sb_puts(&key, live[e]);
               ck_strv_push(&findings, key.p);
            }
         }
         /* \b([a-z_]\w*)\s*\(  - each callee defined here, not seen */
         for (p = f->body; *p; )
         {
            const char *w, *q;
            if (!((*p >= 'a' && *p <= 'z') || *p == '_')
                  || (p > f->body && CK_IS_WORD(p[-1])))
            {
               p++;
               continue;
            }
            for (w = p; CK_IS_WORD(*p); p++);
            for (q = p; CK_IS_SPACE(*q); q++);
            if (*q != '(')
               continue;
            if (find(&fs, w, (size_t)(p - w))
                  && !ck_set_find(&seen, w, (size_t)(p - w)))
               ck_strv_push(&todo, ck_strndup(w, (size_t)(p - w)));
         }
         free(fn);
      }
      ck_strv_free(&todo);
      for (k = 0; k < seen.cap; k++)
         free(seen.keys[k]);
      free(seen.keys);
      free(seen.vals);
      for (k = 0; k < fs.n; k++)
      {
         free(fs.v[k].name);
         free(fs.v[k].body);
      }
      free(fs.v);
      for (k = 0; k < fs.index.cap; k++)
         free(fs.index.keys[k]);
      free(fs.index.keys);
      free(fs.index.vals);
      free(text);
   }

   /* the allowed files, each still reaching something */
   {
      struct ck_strv stale = {0};
      for (i = 0; i < COUNT(allowed); i++)
      {
         int reached = 0;
         size_t n = strlen(allowed[i][0]);
         for (k = 0; k < findings.n; k++)
            reached |= !strncmp(findings.v[k], allowed[i][0], n)
               && findings.v[k][n] == '\001';
         if (!reached)
            ck_strv_push(&stale, ck_strdup(allowed[i][0]));
      }
      ck_strv_sort(&stale);
      for (i = 0; i < stale.n; i++)
         printf("%s no longer reaches live state: drop it from allowed[] in "
               "tools/video_thread_state_check.c\n", stale.v[i]);
      stale_any = stale.n > 0;
      ck_strv_free(&stale);
   }
   if (findings.n > 1)
      qsort(findings.v, findings.n, sizeof(char*), cmp_str);
   for (k = 0; k < findings.n; k++)
   {
      char *a, *b;
      int is_allowed = 0;
      if (k && !strcmp(findings.v[k], findings.v[k - 1]))
         continue;
      a = strchr(findings.v[k], '\001');
      *a = '\0';
      b = strchr(a + 1, '\001');
      *b = '\0';
      for (i = 0; i < COUNT(allowed); i++)
         is_allowed |= !strcmp(findings.v[k], allowed[i][0]);
      if (!is_allowed)
      {
         printf("%s: %s() reaches %s() on the video thread; read the "
               "frame's snapshot instead\n", findings.v[k], a + 1, b + 1);
         bad = 1;
      }
      *a = '\001';
      *b = '\001';
   }
   if (bad || stale_any)
      return 1;
   printf("video-thread entry points in %lu driver(s): no live-state "
         "access this can follow\n", scanned);
   return 0;
}
