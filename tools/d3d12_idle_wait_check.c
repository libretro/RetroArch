/* D3D12 wait check.
 *
 * Flags every whole-queue drain (d3d12_queue_drain) and every CPU wait
 * on a fence (SetEventOnCompletion) in the D3D12 driver that is not on
 * the allowlist.
 *
 * Rationale: a drain signals the queue and waits for everything on it,
 * so under threaded video it also waits for the work a hardware core
 * has queued since - its next frame - and it is a stall on every frame
 * it sits in. The driver's own submissions each carry a signal of their
 * own (frameSignal for the frame command list, the per-buffer fences of
 * the upload and lend paths), and what a frame needs is to wait for its
 * predecessor's (d3d12_fence_wait, d3d12_cmd_begin) or not at all: a
 * buffer a frame still reads is retired behind its fence (d3d12_retire),
 * and a lend or an update that finds its buffer busy is declined or
 * dropped. The frame paths drained once; this check keeps a drain, or a
 * new fence wait, from coming back without a reason written down.
 *
 * The allowlist names the sites that legitimately remain, as
 * `file|function|call|count` entries, one per line. A new site fails the
 * check; so does a stale entry, so the list shrinks with the code.
 *
 * One drain is required rather than allowed: a ResizeBuffers must
 * follow a d3d12_queue_drain in its function. The frame's own signal
 * is behind its command list, not its Present, and resizing a back
 * buffer a Present still holds is a device error (it crashed the
 * driver on a software core's first frame).
 *
 * Usage:
 *   tools/check.sh d3d12_idle_wait_check [--root DIR] [--allow FILE]
 *   tools/check.sh d3d12_idle_wait_check --selftest
 *
 * Exit status: 0 clean, 1 findings (or self-test failure). */

#include "check_common.h"

static const char *const files[] = {
   "gfx/drivers/d3d12.c"
};

static const char *const wait_names[] = {
   "d3d12_queue_drain", "SetEventOnCompletion"
};

static const char *const resize_names[] = {
   "DXGIResizeBuffers", "ResizeBuffers"
};

/* \b(name)\s*\(  each on the line, by name; NULL when none is left
 * from p. */
static const char *next_call(const char *p, const char *e, const char **nb,
      const char **ne, const char *b, const char *const *names,
      size_t count)
{
   for (; p < e; p++)
   {
      size_t i;
      if (!CK_IS_WORD(*p) || (p > b && CK_IS_WORD(p[-1])))
         continue;
      for (i = 0; i < count; i++)
      {
         size_t n = strlen(names[i]);
         const char *q;
         if ((size_t)(e - p) < n || memcmp(p, names[i], n))
            continue;
         for (q = p + n; q < e && CK_IS_SPACE(*q); q++);
         if (q < e && *q == '(')
         {
            *nb = p;
            *ne = p + n;
            return q + 1;
         }
      }
   }
   return NULL;
}

static int ends_with_semicolon(const char *b, const char *e)
{
   while (e > b && (CK_IS_SPACE(e[-1]) || (e[-1] >= 0x1c && e[-1] <= 0x1f)))
      e--;
   return e > b && e[-1] == ';';
}

static void add(struct ck_set *set, const char *key, unsigned long n)
{
   void **slot = ck_set_add(set, key, strlen(key));
   if (!*slot)
   {
      *slot = ck_alloc(sizeof(unsigned long));
      *(unsigned long*)*slot = 0;
   }
   *(unsigned long*)*slot += n;
}

static void put(struct ck_set *set, const char *key, unsigned long n)
{
   void **slot = ck_set_add(set, key, strlen(key));
   if (!*slot)
      *slot = ck_alloc(sizeof(unsigned long));
   *(unsigned long*)*slot = n;
}

static unsigned long get(const struct ck_set *set, const char *key)
{
   void **slot = ck_set_find(set, key, strlen(key));
   return slot ? *(unsigned long*)*slot : 0;
}

/* {file|function|call: count} of waits in one file, and of resizes
 * with no drain before them in their function in @undrained */
static void scan_file(const char *path, const char *rel, struct ck_set *found,
      struct ck_set *undrained)
{
   struct ck_sb func = {0};
   size_t len;
   char *raw = ck_read_file(path, &len);
   char *text, *line;
   int drained = 0;

   if (!raw)
      return;
   text = ck_blank_code(raw, len, &len);
   free(raw);
   ck_sb_puts(&func, "<file scope>");
   for (line = text; line; )
   {
      char *nl      = (char*)memchr(line, '\n', (size_t)(text + len - line));
      const char *e = nl ? nl : text + len;
      const char *nb, *ne;
      const char *p  = line;
      if (ck_def_name(line, e, &nb, &ne) && !ends_with_semicolon(line, e))
      {
         func.len = 0;
         ck_sb_add(&func, nb, (size_t)(ne - nb));
         drained  = 0;
         /* the definition of the drain itself is not a call */
         p = e;
      }
      for (;;)
      {
         struct ck_sb key = {0};
         const char *w, *r, *wb = NULL, *we = NULL, *rb = NULL, *re = NULL;
         w = next_call(p, e, &wb, &we, line, wait_names, 2);
         r = next_call(p, e, &rb, &re, line, resize_names, 2);
         if (!w && !r)
            break;
         ck_sb_puts(&key, rel);
         ck_sb_putc(&key, '|');
         ck_sb_puts(&key, func.p);
         ck_sb_putc(&key, '|');
         if (w && (!r || wb < rb))
         {
            ck_sb_add(&key, wb, (size_t)(we - wb));
            add(found, key.p, 1);
            if (!memcmp(wb, wait_names[0], (size_t)(we - wb)))
               drained = 1;
            p = w;
         }
         else
         {
            ck_sb_puts(&key, "ResizeBuffers");
            if (!drained)
               add(undrained, key.p, 1);
            p = r;
         }
         ck_sb_free(&key);
      }
      line = nl ? nl + 1 : NULL;
   }
   free(text);
   ck_sb_free(&func);
}

static void load_allow(const char *path, struct ck_set *allow)
{
   char *text, *line;
   if (!path || !(text = ck_read_file(path, NULL)))
      return;
   for (line = text; line; )
   {
      char *nl   = strchr(line, '\n');
      char *hash = NULL;
      const char *b = line, *e;
      char *f1, *f2;
      if (nl)
         *nl = '\0';
      hash = strchr(line, '#');
      e    = hash ? hash : line + strlen(line);
      while (b < e && CK_IS_SPACE(*b))
         b++;
      while (e > b && CK_IS_SPACE(e[-1]))
         e--;
      if (b < e)
      {
         char *l = ck_strndup(b, (size_t)(e - b));
         f1      = strchr(l, '|');
         f1      = f1 ? strchr(f1 + 1, '|') : NULL;
         f2      = f1 ? strchr(f1 + 1, '|') : NULL;
         if (!f2 || strchr(f2 + 1, '|'))
         {
            const char *re = line + strlen(line);
            while (re > line && CK_IS_SPACE(re[-1]))
               re--;
            fprintf(stderr, "bad allowlist line: %.*s\n", (int)(re - line),
                  line);
            exit(1);
         }
         *f2 = '\0';
         put(allow, l, strtoul(f2 + 1, NULL, 10));
         free(l);
      }
      line = nl ? nl + 1 : NULL;
   }
   free(text);
}

static void sorted_keys(const struct ck_set *set, struct ck_strv *out)
{
   size_t i;
   for (i = 0; i < set->cap; i++)
   {
      if (set->keys[i])
         ck_strv_push(out, ck_strdup(set->keys[i]));
   }
   ck_strv_sort(out);
}

static void free_set(struct ck_set *set)
{
   size_t i;
   for (i = 0; i < set->cap; i++)
   {
      free(set->keys[i]);
      free(set->vals[i]);
   }
   free(set->keys);
   free(set->vals);
   memset(set, 0, sizeof(*set));
}

static int check(const char *root, const char *allow_path)
{
   struct ck_set allow = {0};
   struct ck_set found = {0};
   struct ck_set undrained = {0};
   struct ck_strv keys = {0};
   size_t i;
   int bad = 0;

   load_allow(allow_path, &allow);
   for (i = 0; i < sizeof(files) / sizeof(files[0]); i++)
   {
      struct ck_sb path = {0};
      ck_sb_puts(&path, root);
      ck_sb_putc(&path, '/');
      ck_sb_puts(&path, files[i]);
      if (ck_is_file(path.p) || ck_is_dir(path.p))
         scan_file(path.p, files[i], &found, &undrained);
      ck_sb_free(&path);
   }
   sorted_keys(&undrained, &keys);
   for (i = 0; i < keys.n; i++)
   {
      fprintf(stderr, "resize: %s: %lu call(s) with no d3d12_queue_drain"
            " before them in the function\n", keys.v[i],
            get(&undrained, keys.v[i]));
      bad++;
   }
   ck_strv_free(&keys);
   sorted_keys(&found, &keys);
   for (i = 0; i < keys.n; i++)
   {
      unsigned long n    = get(&found, keys.v[i]);
      unsigned long want = get(&allow, keys.v[i]);
      if (n > want)
      {
         fprintf(stderr, "wait: %s: %lu call(s), %lu allowed\n",
               keys.v[i], n, want);
         bad++;
      }
   }
   ck_strv_free(&keys);
   sorted_keys(&allow, &keys);
   for (i = 0; i < keys.n; i++)
   {
      unsigned long want = get(&allow, keys.v[i]);
      unsigned long n    = get(&found, keys.v[i]);
      if (n < want)
      {
         fprintf(stderr, "stale allowlist entry: %s|%lu (found %lu)\n",
               keys.v[i], want, n);
         bad++;
      }
   }
   ck_strv_free(&keys);
   free_set(&allow);
   free_set(&found);
   free_set(&undrained);
   return bad;
}

static const char selftest_src[] =
   "/* d3d12_queue" "_drain(in a comment) */\n"
   "static void d3d12_queue_drain(d3d12_video_t *d3d12)\n"
   "{\n"
   "   f->lpVtbl->SetEvent" "OnCompletion(f, v, e);\n"
   "}\n"
   "\n"
   "static void keep_me(void *p)\n"
   "{\n"
   "   d3d12_queue" "_drain(d3d12);\n"
   "}\n"
   "\n"
   "static void new_offender(void *p)\n"
   "{\n"
   "   const char *s = \"d3d12_queue" "_drain(in a string)\";\n"
   "   d3d12_queue" "_drain(d3d12);\n"
   "}\n";

static const char selftest_resize_src[] =
   "\n"
   "static void resize_ok(void *p)\n"
   "{\n"
   "   d3d12_queue" "_drain(d3d12);\n"
   "   DXGIResize" "Buffers(c, 2, w, h, f, 0);\n"
   "}\n"
   "\n"
   "static void resize_late(void *p)\n"
   "{\n"
   "   c->lpVtbl->Resize" "Buffers(c, 2, w, h, f, 0); d3d12_queue" "_drain(d3d12);\n"
   "}\n"
   "\n"
   "static void resize_none(void *p)\n"
   "{\n"
   "   d3d12_fence_wait(d3d12, d3d12->queue.frameSignal);\n"
   "   DXGIResize" "Buffers(c, 2, w, h, f, 0);\n"
   "}\n";

static int selftest(void)
{
   static const char rel[] = "gfx/drivers/d3d12.c";
   struct ck_set found = {0};
   struct ck_set undrained = {0};
   struct ck_sb src    = {0};
   struct ck_sb allow  = {0};
   struct ck_sb text   = {0};
   char *tmp           = ck_tmpdir();
   int rc              = 0;

   ck_sb_puts(&src, tmp);
   ck_sb_putc(&src, '/');
   ck_sb_puts(&src, rel);
   ck_mkdirs_for(src.p);
   ck_sb_puts(&text, selftest_src);
   ck_sb_puts(&text, selftest_resize_src);
   ck_write_file(src.p, text.p);
   ck_sb_puts(&allow, tmp);
   ck_sb_puts(&allow, "/allow.list");
   ck_write_file(allow.p,
         "gfx/drivers/d3d12.c|d3d12_queue_drain|SetEventOnCompletion|1\n"
         "gfx/drivers/d3d12.c|keep_me|d3d12_queue_drain|1\n"
         "gfx/drivers/d3d12.c|resize_ok|d3d12_queue_drain|1\n"
         "gfx/drivers/d3d12.c|resize_late|d3d12_queue_drain|1\n"
         "gfx/drivers/d3d12.c|gone|d3d12_queue_drain|1\n");
   scan_file(src.p, rel, &found, &undrained);
   if (found.n != 5
         || undrained.n != 2
         || get(&undrained, "gfx/drivers/d3d12.c|resize_late|"
            "ResizeBuffers") != 1
         || get(&undrained, "gfx/drivers/d3d12.c|resize_none|"
            "ResizeBuffers") != 1
         || get(&found, "gfx/drivers/d3d12.c|d3d12_queue_drain|"
            "SetEventOnCompletion") != 1
         || get(&found, "gfx/drivers/d3d12.c|keep_me|d3d12_queue_drain") != 1
         || get(&found, "gfx/drivers/d3d12.c|new_offender|"
            "d3d12_queue_drain") != 1)
   {
      fprintf(stderr, "selftest: scan mismatch\n");
      rc = 1;
   }
   /* one new offender, one stale entry, two undrained resizes */
   else if (check(tmp, allow.p) != 4)
   {
      fprintf(stderr, "selftest: expected 4 findings\n");
      rc = 1;
   }
   else
      fprintf(stderr, "selftest: ok\n");
   free_set(&found);
   free_set(&undrained);
   ck_rmtree(tmp);
   free(tmp);
   ck_sb_free(&src);
   ck_sb_free(&allow);
   ck_sb_free(&text);
   return rc;
}

int main(int argc, char **argv)
{
   const char *root  = ".";
   const char *allow = NULL;
   struct ck_sb def  = {0};
   int self          = 0;
   int bad, i;

   for (i = 1; i < argc; i++)
   {
      if (!strcmp(argv[i], "--selftest"))
         self = 1;
      else if (!strcmp(argv[i], "--root") && i + 1 < argc)
         root = argv[++i];
      else if (!strncmp(argv[i], "--root=", 7))
         root = argv[i] + 7;
      else if (!strcmp(argv[i], "--allow") && i + 1 < argc)
         allow = argv[++i];
      else if (!strncmp(argv[i], "--allow=", 8))
         allow = argv[i] + 8;
      else
      {
         fprintf(stderr, "usage: d3d12_idle_wait_check [--root DIR]"
               " [--allow FILE] [--selftest]\n");
         return 2;
      }
   }
   if (self)
      return selftest();
   if (!allow || !*allow)
   {
      ck_sb_puts(&def, root);
      ck_sb_puts(&def, "/tools/d3d12_idle_wait_allow.list");
      allow = def.p;
   }
   bad = check(root, allow);
   ck_sb_free(&def);
   if (bad)
   {
      fprintf(stderr, "%d wait finding(s)\n", bad);
      return 1;
   }
   return 0;
}
