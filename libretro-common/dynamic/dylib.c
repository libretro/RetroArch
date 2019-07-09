/* Copyright  (C) 2010-2018 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (dylib.c).
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

#include <string.h>
#include <stdio.h>
#include <dynamic/dylib.h>
#include <encodings/utf.h>

#ifdef NEED_DYNAMIC

#ifdef _WIN32
#include <compat/posix_string.h>
#include <windows.h>
#else
#include <dlfcn.h>
#endif

/* Assume W-functions do not work below Win2K and Xbox platforms */
#if defined(_WIN32_WINNT) && _WIN32_WINNT < 0x0500 || defined(_XBOX)

#ifndef LEGACY_WIN32
#define LEGACY_WIN32
#endif

#endif

#ifdef _WIN32
static char last_dyn_error[512];

static void set_dl_error(void)
{
   DWORD err = GetLastError();

   if (FormatMessage(FORMAT_MESSAGE_IGNORE_INSERTS |
            FORMAT_MESSAGE_FROM_SYSTEM,
            NULL,
            err,
            MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT),
            last_dyn_error,
            sizeof(last_dyn_error) - 1,
            NULL) == 0)
      snprintf(last_dyn_error, sizeof(last_dyn_error) - 1,
            "unknown error %lu", err);
}
#endif

/**
 * dylib_load:
 * @path                         : Path to libretro core library.
 *
 * Platform independent dylib loading.
 *
 * Returns: library handle on success, otherwise NULL.
 **/
dylib_t dylib_load(const char *path)
{
#ifdef _WIN32
#ifndef __WINRT__
   int prevmode = SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX);
#endif
#ifdef __WINRT__
   /* On UWP, you can only load DLLs inside your install directory, using a special function that takes a relative path */

   if (!path_is_absolute(path))
      RARCH_WARN("Relative path in dylib_load! This is likely an attempt to load a system library that will fail\n");

   char *relative_path_abbrev = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));
   fill_pathname_abbreviate_special(relative_path_abbrev, path, PATH_MAX_LENGTH * sizeof(char));

   char *relative_path = relative_path_abbrev;
   if (relative_path[0] != ':' || !path_char_is_slash(relative_path[1]))
   {
      /* Path to dylib_load is not inside app install directory.
       * Loading will probably fail. */
   }
   else
      relative_path += 2;


   wchar_t *pathW = utf8_to_utf16_string_alloc(relative_path);
   dylib_t lib = LoadPackagedLibrary(pathW, 0);
   free(pathW);

   free(relative_path_abbrev);
#elif defined(LEGACY_WIN32)
   dylib_t lib  = LoadLibrary(path);
#else
   wchar_t *pathW = utf8_to_utf16_string_alloc(path);
   dylib_t lib  = LoadLibraryW(pathW);
   free(pathW);
#endif

#ifndef __WINRT__
   SetErrorMode(prevmode);
#endif

   if (!lib)
   {
      set_dl_error();
      return NULL;
   }
   last_dyn_error[0] = 0;
#else
   dylib_t lib = dlopen(path, RTLD_LAZY | RTLD_LOCAL);
#endif
   return lib;
}

char *dylib_error(void)
{
#ifdef _WIN32
   if (last_dyn_error[0])
      return last_dyn_error;
   return NULL;
#else
   return (char*)dlerror();
#endif
}

function_t dylib_proc(dylib_t lib, const char *proc)
{
   function_t sym;

#ifdef _WIN32
   HMODULE mod = (HMODULE)lib;
#ifndef __WINRT__
   if (!mod)
      mod = GetModuleHandle(NULL);
#else
   /* GetModuleHandle is not available on UWP */
   if (!mod)
   {
      /* It's not possible to lookup symbols in current executable
       * on UWP. */
      DebugBreak();
      return NULL;
   }
#endif
   sym = (function_t)GetProcAddress(mod, proc);
   if (!sym)
   {
      set_dl_error();
      return NULL;
   }
   last_dyn_error[0] = 0;
#else
   void *ptr_sym = NULL;

   if (lib)
      ptr_sym = dlsym(lib, proc);
   else
   {
      void *handle = dlopen(NULL, RTLD_LAZY);
      if (handle)
      {
         ptr_sym = dlsym(handle, proc);
         dlclose(handle);
      }
   }

   /* Dirty hack to workaround the non-legality of
    * (void*) -> fn-pointer casts. */
   memcpy(&sym, &ptr_sym, sizeof(void*));
#endif

   return sym;
}

/**
 * dylib_close:
 * @lib                          : Library handle.
 *
 * Frees library handle.
 **/
void dylib_close(dylib_t lib)
{
#ifdef _WIN32
   if (!FreeLibrary((HMODULE)lib))
      set_dl_error();
   last_dyn_error[0] = 0;
#else
#ifndef NO_DLCLOSE
   dlclose(lib);
#endif
#endif
}

#endif
