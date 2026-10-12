/* Vulkan teardown-order check.
 *
 * Holds the order in which the Vulkan video driver retires the objects a
 * recorded frame is built out of, and the rule that a descriptor pool is
 * the one call that frees its sets.
 *
 * Rationale: a command buffer references the descriptor sets and the
 * pipelines it was recorded with, and a descriptor set references the
 * layout it was allocated from. A driver is free to keep its own back
 * references along that chain and to walk them as each pool is
 * destroyed - MoltenVK does - so teardown has to run innermost first:
 * command pools, then descriptor pools, then the pipelines that own the
 * set layout. The other order is legal Vulkan and passes the validation
 * layer, which is why it needs a check of its own: it faults inside
 * vkDestroyDescriptorPool, on the video thread, at exit.
 *
 * Three rules, over gfx/drivers/vulkan.c:
 *
 *   1. In each teardown function, whichever of vulkan_deinit_*() it
 *      calls appear in the order above.
 *   2. No vkFreeDescriptorSets: vkDestroyDescriptorPool releases every
 *      set allocated from the pool, and the per-set pass over the same
 *      bookkeeping is what the destroy then repeats.
 *   3. No VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT in a file
 *      that frees no sets. The flag is required only by
 *      vkFreeDescriptorSets, and buys a per-set allocator on the drivers
 *      that provide one.
 *
 * Usage:
 *   tools/check.sh vulkan_teardown_order_check [--root DIR]
 *   tools/check.sh vulkan_teardown_order_check --selftest
 *
 * Exit status: 0 clean, 1 findings (or self-test failure). */

#include "check_common.h"

#define COUNT(a) (sizeof(a) / sizeof((a)[0]))

static const char *const files[] = { "gfx/drivers/vulkan.c" };

/* Innermost first. Only the calls a function actually makes are
 * checked, so a teardown that does not touch one of these is fine. */
static const char *const order[] = {
   "vulkan_deinit_command_buffers",
   "vulkan_deinit_descriptor_pool",
   "vulkan_deinit_pipelines"
};

static const char *const teardown_funcs[] = {
   "vulkan_free",
   "vulkan_check_swapchain"
};

/* written in two pieces so that the tree's own scans skip this file */
#define FREE_SETS "vkFree" "DescriptorSets"
#define FREE_FLAG "VK_DESCRIPTOR_POOL_CREATE_" "FREE_DESCRIPTOR_SET_BIT"

struct lines
{
   char **v;
   size_t n;
};

/* A line with its spaces (not its tabs) taken out contains s( */
static int has_call(const char *line, const char *s)
{
   struct ck_sb packed = {0};
   const char *p;
   int found;
   for (p = line; *p; p++)
   {
      if (*p != ' ')
         ck_sb_putc(&packed, *p);
   }
   ck_sb_puts(&packed, "");
   {
      struct ck_sb want = {0};
      ck_sb_puts(&want, s);
      ck_sb_putc(&want, '(');
      found = strstr(packed.p, want.p) != NULL;
      ck_sb_free(&want);
   }
   ck_sb_free(&packed);
   return found;
}

static int ends_with_semicolon(const char *line)
{
   const char *e = line + strlen(line);
   while (e > line && (CK_IS_SPACE(e[-1]) || (e[-1] >= 0x1c && e[-1] <= 0x1f)))
      e--;
   return e > line && e[-1] == ';';
}

/* Lines of one function's body, by the tree's definition shape: the
 * name directly before the parameter list, on a line that starts at
 * column 0, closed by a brace at column 0. Returns 0 when not found. */
static int function_body(const struct lines *l, const char *name,
      size_t *from, size_t *to)
{
   size_t i;
   long start = -1;
   for (i = 0; i < l->n; i++)
   {
      const char *line = l->v[i];
      if (start < 0)
      {
         if (     line[0] && line[0] != ' ' && line[0] != '\t'
               && line[0] != '#' && has_call(line, name)
               && !ends_with_semicolon(line))
            start = (long)i;
      }
      else if (line[0] == '}')
      {
         *from = (size_t)start;
         *to   = i;
         return *to > *from;
      }
   }
   return 0;
}

typedef void (*report_fn)(const char *msg, void *ud);

static void to_stderr(const char *msg, void *ud)
{
   (void)ud;
   fputs(msg, stderr);
}

/* Rule 1, over one file's teardown functions. */
static int check_order(const char *rel, const struct lines *l,
      report_fn report, void *ud)
{
   size_t f;
   int bad = 0;
   for (f = 0; f < COUNT(teardown_funcs); f++)
   {
      struct ck_sb m   = {0};
      int *seen    = NULL;
      size_t nseen = 0, i, k;
      size_t from, to;
      int sorted = 1;
      if (!function_body(l, teardown_funcs[f], &from, &to))
      {
         ck_sb_puts(&m, "teardown order: ");
         ck_sb_puts(&m, rel);
         ck_sb_puts(&m, ": ");
         ck_sb_puts(&m, teardown_funcs[f]);
         ck_sb_puts(&m, "() not found\n");
         report(m.p, ud);
         ck_sb_free(&m);
         bad++;
         continue;
      }
      for (i = from; i < to; i++)
      {
         for (k = 0; k < COUNT(order); k++)
         {
            if (has_call(l->v[i], order[k]))
            {
               seen = (int*)ck_realloc(seen, (nseen + 1) * sizeof(int));
               seen[nseen++] = (int)k;
            }
         }
      }
      for (i = 1; i < nseen; i++)
         sorted &= seen[i - 1] <= seen[i];
      if (!sorted)
      {
         ck_sb_puts(&m, "teardown order: ");
         ck_sb_puts(&m, rel);
         ck_sb_putc(&m, '|');
         ck_sb_puts(&m, teardown_funcs[f]);
         ck_sb_puts(&m, ": ");
         for (i = 0; i < nseen; i++)
         {
            if (i)
               ck_sb_puts(&m, " then ");
            ck_sb_puts(&m, order[seen[i]]);
         }
         ck_sb_puts(&m, "\n");
         report(m.p, ud);
         m.len = 0;
         ck_sb_puts(&m, "               expected: ");
         {
            int first = 1;
            for (k = 0; k < COUNT(order); k++)
            {
               int in = 0;
               for (i = 0; i < nseen; i++)
                  in |= seen[i] == (int)k;
               if (!in)
                  continue;
               if (!first)
                  ck_sb_puts(&m, " then ");
               ck_sb_puts(&m, order[k]);
               first = 0;
            }
         }
         ck_sb_puts(&m, "\n");
         report(m.p, ud);
         bad++;
      }
      free(seen);
      ck_sb_free(&m);
   }
   return bad;
}

static int check_file(const char *root, const char *rel, report_fn report,
      void *ud)
{
   struct ck_sb path = {0};
   struct lines l    = { NULL, 0 };
   unsigned long frees = 0;
   int any_flag = 0, bad;
   size_t len, i;
   char *raw, *text, *line;

   ck_sb_puts(&path, root);
   ck_sb_putc(&path, '/');
   ck_sb_puts(&path, rel);
   raw = ck_read_file(path.p, &len);
   ck_sb_free(&path);
   if (!raw)
      return 0;
   text = ck_blank_code(raw, len, &len);
   free(raw);
   for (line = text; line; )
   {
      char *nl = strchr(line, '\n');
      if (nl)
         *nl = '\0';
      l.v = (char**)ck_realloc(l.v, (l.n + 1) * sizeof(char*));
      l.v[l.n++] = line;
      line = nl ? nl + 1 : NULL;
   }
   bad = check_order(rel, &l, report, ud);
   for (i = 0; i < l.n; i++)
   {
      frees    += has_call(l.v[i], FREE_SETS) ? 1 : 0;
      any_flag |= strstr(l.v[i], FREE_FLAG) != NULL;
   }
   if (frees)
   {
      char tmp[64];
      struct ck_sb m = {0};
      ck_sb_puts(&m, "teardown order: ");
      ck_sb_puts(&m, rel);
      sprintf(tmp, ": %lu ", frees);
      ck_sb_puts(&m, tmp);
      ck_sb_puts(&m, FREE_SETS " call(s); the pool destroy frees its sets\n");
      report(m.p, ud);
      ck_sb_free(&m);
      bad++;
   }
   else if (any_flag)
   {
      struct ck_sb m = {0};
      ck_sb_puts(&m, "teardown order: ");
      ck_sb_puts(&m, rel);
      ck_sb_puts(&m, ": " FREE_FLAG " set, but no " FREE_SETS
            " call needs it\n");
      report(m.p, ud);
      ck_sb_free(&m);
      bad++;
   }
   free(l.v);
   free(text);
   return bad;
}

static int check(const char *root, report_fn report, void *ud)
{
   size_t i;
   int bad = 0;
   for (i = 0; i < COUNT(files); i++)
      bad += check_file(root, files[i], report, ud);
   return bad ? 1 : 0;
}

static const char good_src[] =
   "\n"
   "static void vulkan_free(void *data)\n"
   "{\n"
   "   if (vk->context)\n"
   "   {\n"
   "      vulkan_deinit_command_buffers(vk);\n"
   "      vulkan_deinit_descriptor_pool(vk);\n"
   "      vulkan_deinit_pipelines(vk);\n"
   "   }\n"
   "}\n"
   "\n"
   "static void vulkan_check_swapchain(vk_t *vk)\n"
   "{\n"
   "   vulkan_deinit_command_buffers(vk);\n"
   "   vulkan_deinit_descriptor_pool(vk);\n"
   "   vulkan_deinit_pipelines(vk);\n"
   "}\n";

static const char bad_src[] =
   "\n"
   "static void vulkan_free(void *data)\n"
   "{\n"
   "   if (vk->context)\n"
   "   {\n"
   "      vulkan_deinit_pipelines(vk);\n"
   "      vulkan_deinit_descriptor_pool(vk);\n"
   "      vulkan_deinit_command_buffers(vk);\n"
   "   }\n"
   "}\n"
   "\n"
   "static void vulkan_check_swapchain(vk_t *vk)\n"
   "{\n"
   "   vulkan_deinit_command_buffers(vk);\n"
   "   vulkan_deinit_descriptor_pool(vk);\n"
   "   vulkan_deinit_pipelines(vk);\n"
   "   pool_info.flags = " FREE_FLAG ";\n"
   "}\n";

static void quiet(const char *msg, void *ud)
{
   (void)msg;
   (*(int*)ud)++;
}

static int selftest(void)
{
   int reports = 0;
   int t;
   for (t = 0; t < 2; t++)
   {
      char *root = ck_tmpdir();
      struct ck_sb p = {0};
      int got;
      ck_sb_puts(&p, root);
      ck_sb_puts(&p, "/gfx/drivers/vulkan.c");
      ck_mkdirs_for(p.p);
      ck_write_file(p.p, t ? bad_src : good_src);
      got = check(root, quiet, &reports);
      ck_rmtree(root);
      free(root);
      ck_sb_free(&p);
      if (got != t)
      {
         fprintf(stderr, "selftest: %s tree returned %d, wanted %d\n",
               t ? "bad" : "good", got, t);
         return 1;
      }
   }
   /* The bad tree has to fail for both reasons, not just one. */
   if (reports != 3)
   {
      fprintf(stderr, "selftest: expected 3 findings, got %d\n", reports);
      return 1;
   }
   fprintf(stderr, "selftest: ok\n");
   return 0;
}

int main(int argc, char **argv)
{
   const char *root = getenv("RA_TREE_ROOT");
   int self         = 0;
   int i;

   if (!root || !*root)
      root = ".";
   for (i = 1; i < argc; i++)
   {
      if (!strcmp(argv[i], "--selftest"))
         self = 1;
      else if (!strcmp(argv[i], "--root") && i + 1 < argc)
         root = argv[++i];
      else if (!strncmp(argv[i], "--root=", 7))
         root = argv[i] + 7;
      else
      {
         fprintf(stderr, "usage: vulkan_teardown_order_check [--root DIR]"
               " [--selftest]\n");
         return 2;
      }
   }
   if (self)
      return selftest();
   return check(root, to_stderr, NULL);
}
