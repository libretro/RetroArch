/* Regression test for path_mkdir() on a directory directly under a
 * device root ("ms0:/name", "host:name").  A fake VFS installed through
 * path_vfs_init() models a console namespace: the device root itself
 * cannot be stat'ed, as the PSP firmware refuses a stat of "ms0:/",
 * and every stat and mkdir is recorded.
 *
 * What it pins:
 *   device root - "ms0:/x" and "host:x" are created with one mkdir of
 *                 the path itself; nothing relative ("./") is probed.
 *   chain       - "ms0:/a/b/c" creates a, a/b and a/b/c, in order.
 *   existing    - an existing directory costs no mkdir.
 *   unaffected  - URL schemes and plain paths recurse as before.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <boolean.h>
#include <libretro.h>
#include <file/file_path.h>

static unsigned failures = 0;

#define CHECK(cond, msg) \
   do { \
      if (!(cond)) \
      { \
         printf("FAIL %s\n", msg); \
         failures++; \
      } \
      else \
         printf("ok   %s\n", msg); \
   } while (0)

#define MAX_ENTRIES 32

static char dirs[MAX_ENTRIES][64];
static unsigned n_dirs;
static char mkdirs[MAX_ENTRIES][64];
static unsigned n_mkdirs;
static unsigned n_relative;

/* path_parent_dir() keeps a trailing separator, so "a" and "a/" name
 * the same directory here. */
static void norm(char *dst, const char *src)
{
   size_t n = strlen(src);
   if (n > 1 && (src[n - 1] == '/' || src[n - 1] == '\\'))
      n--;
   memcpy(dst, src, n);
   dst[n] = '\0';
}

static void reset(void)
{
   n_dirs     = 0;
   n_mkdirs   = 0;
   n_relative = 0;
}

static void add_dir(const char *p)
{
   if (n_dirs < MAX_ENTRIES)
      norm(dirs[n_dirs++], p);
}

static int is_dir(const char *p)
{
   char n[64];
   unsigned i;
   norm(n, p);
   for (i = 0; i < n_dirs; i++)
      if (!strcmp(dirs[i], n))
         return 1;
   return 0;
}

static int RETRO_CALLCONV fake_stat(const char *path, int32_t *size)
{
   if (size)
      *size = 0;
   if (!strchr(path, ':') && path[0] != '/')
      n_relative++;
   /* No stat of a device root, as on the PSP. */
   return is_dir(path)
      ? (RETRO_VFS_STAT_IS_VALID | RETRO_VFS_STAT_IS_DIRECTORY) : 0;
}

static int RETRO_CALLCONV fake_mkdir(const char *path)
{
   if (n_mkdirs < MAX_ENTRIES)
      norm(mkdirs[n_mkdirs++], path);
   if (!strchr(path, ':') && path[0] != '/')
      n_relative++;
   if (is_dir(path))
      return -2;
   add_dir(path);
   return 0;
}

static struct retro_vfs_interface fake_iface;

static void install(void)
{
   struct retro_vfs_interface_info info;
   memset(&fake_iface, 0, sizeof(fake_iface));
   fake_iface.stat  = fake_stat;
   fake_iface.mkdir = fake_mkdir;
   info.required_interface_version = 3;
   info.iface                      = &fake_iface;
   path_vfs_init(&info);
}

static int mkdirs_are(unsigned n, const char *a, const char *b,
      const char *c)
{
   const char *want[3];
   unsigned i;
   want[0] = a;
   want[1] = b;
   want[2] = c;
   if (n_mkdirs != n)
      return 0;
   for (i = 0; i < n; i++)
      if (strcmp(mkdirs[i], want[i]))
         return 0;
   return 1;
}

int main(void)
{
   install();

   reset();
   CHECK(path_mkdir("ms0:/vfstest")
         && mkdirs_are(1, "ms0:/vfstest", NULL, NULL) && !n_relative,
         "device root: ms0:/vfstest is one mkdir of itself");

   reset();
   CHECK(path_mkdir("host:vfstest")
         && mkdirs_are(1, "host:vfstest", NULL, NULL) && !n_relative,
         "device root: host:vfstest is one mkdir of itself");

   reset();
   CHECK(path_mkdir("ms0:/trail/")
         && mkdirs_are(1, "ms0:/trail", NULL, NULL) && !n_relative,
         "device root: a trailing separator is kept");

   reset();
   CHECK(path_mkdir("ms0:/a/b/c")
         && mkdirs_are(3, "ms0:/a", "ms0:/a/b", "ms0:/a/b/c")
         && !n_relative,
         "chain: ms0:/a/b/c creates each level in order");

   reset();
   add_dir("ms0:/a");
   CHECK(path_mkdir("ms0:/a") && n_mkdirs == 0,
         "existing: no mkdir for a directory that is there");

   reset();
   add_dir("smb://h/s");
   CHECK(path_mkdir("smb://h/s/x")
         && mkdirs_are(1, "smb://h/s/x", NULL, NULL),
         "unaffected: a URL scheme recurses to its existing parent");

   reset();
   add_dir("/r");
   CHECK(path_mkdir("/r/x") && mkdirs_are(1, "/r/x", NULL, NULL),
         "unaffected: a plain path recurses to its existing parent");

   if (failures)
   {
      printf("%u check(s) failed\n", failures);
      return 1;
   }
   printf("[pass] path_mkdir_test\n");
   return 0;
}
