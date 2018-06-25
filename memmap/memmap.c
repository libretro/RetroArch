/* Copyright  (C) 2010-2018 The RetroArch team
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
void* mmap(void *addr, size_t len, int prot, int flags,
      int fildes, size_t offset)
{
   void     *map = (void*)NULL;
   HANDLE handle = INVALID_HANDLE_VALUE;

   switch (prot)
   {
      case PROT_READ:
      default:
         handle = CreateFileMapping((HANDLE)
               _get_osfhandle(fildes), 0, PAGE_READONLY, 0,
               len, 0);
         if (!handle)
            break;
         map = (void*)MapViewOfFile(handle, FILE_MAP_READ, 0, 0, len);
         CloseHandle(handle);
         break;
      case PROT_WRITE:
         handle = CreateFileMapping((HANDLE)
               _get_osfhandle(fildes),0,PAGE_READWRITE,0,
               len, 0);
         if (!handle)
            break;
         map = (void*)MapViewOfFile(handle, FILE_MAP_WRITE, 0, 0, len);
         CloseHandle(handle);
         break;
      case PROT_READWRITE:
         handle = CreateFileMapping((HANDLE)
               _get_osfhandle(fildes),0,PAGE_READWRITE,0,
               len, 0);
         if (!handle)
            break;
         map = (void*)MapViewOfFile(handle, FILE_MAP_ALL_ACCESS, 0, 0, len);
         CloseHandle(handle);
         break;
   }

   if (map == (void*)NULL)
      return((void*)MAP_FAILED);
   return((void*) ((int8_t*)map + offset));
}

int munmap(void *addr, size_t length)
{
   if (!UnmapViewOfFile(addr))
      return -1;
   return 0;
}

int mprotect(void *addr, size_t len, int prot)
{
   /* Incomplete, just assumes PAGE_EXECUTE_READWRITE right now
    * instead of correctly handling prot */
   prot = 0;
   if (prot & (PROT_READ | PROT_WRITE | PROT_EXEC))
      prot = PAGE_EXECUTE_READWRITE;
   return VirtualProtect(addr, len, prot, 0);
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
   size_t len = (char*)end - (char*)start;
#if defined(__MACH__) && defined(__arm__)
   sys_dcache_flush(start ,len);
   sys_icache_invalidate(start, len);
   return 0;
#elif defined(__arm__) && !defined(__QNX__)
   (void)len;
   __clear_cache(start, end);
   return 0;
#elif defined(HAVE_MMAN)
   return msync(start, len, MS_SYNC | MS_INVALIDATE
#ifdef __QNX__
         MS_CACHE_ONLY
#endif
         );
#else
   (void)len;
   return 0;
#endif
}

int memprotect(void *addr, size_t len)
{
   return mprotect(addr, len, PROT_READ | PROT_WRITE | PROT_EXEC);
}
