/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (jni_test.c).
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

/* vfs_implementation_saf.c, built for the host, driving the real Java
 * class in a JVM over FakeProvider: every method ID it looks up at init
 * and every JNI call it makes afterwards. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <jni.h>
#include <vfs/vfs_implementation_saf.h>

#define TREE "content://com.android.externalstorage.documents/tree/primary%3ATree"

static JNIEnv *test_env;
static int     failures;

static JNIEnv *get_env(void)
{
   return test_env;
}

static void check(int ok, const char *what)
{
   printf("%s %s\n", ok ? "[SUCCESS]" : "[FAILED]", what);
   if (!ok)
      failures++;
}

static void test_stat(void)
{
   int64_t size = -1;
   int     r    = retro_vfs_stat_saf(TREE, "/big.bin", &size);
   check(r == RETRO_VFS_STAT_IS_VALID && size == 5000000000LL, "stat of a file gives its size");
   r = retro_vfs_stat_saf(TREE, "/roms", NULL);
   check(r == (RETRO_VFS_STAT_IS_VALID | RETRO_VFS_STAT_IS_DIRECTORY), "stat of a directory");
   size = 7;
   r = retro_vfs_stat_saf(TREE, "/missing", &size);
   check(r == 0 && size == 7, "stat of a missing path is 0 and leaves the size alone");
}

static void test_readdir(void)
{
   libretro_vfs_implementation_saf_dir *d = retro_vfs_opendir_saf(TREE, "/roms", false);
   unsigned count = 0, games = 0;
   int      odd = 0, sub = 0, order = 1;
   char     want[32];

   check(d != NULL, "opendir");
   if (!d)
      return;
   while (retro_vfs_readdir_saf(d))
   {
      const char *name = retro_vfs_dirent_get_name_saf(d);
      bool        dir  = retro_vfs_dirent_is_dir_saf(d);
      count++;
      if (!strcmp(name, "sub"))
         sub = dir;
      /* U+1F3AE as the two surrogates GetStringUTFChars encodes, NUL as C0 80 */
      else if (!strcmp(name, "caf\xc3\xa9 \xed\xa0\xbc\xed\xbe\xae \xc0\x80x"))
         odd = !dir;
      else
      {
         sprintf(want, "game%03u.sfc", games % 1000u);
         if (strcmp(name, want) || dir)
            order = 0;
         games++;
      }
   }
   check(count == 302 && games == 300, "readdir walks every child across batches");
   check(order, "readdir keeps names intact across batch boundaries");
   check(odd, "readdir hands names over in modified UTF-8");
   check(sub, "readdir flags directories");
   check(!retro_vfs_readdir_saf(d) && retro_vfs_dirent_get_name_saf(d) == NULL, "readdir stays exhausted");
   check(retro_vfs_closedir_saf(d) == 0, "closedir");

   d = retro_vfs_opendir_saf(TREE, "/empty", false);
   check(d && !retro_vfs_readdir_saf(d), "readdir of an empty directory");
   if (d)
      retro_vfs_closedir_saf(d);
}

static void test_mutations(void)
{
   int64_t size = -1;
   check(retro_vfs_mkdir_saf(TREE, "/newdir") == 0, "mkdir");
   check(retro_vfs_mkdir_saf(TREE, "/newdir") == -2, "mkdir of an existing directory");
   /* The descriptor is the fake provider's number, not one to close */
   check(retro_vfs_file_open_saf(TREE, "/newdir/new.srm", RETRO_VFS_FILE_ACCESS_WRITE) >= 0, "open for write creates");
   check(retro_vfs_file_open_saf(TREE, "/missing.srm", RETRO_VFS_FILE_ACCESS_READ) == -1, "open for read of a missing file");
   check(retro_vfs_file_rename_saf(TREE, "/newdir/new.srm", TREE, "/newdir/old.srm") == 0, "rename");
   check(retro_vfs_stat_saf(TREE, "/newdir/old.srm", &size) == RETRO_VFS_STAT_IS_VALID && size == 0, "renamed file is there");
   check(retro_vfs_file_remove_saf(TREE, "/newdir/old.srm") == 0, "remove");
   check(retro_vfs_stat_saf(TREE, "/newdir/old.srm", NULL) == 0, "removed file is gone");
}

int main(void)
{
   JavaVM        *vm;
   JavaVMInitArgs args;
   JavaVMOption   option;
   jclass         cls;
   jmethodID      ctor;
   jobject        activity;

   option.optionString       = (char *)"-Djava.class.path=build";
   option.extraInfo          = NULL;
   args.version              = JNI_VERSION_1_6;
   args.nOptions             = 1;
   args.options              = &option;
   args.ignoreUnrecognized   = JNI_FALSE;
   if (JNI_CreateJavaVM(&vm, (void **)&test_env, &args) != JNI_OK)
   {
      printf("[FAILED] JNI_CreateJavaVM\n");
      return 1;
   }

   if (     !(cls = (*test_env)->FindClass(test_env, "FakeActivity"))
         || !(ctor = (*test_env)->GetMethodID(test_env, cls, "<init>", "()V"))
         || !(activity = (*test_env)->NewObject(test_env, cls, ctor)))
   {
      (*test_env)->ExceptionDescribe(test_env);
      printf("[FAILED] FakeActivity\n");
      return 1;
   }

   check(retro_vfs_init_saf(get_env, activity), "init finds every class and method it uses");
   if (!failures)
   {
      test_stat();
      test_readdir();
      test_mutations();
      check(retro_vfs_deinit_saf(), "deinit");
   }

   (*vm)->DestroyJavaVM(vm);

   if (failures)
   {
      printf("%d SAF JNI test(s) failed\n", failures);
      return 1;
   }
   printf("All SAF JNI tests passed.\n");
   return 0;
}
