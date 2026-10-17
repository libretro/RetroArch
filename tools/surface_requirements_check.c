/* Producers ask the surface layer.
 *
 * What a producer should emit - channel order, whether a 10-bit source
 * is worth decoding - is the surface layer's answer
 * (gfx_surface_query_requirements), so that a decoder has one place to
 * ask instead of three unrelated flags to know about.
 *
 * The files listed below asked those flags directly before the query
 * existed, and are migrated one at a time; a file that is not on the
 * list may not start. That is the whole rule: this fails when a source
 * file outside the allowlist reads VIDEO_FLAG_USE_RGBA or
 * GFX_CTX_FLAGS_SCREEN_10BPC_SOURCE, and it fails just as loudly when a
 * file on the list has stopped reading them and nobody removed it - a
 * migration is not finished until the allowlist says so.
 *
 * The video drivers themselves are not producers: they are where the
 * flags are answered from, so they are not listed and not checked.
 *
 * A line that starts with a comment is not read.
 *
 * Usage: tools/check.sh surface_requirements_check [--selftest]
 *
 * --selftest writes a probe file into the tree's menu/ and removes it. */

#include "check_common.h"

#define COUNT(a) (sizeof(a) / sizeof((a)[0]))

/* written in two pieces so that this file is not a producer */
static const char *const flags[] = {
   "VIDEO_FLAG" "_USE_RGBA", "GFX_CTX_FLAGS" "_SCREEN_10BPC_SOURCE"
};

/* Where the flags are answered rather than consumed. */
static const char *const exempt_dirs[] = {
   "gfx/drivers/", "gfx/drivers_shader/", "gfx/drivers_context/",
   "samples/", "deps/"
};
static const char *const exempt_files[] = {
   "gfx/gfx_surface.c", "gfx/video_driver.c"
};

/* Producers that read the flags directly, each still to be migrated to
 * gfx_surface_query_requirements(). Shrinks; never grows.
 * retroarch.c clears the flag as part of tearing a driver down rather
 * than deciding what to decode. */
static const char *const allowlist[] = {
   "retroarch.c",
   NULL
};

struct state
{
   const char *root;
   struct ck_strv found;
};

static int skip_dir(const char *name, const char *rel, void *ud)
{
   (void)rel;
   (void)ud;
   return !strcmp(name, ".git") || !strcmp(name, "deps")
      || !strcmp(name, "media") || !strcmp(name, "pkg");
}

static int asks(const char *text)
{
   const char *line = text;
   while (line)
   {
      const char *nl = strchr(line, '\n');
      const char *e  = nl ? nl : line + strlen(line);
      const char *b  = line;
      size_t i;
      while (b < e && (CK_IS_SPACE(*b) || (*b >= 0x1c && *b <= 0x1f)))
         b++;
      if (!((e - b >= 2 && b[0] == '/' && (b[1] == '*' || b[1] == '/'))
               || (b < e && *b == '*')))
      {
         for (i = 0; i < COUNT(flags); i++)
         {
            size_t n = strlen(flags[i]);
            const char *p;
            for (p = b; (size_t)(e - p) >= n; p++)
            {
               if (*p == *flags[i] && !memcmp(p, flags[i], n))
                  return 1;
            }
         }
      }
      line = nl ? nl + 1 : NULL;
   }
   return 0;
}

static void source(const char *path, const char *rel, void *ud)
{
   struct state *st = (struct state*)ud;
   size_t i;
   char *text;
   if (!(ck_ends_with(rel, ".c") || ck_ends_with(rel, ".m")
            || ck_ends_with(rel, ".cpp")))
      return;
   for (i = 0; i < COUNT(exempt_files); i++)
   {
      if (!strcmp(rel, exempt_files[i]))
         return;
   }
   for (i = 0; i < COUNT(exempt_dirs); i++)
   {
      if (ck_starts_with(rel, exempt_dirs[i]))
         return;
   }
   if (!(text = ck_read_file(path, NULL)))
      return;
   if (asks(text))
      ck_strv_push(&st->found, ck_strdup(rel));
   free(text);
}

static int listed(const char *const *list, const char *extra, const char *rel)
{
   for (; *list; list++)
   {
      if (!strcmp(*list, rel))
         return 1;
   }
   return extra && !strcmp(extra, rel);
}

static int main_check(const char *root, const char *extra, int quiet)
{
   struct state st;
   struct ck_strv stale = {0};
   size_t i, nnew = 0;
   const char *const *a;

   st.root = root;
   memset(&st.found, 0, sizeof(st.found));
   ck_walk(root, skip_dir, source, &st);
   ck_strv_sort(&st.found);
   for (i = 0; i < st.found.n; i++)
   {
      if (listed(allowlist, extra, st.found.v[i]))
         continue;
      nnew++;
      if (!quiet)
         printf("%s asks the video flags directly; ask "
               "gfx_surface_query_requirements() instead\n", st.found.v[i]);
   }
   for (a = allowlist; ; a++)
   {
      const char *name = *a ? *a : extra;
      int seen = 0;
      if (!name)
         break;
      for (i = 0; i < st.found.n; i++)
         seen |= !strcmp(st.found.v[i], name);
      if (!seen)
         ck_strv_push(&stale, ck_strdup(name));
      if (!*a)
         break;
   }
   ck_strv_sort(&stale);
   for (i = 0; i < stale.n; i++)
   {
      if (!quiet)
         printf("%s no longer asks the video flags: drop it from the "
               "allowlist in tools/surface_requirements_check.c\n",
               stale.v[i]);
   }
   if (nnew || stale.n)
   {
      ck_strv_free(&st.found);
      ck_strv_free(&stale);
      return 1;
   }
   if (!quiet)
      printf("producers asking the video flags directly: %lu, all known\n",
            (unsigned long)st.found.n);
   ck_strv_free(&st.found);
   return 0;
}

/* A file outside the allowlist that asks must be caught, and an entry
 * whose file has stopped asking must be caught: the second is what
 * keeps the list shrinking as producers migrate. */
static int selftest(const char *root)
{
   struct ck_sb probe = {0};
   struct ck_sb text  = {0};
   int ok = 1;

   ck_sb_puts(&probe, root);
   ck_sb_puts(&probe, "/menu/surface_requirements_probe.c");
   ck_sb_puts(&text, "int probe = ");
   ck_sb_puts(&text, flags[0]);
   ck_sb_puts(&text, ";\n");
   ck_write_file(probe.p, text.p);
   if (main_check(root, NULL, 1) == 0)
   {
      printf("selftest: an unlisted producer was not caught\n");
      ok = 0;
   }
   remove(probe.p);
   if (main_check(root, "no/such/file.c", 1) == 0)
   {
      printf("selftest: a stale allowlist entry was not caught\n");
      ok = 0;
   }
   if (main_check(root, NULL, 1) != 0)
   {
      printf("selftest: the tree does not pass its own check\n");
      ok = 0;
   }
   if (ok)
      printf("selftest: unlisted producer and stale entry both caught\n");
   ck_sb_free(&probe);
   ck_sb_free(&text);
   return ok ? 0 : 1;
}

int main(int argc, char **argv)
{
   const char *root = getenv("RA_TREE_ROOT");
   int i;
   if (!root || !*root)
      root = ".";
   for (i = 1; i < argc; i++)
   {
      if (!strcmp(argv[i], "--selftest"))
         return selftest(root);
   }
   if (argc > 1)
   {
      fprintf(stderr, "usage: surface_requirements_check [--selftest]\n");
      return 2;
   }
   return main_check(root, NULL, 0);
}
