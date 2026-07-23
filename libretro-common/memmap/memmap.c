/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (memmap.c).
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

#include <stdint.h>
#include <stdlib.h>
#include <memmap.h>

#ifndef PROT_READ
#define PROT_READ         0x1  /* Page can be read */
#endif

#ifndef PROT_WRITE
#define PROT_WRITE        0x2  /* Page can be written. */
#endif

#ifndef PROT_READWRITE
#define PROT_READWRITE    0x3  /* Page can be written to and read from. */
#endif

#ifndef PROT_EXEC
#define PROT_EXEC         0x4  /* Page can be executed. */
#endif

#ifndef PROT_NONE
#define PROT_NONE         0x0  /* Page can not be accessed. */
#endif

#ifndef MAP_FAILED
#define MAP_FAILED        ((void *) -1)
#endif

#ifdef _WIN32
/* Map POSIX prot bits to a PAGE_* protection constant.  Windows has
 * no write-only or exec-only protections; those requests take the
 * nearest expressible superset, as every mman shim does. */
static DWORD win32_page_prot(int prot)
{
   if (prot == PROT_NONE)
      return PAGE_NOACCESS;
   if (prot & PROT_EXEC)
      return (prot & PROT_WRITE) ? PAGE_EXECUTE_READWRITE
                                 : PAGE_EXECUTE_READ;
   return (prot & PROT_WRITE) ? PAGE_READWRITE : PAGE_READONLY;
}

/* The FILE_MAP_* access for a view of a section created with the
 * protection above. */
static DWORD win32_view_access(int prot)
{
   DWORD access = FILE_MAP_READ;
   if (prot & PROT_WRITE)
      access = FILE_MAP_ALL_ACCESS;
   if (prot & PROT_EXEC)
      access |= FILE_MAP_EXECUTE;
   return access;
}

void* mmap(void *addr, size_t len, int prot, int flags,
      int fildes, size_t offset)
{
   void  *map    = NULL;
   HANDLE handle;
   /* Sections are created with the maximum protection a later
    * mprotect may need: VirtualProtect on a view cannot exceed the
    * section's protection, so an anonymous mapping opened without
    * PROT_EXEC can never later become executable (request PROT_EXEC
    * up front, as every JIT does), and PROT_NONE starts from a
    * readable section and is locked down after mapping. */
   int    sect_prot = (prot == PROT_NONE) ? PROT_READ : prot;

   (void)addr; /* placement hint not honoured, as before */

   if ((flags & MAP_ANONYMOUS) || fildes < 0)
   {
      /* Anonymous: a pagefile-backed section, so munmap stays
       * UnmapViewOfFile for every mapping this function returns. */
      handle = CreateFileMapping(INVALID_HANDLE_VALUE, NULL,
            win32_page_prot(sect_prot) | SEC_COMMIT,
            (DWORD)((uint64_t)len >> 32), (DWORD)len, NULL);
      if (!handle)
         return MAP_FAILED;
      map = MapViewOfFile(handle, win32_view_access(sect_prot),
            0, 0, len);
      CloseHandle(handle);
   }
   else
   {
      /* File-backed.  The file offset must be a multiple of the
       * allocation granularity (64 KiB): MapViewOfFile accepts
       * nothing finer, and the historical behaviour here - mapping
       * from 0 and adding the offset to the returned pointer - both
       * read the wrong bytes past the mapped length and broke the
       * later UnmapViewOfFile, which needs the view base. */
      SYSTEM_INFO si;
      uint64_t    end = (uint64_t)offset + len;

      GetSystemInfo(&si);
      if (offset & (si.dwAllocationGranularity - 1))
         return MAP_FAILED;

      handle = CreateFileMapping((HANDLE)_get_osfhandle(fildes), NULL,
            win32_page_prot(sect_prot),
            (DWORD)(end >> 32), (DWORD)end, NULL);
      if (!handle)
         return MAP_FAILED;
      map = MapViewOfFile(handle, win32_view_access(sect_prot),
            (DWORD)((uint64_t)offset >> 32), (DWORD)offset, len);
      CloseHandle(handle);
   }

   if (!map)
      return MAP_FAILED;

   if (prot == PROT_NONE)
   {
      DWORD old;
      VirtualProtect(map, len, PAGE_NOACCESS, &old);
   }

   return map;
}

int munmap(void *addr, size_t len)
{
   (void)len;
   return (UnmapViewOfFile(addr)) ? 0 : -1;
}

int mprotect(void *addr, size_t len, int prot)
{
   /* The previous version dead-stored prot to 0 on entry and passed
    * a NULL old-protection pointer, so VirtualProtect failed on
    * every call (protection 0 is invalid and lpflOldProtect is
    * mandatory) - and the raw BOOL return inverted the POSIX
    * convention on top.  Nothing in this tree exercised it on
    * Windows, which is how it survived. */
   DWORD old;
   if (!VirtualProtect(addr, len, win32_page_prot(prot), &old))
      return -1;
   return 0;
}

#elif !defined(HAVE_MMAN)
void* mmap(void *addr, size_t len, int prot, int flags,
      int fildes, size_t offset)
{
   return malloc(len);
}

int munmap(void *addr, size_t len)
{
   free(addr);
   return 0;
}

int mprotect(void *addr, size_t len, int prot)
{
   /* stub - not really needed at this point
    * since this codepath has no dynarecs. */
   return 0;
}

#endif

#if defined(__MACH__) && defined(__arm__)
#include <libkern/OSCacheControl.h>
#endif

int memsync(void *start, void *end)
{
#if defined(__MACH__) && defined(__arm__)
   size_t _len = (char*)end - (char*)start;
   sys_dcache_flush(start, _len);
   sys_icache_invalidate(start, _len);
   return 0;
#elif defined(__arm__) && !defined(__QNX__)
   __clear_cache(start, end);
   return 0;
#elif defined(HAVE_MMAN)
   size_t _len = (char*)end - (char*)start;
   return msync(start, _len, MS_SYNC | MS_INVALIDATE
#ifdef __QNX__
         MS_CACHE_ONLY
#endif
         );
#else
   return 0;
#endif
}

int memprotect(void *addr, size_t len)
{
   return mprotect(addr, len, PROT_READ | PROT_WRITE | PROT_EXEC);
}
