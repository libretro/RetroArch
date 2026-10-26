/* Vulkan NULL-submit check.
 *
 * Flags every vkQueueSubmit call under gfx/ whose pSubmits argument is
 * NULL (or 0, or nullptr). An empty submit that only signals a fence is
 * valid Vulkan, but layers that hook vkQueueSubmit - OptiScaler, loaded
 * as nvngx.dll for the temporal upscaler - refuse a NULL pSubmits
 * without submitting, so the fence never signals and whoever waits on
 * it waits forever. vulkan_queue_signal_fence() submits an empty batch
 * instead.
 *
 * Usage:
 *   tools/check.sh vulkan_null_submit_check [--root DIR]
 *   tools/check.sh vulkan_null_submit_check --selftest
 *
 * Exit status: 0 clean, 1 findings (or self-test failure). */

#include "check_common.h"

static int is_null_arg(const char *b, const char *e)
{
   while (b < e && CK_IS_SPACE(*b))
      b++;
   while (e > b && CK_IS_SPACE(e[-1]))
      e--;
   return (e - b == 4 && !memcmp(b, "NULL", 4))
      || (e - b == 7 && !memcmp(b, "nullptr", 7))
      || (e - b == 1 && *b == '0');
}

/* Findings in @text (comments and strings blanked), reported as
 * rel:line */
static int scan_text(const char *text, size_t len, const char *rel)
{
   static const char name[] = "vkQueueSubmit";
   const size_t nlen = sizeof(name) - 1;
   const char *end   = text + len;
   const char *p;
   int bad = 0;

   for (p = text; p + nlen <= end; p++)
   {
      const char *q, *arg;
      int depth, idx;
      if (*p != 'v' || memcmp(p, name, nlen)
            || (p > text && CK_IS_WORD(p[-1])))
         continue;
      q = p + nlen;
      if (q < end && CK_IS_WORD(*q))
         continue;
      while (q < end && CK_IS_SPACE(*q))
         q++;
      if (q >= end || *q != '(')
         continue;
      /* the third argument, at the call's own depth */
      depth = 0;
      idx   = 0;
      arg   = q + 1;
      for (; q < end; q++)
      {
         if (*q == '(' || *q == '[' || *q == '{')
            depth++;
         else if (*q == ')' || *q == ']' || *q == '}')
         {
            if (--depth == 0)
               break;
         }
         else if (*q == ',' && depth == 1)
         {
            if (idx == 2)
               break;
            idx++;
            arg = q + 1;
         }
      }
      if (idx == 2 && q < end && is_null_arg(arg, q))
      {
         const char *c;
         unsigned long line = 1;
         for (c = text; c < p; c++)
            if (*c == '\n')
               line++;
         fprintf(stderr, "NULL pSubmits: %s:%lu: use vulkan_queue_signal_fence()\n",
               rel, line);
         bad++;
      }
   }
   return bad;
}

static int scan_file(const char *path, const char *rel)
{
   size_t len;
   char *raw = ck_read_file(path, &len);
   char *text;
   int bad;
   if (!raw)
      return 0;
   text = ck_blank_code(raw, len, &len);
   free(raw);
   bad = scan_text(text, len, rel);
   free(text);
   return bad;
}

struct walk_state
{
   const char *prefix;
   int bad;
};

static void on_file(const char *path, const char *rel, void *ud)
{
   struct walk_state *st = (struct walk_state*)ud;
   struct ck_sb full = {0};
   if (!ck_ends_with(rel, ".c") && !ck_ends_with(rel, ".cpp")
         && !ck_ends_with(rel, ".m") && !ck_ends_with(rel, ".h"))
      return;
   ck_sb_puts(&full, st->prefix);
   ck_sb_putc(&full, '/');
   ck_sb_puts(&full, rel);
   st->bad += scan_file(path, full.p);
   ck_sb_free(&full);
}

static int check(const char *root)
{
   static const char *const dirs[] = { "gfx" };
   struct walk_state st;
   size_t i;
   st.bad = 0;
   for (i = 0; i < sizeof(dirs) / sizeof(dirs[0]); i++)
   {
      struct ck_sb path = {0};
      ck_sb_puts(&path, root);
      ck_sb_putc(&path, '/');
      ck_sb_puts(&path, dirs[i]);
      st.prefix = dirs[i];
      if (ck_is_dir(path.p))
         ck_walk(path.p, NULL, on_file, &st);
      ck_sb_free(&path);
   }
   return st.bad;
}

static const char selftest_src[] =
   "/* vkQueue" "Submit(q, 0, NULL, f) in a comment */\n"
   "static void ok(void)\n"
   "{\n"
   "   const char *s = \"vkQueue" "Submit(q, 0, NULL, f)\";\n"
   "   vkQueue" "Submit(q, 1, &submit, f);\n"
   "   vkQueue" "Submit2(q, 0, NULL, f);\n"
   "   vk->vkQueue" "Submit(q, n, (pick(a, b)), f);\n"
   "}\n"
   "static void bad(void)\n"
   "{\n"
   "   vkQueue" "Submit(q, 0, NULL, f);\n"
   "   res = vkQueue" "Submit (ctx->queue, 0,\n"
   "         0, fences[slot]);\n"
   "}\n";

static int selftest(void)
{
   struct ck_sb file = {0};
   char *tmp         = ck_tmpdir();
   int found, rc     = 0;

   ck_sb_puts(&file, tmp);
   ck_sb_puts(&file, "/gfx/drivers/vk.c");
   ck_mkdirs_for(file.p);
   ck_write_file(file.p, selftest_src);
   found = check(tmp);
   if (found != 2)
   {
      fprintf(stderr, "selftest: expected 2 findings, got %d\n", found);
      rc = 1;
   }
   else
      fprintf(stderr, "selftest: ok\n");
   ck_rmtree(tmp);
   free(tmp);
   ck_sb_free(&file);
   return rc;
}

int main(int argc, char **argv)
{
   const char *root = ".";
   int self         = 0;
   int bad, i;

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
         fprintf(stderr, "usage: vulkan_null_submit_check [--root DIR] [--selftest]\n");
         return 2;
      }
   }
   if (self)
      return selftest();
   bad = check(root);
   if (bad)
   {
      fprintf(stderr, "%d NULL-submit finding(s)\n", bad);
      return 1;
   }
   return 0;
}
