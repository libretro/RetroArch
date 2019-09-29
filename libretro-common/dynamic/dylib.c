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

/* MadMonkey's original LZMA .so file support push, still needs reviewing
 * so assigning against CLASSIC platform only for now */
#if defined(HAVE_CLASSIC) && defined(__GLIBC__) && defined(__GLIBC_PREREQ)
#if __GLIBC_PREREQ(2, 2) /* shm_open */
#define soramLoader
#endif
#endif

#ifdef soramLoader
/* stolen from here: https://x-c3ll.github.io/posts/fileless-memfd_create/ */

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <errno.h>
#include <link.h>
#include <pthread.h>

#if 0
#define debug_printf(...) printf(__VA_ARGS__)
#else
static int debug_printf(const char *format, ...) { return 0; }
#endif

typedef struct _soramHandle
{
   dylib_t handle;
   char *soname;
   int32_t ref;
} soramHandle_t;

#define VECTOR_LIST_TYPE soramHandle_t
#define VECTOR_LIST_NAME soram
#include <../lists/vector_list.c>
typedef struct TYPE_NAME() soramList_t;
#undef VECTOR_LIST_TYPE
#undef VECTOR_LIST_NAME

static soramList_t *soramList = 0;
static pthread_mutex_t soramMutex = PTHREAD_MUTEX_INITIALIZER;

static soramHandle_t* soramFindName(const char *soname)
{
   size_t i;
   if (soramList == 0)
      return 0;
   for (i = 0; i < soramList->count; ++i)
      if (strcmp(soname, soramList->data[i].soname) == 0)
         return &soramList->data[i];
   return 0;
}

static soramHandle_t* soramFindHandle(dylib_t handle)
{
   size_t i;
   if (soramList == 0)
      return 0;
   for (i = 0; i < soramList->count; ++i)
      if (handle == soramList->data[i].handle)
         return &soramList->data[i];
   return 0;
}

static void soramAdd(const char *soname, dylib_t handle)
{
   soramHandle_t *so, _so;
   if (soramList == 0)
      soramList = soram_vector_list_new();
   so = soramFindHandle(0);
   if (so == 0)
      so = &_so;
   so->handle = handle;
   so->soname = strdup(soname);
   so->ref = 1;
   soram_vector_list_append(soramList, *so);
}

static bool soramRemove(dylib_t handle)
{
   size_t i, count;
   soramHandle_t *so = soramFindHandle(handle);
   if (so == 0)
      return 0;
   if (--so->ref > 0)
      return 1;
#ifndef NO_DLCLOSE
   dlclose(so->handle);
   free(so->soname);
   so->handle = 0;
   so->soname = 0;
   count = 0;
   for (i = 0; i < soramList->count; ++i)
      if (soramList->data[i].ref > 0)
         ++count;
   if (count)
      return 1;
   soram_vector_list_free(soramList);
   soramList = 0;
#endif
   return 1;
}

static void soramLock()
{
   pthread_mutex_lock(&soramMutex);
}

static void soramUnlock()
{
   pthread_mutex_unlock(&soramMutex);
}

/* Wrapper to call memfd_create syscall */
static int ra_memfd_create(const char *pathname, int flags)
{
   return syscall(319, pathname, flags);
}

/* Returns a file descriptor where we can write our shared object */
static int open_ramfs(const char *bname, int *shm)
{
   int fd = ra_memfd_create(bname, 1);
   if (shm) *shm = 0;
   if (fd < 0)
   {
      fd = shm_open(bname, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
      if (shm) *shm = 1;
   }
   return fd;
}

static int is_xz(const char *pathname)
{
   uint32_t buffer[2];
   FILE *hf = fopen(pathname, "rb");
   buffer[0] = 0;
   if (hf)
   {
      if (fread(buffer, 1, 4, hf) != 4)
      {
         buffer[0] = 0;
      }
      fclose(hf);
   }
   return (ntohl(buffer[0]) == 0xfd377a58) ? 0 : -1;
}

static int open_xz(const char *pathname, const char *bname, int *shm)
{
   int status = -1;
   size_t size = 0x1000, rchunk = 0, wchunk = 0;
   char *buffer;
   FILE *fp;
   int fd;

   fd = open_ramfs(bname, shm);
   if (fd < 0)
      return fd;

   buffer = (char*)malloc(size + 8);
   if (buffer)
   {
      snprintf(buffer, size, "xz -cd '%s'", pathname);
      fp = popen(buffer, "re");
      if (fp != 0)
      {
         while (!feof(fp) && !ferror(fp))
         {
            rchunk=TEMP_FAILURE_RETRY(fread(buffer, 1, size, fp));
            if (rchunk > 0)
            {
               wchunk=TEMP_FAILURE_RETRY(write(fd, buffer, rchunk));
               if (wchunk != rchunk)
                  break;
            }
         }
         status = pclose(fp);
      }

      free(buffer);
      if ((status == 0) && (wchunk == rchunk))
         return fd;
   }

   close(fd);
   return -1;
}

#ifdef DEBUG
static int dlcallback(struct dl_phdr_info *info, size_t size, void *data)
{
   if (info && info->dlpi_name)
      debug_printf("\t[+] %s\n", info->dlpi_name);
   return 0;
}
#endif

/* Load the shared object */
static dylib_t soramLoad(const char *pathname, int flag)
{
   char path[128];
   void *handle;
   char *dname, *bname;
   soramHandle_t *so;
   int fd, shm;

   if (is_xz(pathname) < 0)
      return 0;

   dname = strdup(pathname);
   if (dname == 0)
      return 0;
   bname = basename(dname);

   soramLock();
   so = soramFindName(bname);
   if (so)
   {
      ++so->ref;
      soramUnlock();
      free(dname);
      return so->handle;
   }

   debug_printf("[INFO] [dylib] soramLoad(%s)\n", pathname);
   fd = open_xz(pathname, bname, &shm);
   if (fd < 0)
   {
      soramUnlock();
      free(dname);
      return 0;
   }

   snprintf(path, sizeof(path), "/proc/self/fd/%d", fd);
   handle = dlopen(path, flag);
   close(fd);
#ifdef DEBUG
   dl_iterate_phdr(dlcallback, 0);
#endif
   if (shm)
   {
#ifdef DEBUG
      debug_printf("\t\tshm_unlink(%s) - ", bname);
      errno = 0;
      shm_unlink(bname);
      debug_printf("%d\n", errno);
#else
      shm_unlink(bname);
#endif
   }
   soramAdd(bname, handle);
   soramUnlock();
   free(dname);
   debug_printf("[INFO] [dylib] soramLoad(%s) - %s\n", pathname, handle?"ok":"fail");
   return handle;
}

static bool soramUnload(dylib_t handle)
{
   bool ret;
   soramLock();
   ret = soramRemove(handle);
   soramUnlock();
   return ret;
}
#endif /* soramLoader */

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
   dylib_t lib;
  #ifdef soramLoader
    lib = soramLoad(path, RTLD_LAZY);
    if (!lib)
  #endif
   lib = dlopen(path, RTLD_LAZY);
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
#ifdef soramLoader
   if (soramUnload(lib) == 0)
#endif
#ifndef NO_DLCLOSE
      dlclose(lib);
#else
      ;
#endif
#endif
}

#endif
