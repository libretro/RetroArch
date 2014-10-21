/* Copyright  (C) 2010-2014 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (file_path.c).
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

#include <file/file_path.h>
#include <stdlib.h>
#include <boolean.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <compat/strl.h>
#include <compat/posix_string.h>
#include <retro_miscellaneous.h>

#ifdef __HAIKU__
#include <kernel/image.h>
#endif

#if (defined(__CELLOS_LV2__) && !defined(__PSL1GHT__)) || defined(__QNX__) || defined(PSP)
#include <unistd.h> /* stat() is defined here */
#endif

#if defined(__CELLOS_LV2__)

#ifndef S_ISDIR
#define S_ISDIR(x) (x & 0040000)
#endif

#endif

#if defined(_WIN32)
#ifdef _MSC_VER
#define setmode _setmode
#endif
#ifdef _XBOX
#include <xtl.h>
#define INVALID_FILE_ATTRIBUTES -1
#else
#include <io.h>
#include <fcntl.h>
#include <direct.h>
#include <windows.h>
#endif
#else
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#endif

const char *path_get_extension(const char *path)
{
   const char *ext = strrchr(path_basename(path), '.');
   if (ext)
      return ext + 1;
   return "";
}

char *path_remove_extension(char *path)
{
   char *last = (char*)strrchr(path_basename(path), '.');
   if (*last)
      *last = '\0';
   return last;
}

static bool path_char_is_slash(char c)
{
#ifdef _WIN32
   return (c == '/') || (c == '\\');
#else
   return (c == '/');
#endif
}

static const char *path_default_slash(void)
{
#ifdef _WIN32
   return "\\";
#else
   return "/";
#endif
}

bool path_contains_compressed_file(const char *path)
{
   /*
    * Currently we only check for hash symbol inside the pathname.
    * If path is ever expanded to a general URI, we should check for that here.
    */
   return (strchr(path,'#') != NULL);
}

bool path_is_compressed_file(const char* path)
{
#ifdef HAVE_COMPRESSION
   const char* file_ext = path_get_extension(path);
#ifdef HAVE_7ZIP
   if (strcmp(file_ext,"7z") == 0)
      return true;
#endif
#ifdef HAVE_ZLIB
   if (strcmp(file_ext,"zip") == 0)
      return true;
#endif

#endif
   return false;
}

bool path_is_directory(const char *path)
{
#ifdef _WIN32
   DWORD ret = GetFileAttributes(path);
   return (ret & FILE_ATTRIBUTE_DIRECTORY) && (ret != INVALID_FILE_ATTRIBUTES);
#else
   struct stat buf;
   if (stat(path, &buf) < 0)
      return false;

   return S_ISDIR(buf.st_mode);
#endif
}

bool path_file_exists(const char *path)
{
   FILE *dummy = fopen(path, "rb");

   if (dummy)
   {
      fclose(dummy);
      return true;
   }

   return false;
}

void fill_pathname(char *out_path, const char *in_path,
      const char *replace, size_t size)
{
   char tmp_path[PATH_MAX];
   char *tok;

   rarch_assert(strlcpy(tmp_path, in_path,
            sizeof(tmp_path)) < sizeof(tmp_path));
   if ((tok = (char*)strrchr(path_basename(tmp_path), '.')))
      *tok = '\0';

   rarch_assert(strlcpy(out_path, tmp_path, size) < size);
   rarch_assert(strlcat(out_path, replace, size) < size);
}

void fill_pathname_noext(char *out_path, const char *in_path,
      const char *replace, size_t size)
{
   rarch_assert(strlcpy(out_path, in_path, size) < size);
   rarch_assert(strlcat(out_path, replace, size) < size);
}

static char *find_last_slash(const char *str)
{
   const char *slash = strrchr(str, '/');
#ifdef _WIN32
   const char *backslash = strrchr(str, '\\');

   if (backslash && ((slash && backslash > slash) || !slash))
      slash = backslash;
#endif

   return (char*)slash;
}

/* Assumes path is a directory. Appends a slash
 * if not already there. */
void fill_pathname_slash(char *path, size_t size)
{
   size_t path_len = strlen(path);
   const char *last_slash = find_last_slash(path);

   // Try to preserve slash type.
   if (last_slash && (last_slash != (path + path_len - 1)))
   {
      char join_str[2] = {*last_slash};
      rarch_assert(strlcat(path, join_str, size) < size);
   }
   else if (!last_slash)
      rarch_assert(strlcat(path, path_default_slash(), size) < size);
}

void fill_pathname_dir(char *in_dir, const char *in_basename,
      const char *replace, size_t size)
{
   fill_pathname_slash(in_dir, size);
   const char *base = path_basename(in_basename);
   rarch_assert(strlcat(in_dir, base, size) < size);
   rarch_assert(strlcat(in_dir, replace, size) < size);
}

void fill_pathname_base(char *out, const char *in_path, size_t size)
{
   const char *ptr = find_last_slash(in_path);

   if (ptr)
      ptr++;
   else
      ptr = in_path;

   /* In case of compression, we also have to consider paths like
    *   /path/to/archive.7z#mygame.img
    *   and
    *   /path/to/archive.7z#folder/mygame.img
    *   basename would be mygame.img in both cases
    */

#ifdef HAVE_COMPRESSION
   const char *ptr_bak = ptr;
   ptr = strchr(ptr_bak,'#');
   if (ptr)
      ptr++;
   else
      ptr = ptr_bak;
#endif

   rarch_assert(strlcpy(out, ptr, size) < size);
}

void fill_pathname_basedir(char *out_dir,
      const char *in_path, size_t size)
{
   rarch_assert(strlcpy(out_dir, in_path, size) < size);
   path_basedir(out_dir);
}

void fill_pathname_parent_dir(char *out_dir,
      const char *in_dir, size_t size)
{
   rarch_assert(strlcpy(out_dir, in_dir, size) < size);
   path_parent_dir(out_dir);
}

void fill_dated_filename(char *out_filename,
      const char *ext, size_t size)
{
   time_t cur_time;
   time(&cur_time);

   strftime(out_filename, size,
         "RetroArch-%m%d-%H%M%S.", localtime(&cur_time));
   strlcat(out_filename, ext, size);
}

void path_basedir(char *path)
{
   if (strlen(path) < 2)
      return;

   char *last;

#ifdef HAVE_COMPRESSION
   /* We want to find the directory with the zipfile in basedir. */
   last = strchr(path,'#');
   if (last)
      *last = '\0';
#endif

   last = find_last_slash(path);

   if (last)
      last[1] = '\0';
   else
      snprintf(path, 3, ".%s", path_default_slash());
}

void path_parent_dir(char *path)
{
   size_t len = strlen(path);
   if (len && path_char_is_slash(path[len - 1]))
      path[len - 1] = '\0';
   path_basedir(path);
}

const char *path_basename(const char *path)
{
   const char *last = find_last_slash(path);

   /* We cut either at the last hash or the last slash; whichever comes last */
#ifdef HAVE_COMPRESSION
   const char *last_hash = strchr(path,'#');
   if (last_hash > last)
   {
      return last_hash + 1;
   }
#endif

   if (last)
      return last + 1;
   return path;
}

bool path_is_absolute(const char *path)
{
#ifdef _WIN32
   /* Many roads lead to Rome ... */
   return path[0] == '/' || (strstr(path, "\\\\") == path)
      || strstr(path, ":/") || strstr(path, ":\\") || strstr(path, ":\\\\");
#else
   return path[0] == '/';
#endif
}

void path_resolve_realpath(char *buf, size_t size)
{
#ifndef RARCH_CONSOLE
   char tmp[PATH_MAX];
   strlcpy(tmp, buf, sizeof(tmp));

#ifdef _WIN32
   if (!_fullpath(buf, tmp, size))
      strlcpy(buf, tmp, size);
#else
   rarch_assert(size >= PATH_MAX);

   /* NOTE: realpath() expects at least PATH_MAX bytes in buf.
    * Technically, PATH_MAX needn't be defined, but we rely on it anyways.
    * POSIX 2008 can automatically allocate for you,
    * but don't rely on that. */
   if (!realpath(tmp, buf))
      strlcpy(buf, tmp, size);
#endif

#else
   (void)buf;
   (void)size;
#endif
}

static bool path_mkdir_norecurse(const char *dir)
{
   int ret;
#if defined(_WIN32)
   ret = _mkdir(dir);
#elif defined(IOS)
   ret = mkdir(dir, 0755);
#else
   ret = mkdir(dir, 0750);
#endif
   /* Don't treat this as an error. */
   if (ret < 0 && errno == EEXIST && path_is_directory(dir))
      ret = 0;
   if (ret < 0)
      RARCH_ERR("mkdir(%s) error: %s.\n", dir, strerror(errno));
   return ret == 0;
}

bool path_mkdir(const char *dir)
{
   const char *target = NULL;
   /* Use heap. Real chance of stack overflow if we recurse too hard. */
   char *basedir = strdup(dir);
   bool ret = true;

   if (!basedir)
      return false;

   path_parent_dir(basedir);
   if (!*basedir || !strcmp(basedir, dir))
   {
      ret = false;
      goto end;
   }

   if (path_is_directory(basedir))
   {
      target = dir;
      ret = path_mkdir_norecurse(dir);
   }
   else
   {
      target = basedir;
      ret = path_mkdir(basedir);
      if (ret)
      {
         target = dir;
         ret = path_mkdir_norecurse(dir);
      }
   }

end:
   if (target && !ret)
      RARCH_ERR("Failed to create directory: \"%s\".\n", target);
   free(basedir);
   return ret;
}

void fill_pathname_resolve_relative(char *out_path,
      const char *in_refpath, const char *in_path, size_t size)
{
   if (path_is_absolute(in_path))
      rarch_assert(strlcpy(out_path, in_path, size) < size);
   else
   {
      rarch_assert(strlcpy(out_path, in_refpath, size) < size);
      path_basedir(out_path);
      rarch_assert(strlcat(out_path, in_path, size) < size);
   }
}

void fill_pathname_join(char *out_path,
      const char *dir, const char *path, size_t size)
{
   rarch_assert(strlcpy(out_path, dir, size) < size);

   if (*out_path)
      fill_pathname_slash(out_path, size);

   rarch_assert(strlcat(out_path, path, size) < size);
}

void fill_pathname_join_delim(char *out_path, const char *dir,
      const char *path, const char delim, size_t size)
{
   size_t copied = strlcpy(out_path, dir, size);
   rarch_assert(copied < size+1);

   out_path[copied]=delim;
   out_path[copied+1] = '\0';

   rarch_assert(strlcat(out_path, path, size) < size);
}

void fill_pathname_expand_special(char *out_path,
      const char *in_path, size_t size)
{
#if !defined(RARCH_CONSOLE)
   if (*in_path == '~')
   {
      const char *home = getenv("HOME");
      if (home)
      {
         size_t src_size = strlcpy(out_path, home, size);
         rarch_assert(src_size < size);

         out_path  += src_size;
         size -= src_size;
         in_path++;
      }
   }
   else if ((in_path[0] == ':') &&
#ifdef _WIN32
         ((in_path[1] == '/') || (in_path[1] == '\\'))
#else
         (in_path[1] == '/')
#endif
            )
   {
      char application_dir[PATH_MAX];
      fill_pathname_application_path(application_dir, sizeof(application_dir));
      path_basedir(application_dir);

      size_t src_size = strlcpy(out_path, application_dir, size);
      rarch_assert(src_size < size);

      out_path  += src_size;
      size -= src_size;
      in_path += 2;
   }
#endif

   rarch_assert(strlcpy(out_path, in_path, size) < size);
}

void fill_short_pathname_representation(char* out_rep,
      const char *in_path, size_t size)
{
   char path_short[PATH_MAX];
   fill_pathname(path_short, path_basename(in_path), "",
            sizeof(path_short));

   char* last_hash = strchr(path_short,'#');
   /* We handle paths like:
    * /path/to/file.7z#mygame.img
    * short_name: mygame.img:
    */
   if(last_hash != NULL)
   {
      /* We check whether something is actually 
       * after the hash to avoid going over the buffer.
       */
      rarch_assert(strlen(last_hash) > 1);
      strlcpy(out_rep,last_hash + 1, size);
   }
   else
      strlcpy(out_rep,path_short, size);
}


void fill_pathname_abbreviate_special(char *out_path,
      const char *in_path, size_t size)
{
#if !defined(RARCH_CONSOLE)
   unsigned i;

   const char *home = getenv("HOME");
   char application_dir[PATH_MAX];
   fill_pathname_application_path(application_dir, sizeof(application_dir));
   path_basedir(application_dir);

   /* application_dir could be zero-string. Safeguard against this.
    *
    * Keep application dir in front of home, moving app dir to a
    * new location inside home would break otherwise. */

   const char *candidates[3] = { application_dir, home, NULL };
   const char *notations[3] = { ":", "~", NULL };
   
   for (i = 0; candidates[i]; i++)
   {
      if (*candidates[i] && strstr(in_path, candidates[i]) == in_path)
      {
         size_t src_size = strlcpy(out_path, notations[i], size);
         rarch_assert(src_size < size);
      
         out_path += src_size;
         size -= src_size;
         in_path += strlen(candidates[i]);
      
         if (!path_char_is_slash(*in_path))
         {
            rarch_assert(strlcpy(out_path, path_default_slash(), size) < size);
            out_path++;
            size--;
         }

         break; /* Don't allow more abbrevs to take place. */
      }
   }
#endif

   rarch_assert(strlcpy(out_path, in_path, size) < size);
}

#ifndef RARCH_CONSOLE
void fill_pathname_application_path(char *buf, size_t size)
{
   size_t i;
   (void)i;
   if (!size)
      return;

#ifdef _WIN32
   DWORD ret = GetModuleFileName(GetModuleHandle(NULL), buf, size - 1);
   buf[ret] = '\0';
#elif defined(__APPLE__)
   CFBundleRef bundle = CFBundleGetMainBundle();
   if (bundle)
   {
      CFURLRef bundle_url = CFBundleCopyBundleURL(bundle);
      CFStringRef bundle_path = CFURLCopyPath(bundle_url);
      CFStringGetCString(bundle_path, buf, size, kCFStringEncodingUTF8);
      CFRelease(bundle_path);
      CFRelease(bundle_url);
      
      rarch_assert(strlcat(buf, "nobin", size) < size);
      return;
   }
#elif defined(__HAIKU__)
   image_info info;
   int32 cookie = 0;

   while (get_next_image_info(0, &cookie, &info) == B_OK)
   {
      if (info.type == B_APP_IMAGE)
      {
         strlcpy(buf, info.name, size);
         return;
      }
   }
#else
   *buf = '\0';
   pid_t pid = getpid(); 
   char link_path[PATH_MAX];
   /* Linux, BSD and Solaris paths. Not standardized. */
   static const char *exts[] = { "exe", "file", "path/a.out" };
   for (i = 0; i < ARRAY_SIZE(exts); i++)
   {
      snprintf(link_path, sizeof(link_path), "/proc/%u/%s",
            (unsigned)pid, exts[i]);
      ssize_t ret = readlink(link_path, buf, size - 1);
      if (ret >= 0)
      {
         buf[ret] = '\0';
         return;
      }
   }
   
   RARCH_ERR("Cannot resolve application path! This should not happen.\n");
#endif
}
#endif
