/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2015 - Daniel De Matteis
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include <string.h>

#include "dynamic.h"

#ifdef NEED_DYNAMIC

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
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
   dylib_t lib = LoadLibrary(path);
#else
   dylib_t lib = dlopen(path, RTLD_LAZY);
#endif
   return lib;
}

function_t dylib_proc(dylib_t lib, const char *proc)
{
   function_t sym;
   void *ptr_sym = NULL;

   (void)ptr_sym;

#ifdef _WIN32
   sym = (function_t)GetProcAddress(lib ?
         (HMODULE)lib : GetModuleHandle(NULL), proc);
#else
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
   FreeLibrary((HMODULE)lib);
#else
#ifndef NO_DLCLOSE
   dlclose(lib);
#endif
#endif
}

#endif
