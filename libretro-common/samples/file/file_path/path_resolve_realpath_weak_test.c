/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (path_resolve_realpath_weak_test.c).
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

/* path_resolve_realpath_weak(): the existing leading part of a path
 * resolved with its symlinks, the rest (not on disk yet) appended and
 * resolved by its text.  A path into a symlinked directory is judged by
 * where that directory really is: what an installer checks before it
 * writes a file it was told the name of. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#else
#include <unistd.h>
#include <sys/stat.h>
#endif

#include <compat/strl.h>
#include <file/file_path.h>
#include <retro_miscellaneous.h>

static int failures = 0;

static void expect(const char *in, const char *want)
{
   char buf[PATH_MAX_LENGTH];
   strlcpy(buf, in, sizeof(buf));
   if (!path_resolve_realpath_weak(buf, sizeof(buf)))
   {
      printf("[FAIL] '%s': NULL, wanted '%s'\n", in, want);
      failures++;
   }
   else if (strcmp(buf, want))
   {
      printf("[FAIL] '%s': '%s', wanted '%s'\n", in, buf, want);
      failures++;
   }
   else
      printf("[ok]   '%s' -> '%s'\n", in, buf);
}

int main(void)
{
#ifndef _WIN32
   char dir[]  = "/tmp/realpath_weak_XXXXXX";
   char real_dir[PATH_MAX_LENGTH];
   char real_tmp[PATH_MAX_LENGTH];
   char cwd[PATH_MAX_LENGTH];
   char in[PATH_MAX_LENGTH * 2];
   char want[PATH_MAX_LENGTH * 2];
   char *r;

   if (!mkdtemp(dir) || !(r = realpath(dir, NULL)))
      return 1;
   strlcpy(real_dir, r, sizeof(real_dir));
   free(r);
   if (!(r = realpath("/tmp", NULL)))
      return 1;
   strlcpy(real_tmp, r, sizeof(real_tmp));
   free(r);
   if (!getcwd(cwd, sizeof(cwd)))
      return 1;

   snprintf(in, sizeof(in), "%s/inside", dir);
   mkdir(in, 0700);
   snprintf(in, sizeof(in), "%s/link", dir);
   if (symlink("/tmp", in))
      return 1;

   /* A file not there yet, in a directory that is */
   snprintf(in, sizeof(in), "%s/inside/new/file", dir);
   snprintf(want, sizeof(want), "%s/inside/new/file", real_dir);
   expect(in, want);

   /* Into a symlinked directory: where it really is, outside */
   snprintf(in, sizeof(in), "%s/link/new", dir);
   snprintf(want, sizeof(want), "%s/new", real_tmp);
   expect(in, want);

   /* ".." in the part that does not exist, by its text */
   snprintf(in, sizeof(in), "%s/nope/../x", dir);
   snprintf(want, sizeof(want), "%s/x", real_dir);
   expect(in, want);

   /* Repeated slashes and "." */
   snprintf(in, sizeof(in), "%s//inside/./y", dir);
   snprintf(want, sizeof(want), "%s/inside/y", real_dir);
   expect(in, want);

   /* Only the root exists */
   expect("/realpath_weak_missing/a", "/realpath_weak_missing/a");

   /* Relative, none of it there: rebased on the working directory */
   snprintf(want, sizeof(want), "%s/realpath_weak_rel/a", cwd);
   expect("realpath_weak_rel/a", want);

   snprintf(in, sizeof(in), "%s/link", dir);
   unlink(in);
   snprintf(in, sizeof(in), "%s/inside", dir);
   rmdir(in);
   rmdir(dir);
#else
   /* Windows: the existing directory as the file system names it, the
    * part not there yet appended, ".." in it resolved by its text */
   char tmp_dir[MAX_PATH];
   char dir[PATH_MAX_LENGTH];
   char base[PATH_MAX_LENGTH];
   char in[PATH_MAX_LENGTH * 2];
   char want[PATH_MAX_LENGTH * 2];

   if (!GetTempPathA(sizeof(tmp_dir), tmp_dir))
      return 1;
   snprintf(dir, sizeof(dir), "%srealpath_weak_test", tmp_dir);
   _mkdir(dir);
   strlcpy(base, dir, sizeof(base));
   if (!path_resolve_realpath_weak(base, sizeof(base)))
   {
      printf("[FAIL] '%s': NULL\n", dir);
      return 1;
   }

   snprintf(in, sizeof(in), "%s\\new\\file", dir);
   snprintf(want, sizeof(want), "%s\\new\\file", base);
   expect(in, want);

   snprintf(in, sizeof(in), "%s\\nope\\..\\x", dir);
   snprintf(want, sizeof(want), "%s\\x", base);
   expect(in, want);

   snprintf(in, sizeof(in), "%s/./y", dir);
   snprintf(want, sizeof(want), "%s\\y", base);
   expect(in, want);

   /* A directory given by its 8.3 name comes back by its long one */
   snprintf(in, sizeof(in), "%s\\Long Directory Name", dir);
   _mkdir(in);
   {
      char shrt[MAX_PATH];
      DWORD n = GetShortPathNameA(in, shrt, sizeof(shrt));
      if (n && n < sizeof(shrt) && strcmp(shrt, in))
      {
         snprintf(in, sizeof(in), "%s\\f", shrt);
         snprintf(want, sizeof(want), "%s\\Long Directory Name\\f", base);
         expect(in, want);
      }
      else
         printf("[skip] no 8.3 name for '%s'\n", in);
   }
   snprintf(in, sizeof(in), "%s\\Long Directory Name", dir);
   _rmdir(in);

   _rmdir(dir);
#endif

   if (failures)
   {
      printf("\n%d test(s) failed\n", failures);
      return 1;
   }
   printf("\nAll path_resolve_realpath_weak tests passed.\n");
   return 0;
}
