/* RGUI 8-bit -> 16bpp pixel format conversion contract check.
 *
 * RGUI renders into a 16bpp framebuffer, so every menu colour, wallpaper
 * and thumbnail pixel goes through one of the argb32_to_* converters in
 * menu/drivers/rgui.c. Each converter quantises the four 8-bit input
 * channels to the widths of its target format and then ORs them into
 * their fields. The widths and the shifts are written out by hand, on
 * separate lines, once per format - and there are fourteen driver idents
 * mapping onto seven formats, plus a dithered twin of each.
 *
 * Nothing checks that a channel's quantised width still fits the shift
 * it is packed at. When it does not, the OR silently spills the top of
 * one channel into the bottom of its neighbour: no warning, no overflow,
 * just wrong colours. Worse, the formats are per-platform, so the damage
 * lands on whichever platform owns that converter and on no other. A
 * desktop build stays perfect.
 *
 * This actually shipped. c6b66c9 widened green in argb32_to_rgb565 from
 * 5 bits to 6, correctly, but left the packing at `(g << 6)` where a
 * 6-bit green needs `(g << 5)`. Green's top bit landed on red's LSB and
 * the green field became (g & 31) << 1, so green wrapped hard at half
 * scale - mid grey came out (139, 0, 131) - on sdl_dingux, sdl_rs90 and
 * xvideo. It reached users as #19469, bisected by hand on an Anbernic
 * RG300X, because no machine in CI runs a 565 menu.
 *
 * So, per driver ident in rgui_pixel_format_map, and for the fallback:
 *
 * 1. The bits each input channel can change must be disjoint from every
 *    other channel's. This is the c6b66c9 failure, stated generally.
 * 2. Each channel's bits must be contiguous.
 * 3. Endpoints must be exact: input 0 gives an all-zero field, input
 *    255 an all-ones field.
 * 4. Field value must be monotonic in the input - the visible symptom
 *    of a spill is a channel that falls as its input rises.
 * 5. The dithered twin must reach exactly the same bits as the plain
 *    converter, and land within one level of it. Equal reach is also
 *    what catches a table row wired to the wrong format's twin.
 *
 * The converters are static inside a 9000-line file, so the marked
 * `pixel format conversion` block is lifted out and compiled on its own
 * inside tools/rgui_pixfmt_harness.inc, against two stubs. That keeps
 * the test on the real shipped code - including the real dispatch table
 * - rather than on a copy that can drift. Nothing is hardcoded about any
 * format's layout; the field masks are measured from the converters
 * themselves.
 *
 * Usage:
 *    tools/check.sh rgui_pixfmt_check [--selftest] [--root DIR] [--cc CC]
 *
 * Exit status is 0 when every converter honours the contract, 1
 * otherwise. */

#include "check_common.h"

#define RGUI_C     "menu/drivers/rgui.c"
#define MARK_START "pixel format conversion START"
#define MARK_END   "pixel format conversion END"

static const char *g_root = ".";
static const char *g_cc   = "cc";
static const char *g_tools;

/* The marked pixel-format region of rgui.c, as C source. */
static char *extract_block(void)
{
   struct ck_sb path = {0};
   const char **lines = NULL;
   size_t n = 0, i;
   long start = -1, end = -1;
   char *text, *p;

   ck_sb_puts(&path, g_root);
   ck_sb_puts(&path, "/" RGUI_C);
   if (!(text = ck_read_file(path.p, NULL)))
   {
      fprintf(stderr, "%s: cannot read it\n", path.p);
      exit(1);
   }
   for (p = text; *p; )
   {
      char *nl = strchr(p, '\n');
      lines = (const char**)ck_realloc(lines, (n + 2) * sizeof(char*));
      lines[n++] = p;
      if (!nl)
         break;
      p = nl + 1;
   }
   lines[n] = text + strlen(text);
   for (i = 0; i < n; i++)
   {
      const char *e = lines[i + 1];
      char *line    = ck_strndup(lines[i], (size_t)(e - lines[i]));
      if (strstr(line, MARK_START))
         start = (long)i;
      else if (strstr(line, MARK_END))
      {
         end = (long)i;
         free(line);
         break;
      }
      free(line);
   }
   if (start < 0 || end < 0 || end <= start)
   {
      fprintf(stderr, "%s: could not find the '%s' / '%s' banner"
            " comments.\nThe checker lifts the converters out by those"
            " markers; if they were renamed, update MARK_START/MARK_END"
            " here too.\n", path.p, MARK_START, MARK_END);
      exit(1);
   }
   ck_sb_free(&path);
   /* The markers sit on the middle line of a three-line banner, so step
    * over the banner's closing (and opening) rule. */
   {
      long a = start + 2, b = end - 1;
      char *out;
      if (b < a)
         b = a;
      if ((size_t)b > n)
         b = (long)n;
      if ((size_t)a > n)
         a = (long)n;
      out = ck_strndup(lines[a], (size_t)(lines[b] - lines[a]));
      free(lines);
      free(text);
      return out;
   }
}

/* Compile the harness around block and run it: its exit status, and
 * its output (or the compiler's) in *out. */
static int run_block(const char *block, char **out)
{
   char *tmp = ck_tmpdir();
   struct ck_sb f = {0}, cmd = {0}, exe = {0};
   char *res;
   int st;

   ck_sb_puts(&f, tmp);
   ck_sb_puts(&f, "/rgui_pixfmt_block.h");
   if (!ck_write_file(f.p, block))
      ck_die("rgui_pixfmt_check: cannot write the lifted block");
   ck_sb_puts(&exe, tmp);
   ck_sb_puts(&exe, "/rgui_pixfmt_harness");

   ck_sb_puts(&cmd, g_cc);
   ck_sb_puts(&cmd, " -std=c89 -pedantic -Wall -x c -I");
   f.len = 0;
   ck_sb_puts(&f, g_root);
   ck_sb_puts(&f, "/libretro-common/include");
   ck_sb_shq(&cmd, f.p);
   ck_sb_puts(&cmd, " -I");
   ck_sb_shq(&cmd, g_root);
   ck_sb_puts(&cmd, " -I");
   ck_sb_shq(&cmd, tmp);
   ck_sb_putc(&cmd, ' ');
   f.len = 0;
   ck_sb_puts(&f, g_tools);
   ck_sb_puts(&f, "/rgui_pixfmt_harness.inc");
   ck_sb_shq(&cmd, f.p);
   ck_sb_puts(&cmd, " -o ");
   ck_sb_shq(&cmd, exe.p);
   ck_sb_puts(&cmd, " 2>&1");
   res = ck_capture(cmd.p, NULL, &st);
   if (st != 0)
   {
      struct ck_sb m = {0};
      ck_sb_puts(&m, "harness failed to compile - the lifted region may"
            " have gained a dependency that needs a stub:\n");
      ck_sb_puts(&m, res);
      free(res);
      *out = m.p;
      st   = 1;
   }
   else
   {
      free(res);
      cmd.len = 0;
      ck_sb_shq(&cmd, exe.p);
      ck_sb_puts(&cmd, " 2>&1");
      *out = ck_capture(cmd.p, NULL, &st);
   }
   ck_rmtree(tmp);
   free(tmp);
   ck_sb_free(&f);
   ck_sb_free(&cmd);
   ck_sb_free(&exe);
   return st;
}

static const char *ws(const char *p)
{
   while (CK_IS_SPACE(*p))
      p++;
   return p;
}

static const char *lit(const char *p, const char *s)
{
   size_t n = strlen(s);
   return !strncmp(p, s, n) ? p + n : NULL;
}

/* return\s*\(r\s*<<\s*11\)\s*\|\s*\(g\s*<<\s*  (\d+)  \)\s*\|\s*b;
 * at p: where the shift's digits start and end, and the match's end */
static int pack_565_at(const char *p, const char **db, const char **de,
      const char **me)
{
   static const char *const pre[] = {
      "return", "(r", "<<", "11)", "|", "(g", "<<"
   };
   size_t k;
   const char *q = p;
   for (k = 0; k < 7; k++)
   {
      if (k)
         q = ws(q);
      /* "(r" and "(g": \( then the letter, no space between */
      if (!(q = lit(q, pre[k])))
         return 0;
   }
   q = ws(q);
   if (!CK_IS_DIGIT(*q))
      return 0;
   *db = q;
   while (CK_IS_DIGIT(*q))
      q++;
   *de = q;
   if (*q != ')')
      return 0;
   q = ws(q + 1);
   if (*q != '|')
      return 0;
   q = ws(q + 1);
   if (*q != 'b' || q[1] != ';')
      return 0;
   *me = q + 2;
   return 1;
}

/* the block with the RGB565 green packing forced to shift; NULL when
 * there is no such line */
static char *repack_565(const char *block, unsigned shift)
{
   struct ck_sb o = {0};
   const char *p = block;
   int found = 0;
   while (*p)
   {
      const char *db, *de, *me;
      if (pack_565_at(p, &db, &de, &me))
      {
         ck_sb_add(&o, p, (size_t)(db - p));
         ck_sb_putu(&o, shift);
         ck_sb_add(&o, de, (size_t)(me - de));
         p     = me;
         found = 1;
         continue;
      }
      ck_sb_putc(&o, *p++);
   }
   ck_sb_add(&o, "", 0);
   if (!found)
   {
      ck_sb_free(&o);
      return NULL;
   }
   return o.p;
}

/* Prove the checker separates the c6b66c9 regression from its fix.
 *
 * Both fixtures are derived from the tree by forcing the green packing
 * shift either way, so the selftest says the same thing whether or not
 * the tree is currently fixed. */
static int selftest(void)
{
   static const char from[] = "argb32_to_rgb565,   argb32_to_rgb565_dither";
   static const char to[]   = "argb32_to_rgb565,   argb32_to_abgr4444_dither";
   char *block = extract_block();
   char *good, *bad, *out, *at;
   int failures = 0, st;

   if (!(good = repack_565(block, 5)))
   {
      printf("[fail] selftest: could not find the RGB565 packing line;"
            " the fixture generator needs updating\n");
      free(block);
      return 1;
   }
   st = run_block(good, &out);
   if (st == 0)
      printf("[ok]   fixture: 6-bit green packed at <<5 passes\n");
   else
   {
      printf("[fail] fixture: the corrected converter should pass:\n%s\n",
            out);
      failures++;
   }
   free(out);
   free(good);

   bad = repack_565(block, 6);
   st  = run_block(bad, &out);
   if (st != 0 && strstr(out, "share bits"))
      printf("[ok]   fixture: c6b66c9's 6-bit green at <<6 is caught\n");
   else
   {
      printf("[fail] fixture: the #19469 regression went undetected:\n%s\n",
            out);
      failures++;
   }
   free(out);
   free(bad);

   /* A table row pointing at another format's dithered twin. */
   if (!(at = strstr(block, from)))
   {
      printf("[fail] selftest: could not build the mismatched-pair"
            " fixture; the dispatch table's formatting changed\n");
      failures++;
   }
   else
   {
      struct ck_sb m = {0};
      ck_sb_add(&m, block, (size_t)(at - block));
      ck_sb_puts(&m, to);
      ck_sb_puts(&m, at + sizeof(from) - 1);
      st = run_block(m.p, &out);
      if (st != 0)
         printf("[ok]   fixture: a mismatched conv/conv_dither pair"
               " is caught\n");
      else
      {
         printf("[fail] fixture: mismatched converter pair went"
               " undetected:\n%s\n", out);
         failures++;
      }
      free(out);
      ck_sb_free(&m);
   }
   free(block);

   if (failures)
   {
      printf("\nSelftest failed: the checker is not trustworthy.\n");
      return 1;
   }
   printf("\nSelftest passed.\n");
   return 0;
}

static int usage(void)
{
   fprintf(stderr, "usage: rgui_pixfmt_check [--selftest] [--root DIR]"
         " [--cc CC]\n");
   return 2;
}

int main(int argc, char **argv)
{
   int a, do_selftest = 0, st;
   const char *cc = getenv("CC");
   char *block, *out;

   if (cc && *cc)
      g_cc = cc;
   if (!(g_tools = getenv("RA_TREE_ROOT")) || !*g_tools)
      g_tools = ".";
   {
      struct ck_sb t = {0};
      ck_sb_puts(&t, g_tools);
      ck_sb_puts(&t, "/tools");
      g_tools = t.p;
   }
   for (a = 1; a < argc; a++)
   {
      const char *v = NULL;
      if (!strcmp(argv[a], "--selftest"))
         do_selftest = 1;
      else if (!strcmp(argv[a], "--root") || !strcmp(argv[a], "--cc"))
      {
         if (a + 1 >= argc)
            return usage();
         v = argv[a + 1];
         if (argv[a][2] == 'r')
            g_root = v;
         else
            g_cc = v;
         a++;
      }
      else if (!strncmp(argv[a], "--root=", 7))
         g_root = argv[a] + 7;
      else if (!strncmp(argv[a], "--cc=", 5))
         g_cc = argv[a] + 5;
      else
         return usage();
   }

   if (do_selftest)
      return selftest();

   block = extract_block();
   st    = run_block(block, &out);
   fputs(out, stdout);
   if (st != 0)
      printf("\nSee tools/rgui_pixfmt_check.c for what these mean.\n");
   free(out);
   free(block);
   return st;
}
