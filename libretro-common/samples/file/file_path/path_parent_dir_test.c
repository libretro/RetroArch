/* Regression test for path_parent_dir() on VFS URL paths.
 *
 * The slashes of "scheme://" are not directory separators, so going
 * up from a scheme root ("nfs://host/") must end at "" (the drive
 * list), as "/" and "C:\" do, rather than walking through "nfs://",
 * "nfs:/" and "./" into the local working directory.
 *
 * What it pins:
 *   table       - each level up, ending in "" at a root.
 *   file        - a path with no trailing slash keeps "scheme://".
 *   menu        - the menu's Parent Directory shape (join, then
 *                 fill_pathname_parent_dir() twice).
 */

#include <stdio.h>
#include <string.h>

#include <compat/strl.h>
#include <file/file_path.h>

static unsigned failures = 0;

static void report(const char *what, const char *in, const char *got,
      size_t got_len, const char *want)
{
   if (strcmp(got, want) || got_len != strlen(want))
   {
      printf("FAIL %s(\"%s\") = \"%s\" (%u), want \"%s\"\n",
            what, in, got, (unsigned)got_len, want);
      failures++;
   }
   else
      printf("ok   %s(\"%s\") = \"%s\"\n", what, in, got);
}

static void check_parent(const char *in, const char *want)
{
   char s[64];
   size_t _len;
   strlcpy(s, in, sizeof(s));
   _len = path_parent_dir(s, strlen(s));
   report("path_parent_dir", in, s, _len, want);
}

static void check_basedir(const char *in, const char *want)
{
   char s[64];
   size_t _len;
   strlcpy(s, in, sizeof(s));
   _len = path_basedir(s);
   report("path_basedir", in, s, _len, want);
}

static void check_menu_up(const char *menu_path, const char *want)
{
   char tmp[128];
   char parent[128];
   size_t _len;
   fill_pathname_join_special(tmp, menu_path, "<Parent Directory>",
         sizeof(tmp));
   fill_pathname_parent_dir(parent, tmp, sizeof(parent));
   _len = fill_pathname_parent_dir(parent, parent, sizeof(parent));
   report("menu parent", menu_path, parent, _len, want);
}

int main(void)
{
   check_parent("nfs://host/export/dir/", "nfs://host/export/");
   check_parent("nfs://host/export/",     "nfs://host/");
   check_parent("nfs://host/",            "");
   check_parent("nfs://",                 "");
   check_parent("smb://srv/share/",       "smb://srv/");
   check_parent("smb://srv/",             "");
   check_parent("saf://tree/",            "");
   check_parent("/a/",                    "/");
   /* "://" inside a local folder name is not a scheme */
   check_parent("/home/a://b/",           "/home/a://");
   check_parent("/home/a://",             "/home/a:/");
   check_parent("/",                      "");
#ifdef _WIN32
   check_parent("C:\\",                   "");
#endif

   check_parent("cdrom://drive1.cue",     "cdrom://");
   check_basedir("cdrom://drive1.cue",    "cdrom://");

   check_menu_up("nfs://host",            "");
   check_menu_up("nfs://host/export",     "nfs://host/");

   if (failures)
   {
      printf("%u check(s) failed\n", failures);
      return 1;
   }
   printf("[pass] path_parent_dir_test\n");
   return 0;
}
