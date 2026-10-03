/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (cloudsync_savefile_owner_test.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* Regression test for the save-data rollback that cloud sync could
 * cause at startup.
 *
 * A core loads its save RAM into memory when content starts and writes
 * that memory back over the file on disk when the content closes.  A
 * sync that replaces the file in between has its work overwritten from
 * core memory, and the stale result is then uploaded in place of the
 * server's copy.  save.c::content_savefile_is_live() is what cloud sync
 * asks before it touches a local file, and this test pins the two
 * properties the fix rests on:
 *
 *   1. Ownership tracks the save file list exactly - nothing is owned
 *      before the list exists or after the deinit chain frees it, and
 *      matching ignores case so a path that differs only in case still
 *      reads as owned.  A false negative here is the data loss.
 *
 *   2. file_list_search() resolves a full portable manifest key to its
 *      own entry even where a longer key shares it as a prefix.  The
 *      carry-forward in task_cloudsync.c uses it to recover the hash the
 *      last sync recorded, and settling on a neighbouring entry would
 *      write the wrong hash into the local manifest - which is exactly
 *      the state that makes the next diff miss the difference.
 *
 * content_savefile_is_live() reads a file-static list, so the predicate
 * is copied verbatim below rather than linked.  If save.c amends it, the
 * copy here must follow.  file_list_search() is the real one.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <lists/string_list.h>
#include <lists/file_list.h>
#include <lists/dir_list.h>
#include <string/stdstring.h>

/* Stands in for the file-static list in save.c, which
 * path_init_savefile_new() creates at content load and
 * path_deinit_savefile() frees once save RAM has been written out. */
static struct string_list *task_save_files = NULL;

/* Verbatim copy of save.c::content_savefile_is_live(). */
static bool content_savefile_is_live(const char *path)
{
   size_t i;

   if (!task_save_files || string_is_empty(path))
      return false;

   for (i = 0; i < task_save_files->size; i++)
      if (string_is_equal_noncase(task_save_files->elems[i].data, path))
         return true;

   return false;
}

static int failures = 0;

static void check(bool cond, const char *what)
{
   if (cond)
      printf("  [pass] %s\n", what);
   else
   {
      printf("  [FAIL] %s\n", what);
      failures++;
   }
}

static void test_ownership_window(void)
{
   union string_list_elem_attr attr;

   attr.i = 0;

   printf("ownership window\n");

   /* Before content load there is no list at all. */
   check(!content_savefile_is_live("/home/u/saves/foo.srm"),
         "nothing is owned before the save file list exists");
   check(!content_savefile_is_live(NULL), "a NULL path is not owned");

   task_save_files = string_list_new();
   check(!content_savefile_is_live("/home/u/saves/foo.srm"),
         "nothing is owned while the list is empty");

   string_list_append(task_save_files, "/home/u/saves/foo.srm", attr);
   string_list_append(task_save_files, "/home/u/saves/foo.srm.rtc", attr);

   check(content_savefile_is_live("/home/u/saves/foo.srm"),
         "save RAM of the running content is owned");
   check(content_savefile_is_live("/home/u/saves/foo.srm.rtc"),
         "the inferred RTC path is owned");
   check(content_savefile_is_live("/home/u/SAVES/FOO.SRM"),
         "a path differing only in case is owned");
   check(!content_savefile_is_live("/home/u/saves/bar.srm"),
         "another game's save RAM is not owned");
   check(!content_savefile_is_live("/home/u/saves/foo.sr"),
         "a prefix of an owned path is not owned");
   check(!content_savefile_is_live("/home/u/saves/foo.srm2"),
         "an owned path extended is not owned");
   check(!content_savefile_is_live("/home/u/states/foo.state"),
         "save states are not owned");
   check(!content_savefile_is_live(""), "an empty path is not owned");

   /* The deinit chain writes save RAM out and then frees the list, which
    * is what releases the files to the next sync. */
   string_list_free(task_save_files);
   task_save_files = NULL;

   check(!content_savefile_is_live("/home/u/saves/foo.srm"),
         "ownership is released once the list is torn down");
}

static void append_entry(file_list_t *list, size_t idx,
      const char *path, const char *key)
{
   file_list_append(list, path, NULL, 0, 0, 0);
   file_list_set_alt_at_offset(list, idx, key);
}

static void test_manifest_key_lookup(void)
{
   file_list_t *manifest;
   size_t       idx;

   printf("manifest key lookup\n");

   manifest = (file_list_t*)calloc(1, sizeof(*manifest));
   if (!manifest)
   {
      printf("  [FAIL] out of memory\n");
      failures++;
      return;
   }

   /* Manifests are sorted by portable key. */
   append_entry(manifest, 0, "/home/u/saves/foo.srm",     "saves/foo.srm");
   append_entry(manifest, 1, "/home/u/saves/foo.srm.bak", "saves/foo.srm.bak");
   append_entry(manifest, 2, "/home/u/states/foo.srm",    "states/foo.srm");

   idx = (size_t)-1;
   check(file_list_search(manifest, "saves/foo.srm", &idx) && idx == 0,
         "a key resolves to its own entry, not the longer one sharing its prefix");

   idx = (size_t)-1;
   check(file_list_search(manifest, "saves/foo.srm.bak", &idx) && idx == 1,
         "the longer key resolves to itself");

   idx = (size_t)-1;
   check(file_list_search(manifest, "states/foo.srm", &idx) && idx == 2,
         "an identical basename under another directory resolves to itself");

   check(!file_list_search(manifest, "saves/absent.srm", &idx),
         "a key with no entry is not found");
   check(!file_list_search(NULL, "saves/foo.srm", &idx),
         "a NULL manifest is handled, so the carry-forward needs no guard");

   file_list_free(manifest);
}


static void test_unlistable_directory_is_not_empty(void)
{
   struct string_list *list;
   char                empty_dir[] = "/tmp/cs_empty_XXXXXX";

   printf("directory listing\n");

   /* The manifest builder acts on the difference between these two.
    * An empty directory is a statement - every file that used to be
    * here is gone - and the diff answers it by removing the server's
    * copies. A directory that could not be read is not that statement,
    * so it has to be distinguishable or the sync deletes data over a
    * missing mount point or an unset path. */
   list = dir_list_new("/tmp/cs_no_such_directory_here", NULL,
         false, true, true, true);
   check(list == NULL,
         "a root that cannot be opened is reported as failure, not as empty");
   if (list)
      string_list_free(list);

   if (!mkdtemp(empty_dir))
   {
      printf("  [FAIL] could not create a temporary directory\n");
      failures++;
      return;
   }

   list = dir_list_new(empty_dir, NULL, false, true, true, true);
   check(list != NULL, "an empty directory lists successfully");
   if (list)
   {
      check(list->size == 0, "an empty directory lists no entries");
      string_list_free(list);
   }

   rmdir(empty_dir);
}

int main(void)
{
   test_ownership_window();
   test_manifest_key_lookup();
   test_unlistable_directory_is_not_empty();

   if (failures)
   {
      printf("FAILED: %d\n", failures);
      return 1;
   }

   printf("all checks passed\n");
   return 0;
}
