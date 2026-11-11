/* Regression test for path_remove_tree() and path_copy_tree(), on a
 * real tree in a scratch directory under the working directory.
 *
 * What it pins:
 *   copy        - files at every depth, a hidden file and an empty
 *                 directory arrive, byte for byte; a trailing
 *                 separator on either path changes nothing.
 *   overwrite   - copying again replaces changed files and leaves
 *                 anything else at the destination.
 *   one file    - a file copies to a file.
 *   remove      - the whole tree goes, and so does a single file;
 *                 nothing at the path counts as removed.
 *   links       - (POSIX) a link to a directory outside the tree is
 *                 removed, and what it points to is left alone.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !defined(_WIN32)
#include <unistd.h>
#endif

#include <boolean.h>
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

#define ROOT "path_tree_scratch"

static void write_file(const char *path, const char *text)
{
   FILE *f = fopen(path, "wb");
   if (f)
   {
      fputs(text, f);
      fclose(f);
   }
}

/* True if @path holds exactly @text */
static bool file_is(const char *path, const char *text)
{
   char   buf[256];
   size_t n;
   FILE  *f = fopen(path, "rb");
   if (!f)
      return false;
   n = fread(buf, 1, sizeof(buf), f);
   fclose(f);
   return n == strlen(text) && memcmp(buf, text, n) == 0;
}

static void make_tree(void)
{
   path_mkdir(ROOT "/src/a/b/c");
   path_mkdir(ROOT "/src/empty");
   write_file(ROOT "/src/top.txt", "top");
   write_file(ROOT "/src/.hidden", "hidden");
   write_file(ROOT "/src/a/one.txt", "one");
   write_file(ROOT "/src/a/b/two.txt", "two");
   write_file(ROOT "/src/a/b/c/three.bin", "three\001\002");
}

static void test_copy(void)
{
   CHECK(path_copy_tree(ROOT "/src", ROOT "/dst"), "copy: tree copied");
   CHECK(file_is(ROOT "/dst/top.txt", "top"), "copy: top level file");
   CHECK(file_is(ROOT "/dst/.hidden", "hidden"), "copy: hidden file");
   CHECK(file_is(ROOT "/dst/a/one.txt", "one"), "copy: depth 1");
   CHECK(file_is(ROOT "/dst/a/b/two.txt", "two"), "copy: depth 2");
   CHECK(file_is(ROOT "/dst/a/b/c/three.bin", "three\001\002"),
         "copy: depth 3, binary");
   CHECK(path_is_directory(ROOT "/dst/empty"), "copy: empty directory");

   CHECK(path_copy_tree(ROOT "/src/", ROOT "/dst2/"),
         "copy: trailing separators");
   CHECK(file_is(ROOT "/dst2/a/b/two.txt", "two"),
         "copy: trailing separators, depth 2");
}

static void test_overwrite(void)
{
   write_file(ROOT "/src/a/one.txt", "changed");
   write_file(ROOT "/dst/extra.txt", "extra");
   CHECK(path_copy_tree(ROOT "/src", ROOT "/dst"), "overwrite: copied again");
   CHECK(file_is(ROOT "/dst/a/one.txt", "changed"),
         "overwrite: changed file replaced");
   CHECK(file_is(ROOT "/dst/extra.txt", "extra"),
         "overwrite: other file left");
   CHECK(path_copy_tree(ROOT "/src/top.txt", ROOT "/top_copy.txt"),
         "one file: copied");
   CHECK(file_is(ROOT "/top_copy.txt", "top"), "one file: contents");
}

static void test_remove(void)
{
   CHECK(path_remove_tree(ROOT "/dst"), "remove: tree removed");
   CHECK(!path_is_valid(ROOT "/dst"), "remove: nothing left");
   CHECK(path_remove_tree(ROOT "/dst2/"), "remove: trailing separator");
   CHECK(!path_is_valid(ROOT "/dst2"), "remove: trailing separator, gone");
   CHECK(path_remove_tree(ROOT "/top_copy.txt"), "remove: one file");
   CHECK(!path_is_valid(ROOT "/top_copy.txt"), "remove: one file, gone");
   CHECK(path_remove_tree(ROOT "/missing"), "remove: nothing there");
}

#if !defined(_WIN32)
static void test_links(void)
{
   path_mkdir(ROOT "/outside");
   write_file(ROOT "/outside/keep.txt", "keep");
   path_mkdir(ROOT "/linked");
   write_file(ROOT "/linked/file.txt", "x");
   if (symlink("../outside", ROOT "/linked/link") != 0)
   {
      printf("skip links: no symlink\n");
      return;
   }
   CHECK(path_remove_tree(ROOT "/linked"), "links: tree with link removed");
   CHECK(!path_is_valid(ROOT "/linked"), "links: tree gone");
   CHECK(file_is(ROOT "/outside/keep.txt", "keep"),
         "links: what the link pointed to is left");
}
#endif

int main(void)
{
   path_remove_tree(ROOT);
   make_tree();
   test_copy();
   test_overwrite();
   test_remove();
#if !defined(_WIN32)
   test_links();
#endif
   CHECK(path_remove_tree(ROOT), "cleanup: scratch removed");
   CHECK(!path_is_valid(ROOT), "cleanup: scratch gone");

   if (failures)
   {
      printf("\n%u check(s) failed\n", failures);
      return 1;
   }
   printf("\nall checks passed\n");
   return 0;
}
