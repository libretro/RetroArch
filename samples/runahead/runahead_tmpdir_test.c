/* runahead_tmp_dir_private(), extracted verbatim from runahead.c,
 * against real directories: the run-ahead copy of a core goes only
 * into a directory of ours that no one else could have written to. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define RARCH_WARN(...) do { } while (0)

#include "runahead_tmpdir_fragment.c"

static unsigned failures;
#define CHECK(c, ...) do { if (!(c)) { failures++; printf("   FAIL "); \
   printf(__VA_ARGS__); printf("\n"); } } while (0)

static char base[64];
static char path[128];

static const char *at(const char *name)
{
   snprintf(path, sizeof(path), "%s/%s", base, name);
   return path;
}

static mode_t mode_of(const char *p)
{
   struct stat st;
   return lstat(p, &st) == 0 ? (st.st_mode & 0777) : 0;
}

static void make_dir(const char *name, mode_t mode)
{
   mkdir(at(name), 0700);
   chmod(at(name), mode);
}

int main(void)
{
   char target[128];

   strcpy(base, "/tmp/ra_tmpdir_XXXXXX");
   if (!mkdtemp(base))
      return 2;

   make_dir("own", 0700);
   CHECK(runahead_tmp_dir_private(at("own")), "a 0700 directory of ours refused");
   printf("   ok   a directory of ours, closed to others, is used\n");

   make_dir("readable", 0755);
   CHECK(runahead_tmp_dir_private(at("readable")), "a 0755 directory of ours refused");
   CHECK(mode_of(at("readable")) == 0700, "left at %o, want 700", mode_of(at("readable")));
   printf("   ok   one others can only read is used, and closed to them\n");

   make_dir("upg", 0770);
   CHECK(runahead_tmp_dir_private(at("upg")), "a group-writable directory in our own group refused");
   printf("   ok   group-writable in our own group is used\n");

   make_dir("world", 0777);
   CHECK(!runahead_tmp_dir_private(at("world")), "a world-writable directory used");
   CHECK(mode_of(at("world")) == 0777, "a refused directory's mode changed");
   printf("   ok   one anyone could have written to is refused, as it is\n");

   make_dir("real", 0700);
   snprintf(target, sizeof(target), "%s/real", base);
   CHECK(symlink(target, at("link")) == 0, "symlink fixture");
   CHECK(!runahead_tmp_dir_private(at("link")), "a link to a directory used");
   printf("   ok   a link to a directory is refused\n");

   fclose(fopen(at("file"), "w"));
   CHECK(!runahead_tmp_dir_private(at("file")), "a regular file used");
   CHECK(!runahead_tmp_dir_private(at("missing")), "a missing directory used");
   printf("   ok   a file, or nothing, is refused\n");

   if (geteuid() == 0)
   {
      make_dir("theirs", 0700);
      CHECK(chown(at("theirs"), 65534, 65534) == 0, "chown fixture");
      CHECK(!runahead_tmp_dir_private(at("theirs")), "another user's directory used");
      make_dir("group", 0770);
      CHECK(chown(at("group"), 0, 65534) == 0, "chgrp fixture");
      CHECK(!runahead_tmp_dir_private(at("group")), "a directory another group can write used");
      printf("   ok   another user's, or one another group can write, is refused\n");
   }
   else
      printf("   skip another owner and another group need root to set up\n");

   {
      char cmd[96];
      snprintf(cmd, sizeof(cmd), "rm -rf %s", base);
      if (system(cmd) != 0) { }
   }
   if (failures)
   {
      printf("FAIL runahead_tmpdir_test: %u failure(s)\n", failures);
      return 1;
   }
   printf("PASS runahead_tmpdir_test\n");
   return 0;
}
