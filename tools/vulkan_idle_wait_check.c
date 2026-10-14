/* Vulkan idle-wait check.
 *
 * Flags every vkQueueWaitIdle and vkDeviceWaitIdle call in the Vulkan
 * video driver, the slang filter chain and the Vulkan context code that
 * is not on the allowlist.
 *
 * Rationale: an idle wait drains the whole queue, so under threaded video
 * it also drains the work a hardware core is submitting from its own
 * thread, and it has to run with queue_lock held, since the Vulkan spec
 * defines vkDeviceWaitIdle as vkQueueWaitIdle on every queue. A hardware
 * core that is parked in lock_queue with work still to submit - work the
 * queue may be waiting on - then never lets the queue go idle, and the
 * frontend stops (the parallel-GS/Granite present deadlock). The driver's
 * own objects are only referenced by the driver's own submissions, each
 * of which carries a fence, so the right wait is on those fences, lock
 * free: vulkan_wait_own_submissions() in gfx/drivers/vulkan.c. Every
 * idle wait that used to stand in for it has been replaced; this check
 * keeps them from coming back.
 *
 * The allowlist names the sites that legitimately remain, as
 * `file|function|count` entries, one per line. A new idle wait fails the
 * check; so does a stale entry, so the list shrinks with the code.
 *
 * Usage:
 *   tools/check.sh vulkan_idle_wait_check [--root DIR] [--allow FILE]
 *   tools/check.sh vulkan_idle_wait_check --selftest
 *
 * Exit status: 0 clean, 1 findings (or self-test failure). */

#include "check_common.h"

static const char *const files[] = {
   "gfx/drivers/vulkan.c",
   "gfx/drivers_shader/shader_vulkan.c",
   "gfx/common/vulkan_common.c",
   "gfx/common/win_vulkan_dxgi.c",
   "gfx/drivers_context/android_vk_openxr_ctx.c",
   "gfx/video_thread_hw.c"
};

/* \bvk(Queue|Device)WaitIdle\s*\(  anywhere on the line */
static int has_call(const char *b, const char *e)
{
   const char *p;
   for (p = b; p < e; p++)
   {
      const char *q;
      if (*p != 'v' || (p > b && CK_IS_WORD(p[-1])) || e - p < 2 || p[1] != 'k')
         continue;
      q = p + 2;
      if (e - q >= 5 && !memcmp(q, "Queue", 5))
         q += 5;
      else if (e - q >= 6 && !memcmp(q, "Device", 6))
         q += 6;
      else
         continue;
      if (e - q < 8 || memcmp(q, "WaitIdle", 8))
         continue;
      for (q += 8; q < e && CK_IS_SPACE(*q); q++);
      if (q < e && *q == '(')
         return 1;
   }
   return 0;
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

/* {file|function: count} of idle waits in one file */
static void scan_file(const char *path, const char *rel, struct ck_set *found)
{
   struct ck_sb func = {0};
   size_t len;
   char *raw = ck_read_file(path, &len);
   char *text, *line;

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
      if (ck_def_name(line, e, &nb, &ne) && !ends_with_semicolon(line, e))
      {
         func.len = 0;
         ck_sb_add(&func, nb, (size_t)(ne - nb));
      }
      if (has_call(line, e))
      {
         struct ck_sb key = {0};
         ck_sb_puts(&key, rel);
         ck_sb_putc(&key, '|');
         ck_sb_puts(&key, func.p);
         add(found, key.p, 1);
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
         scan_file(path.p, files[i], &found);
      ck_sb_free(&path);
   }
   sorted_keys(&found, &keys);
   for (i = 0; i < keys.n; i++)
   {
      unsigned long n    = get(&found, keys.v[i]);
      unsigned long want = get(&allow, keys.v[i]);
      if (n > want)
      {
         fprintf(stderr, "idle wait: %s: %lu call(s), %lu allowed\n",
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
   return bad;
}

static const char selftest_src[] =
   "/* vkQueue" "WaitIdle(in a comment) */\n"
   "static void keep_me(void *p)\n"
   "{\n"
   "   vkQueue" "WaitIdle(q);\n"
   "}\n"
   "\n"
   "static void new_offender(void *p)\n"
   "{\n"
   "   const char *s = \"vkDevice" "WaitIdle(in a string)\";\n"
   "   vkDevice" "WaitIdle(d);\n"
   "}\n";

static int selftest(void)
{
   struct ck_set found = {0};
   struct ck_sb src    = {0};
   struct ck_sb allow  = {0};
   char *tmp           = ck_tmpdir();
   int rc              = 0;

   ck_sb_puts(&src, tmp);
   ck_sb_puts(&src, "/gfx/drivers/vulkan.c");
   ck_mkdirs_for(src.p);
   ck_write_file(src.p, selftest_src);
   ck_sb_puts(&allow, tmp);
   ck_sb_puts(&allow, "/allow.list");
   ck_write_file(allow.p, "gfx/drivers/vulkan.c|keep_me|1\n"
         "gfx/drivers/vulkan.c|gone|1\n");
   scan_file(src.p, "gfx/drivers/vulkan.c", &found);
   if (found.n != 2 || get(&found, "gfx/drivers/vulkan.c|keep_me") != 1
         || get(&found, "gfx/drivers/vulkan.c|new_offender") != 1)
   {
      fprintf(stderr, "selftest: scan mismatch\n");
      rc = 1;
   }
   /* one new offender, one stale entry: two findings */
   else if (check(tmp, allow.p) != 2)
   {
      fprintf(stderr, "selftest: expected 2 findings\n");
      rc = 1;
   }
   else
      fprintf(stderr, "selftest: ok\n");
   free_set(&found);
   ck_rmtree(tmp);
   free(tmp);
   ck_sb_free(&src);
   ck_sb_free(&allow);
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
         fprintf(stderr, "usage: vulkan_idle_wait_check [--root DIR]"
               " [--allow FILE] [--selftest]\n");
         return 2;
      }
   }
   if (self)
      return selftest();
   if (!allow || !*allow)
   {
      ck_sb_puts(&def, root);
      ck_sb_puts(&def, "/tools/vulkan_idle_wait_allow.list");
      allow = def.p;
   }
   bad = check(root, allow);
   ck_sb_free(&def);
   if (bad)
   {
      fprintf(stderr, "%d idle-wait finding(s)\n", bad);
      return 1;
   }
   return 0;
}
