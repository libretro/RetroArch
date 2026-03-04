/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (nbio_unixmmap.c).
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

#include <stdio.h>
#include <stdlib.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <file/nbio.h>

#if defined(HAVE_MMAP) && defined(BSD)

#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif
#include <fcntl.h>
#include <sys/mman.h>

#ifdef __APPLE__
#  ifndef O_CLOEXEC
#    define O_CLOEXEC 0x1000000
#  endif
#else
#  ifndef O_CLOEXEC
#    define O_CLOEXEC 0
#  endif
#endif

/* Sentinel for a zero-length file: mmap requires len > 0, so we skip
 * the mmap call entirely and return this non-NULL marker instead.
 * get_ptr() converts it back to NULL for callers. */
#define NBIO_EMPTY_PTR ((void*)1)

struct nbio_mmap_unix_t
{
   void  *ptr;
   size_t len;
   int    fd;
   int    map_flags;
};

static void *nbio_mmap_unix_open(const char *filename, unsigned mode)
{
   static const int o_flags[] = {
      O_RDONLY,
      O_RDWR | O_CREAT | O_TRUNC,
      O_RDWR,
      O_RDONLY,
      O_RDWR | O_CREAT | O_TRUNC
   };
   static const int map_flags[] = {
      PROT_READ,
      PROT_WRITE | PROT_READ,
      PROT_WRITE | PROT_READ,
      PROT_READ,
      PROT_WRITE | PROT_READ
   };

   struct nbio_mmap_unix_t *handle;
   void  *ptr;
   size_t len;
   int    fd = open(filename, o_flags[mode] | O_CLOEXEC, 0644);

   if (fd < 0)
      return NULL;

   len = (size_t)lseek(fd, 0, SEEK_END);

   /* Zero-length file: skip mmap, use sentinel */
   if (len == 0)
      ptr = NBIO_EMPTY_PTR;
   else
   {
      ptr = mmap(NULL, len, map_flags[mode], MAP_SHARED, fd, 0);
      if (ptr == MAP_FAILED)
      {
         close(fd);
         return NULL;
      }
   }

   handle = (struct nbio_mmap_unix_t*)malloc(sizeof(*handle));
   if (!handle)
   {
      if (ptr != NBIO_EMPTY_PTR)
         munmap(ptr, len);
      close(fd);
      return NULL;
   }

   handle->fd        = fd;
   handle->map_flags = map_flags[mode];
   handle->len       = len;
   handle->ptr       = ptr;
   return handle;
}

static void nbio_mmap_unix_begin_read(void *data)  { (void)data; }
static void nbio_mmap_unix_begin_write(void *data) { (void)data; }
static void nbio_mmap_unix_cancel(void *data)      { (void)data; }
static bool nbio_mmap_unix_iterate(void *data)     { return true; }

static void nbio_mmap_unix_resize(void *data, size_t len)
{
   void *ptr;
   struct nbio_mmap_unix_t *handle = (struct nbio_mmap_unix_t*)data;

   if (!handle)
      return;

   /* Shrinking is blocked to stay compatible with other nbio backends */
   if (len < handle->len)
      abort();

   if (ftruncate(handle->fd, (off_t)len) != 0)
      abort();

   /* Unmap old region only after ftruncate succeeds */
   if (handle->ptr != NBIO_EMPTY_PTR)
      munmap(handle->ptr, handle->len);

   ptr = mmap(NULL, len, handle->map_flags, MAP_SHARED, handle->fd, 0);
   if (ptr == MAP_FAILED)
      abort();

   handle->ptr = ptr;
   handle->len = len;
}

static void *nbio_mmap_unix_get_ptr(void *data, size_t *len)
{
   struct nbio_mmap_unix_t *handle = (struct nbio_mmap_unix_t*)data;

   if (!handle)
      return NULL;
   if (len)
      *len = handle->len;

   /* Hide the sentinel from callers; a zero-length file has no data */
   return (handle->ptr == NBIO_EMPTY_PTR) ? NULL : handle->ptr;
}

static void nbio_mmap_unix_free(void *data)
{
   struct nbio_mmap_unix_t *handle = (struct nbio_mmap_unix_t*)data;

   if (!handle)
      return;

   if (handle->ptr != NBIO_EMPTY_PTR)
      munmap(handle->ptr, handle->len);

   close(handle->fd);
   free(handle);
}

nbio_intf_t nbio_mmap_unix = {
   nbio_mmap_unix_open,
   nbio_mmap_unix_begin_read,
   nbio_mmap_unix_begin_write,
   nbio_mmap_unix_iterate,
   nbio_mmap_unix_resize,
   nbio_mmap_unix_get_ptr,
   nbio_mmap_unix_cancel,
   nbio_mmap_unix_free,
   "nbio_mmap_unix",
};

#else

nbio_intf_t nbio_mmap_unix = {
   NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
   "nbio_mmap_unix",
};

#endif
