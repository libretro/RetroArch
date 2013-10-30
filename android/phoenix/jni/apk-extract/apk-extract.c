#include "file_extract.h"
#include "file.h"

#include <stdio.h>
#include "../native/com_retroarch_browser_NativeInterface.h"

struct userdata
{
   const char *subdir;
   const char *dest;
};

static bool zlib_cb(const char *name, const uint8_t *cdata, unsigned cmode, uint32_t csize, uint32_t size,
      uint32_t crc32, void *userdata)
{
   struct userdata *user = userdata;
   const char *subdir = user->subdir;
   const char *dest   = user->dest;

   if (strstr(name, subdir) != name)
      return true;

   name += strlen(subdir) + 1;

   char path[PATH_MAX];
   char path_dir[PATH_MAX];
   fill_pathname_join(path, dest, name, sizeof(path));
   fill_pathname_basedir(path_dir, path, sizeof(path_dir));

   if (!path_mkdir(path_dir))
   {
      RARCH_ERR("Failed to create dir: %s.\n", path_dir);
      return false;
   }

   RARCH_LOG("Extracting %s -> %s ...\n", name, path);

   switch (cmode)
   {
      case 0: // Uncompressed
         if (!write_file(path, cdata, size))
         {
            RARCH_ERR("Failed to write file: %s.\n", path);
            return false;
         }
         break;

      case 8: // Deflate
         if (!zlib_inflate_data_to_file(path, cdata, csize, size, crc32))
         {
            RARCH_ERR("Failed to deflate to: %s.\n", path);
            return false;
         }
         break;

      default:
         return false;
   }

   return true;
}

JNIEXPORT jboolean JNICALL Java_com_retroarch_browser_NativeInterface_extractArchiveTo(
      JNIEnv *env, jclass cls, jstring archive, jstring subdir, jstring dest)
{
   const char *archive_c = (*env)->GetStringUTFChars(env, archive, NULL);
   const char *subdir_c  = (*env)->GetStringUTFChars(env, subdir, NULL);
   const char *dest_c    = (*env)->GetStringUTFChars(env, dest, NULL);

   jboolean ret = JNI_TRUE;

   struct userdata data = {
      .subdir = subdir_c,
      .dest = dest_c,
   };

   if (!zlib_parse_file(archive_c, zlib_cb, &data))
   {
      RARCH_ERR("Failed to parse APK: %s.\n", archive_c);
      ret = JNI_FALSE;
   }

   (*env)->ReleaseStringUTFChars(env, archive, archive_c);
   (*env)->ReleaseStringUTFChars(env, subdir, subdir_c);
   (*env)->ReleaseStringUTFChars(env, dest, dest_c);
   return ret;
}

