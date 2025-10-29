/* Copyright  (C) 2010-2020 The RetroArch team
*
* ---------------------------------------------------------------------------------------
* The following license statement only applies to this file (vfs_implementation_saf.c).
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

#if defined(ANDROID) && defined(HAVE_SAF)

#include <vfs/vfs_implementation_saf.h>
#include <stdlib.h>
#include <string.h>

static JNIEnv *(*vfs_saf_get_jni_env)(void) = NULL;
static jobject vfs_saf_content_resolver_object;
static jclass vfs_saf_vfs_implementation_saf_class;
static jmethodID vfs_saf_open_saf_file_method;
static jmethodID vfs_saf_remove_saf_file_method;
static jclass vfs_saf_saf_stat_class;
static jmethodID vfs_saf_saf_stat_init_method;
static jmethodID vfs_saf_saf_stat_get_is_open_method;
static jmethodID vfs_saf_saf_stat_get_is_directory_method;
static jmethodID vfs_saf_saf_stat_get_size_method;
static jmethodID vfs_saf_mkdir_saf_method;
static jclass vfs_saf_saf_directory_class;
static jmethodID vfs_saf_saf_directory_init_method;
static jmethodID vfs_saf_saf_directory_close_method;
static jmethodID vfs_saf_saf_directory_readdir_method;
static jmethodID vfs_saf_saf_directory_get_dirent_name_method;
static jmethodID vfs_saf_saf_directory_get_dirent_is_directory_method;

bool retro_vfs_init_saf(JNIEnv *(*get_jni_env)(void), jobject activity_object)
{
   JNIEnv *env;

   if (vfs_saf_get_jni_env != NULL && !retro_vfs_deinit_saf())
      return false;

   env = get_jni_env();
   if (env == NULL)
      return false;

   vfs_saf_content_resolver_object = NULL;
   vfs_saf_vfs_implementation_saf_class = NULL;
   vfs_saf_saf_stat_class = NULL;
   vfs_saf_saf_directory_class = NULL;

   /*
    * ClassLoader class_loader_object = activity_object.getClassLoader();
    * Class<?> vfs_saf_vfs_implementation_saf_class = class_loader_object.loadClass("com.libretro.common.vfs.VfsImplementationSaf");
    * Class<?> vfs_saf_saf_stat_class = class_loader_object.loadClass("com.libretro.common.vfs.VfsImplementationSaf$SafStat");
    * Class<?> vfs_saf_saf_directory_class = class_loader_object.loadClass("com.libretro.common.vfs.VfsImplementationSaf$SafDirectory");
    */
   {
      jobject class_loader_object;
      jmethodID load_class_method;
      jstring vfs_implementation_saf_string;
      jclass vfs_implementation_saf_class;
      jstring saf_stat_string;
      jclass saf_stat_class;
      jstring saf_directory_string;
      jclass saf_directory_class;

      {
         jclass activity_class;
         jclass class_loader_class;
         jmethodID get_class_loader_method;
         activity_class = (*env)->GetObjectClass(env, activity_object);
         if ((*env)->ExceptionOccurred(env)) goto error;
         get_class_loader_method = (*env)->GetMethodID(env, activity_class, "getClassLoader", "()Ljava/lang/ClassLoader;");
         if ((*env)->ExceptionOccurred(env)) goto error;
         class_loader_object = (*env)->CallObjectMethod(env, activity_object, get_class_loader_method);
         if ((*env)->ExceptionOccurred(env)) goto error;
         class_loader_class = (*env)->FindClass(env, "java/lang/ClassLoader");
         if ((*env)->ExceptionOccurred(env)) goto error;
         load_class_method = (*env)->GetMethodID(env, class_loader_class, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
         if ((*env)->ExceptionOccurred(env)) goto error;
      }

      vfs_implementation_saf_string = (*env)->NewStringUTF(env, "com.libretro.common.vfs.VfsImplementationSaf");
      if ((*env)->ExceptionOccurred(env)) goto error;
      vfs_implementation_saf_class = (jclass)(*env)->CallObjectMethod(env, class_loader_object, load_class_method, vfs_implementation_saf_string);
      if ((*env)->ExceptionOccurred(env)) goto error;
      vfs_implementation_saf_class = (jclass)(*env)->NewGlobalRef(env, vfs_implementation_saf_class);
      if ((*env)->ExceptionOccurred(env)) goto error;
      vfs_saf_vfs_implementation_saf_class = vfs_implementation_saf_class;

      saf_stat_string = (*env)->NewStringUTF(env, "com.libretro.common.vfs.VfsImplementationSaf$SafStat");
      if ((*env)->ExceptionOccurred(env)) goto error;
      saf_stat_class = (jclass)(*env)->CallObjectMethod(env, class_loader_object, load_class_method, saf_stat_string);
      if ((*env)->ExceptionOccurred(env)) goto error;
      saf_stat_class = (jclass)(*env)->NewGlobalRef(env, saf_stat_class);
      if ((*env)->ExceptionOccurred(env)) goto error;
      vfs_saf_saf_stat_class = saf_stat_class;

      saf_directory_string = (*env)->NewStringUTF(env, "com.libretro.common.vfs.VfsImplementationSaf$SafDirectory");
      if ((*env)->ExceptionOccurred(env)) goto error;
      saf_directory_class = (jclass)(*env)->CallObjectMethod(env, class_loader_object, load_class_method, saf_directory_string);
      if ((*env)->ExceptionOccurred(env)) goto error;
      saf_directory_class = (jclass)(*env)->NewGlobalRef(env, saf_directory_class);
      if ((*env)->ExceptionOccurred(env)) goto error;
      vfs_saf_saf_directory_class = saf_directory_class;
   }

   vfs_saf_open_saf_file_method = (*env)->GetStaticMethodID(env, vfs_saf_vfs_implementation_saf_class, "openSafFile", "(Landroid/content/ContentResolver;Ljava/lang/String;Ljava/lang/String;ZZZ)I");
   if ((*env)->ExceptionOccurred(env)) goto error;

   vfs_saf_remove_saf_file_method = (*env)->GetStaticMethodID(env, vfs_saf_vfs_implementation_saf_class, "removeSafFile", "(Landroid/content/ContentResolver;Ljava/lang/String;Ljava/lang/String;)Z");
   if ((*env)->ExceptionOccurred(env)) goto error;

   vfs_saf_saf_stat_init_method = (*env)->GetMethodID(env, vfs_saf_saf_stat_class, "<init>", "(Landroid/content/ContentResolver;Ljava/lang/String;Ljava/lang/String;Z)V");
   if ((*env)->ExceptionOccurred(env)) goto error;

   vfs_saf_saf_stat_get_is_open_method = (*env)->GetMethodID(env, vfs_saf_saf_stat_class, "getIsOpen", "()Z");
   if ((*env)->ExceptionOccurred(env)) goto error;

   vfs_saf_saf_stat_get_is_directory_method = (*env)->GetMethodID(env, vfs_saf_saf_stat_class, "getIsDirectory", "()Z");
   if ((*env)->ExceptionOccurred(env)) goto error;

   vfs_saf_saf_stat_get_size_method = (*env)->GetMethodID(env, vfs_saf_saf_stat_class, "getSize", "()J");
   if ((*env)->ExceptionOccurred(env)) goto error;

   vfs_saf_mkdir_saf_method = (*env)->GetStaticMethodID(env, vfs_saf_vfs_implementation_saf_class, "mkdirSaf", "(Landroid/content/ContentResolver;Ljava/lang/String;Ljava/lang/String;)I");
   if ((*env)->ExceptionOccurred(env)) goto error;

   vfs_saf_saf_directory_init_method = (*env)->GetMethodID(env, vfs_saf_saf_directory_class, "<init>", "(Landroid/content/ContentResolver;Ljava/lang/String;Ljava/lang/String;)V");
   if ((*env)->ExceptionOccurred(env)) goto error;

   vfs_saf_saf_directory_close_method = (*env)->GetMethodID(env, vfs_saf_saf_directory_class, "close", "()V");
   if ((*env)->ExceptionOccurred(env)) goto error;

   vfs_saf_saf_directory_readdir_method = (*env)->GetMethodID(env, vfs_saf_saf_directory_class, "readdir", "()Z");
   if ((*env)->ExceptionOccurred(env)) goto error;

   vfs_saf_saf_directory_get_dirent_name_method = (*env)->GetMethodID(env, vfs_saf_saf_directory_class, "getDirentName", "()Ljava/lang/String;");
   if ((*env)->ExceptionOccurred(env)) goto error;

   vfs_saf_saf_directory_get_dirent_is_directory_method = (*env)->GetMethodID(env, vfs_saf_saf_directory_class, "getDirentIsDirectory", "()Z");
   if ((*env)->ExceptionOccurred(env)) goto error;

   /*
    * ContentResolver vfs_saf_content_resolver_object = activity_object.getContentResolver();
    */
   {
      jclass activity_class;
      jmethodID content_resolver_method;
      jobject content_resolver_object;
      activity_class = (*env)->GetObjectClass(env, activity_object);
      if ((*env)->ExceptionOccurred(env)) goto error;
      content_resolver_method = (*env)->GetMethodID(env, activity_class, "getContentResolver", "()Landroid/content/ContentResolver;");
      if ((*env)->ExceptionOccurred(env)) goto error;
      content_resolver_object = (*env)->CallObjectMethod(env, activity_object, content_resolver_method);
      if ((*env)->ExceptionOccurred(env)) goto error;
      content_resolver_object = (*env)->NewGlobalRef(env, content_resolver_object);
      if ((*env)->ExceptionOccurred(env)) goto error;
      vfs_saf_content_resolver_object = content_resolver_object;
   }

   vfs_saf_get_jni_env = get_jni_env;
   return true;

error:
   (*env)->ExceptionDescribe(env);
   (*env)->ExceptionClear(env);
   if (vfs_saf_content_resolver_object != NULL)
   {
      (*env)->DeleteGlobalRef(env, vfs_saf_content_resolver_object);
      vfs_saf_content_resolver_object = NULL;
      if ((*env)->ExceptionOccurred(env)) goto error;
   }
   if (vfs_saf_vfs_implementation_saf_class != NULL)
   {
      (*env)->DeleteGlobalRef(env, vfs_saf_vfs_implementation_saf_class);
      vfs_saf_vfs_implementation_saf_class = NULL;
      if ((*env)->ExceptionOccurred(env)) goto error;
   }
   if (vfs_saf_saf_stat_class != NULL)
   {
      (*env)->DeleteGlobalRef(env, vfs_saf_saf_stat_class);
      vfs_saf_saf_stat_class = NULL;
      if ((*env)->ExceptionOccurred(env)) goto error;
   }
   if (vfs_saf_saf_directory_class != NULL)
   {
      (*env)->DeleteGlobalRef(env, vfs_saf_saf_directory_class);
      vfs_saf_saf_directory_class = NULL;
      if ((*env)->ExceptionOccurred(env)) goto error;
   }
   return false;
}

bool retro_vfs_deinit_saf(void)
{
   JNIEnv *env;

   if (vfs_saf_get_jni_env == NULL)
      return false;
   env = vfs_saf_get_jni_env();
   if (env == NULL)
      return false;

   (*env)->DeleteGlobalRef(env, vfs_saf_content_resolver_object);
   if ((*env)->ExceptionOccurred(env))
   {
      (*env)->ExceptionDescribe(env);
      (*env)->ExceptionClear(env);
   }

   (*env)->DeleteGlobalRef(env, vfs_saf_vfs_implementation_saf_class);
   if ((*env)->ExceptionOccurred(env))
   {
      (*env)->ExceptionDescribe(env);
      (*env)->ExceptionClear(env);
   }

   (*env)->DeleteGlobalRef(env, vfs_saf_saf_stat_class);
   if ((*env)->ExceptionOccurred(env))
   {
      (*env)->ExceptionDescribe(env);
      (*env)->ExceptionClear(env);
   }

   (*env)->DeleteGlobalRef(env, vfs_saf_saf_directory_class);
   if ((*env)->ExceptionOccurred(env))
   {
      (*env)->ExceptionDescribe(env);
      (*env)->ExceptionClear(env);
   }

   vfs_saf_get_jni_env = NULL;
   return true;
}

bool retro_vfs_path_split_saf(struct libretro_vfs_implementation_saf_path_split_result *out, const char *serialized_path)
{
   char *tree;
   char *tree_ptr;
   char *path;

   out->tree = NULL;
   out->path = NULL;

   if (strncmp(serialized_path, "saf://", sizeof "saf://" - 1) != 0)
      return false;
   serialized_path += sizeof "saf://" - 1;

   tree = (char *)malloc(strlen(serialized_path) + 1);
   if (tree == NULL)
      return false;
   tree_ptr = tree;

   while (*serialized_path != '/' && *serialized_path != 0)
   {
      if (
         *serialized_path == '%'
            && (
               (serialized_path[1] >= '0' && serialized_path[1] <= '9')
                  || (serialized_path[1] >= 'A' && serialized_path[1] <= 'F')
                  || (serialized_path[1] >= 'a' && serialized_path[1] <= 'f')
            ) && (
               (serialized_path[2] >= '0' && serialized_path[2] <= '9')
                  || (serialized_path[2] >= 'A' && serialized_path[2] <= 'F')
                  || (serialized_path[2] >= 'a' && serialized_path[2] <= 'f')
            ) && (
               serialized_path[1] != '0'
                  || serialized_path[2] != '0'
            )
      )
      {
         if (serialized_path[1] >= '0' && serialized_path[1] <= '9')
            *tree_ptr = (serialized_path[1] - '0') << 4;
         else if (serialized_path[1] >= 'A' && serialized_path[1] <= 'F')
            *tree_ptr = (serialized_path[1] - ('A' - 10)) << 4;
         else
            *tree_ptr = (serialized_path[1] - ('a' - 10)) << 4;
         if (serialized_path[2] >= '0' && serialized_path[2] <= '9')
            *tree_ptr |= serialized_path[2] - '0';
         else if (serialized_path[2] >= 'A' && serialized_path[2] <= 'F')
            *tree_ptr |= serialized_path[2] - ('A' - 10);
         else
            *tree_ptr |= serialized_path[2] - ('a' - 10);
         ++tree_ptr;
         serialized_path += 3;
      }
      else
         *tree_ptr++ = *serialized_path++;
   }

   *tree_ptr = 0;

   if (*serialized_path != 0)
      ++serialized_path;

   path = strdup(serialized_path);
   if (path == NULL)
   {
      free(tree);
      return false;
   }

   out->tree = tree;
   out->path = path;
   return true;
}

char *retro_vfs_path_join_saf(const char *tree, const char *path)
{
   size_t tree_length = strlen(tree);
   size_t path_length = strlen(path);
   char *serialized_path;
   char *serialized_path_ptr;

   if (3 * tree_length < tree_length || 3 * tree_length + path_length < 3 * tree_length || 3 * tree_length + path_length + (sizeof "saf://" - 1) + 2 < 3 * tree_length + path_length)
      return NULL;
   serialized_path = (char *)malloc(3 * tree_length + path_length + (sizeof "saf://" - 1) + 2);
   if (serialized_path == NULL)
      return NULL;
   serialized_path_ptr = serialized_path;

   memcpy(serialized_path_ptr, "saf://", sizeof "saf://" - 1);
   serialized_path_ptr += sizeof "saf://" - 1;

   for (; *tree != 0; ++tree)
      switch (*tree)
      {
         case '%':
            memcpy(serialized_path_ptr, "%25", 3);
            serialized_path_ptr += 3;
            break;
         case '/':
            memcpy(serialized_path_ptr, "%2F", 3);
            serialized_path_ptr += 3;
            break;
         default:
            *serialized_path_ptr++ = *tree;
            break;
      }

   if (*path != 0)
   {
      *serialized_path_ptr++ = '/';
      *serialized_path_ptr = 0;
      strcpy(serialized_path_ptr, path);
   }
   else
      *serialized_path_ptr = 0;

   return serialized_path;
}

int retro_vfs_file_open_saf(const char *tree, const char *path, unsigned mode)
{
   JNIEnv *env;
   jstring tree_object;
   jstring path_object;
   int fd;

   if (vfs_saf_get_jni_env == NULL)
      return -1;
   env = vfs_saf_get_jni_env();
   if (env == NULL)
      return -1;

   tree_object = (*env)->NewStringUTF(env, tree);
   if ((*env)->ExceptionOccurred(env)) goto error;

   path_object = (*env)->NewStringUTF(env, path);
   if ((*env)->ExceptionOccurred(env)) goto error;

   fd = (*env)->CallStaticIntMethod(env, vfs_saf_vfs_implementation_saf_class, vfs_saf_open_saf_file_method, vfs_saf_content_resolver_object, tree_object, path_object, !!(mode & RETRO_VFS_FILE_ACCESS_READ), !!(mode & RETRO_VFS_FILE_ACCESS_WRITE), !(mode & RETRO_VFS_FILE_ACCESS_UPDATE_EXISTING));
   if ((*env)->ExceptionOccurred(env)) goto error;

   return fd;

error:
   (*env)->ExceptionDescribe(env);
   (*env)->ExceptionClear(env);
   return -1;
}

int retro_vfs_file_remove_saf(const char *tree, const char *path)
{
   JNIEnv *env;
   jstring tree_object;
   jstring path_object;
   bool ret;

   if (vfs_saf_get_jni_env == NULL)
      return -1;
   env = vfs_saf_get_jni_env();
   if (env == NULL)
      return -1;

   tree_object = (*env)->NewStringUTF(env, tree);
   if ((*env)->ExceptionOccurred(env)) goto error;

   path_object = (*env)->NewStringUTF(env, path);
   if ((*env)->ExceptionOccurred(env)) goto error;

   ret = (*env)->CallStaticBooleanMethod(env, vfs_saf_vfs_implementation_saf_class, vfs_saf_remove_saf_file_method, vfs_saf_content_resolver_object, tree_object, path_object);
   if ((*env)->ExceptionOccurred(env)) goto error;

   return ret ? 0 : -1;

error:
   (*env)->ExceptionDescribe(env);
   (*env)->ExceptionClear(env);
   return -1;
}

int retro_vfs_file_rename_saf(const char *old_tree, const char *old_path, const char *new_tree, const char *new_path)
{
   /* TODO: implement */
   return -1;
}

int retro_vfs_stat_saf(const char *tree, const char *path, int32_t *size)
{
   JNIEnv *env;
   jstring tree_object;
   jstring path_object;
   jobject saf_stat;
   int64_t saf_stat_size;
   bool saf_stat_is_directory;

   if (vfs_saf_get_jni_env == NULL)
      return 0;
   env = vfs_saf_get_jni_env();
   if (env == NULL)
      return 0;

   tree_object = (*env)->NewStringUTF(env, tree);
   if ((*env)->ExceptionOccurred(env)) goto error;

   path_object = (*env)->NewStringUTF(env, path);
   if ((*env)->ExceptionOccurred(env)) goto error;

   saf_stat = (*env)->NewObject(env, vfs_saf_saf_stat_class, vfs_saf_saf_stat_init_method, vfs_saf_content_resolver_object, tree_object, path_object, size != NULL);
   if ((*env)->ExceptionOccurred(env)) goto error;

   if (size != NULL)
   {
      saf_stat_size = (*env)->CallLongMethod(env, saf_stat, vfs_saf_saf_stat_get_size_method);
      if ((*env)->ExceptionOccurred(env)) goto error;
      if (saf_stat_size < 0)
         return 0;
   }
   else
   {
      bool saf_stat_is_open = (*env)->CallBooleanMethod(env, saf_stat, vfs_saf_saf_stat_get_is_open_method);
      if ((*env)->ExceptionOccurred(env)) goto error;
      if (!saf_stat_is_open)
         return 0;
   }

   saf_stat_is_directory = (*env)->CallBooleanMethod(env, saf_stat, vfs_saf_saf_stat_get_is_directory_method);
   if ((*env)->ExceptionOccurred(env)) goto error;

   if (size != NULL)
      *size = saf_stat_size > INT32_MAX ? INT32_MAX : (int32_t)saf_stat_size;

   return saf_stat_is_directory ? RETRO_VFS_STAT_IS_VALID | RETRO_VFS_STAT_IS_DIRECTORY : RETRO_VFS_STAT_IS_VALID;

error:
   (*env)->ExceptionDescribe(env);
   (*env)->ExceptionClear(env);
   return 0;
}

int retro_vfs_mkdir_saf(const char *tree, const char *dir)
{
   JNIEnv *env;
   jstring tree_object;
   jstring path_object;
   int ret;

   if (vfs_saf_get_jni_env == NULL)
      return -1;
   env = vfs_saf_get_jni_env();
   if (env == NULL)
      return -1;

   tree_object = (*env)->NewStringUTF(env, tree);
   if ((*env)->ExceptionOccurred(env)) goto error;

   path_object = (*env)->NewStringUTF(env, dir);
   if ((*env)->ExceptionOccurred(env)) goto error;

   ret = (*env)->CallStaticIntMethod(env, vfs_saf_vfs_implementation_saf_class, vfs_saf_mkdir_saf_method, vfs_saf_content_resolver_object, tree_object, path_object);
   if ((*env)->ExceptionOccurred(env)) goto error;

   return ret;

error:
   (*env)->ExceptionDescribe(env);
   (*env)->ExceptionClear(env);
   return -1;
}

libretro_vfs_implementation_saf_dir *retro_vfs_opendir_saf(const char *tree, const char *dir, bool include_hidden)
{
   JNIEnv *env;
   libretro_vfs_implementation_saf_dir *dirstream;
   jstring tree_object;
   jstring path_object;

   if (vfs_saf_get_jni_env == NULL)
      return NULL;
   env = vfs_saf_get_jni_env();
   if (env == NULL)
      return NULL;

   dirstream = (libretro_vfs_implementation_saf_dir *)malloc(sizeof *dirstream);
   if (dirstream == NULL)
      return NULL;

   tree_object = (*env)->NewStringUTF(env, tree);
   if ((*env)->ExceptionOccurred(env)) goto error;

   path_object = (*env)->NewStringUTF(env, dir);
   if ((*env)->ExceptionOccurred(env)) goto error;

   dirstream->directory_object = (*env)->NewObject(env, vfs_saf_saf_directory_class, vfs_saf_saf_directory_init_method, vfs_saf_content_resolver_object, tree_object, path_object);
   if ((*env)->ExceptionOccurred(env)) goto error;

   dirstream->directory_object = (*env)->NewGlobalRef(env, dirstream->directory_object);
   if ((*env)->ExceptionOccurred(env)) goto error;

   dirstream->dirent_name_object = NULL;
   dirstream->dirent_name = NULL;
   dirstream->dirent_is_dir = false;
   return dirstream;

error:
   free(dirstream);
   (*env)->ExceptionDescribe(env);
   (*env)->ExceptionClear(env);
   return NULL;
}

bool retro_vfs_readdir_saf(libretro_vfs_implementation_saf_dir *dirstream)
{
   JNIEnv *env;
   bool ret;
   jobject dirent_name_object;
   const char *dirent_name;

   if (vfs_saf_get_jni_env == NULL)
      return false;
   env = vfs_saf_get_jni_env();
   if (env == NULL)
      return false;

   ret = (*env)->CallBooleanMethod(env, dirstream->directory_object, vfs_saf_saf_directory_readdir_method);
   if ((*env)->ExceptionOccurred(env)) goto error;

   if (dirstream->dirent_name != NULL)
   {
      (*env)->ReleaseStringUTFChars(env, dirstream->dirent_name_object, dirstream->dirent_name);
      if ((*env)->ExceptionOccurred(env)) goto error;
      dirstream->dirent_name = NULL;
   }
   if (dirstream->dirent_name_object != NULL)
   {
      (*env)->DeleteGlobalRef(env, dirstream->dirent_name_object);
      if ((*env)->ExceptionOccurred(env)) goto error;
      dirstream->dirent_name_object = NULL;
   }

   if (!ret)
   {
      dirstream->dirent_is_dir = false;
      return false;
   }

   dirstream->dirent_is_dir = (*env)->CallBooleanMethod(env, dirstream->directory_object, vfs_saf_saf_directory_get_dirent_is_directory_method);
   if ((*env)->ExceptionOccurred(env)) goto error;

   dirent_name_object = (*env)->CallObjectMethod(env, dirstream->directory_object, vfs_saf_saf_directory_get_dirent_name_method);
   if ((*env)->ExceptionOccurred(env)) goto error;

   dirent_name_object = (*env)->NewGlobalRef(env, dirent_name_object);
   if ((*env)->ExceptionOccurred(env)) goto error;
   dirstream->dirent_name_object = dirent_name_object;

   dirent_name = (*env)->GetStringUTFChars(env, (jstring)dirstream->dirent_name_object, NULL);
   if ((*env)->ExceptionOccurred(env)) goto error;
   dirstream->dirent_name = dirent_name;

   return true;

error:
   (*env)->ExceptionDescribe(env);
   (*env)->ExceptionClear(env);
   if (dirstream->dirent_name != NULL)
   {
      (*env)->ReleaseStringUTFChars(env, dirstream->dirent_name_object, dirstream->dirent_name);
      dirstream->dirent_name = NULL;
      if ((*env)->ExceptionOccurred(env)) goto error;
   }
   if (dirstream->dirent_name_object != NULL)
   {
      (*env)->DeleteGlobalRef(env, dirstream->dirent_name_object);
      dirstream->dirent_name_object = NULL;
      if ((*env)->ExceptionOccurred(env)) goto error;
   }
   dirstream->dirent_is_dir = false;
   return false;
}

const char *retro_vfs_dirent_get_name_saf(libretro_vfs_implementation_saf_dir *dirstream)
{
   return dirstream->dirent_name;
}

bool retro_vfs_dirent_is_dir_saf(libretro_vfs_implementation_saf_dir *dirstream)
{
   return dirstream->dirent_is_dir;
}

int retro_vfs_closedir_saf(libretro_vfs_implementation_saf_dir *dirstream)
{
   JNIEnv *env;

   if (dirstream == NULL)
      return -1;

   if (vfs_saf_get_jni_env == NULL)
      return -1;
   env = vfs_saf_get_jni_env();
   if (env == NULL)
      return -1;

   if (dirstream->dirent_name != NULL)
   {
      (*env)->ReleaseStringUTFChars(env, dirstream->dirent_name_object, dirstream->dirent_name);
      if ((*env)->ExceptionOccurred(env))
      {
         (*env)->ExceptionDescribe(env);
         (*env)->ExceptionClear(env);
      }
   }
   if (dirstream->dirent_name_object != NULL)
   {
      (*env)->DeleteGlobalRef(env, dirstream->dirent_name_object);
      if ((*env)->ExceptionOccurred(env))
      {
         (*env)->ExceptionDescribe(env);
         (*env)->ExceptionClear(env);
      }
   }

   (*env)->CallVoidMethod(env, dirstream->directory_object, vfs_saf_saf_directory_close_method);
   if ((*env)->ExceptionOccurred(env))
   {
      (*env)->ExceptionDescribe(env);
      (*env)->ExceptionClear(env);
   }

   (*env)->DeleteGlobalRef(env, dirstream->directory_object);
   if ((*env)->ExceptionOccurred(env))
   {
      (*env)->ExceptionDescribe(env);
      (*env)->ExceptionClear(env);
   }

   free(dirstream);
   return 0;
}

#endif
