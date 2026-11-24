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
static jmethodID vfs_saf_rename_saf_file_method;
static jmethodID vfs_saf_stat_saf_method;
static jmethodID vfs_saf_mkdir_saf_method;
static jclass vfs_saf_saf_directory_class;
static jmethodID vfs_saf_saf_directory_init_method;
static jmethodID vfs_saf_saf_directory_close_method;
static jmethodID vfs_saf_saf_directory_readdir_batch_method;

bool retro_vfs_init_saf(JNIEnv *(*get_jni_env)(void), jobject activity_object)
{
   JNIEnv *env;

   if (vfs_saf_get_jni_env != NULL && !retro_vfs_deinit_saf())
      return false;

   env = get_jni_env();
   if (env == NULL)
      return false;

   (*env)->PushLocalFrame(env, 14);
   if ((*env)->ExceptionOccurred(env))
   {
      (*env)->ExceptionDescribe(env);
      (*env)->ExceptionClear(env);
      return false;
   }

   vfs_saf_content_resolver_object = NULL;
   vfs_saf_vfs_implementation_saf_class = NULL;
   vfs_saf_saf_directory_class = NULL;

   /*
    * ClassLoader class_loader_object = activity_object.getClassLoader();
    * Class<?> vfs_saf_vfs_implementation_saf_class = class_loader_object.loadClass("com.libretro.common.vfs.VfsImplementationSaf");
    * Class<?> vfs_saf_saf_directory_class = class_loader_object.loadClass("com.libretro.common.vfs.VfsImplementationSaf$SafDirectory");
    */
   {
      jobject class_loader_object;
      jmethodID load_class_method;
      jstring vfs_implementation_saf_string;
      jclass vfs_implementation_saf_class;
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

   vfs_saf_rename_saf_file_method = (*env)->GetStaticMethodID(env, vfs_saf_vfs_implementation_saf_class, "renameSafFile", "(Landroid/content/ContentResolver;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)Z");
   if ((*env)->ExceptionOccurred(env)) goto error;

   vfs_saf_stat_saf_method = (*env)->GetStaticMethodID(env, vfs_saf_vfs_implementation_saf_class, "statSaf", "(Landroid/content/ContentResolver;Ljava/lang/String;Ljava/lang/String;Z)J");
   if ((*env)->ExceptionOccurred(env)) goto error;

   vfs_saf_mkdir_saf_method = (*env)->GetStaticMethodID(env, vfs_saf_vfs_implementation_saf_class, "mkdirSaf", "(Landroid/content/ContentResolver;Ljava/lang/String;Ljava/lang/String;)I");
   if ((*env)->ExceptionOccurred(env)) goto error;

   vfs_saf_saf_directory_init_method = (*env)->GetMethodID(env, vfs_saf_saf_directory_class, "<init>", "(Landroid/content/ContentResolver;Ljava/lang/String;Ljava/lang/String;)V");
   if ((*env)->ExceptionOccurred(env)) goto error;

   vfs_saf_saf_directory_close_method = (*env)->GetMethodID(env, vfs_saf_saf_directory_class, "close", "()V");
   if ((*env)->ExceptionOccurred(env)) goto error;

   vfs_saf_saf_directory_readdir_batch_method = (*env)->GetMethodID(env, vfs_saf_saf_directory_class, "readdirBatch", "(I)[B");
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
   (*env)->PopLocalFrame(env, NULL);
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
   if (vfs_saf_saf_directory_class != NULL)
   {
      (*env)->DeleteGlobalRef(env, vfs_saf_saf_directory_class);
      vfs_saf_saf_directory_class = NULL;
      if ((*env)->ExceptionOccurred(env)) goto error;
   }
   (*env)->PopLocalFrame(env, NULL);
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

bool retro_vfs_path_split_content_saf(struct libretro_vfs_implementation_saf_path_split_result *out, const char *content_uri)
{
   const char *ptr;
   char *tree;
   char *path;

   if (strncmp(content_uri, "content://", sizeof "content://" - 1) != 0)
      return false;

   /* Advance ptr to the first forward slash in the content URI after the content:// */
   for (ptr = content_uri + (sizeof "content://" - 1); *ptr != 0 && *ptr != '/'; ++ptr);
   if (*ptr == 0)
      return false;

   /* If the content URI does not contain a tree component, consider the entire content URI to be the tree and the path to be the root */
   if (strncmp(ptr, "/tree/", sizeof "/tree/" - 1) != 0)
   {
      if ((tree = strdup(content_uri)) == NULL)
         return false;
      if ((path = strdup("/")) == NULL)
      {
         free(tree);
         return false;
      }
   }
   else
   {
      size_t tree_size;

      /* Advance ptr to the next forward slash after /tree/ */
      for (tree_size = 0, ptr += sizeof "/tree/" - 1; *ptr != 0 && *ptr != '/'; ++tree_size)
      {
         if (
            *ptr == '%'
               && (
                  (ptr[1] >= '0' && ptr[1] <= '9')
                     || (ptr[1] >= 'A' && ptr[1] <= 'F')
                     || (ptr[1] >= 'a' && ptr[1] <= 'f')
               ) && (
                  (ptr[2] >= '0' && ptr[2] <= '9')
                     || (ptr[2] >= 'A' && ptr[2] <= 'F')
                     || (ptr[2] >= 'a' && ptr[2] <= 'f')
               ) && (
                  ptr[1] != '0'
                     || ptr[2] != '0'
               )
         )
            ptr += 3;
         else
            ++ptr;
      }

      /* If the content URI does not also contain a document component, consider the entire content URI to be the tree and the path to be the root */
      if (*ptr == 0 || strncmp(ptr, "/document/", sizeof "/document/" - 1) != 0)
      {
         if ((tree = strdup(content_uri)) == NULL)
            return false;
         if ((path = strdup("/")) == NULL)
         {
            free(tree);
            return false;
         }
      }
      /* Otherwise, isolate the document component of the content URI as the path and use the rest of the content URI as the tree */
      else
      {
         char *path_ptr;
         size_t path_size;

         if ((tree = (char *)malloc(ptr - content_uri + 1)) == NULL)
            return false;
         memcpy(tree, content_uri, ptr - content_uri);
         tree[ptr - content_uri] = 0;

         content_uri = ptr + (sizeof "/document/" - 1);
         for (path_size = 0, ptr = content_uri; *ptr != 0 && *ptr != '/'; ++path_size)
         {
            if (
               *ptr == '%'
                  && (
                     (ptr[1] >= '0' && ptr[1] <= '9')
                        || (ptr[1] >= 'A' && ptr[1] <= 'F')
                        || (ptr[1] >= 'a' && ptr[1] <= 'f')
                  ) && (
                     (ptr[2] >= '0' && ptr[2] <= '9')
                        || (ptr[2] >= 'A' && ptr[2] <= 'F')
                        || (ptr[2] >= 'a' && ptr[2] <= 'f')
                  ) && (
                     ptr[1] != '0'
                        || ptr[2] != '0'
                  )
            )
            {
               ptr += 3;
               if (path_size < tree_size)
                  content_uri += 3;
            }
            else
            {
               ++ptr;
               if (path_size < tree_size)
                  ++content_uri;
            }
         }
         if (path_size <= tree_size)
            path_size = 0;
         else
            path_size -= tree_size;
         if (*content_uri != '%' || content_uri[1] != '2' || (content_uri[2] != 'F' && content_uri[2] != 'f'))
            ++path_size;
         if ((path_ptr = path = (char *)malloc(path_size + 1)) == NULL)
         {
            free(tree);
            return false;
         }
         if (*content_uri != '%' || content_uri[1] != '2' || (content_uri[2] != 'F' && content_uri[2] != 'f'))
            *path_ptr++ = '/';
         for (ptr = content_uri; *ptr != 0 && *ptr != '/';)
         {
            if (
               *ptr == '%'
                  && (
                     (ptr[1] >= '0' && ptr[1] <= '9')
                        || (ptr[1] >= 'A' && ptr[1] <= 'F')
                        || (ptr[1] >= 'a' && ptr[1] <= 'f')
                  ) && (
                     (ptr[2] >= '0' && ptr[2] <= '9')
                        || (ptr[2] >= 'A' && ptr[2] <= 'F')
                        || (ptr[2] >= 'a' && ptr[2] <= 'f')
                  ) && (
                     ptr[1] != '0'
                        || ptr[2] != '0'
                  )
            )
            {
               if (ptr[1] >= '0' && ptr[1] <= '9')
                  *path_ptr = (ptr[1] - '0') << 4;
               else if (ptr[1] >= 'A' && ptr[1] <= 'F')
                  *path_ptr = (ptr[1] - ('A' - 10)) << 4;
               else
                  *path_ptr = (ptr[1] - ('a' - 10)) << 4;
               if (ptr[2] >= '0' && ptr[2] <= '9')
                  *path_ptr |= ptr[2] - '0';
               else if (ptr[2] >= 'A' && ptr[2] <= 'F')
                  *path_ptr |= ptr[2] - ('A' - 10);
               else
                  *path_ptr |= ptr[2] - ('a' - 10);
               ++path_ptr;
               ptr += 3;
            }
            else
               *path_ptr++ = *ptr++;
         }
         *path_ptr = 0;
      }
   }

   out->tree = tree;
   out->path = path;
   return true;
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

   (*env)->PushLocalFrame(env, 2);
   if ((*env)->ExceptionOccurred(env))
   {
      (*env)->ExceptionDescribe(env);
      (*env)->ExceptionClear(env);
      return -1;
   }

   tree_object = (*env)->NewStringUTF(env, tree);
   if ((*env)->ExceptionOccurred(env)) goto error;

   path_object = (*env)->NewStringUTF(env, path);
   if ((*env)->ExceptionOccurred(env)) goto error;

   fd = (*env)->CallStaticIntMethod(env, vfs_saf_vfs_implementation_saf_class, vfs_saf_open_saf_file_method, vfs_saf_content_resolver_object, tree_object, path_object, !!(mode & RETRO_VFS_FILE_ACCESS_READ), !!(mode & RETRO_VFS_FILE_ACCESS_WRITE), !(mode & RETRO_VFS_FILE_ACCESS_UPDATE_EXISTING));
   if ((*env)->ExceptionOccurred(env)) goto error;

   (*env)->PopLocalFrame(env, NULL);
   return fd;

error:
   (*env)->ExceptionDescribe(env);
   (*env)->ExceptionClear(env);
   (*env)->PopLocalFrame(env, NULL);
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

   (*env)->PushLocalFrame(env, 2);
   if ((*env)->ExceptionOccurred(env))
   {
      (*env)->ExceptionDescribe(env);
      (*env)->ExceptionClear(env);
      return -1;
   }

   tree_object = (*env)->NewStringUTF(env, tree);
   if ((*env)->ExceptionOccurred(env)) goto error;

   path_object = (*env)->NewStringUTF(env, path);
   if ((*env)->ExceptionOccurred(env)) goto error;

   ret = (*env)->CallStaticBooleanMethod(env, vfs_saf_vfs_implementation_saf_class, vfs_saf_remove_saf_file_method, vfs_saf_content_resolver_object, tree_object, path_object);
   if ((*env)->ExceptionOccurred(env)) goto error;

   (*env)->PopLocalFrame(env, NULL);
   return ret ? 0 : -1;

error:
   (*env)->ExceptionDescribe(env);
   (*env)->ExceptionClear(env);
   (*env)->PopLocalFrame(env, NULL);
   return -1;
}

int retro_vfs_file_rename_saf(const char *old_tree, const char *old_path, const char *new_tree, const char *new_path)
{
   JNIEnv *env;
   jstring tree_object;
   jstring old_path_object;
   jstring new_path_object;
   bool ret;

   /* A document can only be renamed within the tree it was granted through */
   if (strcmp(old_tree, new_tree) != 0)
      return -1;

   if (vfs_saf_get_jni_env == NULL)
      return -1;
   env = vfs_saf_get_jni_env();
   if (env == NULL)
      return -1;

   (*env)->PushLocalFrame(env, 3);
   if ((*env)->ExceptionOccurred(env))
   {
      (*env)->ExceptionDescribe(env);
      (*env)->ExceptionClear(env);
      return -1;
   }

   tree_object = (*env)->NewStringUTF(env, old_tree);
   if ((*env)->ExceptionOccurred(env)) goto error;

   old_path_object = (*env)->NewStringUTF(env, old_path);
   if ((*env)->ExceptionOccurred(env)) goto error;

   new_path_object = (*env)->NewStringUTF(env, new_path);
   if ((*env)->ExceptionOccurred(env)) goto error;

   ret = (*env)->CallStaticBooleanMethod(env, vfs_saf_vfs_implementation_saf_class, vfs_saf_rename_saf_file_method, vfs_saf_content_resolver_object, tree_object, old_path_object, new_path_object);
   if ((*env)->ExceptionOccurred(env)) goto error;

   (*env)->PopLocalFrame(env, NULL);
   return ret ? 0 : -1;

error:
   (*env)->ExceptionDescribe(env);
   (*env)->ExceptionClear(env);
   (*env)->PopLocalFrame(env, NULL);
   return -1;
}

int retro_vfs_stat_saf(const char *tree, const char *path, int64_t *size)
{
   JNIEnv *env;
   jstring tree_object;
   jstring path_object;
   jlong packed;

   if (vfs_saf_get_jni_env == NULL)
      return 0;
   env = vfs_saf_get_jni_env();
   if (env == NULL)
      return 0;

   (*env)->PushLocalFrame(env, 2);
   if ((*env)->ExceptionOccurred(env))
   {
      (*env)->ExceptionDescribe(env);
      (*env)->ExceptionClear(env);
      return 0;
   }

   tree_object = (*env)->NewStringUTF(env, tree);
   if ((*env)->ExceptionOccurred(env)) goto error;

   path_object = (*env)->NewStringUTF(env, path);
   if ((*env)->ExceptionOccurred(env)) goto error;

   /* Size << 1 | is-directory, or INT64_MIN if it could not be queried */
   packed = (*env)->CallStaticLongMethod(env, vfs_saf_vfs_implementation_saf_class, vfs_saf_stat_saf_method, vfs_saf_content_resolver_object, tree_object, path_object, size != NULL);
   if ((*env)->ExceptionOccurred(env)) goto error;

   (*env)->PopLocalFrame(env, NULL);

   if (packed == (jlong)INT64_MIN)
      return 0;
   if (size != NULL)
      *size = (int64_t)((uint64_t)packed >> 1);
   return (packed & 1) ? RETRO_VFS_STAT_IS_VALID | RETRO_VFS_STAT_IS_DIRECTORY : RETRO_VFS_STAT_IS_VALID;

error:
   (*env)->ExceptionDescribe(env);
   (*env)->ExceptionClear(env);
   (*env)->PopLocalFrame(env, NULL);
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

   (*env)->PushLocalFrame(env, 2);
   if ((*env)->ExceptionOccurred(env))
   {
      (*env)->ExceptionDescribe(env);
      (*env)->ExceptionClear(env);
      return -1;
   }

   tree_object = (*env)->NewStringUTF(env, tree);
   if ((*env)->ExceptionOccurred(env)) goto error;

   path_object = (*env)->NewStringUTF(env, dir);
   if ((*env)->ExceptionOccurred(env)) goto error;

   ret = (*env)->CallStaticIntMethod(env, vfs_saf_vfs_implementation_saf_class, vfs_saf_mkdir_saf_method, vfs_saf_content_resolver_object, tree_object, path_object);
   if ((*env)->ExceptionOccurred(env)) goto error;

   (*env)->PopLocalFrame(env, NULL);
   return ret;

error:
   (*env)->ExceptionDescribe(env);
   (*env)->ExceptionClear(env);
   (*env)->PopLocalFrame(env, NULL);
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

   /* calloc, not malloc: the batch arrays must start empty so that a
    * failure before the first fill cannot free garbage pointers. */
   dirstream = (libretro_vfs_implementation_saf_dir *)calloc(1, sizeof *dirstream);
   if (dirstream == NULL)
      return NULL;

   (*env)->PushLocalFrame(env, 2);
   if ((*env)->ExceptionOccurred(env))
   {
      free(dirstream);
      (*env)->ExceptionDescribe(env);
      (*env)->ExceptionClear(env);
      return NULL;
   }

   tree_object = (*env)->NewStringUTF(env, tree);
   if ((*env)->ExceptionOccurred(env)) goto error;

   path_object = (*env)->NewStringUTF(env, dir);
   if ((*env)->ExceptionOccurred(env)) goto error;

   dirstream->directory_object = (*env)->NewObject(env, vfs_saf_saf_directory_class, vfs_saf_saf_directory_init_method, vfs_saf_content_resolver_object, tree_object, path_object);
   if ((*env)->ExceptionOccurred(env)) goto error;

   dirstream->directory_object = (*env)->NewGlobalRef(env, dirstream->directory_object);
   if ((*env)->ExceptionOccurred(env)) goto error;

   dirstream->batch_count   = 0;
   dirstream->batch_pos     = 0;
   dirstream->exhausted     = false;
   dirstream->dirent_name   = NULL;
   dirstream->dirent_is_dir = false;
   (*env)->PopLocalFrame(env, NULL);
   return dirstream;

error:
   free(dirstream);
   (*env)->ExceptionDescribe(env);
   (*env)->ExceptionClear(env);
   (*env)->PopLocalFrame(env, NULL);
   return NULL;
}

/* Pull the next batch across in one JNI call and one copy into the
 * reused arena, then lend names out of it, so that stepping through
 * them afterwards costs nothing. Returns false when the directory is
 * exhausted. */
static bool saf_batch_fill(JNIEnv *env,
      libretro_vfs_implementation_saf_dir *dirstream)
{
   jbyteArray  batch;
   jsize       length;
   const char *p;
   const char *end;
   bool        ok = false;

   dirstream->batch_count = 0;
   dirstream->batch_pos   = 0;

   if (dirstream->exhausted)
      return false;

   if ((*env)->PushLocalFrame(env, 1) < 0)
   {
      if ((*env)->ExceptionCheck(env))
      {
         (*env)->ExceptionDescribe(env);
         (*env)->ExceptionClear(env);
      }
      return false;
   }

   batch = (jbyteArray)(*env)->CallObjectMethod(env,
         dirstream->directory_object,
         vfs_saf_saf_directory_readdir_batch_method,
         (jint)RETRO_VFS_SAF_DIRENT_BATCH);
   if ((*env)->ExceptionCheck(env) || !batch)
      goto done;

   if ((length = (*env)->GetArrayLength(env, batch)) <= 0)
   {
      dirstream->exhausted = true;
      goto done;
   }

   if ((size_t)length > dirstream->arena_size)
   {
      char *arena = (char *)realloc(dirstream->arena, (size_t)length);
      if (!arena)
         goto done;
      dirstream->arena      = arena;
      dirstream->arena_size = (size_t)length;
   }

   (*env)->GetByteArrayRegion(env, batch, 0, length, (jbyte *)dirstream->arena);
   if ((*env)->ExceptionCheck(env))
      goto done;

   /* Per entry: a directory flag, the name, its terminator */
   for (  p = dirstream->arena, end = p + length;
          p < end && dirstream->batch_count < RETRO_VFS_SAF_DIRENT_BATCH;)
   {
      const char *name = p + 1;
      const char *nul  = (const char *)memchr(name, 0, (size_t)(end - name));
      if (!nul)
         break;
      dirstream->batch_name[dirstream->batch_count]   = name;
      dirstream->batch_is_dir[dirstream->batch_count] = (*p != 0);
      dirstream->batch_count++;
      p = nul + 1;
   }

   /* A short batch means the cursor ran dry; do not ask again. */
   if (dirstream->batch_count < RETRO_VFS_SAF_DIRENT_BATCH)
      dirstream->exhausted = true;

   ok = (dirstream->batch_count > 0);

done:
   if ((*env)->ExceptionCheck(env))
   {
      (*env)->ExceptionDescribe(env);
      (*env)->ExceptionClear(env);
   }
   (*env)->PopLocalFrame(env, NULL);
   return ok;
}

bool retro_vfs_readdir_saf(libretro_vfs_implementation_saf_dir *dirstream)
{
   JNIEnv *env;

   if (vfs_saf_get_jni_env == NULL)
      return false;
   env = vfs_saf_get_jni_env();
   if (env == NULL)
      return false;

   if (dirstream->batch_pos >= dirstream->batch_count)
   {
      if (!saf_batch_fill(env, dirstream))
      {
         dirstream->dirent_name   = NULL;
         dirstream->dirent_is_dir = false;
         return false;
      }
   }

   if (dirstream->batch_pos >= dirstream->batch_count)
   {
      dirstream->dirent_name   = NULL;
      dirstream->dirent_is_dir = false;
      return false;
   }

   dirstream->dirent_name   = dirstream->batch_name[dirstream->batch_pos];
   dirstream->dirent_is_dir = dirstream->batch_is_dir[dirstream->batch_pos];
   dirstream->batch_pos++;

   return true;
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

   dirstream->batch_count   = 0;
   dirstream->dirent_name   = NULL;
   dirstream->dirent_is_dir = false;

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

   free(dirstream->arena);
   free(dirstream);
   return 0;
}

#endif
