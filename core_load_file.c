/* Copyright (C) 2026 The RetroArch team
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include <string.h>

#ifdef ANDROID
#include <dlfcn.h>
#include <unistd.h>
#include <android/dlext.h>
#endif

#include <file/file_path.h>
#include <string/stdstring.h>

#include "core_load_file.h"

#ifdef ANDROID
typedef void *(*core_dlopen_ext_t)(const char *name, int flags,
      const android_dlextinfo *info);
#endif

dylib_t core_load_file(core_file_t *f, const char *path)
{
#ifdef ANDROID
   /* Public from API 21; the Jelly Bean build targets 16 */
   void *sym = dlsym(RTLD_DEFAULT, "android_dlopen_ext");
   if (sym)
   {
      core_dlopen_ext_t dlopen_ext;
      android_dlextinfo info;
      /* Android 5's linker reads the ELF header at the fd's offset,
       * which the check's hash or an earlier load may have moved */
      if (lseek(core_file_fd(f), 0, SEEK_SET) != 0)
         return NULL;
      memcpy(&dlopen_ext, &sym, sizeof(dlopen_ext));
      memset(&info, 0, sizeof(info));
      info.flags             = ANDROID_DLEXT_USE_LIBRARY_FD;
      info.library_fd        = core_file_fd(f);
      info.library_fd_offset = 0;
      return dlopen_ext(path, RTLD_LAZY | RTLD_LOCAL, &info);
   }
#else
   (void)f;
#endif
   return dylib_load(path);
}

bool core_load_framework_binary(const char *path, char *s, size_t len)
{
   static const char ext[] = ".framework";
   const char *name;
   size_t n, name_len;
   if (!string_ends_with(path, ext))
      return false;
   n        = strlen(path);
   /* dylib_load's choice, which cuts at an archive's '#' */
   name     = path_basename(path);
   name_len = strlen(name) - (sizeof(ext) - 1);
   if (!name_len || n + name_len + 2 > len)
      return false;
   memcpy(s, path, n);
   s[n] = '/';
   memcpy(s + n + 1, name, name_len);
   s[n + 1 + name_len] = '\0';
   return true;
}
