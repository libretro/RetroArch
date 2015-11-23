/* Copyright  (C) 2010-2015 The RetroArch team
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

#include <stdlib.h>
#include <boolean.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif

#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#endif

#include <file/file_path.h>

#include <compat/strl.h>
#include <compat/posix_string.h>
#include <retro_assert.h>
#include <retro_miscellaneous.h>

#include "verbosity.h"

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
         retro_assert(src_size < size);

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
      size_t src_size;
      char application_dir[PATH_MAX_LENGTH] = {0};

      fill_pathname_application_path(application_dir, sizeof(application_dir));
      path_basedir(application_dir);

      src_size   = strlcpy(out_path, application_dir, size);
      retro_assert(src_size < size);

      out_path  += src_size;
      size      -= src_size;
      in_path   += 2;
   }
#endif

   retro_assert(strlcpy(out_path, in_path, size) < size);
}


void fill_pathname_abbreviate_special(char *out_path,
      const char *in_path, size_t size)
{
#if !defined(RARCH_CONSOLE)
   unsigned i;
   const char *candidates[3];
   const char *notations[3];
   char application_dir[PATH_MAX_LENGTH] = {0};
   const char                      *home = getenv("HOME");

   /* application_dir could be zero-string. Safeguard against this.
    *
    * Keep application dir in front of home, moving app dir to a
    * new location inside home would break otherwise. */

   /* ugly hack - use application_dir pointer before filling it in. C89 reasons */
   candidates[0] = application_dir;
   candidates[1] = home;
   candidates[2] = NULL;

   notations [0] = ":";
   notations [1] = "~";
   notations [2] = NULL;

   fill_pathname_application_path(application_dir, sizeof(application_dir));
   path_basedir(application_dir);
   
   for (i = 0; candidates[i]; i++)
   {
      if (*candidates[i] && strstr(in_path, candidates[i]) == in_path)
      {
         size_t src_size  = strlcpy(out_path, notations[i], size);

         retro_assert(src_size < size);
      
         out_path        += src_size;
         size            -= src_size;
         in_path         += strlen(candidates[i]);
      
         if (!path_char_is_slash(*in_path))
         {
            retro_assert(strlcpy(out_path, path_default_slash(), size) < size);
            out_path++;
            size--;
         }

         break; /* Don't allow more abbrevs to take place. */
      }
   }
#endif

   retro_assert(strlcpy(out_path, in_path, size) < size);
}

#if !defined(RARCH_CONSOLE)
void fill_pathname_application_path(char *buf, size_t size)
{
#ifdef __APPLE__
  CFBundleRef bundle = CFBundleGetMainBundle();
#endif
   size_t i;
   (void)i;

   if (!size)
      return;

#ifdef _WIN32
   DWORD ret = GetModuleFileName(GetModuleHandle(NULL), buf, size - 1);
   buf[ret] = '\0';
#elif defined(__APPLE__)
   if (bundle)
   {
      CFURLRef bundle_url = CFBundleCopyBundleURL(bundle);
      CFStringRef bundle_path = CFURLCopyPath(bundle_url);
      CFStringGetCString(bundle_path, buf, size, kCFStringEncodingUTF8);
      CFRelease(bundle_path);
      CFRelease(bundle_url);
      
      retro_assert(strlcat(buf, "nobin", size) < size);
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
   {
      pid_t pid;
      static const char *exts[] = { "exe", "file", "path/a.out" };
      char link_path[PATH_MAX_LENGTH] = {0};

      *buf      = '\0';
      pid       = getpid(); 

      /* Linux, BSD and Solaris paths. Not standardized. */
      for (i = 0; i < ARRAY_SIZE(exts); i++)
      {
         ssize_t ret;

         snprintf(link_path, sizeof(link_path), "/proc/%u/%s",
               (unsigned)pid, exts[i]);
         ret = readlink(link_path, buf, size - 1);

         if (ret >= 0)
         {
            buf[ret] = '\0';
            return;
         }
      }
   }
   
   RARCH_ERR("Cannot resolve application path! This should not happen.");
#endif
}
#endif
