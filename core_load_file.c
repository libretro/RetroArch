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

/* ANDROID_DLEXT_FORCE_LOAD, Android 6: a new instance whatever is
 * loaded already. Older linkers refuse the bit. */
#define CORE_DLEXT_FORCE_LOAD 0x40

/* NULL without android_dlopen_ext (Jelly Bean) or when it refuses */
static dylib_t core_load_fd(core_file_t *f, const char *path,
      uint64_t flags, bool *have_ext)
{
   core_dlopen_ext_t dlopen_ext;
   android_dlextinfo info;
   /* Public from API 21; the Jelly Bean build targets 16 */
   void *sym = dlsym(RTLD_DEFAULT, "android_dlopen_ext");
   if (!(*have_ext = (sym != NULL)))
      return NULL;
   /* Android 5's linker reads the ELF header at the fd's offset,
    * which the check's hash or an earlier load may have moved */
   if (lseek(core_file_fd(f), 0, SEEK_SET) != 0)
      return NULL;
   memcpy(&dlopen_ext, &sym, sizeof(dlopen_ext));
   memset(&info, 0, sizeof(info));
   info.flags             = ANDROID_DLEXT_USE_LIBRARY_FD | flags;
   info.library_fd        = core_file_fd(f);
   info.library_fd_offset = 0;
   return dlopen_ext(path, RTLD_LAZY | RTLD_LOCAL, &info);
}
#endif

dylib_t core_load_file(core_file_t *f, const char *path)
{
#ifdef ANDROID
   bool have_ext;
   dylib_t lib = core_load_fd(f, path, 0, &have_ext);
   if (have_ext)
      return lib;
#else
   (void)f;
#endif
   return dylib_load(path);
}

#ifdef ANDROID
dylib_t core_load_file_instance(core_file_t *f, const char *path)
{
   bool have_ext;
   return core_load_fd(f, path, CORE_DLEXT_FORCE_LOAD, &have_ext);
}
#endif
