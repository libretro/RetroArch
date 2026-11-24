/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (jni_copy_test.c).
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

/* The VFS copy with SAF ends, built for the host with the VFS compiled
 * as on Android and the provider handing out real descriptors over
 * files in build/backing. Copies to, from and between SAF documents
 * must come out byte-exact; the Makefile checks the copy debug log for
 * the kernel path having carried them. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <jni.h>
#include <vfs/vfs_implementation.h>
#include <vfs/vfs_implementation_saf.h>

#define TREE    "content://com.android.externalstorage.documents/tree/primary%3ATree"
#define BACKING "build/backing/"
#define SIZE    (3 * 1024 * 1024 + 123)

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

static unsigned char *slurp(const char *path, long *len)
{
   FILE          *f = fopen(path, "rb");
   unsigned char *buf;
   if (!f)
      return NULL;
   fseek(f, 0, SEEK_END);
   *len = ftell(f);
   fseek(f, 0, SEEK_SET);
   buf = (unsigned char *)malloc(*len > 0 ? (size_t)*len : 1);
   if (buf && fread(buf, 1, (size_t)*len, f) != (size_t)*len)
   {
      free(buf);
      buf = NULL;
   }
   fclose(f);
   return buf;
}

static int same_as(const unsigned char *want, const char *path)
{
   long           len = 0;
   unsigned char *got = slurp(path, &len);
   int            ok  = got && len == SIZE && !memcmp(got, want, SIZE);
   free(got);
   return ok;
}

static int copy(const char *src, const char *dst)
{
   int64_t done, total;
   int     status;
   struct retro_vfs_copy_handle *h = retro_vfs_copy_begin_impl(src, dst, 0);
   if (!h)
      return 0;
   /* Several steps, so the kernel path resumes from its own offsets */
   while ((status = retro_vfs_copy_step_impl(h, 1024 * 1024, &done, &total)) == RETRO_VFS_COPY_RUNNING);
   retro_vfs_copy_close_impl(h);
   return status == RETRO_VFS_COPY_DONE && done == SIZE;
}

int main(void)
{
   JavaVM        *vm;
   JavaVMInitArgs args;
   JavaVMOption   options[3];
   jclass         cls;
   jmethodID      ctor;
   jobject        activity;
   unsigned char *data;
   FILE          *f;
   char          *saf_src, *saf_dst, *saf_dst2;
   long           i;

   if (!(data = (unsigned char *)malloc(SIZE)))
      return 1;
   for (i = 0; i < SIZE; i++)
      data[i] = (unsigned char)((i * 2654435761u) >> 13);
   if (!(f = fopen(BACKING "primary_Tree_src.bin", "wb")) || fwrite(data, 1, SIZE, f) != SIZE)
   {
      printf("[FAILED] fixture\n");
      return 1;
   }
   fclose(f);
   if (!(f = fopen("build/native_src.bin", "wb")) || fwrite(data, 1, SIZE, f) != SIZE)
   {
      printf("[FAILED] fixture\n");
      return 1;
   }
   fclose(f);
   remove("build/native_dst.bin");

   options[0].optionString = (char *)"-Djava.class.path=build";
   options[1].optionString = (char *)"-Dsaf.backing=" BACKING;
   /* FakeProvider takes the descriptor number out of a FileDescriptor */
   options[2].optionString = (char *)"--add-opens=java.base/java.io=ALL-UNNAMED";
   options[0].extraInfo    = options[1].extraInfo = options[2].extraInfo = NULL;
   args.version            = JNI_VERSION_1_6;
   args.nOptions           = 3;
   args.options            = options;
   args.ignoreUnrecognized = JNI_FALSE;
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
   check(retro_vfs_init_saf(get_env, activity), "init");

   saf_src  = retro_vfs_path_join_saf(TREE, "src.bin");
   saf_dst  = retro_vfs_path_join_saf(TREE, "copy.bin");
   saf_dst2 = retro_vfs_path_join_saf(TREE, "empty/copy2.bin");

   check(copy("build/native_src.bin", saf_dst) && same_as(data, BACKING "primary_Tree_copy.bin"),
         "copy from a file into a SAF document");
   check(copy(saf_src, "build/native_dst.bin") && same_as(data, "build/native_dst.bin"),
         "copy from a SAF document into a file");
   check(copy(saf_src, saf_dst2) && same_as(data, BACKING "primary_Tree_empty_copy2.bin"),
         "copy between SAF documents");

   free(saf_src);
   free(saf_dst);
   free(saf_dst2);
   free(data);
   retro_vfs_deinit_saf();
   (*vm)->DestroyJavaVM(vm);

   if (failures)
   {
      printf("%d SAF copy test(s) failed\n", failures);
      return 1;
   }
   printf("All SAF copy tests passed.\n");
   return 0;
}
