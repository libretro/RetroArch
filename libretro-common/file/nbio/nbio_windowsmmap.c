/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (nbio_windowsmmap.c).
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

#include <file/nbio.h>

#if defined(_WIN32)
#if defined(_MSC_VER) && _MSC_VER >= 1500

#ifndef HAVE_MMAP_WIN32
#define HAVE_MMAP_WIN32
#endif

#elif !defined(_MSC_VER)

#ifndef HAVE_MMAP_WIN32
#define HAVE_MMAP_WIN32
#endif
#endif

#endif

#if defined(HAVE_MMAP_WIN32)

#include <stdlib.h>

#include <encodings/utf.h>

#include <windows.h>

/* Assume W-functions do not work below Win2K and Xbox platforms */
#if (defined(_WIN32_WINNT) && _WIN32_WINNT < 0x0500) || defined(_XBOX)

#ifndef LEGACY_WIN32
#define LEGACY_WIN32
#endif

#endif

#ifndef FILE_SHARE_ALL
#define FILE_SHARE_ALL (FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE)
#endif

/* Precomputed access flag pairs: [0]=read protect+map, [1]=write protect+map */
#define MMAP_PROTECT_READ     PAGE_READONLY
#define MMAP_PROTECT_WRITE    PAGE_READWRITE
#define MMAP_VIEW_READ        FILE_MAP_READ
#define MMAP_VIEW_WRITE       (FILE_MAP_READ|FILE_MAP_WRITE)

struct nbio_mmap_win32_t
{
   HANDLE file;
   void*  ptr;
   size_t len;
   /* Precomputed flags hoisted out of hot paths */
   DWORD  map_protect;
   DWORD  map_view_access;
};

/*
 * Helper: create a file mapping and map a view in one call.
 * Keeps the mapping handle lifetime as short as possible
 * (CloseHandle right after MapViewOfFile is safe per MSDN).
 */
static void *nbio_mmap_win32_map(HANDLE file, DWORD protect, DWORD view_access, SIZE_T len)
{
   void  *ptr;
   HANDLE mem = CreateFileMapping(file, NULL, protect, 0, 0, NULL);
   if (mem == NULL || mem == INVALID_HANDLE_VALUE)
      return NULL;
   ptr = MapViewOfFile(mem, view_access, 0, 0, len);
   CloseHandle(mem);
   return ptr;
}

static void *nbio_mmap_win32_open(const char *filename, unsigned mode)
{
   static const DWORD dispositions[] = {
      OPEN_EXISTING,  /* NBIO_READ   */
      CREATE_ALWAYS,  /* NBIO_WRITE  */
      OPEN_ALWAYS,    /* NBIO_UPDATE */
      OPEN_EXISTING,  /* BIO_READ    */
      CREATE_ALWAYS   /* BIO_WRITE   */
   };

   HANDLE                    file;
   void                     *ptr;
   struct nbio_mmap_win32_t *handle;
   DWORD                     map_protect;
   DWORD                     map_view_access;
   DWORD                     access;
   int                       is_write;
#if defined(_WIN32_WINNT) && _WIN32_WINNT >= 0x0500
   LARGE_INTEGER             len;
   SIZE_T                    map_len;
#else
   DWORD                     len;
#endif

   is_write       = (mode == NBIO_WRITE || mode == NBIO_UPDATE || mode == BIO_WRITE);
   access         = is_write ? (GENERIC_READ|GENERIC_WRITE) : GENERIC_READ;
   map_protect    = is_write ? MMAP_PROTECT_WRITE : MMAP_PROTECT_READ;
   map_view_access= is_write ? MMAP_VIEW_WRITE    : MMAP_VIEW_READ;

#if !defined(_WIN32) || defined(LEGACY_WIN32)
   file = CreateFile(filename, access, FILE_SHARE_ALL, NULL,
                     dispositions[mode], FILE_ATTRIBUTE_NORMAL, NULL);
#else
   {
      wchar_t *filename_wide = utf8_to_utf16_string_alloc(filename);
#ifdef __WINRT__
      file = CreateFile2(filename_wide, access, FILE_SHARE_ALL,
                         dispositions[mode], NULL);
#else
      file = CreateFileW(filename_wide, access, FILE_SHARE_ALL, NULL,
                         dispositions[mode], FILE_ATTRIBUTE_NORMAL, NULL);
#endif
      if (filename_wide)
         free(filename_wide);
   }
#endif

   if (file == INVALID_HANDLE_VALUE)
      return NULL;

#if defined(_WIN32_WINNT) && _WIN32_WINNT >= 0x0500
   /* GetFileSizeEx is new for Windows 2000 */
   if (!GetFileSizeEx(file, &len))
   {
      CloseHandle(file);
      return NULL;
   }
   map_len = (SIZE_T)len.QuadPart;
   ptr     = nbio_mmap_win32_map(file, map_protect, map_view_access, map_len);
#else
   len = GetFileSize(file, NULL);
   if (len == INVALID_FILE_SIZE)
   {
      CloseHandle(file);
      return NULL;
   }
   ptr = nbio_mmap_win32_map(file, map_protect, map_view_access, (SIZE_T)len);
#endif

   if (!ptr)
   {
      CloseHandle(file);
      return NULL;
   }

   handle = (struct nbio_mmap_win32_t*)malloc(sizeof(*handle));
   if (!handle)
   {
      UnmapViewOfFile(ptr);
      CloseHandle(file);
      return NULL;
   }

   handle->file           = file;
   handle->ptr            = ptr;
   handle->map_protect    = map_protect;
   handle->map_view_access= map_view_access;
#if defined(_WIN32_WINNT) && _WIN32_WINNT >= 0x0500
   handle->len            = (size_t)len.QuadPart;
#else
   handle->len            = (size_t)len;
#endif

   return handle;
}

static void nbio_mmap_win32_begin_read(void *data)  { (void)data; }
static void nbio_mmap_win32_begin_write(void *data) { (void)data; }
static bool nbio_mmap_win32_iterate(void *data)     { return 1; }
static void nbio_mmap_win32_cancel(void *data)      { (void)data; }

static void nbio_mmap_win32_resize(void *data, size_t len)
{
   HANDLE                    mem;
   struct nbio_mmap_win32_t *handle = (struct nbio_mmap_win32_t*)data;
#if defined(_WIN32_WINNT) && _WIN32_WINNT >= 0x0500
   LARGE_INTEGER             len_li;
#endif

   if (!handle)
      return;

   /* Shrinking is blocked for cross-implementation compatibility */
   if (len < handle->len)
      abort();

   /* Unmap before repositioning the file pointer to avoid stale views */
   UnmapViewOfFile(handle->ptr);
   handle->ptr = NULL;

#if defined(_WIN32_WINNT) && _WIN32_WINNT >= 0x0500
   len_li.QuadPart = (LONGLONG)len;
   SetFilePointerEx(handle->file, len_li, NULL, FILE_BEGIN);
#else
   SetFilePointer(handle->file, (LONG)len, NULL, FILE_BEGIN);
#endif

   if (!SetEndOfFile(handle->file))
      abort();

   handle->len = len;
   handle->ptr = nbio_mmap_win32_map(handle->file,
                 handle->map_protect,
                 handle->map_view_access,
                 (SIZE_T)len);
   if (!handle->ptr)
      abort();
}

static void *nbio_mmap_win32_get_ptr(void *data, size_t *len)
{
   const struct nbio_mmap_win32_t *handle = (const struct nbio_mmap_win32_t*)data;
   if (!handle)
      return NULL;
   if (len)
      *len = handle->len;
   return handle->ptr;
}

static void nbio_mmap_win32_free(void *data)
{
   struct nbio_mmap_win32_t *handle = (struct nbio_mmap_win32_t*)data;
   if (!handle)
      return;
   if (handle->ptr)
      UnmapViewOfFile(handle->ptr);
   CloseHandle(handle->file);
   free(handle);
}

nbio_intf_t nbio_mmap_win32 = {
   nbio_mmap_win32_open,
   nbio_mmap_win32_begin_read,
   nbio_mmap_win32_begin_write,
   nbio_mmap_win32_iterate,
   nbio_mmap_win32_resize,
   nbio_mmap_win32_get_ptr,
   nbio_mmap_win32_cancel,
   nbio_mmap_win32_free,
   "nbio_mmap_win32",
};

#else

nbio_intf_t nbio_mmap_win32 = {
   NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
   "nbio_mmap_win32",
};

#endif
